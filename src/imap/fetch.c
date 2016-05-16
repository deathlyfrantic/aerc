/*
 * imap/select.c - issues and handles IMAP SELECT commands, as well as common
 * responses from SELECT commands
 */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <assert.h>
#include "internal/imap.h"
#include "util/list.h"
#include "util/stringop.h"
#include "log.h"

void imap_fetch(struct imap_connection *imap, imap_callback_t callback,
		void *data, int min, int max, const char *what) {
	// TODO: More optimal strategy for splitting up a range with intermetient
	// fetching messages into several sub-ranges instead of sending each
	// individually
	bool seperate = false;

	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	assert(min >= 1);
	assert(max <= mbox->messages->length);
	for (int i = min; i < max; ++i) {
		struct mailbox_message *msg = mbox->messages->items[i - 1];
		if (msg->fetching) {
			seperate = true;
			msg->fetching = true;
			break;
		}
		msg->fetching = true;
	}

	if (seperate && false) {
		// TODO
	} else {
		imap_send(imap, callback, data, "FETCH %d:%d (%s)", min, max, what);
	}
}

static void handle_flags(struct mailbox_message *msg, imap_arg_t *args) {
	args = args->list;
	free_flat_list(msg->flags);
	msg->flags = create_list();
	while (args) {
		assert(args->type == IMAP_ATOM);
		list_add(msg->flags, strdup(args->str));
		worker_log(L_DEBUG, "Flag: %s", args->str);
		args = args->next;
	}
}

void handle_imap_fetch(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args->type == IMAP_NUMBER);
	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	struct mailbox_message *msg = mbox->messages->items[args->num - 1];
	worker_log(L_DEBUG, "Received FETCH for message %ld", args->num);
	args = args->next;
	assert(args->type == IMAP_LIST);
	assert(!args->next);
	args = args->list;

	const struct {
		const char *name;
		enum imap_type expected_type;
		void (*handler)(struct mailbox_message *, imap_arg_t *);
	} handlers[] = {
		{ "FLAGS", IMAP_LIST, handle_flags }
	};

	while (args) {
		const char *name = args->str;
		args = args->next;
		for (size_t i = 0;
				i < sizeof(handlers) / (sizeof(void *) * 2 + sizeof(enum imap_type));
				++i) {
			if (strcmp(handlers[i].name, name) == 0) {
				assert(args->type == handlers[i].expected_type);
				handlers[i].handler(msg, args);
			}
		}
		args = args->next;
	}
	msg->fetching = false;
	msg->populated = true;
	if (imap->events.message_updated) {
		imap->events.message_updated(imap, msg);
	}
}
