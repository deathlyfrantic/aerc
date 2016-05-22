/*
 * imap/imap.c - responsible for opening and maintaining the IMAP socket
 */
#define _POSIX_C_SOURCE 201112LL

#include <assert.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "absocket.h"
#include "imap/imap.h"
#include "internal/imap.h"
#include "log.h"
#include "urlparse.h"
#include "util/hashtable.h"
#include "util/list.h"
#include "util/stringop.h"

#define BUFFER_SIZE 1024

bool inited = false;
hashtable_t *internal_handlers = NULL;

/*
 * The internal IMAP command handler type.
 */
typedef void (*imap_handler_t)(struct imap_connection *imap,
	const char *token, const char *cmd, imap_arg_t *args);

struct imap_pending_callback *make_callback(imap_callback_t callback, void *data) {
	/* 
	 * We need to keep the data passed in by the user around so we can
	 * eventually pass it back to them when we invoke their callback.
	 */
	struct imap_pending_callback *cb = malloc(sizeof(struct imap_pending_callback));
	cb->callback = callback;
	cb->data = data;
	return cb;
}

int handle_line(struct imap_connection *imap, imap_arg_t *arg) {
	assert(arg && arg->next); // At least a tag and command
	/*
	 * We grab a handler based on the IMAP command in question. IMAP commands
	 * are formatted like this:
	 *
	 * [tag] [command] [...]
	 *
	 * The tag identifies the particular event this command refers to, or '*'.
	 * It's set to whatever tag we passed in when we asked the server to do a
	 * thing, but can often be * to provide updates to aerc's internal state, or
	 * meta responses. Our hashtable is keyed on the command string.
	 */
	if (arg->next && arg->next->type == IMAP_NUMBER) {
		/*
		 * Some commands (namely EXISTS and RECENT) are formatted like so:
		 *
		 * [tag] [arg] [command] [...]
		 *
		 * Which is fucking stupid. But we handle that here by checking if the
		 * command name is a number and then rearranging it.
		 */
		imap_arg_t *num = arg->next;
		imap_arg_t *cmd = num->next;
		imap_arg_t *rest = cmd->next;
		cmd->next = num;
		num->next = rest;
		arg->next = cmd;
	}
	assert(arg->type == IMAP_ATOM);
	assert(arg->next->type == IMAP_ATOM);
	imap_handler_t handler = hashtable_get(internal_handlers, arg->next->str);
	if (handler) {
		/*
		 * Here we join the arguments - the [...] from above, then parse this as
		 * an IMAP argument list (which has special syntax and semantic
		 * meaning). Then we invoke our internal handler for this IMAP command.
		 */
		handler(imap, arg->str, arg->next->str, arg->next->next);
	} else {
		worker_log(L_DEBUG, "Recieved unknown IMAP command: %s", arg->str);
	}
	return 0;
}

void imap_send(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *fmt, ...) {
	/*
	 * First, we determine the length of the formatted string.
	 */
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	/*
	 * Then we allocate a buffer that length, and do the actual printf into it.
	 */
	char *buf = malloc(len + 1);
	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	/*
	 * We now have the command they want to send, but we need to add a tag and a
	 * CRLF. By convention our tags are aXXXX, where XXXX is a number that
	 * increments each time this function is called. Here we find the length,
	 * allocate a buffer, and printf into that buffer again.
	 */
	len = snprintf(NULL, 0, "a%04d", imap->next_tag);
	char *tag = malloc(len + 1);
	snprintf(tag, len + 1, "a%04d", imap->next_tag++);
	/*
	 * Once we've printed a tag, we have all of the pieces necessary to form the
	 * full IMAP command. We do this here - measure, allocate, print.
	 */
	len = snprintf(NULL, 0, "%s %s\r\n", tag, buf);
	char *cmd = malloc(len + 1);
	snprintf(cmd, len + 1, "%s %s\r\n", tag, buf);
	/*
	 * We can now send the completed IMAP command to the server. We also take a
	 * moment to add the user-provided callback to the pending callbacks
	 * hashtable, keyed on the tag. The server will reference this tag when it
	 * sends us the response, and we can pull that out of the hashtable later to
	 * invoke the callback.
	 */
	ab_send(imap->socket, cmd, len);
	hashtable_set(imap->pending, tag, make_callback(callback, data));
	if (strncmp("LOGIN ", buf, 6) != 0) {
		worker_log(L_DEBUG, "-> %s %s", tag, buf);
	} else {
		/* Obsfucate the debug logging if sending a sensitive command */
		worker_log(L_DEBUG, "-> %s LOGIN *****", tag);
	}

	free(cmd);
	free(buf);
	free(tag);
}

