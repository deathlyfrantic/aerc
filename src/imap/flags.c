/*
 * imap/flags.c - handles IMAP FLAGS and PERMANENTFLAGS commands
 */
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "imap/imap.h"
#include "internal/imap.h"
#include "util/list.h"
#include "util/stringop.h"

void handle_imap_flags(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	free_flat_list(mbox->flags);
	mbox->flags = create_list();

	bool perm = strcmp(cmd, "PERMANENTFLAGS") == 0;

	imap_arg_t *flags = args->list;
	while (flags) {
		if (flags->type == IMAP_ATOM) {
			struct mailbox_flag *flag = mailbox_get_flag(imap, mbox->name, flags->str);
			if (!flag) {
				flag = calloc(1, sizeof(struct mailbox_flag));
				flag->name = strdup(flags->str);
				list_add(mbox->flags, flag);
			}
			flag->permanent = perm;
		}
		flags = flags->next;
	}
}
