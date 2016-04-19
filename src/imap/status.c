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

	struct { const char *name; bool *ptr; } ptrs[] = {
		{ "IMAP4rev1", &cap->imap4rev1 },
		{ "STARTTLS", &cap->starttls },
		{ "LOGINDISABLED", &cap->logindisabled },
		{ "AUTH=PLAIN", &cap->auth_plain },
		{ "AUTH=LOGIN", &cap->auth_login },
		{ "IDLE", &cap->idle }
	};

	for (int i = 0; i < args->length; ++i) {
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
		imap->events.cap(imap, STATUS_OK, _args);
	}
	free_flat_list(args);
}

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	worker_log(L_DEBUG, "IMAP OK: %s", args);
	// Handle initial OK
	if (!imap->ready && imap->events.ready) {
		imap->ready = true;
		imap->events.ready(imap, STATUS_OK, args);
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
		status[i + 2] = '\0';
		args = args + 3 + i;
		handle_line(imap, status);
		free(status);
	}
	enum imap_status estatus;
	if (strcmp(cmd, "OK") == 0) {
		estatus = STATUS_OK;
	} else if (strcmp(cmd, "NO") == 0) {
		estatus = STATUS_NO;
	} else if (strcmp(cmd, "BAD") == 0) {
		estatus = STATUS_BAD;
	} else if (strcmp(cmd, "PREAUTH") == 0) {
		estatus = STATUS_PREAUTH;
	} else if (strcmp(cmd, "BYE") == 0) {
		estatus = STATUS_BYE;
	}
	if (strcmp(token, "*") == 0) {
		if (estatus == STATUS_OK) {
			handle_imap_OK(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unhandled status command %s %s",
					cmd,args);
		}
	} else {
		bool has_callback = hashtable_contains(imap->pending, token);
		imap_callback_t callback = hashtable_del(imap->pending, token);
		if (has_callback) {
			if (callback) {
				callback(imap, estatus, args);
			}
		} else {
			worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
		}
	}
}
