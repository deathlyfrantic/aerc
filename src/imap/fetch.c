/*
 * imap/select.c - issues and handles IMAP SELECT commands, as well as common
 * responses from SELECT commands
 */
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "email/headers.h"
#include "imap/date.h"
#include "imap/imap.h"
#include "internal/imap.h"
#include "log.h"
#include "util/list.h"
#include "util/stringop.h"

void imap_fetch(struct imap_connection *imap, imap_callback_t callback,
		void *data, size_t min, size_t max, const char *what) {
	// TODO: More optimal strategy for splitting up a range with intermetient
	// fetching messages into several sub-ranges instead of sending each
	// individually
	bool seperate = false;

	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	assert(min >= 1);
	assert(max <= mbox->messages->length);
	for (size_t i = min; i < max; ++i) {
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

static int handle_flags(struct mailbox_message *msg, imap_arg_t *args) {
	args = args->list;
	free_flat_list(msg->flags);
	msg->flags = create_list();
	while (args) {
		assert(args->type == IMAP_ATOM);
		list_add(msg->flags, strdup(args->str));
		worker_log(L_DEBUG, "Set flag for message: %s", args->str);
		args = args->next;
	}
	return 0;
}

static int handle_uid(struct mailbox_message *msg, imap_arg_t *args) {
	assert(args->type == IMAP_NUMBER);
	worker_log(L_DEBUG, "Message UID: %ld", args->num);
	msg->uid = args->num;
	return 0;
}

static int handle_internaldate(struct mailbox_message *msg, imap_arg_t *args) {
	assert(args->type == IMAP_STRING);
	msg->internal_date = malloc(sizeof(struct tm));
	char *r = parse_imap_date(args->str, msg->internal_date);
	if (!r || *r) {
		worker_log(L_DEBUG, "Warning: received invalid date for message (%s)",
				args->str);
	} else {
		char date[64];
		strftime(date, sizeof(date), "%F %H:%M %z", msg->internal_date);
		worker_log(L_DEBUG, "Message internal date: %s", date);
	}
	return 0;
}

static int handle_body(struct mailbox_message *msg, imap_arg_t *args) {
	assert(args->type == IMAP_RESPONSE);
	worker_log(L_DEBUG, "Handling message body fields");
	/*
	 * We get an IMAP_RESPONSE here, but we don't really care about it. We're
	 * just going to parse the message body, which comes next.
	 */
	args = args->next;
	assert(args && args->type == IMAP_STRING);
	list_t *headers = create_list();
	parse_headers(args->str, headers);
	free_headers(msg->headers);
	msg->headers = headers;
	return 1; // We used one extra argument
}

void handle_imap_fetch(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	assert(args->type == IMAP_NUMBER);
	struct mailbox *mbox = get_mailbox(imap, imap->selected);
	int index = args->num - 1;
	struct mailbox_message *msg = mbox->messages->items[index];
	worker_log(L_DEBUG, "Received FETCH for message %d", index + 1);
	args = args->next;
	assert(args->type == IMAP_LIST);
	assert(!args->next);
	args = args->list;

	const struct {
		const char *name;
		enum imap_type expected_type;
		int (*handler)(struct mailbox_message *, imap_arg_t *);
	} handlers[] = {
		{ "UID", IMAP_NUMBER, handle_uid },
		{ "FLAGS", IMAP_LIST, handle_flags },
		{ "INTERNALDATE", IMAP_STRING, handle_internaldate },
		{ "BODY", IMAP_RESPONSE, handle_body }
		// TODO: More fields, I guess
	};

	while (args) {
		const char *name = args->str;
		args = args->next;
		if (!args) {
			break;
		}
		for (size_t i = 0;
				i < sizeof(handlers) / (sizeof(void *) * 2 + sizeof(enum imap_type));
				++i) {
			if (strcmp(handlers[i].name, name) == 0) {
				assert(args->type == handlers[i].expected_type);
				int j = handlers[i].handler(msg, args);
				while (j-- && args) args = args->next;
			}
		}
		if (args) {
			args = args->next;
		}
	}
	msg->index = index;
	msg->fetching = false;
	msg->populated = true;
	if (imap->events.message_updated) {
		imap->events.message_updated(imap, msg);
	}
}
