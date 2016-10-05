#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "email/headers.h"
#include "log.h"
#include "util/list.h"
#include "util/base64.h"

static int handle_base64(char *input, int len) {
	int flen;
	unsigned char *result = unbase64(input, len, &flen);
	memcpy(input, (char *)result, flen);
	free(result);
	return flen;
}

static int handle_q(char *input, int len) {
	for (int i = 0; i < len && input[i]; ++i) {
		if (input[i] == '_') {
			input[i] = ' ';
		} else if (input[i] == '=') {
			char temp = input[i + 3];
			input[i + 3] = '\0';
			char val = (char)strtol(input + i + 1, NULL, 16);
			input[i + 3] = temp;
			input[i] = val;
			memmove(&input[i + 1], &input[i + 3], len - i - 3);
			len -= 2;
		}
	}
	return len;
}

static void decode_rfc1342(char *input) {
	for (int i = 0; input[i]; ++i) {
		if (strstr(&input[i], "=?") == &input[i]) {
			char *start = &input[i];
			char *charset = start + 2;
			// TODO: Convert from whatever charset to utf-8 (or native)
			char *encoding = strchr(charset, '?');
			if (!encoding) {
				continue;
			}
			encoding++;
			if (encoding[1] != '?') {
				continue;
			}
			char *data = encoding + 2;
			char *end = strstr(data, "?=");
			int len;
			if (tolower(encoding[0]) == 'b') {
				len = handle_base64(data, end - data);
			} else if (tolower(encoding[0]) == 'q') {
				len = handle_q(data, end - data);
			} else {
				continue;
			}
			int diff = end - data - len;
			memmove(start, data, strlen(data) + 1);
			char *_ = start + len + diff + 2;
			memmove(start + len, _, strlen(_) + 1);
		}
	}
}

int parse_headers(const char *headers, list_t *output) {
	while (*headers && strstr(headers, "\r\n") == headers) {
		headers += 2;
	}
	if (strstr(headers, "From: \"United") == headers) {
		headers++;
		headers--;
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
		if (strstr(colon + 1, "\r\n") != colon + 1) {
			colon_i += 2;
		}
		char *value = malloc(eol_i - colon_i + 1);
		strncpy(value, headers + colon_i, eol_i - colon_i);
		value[eol_i - colon_i] = '\0';
		headers = eol;
		if (*headers) headers += 2;
		struct email_header *header = calloc(1, sizeof(struct email_header));
		header->key = key;
		header->value = value;
		list_add(output, header);
	}
	// Extra decoding here if necessary
	for (size_t i = 0; i < output->length; ++i) {
		struct email_header *header = output->items[i];
		decode_rfc1342(header->value);
		worker_log(L_DEBUG, "Parsed header: %s: %s", header->key, header->value);
	}
	return 0;
}

void free_headers(list_t *headers) {
	if (!headers) return;
	for (size_t i = 0; i < headers->length; ++i) {
		struct email_header *header = headers->items[i];
		if (header) {
			free(header->key);
			free(header->value);
			free(header);
		}
	}
	list_free(headers);
}
