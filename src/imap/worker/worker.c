#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "worker.h"
#include "urlparse.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "log.h"

struct action_handler {
	enum worker_message_type action;
	void (*handler)(struct worker_pipe *pipe, struct worker_message *message);
};

struct action_handler handlers[] = {
	{ WORKER_CONNECT, handle_worker_connect },
#ifdef USE_OPENSSL
	{ WORKER_CONNECT_CERT_OKAY, handle_worker_cert_okay }
#endif
};

void handle_message(struct worker_pipe *pipe, struct worker_message *message) {
	for (size_t i = 0; i < sizeof(handlers) / sizeof(struct action_handler); i++) {
		if (handlers[i].action == message->type) {
			handlers[i].handler(pipe, message);
			return;
		}
	}
	worker_post_message(pipe, WORKER_UNSUPPORTED, message, NULL);
}

void *imap_worker(void *_pipe) {
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	struct imap_connection *imap = calloc(1, sizeof(struct imap_connection));
	pipe->data = imap;
	worker_log(L_DEBUG, "Starting IMAP worker");
	while (1) {
		if (worker_get_action(pipe, &message)) {
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
		imap_receive(imap);

		struct timespec spec = { 0, .5e+8 };
		nanosleep(&spec, NULL);
	}
	return NULL;
}
