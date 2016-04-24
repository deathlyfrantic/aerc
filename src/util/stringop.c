#define _POSIX_C_SOURCE 201112LL
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include "util/stringop.h"
#include "util/list.h"

const char whitespace[] = " \f\n\r\t\v";

unsigned int hash_string(const void *_str) {
	/* djb2 */
	const char *str = _str;
	unsigned int hash = 5381;
	char c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

list_t *split_string(const char *str, const char *delims) {
	/*
	 * Splits up a string at each delimiter, and returns a list_t with the
	 * results.
	 */
	list_t *res = create_list();
	char *copy = strdup(str);
	char *token;

	token = strtok(copy, delims);
	while(token) {
		token = strdup(token);
		list_add(res, token);
		token = strtok(NULL, delims);
	}
	free(copy);
	return res;
}

void free_flat_list(list_t *list) {
	/*
	 * Frees a list_t and each item inside of it.
	 */
	int i;
	for (i = 0; i < list->length; ++i) {
		free(list->items[i]);
	}
	list_free(list);
}

char **split_args(const char *start, int *argc) {
	/*
	 * Splits an argument string, respecting quotes.
	 */
	*argc = 0;
	int alloc = 2;
	char **argv = malloc(sizeof(char *) * alloc);
	bool in_token = false;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	const char *end = start;
	if (start) {
		while (*start) {
			if (!in_token) {
				start = (end += strspn(end, whitespace));
				in_token = true;
			}
			if (*end == '"' && !in_char && !escaped) {
				in_string = !in_string;
			} else if (*end == '\'' && !in_string && !escaped) {
				in_char = !in_char;
			} else if (*end == '\\') {
				escaped = !escaped;
			} else if (*end == '\0' || (!in_string && !in_char
						&& !escaped && strchr(whitespace, *end))) {
				goto add_token;
			}
			if (*end != '\\') {
				escaped = false;
			}
			++end;
			continue;
			add_token:
			if (end - start > 0) {
				char *token = malloc(end - start + 1);
				strncpy(token, start, end - start + 1);
				token[end - start] = '\0';
				argv[*argc] = token;
				if (++*argc + 1 == alloc) {
					argv = realloc(argv, (alloc *= 2) * sizeof(char *));
				}
			}
			in_token = false;
			escaped = false;
		}
	}
	argv[*argc] = NULL;
	return argv;
}

void free_argv(int argc, char **argv) {
	while (--argc > 0) {
		free(argv[argc]);
	}
	free(argv);
}

char *join_args(char **argv, int argc) {
	/*
	 * Joins an argv+argc into a single string, separating each with spaces.
	 */
	int len = 0, i;
	for (i = 0; i < argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	char *res = malloc(len);
	len = 0;
	for (i = 0; i < argc; ++i) {
		strcpy(res + len, argv[i]);
		len += strlen(argv[i]);
		res[len++] = ' ';
	}
	res[len - 1] = '\0';
	return res;
}

char *join_list(list_t *list, char *separator) {
	/*
	 * Join a list of strings, adding separator in between. Separator can be
	 * NULL.
	 */
	size_t len = 1; // NULL terminator
	size_t sep_len = 0;
	if (separator != NULL) {
		sep_len = strlen(separator);
		len += (list->length - 1) * sep_len;
	}

	for (int i = 0; i < list->length; i++) {
		len += strlen(list->items[i]);
	}

	char *res = malloc(len);

	char *p = res + strlen(list->items[0]);
	strcpy(res, list->items[0]);

	for (int i = 1; i < list->length; i++) {
		if (sep_len) {
			memcpy(p, separator, sep_len);
			p += sep_len;
		}
		strcpy(p, list->items[i]);
		p += strlen(list->items[i]);
	}

	*p = '\0';

	return res;
}
