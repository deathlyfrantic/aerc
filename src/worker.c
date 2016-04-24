/*
 * worker.c - support code for mail workers
 */
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "worker.h"
#include "util/aqueue.h"

struct worker_pipe *worker_pipe_new() {
	/*
	 * Creates a pipe with two aqueue_t-s, one for the worker to push messages
	 * through and one for the main thread to push actions through.
	 */
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

static bool _worker_get(aqueue_t *queue,
		struct worker_message **message) {
	/*
	 * Gets the next worker_message from an arbitrary aqueue_t.
	 */
	void *msg;
	if (aqueue_dequeue(queue, &msg)) {
		*message = (struct worker_message *)msg;
		return true;
	}
	return false;
}

bool worker_get_message(struct worker_pipe *pipe,
		struct worker_message **message) {
	/*
	 * Gets the next worker->master message.
	 */
	return _worker_get(pipe->messages, message);
}

bool worker_get_action(struct worker_pipe *pipe,
		struct worker_message **message) {
	/*
	 * Gets the next master->worker action.
	 */
	return _worker_get(pipe->actions, message);
}

void _worker_post(aqueue_t *queue,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data) {
	/*
	 * Posts a new worker_message to an arbitrary aqueue_t.
	 */
	struct worker_message *message = calloc(1, sizeof(struct worker_message));
	if (!message) {
		fprintf(stderr, "Unable to allocate messages, aborting worker thread");
		pthread_exit(NULL);
		return;
	}
	message->type = type;
	message->in_response_to = in_response_to;
	message->data = data;
	aqueue_enqueue(queue, message);
}

void worker_post_message(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data) {
	/*
	 * Posts a new worker->master message.
	 */
	_worker_post(pipe->messages, type, in_response_to, data);
}

void worker_post_action(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data) {
	/*
	 * Posts a new master->worker message.
	 */
	_worker_post(pipe->actions, type, in_response_to, data);
}

void worker_message_free(struct worker_message *msg) {
	free(msg);
}
