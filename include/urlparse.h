#ifndef _URLPARSE_H
#define _URLPARSE_H

#include <stdbool.h>

struct uri {
	char *scheme;
	char *username;
	char *password;
	char *hostname;
	char *port;
	char *path;
	char *query;
	char *fragment;
};

bool parse_uri(struct uri *uri, const char *src);
void uri_free(struct uri *res);

#endif
