/*
 * imap/parse.c - parser for IMAP argument strings
 */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imap/imap.h"

static long parse_number(const char **str) {
	/*
	 * Parses a base 10 number from the string and advances the pointer to the
	 * end of the argument (usually one of ' ', ')', or '\0').
	 */
	char *end = NULL;
	long l = strtol(*str, &end, 10);
	*str = end;
	return l;
}

static char *parse_string(const char **str, int *remaining) {
	/*
	 * IMAP strings come in two forms - quoted or literal. A quoted string has
	 * limitations on the characters in use (no quotes, no spaces, maybe some
	 * others). A literal string begins with a prefix {n}, where n is the length
	 * of the string in characters, followed by that many characters.
	 */
	if (**str == '"') {
		(*str)++; // advance past "
		const char *end = strchr(*str, '"');
		if (end == NULL) {
			// We don't have the complete string, but we also don't know how
			// long the completed string is. Just return 1 here.
			*remaining = 1;
			while (**str) (*str)++;
			return NULL;
		}
		/* Allocate space for the string and copy it in, then advance *str */
		char *result = malloc(end - *str + 1);
		strncpy(result, *str, end - *str);
		result[end - *str] = '\0';
		*str = end + 1;
		return result;
	} else if (**str == '{') {
		(*str)++; // advance past {
		long len = parse_number(str);
		if (**str != '}') {
			return NULL;
		}
		(*str)++; // advance past }
		if ((long)strlen(*str) < len) {
			// We don't have the full string. Return the expected length of the
			// string.
			*remaining = (int)(len - strlen(*str));
			while (**str) (*str)++;
			return NULL;
		}
		/* Allocate space for the string and copy it in, then advance *str */
		char *result = malloc(len + 1);
		strncpy(result, *str, len);
		result[len] = '\0';
		*str += len;
		return result;
	}
	return NULL;
}

