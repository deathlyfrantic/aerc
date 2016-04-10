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
#include <poll.h>
#include "imap/imap.h"
#include "worker.h"
#include "log.h"
#define BUFFER_SIZE 1024

void imap_receive(struct imap_connection *imap) {
	poll(imap->poll, 1, -1);
	if (imap->poll[0].revents & POLLIN) {
		if (imap->mode == RECV_LINE) {
			ssize_t amt = recv(imap->sockfd, imap->line + imap->line_index,
					imap->line_size - imap->line_index, 0);
			imap->line_index += amt;
			if (imap->line_index == imap->line_size) {
				imap->line = realloc(imap->line,
						imap->line_size + BUFFER_SIZE + 1);
				memset(imap->line + imap->line_index + 1, 0,
						(imap->line_size - imap->line_index + 1));
				imap->line_size = imap->line_size + BUFFER_SIZE;
			}
			char *esc;
			while ((esc = strstr(imap->line, "\r\n"))) {
				*esc = '\0';
				worker_log(L_DEBUG, "[IMAP] %s", imap->line);
				memmove(imap->line, esc + 2, imap->line_size - (esc - imap->line + 2));
			}
		}
	}
}

void imap_init(struct imap_connection *imap) {
	imap->mode = RECV_LINE;
	imap->line = calloc(1, BUFFER_SIZE + 1);
	imap->line_index = 0;
	imap->line_size = BUFFER_SIZE;
}

bool imap_connect(struct imap_connection *imap, const char *host,
		const char *port, bool use_ssl) {
	imap_init(imap);
	if (use_ssl) {
		worker_log(L_ERROR, "TODO: SSL");
		return false;
	}
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
		imap->sockfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (imap->sockfd == -1) {
			continue;
		}
		if (connect(imap->sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		err = errno;
		close(imap->sockfd);
	}
	if (rp == NULL) {
		freeaddrinfo(result);
		worker_log(L_ERROR, "Connection failed: %s", strerror(err));
		return false;
	}
	freeaddrinfo(result);
	imap->poll[0].fd = imap->sockfd;
	imap->poll[0].events = POLLIN;
	return true;
}
