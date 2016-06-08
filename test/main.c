#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include "tests.h"
#include "state.h"

struct aerc_state *state;

int main(int argc, char **argv) {
	int ret = 0;

	// TODO: Run only specific tests etc
	ret += run_tests_urlparse();
	ret += run_tests_imap();

	return ret;
}
