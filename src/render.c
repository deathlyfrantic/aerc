#include <string.h>
#include <termbox.h>
#include <time.h>

#include "colors.h"
#include "config.h"
#include "state.h"
#include "ui.h"
#include "util/list.h"
#include "worker.h"

static void clear_remaining(struct tb_cell *cell, int x, int y, int width, int height) {
	cell->ch = ' ';
	for (int _y = 0; _y < height; ++_y) {
		for (int _x = 0; _x < width; ++_x) {
			tb_put_cell(x + _x, y + _y, cell);
		}
	}
}

void render_account_bar(int x, int y, int width, int folder_width) {
	struct tb_cell cell;

	/* Render folder list header */
	get_color("borders", &cell);
	const char *aerc = "aerc"; // 4 chars
	int sides = (folder_width - 4) / 2;
	for (int _x = 0; _x < sides; ++_x) {
		tb_printf(x++, y, &cell, " ");
	}
	tb_printf(x, y, &cell, aerc); x += 4;
	for (int _x = 0; _x < sides - 1; ++_x) {
		tb_printf(x++, y, &cell, " ");
	}
	tb_printf(x, y, &cell, " "); x += 1;

	/* Render account tabs */
	for (int i = 0; i < state->accounts->length; ++i) {
		struct account_state *account = state->accounts->items[i];
		if (i == state->selected_account) {
			get_color("account-selected", &cell);
		} else {
			get_color("account-unselected", &cell);
			if (account->status.status == ACCOUNT_ERROR) {
				get_color("account-error", &cell);
			}
		}
		x += tb_printf(x, 0, &cell, " %s ", account->name);
	}
	get_color("borders", &cell);
	clear_remaining(&cell, x, y, width, 1);
}

static int compare_mailboxes(const void *_a, const void *_b) {
	const struct aerc_mailbox *a = *(void **)_a;
	const struct aerc_mailbox *b = *(void **)_b;
	return strcmp(a->name, b->name);
}

void render_folder_list(int x, int y, int width, int height) {
	struct account_state *account =
		state->accounts->items[state->selected_account];

	struct tb_cell cell;
	get_color("borders", &cell);
	int _x = x, _y = y;
	_x += width - 1;
	for (; _y < height; ++_y) {
		cell.ch = ' ';
		tb_put_cell(_x, _y, &cell);
	}

	_x = x, _y = y;
	if (account->mailboxes) {
		list_qsort(account->mailboxes, compare_mailboxes);
		for (int i = 0; y < height && i < account->mailboxes->length; ++i, ++y) {
			struct aerc_mailbox *mailbox = account->mailboxes->items[i];
			if (strcmp(mailbox->name, account->selected) == 0) {
				get_color("folder-selected", &cell);
			} else {
				get_color("folder-unselected", &cell);
			}
			char c = '\0';
			// TODO: utf-8 strlen
			// TODO: decode mailbox names according to spec
			if ((int)strlen(mailbox->name) > width - 1) {
				mailbox->name[width - 1] = '\0';
			}
			int l = tb_printf(x, y, &cell, "%s", mailbox->name);
			if (c != '\0') {
				mailbox->name[width - 1] = c;
			}
			cell.ch = ' ';
			while (l < width - 1) {
				tb_put_cell(x + l, y, &cell);
				l++;
			}
		}
		x = _x; ++y;
	} else {
		add_loading(x, y);
		x = _x;
	}
	get_color("folder-unselected", &cell);
	clear_remaining(&cell, x, y, width - 1, height);
}

static void render_command(int x, int y, int width) {
	struct tb_cell cell;
	get_color("ex-line", &cell);
	cell.ch = ' ';
	for (int _x = 0; _x < width; ++_x) {
		tb_put_cell(x + _x, y, &cell);
	}
	tb_printf(x, y, &cell, ":%s", state->command.text);
}

void render_status(int x, int y, int width) {
	if (state->command.text != NULL) {
		render_command(x, y, width);
		return;
	}

	struct account_state *account =
		state->accounts->items[state->selected_account];
	if (!account->status.text) return;

	struct tb_cell cell;
	get_color("status-line", &cell);
	cell.ch = ' ';
	if (account->status.status == ACCOUNT_ERROR) {
		get_color("status-line-error", &cell);
	}
	for (int _x = 0; _x < width; ++_x) {
		tb_put_cell(x + _x, y, &cell);
	}
	if (account->status.status == ACCOUNT_OKAY) {
		tb_printf(x, y, &cell, "%s -- %s",
				account->selected,
				account->status.text);
	} else {
		tb_printf(x, y, &cell, "%s", account->status.text);
	}
}

void render_items(int x, int y, int width, int height) {
	struct tb_cell cell;
	get_color("message-list-unselected", &cell);
	clear_remaining(&cell, x, y, width, height);

	struct account_state *account =
		state->accounts->items[state->selected_account];
	struct aerc_mailbox *mailbox = get_aerc_mailbox(account, account->selected);

	if (!mailbox || !mailbox->messages) {
		add_loading(x + width / 2, y);
		return;
	}

	for (int i = account->ui.list_offset;
			i < mailbox->messages->length && y < height;
			++i, ++y) {
		struct aerc_message *message = mailbox->messages->items[i];
		if (account->ui.selected_message == i) {
			get_color("message-list-selected", &cell);
		} else {
			get_color("message-list-unselected", &cell);
		}
		if (!message) {
			add_loading(x, y);
		} else {
			char date[64];
			strftime(date, sizeof(date), config->ui.timestamp_format,
					message->internal_date);
			const char *subject = get_message_header(message, "Subject");
			int l = tb_printf(x, y, &cell, "%s %s", date, subject);
			clear_remaining(&cell, x + l, y, width - l, 1);
		}
	}
}
