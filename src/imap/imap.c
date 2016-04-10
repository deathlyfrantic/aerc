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
#include "imap/imap.h"
#include "worker.h"

bool imap_connect(struct imap_connection *imap, const char *host,
		const char *port, bool use_ssl) {
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *result, *rp;
	int s;
	if ((s = getaddrinfo(host, port, &hints, &result))) {
		//worker_log(L_ERROR, "Connection failed: %s", gai_strerror(s));
		return false;
	}
	int err = -1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		imap->sockfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (imap->sockfd == -1) {
			continue;
		}
		if (connect(imap->sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			err = errno;
			break;
		}
		close(imap->sockfd);
	}
	if (rp == NULL) {
		freeaddrinfo(result);
		//worker_log(L_ERROR, "Connection failed: %s", strerror(err));
		return false;
	}
	freeaddrinfo(result);
	return true;
}
