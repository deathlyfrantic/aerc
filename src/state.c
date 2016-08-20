#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "email/headers.h"
#include "config.h"
#include "state.h"
#include "ui.h"
#include "util/stringop.h"
#include "util/list.h"
#include "worker.h"

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
	if (!account->mailboxes || !name) {
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

void free_aerc_message(struct aerc_message *msg) {
	if (!msg) return;
	free_flat_list(msg->flags);
	if (msg->headers) {
		for (size_t i = 0; i < msg->headers->length; ++i) {
			struct email_header *header = msg->headers->items[i];
			free(header->key);
			free(header->value);
		}
	}
	free_flat_list(msg->headers);
	free(msg);
}

const char *get_message_header(struct aerc_message *msg, char *key) {
	for (size_t i = 0; i < msg->headers->length; ++i) {
		struct email_header *header = msg->headers->items[i];
		if (strcmp(header->key, key) == 0) {
			return header->value;
		}
	}
	return NULL;
}

bool get_mailbox_flag(struct aerc_mailbox *mbox, char *flag) {
	if (!mbox->flags) return false;
	for (size_t i = 0; i < mbox->flags->length; ++i) {
		const char *_flag = mbox->flags->items[i];
		if (strcmp(flag, _flag) == 0) {
			return true;
		}
	}
	return false;
}

bool get_message_flag(struct aerc_message *msg, char *flag) {
	if (!msg->flags) return false;
	for (size_t i = 0; i < msg->flags->length; ++i) {
		const char *_flag = msg->flags->items[i];
		if (strcmp(flag, _flag) == 0) {
			return true;
		}
	}
	return false;
}

struct account_config *config_for_account(const char *name) {
	for (size_t i = 0; i < config->accounts->length; ++i) {
		struct account_config *c = config->accounts->items[i];
		if (strcmp(c->name, name) == 0) {
			return c;
		}
	}
	return NULL;
}
