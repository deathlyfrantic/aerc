/*
 * handlers.c - handlers for worker messages
 */
#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "state.h"
#include "ui.h"
#include "util/list.h"
#include "worker.h"

void handle_worker_connect_done(struct account_state *account,
		struct worker_message *message) {
	worker_post_action(account->worker.pipe, WORKER_LIST, NULL, NULL);
	set_status(account, ACCOUNT_OKAY, "Connected.");
}

void handle_worker_connect_error(struct account_state *account,
		struct worker_message *message) {
	set_status(account, ACCOUNT_ERROR, (char *)message->data);
}

void handle_worker_list_done(struct account_state *account,
		struct worker_message *message) {
	account->mailboxes = message->data;
	bool have_inbox = false;
	for (int i = 0; i < account->mailboxes->length; ++i) {
		struct aerc_mailbox *mbox = account->mailboxes->items[i];
		if (strcmp(mbox->name, "INBOX") == 0) {
			have_inbox = true;
		}
	}
	// TODO: let user configure default mailbox
	if (have_inbox) {
		free(account->selected);
		account->selected = strdup("INBOX");
		worker_post_action(account->worker.pipe, WORKER_SELECT_MAILBOX,
				NULL, strdup("INBOX"));
	}
	// TODO: only rerender that list
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

static void fetch_necessary(struct account_state *account,
		struct aerc_mailbox *mbox) {
	int min = -1, max = -1, i;
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
		worker_post_action(account->worker.pipe, WORKER_FETCH_MESSAGES,
				NULL, range);
	}
}

void handle_worker_mailbox_updated(struct account_state *account,
		struct worker_message *message) {
	struct aerc_mailbox *new = message->data;
	struct aerc_mailbox *old = NULL;

	bool rerendered = false;
	for (int i = 0; i < account->mailboxes->length; ++i) {
		old = account->mailboxes->items[i];
		if (strcmp(old->name, new->name) == 0) {
			account->mailboxes->items[i] = new;
			rerender();
			rerendered = true;
			break;
		}
	}

	if (rerendered) {
		fetch_necessary(account, new);
	}
	free_aerc_mailbox(old);
}

void handle_worker_message_updated(struct account_state *account,
		struct worker_message *message) {
	worker_log(L_DEBUG, "Updated message on main thread");
	struct aerc_message *new = message->data;
	struct aerc_mailbox *mbox = get_aerc_mailbox(account, account->selected);
	for (int i = 0; i < mbox->messages->length; ++i) {
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
