#ifndef _IMAP_H
#define _IMAP_H

// Implements RFC 3501 (Internet Mail Access Protocol)

#include <stdbool.h>
#include <poll.h>
#include <stdarg.h>
#include "util/hashtable.h"
#include "urlparse.h"
#include "absocket.h"

// TODO: Refactor these into the internal header:
// - recv_mode
// - struct imap_connection

struct imap_capabilities {
    bool imap4rev1;
    bool starttls;
    bool logindisabled;
    bool auth_plain;
    bool auth_login;
    bool idle;
};

enum imap_status {
    STATUS_OK, STATUS_NO, STATUS_BAD, STATUS_PREAUTH, STATUS_BYE
};

enum recv_mode {
    RECV_WAIT,
    RECV_LINE,
    RECV_BULK
};

struct imap_connection;

typedef void (*imap_callback_t)(struct imap_connection *imap,
        void *data, enum imap_status status, const char *args);

struct imap_state {
    char *selected_mailbox;
};

struct imap_connection {
    bool logged_in;
    absocket_t *socket;
    enum recv_mode mode;
    char *line;
    int line_index, line_size;
    struct pollfd poll[1];
    int next_tag;
    hashtable_t *pending;
    struct imap_capabilities *cap;
    struct imap_state *state;
    struct uri *uri;
};

bool imap_connect(struct imap_connection *imap, const char *host,
		const char *port, bool use_ssl, imap_callback_t callback, void *data);
void imap_receive(struct imap_connection *imap);
void imap_send(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *fmt, ...);
void imap_close(struct imap_connection *imap);

void imap_list(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *refname, const char *boxname);
void imap_capability(struct imap_connection *imap, imap_callback_t callback,
        void *data);

#endif
