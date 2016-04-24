#define _POSIX_C_SOURCE 200809L
#include "internal/imap.h"
#include <string.h>
#include <ctype.h>
#include <assert.h>

static long parse_digit(const char **str) {
	char *end = NULL;
	long l = strtol(*str, &end, 10);
	*str = end;
	return l;
}

static char *parse_string(const char **str, int *remaining) {
	if (**str == '"') {
		(*str)++;
		const char *end = strchr(*str, '"');
		if (end == NULL) {
			*remaining = 1;
			return NULL;
		}
		char *result = malloc(end - *str + 1);
		strncpy(result, *str, end - *str);
		result[end - *str] = '\0';
		*str = end + 1;
		return result;
	} else if (**str == '{') {
		(*str)++;
		long len = parse_digit(str);
		if (**str != '}') {
			return NULL;
		}
		(*str)++;
		if ((long)strlen(*str) < len) {
			*remaining = (int)(len - strlen(*str));
			return NULL;
		}
		char *result = malloc(len + 1);
		strncpy(result, *str, len);
		result[len] = '\0';
		*str += len;
		return result;
	}
	return NULL;
}

static char *parse_atom(const char **str) {
	char *end;
	char *paren = strchr(*str, ')');
	char *space = strchr(*str, ' ');
	if (paren && paren < space) end = paren;
	else end = space;

	if (!end) {
		char *_ = strdup(*str);
		*str = strchr(*str, '\0');
		return _;
	} else {
		char *_ = malloc(end - *str + 1);
		strncpy(_, *str, end - *str);
		_[end - *str] = '\0';
		*str = end;
		return _;
	}
}

static int _imap_parse_args(const char **str, imap_arg_t *args) {
	assert(args && str);
	int remaining = 0;
	while (**str && **str != ')') {
		if (isdigit(**str)) {
			args->type = IMAP_NUMBER;
			args->num = parse_digit(str);
		} else if (**str == '"' || **str == '{') {
			args->type = IMAP_STRING;
			args->str = parse_string(str, &remaining);
			if (remaining > 0) {
				return remaining;
			}
		} else if (**str == '(') {
			// Parse list
			args->type = IMAP_LIST;
			args->list = calloc(1, sizeof(imap_arg_t));
			(*str)++;
			remaining = _imap_parse_args(str, args->list);
			if (remaining > 0) {
				return remaining;
			}
			if (**str != ')') {
				return 1;
			}
			(*str)++;
			if (args->list->type == IMAP_ATOM && !args->list->str) {
				free(args->list);
				args->list = NULL;
			}
		} else {
			// Parse atom
			// Note: this will also catch NIL and interpret it as an atom
			// This is intentional because the IMAP specificiation is
			// explicitly ambiguous on whether or not the "NIL" characters as
			// an argument could be an atom or the literal NIL. I leave it up
			// to the command implementation to strcmp an atom against NIL to
			// find the difference if it matters to that command.
			args->type = IMAP_ATOM;
			args->str = parse_atom(str);
		}
		if (**str == ' ') (*str)++;
		if (**str && **str != ')') {
			imap_arg_t *prev = args;
			args = calloc(1, sizeof(imap_arg_t));
			prev->next = args;
		}
	}
	return remaining;
}

int imap_parse_args(const char *str, imap_arg_t *args) {
	int r = _imap_parse_args(&str, args);
	args->original = strdup(str);
	return r;
}

void imap_arg_free(imap_arg_t *args) {
	while (args) {
		free(args->original);
		free(args->str);
		imap_arg_free(args->list);
		imap_arg_t *_ = args;
		args = args->next;
		free(_);
	}
}

void print_imap_args(imap_arg_t *args, int indent) {
	while (args) {
		char *types[] = {
			"ATOM", "NUMBER", "STRING",
			"LIST", "NIL"
		};
		for (int i = indent; i; --i) printf(" ");
		switch (args->type) {
		case IMAP_NUMBER:
			printf("%s %ld\n", types[args->type], args->num);
			break;
		case IMAP_LIST:
			printf("%s\n", types[args->type]);
			print_imap_args(args->list, indent + 2);
			break;
		default:
			printf("%s %s\n", types[args->type], args->str);
			break;
		}
		args = args->next;
	}
}
