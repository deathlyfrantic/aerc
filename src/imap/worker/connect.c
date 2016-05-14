/*
 * imap/worker/connect.c - Handles IMAP worker connect actions
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worker.h"
#include "urlparse.h"
#include "worker.h"
#include "imap/imap.h"
#include "imap/worker.h"
#include "log.h"

void handle_imap_ready(struct imap_connection *imap, void *data,
		enum imap_status status, const char *args);

void handle_worker_connect(struct worker_pipe *pipe, struct worker_message *message) {
	/*
	 * The main thread has asked us to establish an IMAP connection. Let's do
	 * it!
	 */
	struct imap_connection *imap = pipe->data;
	worker_post_message(pipe, WORKER_ACK, message, NULL);

	/*
	 * We'll have been passed a URI to connect to in the message, something like
	 * this: imap[s]://user:password@host:port. We'll parse the connection
	 * string first.
	 */
	struct uri *uri = malloc(sizeof(struct uri));
	if (!parse_uri(uri, (char *)message->data)) {
		worker_log(L_ERROR, "Invalid connection string '%s'",
			(char*)message->data);
	}
	/*
	 * Check for SSL or unsupported protocols.
	 */
	bool ssl = false;
	if (strcmp(uri->scheme, "imap") == 0) {
		ssl = false;
	} else if (strcmp(uri->scheme, "imaps") == 0) {
		ssl = true;
	} else {
		worker_log(L_ERROR, "Unsupported protocol '%s'", uri->scheme);
		return;
	}
	/*
	 * If the user didn't specify a port, use the default as appropriate for the
	 * requested protocol.
	 */
	if (!uri->port) {
		uri->port = strdup(ssl ? "993" : "143");
	}
	worker_log(L_DEBUG, "Connecting to IMAP server");
	/*
	 * Connect!
	 */
	bool res = imap_connect(imap, uri, ssl, handle_imap_ready, pipe);
	if (res) {
		worker_log(L_DEBUG, "Connected to IMAP server");
		if (ssl) {
			/*
			 * If we're using SSL, we need to wait to start doing IMAP
			 * housekeeping until the main thread approves of the certificate.
			 */
#ifdef USE_OPENSSL
			struct cert_check_message *ccm = calloc(1,
					sizeof(struct cert_check_message));
			ccm->cert = imap->socket->cert;
			worker_post_message(pipe, WORKER_CONNECT_CERT_CHECK, message, ccm);
#endif
		} else {
			/*
			 * Otherwise, go for it.
			 */
			imap->mode = RECV_LINE;
		}
	} else {
		worker_log(L_DEBUG, "Error connecting to IMAP server");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, message, NULL);
	}
	imap->uri = uri;
}

void handle_worker_cert_okay(struct worker_pipe *pipe, struct worker_message *message) {
	/*
	 * The main thread has given the go-ahead to proceed with the certificate
	 * provided by the server.
	 */
	struct imap_connection *imap = pipe->data;
	imap->mode = RECV_LINE;
}

void handle_imap_logged_in(struct imap_connection *imap, void *data,
		enum imap_status status, const char *args) {
	/*
	 * Once the log in is complete, we can tell the main thread that we're
	 * logged in and it can start asking us to open mailboxes and such.
	 */
	struct worker_pipe *pipe = data;
	if (status == STATUS_OK) {
		worker_post_message(pipe, WORKER_CONNECT_DONE, NULL, NULL);
	} else {
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, strdup(args));
	}
}

void handle_imap_cap(struct imap_connection *imap, void *data,
		enum imap_status status, const char *args) {
	/*
	 * We have received the server's capabilities and we can now look over them
	 * and attempt to log in.
	 */
	struct worker_pipe *pipe = data;
	if (status != STATUS_OK) {
		worker_log(L_ERROR, "IMAP error: %s", args);
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
		return;
	}
	/*
	 * Only super ancient IMAP servers do not support IMAP4rev1, and we don't
	 * bother supporting anything older than that.
	 */
	if (!imap->cap->imap4rev1) {
		worker_log(L_ERROR, "IMAP server does not support IMAP4rev1");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
		return;
	}
	/*
	 * If we're already logged in, we just received an update to the server's
	 * capabilities (likely as a result of logging in), so we don't need to
	 * attempt another log in.
	 */
	if (imap->logged_in) return;
	/*
	 * Otherwise, let's look through our authentication options and attempt to
	 * log in. The easiest route is PREAUTH, where we were already authenticated
	 * by means of a client side certificate or something.
	 */
	if (status == STATUS_PREAUTH) {
		imap->logged_in = true;
		worker_post_message(pipe, WORKER_CONNECT_DONE, NULL, NULL);
	} else if (imap->cap->auth_login) {
		if (imap->uri->username && imap->uri->password) {
			imap->logged_in = true;
			// TODO: Use literal strings?
			imap_send(imap, handle_imap_logged_in, pipe, "LOGIN \"%s\" \"%s\"",
					imap->uri->username, imap->uri->password);
		}
	} else {
		// TODO: Raise as an error
		worker_log(L_DEBUG, "IMAP server and client do not share any supported "
				"authentication mechanisms. Did you provide a username/password?");
		worker_post_message(pipe, WORKER_CONNECT_ERROR, NULL, NULL);
	}
}

void handle_imap_ready(struct imap_connection *imap, void *data,
		enum imap_status status, const char *args) {
	/*
	 * This callback is invoked after the connection is established and the
	 * server is ready to exchange login info and such. We may have received the
	 * capabilities immediately, but if not we will go ahead and ask the server
	 * for them before proceeding.
	 */
	struct worker_pipe *pipe = data;
	if (!imap->cap) {
		imap_capability(imap, handle_imap_cap, pipe);
		return;
	}
	handle_imap_cap(imap, pipe, STATUS_OK, NULL);
}
