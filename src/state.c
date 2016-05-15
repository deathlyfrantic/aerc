#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "list.h"
#include "state.h"

static int get_mbox_compare(const void *_mbox, const void *_name) {
	const struct aerc_mailbox *mbox = _mbox;
	const char *name = _name;
	return strcmp(mbox->name, name);
}

struct aerc_mailbox *get_aerc_mailbox(const char *name) {
	int i = list_seq_find(state->mailboxes, get_mbox_compare, name);
	if (i == -1) {
		return NULL;
	}
	return state->mailboxes->items[i];
}