void imap_receive(struct imap_connection *imap) {
	/*
	 * This function will poll(3) for data waiting on the socket, then attempt
	 * to receive it per the various modes the connection may be in.
	 */
	poll(imap->poll, 1, -1);
	if (imap->poll[0].revents & POLLIN) {
		if (imap->mode == RECV_WAIT) {
			/* The mode may be RECV_WAIT if we are waiting on the user to verify
			 * the SSL certificate, for example. */
		} else if (imap->mode == RECV_LINE) {
			/*
			 * We are receiving data a line at a time. IMAP lines are delimited
			 * with CRLF, so we attempt to receive data until we've filled our
			 * buffer with a full line.
			 */
			ssize_t amt = ab_recv(imap->socket, imap->line + imap->line_index,
					imap->line_size - imap->line_index);
			imap->line_index += amt;
			if (imap->line_index == imap->line_size) {
				/*
				 * We have filled our entire line buffer. Reallocate it a bit
				 * bigger, set the new space to NULL, and we'll try again
				 * shortly.
				 */
				imap->line = realloc(imap->line,
						imap->line_size + BUFFER_SIZE + 1);
				memset(imap->line + imap->line_index + 1, 0,
						(imap->line_size - imap->line_index + 1));
				imap->line_size = imap->line_size + BUFFER_SIZE;
			}
			/*
			 * Here we select lines from the buffer and pass them along to
			 * handle_line.
			 */
			int remaining = 0;
			while (!remaining) {
				/*
				 * Attempt to parse what we've got:
				 */
				imap_arg_t *arg = calloc(1, sizeof(imap_arg_t));
				int len = imap_parse_args(imap->line, arg, &remaining);
				if (remaining == 0) {
					/*
					 * If we got a complete IMAP command, pass it along to the
					 * handlers:
					 */
					char c = imap->line[len];
					imap->line[len] = '\0';
					worker_log(L_DEBUG, "Handling %s", imap->line);
					imap->line[len] = c;

					handle_line(imap, arg);
				}
				imap_arg_free(arg);
				/*
				 * If we got a full command, shift the end of the buffer up to
				 * the top and loop. Otherwise, append further bytes to the end
				 * of the buffer and move on.
				 */
				if (len > 0 && remaining == 0) {
					memmove(imap->line, imap->line + len, imap->line_size - len);
					imap->line_index = 0;
				} else {
					imap->line_index += len;
				}
			}
		}
	}
}

void handle_noop(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	/*
	 * Handler for commands we don't care about to avoid logging unknown command
	 * warnings.
	 */
}

void imap_init(struct imap_connection *imap) {
	/* Set up the internal state of the IMAP connection */
	imap->mode = RECV_WAIT;
	imap->line = calloc(1, BUFFER_SIZE + 1);
	imap->line_index = 0;
	imap->line_size = BUFFER_SIZE;
	imap->next_tag = 1;
	imap->pending = create_hashtable(128, hash_string);
	imap->mailboxes = create_list();
	if (internal_handlers == NULL) {
		/*
		 * Internal IMAP handlers are stored in a hashtable keyed on the IMAP
		 * command they handle. Here we register all of the internal handlers.
		 */
		internal_handlers = create_hashtable(128, hash_string);
		hashtable_set(internal_handlers, "OK", handle_imap_status);
		hashtable_set(internal_handlers, "NO", handle_imap_status);
		hashtable_set(internal_handlers, "BAD", handle_imap_status);
		hashtable_set(internal_handlers, "PREAUTH", handle_imap_status);
		hashtable_set(internal_handlers, "BYE", handle_imap_status);
		hashtable_set(internal_handlers, "CAPABILITY", handle_imap_capability);
		hashtable_set(internal_handlers, "LIST", handle_imap_list);
		hashtable_set(internal_handlers, "FLAGS", handle_imap_flags);
		hashtable_set(internal_handlers, "PERMANENTFLAGS", handle_imap_flags);
		hashtable_set(internal_handlers, "EXISTS", handle_imap_existsunseenrecent);
		hashtable_set(internal_handlers, "UNSEEN", handle_imap_existsunseenrecent);
		hashtable_set(internal_handlers, "RECENT", handle_imap_existsunseenrecent);
		hashtable_set(internal_handlers, "UIDNEXT", handle_imap_uidnext);
		hashtable_set(internal_handlers, "READ-WRITE", handle_imap_readwrite);
		hashtable_set(internal_handlers, "UIDVALIDITY", handle_noop);
		hashtable_set(internal_handlers, "HIGHESTMODSET", handle_noop); // RFC 4551
		hashtable_set(internal_handlers, "FETCH", handle_imap_fetch);
	}
}

void imap_close(struct imap_connection *imap) {
	absocket_free(imap->socket);
	free(imap->line);
	free(imap);
}

bool imap_connect(struct imap_connection *imap, const struct uri *uri,
		bool use_ssl, imap_callback_t callback, void *data) {
	/*
	 * Initializes an IMAP connection, connects to the server, and registers a
	 * callback for when the connection is established and the server is ready.
	 */
	imap_init(imap);
	imap->socket = absocket_new(uri, use_ssl);
	if (!imap->socket) {
		return false;
	}
	imap->poll[0].fd = imap->socket->basefd;
	imap->poll[0].events = POLLIN;
	hashtable_set(imap->pending, "*", make_callback(callback, data));
	return true;
}
