/*
 * imap/worker/fetch.c - Handles the WORKER_FETCH_* actions
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include "worker.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "log.h"

void handle_worker_fetch_messages(struct worker_pipe *pipe,
		struct worker_message *message) {
	// TODO
}
