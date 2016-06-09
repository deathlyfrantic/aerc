#include <stdlib.h>
#include <string.h>
#include "tests.h"
#include "util/list.h"
#include "email/headers.h"

static void test_parse_headers_simple(void **state) {
	const char *headers = "Subject: hello world\r\n"
		"Date: test\r\n"
		"From: Foo Bar <fbar@example.org>";
	list_t *output = create_list();
	parse_headers(headers, output);
	assert_int_equal(3, output->length);
	struct email_header expected[] = {
		{ "Subject", "hello world" },
		{ "Date", "test" },
		{ "From", "Foo Bar <fbar@example.org>" }
	};
	for (int i = 0; i < 3; ++i) {
		struct email_header *test = output->items[i];
		assert_string_equal(test->key, expected[i].key);
		assert_string_equal(test->value, expected[i].value);
	}
	free_headers(output);
}

static void test_parse_headers_continued(void **state) {
	const char *headers = "Subject: hello world\r\n"
		" extended\r\n"
		"Date: test\r\n"
		"From: Foo Bar <fbar@example.org>";
	list_t *output = create_list();
	parse_headers(headers, output);
	assert_int_equal(3, output->length);
	struct email_header expected[] = {
		{ "Subject", "hello world extended" },
		{ "Date", "test" },
		{ "From", "Foo Bar <fbar@example.org>" }
	};
	for (int i = 0; i < 3; ++i) {
		struct email_header *test = output->items[i];
		assert_string_equal(test->key, expected[i].key);
		assert_string_equal(test->value, expected[i].value);
	}
	free_headers(output);
}

int run_tests_headers() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_parse_headers_simple),
		cmocka_unit_test(test_parse_headers_continued),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
