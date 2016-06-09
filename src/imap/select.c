/*
 * imap/select.c - issues and handles IMAP SELECT commands, as well as common
 * responses from SELECT commands
 */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "imap/imap.h"
#include "internal/imap.h"
#include "log.h"
#include "util/list.h"

void imap_select(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox) {
	if (mailbox_get_flag(imap, mailbox, "\\noselect")) {
		callback(imap, data, STATUS_PRE_ERROR, "Cannot select this mailbox");
		return;
	}
	imap->selected = strdup(mailbox);
	imap_send(imap, callback, data, "SELECT \"%s\"", mailbox);
}

void handle_imap_existsunseenrecent(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args);
	assert(args->type == IMAP_NUMBER);
	struct mailbox *mbox = get_mailbox(imap, imap->selected);

	struct { const char *cmd; long *ptr; } ptrs[] = {
		{ "EXISTS", &mbox->exists },
		{ "UNSEEN", &mbox->unseen },
		{ "RECENT", &mbox->recent }
	};

	bool set = false;
	for (size_t i = 0; i < sizeof(ptrs) / (sizeof(void*) * 2); ++i) {
		if (strcmp(ptrs[i].cmd, cmd) == 0) {
			set = true;
			if (i == 0 /* EXISTS */) {
				int diff = args->num - mbox->exists;
				if (mbox->exists == -1) {
					diff = args->num;
				}
				if (diff > 0) {
					while (diff--) {
						struct mailbox_message *msg = calloc(1,
								sizeof(struct mailbox_message));
						msg->index = mbox->messages->length;
						list_add(mbox->messages, msg);
					}
				} else if (diff == 0) {
					/* no-op */
				} else {
					worker_log(L_ERROR, "Got EXISTS with negative diff, not supposed to happen");
				}
			}
			*ptrs[i].ptr = args->num;
			break;
		}
	}

	if (set) {
		if (imap->events.mailbox_updated) {
			imap->events.mailbox_updated(imap);
		}
	} else {
		worker_log(L_DEBUG, "Got weird command %s", cmd);
	}
}

void handle_imap_uidnext(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args);
	assert(args->type == IMAP_NUMBER);
	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	mbox->nextuid = args->num;
}

void handle_imap_readwrite(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	mbox->read_write = true;
	if (imap->events.mailbox_updated) {
		imap->events.mailbox_updated(imap);
	}
}
