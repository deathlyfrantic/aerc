#include "tests.h"
#include "util/hashtable.h"

void *__wrap_hashtable_get(hashtable_t *table, const void *key) {
	check_expected_ptr(key);
	return mock_type(void *);
}
