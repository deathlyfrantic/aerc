#include <termbox.h>
#include <string.h>
#include "util/list.h"
#include "state.h"
#include "ui.h"
#include "render.h"

void render_account_bar(int x, int y, int width, int folder_width) {
	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };

	/* Render folder list header */
	const char *aerc = "┤aerc├"; // 6 chars
	int sides = (folder_width - 6) / 2;
	for (int _x = 0; _x < sides; ++_x) {
		tb_printf(x++, y, &cell, "─");
	}
	tb_printf(x, y, &cell, aerc); x += 6;
	for (int _x = 0; _x < sides - 1; ++_x) {
		tb_printf(x++, y, &cell, "─");
	}
	tb_printf(x++, y, &cell, "┐");

	/* Render account tabs */
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
		x += tb_printf(x, 0, &cell, " %s ", account->name);
	}
	cell.fg = TB_BLACK; cell.bg = TB_WHITE;

	while (x < width) tb_put_cell(x++, y, &cell);
}

static int compare_mailboxes(const void *_a, const void *_b) {
	const struct aerc_mailbox *a = *(void **)_a;
	const struct aerc_mailbox *b = *(void **)_b;
	return strcmp(a->name, b->name);
}

void render_folder_list(int x, int y, int width, int height) {
	struct account_state *account =
		state->accounts->items[state->selected_account];

	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };
	int _x = x, _y = y;
	_x += width - 1;
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
	} else {
		tb_printf(x, y, &cell, "....");
	}
}

void render_status(int x, int y, int width) {
	struct account_state *account =
		state->accounts->items[state->selected_account];
	if (!account->status.text) return;

	struct tb_cell cell = { .fg = TB_BLACK, .bg = TB_WHITE };
	cell.ch = ' ';
	if (account->status.status == ACCOUNT_ERROR) {
		cell.bg = TB_RED;
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
	struct tb_cell cell = { .fg = TB_DEFAULT, .bg = TB_DEFAULT };
	tb_printf(x, y, &cell, " ....");
}
