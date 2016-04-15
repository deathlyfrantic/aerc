#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "worker.h"
#include "urlparse.h"
#include "imap/imap.h"
#include "log.h"

void handle_worker_connect(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	worker_post_message(pipe, WORKER_ACK, message, NULL);
	struct uri uri;
	if (!parse_uri(&uri, (char *)message->data)) {
		worker_log(L_ERROR, "Invalid connection string '%s'",
			(char*)message->data);
	}
	bool ssl = false;
	if (strcmp(uri.scheme, "imap") == 0) {
		ssl = false;
	} else if (strcmp(uri.scheme, "imaps") == 0) {
		ssl = true;
	} else {
		worker_log(L_ERROR, "Unsupported protocol '%s'", uri.scheme);
		return;
	}
	if (!uri.port) {
		uri.port = strdup(ssl ? "993" : "143");
	}
	worker_log(L_DEBUG, "Connecting to IMAP server");
	bool res = imap_connect(imap, uri.hostname, uri.port, ssl);
	if (res) {
		worker_log(L_INFO, "Connected to IMAP server");
		if (ssl) {
#ifdef USE_OPENSSL
			struct cert_check_message *ccm = calloc(1,
					sizeof(struct cert_check_message));
			ccm->cert = imap->socket->cert;
			worker_post_message(pipe, WORKER_CONNECT_CERT_CHECK, message, ccm);
#endif
		} else {
			imap->mode = RECV_LINE;
		}
	} else {
		worker_log(L_DEBUG, "Error connecting to IMAP server");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, message, NULL);
	}
	uri_free(&uri);
}

void handle_worker_cert_okay(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	imap->mode = RECV_LINE;
}

struct action_handler {
	enum worker_message_type action;
	void (*handler)(struct worker_pipe *pipe, struct worker_message *message);
};
struct action_handler handlers[] = {
	{ WORKER_CONNECT, handle_worker_connect },
	{ WORKER_CONNECT_CERT_OKAY, handle_worker_cert_okay }
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

void handle_imap_cap(struct imap_connection *imap,
		enum imap_status status, const char *args) {
	if (!imap->ready) {
		return;
	}
	struct worker_pipe *pipe = imap->data;
	if (status != STATUS_OK) {
		worker_log(L_ERROR, "IMAP error: %s", args);
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
		return;
	}
	if (!imap->cap->imap4rev1) {
		worker_log(L_ERROR, "IMAP server does not support IMAP4rev1");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
		return;
	}
	// TODO: Login
	worker_post_message(pipe, WORKER_CONNECT_DONE, NULL, NULL);
}

void handle_imap_ready(struct imap_connection *imap,
		enum imap_status status, const char *args) {
	struct worker_pipe *pipe = imap->data;
	if (!imap->cap) {
		imap_send(imap, handle_imap_cap, "CAPABILITY");
		return;
	}
	handle_imap_cap((struct imap_connection *)pipe->data, STATUS_OK, NULL);
}

void register_imap_handlers(struct imap_connection *imap,
		struct worker_pipe *pipe) {
	imap->data = pipe;
	imap->events.ready = handle_imap_ready;
	imap->events.cap = handle_imap_cap;
}

void *imap_worker(void *_pipe) {
	struct worker_pipe *pipe = _pipe;
	struct worker_message *message;
	struct imap_connection *imap = calloc(1, sizeof(struct imap_connection));
	register_imap_handlers(imap, pipe);
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
