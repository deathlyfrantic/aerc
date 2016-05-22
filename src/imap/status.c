/*
 * imap/status.c - handles IMAP status commands and IMAP status responses
 */
#define _POSIX_C_SOURCE 201112LL

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "imap/imap.h"
#include "internal/imap.h"
#include "log.h"
#include "util/hashtable.h"

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	// This space intentionally left blank
}

void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	if (args->type == IMAP_RESPONSE) {
		/*
		 * We have a status response included in this command. We'll produce a
		 * fake "command" and send it through the line handler again. We
		 * allocate space for the status response plus the prefix, then populate
		 * the buffer and pass it to handle_line and continue with status
		 * handling.
		 */
		const char *prefix = "* ";
		int len = strlen(args->str) + strlen(prefix);
		char *status = malloc(len + 1);
		strcpy(status, prefix);
		strcat(status, args->str);
		imap_arg_t *a = calloc(1, sizeof(imap_arg_t));
		int _;
		imap_parse_args(status, a, &_);
		handle_line(imap, a);
		imap_arg_free(a);
		free(status);
		args = args->next;
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
	} else {
		return;
	}
	/*
	 * STATUS commands are usually sent by the server in response to a command
	 * we asked it to do earlier. We passed in a tag with this command, and the
	 * server passes that tag back with the STATUS command to tell us it's done.
	 * The pending callbacks hashtable is keyed on the tag, so we pull the
	 * callback out and invoke it based on that tag.
	 */
	bool has_callback = hashtable_contains(imap->pending, token);
	struct imap_pending_callback *callback = hashtable_del(imap->pending, token);
	if (has_callback) {
		if (callback && callback->callback) {
			callback->callback(imap, callback->data, estatus, args->original);
		}
		free(callback);
	} else if (strcmp(token, "*") == 0) {
		/*
		 * Sometimes, though, the tag will be *, which is used for meta commands
		 * or passively informing the client of activity or updates to their
		 * state. Note that on connection we receive a STATUS message prefixed
		 * with * to indicate that the server is ready - we register a special
		 * callback for this case and it's handled by the earlier branch of this
		 * if statement.
		 */
		if (estatus == STATUS_OK) {
			handle_imap_OK(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unhandled status command %s", cmd);
		}
	} else {
		worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
	}
}
