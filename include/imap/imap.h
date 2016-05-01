#ifndef _IMAP_H
#define _IMAP_H

// Implements RFC 3501 (Internet Mail Access Protocol)

#include <stdbool.h>
#include <poll.h>
#include <stdarg.h>
#include "util/list.h"
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
    STATUS_OK, STATUS_NO, STATUS_BAD, STATUS_PREAUTH, STATUS_BYE,
    STATUS_PRE_ERROR // Returned when our code anticipates an error
};

enum recv_mode {
    RECV_WAIT,
    RECV_LINE,
    RECV_BULK
};

struct imap_connection;

typedef void (*imap_callback_t)(struct imap_connection *imap,
        void *data, enum imap_status status, const char *args);

struct mailbox_flag {
    char *name;
    bool permanent;
};

struct mailbox {
    list_t *flags;
    list_t *messages;
    char *name;
    int exists, recent, unseen;
    int nextuid; // Predicted, not definite
    bool read_write;
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
    list_t *mailboxes;
    char *selected;
};

bool imap_connect(struct imap_connection *imap, const struct uri *uri,
		bool use_ssl, imap_callback_t callback, void *data);
void imap_receive(struct imap_connection *imap);
void imap_send(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *fmt, ...);
void imap_close(struct imap_connection *imap);

void imap_list(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *refname, const char *boxname);
void imap_capability(struct imap_connection *imap, imap_callback_t callback,
        void *data);
void imap_select(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox);

#endif
