#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include "util/list.h"
#include "state.h"
#include "ui.h"

void set_status(struct account_state *account, enum account_status state,
        const char *text) {
	free(account->status.text);
	if (text == NULL) {
		text = "Unknown error occured";
	}
	account->status.text = strdup(text);
	account->status.status = state;
	clock_gettime(CLOCK_MONOTONIC, &account->status.since);
	rerender(); // TODO: just rerender the status bar
}
