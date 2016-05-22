/*
 * urlparse.c - parser for RFC 3986 URI strings
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "urlparse.h"

bool percent_decode(char *s) {
	/*
	 * Percent encoded strings (RFC 1738, 2.2) use a percent symbol followed by
	 * two hex digits to represent certain characters. This function replaces
	 * the percent encoded characters with the actual characters they represent
	 * in-place.
	 */
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
	/*
	 * URIs take the general form of:
	 *
	 * 	protocol:[//][user[:password]@]hostname[:port][/path][?query][#fragment]
	 *
	 * This function only concerns itself with the protocol, user, password,
	 * hostname, and port. It returns true if the URI is valid.
	 */
	memset(res, 0, sizeof(struct uri));
	/*
	 * First, we parse the scheme.
	 */
	const char *cur = src, *start = src;
	while (*cur && *cur != ':') {
		++cur;
	}
	if (!*cur) return false;
	res->scheme = malloc(cur - start + 1);
	strncpy(res->scheme, start, cur - start);
	res->scheme[cur - start] = '\0';
	++cur;
	/*
	 * The // is optional.
	 */
	if (*cur == '/') cur++;
	if (*cur == '/') cur++;
	/*
	 * Next, we extract a string that contains the user, pass, domain, and port.
	 * This function doesn't parse the path/query/fragment, but we do tolerate
	 * it.
	 */
	if (!*cur) return false;
	char *stop = "/?#\0"; // Any of these chararacters are stopping points
	start = cur;
	while (*cur && !strchr(stop, *cur)) {
		++cur;
	}
	/*
	 * Measure/allocate/copy some the hostname string, which will (for now)
	 * include the user/pass/port as well.
	 */
	res->hostname = malloc(cur - start + 1);
	strncpy(res->hostname, start, cur - start);
	res->hostname[cur - start] = '\0';
	if (strchr(res->hostname, '@')) {
		/*
		 * A username/pass string is present in the hostname string. Extract it.
		 */
		char *at = strchr(res->hostname, '@');
		*at = '\0';
		res->username = res->hostname;
		res->hostname = strdup(at + 1);
		if (strchr(res->username, ':')) {
			/*
			 * A password is present in the username/pass string. Extract it.
			 */
			at = strchr(res->username, ':');
			*at = '\0';
			res->password = strdup(at + 1);
		}
	}
	if (strchr(res->hostname, ':')) {
		/*
		 * A port is present in the hostname string. Extract it.
		 */
		char *at = strchr(res->hostname, ':');
		*at = '\0';
		res->port = strdup(at + 1);
	}
	/*
	 * Percent decode everything and return.
	 */
	if (!percent_decode(res->hostname)) return false;
	if (!percent_decode(res->username)) return false;
	if (!percent_decode(res->password)) return false;
	if (!percent_decode(res->port)) return false;
	if (!*cur) return true;

	return false;
}

void uri_free(struct uri *uri) {
	free(uri->scheme);
	free(uri->username);
	free(uri->password);
	free(uri->hostname);
	free(uri->port);
	free(uri->path);
	free(uri->query);
	free(uri->fragment);
}
