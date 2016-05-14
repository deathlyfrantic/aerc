#ifndef STATE_H
#define STATE_H

#include "util/list.h"
#include "worker.h"

struct aerc_mailbox {
    char *name;
    list_t *flags;
};

struct account_state {
    struct worker_pipe *pipe;
    list_t *mailboxes;
    char *selected;
    int selected_message;
    int list_offset;
};

struct aerc_state {
    int selected_account;
    list_t *accounts;
};

extern struct aerc_state state;

#endif
