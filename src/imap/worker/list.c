/*
 * imap/worker/list.c - Handles IMAP worker list actions
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worker.h"
#include "worker.h"
#include "util/list.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "log.h"

struct list_data {
	struct worker_pipe *pipe;
	struct worker_message *message;
};

void imap_list_callback(struct imap_connection *imap,
        void *_data, enum imap_status status, const char *args) {
	/*
	 * Listing complete. Process some things and let the main thread know.
	 */
	struct list_data *data = _data;
	if (status == STATUS_OK) {
		list_t *mboxes = create_list();
		for (int i = 0; i < imap->mailboxes->length; ++i) {
			struct mailbox *source = imap->mailboxes->items[i];
			struct aerc_mailbox *dest = calloc(1, sizeof(struct mailbox));
			dest->name = strdup(source->name);
			dest->flags = create_list();
			for (int j = 0; j < source->flags->length; ++j) {
				list_add(dest->flags, strdup(source->flags->items[j]));
			}
			list_add(mboxes, dest);
		}
		worker_post_message(data->pipe, WORKER_LIST_DONE, data->message, mboxes);
	} else {
		worker_post_message(data->pipe, WORKER_LIST_ERROR, data->message, NULL);
	}
	free(data);
}

void handle_worker_list(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	struct list_data *data = malloc(sizeof(struct list_data));
	data->pipe = pipe; data->message = message;
	worker_post_message(pipe, WORKER_ACK, message, NULL);
	imap_list(imap, imap_list_callback, data, "", "%");
}
