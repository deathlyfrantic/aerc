/*
 * imap/worker/select.c - Handles IMAP worker mailbox select actions
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

void handle_worker_select_mailbox(struct worker_pipe *pipe, struct worker_message *message) {
	/*
	 * This issues two IMAP commands - one to find the delimiter, and one to
	 * do the actual listing.
	 */
	struct imap_connection *imap = pipe->data;
	worker_post_message(pipe, WORKER_ACK, message, NULL);
	imap_select(imap, NULL, NULL, (const char *)message->data);
}
