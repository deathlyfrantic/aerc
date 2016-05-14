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
};

#endif
