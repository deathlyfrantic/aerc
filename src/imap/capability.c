#define _POSIX_C_SOURCE 201112LL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "internal/imap.h"
#include "util/stringop.h"
#include "util/list.h"
#include "log.h"

void imap_capability(struct imap_connection *imap, imap_callback_t callback,
		void *data) {
	/* CAPABILITY commands have no arguments or fancy logic */
	imap_send(imap, callback, data, "CAPABILITY");
}

void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args) {
	struct imap_capabilities *cap = calloc(1,
			sizeof(struct imap_capabilities));

	/*
	 * Each one of these capabilities is supported (or at least tracked) by
	 * aerc. This array includes the name of the capability and a pointer to the
	 * corresponding bool in the imap_capabilities structure.
	 */
	struct { const char *name; bool *ptr; } ptrs[] = {
		{ "IMAP4rev1", &cap->imap4rev1 },
		{ "STARTTLS", &cap->starttls },
		{ "LOGINDISABLED", &cap->logindisabled },
		{ "AUTH=PLAIN", &cap->auth_plain },
		{ "AUTH=LOGIN", &cap->auth_login },
		{ "IDLE", &cap->idle }
	};

	/* For each argument... */
	while (args) {
		if (args->type == IMAP_STRING || args->type == IMAP_ATOM) {
			/* if it's an atom or string, iterate over aerc's capability list... */
			for (size_t j = 0; j < sizeof(ptrs) / (sizeof(void*) * 2); ++j) {
				/* and compare our capability with the server's capability.. */
				if (strcmp(ptrs[j].name, args->str) == 0) {
					/* and set it to true if the server provides it */
					*ptrs[j].ptr = true;
				}
			}
		}
		args = args->next;
	}

	free(imap->cap);
	imap->cap = cap;
}
