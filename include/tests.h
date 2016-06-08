#ifndef __TESTS_H
#define __TESTS_H

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <poll.h>
#include "absocket.h"
#include "util/list.h"
#include "util/hashtable.h"

/* Wrappers */
void *__wrap_hashtable_get(hashtable_t *table, const void *key);
int __wrap_poll(struct pollfd fds[], nfds_t nfds, int timeout);
void set_ab_recv_result(void *buffer, size_t size);
int __wrap_ab_recv(absocket_t *socket, void *buffer, size_t *len);

/* Tests */
int run_tests_urlparse();
int run_tests_imap();

#endif
