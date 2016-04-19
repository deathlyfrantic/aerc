#include "imap/imap.h"
#include "util/list.h"
#include "util/stringop.h"

void imap_list(struct imap_connection *imap, imap_callback_t callback,
		const char *refname, const char *boxname) {
	imap_send(imap, callback, "LIST \"%s\" \"%s\"", refname, boxname);
}

void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args) {
	list_t *args = split_string(_args, " ");
	free_flat_list(args);
}
