#define _POSIX_C_SOURCE 201112LL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "imap/imap.h"
#include "util/hashtable.h"
#include "log.h"

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	worker_log(L_DEBUG, "IMAP OK: %s", args);
	imap->events.ready(imap->events.data);
}

void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	// TODO: Parse status response codes
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
			// TODO: parse out status response codes
			handler(imap, token, cmd, args);
		} else {
			worker_log(L_DEBUG, "Got unsolicited status command for %s", token);
		}
	}
}
