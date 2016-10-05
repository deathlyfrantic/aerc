#ifndef _HANDLERS_H
#define _HANDLERS_H

#include "state.h"
#include "worker.h"

void handle_worker_connect_done(struct account_state *account,
		struct worker_message *message);
void handle_worker_connect_error(struct account_state *account,
		struct worker_message *message);
void handle_worker_select_done(struct account_state *account,
		struct worker_message *message);
void handle_worker_select_error(struct account_state *account,
		struct worker_message *message);
void handle_worker_list_done(struct account_state *account,
		struct worker_message *message);
void handle_worker_list_error(struct account_state *account,
		struct worker_message *message);
void handle_worker_connect_cert_check(struct account_state *account,
		struct worker_message *message);
void handle_worker_mailbox_updated(struct account_state *account,
		struct worker_message *message);
void handle_worker_message_updated(struct account_state *account,
		struct worker_message *message);
void handle_worker_mailbox_deleted(struct account_state *account,
		struct worker_message *message);

#endif
