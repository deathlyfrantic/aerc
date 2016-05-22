#ifndef _UI_H
#define _UI_H

#include <stdbool.h>

#include "termbox.h"

void init_ui();
void teardown_ui();
void rerender();
bool ui_tick();
int tb_printf(int x, int y, struct tb_cell *basis, const char *fmt, ...);
void add_loading(int x, int y);

#endif
