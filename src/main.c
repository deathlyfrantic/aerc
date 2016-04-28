/*
 * main.c - entry point and temporary test code
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef USE_OPENSSL
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif
#include "absocket.h"
#include "worker.h"
#include "imap/worker.h"
#include "log.h"

void handle_worker_message(struct worker_pipe *pipe, struct worker_message *msg) {
	/*
	 * Handle incoming messages from a worker. This is pretty bare right now.
	 */
	switch (msg->type) {
	case WORKER_CONNECT_DONE:
		fprintf(stderr, "Connection complete.\n");
		worker_post_action(pipe, WORKER_LIST, NULL, NULL);
		break;
	case WORKER_CONNECT_ERROR:
		fprintf(stderr, "Error connecting to mail service.\n");
		break;
	case WORKER_LIST_DONE:
		fprintf(stderr, "Mailboxes available:\n");
		list_t *mailboxes = msg->data;
		bool have_inbox = false;
		for (int i = 0; i < mailboxes->length; ++i) {
			struct aerc_mailbox *mbox = mailboxes->items[i];
			fprintf(stderr, "%s\n", mbox->name);
			if (strcmp(mbox->name, "INBOX") == 0) {
				have_inbox = true;
			}
		}
		if (have_inbox) {
			worker_post_action(pipe, WORKER_SELECT_MAILBOX, NULL, strdup("INBOX"));
		}
		break;
	case WORKER_LIST_ERROR:
		fprintf(stderr, "Error listing mailboxes!\n");
		break;
#ifdef USE_OPENSSL
	case WORKER_CONNECT_CERT_CHECK:
		fprintf(stderr, "TODO: interactive certificate check\n");
		worker_post_action(pipe, WORKER_CONNECT_CERT_OKAY, msg, NULL);
		break;
#endif
	default:
		// No-op
		break;
	}
}

int main(int argc, char **argv) {
	/*
	 * UI thread. Not much happening here yet.
	 */

	init_log(L_DEBUG); // TODO: Customizable
	abs_init();
	struct worker_pipe *worker_pipe = worker_pipe_new();

	pthread_t worker;
	pthread_create(&worker, NULL, imap_worker, worker_pipe);

	// TODO: configuration
	char *connection_string = getenv("CS");
	if (!connection_string || strlen(connection_string) == 0) {
		fprintf(stderr, "Usage: env CS='connection string' %s\n", argv[0]);
		exit(1);
	}
	worker_post_action(worker_pipe, WORKER_CONNECT, NULL, connection_string);

	while (1) {
		struct worker_message *msg;
		if (worker_get_message(worker_pipe, &msg)) {
			handle_worker_message(worker_pipe, msg);
			worker_message_free(msg);
		}

		struct timespec spec = { 0, .5e+8 };
		nanosleep(&spec, NULL);
	}
	return 0;
}
