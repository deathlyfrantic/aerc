#ifndef _INTERNAL_IMAP_H
#define _INTERNAL_IMAP_H

#include "imap/imap.h"

struct imap_pending_callback {
    imap_callback_t callback;
    void *data;
};

void handle_line(struct imap_connection *imap, const char *line);
struct imap_pending_callback *make_callback(imap_callback_t callback, void *data);

void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args);
void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args);
void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args);

#endif
