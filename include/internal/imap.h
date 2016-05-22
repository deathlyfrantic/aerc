#ifndef _INTERNAL_IMAP_H
#define _INTERNAL_IMAP_H

#include <stdio.h>

#include "imap/imap.h"
#include "util/list.h"

struct imap_pending_callback {
    imap_callback_t callback;
    void *data;
};

int handle_line(struct imap_connection *imap, imap_arg_t *arg);

void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_flags(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_existsunseenrecent(struct imap_connection *imap,
        const char *token, const char *cmd, imap_arg_t *args);
void handle_imap_uidnext(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_readwrite(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_fetch(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);

/* Parses an IMAP argument string and sets "remaining" the number of characters
 * necessary to complete parsing (if the string doesn't represent a complete
 * arg string). Returns the number of bytes used from the string.
 */
int imap_parse_args(const char *str, imap_arg_t *args, int *remaining);
void print_imap_args(FILE *f, imap_arg_t *args, int indent);
char *serialize_args(const imap_arg_t *args);

/*
 * Utility functions
 */
struct imap_pending_callback *make_callback(imap_callback_t callback, void *data);
struct mailbox *get_mailbox(struct imap_connection *imap, const char *name);
struct mailbox *get_or_make_mailbox(struct imap_connection *imap,
		const char *name);
struct mailbox_flag *mailbox_get_flag(struct imap_connection *imap,
        const char *mbox, const char *flag);

#endif
