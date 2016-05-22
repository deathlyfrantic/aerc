#ifndef _COLORS_H
#define _COLORS_H

struct tb_cell;

void set_color(const char *name, const char *value);
void get_color(const char *name, struct tb_cell *cell);
void colors_init();

#endif
