#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "urlparse.h"

bool percent_decode(char *s) {
	if (!s) return true;
	while (*s) {
		if (*s == '+') {
			*s = ' ';
		} else if (*s == '%') {
			int c;
			if (!isxdigit(*(s + 1)) || !isxdigit(*(s + 2)) || !sscanf(s + 1, "%2x", &c)) {
				return false;
			} else {
				*s = c;
				memmove(s + 1, s + 3, strlen(s) - 1);
			}
		}
		++s;
	}
	return true;
}
 
bool parse_uri(struct uri *res, const char *src) {
	memset(res, 0, sizeof(struct uri));
	// Start with scheme
	const char *cur = src, *start = src;
	while (*cur && *cur != ':') {
		++cur;
	}
	if (!*cur) return false;
	res->scheme = malloc(cur - start + 1);
	strncpy(res->scheme, start, cur - start);
	res->scheme[cur - start] = '\0';
	++cur;
	if (*cur == '/') cur++;
	if (*cur == '/') cur++; // Skip optional //

	// Extract user/pass/domain/port string
	if (!*cur) return false;
	char *stop = "/?#\0";
	start = cur;
	while (*cur && !strchr(stop, *cur)) {
		++cur;
	}
	res->hostname = malloc(cur - start + 1);
	strncpy(res->hostname, start, cur - start);
	res->hostname[cur - start] = '\0';
	if (strchr(res->hostname, '@')) {
		// Username present
		char *at = strchr(res->hostname, '@');
		*at = '\0';
		res->username = res->hostname;
		res->hostname = strdup(at + 1);
		if (strchr(res->username, ':')) {
			// Password present
			at = strchr(res->username, ':');
			*at = '\0';
			res->password = strdup(at + 1);
		}
	}
	if (strchr(res->hostname, ':')) {
		// Port present
		char *at = strchr(res->hostname, ':');
		*at = '\0';
		res->port = strdup(at + 1);
	}
	if (!percent_decode(res->hostname)) return false;
	if (!percent_decode(res->username)) return false;
	if (!percent_decode(res->password)) return false;
	if (!percent_decode(res->port)) return false;
	if (!*cur) return true;

	// TODO: Extract path/query/fragment if anyone cares
	return false;
}

void uri_free(struct uri *uri) {
	if (uri->scheme) free(uri->scheme);
	if (uri->username) free(uri->username);
	if (uri->password) free(uri->password);
	if (uri->hostname) free(uri->hostname);
	if (uri->port) free(uri->port);
	if (uri->path) free(uri->path);
	if (uri->query) free(uri->query);
	if (uri->fragment) free(uri->fragment);
}
