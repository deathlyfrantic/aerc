#ifndef _IMAP_H
#define _IMAP_H

#include <stdbool.h>
#include <poll.h>
#include "util/hashtable.h"
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
    int next_tag;
    hashtable_t *pending;
    struct {
        void *data;
        void (*ready)(void *data);
    } events;
};

typedef void (*imap_handler_t)(struct imap_connection *imap,
        const char *token, const char *cmd, const char *args);

bool imap_connect(struct imap_connection *imap, const char *host,
        const char *port, bool use_ssl);
void imap_receive(struct imap_connection *imap);
void imap_close(struct imap_connection *imap);

// Handlers
void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args);

#endif
