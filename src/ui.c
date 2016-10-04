#define _POSIX_C_SOURCE 201112LL

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

static void need_rerender() {
	state->rerender = true;
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

void rerender_item(size_t index) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	struct aerc_mailbox *mailbox = get_aerc_mailbox(account, account->selected);
	if (index >= mailbox->messages->length) return;
	int folder_width = 20;
	int width = tb_width(), height = tb_height();
	int x = folder_width, y = mailbox->messages->length - account->ui.list_offset - index;
	struct aerc_message *message = mailbox->messages->items[index];
	if (!message) return;
	size_t selected = mailbox->messages->length - account->ui.selected_message - 1;
	// TODO: Consider scrolling
	for (size_t i = 0; i < loading_indicators->length; ++i) {
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
	size_t size = tb_utf8_char_length(ch);
	size_t len = strlen(state->command.text);
	if (state->command.length < len + size + 1) {
		state->command.text = realloc(state->command.text,
				state->command.length + 1024);
		state->command.length += 1024;
	}
	memcpy(state->command.text + len, &ch, size);
	state->command.text[len + size] = '\0';
	need_rerender();
}

static void abort_command() {
	free(state->command.text);
	state->command.text = NULL;
	need_rerender();
}

static void command_backspace() {
	int len = strlen(state->command.text);
	if (len == 0) {
		return;
	}
	state->command.text[len - 1] = '\0';
	need_rerender();
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
	need_rerender();
}


static struct tb_event *parse_input_command(const char *input, size_t *consumed) {
	if (!input[0]) {
		return NULL;
	}

	if (input[0] == '<') {
		const char *term = strchr(input, '>');
		if (term) {
			char *buf = strdup(input + 1);
			*strchr(buf, '>') = 0;

			struct tb_event *e = bind_translate_key_name(buf);
			free(buf);
			if (e) {
				*consumed += 1 + term - input;
				return e;
			}
		}
	}

	struct tb_event *e = calloc(1, sizeof(struct tb_event));
	e->type = TB_EVENT_KEY;
	e->ch = input[0];
	*consumed += 1;
	return e;
}

static void process_event(struct tb_event* event, aqueue_t *event_queue) {
	switch (event->type) {
	case TB_EVENT_RESIZE:
		need_rerender();
		break;
	case TB_EVENT_KEY:
		if (state->command.text) {
			switch (event->key) {
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
				if (event->ch && !event->mod) {
					command_input(event->ch);
				}
				break;
			}
		} else {
			if (event->ch == ':' && !event->mod) {
				state->command.text = malloc(1024);
				state->command.text[0] = '\0';
				state->command.length = 1024;
				state->command.index = 0;
				state->command.scroll = 0;
			} else {
				const char* command = bind_handle_key_event(state->binds, event);
				if (command) {
					size_t consumed = 0;
					struct tb_event *new_event = NULL;

					while (1) {
						new_event = parse_input_command(command + consumed, &consumed);
						if (!new_event) {
							break;
						}
						aqueue_enqueue(event_queue, new_event);
					}
				}
			}
			need_rerender();
		}
		break;
	}
}

bool ui_tick() {
	if (loading_indicators->length > 1) {
		frame++;
		for (size_t i = 0; i < loading_indicators->length; ++i) {
			struct loading_indicator *indic = loading_indicators->items[i];
			render_loading(indic->x, indic->y);
		}
		tb_present();
	}

	aqueue_t *events = aqueue_new();

	while (1) {
		struct tb_event *event = malloc(sizeof(struct tb_event));
		// Fetch an event and enqueue it if we can
		if (tb_peek_event(event, 0) < 1 || !aqueue_enqueue(events, event)) {
			free(event);
			break;
		}
	}

	struct tb_event *event;
	while (aqueue_dequeue(events, (void**)&event)) {
		process_event(event, events);
		free(event);
		if(state->exit) {
			break;
		}
	}

	// If there's events in the queue still, it's because we're exiting, and we
	// need to clean up.
	while(aqueue_dequeue(events, (void**)&event)) {
		free(event);
	}
	aqueue_free(events);

	if (state->rerender) {
		state->rerender = false;
		rerender();
	}

	return !state->exit;
}
