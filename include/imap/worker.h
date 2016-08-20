#ifndef _IMAP_WORKER_H
#define _IMAP_WORKER_H

#include "imap/imap.h"
#include "worker.h"

void *imap_worker(void *_pipe);
struct aerc_mailbox *serialize_mailbox(struct mailbox *source);
struct aerc_message *serialize_message(struct mailbox_message *source);
// Worker handlers
void handle_worker_connect(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_cert_okay(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_list(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_select_mailbox(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_fetch_messages(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_delete_mailbox(struct worker_pipe *pipe, struct worker_message *message);

#endif
