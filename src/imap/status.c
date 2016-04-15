#define _POSIX_C_SOURCE 201112LL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "imap/imap.h"
#include "util/stringop.h"
#include "util/hashtable.h"
#include "log.h"

void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args) {
	list_t *args = split_string(_args, " ");
	worker_log(L_DEBUG, "Server capabilities: %s", _args);

	struct imap_capabilities *cap = calloc(1,
			sizeof(struct imap_capabilities));

	struct { char *name; bool *ptr; } ptrs[] = {
		{ "IMAP4rev1", &cap->imap4rev1 },
		{ "STARTTLS", &cap->starttls },
		{ "LOGINDISABLED", &cap->logindisabled },
		{ "AUTH=PLAIN", &cap->auth_plain },
		{ "AUTH=LOGIN", &cap->auth_login }
	};

	for (int i = 1; i < args->length; ++i) {
		for (size_t j = 0; j < sizeof(ptrs) / (sizeof(void*) * 2); ++j) {
			if (strcmp(ptrs[j].name, args->items[i]) == 0) {
				*ptrs[j].ptr = true;
			}
		}
	}
	if (imap->cap) {
		free(imap->cap);
	}
	imap->cap = cap;
	if (imap->events.cap) {
		imap->events.cap(imap->events.data);
	}
	free_flat_list(args);
}

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	worker_log(L_DEBUG, "IMAP OK: %s", args);
	if (imap->events.ready) {
		imap->events.ready(imap->events.data);
	}
}

void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	if (args[0] == '[' && strchr(args, ']')) {
		// Includes optional status response
		const char *prefix = "* ";
		int i = strchr(args + 1, ']') - args - 1;
		char *status = malloc(i + 1 + 2);
		strcpy(status, prefix);
		strncpy(status + 2, args + 1, i);
		status[i] = '\0';
		args = args + 3 + i;
		handle_line(imap, status);
		free(status);
	}
	if (strcmp(token, "*") == 0) {
		if (strcmp(cmd, "OK") == 0) {
			handle_imap_OK(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unhandled status command %s %s",
					cmd,args);
		}
	} else {
		imap_handler_t handler = hashtable_get(imap->pending, token);
		if (handler) {
			handler(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
		}
	}
}
