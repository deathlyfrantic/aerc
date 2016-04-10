#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "worker.h"
#include "imap/worker.h"
#include "urlparse.h"

void handle_worker_message(struct worker_pipe *pipe, struct worker_message *msg) {
	switch (msg->type) {
	case WORKER_CONNECT_DONE:
		fprintf(stderr, "Connected.\n");
		break;
	case WORKER_CONNECT_ERROR:
		fprintf(stderr, "Error connecting to mail service.\n");
		break;
	default:
		// No-op
		break;
	}
}

int main(int argc, char **argv) {
	struct worker_pipe *worker_pipe = worker_pipe_new();

	pthread_t worker;
	pthread_create(&worker, NULL, imap_worker, worker_pipe);

	struct worker_connect_info *info = calloc(1,
			sizeof(struct worker_connect_info));
	// TODO: configuration
	info->connection_string = getenv("CS");
	if (!info->connection_string || strlen(info->connection_string) == 0) {
		fprintf(stderr, "Usage: CS='connection string' %s\n", argv[0]);
		exit(1);
	}
	worker_post_action(worker_pipe, WORKER_CONNECT, NULL, info);

	while (1) {
		struct worker_message *msg;
		if (worker_get_message(worker_pipe, &msg)) {
			handle_worker_message(worker_pipe, msg);
			worker_message_free(msg);
		}

		struct timespec spec = { 0, 2.5e+8 };
		nanosleep(&spec, NULL);
	}
	return 0;
}
