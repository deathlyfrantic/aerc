#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termbox.h>
#include <string.h>
#include <ctype.h>

#include "util/stringop.h"
#include "util/list.h"
#include "commands.h"
#include "config.h"
#include "colors.h"
#include "state.h"
#include "render.h"
#include "util/list.h"
#include "util/stringop.h"
#include "ui.h"

int frame = 0;

struct loading_indicator {
	int x, y;
};

list_t *loading_indicators = NULL;

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

	int l = 0;
	int _x = x, _y = y;
	char *b = buf;
	while (*b) {
		b += tb_utf8_char_to_unicode(&basis->ch, b);
		++l;
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
	return l;
}

void rerender() {
	free_flat_list(loading_indicators);
	loading_indicators = create_list();

	tb_clear();
	int width = tb_width(), height = tb_height();
	int folder_width = 20;

	render_account_bar(0, 0, width, folder_width);
	render_folder_list(0, 1, folder_width, height);
	render_status(folder_width, height - 1, width - folder_width);
	render_items(folder_width, 1, width - folder_width, height - 2);

	if (state->command.text) {
		tb_set_cursor(folder_width + strlen(state->command.text) + 1, height - 1);
	} else {
		tb_set_cursor(TB_HIDE_CURSOR, TB_HIDE_CURSOR);
	}
	tb_present();
}

void rerender_item(int index) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	struct aerc_mailbox *mailbox = get_aerc_mailbox(account, account->selected);
	if (index >= mailbox->messages->length) return;
	int folder_width = 20;
	int width = tb_width(), height = tb_height();
	int x = folder_width, y = mailbox->messages->length - account->ui.list_offset - index;
	struct aerc_message *message = mailbox->messages->items[index];
	if (!message) return;
	int selected = mailbox->messages->length - account->ui.selected_message - 1;
	// TODO: Consider scrolling
	for (int i = 0; i < loading_indicators->length; ++i) {
		struct loading_indicator *indic = loading_indicators->items[i];
		if (indic->x == x && indic->y == y) {
			list_del(loading_indicators, i);
			break;
		}
	}
	render_item(x, y, width - folder_width, height - 2, message, selected == index);
	tb_present();
}

static void render_loading(int x, int y) {
	struct tb_cell cell;
	if (config->ui.loading_frames->length == 0) {
		return;
	}
	get_color("loading-indicator", &cell);
	int f = frame / 8 % config->ui.loading_frames->length;
	tb_printf(x, y, &cell, "%s   ",
			(const char *)config->ui.loading_frames->items[f]);
}

void add_loading(int x, int y) {
	struct loading_indicator *indic = calloc(1, sizeof(struct loading_indicator));
	indic->x = x;
	indic->y = y;
	list_add(loading_indicators, indic);
	render_loading(x, y);
}

static void command_input(uint16_t ch) {
	int size = tb_utf8_char_length(ch);
	int len = strlen(state->command.text);
	if (state->command.length < len + size + 1) {
		state->command.text = realloc(state->command.text,
				state->command.length + 1024);
		state->command.length += 1024;
	}
	memcpy(state->command.text + len, &ch, size);
	state->command.text[len + size] = '\0';
	rerender();
}

static void abort_command() {
	free(state->command.text);
	state->command.text = NULL;
	rerender();
}

static void command_backspace() {
	int len = strlen(state->command.text);
	if (len == 0) {
		return;
	}
	state->command.text[len - 1] = '\0';
	rerender();
}

static void command_delete_word() {
	int len = strlen(state->command.text);
	if (len == 0) {
		return;
	}
	char *cmd = state->command.text + len - 1;
	if (isspace(*cmd)) --cmd;
	while (cmd != state->command.text && !isspace(*cmd)) --cmd;
	if (cmd != state->command.text) ++cmd;
	*cmd = '\0';
	rerender();
}

bool ui_tick() {
	if (loading_indicators->length > 1) {
		frame++;
		for (int i = 0; i < loading_indicators->length; ++i) {
			struct loading_indicator *indic = loading_indicators->items[i];
			render_loading(indic->x, indic->y);
		}
		tb_present();
	}

	struct tb_event event;
	switch (tb_peek_event(&event, 0)) {
	case TB_EVENT_RESIZE:
		rerender();
		break;
	case TB_EVENT_KEY:
		if (state->command.text) {
			switch (event.key) {
			case TB_KEY_ESC:
				abort_command();
				break;
			case TB_KEY_BACKSPACE:
			case TB_KEY_BACKSPACE2:
				command_backspace();
				break;
			case TB_KEY_CTRL_W:
				command_delete_word();
				break;
			case TB_KEY_SPACE:
				command_input(' ');
				break;
			case TB_KEY_TAB:
				command_input('\t');
				break;
			case TB_KEY_ENTER:
				handle_command(state->command.text);
				abort_command();
				break;
			default:
				if (event.ch && !event.mod) {
					command_input(event.ch);
				}
				break;
			}
		} else {
			if (event.ch == ':' && !event.mod) {
				state->command.text = malloc(1024);
				state->command.text[0] = '\0';
				state->command.length = 1024;
				state->command.index = 0;
				state->command.scroll = 0;
				rerender();
			}
			// Temporary:
			if (event.ch == 'q' || event.key == TB_KEY_CTRL_C) {
				return false;
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
			// /temporary
		}
		if (event.key == TB_KEY_CTRL_L) {
			rerender();
		}
		// TODO: Handle other keys
		break;
	case -1:
		return false;
	}
	return !state->exit;
}
