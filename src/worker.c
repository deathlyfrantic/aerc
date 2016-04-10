#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "worker.h"
#include "util/aqueue.h"

struct worker_pipe *worker_pipe_new() {
	struct worker_pipe *pipe = calloc(1, sizeof(struct worker_pipe));
	if (!pipe) return NULL;
	pipe->messages = aqueue_new();
	pipe->actions = aqueue_new();
	if (!pipe->messages || !pipe->actions) {
		aqueue_free(pipe->messages);
		aqueue_free(pipe->actions);
		free(pipe);
		return NULL;
	}
	return pipe;
}

void worker_pipe_free(struct worker_pipe *pipe) {
	aqueue_free(pipe->messages);
	aqueue_free(pipe->actions);
	free(pipe);
}

bool worker_get_message(struct worker_pipe *pipe,
		struct worker_message **message) {
	void *msg;
	if (aqueue_dequeue(pipe->messages, &msg)) {
		*message = (struct worker_message *)msg;
		return true;
	}
	return false;
}

bool worker_get_action(struct worker_pipe *pipe,
		struct worker_message **message) {
	// Only semantically different
	return worker_get_message(pipe, message);
}

void worker_post_message(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data) {
	struct worker_message *message = calloc(1, sizeof(struct worker_message));
	if (!message) {
		fprintf(stderr, "Unable to allocate messages, aborting worker thread");
		pthread_exit(NULL);
		return;
	}
	message->type = type;
	message->in_response_to = in_response_to;
	message->data = data;
	aqueue_enqueue(pipe->messages, message);
}

void worker_post_action(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data) {
	// Only semantically different
	worker_post_message(pipe, type, in_response_to, data);
}

void worker_message_free(struct worker_message *msg) {
	free(msg);
}
