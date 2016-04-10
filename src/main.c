#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "worker.h"
#include "imap/worker.h"

int main(int argc, char **argv) {
	struct worker_pipe *worker_pipe = worker_pipe_new();

	pthread_t worker;
	pthread_create(&worker, NULL, imap_worker, worker_pipe);

	while (1) {
		struct worker_message *msg;
		if (worker_get_message(worker_pipe, &msg)) {
			printf("Got message\n");
			worker_message_free(msg);
		}

		struct timespec spec = { 0, 2.5e+8 };
		nanosleep(&spec, NULL);

		if (pthread_kill(worker, 0) == ESRCH) {
			fprintf(stderr, "Worker thread died! Exiting aerc.\n");
			break;
		}
	}
	return 0;
}
