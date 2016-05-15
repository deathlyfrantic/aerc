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
#include "handlers.h"
#include "absocket.h"
#include "worker.h"
#include "imap/worker.h"
#include "log.h"
#include "config.h"
#include "state.h"
#include "ui.h"

struct aerc_state *state;

struct message_handler {
	enum worker_message_type action;
	void (*handler)(struct account_state *account, struct worker_message *message);
};

struct message_handler message_handlers[] = {
	{ WORKER_CONNECT_DONE, handle_worker_connect_done },
	{ WORKER_CONNECT_ERROR, handle_worker_connect_error },
	{ WORKER_LIST_DONE, handle_worker_list_done },
	{ WORKER_LIST_ERROR, handle_worker_list_error },
	{ WORKER_CONNECT_CERT_CHECK, handle_worker_connect_cert_check },
};

void handle_worker_message(struct account_state *account, struct worker_message *msg) {
	/*
	 * Handle incoming messages from a worker.
	 */
	for ( size_t i = 0;
			i < sizeof(message_handlers) / sizeof(struct message_handler);
			++i) {
		struct message_handler handler = message_handlers[i];
		if (handler.action == msg->type) {
			handler.handler(account, msg);
			break;
		}
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
		account->worker.pipe = worker_pipe_new();
		worker_post_action(account->worker.pipe, WORKER_CONNECT, NULL,
				ac->source);
		worker_post_action(account->worker.pipe, WORKER_CONFIGURE, NULL,
				ac->extras);
		// TODO: Detect appropriate worker based on source
		pthread_create(&account->worker.thread, NULL, imap_worker,
				account->worker.pipe);
		list_add(state->accounts, account);
	}

	init_ui();
	rerender();

	while (1) {
		struct worker_message *msg;
		for (int i = 0; i < state->accounts->length; ++i) {
			struct account_state *account = state->accounts->items[i];
			if (worker_get_message(account->worker.pipe, &msg)) {
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
