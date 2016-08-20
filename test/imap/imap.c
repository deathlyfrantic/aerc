#include <stdlib.h>
#include <string.h>
#include "tests.h"
#include "internal/imap.h"
#include "imap/imap.h"

extern void imap_init(struct imap_connection *imap);
extern int handle_line(struct imap_connection *imap, imap_arg_t *arg);

int handler_called = 0;

static void test_handle_line_unknown_handler(void **state) {
	int _;
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);

	imap_arg_t *arg = malloc(sizeof(imap_arg_t));
	imap_parse_args("tag FOOBAR", arg, &_);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, NULL);

	handle_line(imap, arg);

	assert_int_equal(handler_called, 0);

	free(arg);
	imap_close(imap);
}

static void test_handler(struct imap_connection *imap,
	const char *token, const char *cmd, imap_arg_t *args) {
	handler_called++;
	assert_true(strncmp(token, "a00", 3) == 0);
	assert_true(strncmp(cmd, "FOOBA", 5) == 0);
}

static void test_handle_line_known_handler(void **state) {
	int _;
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);

	imap_arg_t *arg = malloc(sizeof(imap_arg_t));
	imap_parse_args("a001 FOOBAR", arg, &_);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);

	handle_line(imap, arg);

	assert_int_equal(handler_called, 1);

	free(arg);
	imap_close(imap);
}

static void test_imap_receive_simple(void **state) {
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);
	imap->mode = RECV_LINE;

	const char *buffer = "a001 FOOBAR\r\n";
	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);

	set_ab_recv_result((void *)buffer, strlen(buffer));
	will_return(__wrap_ab_recv, strlen(buffer));

	will_return(__wrap_poll, 0);
	imap->poll[0].revents = POLLIN;

	imap_receive(imap);

	assert_int_equal(handler_called, 1);

	imap_close(imap);
}

static void test_imap_receive_multiple_lines(void **state) {
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);
	imap->mode = RECV_LINE;

	const char *buffer = "a001 FOOBAR\r\na002 FOOBAZ\r\n";

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);

	expect_string(__wrap_hashtable_get, key, "FOOBAZ");
	will_return(__wrap_hashtable_get, test_handler);

	set_ab_recv_result((void *)buffer, strlen(buffer));
	will_return(__wrap_ab_recv, strlen(buffer));

	will_return(__wrap_poll, 0);
	imap->poll[0].revents = POLLIN;

	imap_receive(imap);

	assert_int_equal(handler_called, 2);

	imap_close(imap);
}

static void test_imap_receive_partial_line(void **state) {
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);
	imap->mode = RECV_LINE;

	const char *buffer = "a001 FOOB";

	set_ab_recv_result((void *)buffer, strlen(buffer));
	will_return(__wrap_ab_recv, strlen(buffer));

	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	imap->poll[0].revents = POLLIN;

	imap_receive(imap);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);

	const char *remaining_buffer = "AR\r\n";

	set_ab_recv_result((void *)remaining_buffer, strlen(remaining_buffer));
	will_return(__wrap_ab_recv, strlen(remaining_buffer));

	imap_receive(imap);

	assert_int_equal(handler_called, 1);

	imap_close(imap);
}

static void test_imap_receive_multi_partial_line(void **state) {
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);
	imap->mode = RECV_LINE;

	const char *buffer = "a001 FOOB";

	set_ab_recv_result((void *)buffer, strlen(buffer));
	will_return(__wrap_ab_recv, strlen(buffer));

	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	imap->poll[0].revents = POLLIN;

	imap_receive(imap);

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);
	expect_string(__wrap_hashtable_get, key, "FOOBAZ");
	will_return(__wrap_hashtable_get, test_handler);

	const char *remaining_buffer = "AR\r\na002 FOOB";

	set_ab_recv_result((void *)remaining_buffer, strlen(remaining_buffer));
	will_return(__wrap_ab_recv, strlen(remaining_buffer));

	imap_receive(imap);

	assert_int_equal(handler_called, 1);

	const char *remaining_buffer_2 = "AZ\r\n";

	set_ab_recv_result((void *)remaining_buffer_2, strlen(remaining_buffer_2));
	will_return(__wrap_ab_recv, strlen(remaining_buffer_2));

	imap_receive(imap);

	assert_int_equal(handler_called, 2);

	imap_close(imap);
}

static void test_imap_receive_full_buffer(void **state) {
	struct imap_connection *imap = malloc(sizeof(struct imap_connection));
	imap_init(imap);
	imap->mode = RECV_LINE;

	char buffer[4096];
	memset(buffer, 'a', 4096);

	const char *cmd_1 =  "a001 FOOBAR ";
	memcpy(buffer, cmd_1, strlen(cmd_1));
	const char *cmd_2 =  "\r\na002 FOOBAZ ";
	memcpy(buffer + 2048 + 128, cmd_2, strlen(cmd_2));
	buffer[4094] = '\r';
	buffer[4095] = '\n';

	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	will_return(__wrap_poll, 0);
	imap->poll[0].revents = POLLIN;

	set_ab_recv_result((void *)buffer, 1024);
	will_return(__wrap_ab_recv, 1024);
	imap_receive(imap); // First command (incomplete)

	set_ab_recv_result((void *)(buffer + 1024), 1024);
	will_return(__wrap_ab_recv, 1024);
	imap_receive(imap); // First command (incomplete)

	expect_string(__wrap_hashtable_get, key, "FOOBAR");
	will_return(__wrap_hashtable_get, test_handler);
	set_ab_recv_result((void *)(buffer + 2048), 1024);
	will_return(__wrap_ab_recv, 1024);
	imap_receive(imap); // First command (complete), second command (incomplete)

	assert_int_equal(handler_called, 1);

	expect_string(__wrap_hashtable_get, key, "FOOBAZ");
	will_return(__wrap_hashtable_get, test_handler);
	set_ab_recv_result((void *)(buffer + 2048), 1024);
	will_return(__wrap_ab_recv, 1024);
	imap_receive(imap); // Second command (complete)

	assert_int_equal(handler_called, 2);

	imap_close(imap);
}

static int setup(void **state) {
	handler_called = 0;
	return 0;
}

int run_tests_imap() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_handle_line_unknown_handler, setup),
		cmocka_unit_test_setup(test_handle_line_known_handler, setup),
		cmocka_unit_test_setup(test_imap_receive_simple, setup),
		cmocka_unit_test_setup(test_imap_receive_multiple_lines, setup),
		cmocka_unit_test_setup(test_imap_receive_partial_line, setup),
		cmocka_unit_test_setup(test_imap_receive_multi_partial_line, setup),
		cmocka_unit_test_setup(test_imap_receive_full_buffer, setup),
	};
	return cmocka_run_group_tests(tests, setup, NULL);
}
