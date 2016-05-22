#define _XOPEN_SOURCE
#include <time.h>
#include "imap/date.h"

char *parse_imap_date(const char *str, struct tm *time) {
	/*
	 * Note: IMAP dates and email header dates (i.e. RFC2822) are two different
	 * formats.
	 */
	return strptime(str, "%d-%b-%Y %H:%M:%S %z", time);
}
