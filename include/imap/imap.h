#ifndef _IMAP_H
#define _IMAP_H

#include <stdbool.h>
#include "worker.h"

struct imap_connection {
    int sockfd;
};

bool imap_connect(struct imap_connection *imap, const char *host,
        const char *port, bool use_ssl);

#endif
