#include <termbox.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "state.h"
#include "ui.h"

int width, height;

void init_ui() {
	tb_init();
	tb_select_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);
}

void teardown_ui() {
	tb_shutdown();
}

int tb_printf(int x, int y, struct tb_cell *basis, const char *fmt, ...) {
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
		int l = tb_utf8_char_to_unicode(&basis->ch, b);
		b += l;
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

void rerender() {
	tb_clear();
	width = tb_width(), height = tb_height();
	/*
	 * Render header bar
	 */
	struct tb_cell cell = { .fg = TB_BLACK, .bg = TB_WHITE };
	tb_printf(0, 0, &cell, "  aerc  ");
	int x = sizeof("  aerc  ") - 1;
	for (int i = 0; i < state->accounts->length; ++i) {
		struct account_state *account = state->accounts->items[i];
		if (i == state->selected_account) {
			cell.fg = TB_WHITE; cell.bg = TB_DEFAULT;
		} else {
			cell.fg = TB_BLACK; cell.bg = TB_WHITE;
		}
		x += tb_printf(x, 0, &cell, " %s ", account->name);
	}
	cell.fg = TB_BLACK; cell.bg = TB_WHITE;
	while (x < width) tb_put_cell(x++, 0, &cell);
	/*
	 * Present.
	 */
	tb_present();
}

bool ui_tick() {
	struct tb_event event;
	switch (tb_peek_event(&event, 0)) {
	case TB_EVENT_RESIZE:
		rerender();
		break;
	case TB_EVENT_KEY:
		if (event.key == TB_KEY_ESC) {
			return false;
		}
		// TODO: Handle other keys
		break;
	case -1:
		return false;
	}
	return true;
}
