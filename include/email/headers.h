#ifndef _EMAIL_HEADERS_H
#define _EMAIL_HEADERS_H

#include "util/list.h"

struct email_header {
    char *key, *value;
};

int parse_headers(const char *headers, list_t *output);
void free_headers(list_t *headers);

#endif
