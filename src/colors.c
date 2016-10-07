#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <termbox.h>

#include "colors.h"
#include "util/stringop.h"
#include "util/hashtable.h"
#include "stdlib.h"

hashtable_t *colors;

void colors_init() {
	colors = create_hashtable(50, hash_string);

	set_color("borders", "white:black");
	set_color("loading-indicator", "default:default");

	set_color("account-unselected", "white:black");
	set_color("account-selected", "default:default");
	set_color("account-error", "red:black");

	set_color("folder-unselected", "default:default");
	set_color("folder-selected", "white:black");

	set_color("status-line", "white:black");
	set_color("status-line-error", "red:black");

	set_color("ex-line", "default:default");

	set_color("message-list-selected", "white:black");
	set_color("message-list-unselected", "default:default");
	set_color("message-list-selected-unread", "white:_black");
	set_color("message-list-unselected-unread", "default:*default");
}

// default color names have to be decremeted one to
// work in 256-color mode in termbox
const struct {
	const char *name;
	uint16_t color;
} color_names[] = {
	{ "default", TB_DEFAULT },
	{ "black", TB_BLACK - 1 },
	{ "red", TB_RED - 1 },
	{ "green", TB_GREEN - 1 },
	{ "yellow", TB_YELLOW - 1 },
	{ "blue", TB_BLUE - 1 },
	{ "magenta", TB_MAGENTA - 1 },
	{ "cyan", TB_CYAN - 1 },
	{ "white", TB_WHITE - 1 }
};

static uint16_t match_color(const char *value) {
	uint16_t mods = 0;
	while (*value == '*' || *value == '_' || *value == '^') {
		if (*value == '*') {
			mods |= TB_BOLD;
		} else if (*value == '_') {
			mods |= TB_UNDERLINE;
		} else if (*value == '^') {
			mods |= TB_REVERSE;
		}
		++value;
	}

	// check for integer color specification
	char *endptr;
	long color_num = strtol(value, &endptr, 10);
	if (value != endptr && color_num < 256) {
		return color_num | mods;
	}

	for (size_t i = 0;
			i < sizeof(color_names) / (sizeof(void *) + sizeof(uint16_t));
			++i) {
		if (strncasecmp(color_names[i].name,
					value,
					strlen(color_names[i].name)) == 0) {
			return color_names[i].color | mods;
		}
	}
	return TB_DEFAULT | mods;
}

void set_color(const char *name, const char *value) {
	struct tb_cell *cell = calloc(1, sizeof(struct tb_cell));
	uint16_t c = match_color(value);
	if (strchr(value, ':')) {
		cell->bg = c;
		value = strchr(value, ':') + 1;
		c = match_color(value);
		cell->fg = c;
	} else {
		cell->bg = TB_DEFAULT;
		cell->fg = c;
	}
	hashtable_set(colors, name, cell);
}

void get_color(const char *name, struct tb_cell *cell) {
	struct tb_cell *c = hashtable_get(colors, name);
	if (!c) {
		cell->fg = TB_DEFAULT;
		cell->bg = TB_DEFAULT;
	} else {
		cell->fg = c->fg;
		cell->bg = c->bg;
	}
}
