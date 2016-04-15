#ifndef _IMAP_H
#define _IMAP_H

#include <stdbool.h>
#include <poll.h>
#include <stdarg.h>
#include "util/hashtable.h"
#include "absocket.h"
#include "worker.h"

enum recv_mode {
    RECV_WAIT,
    RECV_LINE,
    RECV_BULK
};

struct imap_capabilities {
    bool imap4rev1;
    bool starttls;
    bool logindisabled;
    bool auth_plain;
    bool auth_login;
};

enum imap_status {
    STATUS_OK, STATUS_NO, STATUS_BAD, STATUS_PREAUTH, STATUS_BYE
};

struct imap_connection;

typedef void (*imap_handler_t)(struct imap_connection *imap,
        const char *token, const char *cmd, const char *args);
typedef void (*imap_callback_t)(struct imap_connection *imap,
        enum imap_status status, const char *args);

struct imap_connection {
    bool ready;
    absocket_t *socket;
    enum recv_mode mode;
    char *line;
    int line_index, line_size;
    struct pollfd poll[1];
    int next_tag;
    hashtable_t *pending;
    void *data;
    struct {
        imap_callback_t ready;
        imap_callback_t cap;
    } events;
    struct imap_capabilities *cap;
};

struct imap_pending_callback {
    imap_callback_t callback;
    void *data;
};

bool imap_connect(struct imap_connection *imap, const char *host,
        const char *port, bool use_ssl);
void imap_receive(struct imap_connection *imap);
void imap_send(struct imap_connection *imap, imap_callback_t callback,
		const char *fmt, ...);
void imap_close(struct imap_connection *imap);
void handle_line(struct imap_connection *imap, const char *line);

// Handlers
void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args);
void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args);

#endif
