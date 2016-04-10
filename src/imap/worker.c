#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#include "worker.h"

void handle_message(struct worker_pipe *pipe, struct worker_message *message) {
	switch (message->type) {
	case WORKER_PING:
		worker_post_message(pipe, WORKER_ACK, message, NULL);
		break;
	default:
		worker_post_message(pipe, WORKER_UNSUPPORTED, message, NULL);
		break;
	}
}

void *imap_worker(void *_pipe) {
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	while (1) {
		if (worker_get_action(pipe, &message)) {
			if (message->type == WORKER_END) {
				// TODO: Cleanup?
				return NULL;
			} else {
				handle_message(pipe, message);
			}
		}

		struct timespec spec = { 0, 2.5e+8 };
		nanosleep(&spec, NULL);
	}
	return NULL;
}
