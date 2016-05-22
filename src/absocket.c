/*
 * absocket.c - abstract socket implementation
 *
 * Abstracts reads/writes on a socket to optionally support TLS/SSL
 */
#define _POSIX_C_SOURCE 201112LL

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef USE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#else
#include <assert.h>
#endif

#include "log.h"
#include "absocket.h"
#include "urlparse.h"

void abs_init() {
#ifdef USE_OPENSSL
	SSL_load_error_strings();
	SSL_library_init();
#endif
}

#ifdef USE_OPENSSL

static bool ab_ssl_negotiate(absocket_t *abs) {
	/*
	 * This function performs SSL negotiation over the given socket.
	 */
	SSL_set_mode(abs->ssl, SSL_MODE_AUTO_RETRY);
	int err;
	if ((err = SSL_connect(abs->ssl)) != 1) {
		const char *errmsg;
		switch (SSL_get_error(abs->ssl, err))
		{
		case SSL_ERROR_SYSCALL:
			errmsg = "I/O error";
			break;
		case SSL_ERROR_SSL:
			errmsg = ERR_error_string(ERR_get_error(), NULL);
			break;
		default:
			errmsg = "Unknown error";
			break;
		}
		worker_log(L_ERROR, "SSL error %s", errmsg);
		return false;
	}
	/*
	 * Grab the certificate from the server. We may later need to present it to
	 * the user for approval (or simply check it for validatity against the
	 * local certificate store).
	 */
	abs->cert = SSL_get_peer_certificate(abs->ssl);
	if (!abs->cert) {
		worker_log(L_ERROR, "Unable to get peer certificate");
		return false;
	}
	worker_log(L_DEBUG, "%s connection established using %s (%s)",
			SSL_get_version(abs->ssl),
			SSL_get_cipher_version(abs->ssl),
			SSL_get_cipher_name(abs->ssl));
	return true;
}

static bool ab_ssl_connect(absocket_t *abs) {
	/*
	 * Initializes and configures the SSL context, then runs the socket through
	 * ab_ssl_negotiate and we're golden. This function assumes that the
	 * connection has already been established, it just configures SSL for the
	 * socket.
	 */
	long ssl_options = 0;
	abs->ctx = SSL_CTX_new(SSLv23_client_method());
	if (!abs->ctx) {
		worker_log(L_ERROR, "Unable to allocate SSL context");
		goto bail_abs;
	}
	// TODO: Make ssl_options customizable
	ssl_options |= SSL_OP_NO_SSLv3 | SSL_OP_NO_SSLv2;
	if (!SSL_CTX_set_options(abs->ctx, ssl_options)) {
		worker_log(L_ERROR, "Unable to set SSL options");
		goto bail_sslctx;
	}
	// TODO: client certificates
	//SSL_CTX_set_cipher_list(abs->ctx, ""); // TODO: configurable
	abs->ssl = SSL_new(abs->ctx);
	if (!abs->ssl) {
		worker_log(L_ERROR, "Unable to allocate SSL");
		goto bail_sslctx;
	}
	if (SSL_set_fd(abs->ssl, abs->basefd) != 1) {
		worker_log(L_ERROR, "Unable to set SSL fd");
		goto bail_ssl;
	}
	if (!ab_ssl_negotiate(abs)) {
		goto bail_ssl;
	}
	return true;
bail_ssl:
	free(abs->ctx);
bail_sslctx:
	free(abs->ssl);
bail_abs:
	absocket_free(abs);
	return false;
}

#endif

absocket_t *absocket_new(const struct uri *uri, bool use_ssl) {
	/*
	 * Creates an absocket_t and connects to the provided server.
	 */
	absocket_t *abs = calloc(1, sizeof(absocket_t));

	/*
	 * Here we prepare an addrinfo struct for the DNS lookup.
	 */
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	/*
	 * Actually perform the DNS lookup:
	 */
	struct addrinfo *result, *rp;
	int s;
	if ((s = getaddrinfo(uri->hostname, uri->port, &hints, &result))) {
		worker_log(L_ERROR, "Connection failed: %s", gai_strerror(s));
		return NULL;
	}
	/*
	 * For each record returned from the DNS lookup, attempt to connect to it.
	 * struct addrinfo is a linked list of records.
	 */
	int err = -1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		/*
		 * Create a socket for the correct address family and such. This might
		 * fail, for example, if you don't have IPv6 on your system and the DNS
		 * lookup returned AAAA records.
		 */
		abs->basefd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (abs->basefd == -1) {
			continue;
		}
		/*
		 * Establish the actual TCP connection.
		 */
		if (connect(abs->basefd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		err = errno;
		close(abs->basefd);
	}
	if (rp == NULL) {
		freeaddrinfo(result);
		worker_log(L_ERROR, "Connection failed: %s", strerror(err));
		return NULL;
	}
	freeaddrinfo(result);
	/*
	 * If asked to use SSL and it was compiled in, continue with ab_ssl_connect.
	 */
	abs->use_ssl = use_ssl;
	if (use_ssl) {
#ifndef USE_OPENSSL
		worker_log(L_ERROR, "aerc was compiled without SSL support");
		absocket_free(abs);
		return NULL;
#else
		if (!ab_ssl_connect(abs)) {
			absocket_free(abs);
			return NULL;
		}
#endif
	}
	return abs;
}

void absocket_free(absocket_t *socket) {
	if (!socket) return;
	if (socket->use_ssl) {
#ifdef USE_OPENSSL
		SSL_shutdown(socket->ssl);
		SSL_free(socket->ssl);
		SSL_CTX_free(socket->ctx);
#endif
	}
	close(socket->basefd);
	free(socket);
}

ssize_t ab_recv(absocket_t *socket, void *buffer, size_t len) {
	/*
	 * Depending on whether or not SSL was enabled, this function will either
	 * call the POSIX recv function or abstract it over the OpenSSL SSL_read
	 * function.
	 */
	if (socket->use_ssl) {
#ifdef USE_OPENSSL
		return SSL_read(socket->ssl, buffer, len);
#else
		assert(false);
		return -1;
#endif
	} else {
		return recv(socket->basefd, buffer, len, 0);
	}
}

ssize_t ab_send(absocket_t *socket, void *buffer, size_t len) {
	/*
	 * Depending on whether or not SSL was enabled, this function will either
	 * call the POSIX send function or abstract it over the OpenSSL SSL_write
	 * function.
	 */
	if (socket->use_ssl) {
#ifdef USE_OPENSSL
		return SSL_write(socket->ssl, buffer, len);
#else
		assert(false);
		return -1;
#endif
	} else {
		return send(socket->basefd, buffer, len, 0);
	}
}
