#ifndef _INTERNAL_IMAP_H
#define _INTERNAL_IMAP_H

#include "imap/imap.h"
#include "util/list.h"

struct imap_pending_callback {
    imap_callback_t callback;
    void *data;
};

enum imap_type {
    IMAP_ATOM,      /* imap->str is valid */
    IMAP_NUMBER,    /* imap->num is valid */
    IMAP_STRING,    /* imap->str is valid */
    IMAP_LIST,      /* imap->list is valid */
    IMAP_RESPONSE,  /* imap->str is valid */
    IMAP_NIL        /* Not actually used, will be IMAP_ATOM and imap->str will
                       equal "NIL" */
};

struct imap_arg {
    enum imap_type type;
    struct imap_arg *next;
    char *str;
    long num;
    struct imap_arg *list;
    char *original;
};
typedef struct imap_arg imap_arg_t;

void handle_line(struct imap_connection *imap, const char *line);

void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);
void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, imap_arg_t *args);

/* Parses an IMAP argument string and returns the number of characters
 * necessary to complete parsing (if the string doesn't represent a complete
 * arg string).
 */
int imap_parse_args(const char *str, imap_arg_t *args);
void imap_arg_free(imap_arg_t *args);
void print_imap_args(imap_arg_t *args, int indent);

#endif
