#include <malloc.h>
#include <string.h>

#include "tests.h"
#include "bind.h"

static struct tb_event generate_event(int ch, int mod, int key) {
	struct tb_event e;
	memset(&e, 0, sizeof e);
	e.ch = ch;
	e.mod = mod;
	e.key = key;
	return e;
}

static void test_reject_invalid_command(void **state) {
	struct bind bind;
	init_bind(&bind);

	assert_int_equal(BIND_INVALID_COMMAND, bind_add(&bind, "Ctrl+n", NULL));

	destroy_bind(&bind);
}

static void test_reject_invalid_keys(void **state) {
	struct bind bind;
	init_bind(&bind);

	static const char *keys[] = {
		"Ctrl+=",
		"foobar",
		"=",
		"",
		"Shift+W",
		"Shift+Eq",
		"Ctrl+Ctrl+x",
	};

	for (size_t i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
		assert_int_equal(BIND_INVALID_KEYS, bind_add(&bind, keys[i], "command"));
	}

	destroy_bind(&bind);
}

static void test_accept_valid_keys(void **state) {
	struct bind bind;
	init_bind(&bind);

	static const char *keys[] = {
		"Ctrl+q",
		"Ctrl+E",
		"Meta+!",
		"Space",
		"Ctrl++",
		"Ctrl+Eq",
		"# 2",
		"Ctrl+x s",
		"Ctrl+x z",
		"s",
		"W",
		"Ctrl+Meta+Delete",
		";",
		"F1",
		"Meta+Backspace",
		"Shift+Left",
	};

	for (size_t i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
		assert_int_equal(BIND_SUCCESS, bind_add(&bind, keys[i], "command"));
	}

	destroy_bind(&bind);
}

static void test_reject_conflicting_keys(void **state) {
	struct bind bind;
	init_bind(&bind);

	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "a b", "command"));

	//Conflict if it's a prefix of an existing bind
	assert_int_equal(BIND_CONFLICTS, bind_add(&bind, "a", "command"));

	//Conflict if it's an extension of an existing bind
	assert_int_equal(BIND_CONFLICTS, bind_add(&bind, "a b a", "command"));

	destroy_bind(&bind);
}

static void test_translate_key_event(void **state) {
	struct {
		const char *str;
		char ch;
		int mod;
		int key;
	} pairs[] = {
		{"a", 'a', 0, 0},
		{"R", 'R', 0, 0},
		{"9", '9', 0, 0},
		{"Ctrl+a", 0, 0, TB_KEY_CTRL_A},
		{"Left", 0, 0, TB_KEY_ARROW_LEFT},
		{"Meta+q", 'q', TB_MOD_ALT, 0},
		{"Meta+Right", 0, TB_MOD_ALT, TB_KEY_ARROW_RIGHT},
		{"Meta+Eq", '=', TB_MOD_ALT, 0},
		{"Space", ' ', 0, 0},
		{"Meta+Space", ' ', TB_MOD_ALT, 0},
	};

	struct tb_event e;
	memset(&e, 0, sizeof e);
	for(size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i) {
		e = generate_event(pairs[i].ch, pairs[i].mod, pairs[i].key);
		char *str = bind_translate_key_event(&e);
		assert_string_equal(str, pairs[i].str);
		free(str);
	}
}

static void test_translate_key_name(void **state) {
	struct {
		const char *str;
		char ch;
		int mod;
		int key;
	} pairs[] = {
		{"Ctrl+a", 0, 0, TB_KEY_CTRL_A},
		{"Left", 0, 0, TB_KEY_ARROW_LEFT},
		{"Meta+Right", 0, TB_MOD_ALT, TB_KEY_ARROW_RIGHT},
		{"Meta+j", 'j', TB_MOD_ALT, 0},
	};

	struct tb_event e;
	memset(&e, 0, sizeof e);
	for(size_t i = 0; i < sizeof pairs / sizeof pairs[0]; ++i) {
		/* e = generate_event(pairs[i].ch, pairs[i].mod, pairs[i].key); */
		struct tb_event* event = bind_translate_key_name(pairs[i].str);
		assert_non_null(event);

		//Check the parts of the event we care about
		assert_int_equal(event->ch, pairs[i].ch);
		assert_int_equal(event->mod, pairs[i].mod);
		assert_int_equal(event->key, pairs[i].key);

		free(event);
	}
}

static void test_get_input_buffer(void **state) {
	struct bind bind;
	init_bind(&bind);

	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "a b c", "alpha beta"));
	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "a b d", "beta alpha"));

	struct tb_event e;
	e = generate_event('a', 0, 0);
	bind_handle_key_event(&bind, &e);
	e = generate_event('b', 0, 0);
	bind_handle_key_event(&bind, &e);

	char *input = bind_input_buffer(&bind);
	assert_string_equal("a b", input);
	free(input);

	destroy_bind(&bind);
}

static void test_remember_binds(void **state) {
	struct bind bind;
	init_bind(&bind);

	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "a b", "alpha beta"));
	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "b a", "beta alpha"));
	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "Ctrl+x s", "save"));
	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "Meta+q", "quiet"));
	assert_int_equal(BIND_SUCCESS, bind_add(&bind, "Ctrl+x o", "open"));

	struct tb_event e;

	//Test perfect input
	e = generate_event('a', 0, 0);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));
	e = generate_event('b', 0, 0);
	assert_string_equal("alpha beta", bind_handle_key_event(&bind, &e));

	//Test bad input retrieves nothing
	e = generate_event('c', 0, 0);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));

	//Test after bad input the buffer is clear for good input
	e = generate_event(0, 0, TB_KEY_CTRL_X);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));
	e = generate_event('s', 0, 0);
	assert_string_equal("save", bind_handle_key_event(&bind, &e));

	//Test hitting ESC clears the input state
	e = generate_event('a', 0, 0);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));
	e = generate_event(0, 0, TB_KEY_ESC);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));
	e = generate_event('b', 0, 0);
	assert_int_equal(NULL, bind_handle_key_event(&bind, &e));

	destroy_bind(&bind);
}

int run_tests_bind() {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_reject_invalid_command),
		cmocka_unit_test(test_reject_invalid_keys),
		cmocka_unit_test(test_accept_valid_keys),
		cmocka_unit_test(test_reject_conflicting_keys),
		cmocka_unit_test(test_translate_key_event),
		cmocka_unit_test(test_translate_key_name),
		cmocka_unit_test(test_get_input_buffer),
		cmocka_unit_test(test_remember_binds),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
