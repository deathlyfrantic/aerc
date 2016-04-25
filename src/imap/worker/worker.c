/*
 * imap/worker/worker.c - IMAP worker main thread and action dispatcher
 */
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
	{ WORKER_LIST, handle_worker_list },
#ifdef USE_OPENSSL
	{ WORKER_CONNECT_CERT_OKAY, handle_worker_cert_okay }
#endif
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

void *imap_worker(void *_pipe) {
	/*
	 * The IMAP worker's main thread. Receives messages over the async queue and
	 * passes them to message handlers.
	 */
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	struct imap_connection *imap = calloc(1, sizeof(struct imap_connection));
	pipe->data = imap;
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
