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
		return false;
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
		return false;
	}
	freeaddrinfo(result);
	abs->use_ssl = use_ssl;
	if (use_ssl) {
		// TODO
	}
	return abs;
}

void free_absocket(absocket_t *socket) {
	close(socket->basefd);
	free(socket);
}

ssize_t ab_recv(absocket_t *socket, void *buffer, size_t len, int flags) {
	if (socket->use_ssl) {
		// TODO
		return -1;
	} else {
		return recv(socket->basefd, buffer, len, flags);
	}
}
