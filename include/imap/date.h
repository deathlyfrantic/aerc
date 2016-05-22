#ifndef _IMAP_DATE_H
#define _IMAP_DATE_H

#include <time.h>

char *parse_imap_date(const char *str, struct tm *time);

#endif
