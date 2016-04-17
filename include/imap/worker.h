#ifndef _IMAP_WORKER_H
#define _IMAP_WORKER_H

#include "imap/imap.h"
#include "worker.h"

void *imap_worker(void *_pipe);
// Worker handlers
void handle_worker_connect(struct worker_pipe *pipe, struct worker_message *message);
void handle_worker_cert_okay(struct worker_pipe *pipe, struct worker_message *message);
// IMAP handlers
void handle_imap_ready(struct imap_connection *imap, enum imap_status status, const char *args);
void handle_imap_cap(struct imap_connection *imap, enum imap_status status, const char *args);
void handle_imap_logged_in(struct imap_connection *imap, enum imap_status status, const char *args);

#endif
