/*
 * handlers.c - handlers for worker messages
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_OPENSSL
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif
#include "worker.h"
#include "log.h"
#include "config.h"
#include "state.h"
#include "ui.h"

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

void handle_worker_mailbox_updated(struct account_state *account,
		struct worker_message *message) {
	struct aerc_mailbox *new = message->data;
	struct aerc_mailbox *old;
	for (int i = 0; i < account->mailboxes->length; ++i) {
		old = account->mailboxes->items[i];
		if (strcmp(old->name, new->name) == 0) {
			account->mailboxes->items[i] = new;
			rerender();
			break;
		}
	}

	if (old->exists < new->exists) {
		if (old->exists == -1) {
			old->exists = 1;
		}
		struct message_range *range = malloc(sizeof(struct message_range));
		range->min = old->exists;
		range->max = new->exists;
		worker_post_action(account->worker.pipe, WORKER_FETCH_MESSAGES,
				NULL, range);
	}

	free_aerc_mailbox(old);
}
