/*
 * imap/delete.c - issues IMAP DELETE commands
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

static void imap_delete_callback(struct imap_connection *imap,
		void *data, enum imap_status status, const char *args) {
	struct callback_data *cbdata = data;
	struct mailbox *mbox = get_mailbox(imap, cbdata->mailbox);
	if (status == STATUS_OK) {
		for (int i = 0; i < imap->mailboxes->length; ++i) {
			if (imap->mailboxes->items[i] == mbox) {
				list_del(imap->mailboxes, i);
				break;
			}
		}
		if (imap->events.mailbox_deleted) {
			imap->events.mailbox_deleted(imap, mbox->name);
		}
		mailbox_free(mbox);
	}
	if (cbdata->callback) {
		cbdata->callback(imap, data, status, args);
	}
	free(cbdata->mailbox);
	free(cbdata);
}

void imap_delete(struct imap_connection *imap, imap_callback_t callback,
		void *data, const char *mailbox) {
	struct callback_data *cbdata = malloc(sizeof(struct callback_data));
	cbdata->data = data;
	cbdata->mailbox = strdup(mailbox);
	cbdata->callback = callback;
	imap_send(imap, imap_delete_callback, cbdata, "DELETE \"%s\"", mailbox);
}
