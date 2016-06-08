#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include "tests.h"
#include "urlparse.h"

extern bool percent_decode(char *s);

static void test_percent_decode_no_entities(void **state) {
	char *test = "hello world";
	assert_true(percent_decode(test));
	assert_string_equal("hello world", test);
}

static void test_percent_decode_failure(void **state) {
	char *test = "hello %world";
	assert_false(percent_decode(test));
}

static void test_percent_decode(void **state) {
	char *test = strdup("hello%20world");
	assert_true(percent_decode(test));
	assert_string_equal(test, "hello world");
	free(test);
}

static void test_not_a_uri(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_false(parse_uri(uri, "hello world"));
	assert_false(parse_uri(uri, "invalid://"));
	uri_free(uri);
}

static void test_hostname_only(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://hostname"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "hostname");
	assert_null(uri->username);
	assert_null(uri->password);
	assert_null(uri->port);
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

static void test_username(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://username@hostname"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "hostname");
	assert_string_equal(uri->username, "username");
	assert_null(uri->password);
	assert_null(uri->port);
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

static void test_username_and_password(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://username:password@hostname"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "hostname");
	assert_string_equal(uri->username, "username");
	assert_string_equal(uri->password, "password");
	assert_null(uri->port);
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

static void test_port(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://hostname:1234"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "hostname");
	assert_string_equal(uri->port, "1234");
	assert_null(uri->username);
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

static void test_user_pass_port_hostname(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://user:pass@hostname:1234"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "hostname");
	assert_string_equal(uri->port, "1234");
	assert_string_equal(uri->username, "user");
	assert_string_equal(uri->password, "pass");
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

static void test_uri_percent_decode(void **state) {
	struct uri *uri = malloc(sizeof(struct uri));
	assert_true(parse_uri(uri, "protocol://us%20er:pa%20ss@host%20name"));
	assert_string_equal(uri->scheme, "protocol");
	assert_string_equal(uri->hostname, "host name");
	assert_string_equal(uri->username, "us er");
	assert_string_equal(uri->password, "pa ss");
	assert_null(uri->path);
	assert_null(uri->query);
	assert_null(uri->fragment);
	uri_free(uri);
}

int run_tests_urlparse() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_percent_decode_no_entities),
		cmocka_unit_test(test_percent_decode_failure),
		cmocka_unit_test(test_percent_decode),
		cmocka_unit_test(test_not_a_uri),
		cmocka_unit_test(test_hostname_only),
		cmocka_unit_test(test_username),
		cmocka_unit_test(test_username_and_password),
		cmocka_unit_test(test_port),
		cmocka_unit_test(test_user_pass_port_hostname),
		cmocka_unit_test(test_uri_percent_decode),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
