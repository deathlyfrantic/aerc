#define _POSIX_C_SOURCE 201112LL
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>
#include <stdarg.h>
#include "util/hashtable.h"
#include "util/stringop.h"
#include "internal/imap.h"
#include "worker.h"
#include "log.h"
#define BUFFER_SIZE 1024

bool inited = false;
hashtable_t *internal_handlers = NULL;

typedef void (*imap_handler_t)(struct imap_connection *imap,
	const char *token, const char *cmd, const char *args);

void handle_line(struct imap_connection *imap, const char *line) {
	list_t *split = split_string(line, " ");
	if (split->length <= 2) {
		free_flat_list(split);
		worker_log(L_DEBUG, "Got malformed IMAP command: %s", line);
		return;
	}
	worker_log(L_DEBUG, "<- %s %s", (char *)split->items[0],
			(char *)split->items[1]);
	imap_handler_t handler = hashtable_get(internal_handlers, split->items[1]);
	if (handler) {
		char *joined = join_args((char **)(split->items + 2),
				split->length - 2);
		handler(imap, split->items[0], split->items[1], joined);
		free(joined);
	} else {
		worker_log(L_DEBUG, "Recieved unknown IMAP command: %s", line);
	}
	free_flat_list(split);
}

void imap_send(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *buf = malloc(len + 1);

	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	len = snprintf(NULL, 0, "a%04d", imap->next_tag);
	char *tag = malloc(len + 1);
	snprintf(tag, len + 1, "a%04d", imap->next_tag++);
	len = snprintf(NULL, 0, "%s %s\r\n", tag, buf);
	char *cmd = malloc(len + 1);
	snprintf(cmd, len + 1, "%s %s\r\n", tag, buf);
	ab_send(imap->socket, cmd, len);
	hashtable_set(imap->pending, tag, make_callback(callback, data));
	if (strncmp("LOGIN ", buf, 6) != 0) {
		worker_log(L_DEBUG, "-> %s %s", tag, buf);
	} else {
		worker_log(L_DEBUG, "-> %s LOGIN *****", tag);
	}

	free(cmd);
	free(buf);
	free(tag);
}

void imap_receive(struct imap_connection *imap) {
	poll(imap->poll, 1, -1);
	if (imap->poll[0].revents & POLLIN) {
		if (imap->mode == RECV_WAIT) {
			// no-op
		} else if (imap->mode == RECV_LINE) {
			ssize_t amt = ab_recv(imap->socket, imap->line + imap->line_index,
					imap->line_size - imap->line_index);
			imap->line_index += amt;
			if (imap->line_index == imap->line_size) {
				imap->line = realloc(imap->line,
						imap->line_size + BUFFER_SIZE + 1);
				memset(imap->line + imap->line_index + 1, 0,
						(imap->line_size - imap->line_index + 1));
				imap->line_size = imap->line_size + BUFFER_SIZE;
			}
			char *esc;
			while ((esc = strstr(imap->line, "\r\n"))) {
				*esc = '\0';
				handle_line(imap, imap->line);
				memmove(imap->line, esc + 2, imap->line_size - (esc - imap->line + 2));
				imap->line_index = 0;
			}
		}
	}
}

void imap_init(struct imap_connection *imap) {
	imap->mode = RECV_WAIT;
	imap->line = calloc(1, BUFFER_SIZE + 1);
	imap->line_index = 0;
	imap->line_size = BUFFER_SIZE;
	imap->next_tag = 1;
	imap->pending = create_hashtable(128, hash_string);
	if (internal_handlers == NULL) {
		internal_handlers = create_hashtable(128, hash_string);
		hashtable_set(internal_handlers, "OK", handle_imap_status);
		hashtable_set(internal_handlers, "NO", handle_imap_status);
		hashtable_set(internal_handlers, "BAD", handle_imap_status);
		hashtable_set(internal_handlers, "PREAUTH", handle_imap_status);
		hashtable_set(internal_handlers, "BYE", handle_imap_status);
		hashtable_set(internal_handlers, "CAPABILITY", handle_imap_capability);
		hashtable_set(internal_handlers, "LIST", handle_imap_list);
	}
}

void imap_close(struct imap_connection *imap) {
	if (imap->socket) {
		absocket_free(imap->socket);
	}
	free(imap->line);
	free(imap);
}

struct imap_pending_callback *make_callback(imap_callback_t callback, void *data) {
	struct imap_pending_callback *cb = malloc(sizeof(struct imap_pending_callback));
	cb->callback = callback;
	cb->data = data;
	return cb;
}

bool imap_connect(struct imap_connection *imap, const char *host,
		const char *port, bool use_ssl, imap_callback_t callback, void *data) {
	imap_init(imap);
	imap->socket = absocket_new(host, port, use_ssl);
	if (!imap->socket) {
		return false;
	}
	imap->poll[0].fd = imap->socket->basefd;
	imap->poll[0].events = POLLIN;
	hashtable_set(imap->pending, "*", make_callback(callback, data));
	return true;
}
