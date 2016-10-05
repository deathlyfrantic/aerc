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

struct callback_data {
	void *data;
	char *mailbox;
	imap_callback_t callback;
};

static void imap_select_callback(struct imap_connection *imap,
		void *data, enum imap_status status, const char *args) {
	struct callback_data *cbdata = data;
	if (status != STATUS_OK) {
		if (cbdata->callback) {
			cbdata->callback(imap, cbdata->data, status, args);
		}
		return;
	}
	struct mailbox *mbox = get_mailbox(imap, cbdata->mailbox);
	if (imap->selected) {
		free(imap->selected);
	}
	imap->selected = imap->selecting;
	imap->selecting = NULL;
	if (status == STATUS_OK) {
		mbox->selected = true;
	}
	if (cbdata->callback) {
		cbdata->callback(imap, cbdata->data, status, args);
	}
	if (imap->events.mailbox_updated) {
		imap->events.mailbox_updated(imap, mbox);
	}
	free(cbdata->mailbox);
	free(cbdata);
}

void imap_select(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox) {
	if (mailbox_get_flag(imap, mailbox, "\\noselect")) {
		callback(imap, data, STATUS_PRE_ERROR, "Cannot select this mailbox");
		return;
	}
	imap->selecting = strdup(mailbox);
	struct callback_data *cbdata = malloc(sizeof(struct callback_data));
	cbdata->data = data;
	cbdata->mailbox = strdup(mailbox);
	cbdata->callback = callback;
	imap_send(imap, imap_select_callback, cbdata, "SELECT \"%s\"", mailbox);
}

void handle_imap_existsunseenrecent(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args);
	assert(args->type == IMAP_NUMBER);
	char *selected = imap->selecting ? imap->selecting : imap->selected;
	struct mailbox *mbox = get_mailbox(imap, selected);

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
			imap->events.mailbox_updated(imap, mbox);
		}
	} else {
		worker_log(L_DEBUG, "Got weird command %s", cmd);
	}
}

void handle_imap_uidnext(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args);
	assert(args->type == IMAP_NUMBER);
	char *selected = imap->selecting ? imap->selecting : imap->selected;
	struct mailbox *mbox = get_mailbox(imap, selected);
	mbox->nextuid = args->num;
}

void handle_imap_readwrite(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	char *selected = imap->selecting ? imap->selecting : imap->selected;
	struct mailbox *mbox = get_mailbox(imap, selected);
	mbox->read_write = true;
	if (imap->events.mailbox_updated) {
		imap->events.mailbox_updated(imap, mbox);
	}
}
