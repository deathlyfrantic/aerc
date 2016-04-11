#define _POSIX_C_SOURCE 201112LL
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#ifdef USE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#endif
#include "log.h"
#include "absocket.h"

void abs_init() {
#ifdef USE_OPENSSL
	SSL_load_error_strings();
	SSL_library_init();
#endif
}

absocket_t *absocket_new(const char *host, const char *port, bool use_ssl) {
	absocket_t *abs = calloc(1, sizeof(absocket_t));

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *result, *rp;
	int s;
	if ((s = getaddrinfo(host, port, &hints, &result))) {
		worker_log(L_ERROR, "Connection failed: %s", gai_strerror(s));
		return NULL;
	}
	int err = -1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		abs->basefd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (abs->basefd == -1) {
			continue;
		}
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
	abs->use_ssl = use_ssl;
	if (use_ssl) {
#ifndef USE_OPENSSL
		worker_log(L_ERROR, "aerc was compiled without SSL support");
		free_absocket(abs);
		return NULL;
#else
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
		// Negotiate
		SSL_set_mode(abs->ssl, SSL_MODE_AUTO_RETRY);
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
			return NULL;
		}
		abs->cert = SSL_get_peer_certificate(abs->ssl);
		if (!abs->cert) {
			worker_log(L_ERROR, "Unable to get peer certificate");
			return NULL;
		}
		worker_log(L_INFO, "%s connection established using %s (%s)",
				SSL_get_version(abs->ssl),
				SSL_get_cipher_version(abs->ssl),
				SSL_get_cipher_name(abs->ssl));
#endif
	}
	return abs;
#ifdef USE_OPENSSL
bail_ssl:
	free(abs->ctx);
bail_sslctx:
	free(abs->ssl);
bail_abs:
	free_absocket(abs);
	return NULL;
#endif
}

void free_absocket(absocket_t *socket) {
	close(socket->basefd);
	free(socket);
}

ssize_t ab_recv(absocket_t *socket, void *buffer, size_t len, int flags) {
	if (socket->use_ssl) {
		return SSL_read(socket->ssl, buffer, len);
	} else {
		return recv(socket->basefd, buffer, len, flags);
	}
}