static char *parse_atom(const char **str) {
	/*
	 * An atom is basically a shitty string. It's unquoted, not prefixed with
	 * its length, and has limitations on the characters you can use. First, we
	 * look for the end of it - a space or a ) if we're parsing a list. We pick
	 * the one that's closest ahead and get just copy the text into a new
	 * string.
	 */
	char *end = NULL;
	char *delims = " )[\r";
	for (size_t i = 0; i < sizeof(delims) - 1; ++i) {
		char *_ = strchr(*str, delims[i]);
		if (_ && (!end || _ < end)) end = _;
	}

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

static char *parse_status_response(const char **str) {
	/*
	 * Status responses can include extra information in the command text like
	 * this:
	 *
	 * * OK [status response here] [rest of args...]
	 *
	 * So here we pull that status response out into a string.
	 */
	char *end = strchr(*str, ']');
	int len = (end - *str) - 1;
	char *resp = malloc(len + 1);
	strncpy(resp, *str + 1, len);
	resp[len] = '\0';
	*str += len + 2;
	return resp;
}

static int _imap_parse_args(const char **str, imap_arg_t *args) {
	assert(args && str);
	int remaining = 0;
	while (**str
			&& **str != ')' /* ) for recursive list parsing */
			&& **str != '\r' /* end of args */) {
		if (isdigit(**str)) {
			args->type = IMAP_NUMBER;
			args->num = parse_number(str);
		} else if (**str == '"' || **str == '{') {
			args->type = IMAP_STRING;
			args->str = parse_string(str, &remaining);
			if (remaining > 0) {
				break;
			}
		} else if (**str == '[') {
			args->type = IMAP_RESPONSE;
			args->str = parse_status_response(str);
		} else if (**str == '(') {
			args->type = IMAP_LIST;
			args->list = calloc(1, sizeof(imap_arg_t));
			(*str)++;
			/*
			 * Parsing lists is done recursively, since they're basically nested
			 * arg strings.
			 */
			remaining = _imap_parse_args(str, args->list);
			if (remaining == 2) {
				// the recursive call will complain about the lack of CRLF
				remaining = 0;
			}
			if (remaining > 0 || **str != ')') {
				// Incomplete list
				if (remaining == 0) {
					remaining = 1;
				}
				break;
			}
			(*str)++; // advance past )
			if (args->list->type == IMAP_ATOM && !args->list->str) {
				// Special case for an empty list
				free(args->list);
				args->list = NULL;
			}
		} else {
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
		if (**str && **str != ')' && **str != '\r') {
			/*
			 * If we aren't at the end of the loop, allocate the next
			 * argument.
			 */
			imap_arg_t *prev = args;
			args = calloc(1, sizeof(imap_arg_t));
			prev->next = args;
		}
	}
	if (**str == '\r') {
		(*str)++;
		if (**str == '\n') {
			(*str)++;
		} else {
			remaining++;
		}
	} else {
		remaining += 2;
	}
	return remaining;
}

int imap_parse_args(const char *str, imap_arg_t *args, int *remaining) {
	memset(args, 0, sizeof(imap_arg_t));
	const char *orig = str;
	args->original = strdup(str);
	*remaining = _imap_parse_args(&str, args);
	return (int)(str - orig); // len
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

char *serialize_args(const imap_arg_t *args) {
	const imap_arg_t *_args = args;
	/*
	 * Measure:
	 */
	int size = 0;
	while (args) {
		bool literal;
		char *list;
		switch (args->type) {
		case IMAP_ATOM:
			size += snprintf(NULL, 0, "%s", args->str);
			break;
		case IMAP_NUMBER:
			size += snprintf(NULL, 0, "%ld", args->num);
			break;
		case IMAP_STRING:
			literal = false;
			for (size_t i = 0; i < strlen(args->str); ++i) {
				const char *blacklist = "\\\"";
				if (strchr(blacklist, args->str[i])) {
					literal = true;
					break;
				}
			}
			if (literal) {
				size += snprintf(NULL, 0, "{%zd}%s",
						strlen(args->str), args->str);
			} else {
				size += snprintf(NULL, 0, "\"%s\"", args->str);
			}
			break;
		case IMAP_LIST:
			// TODO: Avoid doing the whole serialize thing twice
			list = serialize_args(args->list);
			size += snprintf(NULL, 0, "(%s)", list);
			free(list);
			break;
		default:
			break;
		}
		if (args->next) {
			size++; // ' '
		}
		args = args->next;
	}
	/*
	 * Allocate:
	 */
	char *result = malloc(size + 1);
	char *_ = result;
	args = _args;
	/*
	 * Print:
	 */
	while (args) {
		bool literal;
		char *list;
		switch (args->type) {
		case IMAP_ATOM:
			_ += sprintf(_, "%s", args->str);
			break;
		case IMAP_NUMBER:
			_ += sprintf(_, "%ld", args->num);
			break;
		case IMAP_STRING:
			literal = false;
			for (size_t i = 0; i < strlen(args->str); ++i) {
				const char *blacklist = "\\\"";
				if (strchr(blacklist, args->str[i])) {
					literal = true;
					break;
				}
			}
			if (literal) {
				// TODO: I think these require a thing from the server before
				// you proceed. Check this.
				_ += sprintf(_, "{%zd}%s",
						strlen(args->str), args->str);
			} else {
				_ += sprintf(_, "\"%s\"", args->str);
			}
			break;
		case IMAP_LIST:
			list = serialize_args(args->list);
			_ += sprintf(_, "(%s)", list);
			free(list);
			break;
		default:
			break;
		}
		if (args->next) {
			*_ = ' ';
			++_;
		}
		args = args->next;
	}
	*_ = '\0';
	return result;
}

void print_imap_args(FILE *f, imap_arg_t *args, int indent) {
	while (args) {
		char *types[] = {
			"ATOM", "NUMBER", "STRING",
			"LIST", "RESPONSE", "NIL"
		};
		for (int i = indent; i; --i) fprintf(f, " ");
		switch (args->type) {
		case IMAP_NUMBER:
			fprintf(f, "%s %ld\n", types[args->type], args->num);
			break;
		case IMAP_LIST:
			fprintf(f, "%s\n", types[args->type]);
			print_imap_args(f, args->list, indent + 2);
			break;
		default:
			fprintf(f, "%s %s\n", types[args->type], args->str);
			break;
		}
		args = args->next;
	}
}
