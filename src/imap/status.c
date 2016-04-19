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

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	// This space intentionally left blank
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
	bool has_callback = hashtable_contains(imap->pending, token);
	struct imap_pending_callback *callback = hashtable_del(imap->pending, token);
	if (has_callback) {
		if (callback && callback->callback) {
			callback->callback(imap, callback->data, estatus, args);
		}
		free(callback);
	} else if (strcmp(token, "*") == 0) {
		if (estatus == STATUS_OK) {
			handle_imap_OK(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unhandled status command %s %s",
					cmd,args);
		}
	} else {
		worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
	}
}
