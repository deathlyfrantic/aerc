#include <stdlib.h>
#include "tests.h"
#include "internal/imap.h"
#include "imap/imap.h"

extern void imap_init(struct imap_connection *imap);
extern int handle_line(struct imap_connection *imap, imap_arg_t *arg);

void test_handle_line_unknown_handler(void **state) {
	int _;
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);

	imap_arg_t *arg = malloc(sizeof(imap_arg_t));
	imap_parse_args("tag FOOBAR", arg, &_);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, NULL);

	handle_line(imap, arg);

	free(arg);
	imap_close(imap);
}

bool handler_called = false;

static void test_handler(struct imap_connection *imap,
	const char *token, const char *cmd, imap_arg_t *args) {
	handler_called = true;
	assert_string_equal(token, "tag");
	assert_string_equal(cmd, "FOOBAR");
}

void test_handle_line_known_handler(void **state) {
	int _;
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);

	imap_arg_t *arg = malloc(sizeof(imap_arg_t));
	imap_parse_args("tag FOOBAR", arg, &_);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);

	handle_line(imap, arg);

	assert_true(handler_called);

	free(arg);
	imap_close(imap);
}

int run_tests_imap() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_handle_line_unknown_handler),
		cmocka_unit_test(test_handle_line_known_handler),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
