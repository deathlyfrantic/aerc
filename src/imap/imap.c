#define _POSIX_C_SOURCE 201112LL
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <poll.h>
#include "util/hashtable.h"
#include "util/stringop.h"
#include "imap/imap.h"
#include "worker.h"
#include "log.h"
#define BUFFER_SIZE 1024

bool inited = false;
hashtable_t *internal_handlers = NULL;

void handle_line(struct imap_connection *imap, const char *line) {
	list_t *split = split_string(line, " ");
	if (split->length <= 2) {
		free_flat_list(split);
		worker_log(L_DEBUG, "Got malformed IMAP command: %s", line);
		return;
	}
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

void imap_receive(struct imap_connection *imap) {
	poll(imap->poll, 1, -1);
	if (imap->poll[0].revents & POLLIN) {
		if (imap->mode == RECV_WAIT) {
			// no-op
		} else if (imap->mode == RECV_LINE) {
			ssize_t amt = ab_recv(imap->socket, imap->line + imap->line_index,
					imap->line_size - imap->line_index, 0);
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
			}
		}
	}
}

unsigned int hash_imap_command(const void *_cmd) {
	const char *cmd = _cmd;
	unsigned int hash = 5381;
	char c;
	while ((c = *cmd++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

void imap_init(struct imap_connection *imap) {
	imap->mode = RECV_WAIT;
	imap->line = calloc(1, BUFFER_SIZE + 1);
	imap->line_index = 0;
	imap->line_size = BUFFER_SIZE;
	imap->next_tag = 0;
	if (internal_handlers == NULL) {
		internal_handlers = create_hashtable(128, hash_imap_command);
		hashtable_set(internal_handlers, "OK", handle_imap_OK);
	}
}

void imap_close(struct imap_connection *imap) {
	if (imap->socket) {
		absocket_free(imap->socket);
	}
	free(imap->line);
	free(imap);
}

bool imap_connect(struct imap_connection *imap, const char *host,
		const char *port, bool use_ssl) {
	imap_init(imap);
	imap->socket = absocket_new(host, port, use_ssl);
	if (!imap->socket) {
		return false;
	}
	imap->poll[0].fd = imap->socket->basefd;
	imap->poll[0].events = POLLIN;
	return true;
}
