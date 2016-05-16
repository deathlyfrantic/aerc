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
		void *data, const imap_args_t *fields) {
	// TODO
}
