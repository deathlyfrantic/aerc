#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "worker.h"
#include "imap/imap.h"

void handle_message(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	switch (message->type) {
	case WORKER_CONNECT:
	{
		worker_post_message(pipe, WORKER_ACK, message, NULL);
		struct worker_connect_info *info = message->data;
		bool res = imap_connect(imap, "mail.cmpwn.com", "143", false);
		if (res) {
			worker_post_message(pipe, WORKER_CONNECT_DONE, message, NULL);
		} else {
			worker_post_message(pipe, WORKER_CONNECT_ERROR, message, NULL);
		}
		free(info);
		break;
	}
	default:
		worker_post_message(pipe, WORKER_UNSUPPORTED, message, NULL);
		break;
	}
}

void *imap_worker(void *_pipe) {
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	struct imap_connection *imap = calloc(1, sizeof(struct imap_connection));
	pipe->data = imap;
	while (1) {
		if (worker_get_action(pipe, &message)) {
			if (message->type == WORKER_END) {
				// TODO: Cleanup?
				worker_message_free(message);
				return NULL;
			} else {
				handle_message(pipe, message);
			}
			worker_message_free(message);
		}

		struct timespec spec = { 0, 2.5e+8 };
		nanosleep(&spec, NULL);
	}
	return NULL;
}
