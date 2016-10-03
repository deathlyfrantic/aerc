#ifndef BIND_H
#define BIND_H

#include <termbox.h>

#include "util/list.h"

enum bind_result {
	BIND_SUCCESS,
	BIND_INVALID_KEYS,
	BIND_INVALID_COMMAND,
	BIND_CONFLICTS,
};

struct bind {
	list_t *keys; //list of the keys pressed so far
	void *binds;  //implementation
};

void init_bind(struct bind* bind);
void destroy_bind(struct bind* bind);

//Bind the given key combination to the given command
enum bind_result bind_add(struct bind* bind, const char* keys, const char* command);

//Handle a key-press event. If it results in a command, that command is
//returned, otherwise NULL is returned.
const char* bind_handle_key_event(struct bind* bind, struct tb_event* event);

//Return a string representing the state of the input buffer
char* bind_input_buffer(struct bind* bind);

//returns a string representing the key event, or NULL
char* bind_translate_key_event(struct tb_event* event);

//return an event representing the key name, or NULL
struct tb_event* bind_translate_key_name(const char* key);

#endif
