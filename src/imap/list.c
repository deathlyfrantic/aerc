/*
 * imap/list.c - issues and handles IMAP LIST commands
 */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
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
	imap_arg_t *flags = args->list;
	//const char *delim = args->next->str;
	const char *name = args->next->next->str;

	struct mailbox *mbox = get_or_make_mailbox(imap, name);
	while (flags) {
		if (flags->type == IMAP_ATOM) {
			struct mailbox_flag *flag = calloc(1, sizeof(struct mailbox_flag));
			flag->name = strdup(flags->str);
			list_add(mbox->flags, flag);
		}
		flags = flags->next;
	}
}
