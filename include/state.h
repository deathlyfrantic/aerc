#ifndef STATE_H
#define STATE_H

#include <pthread.h>
#include <time.h>
#include "util/list.h"
#include "worker.h"

enum account_status {
    ACCOUNT_NOT_READY,
    ACCOUNT_OKAY,
    ACCOUNT_ERROR
};

struct account_state {
    struct {
        struct worker_pipe *pipe;
        pthread_t thread;
    } worker;

    struct {
        char *text;
        struct timespec since;
        enum account_status status;
    } status;

    struct {
        int selected_message;
        int list_offset;
    } ui;

    char *name;
    list_t *mailboxes;
    char *selected;
};

struct aerc_state {
    int selected_account;
    list_t *accounts;
};

extern struct aerc_state *state;

void set_status(struct account_state *account, enum account_status state,
        const char *text);

#endif
