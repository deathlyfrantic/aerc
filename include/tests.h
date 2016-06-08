#ifndef __TESTS_H
#define __TESTS_H

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include "util/list.h"
#include "util/hashtable.h"

/* Wrappers */
void *__wrap_hashtable_get(hashtable_t *table, const void *key);

/* Tests */
int run_tests_urlparse();
int run_tests_imap();

#endif
