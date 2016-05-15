/*
 * main.c - entry point and temporary test code
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef USE_OPENSSL
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif
#include "absocket.h"
#include "worker.h"
#include "imap/worker.h"
#include "log.h"
#include "config.h"
#include "state.h"
#include "ui.h"

struct aerc_state *state;

void handle_worker_message(struct account_state *account, struct worker_message *msg) {
	/*
	 * Handle incoming messages from a worker. This is pretty bare right now.
	 */
	switch (msg->type) {
	case WORKER_CONNECT_DONE:
		worker_post_action(account->pipe, WORKER_LIST, NULL, NULL);
		break;
	case WORKER_CONNECT_ERROR:
		// TODO: Tell the user
		break;
	case WORKER_LIST_DONE:
	{
		account->mailboxes = msg->data;
		bool have_inbox = false;
		for (int i = 0; i < account->mailboxes->length; ++i) {
			struct aerc_mailbox *mbox = account->mailboxes->items[i];
			if (strcmp(mbox->name, "INBOX") == 0) {
				have_inbox = true;
			}
		}
		if (have_inbox) {
			free(account->selected);
			account->selected = strdup("INBOX");
			worker_post_action(account->pipe, WORKER_SELECT_MAILBOX, NULL,
					strdup("INBOX"));
		}
		rerender();
		break;
	}
	case WORKER_LIST_ERROR:
		fprintf(stderr, "Error listing mailboxes!\n");
		break;
#ifdef USE_OPENSSL
	case WORKER_CONNECT_CERT_CHECK:
		// TODO: interactive certificate check
		worker_post_action(account->pipe, WORKER_CONNECT_CERT_OKAY, msg, NULL);
		break;
#endif
	default:
		// No-op
		break;
	}
}

void init_state() {
	state = calloc(1, sizeof(struct aerc_state));
	state->accounts = create_list();
}

int main(int argc, char **argv) {
	init_state();
	init_log(L_INFO); // TODO: Customizable
	abs_init();

	if (!load_main_config(NULL)) {
		worker_log(L_ERROR, "Error loading config");
		return 1;
	}

	if (config->accounts->length == 0) {
		worker_log(L_ERROR, "No accounts configured. What do you expect me to do?");
		return 1;
	}

	for (int i = 0; i < config->accounts->length; ++i) {
		struct account_config *ac = config->accounts->items[i];
		if (!ac->source) {
			worker_log(L_ERROR, "No source configured for account %s", ac->name);
			return 1;
		}
		struct account_state *account = calloc(1, sizeof(struct account_state));
		account->name = strdup(ac->name);
		account->pipe = worker_pipe_new();
		worker_post_action(account->pipe, WORKER_CONNECT, NULL, ac->source);
		worker_post_action(account->pipe, WORKER_CONFIGURE, NULL, ac->extras);
		// TODO: Detect appropriate worker based on source
		pthread_create(&account->worker, NULL, imap_worker, account->pipe);
		list_add(state->accounts, account);
	}

	init_ui();
	rerender();

	while (1) {
		struct worker_message *msg;
		for (int i = 0; i < state->accounts->length; ++i) {
			struct account_state *account = state->accounts->items[i];
			if (worker_get_message(account->pipe, &msg)) {
				handle_worker_message(account, msg);
				worker_message_free(msg);
			}
		}

		if (!ui_tick()) {
			break;
		}

		struct timespec spec = { 0, .5e+8 };
		nanosleep(&spec, NULL);
	}

	teardown_ui();
	return 0;
}
