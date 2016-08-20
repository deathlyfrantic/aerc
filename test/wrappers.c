#include <string.h>
#include <stdio.h>
#include "tests.h"
#include "util/hashtable.h"

void *__wrap_hashtable_get(hashtable_t *table, const void *key) {
	check_expected_ptr(key);
	return mock_type(void *);
}

int __wrap_poll(struct pollfd fds[], nfds_t nfds, int timeout) {
	return mock_type(int);
}

void *ab_buffer;
size_t ab_buffer_size;

void set_ab_recv_result(void *buffer, size_t size) {
	ab_buffer = buffer;
	ab_buffer_size = size;
}

int __wrap_ab_recv(absocket_t *socket, void *buffer, size_t len) {
	assert_true(ab_buffer_size <= len);
	if (ab_buffer) {
		memcpy(buffer, ab_buffer, ab_buffer_size);
		ab_buffer = NULL;
	}
	return mock_type(int);
}
