#define _POSIX_C_SOURCE 201112LL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "internal/imap.h"
#include "util/stringop.h"
#include "util/hashtable.h"
#include "log.h"

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	// This space intentionally left blank
}

void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	if (args->type == IMAP_RESPONSE) {
		// Includes optional status response
		const char *prefix = "* ";
		int len = strlen(args->str) + strlen(prefix);
		char *status = malloc(len + 1);
		strcpy(status, prefix);
		strcat(status, args->str);
		handle_line(imap, status);
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
	}
	bool has_callback = hashtable_contains(imap->pending, token);
	struct imap_pending_callback *callback = hashtable_del(imap->pending, token);
	if (has_callback) {
		if (callback && callback->callback) {
			callback->callback(imap, callback->data, estatus, args->original);
		}
		free(callback);
	} else if (strcmp(token, "*") == 0) {
		if (estatus == STATUS_OK) {
			handle_imap_OK(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unhandled status command %s", cmd);
		}
	} else {
		worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
	}
}
