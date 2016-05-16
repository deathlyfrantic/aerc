#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include "util/stringop.h"
#include "util/list.h"
#include "state.h"
#include "ui.h"

void set_status(struct account_state *account, enum account_status state,
        const char *text) {
	free(account->status.text);
	if (text == NULL) {
		text = "Unknown error occured";
	}
	account->status.text = strdup(text);
	account->status.status = state;
	clock_gettime(CLOCK_MONOTONIC, &account->status.since);
	rerender(); // TODO: just rerender the status bar
}

static int get_mbox_compare(const void *_mbox, const void *_name) {
	const struct aerc_mailbox *mbox = _mbox;
	const char *name = _name;
	return strcmp(mbox->name, name);
}

struct aerc_mailbox *get_aerc_mailbox(struct account_state *account,
		const char *name) {
	if (!account->mailboxes) {
		return NULL;
	}
	int i = list_seq_find(account->mailboxes, get_mbox_compare, name);
	if (i == -1) {
		return NULL;
	}
	return account->mailboxes->items[i];
}

void free_aerc_mailbox(struct aerc_mailbox *mbox) {
	if (!mbox) return;
	free(mbox->name);
	free_flat_list(mbox->flags);
	// TODO: Free messages
	free(mbox);
}
