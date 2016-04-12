#define _POSIX_C_SOURCE 201112LL
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "imap/imap.h"
#include "log.h"

void handle_imap_OK(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args) {
	worker_log(L_DEBUG, "IMAP OK %s", args);
}
