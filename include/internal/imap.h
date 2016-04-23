#ifndef _INTERNAL_IMAP_H
#define _INTERNAL_IMAP_H

#include "imap/imap.h"
#include "util/list.h"

struct imap_pending_callback {
    imap_callback_t callback;
    void *data;
};

void handle_line(struct imap_connection *imap, const char *line);
struct imap_pending_callback *make_callback(imap_callback_t callback, void *data);

void init_status_handlers();
void handle_imap_status(struct imap_connection *imap, const char *token,
		const char *cmd, const char *args);
void handle_imap_capability(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args);
void handle_imap_list(struct imap_connection *imap, const char *token,
		const char *cmd, const char *_args);

enum imap_type {
    IMAP_ATOM,
    IMAP_NUMBER,
    IMAP_STRING,
    IMAP_LIST,
    IMAP_NIL
};

struct imap_arg {
    enum imap_type type;
    struct imap_arg *next;

    char *str; // Atom or string
    long num;
    struct imap_arg *list;
};
typedef struct imap_arg imap_arg_t;

/* Parses an IMAP argument string and returns the number of characters
 * necessary to complete parsing (if the string doesn't represent a complete
 * arg string).
 */
int imap_parse_args(const char *str, imap_arg_t *args);
void print_imap_args(imap_arg_t *args, int indent);

#endif
