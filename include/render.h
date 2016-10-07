#ifndef _RENDER_H
#define _RENDER_H

#include "state.h"

void render_account_bar(struct geometry geo);
void render_folder_list(struct geometry geo);
void render_status(struct geometry geo);
void render_items(struct geometry geo);
void render_item(struct geometry geo, struct aerc_message *message, bool selected);

#endif
