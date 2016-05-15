#include <termbox.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "state.h"
#include "ui.h"

int width, height;
int x, y;

void init_ui() {
	tb_init();
	tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
}

void teardown_ui() {
	tb_shutdown();
}

int tb_printf(struct tb_cell *basis, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *buf = malloc(len + 1);
	va_start(args, fmt);
	vsnprintf(buf, len + 1, fmt, args);
	va_end(args);

	int _x = x, _y = y;
	char *b = buf;
	while (*b) {
		if (_x >= width) {
			/* Wrap */
			_x = x;
			_y++;
		}
		b += tb_utf8_char_to_unicode(&basis->ch, b);
		switch (basis->ch) {
		case '\n':
			_x = x;
			_y++;
			break;
		case '\r':
			_x = x;
			break;
		default:
			tb_put_cell(_x, _y, basis);
			_x++;
			break;
		}
	}

	free(buf);
	return len;
}

void render_account_bar() {
	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };
	tb_printf(&cell, "───────┤aerc├──────┐");
	x += sizeof("        aerc        ") - 1;
	for (int i = 0; i < state->accounts->length; ++i) {
		struct account_state *account = state->accounts->items[i];
		if (i == state->selected_account) {
			cell.fg = TB_DEFAULT; cell.bg = TB_DEFAULT;
		} else {
			cell.fg = TB_BLACK; cell.bg = TB_WHITE;
			if (account->status.status == ACCOUNT_ERROR) {
				cell.bg = TB_RED;
			}
		}
		x += tb_printf(&cell, " %s ", account->name);
	}
	cell.fg = TB_BLACK; cell.bg = TB_WHITE;
	while (x < width) tb_put_cell(x++, y, &cell);
	x = 0; y++;
}

static int compare_mailboxes(const void *_a, const void *_b) {
	const struct aerc_mailbox *a = *(void **)_a;
	const struct aerc_mailbox *b = *(void **)_b;
	return strcmp(a->name, b->name);
}

void render_folder_list() {
	struct account_state *account =
		state->accounts->items[state->selected_account];

	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };
	int _x = x, _y = y;
	_x += 19; // TODO: Configurable
	for (; _y < height; ++_y) {
		cell.ch = u'│';
		tb_put_cell(_x, _y, &cell);
	}
	_x = x, _y = y;
	if (account->mailboxes) {
		list_qsort(account->mailboxes, compare_mailboxes);
		for (int i = 0; y < height && i < account->mailboxes->length; ++i, ++y) {
			struct aerc_mailbox *mailbox = account->mailboxes->items[i];
			if (strcmp(mailbox->name, account->selected) == 0) {
				cell.fg = TB_BLACK; cell.bg = TB_WHITE;
			} else {
				cell.fg = TB_DEFAULT; cell.bg = TB_DEFAULT;
			}
			char c = '\0';
			// TODO: utf-8 strlen
			// TODO: decode mailbox names according to spec
			if (strlen(mailbox->name) > 19) {
				mailbox->name[19] = '\0';
			}
			int l = tb_printf(&cell, "%s", mailbox->name);
			if (c != '\0') {
				mailbox->name[19] = c;
			}
			cell.ch = ' ';
			while (l < 19) {
				tb_put_cell(x + l, y, &cell);
				l++;
			}
		}
	} else {
		tb_printf(&cell, "        ....");
	}
	x += 20;
	y = _y;
}

void render_status() {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	if (!account->status.text) return;

	struct tb_cell cell = { .fg = TB_BLACK, .bg = TB_WHITE };
	if (account->status.status == ACCOUNT_ERROR) {
		cell.bg = TB_RED;
	}
	int _x = x, _y = y;
	y = height - 1;
	tb_printf(&cell, "%s", account->status.text);
	x += strlen(account->status.text);
	cell.ch = ' ';
	while (x < width) tb_put_cell(x++, y, &cell);
	y = _y; x = _x;
}

void render_items() {
	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };
	tb_printf(&cell, " ....");
}

void rerender() {
	tb_clear();
	width = tb_width(), height = tb_height();
	x = 0, y = 0;

	render_account_bar();
	render_folder_list();
	render_status();
	render_items();

	tb_present();
}

bool ui_tick() {
	struct tb_event event;
	switch (tb_peek_event(&event, 0)) {
	case TB_EVENT_RESIZE:
		rerender();
		break;
	case TB_EVENT_KEY:
		if (event.key == TB_KEY_ESC
				|| event.ch == 'q'
				|| event.key == TB_KEY_CTRL_C) {
			return false;
		}
		if (event.key == TB_KEY_CTRL_L) {
			rerender();
		}
		if (event.key == TB_KEY_ARROW_RIGHT) {
			state->selected_account++;
			state->selected_account %= state->accounts->length;
			rerender();
		}
		if (event.key == TB_KEY_ARROW_LEFT) {
			state->selected_account--;
			state->selected_account %= state->accounts->length;
			rerender();
		}
		// TODO: Handle other keys
		break;
	case -1:
		return false;
	}
	return true;
}
