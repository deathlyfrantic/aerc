/*
 * imap/list.c - issues and handles IMAP LIST commands
 */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "internal/imap.h"
#include "util/list.h"
#include "util/stringop.h"

void imap_list(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *refname, const char *boxname) {
	imap_send(imap, callback, data, "LIST \"%s\" \"%s\"", refname, boxname);
}

void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	struct mailbox *mbox = malloc(sizeof(struct mailbox));
	mbox->flags = create_list();
	imap_arg_t *flags = args->list;
	while (flags) {
		if (flags->type == IMAP_ATOM) {
			list_add(mbox->flags, strdup(flags->str));
		}
		flags = flags->next;
	}
	args = args->next->next;
	mbox->name = strdup(args->str);
	list_add(imap->mailboxes, mbox);
}
