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
			ssize_t amt = ab_recv(imap->socket, imap->line + imap->line_index,
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
	imap->socket = absocket_new(host, port, use_ssl);
	if (!imap->socket) {
		return false;
	}
	imap->poll[0].fd = imap->socket->basefd;
	imap->poll[0].events = POLLIN;
	return true;
}
