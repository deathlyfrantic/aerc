/*
 * imap/worker/worker.c - IMAP worker main thread and action dispatcher
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "worker.h"
#include "email/headers.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "internal/imap.h"
#include "log.h"
#include "util/list.h"

struct action_handler {
	enum worker_message_type action;
	void (*handler)(struct worker_pipe *pipe, struct worker_message *message);
};

struct action_handler handlers[] = {
	{ WORKER_CONNECT, handle_worker_connect },
	{ WORKER_LIST, handle_worker_list },
	{ WORKER_SELECT_MAILBOX, handle_worker_select_mailbox },
#ifdef USE_OPENSSL
	{ WORKER_CONNECT_CERT_OKAY, handle_worker_cert_okay },
#endif
	{ WORKER_FETCH_MESSAGES, handle_worker_fetch_messages },
	{ WORKER_DELETE_MAILBOX, handle_worker_delete_mailbox },
};

void handle_message(struct worker_pipe *pipe, struct worker_message *message) {
	/*
	 * Here we iterate over each of the handlers we support and invoke the
	 * appropriate one.
	 */
	for (size_t i = 0; i < sizeof(handlers) / sizeof(struct action_handler); i++) {
		if (handlers[i].action == message->type) {
			handlers[i].handler(pipe, message);
			return;
		}
	}
	worker_post_message(pipe, WORKER_UNSUPPORTED, message, NULL);
}

struct aerc_message *serialize_message(struct mailbox_message *source) {
	if (!source) return NULL;
	struct aerc_message *dest = calloc(1, sizeof(struct aerc_message));
	dest->index = source->index;
	dest->fetched = source->populated;
	if (!source->populated) {
		return dest;
	}
	dest->uid = source->uid;
	dest->flags = create_list();
	for (size_t i = 0; i < source->flags->length; ++i) {
		list_add(dest->flags, strdup(source->flags->items[i]));
	}
	dest->headers = create_list();
	for (size_t i = 0; i < source->headers->length; ++i) {
		struct email_header *header = source->headers->items[i];
		struct email_header *copy = malloc(sizeof(struct email_header));
		copy->key = strdup(header->key);
		copy->value = strdup(header->value);
		list_add(dest->headers, copy);
	}
	dest->internal_date = calloc(1, sizeof(struct tm));
	memcpy(dest->internal_date, source->internal_date, sizeof(struct tm));
	return dest;
}

struct aerc_mailbox *serialize_mailbox(struct mailbox *source) {
	struct aerc_mailbox *dest = calloc(1, sizeof(struct mailbox));
	dest->name = strdup(source->name);
	dest->exists = source->exists;
	dest->recent = source->recent;
	dest->unseen = source->unseen;
	dest->selected = source->selected;
	dest->flags = create_list();
	for (size_t i = 0; i < source->flags->length; ++i) {
		struct mailbox_flag *flag = source->flags->items[i];
		// TODO: Send along the permanent bool as well
		list_add(dest->flags, strdup(flag->name));
	}
	dest->messages = create_list();
	for (size_t i = 0; i < source->messages->length; ++i) {
		list_add(dest->messages, serialize_message(source->messages->items[i]));
	}
	return dest;
}

static void update_mailbox(struct imap_connection *imap, struct mailbox *updated) {
	/*
	 * Some detail about the mailbox has changed. Re-serialize it and re-send it
	 * to the main thread.
	 */
	struct aerc_mailbox *mbox = serialize_mailbox(updated);
	struct worker_pipe *pipe = imap->data;
	worker_post_message(pipe, WORKER_MAILBOX_UPDATED, NULL, mbox);
}

static void update_message(struct imap_connection *imap,
		struct mailbox_message *msg) {
	struct aerc_message *aerc_msg = serialize_message(msg);
	struct worker_pipe *pipe = imap->data;
	worker_post_message(pipe, WORKER_MESSAGE_UPDATED, NULL, aerc_msg);
}

static void delete_mailbox(struct imap_connection *imap, const char *mailbox) {
	struct worker_pipe *pipe = imap->data;
	worker_post_message(pipe, WORKER_MAILBOX_DELETED, NULL, strdup(mailbox));
}

void *imap_worker(void *_pipe) {
	/*
	 * The IMAP worker's main thread. Receives messages over the async queue and
	 * passes them to message handlers.
	 */
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	struct imap_connection *imap = calloc(1, sizeof(struct imap_connection));
	pipe->data = imap;
	imap->data = pipe;
	imap->events.mailbox_updated = update_mailbox;
	imap->events.mailbox_deleted = delete_mailbox;
	imap->events.message_updated = update_message;
	worker_log(L_DEBUG, "Starting IMAP worker");
	while (1) {
		/*
		 * First, we check for any actions the main thread has requested us to
		 * perform.
		 */
		if (worker_get_action(pipe, &message)) {
			/*
			 * With each action, we check if it's asking us to close down the
			 * thread, or we pass it along to the message handlers.
			 */
			if (message->type == WORKER_END) {
				imap_close(imap);
				free(imap);
				worker_message_free(message);
				return NULL;
			} else {
				handle_message(pipe, message);
			}
			worker_message_free(message);
		}
		/* 
		 * Then, we do the usual IMAP connection housekeeping, receiving new
		 * messages and passing them along to various handlers.
		 */
		imap_receive(imap);

		/*
		 * Finally, we sleep for a bit and then loop back around again.
		 */
		struct timespec spec = { 0, .5e+8 };
		nanosleep(&spec, NULL);
	}
	return NULL;
}
