#ifndef _WORKER_H
#define _WORKER_H
#include "util/aqueue.h"
#include "util/list.h"
#ifdef USE_OPENSSL
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

/* worker.h
 *
 * Defines an abstract interface to an asynchronous mail worker.
 *
 * Messages are passed through an atomic queue with actions and messages.
 * Whenever passing extra data with a message, ownership of that data is
 * transfered to the recipient.
 */

enum worker_message_type {
    /* Basics */
    WORKER_ACK,
    WORKER_END,
    WORKER_OOM,
    WORKER_UNSUPPORTED,
    WORKER_CONFIGURE,
    /* Connection */
    WORKER_CONNECT,
    WORKER_CONNECT_DONE,
    WORKER_CONNECT_ERROR,
#ifdef USE_OPENSSL
    WORKER_CONNECT_CERT_CHECK,
    WORKER_CONNECT_CERT_OKAY,
#endif
    /* Listing */
    WORKER_LIST,
    WORKER_LIST_DONE,
    WORKER_LIST_ERROR,
    /* Selection */
    WORKER_SELECT_MAILBOX,
    WORKER_SELECT_MAILBOX_DONE,
    WORKER_SELECT_MAILBOX_ERROR,
    /* Notifications */
    WORKER_MAILBOX_UPDATED,
};

struct worker_pipe {
    /* Messages from master->worker */
    aqueue_t *actions;
    /* Messages from worker->master */
    aqueue_t *messages;
    /* Arbitrary worker-specific data */
    void *data;
};

struct worker_message {
    enum worker_message_type type;
    struct worker_message *in_response_to;
    void *data;
};

struct aerc_mailbox {
    char *name;
    bool read_write;
    long exists, recent, unseen;
    list_t *flags;
    list_t *messages;
};

#ifdef USE_OPENSSL
struct cert_check_message {
    X509 *cert;
};
#endif

/* Misc */
struct worker_pipe *worker_pipe_new();
void worker_pipe_free(struct worker_pipe *pipe);
bool worker_get_message(struct worker_pipe *pipe,
		struct worker_message **message);
bool worker_get_action(struct worker_pipe *pipe,
		struct worker_message **message);
void worker_post_message(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data);
void worker_post_action(struct worker_pipe *pipe,
		enum worker_message_type type,
		struct worker_message *in_response_to,
		void *data);
void worker_message_free(struct worker_message *msg);

#endif
