/*
 * handlers.c - handlers for worker messages
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "log.h"
#include "state.h"
#include "ui.h"
#include "util/list.h"
#include "worker.h"

static void fetch_necessary(struct account_state *account,
		struct aerc_mailbox *mbox) {
	int min = -1, max = -1;
	size_t i;
	for (i = 0; i < mbox->messages->length; ++i) {
		struct aerc_message *message = mbox->messages->items[i];
		if (min == -1) {
			if (message->should_fetch && !message->fetching) {
				message->fetching = true;
				min = i;
			}
		} else {
			if (!message->should_fetch) {
				max = i - 1;
				struct message_range *range = malloc(
						sizeof(struct message_range));
				range->min = min + 1;
				range->max = max + 1;
				worker_log(L_DEBUG, "Fetching message range %d - %d",
						range->min, range->max);
				worker_post_action(account->worker.pipe, WORKER_FETCH_MESSAGES,
						NULL, range);
				min = max = -1;
			}
		}
	}
	if (min != -1) {
		max = i - 1;
		struct message_range *range = malloc(sizeof(struct message_range));
		range->min = min + 1;
		range->max = max + 1;
		worker_log(L_DEBUG, "Fetching message range %d - %d", range->min, range->max);
		worker_post_action(account->worker.pipe, WORKER_FETCH_MESSAGES,
				NULL, range);
	}
}

void handle_worker_connect_done(struct account_state *account,
		struct worker_message *message) {
	worker_post_action(account->worker.pipe, WORKER_LIST, NULL, NULL);
	set_status(account, ACCOUNT_OKAY, "Connected.");
}

void handle_worker_connect_error(struct account_state *account,
		struct worker_message *message) {
	set_status(account, ACCOUNT_ERROR, (char *)message->data);
}

void handle_worker_select_done(struct account_state *account,
		struct worker_message *message) {
	account->selected = strdup((char *)message->data);
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, account->selected);
	rerender();
	fetch_necessary(account, mbox);
}

void handle_worker_select_error(struct account_state *account,
		struct worker_message *message) {
	set_status(account, ACCOUNT_ERROR, "Unable to select that mailbox.");
}

void handle_worker_list_done(struct account_state *account,
		struct worker_message *message) {
	account->mailboxes = message->data;
	char *wanted = "INBOX";
	struct account_config *c = config_for_account(account->name);
	for (size_t i = 0; i < c->extras->length; ++i) {
		struct account_config_extra *extra = c->extras->items[i];
		if (strcmp(extra->key, "default") == 0) {
			wanted = extra->value;
			break;
		}
	}
	bool have_wanted = false;
	for (size_t i = 0; i < account->mailboxes->length; ++i) {
		struct aerc_mailbox *mbox = account->mailboxes->items[i];
		if (strcmp(mbox->name, wanted) == 0) {
			have_wanted = true;
		}
	}
	if (have_wanted) {
		free(account->selected);
		account->selected = strdup(wanted);
		worker_post_action(account->worker.pipe, WORKER_SELECT_MAILBOX,
				NULL, strdup(wanted));
	}
	rerender();
}

void handle_worker_list_error(struct account_state *account,
		struct worker_message *message) {
	set_status(account, ACCOUNT_ERROR, "Unable to list folders!");
}

void handle_worker_connect_cert_check(struct account_state *account,
		struct worker_message *message) {
#ifdef USE_OPENSSL
	// TODO: interactive certificate check
	worker_post_action(account->worker.pipe, WORKER_CONNECT_CERT_OKAY,
			message, NULL);
#endif
}

void handle_worker_mailbox_updated(struct account_state *account,
		struct worker_message *message) {
	struct aerc_mailbox *new = message->data;
	struct aerc_mailbox *old = NULL;

	bool rerendered = false;
	for (size_t i = 0; i < account->mailboxes->length; ++i) {
		old = account->mailboxes->items[i];
		if (strcmp(old->name, new->name) == 0) {
			account->mailboxes->items[i] = new;
			rerender();
			rerendered = true;
			break;
		}
	}

	if (rerendered && new->selected) {
		fetch_necessary(account, new);
	}
	free_aerc_mailbox(old);
}

void handle_worker_message_updated(struct account_state *account,
		struct worker_message *message) {
	worker_log(L_DEBUG, "Updated message on main thread");
	struct aerc_message *new = message->data;
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, account->selected);
	for (size_t i = 0; i < mbox->messages->length; ++i) {
		struct aerc_message *old = mbox->messages->items[i];
		if (old->index == new->index) {
			free_aerc_message(mbox->messages->items[i]);
			new->fetched = true;
			new->should_fetch = false;
			mbox->messages->items[i] = new;
			rerender_item(i);
		}
	}
}

void handle_worker_mailbox_deleted(struct account_state *account,
		struct worker_message *message) {
	worker_log(L_DEBUG, "Deleting mailbox on main thread");
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, (const char *)message->data);
	for (size_t i = 0; i < account->mailboxes->length; ++i) {
		if (account->mailboxes->items[i] == mbox) {
			list_del(account->mailboxes, i);
		}
	}
	free_aerc_mailbox(mbox);
	rerender();
}
