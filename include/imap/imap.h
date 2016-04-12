#ifndef _IMAP_H
#define _IMAP_H

#include <stdbool.h>
#include <poll.h>
#include "absocket.h"
#include "worker.h"

enum recv_mode {
    RECV_WAIT,
    RECV_LINE,
    RECV_BULK
};

struct imap_connection {
    absocket_t *socket;
    enum recv_mode mode;
    char *line;
    int line_index, line_size;
    struct pollfd poll[1];
};

bool imap_connect(struct imap_connection *imap, const char *host,
        const char *port, bool use_ssl);
void imap_receive(struct imap_connection *imap);
void imap_close(struct imap_connection *imap);

#endif
