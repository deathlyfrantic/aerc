#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "email/headers.h"
#include "log.h"
#include "util/list.h"

int parse_headers(const char *headers, list_t *output) {
	while (strstr(headers, "\r\n") == headers) {
		headers += 2;
	}
	while (*headers) {
		char *crlf = strstr(headers, "\r\n");
		char *null = strchr(headers, '\0');
		char *eol = crlf == NULL ? null : crlf;
		int eol_i = eol - headers;

		if (isspace(*headers)) {
			while (isspace(*++headers));
			if (output->length != 0) {
				/* Concat with previous line */
				struct email_header *h = output->items[output->length - 1];
				char *prev = h->value;
				char *new = malloc(strlen(prev) + eol_i + 2);
				strcpy(new, prev);
				strcat(new, " ");
				strncat(new, headers, eol_i);
				new[eol_i + strlen(prev)] = '\0';
				h->value = new;
				free(prev);
				headers = eol;
				if (*headers) headers += 2;
				worker_log(L_DEBUG, "Extended previous header: %s: %s",
						h->key, h->value);
				continue;
			}
		}

		char *colon = strchr(headers, ':');
		if (!colon) {
			headers = eol;
			if (*headers) headers += 2;
		}
		int colon_i = colon - headers;
		char *key = malloc(colon_i + 1);
		strncpy(key, headers, colon_i);
		key[colon_i] = '\0';
		colon_i += 2;
		char *value = malloc(eol_i - colon_i + 1);
		strncpy(value, headers + colon_i, eol_i - colon_i);
		value[eol_i - colon_i] = '\0';
		worker_log(L_DEBUG, "Parsed header: %s: %s", key, value);
		headers = eol;
		if (*headers) headers += 2;
		struct email_header *header = calloc(1, sizeof(struct email_header));
		header->key = key;
		header->value = value;
		list_add(output, header);
	}
	return 0;
}

void free_headers(list_t *headers) {
	if (!headers) return;
	for (int i = 0; i < headers->length; ++i) {
		struct email_header *header = headers->items[i];
		if (header) {
			free(header->key);
			free(header->value);
			free(header);
		}
	}
	list_free(headers);
}
