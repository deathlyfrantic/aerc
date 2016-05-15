#ifndef STATE_H
#define STATE_H

#include <pthread.h>
#include "util/list.h"
#include "worker.h"

struct account_state {
    char *name;
    struct worker_pipe *pipe;
    pthread_t worker;
    list_t *mailboxes;
    char *selected;
    int selected_message;
    int list_offset;
};

struct aerc_state {
    int selected_account;
    list_t *accounts;
};

extern struct aerc_state *state;

struct aerc_mailbox *get_aerc_mailbox(const char *name);

#endif
