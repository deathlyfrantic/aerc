#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "worker.h"
#include "urlparse.h"
#include "worker.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "log.h"

void handle_worker_connect(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	worker_post_message(pipe, WORKER_ACK, message, NULL);
	struct uri *uri = malloc(sizeof(struct uri));
	if (!parse_uri(uri, (char *)message->data)) {
		worker_log(L_ERROR, "Invalid connection string '%s'",
			(char*)message->data);
	}
	bool ssl = false;
	if (strcmp(uri->scheme, "imap") == 0) {
		ssl = false;
	} else if (strcmp(uri->scheme, "imaps") == 0) {
		ssl = true;
	} else {
		worker_log(L_ERROR, "Unsupported protocol '%s'", uri->scheme);
		return;
	}
	if (!uri->port) {
		uri->port = strdup(ssl ? "993" : "143");
	}
	worker_log(L_DEBUG, "Connecting to IMAP server");
	// TODO: imap_connect should just take a struct uri
	bool res = imap_connect(imap, uri->hostname, uri->port, ssl);
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
	imap->uri = uri;
}

void handle_worker_cert_okay(struct worker_pipe *pipe, struct worker_message *message) {
	struct imap_connection *imap = pipe->data;
	imap->mode = RECV_LINE;
}

void handle_imap_logged_in(struct imap_connection *imap,
		enum imap_status status, const char *args) {
	struct worker_pipe *pipe = imap->data;
	if (status == STATUS_OK) {
		worker_post_message(pipe, WORKER_CONNECT_DONE, NULL, NULL);
	} else {
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, strdup(args));
	}
}

void handle_imap_cap(struct imap_connection *imap,
		enum imap_status status, const char *args) {
	struct worker_pipe *pipe = imap->data;
	if (!imap->ready) return;
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
	if (imap->logged_in) return;
	if (status == STATUS_PREAUTH) {
		imap->logged_in = true;
		worker_post_message(pipe, WORKER_CONNECT_DONE, NULL, NULL);
	} else if (imap->cap->auth_login) {
		if (imap->uri->username && imap->uri->password) {
			imap->logged_in = true;
			imap_send(imap, handle_imap_logged_in, "LOGIN \"%s\" \"%s\"",
					imap->uri->username, imap->uri->password);
			// TODO: Escape quotes in user/pass
		}
	} else {
		worker_log(L_ERROR, "IMAP server and client do not share any supported "
				"authentication mechanisms. Did you provide a username/password?");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
	}
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
