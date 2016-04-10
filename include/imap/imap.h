#ifndef _IMAP_H
#define _IMAP_H

struct imap_connection {
    int sockfd;
};

int imap_connect(struct imap_connection *imap, const char *addr, int port);

#endif
