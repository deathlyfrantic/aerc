/*
 * imap/worker/delete.c - Handles IMAP worker delete actions
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include "imap/imap.h"
#include "worker.h"

void handle_worker_delete_mailbox(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	worker_post_message(pipe, WORKER_ACK, message, NULL);
	imap_delete(imap, NULL, NULL, (const char *)message->data);
}
