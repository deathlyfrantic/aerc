#define _POSIX_C_SOURCE 201112LL

#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <termbox.h>

#include "bind.h"
#include "util/stringop.h"

// bind_node is a node in a trie of all the valid binds
struct bind_node {
	char* key; // the key combination of this node
	char* command; // if node is a leaf, the command, else NULL
	list_t* suffixes; // the children of this node
};

static const char* valid_prefixes[] = {"Ctrl+", "Meta+"};
static const char* valid_special_stubs[] = {
	"Space",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	"Left",
	"Right",
	"Up",
	"Down",
	"Enter",
	"Backspace",
	"PageUp",
	"PageDown",
	"Home",
	"End",
	"Tab",
	"Insert",
	"Delete",
};

static void init_bind_node(struct bind_node* bn)
{
	bn->key = NULL;
	bn->command = NULL;
	bn->suffixes = create_list();
}

static void destroy_bind_node(struct bind_node* bn)
{
	for(size_t i = 0; i < bn->suffixes->length; ++i)
		destroy_bind_node(bn->suffixes->items[i]);
	free(bn->key);
	free(bn->command);
	list_free(bn->suffixes);
}

static int is_valid_key(const char* key)
{
	//Advance past any valid prefixes like Ctrl+ or Meta+
	for(size_t i = 0;
	    i < sizeof valid_prefixes / sizeof valid_prefixes[0];
	    ++i) {
		if(is_prefix_of(valid_prefixes[i], key))
			key += strlen(valid_prefixes[i]);
	}

	//Check Shift+ as well, but if it's used, only special stubs may follow
	//to avoid Shift+W vs W tautology
	int must_be_special = 0;
	if(is_prefix_of("Shift+", key)) {
		key += strlen("Shift+");
		must_be_special = 1;
	}

	//Check if we're left with a valid regular key stub
	if(!must_be_special) {
		//Anything printable is valid, except '=' which can't go in a .ini file
		if(strlen(key) == 1 && isgraph(*key) && *key != '=')
			return 1;

		//Special case: 'Eq' as a alias for '='
		if(strcmp(key, "Eq") == 0)
			return 1;
	}

	//Maybe it's a special stub
	for(size_t i = 0;
		i < sizeof valid_special_stubs / sizeof valid_special_stubs[0];
		++i) {
		//Not one of our special stubs? Can only be invalid.
		if(strcmp(valid_special_stubs[i], key) == 0)
			return 1;
	}

	return 0;
}

static int is_valid_keys(const char* keys)
{
	if(!keys || strlen(keys) == 0)
		return 0;

	int valid = 1;
	list_t *parts = split_string(keys," ");

	for(size_t i = 0; i < parts->length; ++i)
	{
		if(!is_valid_key(parts->items[i])) {
			valid = 0;
			break;
		}
	}

	free_flat_list(parts);
	return valid;
}

void init_bind(struct bind* bind)
{
	bind->keys = create_list();
	bind->binds = malloc(sizeof(struct bind_node));
	init_bind_node(bind->binds);
}

void destroy_bind(struct bind* bind)
{
	destroy_bind_node(bind->binds);
	free(bind->binds);
}

static int compare_node_key(const struct bind_node* node, const char *key)
{
	return strcmp(node->key,key);
}

enum bind_result bind_add(struct bind* bind, const char* keys, const char* command)
{
	if(!command)
		return BIND_INVALID_COMMAND;

	if(!keys)
		return BIND_INVALID_KEYS;

	char* dirty_keys = strdup(keys);
	strip_quotes(dirty_keys);

	if(!is_valid_keys(keys)) {
		free(dirty_keys);
		return BIND_INVALID_KEYS;
	}

	// Split the key up into its individual parts
	list_t *parts = split_string(dirty_keys, " ");

	//Prepare our return code
	int result = BIND_SUCCESS;

	// Traverse the trie
	struct bind_node *node = bind->binds;
	for(size_t i = 0; i < parts->length; ++i) {
		//If we've reached a node that already has a command, there's a conflict
		if(node->command) {
			result = BIND_INVALID_COMMAND;
			break;
		}

		//Find / create a child with the current key
		struct bind_node *next_node = NULL;
		int child_index = list_seq_find(node->suffixes, (void*)&compare_node_key, parts->items[i]);
		if(child_index == -1) {
			//Create our new node
			next_node = malloc(sizeof(struct bind_node));
			init_bind_node(next_node);
			next_node->key = strdup(parts->items[i]);
			list_add(node->suffixes, next_node);
		} else {
			next_node = node->suffixes->items[child_index];
		}

		//We've now found the correct node for this key

		//Check if the node has a command
		if(next_node->command) {
			if(i + 1 < parts->length) {
				//If we're not at the end, it's a conflict
				result = BIND_CONFLICTS;
				break;
			} else {
				//Otherwise we just need to overwrite the existing bind.
				next_node->command = strdup(command);
				result = BIND_SUCCESS;
				break;
			}
		}

		if(i + 1 == parts->length) {
			//this is the last key part, try to insert command
			//but first, make sure there's no children
			if(next_node->suffixes->length > 0) {
				result = BIND_CONFLICTS;
			} else {
				next_node->command = strdup(command);
			}
		} else {
			//Otherwise, move down the trie
			node = next_node;
		}
	}

	free(dirty_keys);
	free_flat_list(parts);
	return result;
}

static int print_event(char* buf, size_t len, struct tb_event* event)
{
	const char* str = NULL;

	//Build prefix first:
	const char *prefix = event->mod & TB_MOD_ALT ? "Meta+" : "";

	//Try plain old character input
	if(event->ch) {
		//Alias '=' into Eq, and ' ' into Space
		if(event->ch == '=')
			return snprintf(buf, len, "%sEq", prefix);
		else if(event->ch == ' ')
			return snprintf(buf, len, "%sSpace", prefix);
		else
			return snprintf(buf, len, "%s%c", prefix, event->ch);
	}

	//Try the first set of special keys
	switch(event->key) {
		case TB_KEY_F1:          str = "F1";        break;
		case TB_KEY_F2:          str = "F2";        break;
		case TB_KEY_F3:          str = "F3";        break;
		case TB_KEY_F4:          str = "F4";        break;
		case TB_KEY_F5:          str = "F5";        break;
		case TB_KEY_F6:          str = "F6";        break;
		case TB_KEY_F7:          str = "F7";        break;
		case TB_KEY_F8:          str = "F8";        break;
		case TB_KEY_F9:          str = "F9";        break;
		case TB_KEY_F10:         str = "F10";       break;
		case TB_KEY_F11:         str = "F11";       break;
		case TB_KEY_F12:         str = "F12";       break;
		case TB_KEY_INSERT:      str = "Insert";    break;
		case TB_KEY_DELETE:      str = "Delete";    break;
		case TB_KEY_HOME:        str = "Home";      break;
		case TB_KEY_END:         str = "End";       break;
		case TB_KEY_PGUP:        str = "PageUp";    break;
		case TB_KEY_PGDN:        str = "PageDown";  break;
		case TB_KEY_ARROW_UP:    str = "Up";        break;
		case TB_KEY_ARROW_DOWN:  str = "Down";      break;
		case TB_KEY_ARROW_LEFT:  str = "Left";      break;
		case TB_KEY_ARROW_RIGHT: str = "Right";     break;
		case TB_KEY_SPACE:       str = "Space";     break;
		case TB_KEY_TAB:         str = "Tab";       break;
		case TB_KEY_BACKSPACE:
		case TB_KEY_BACKSPACE2:  str = "Backspace"; break;
		case TB_KEY_ENTER:       str = "Enter";     break;
	}

	//Was it one of the special keys?
	if(str) {
		return snprintf(buf, len, "%s%s", prefix, str);
	}

	//Other special, hardcoded cases
	switch(event->key) {
		case TB_KEY_CTRL_TILDE:       str = "Ctrl+~";  break;
		case TB_KEY_CTRL_A:           str = "Ctrl+a";  break;
		case TB_KEY_CTRL_B:           str = "Ctrl+b";  break;
		case TB_KEY_CTRL_C:           str = "Ctrl+c";  break;
		case TB_KEY_CTRL_D:           str = "Ctrl+d";  break;
		case TB_KEY_CTRL_E:           str = "Ctrl+e";  break;
		case TB_KEY_CTRL_F:           str = "Ctrl+f";  break;
		case TB_KEY_CTRL_G:           str = "Ctrl+g";  break;
		case TB_KEY_CTRL_H:           str = "Ctrl+h";  break;
		case TB_KEY_CTRL_I:           str = "Ctrl+i";  break;
		case TB_KEY_CTRL_J:           str = "Ctrl+j";  break;
		case TB_KEY_CTRL_K:           str = "Ctrl+k";  break;
		case TB_KEY_CTRL_L:           str = "Ctrl+l";  break;
		case TB_KEY_CTRL_M:           str = "Ctrl+m";  break;
		case TB_KEY_CTRL_N:           str = "Ctrl+n";  break;
		case TB_KEY_CTRL_O:           str = "Ctrl+o";  break;
		case TB_KEY_CTRL_P:           str = "Ctrl+p";  break;
		case TB_KEY_CTRL_Q:           str = "Ctrl+q";  break;
		case TB_KEY_CTRL_R:           str = "Ctrl+r";  break;
		case TB_KEY_CTRL_S:           str = "Ctrl+s";  break;
		case TB_KEY_CTRL_T:           str = "Ctrl+t";  break;
		case TB_KEY_CTRL_U:           str = "Ctrl+u";  break;
		case TB_KEY_CTRL_V:           str = "Ctrl+v";  break;
		case TB_KEY_CTRL_W:           str = "Ctrl+w";  break;
		case TB_KEY_CTRL_X:           str = "Ctrl+x";  break;
		case TB_KEY_CTRL_Y:           str = "Ctrl+y";  break;
		case TB_KEY_CTRL_Z:           str = "Ctrl+z";  break;
		case TB_KEY_CTRL_LSQ_BRACKET: str = "Ctrl+[";  break;
		case TB_KEY_CTRL_RSQ_BRACKET: str = "Ctrl+]";  break;
		case TB_KEY_CTRL_BACKSLASH:   str = "Ctrl+\\"; break;
		case TB_KEY_CTRL_SLASH:       str = "Ctrl+/";  break;
	}

	if(str) {
		return snprintf(buf, len, "%s", str);
	}

	//No idea
	buf[0] = 0;
	return 0;
}

enum lookup_result {
	LOOKUP_PARTIAL,
	LOOKUP_INVALID,
	LOOKUP_MATCH,
};

static enum lookup_result lookup_binding(struct bind_node* node, list_t* keys, const char** out_str)
{
	//Iterate through the list of inputs (e.g. Ctrl+a b)
	for(size_t part = 0; part < keys->length; ++part) {
		//Look for a child that matches the next input part (e.g. Ctrl+a)
		const char* cur_key = keys->items[part];
		int found = 0;
		for(size_t i = 0; i < node->suffixes->length; ++i) {
			struct bind_node* cur_node = node->suffixes->items[i];
			if(strcmp(cur_node->key, cur_key) == 0) {
				//We found a match, move down the trie to the next level
				node = node->suffixes->items[i];
				found = 1;
				break;
			}
		}
		// Did we reach this point without finding a match? Abort.
		if(!found)
			return LOOKUP_INVALID;
	}

	//We've reached the end, if node has a command, it's a match, if not, it's
	//a partial.
	if(node->command) {
		*out_str = node->command;
		return LOOKUP_MATCH;
	}
	return LOOKUP_PARTIAL;
}

char* bind_input_buffer(struct bind* bind)
{
	return join_list(bind->keys, " ");
}

static void clear_input_buffer(struct bind* bind)
{
		free_flat_list(bind->keys);
		bind->keys = create_list();
}

const char* bind_handle_key_event(struct bind* bind, struct tb_event* event)
{
	//If the user hit ESC, clear the input buffer and abort
	if(event->key == TB_KEY_ESC) {
		clear_input_buffer(bind);
		return NULL;
	}

	//Turn the key event into a representative string
	char buffer[128];
	print_event(buffer, sizeof buffer, event);

	//Add it to our input list
	list_add(bind->keys, strdup(buffer));

	//Check if it means anything
	const char* command = NULL;
	enum lookup_result result = lookup_binding(bind->binds, bind->keys, &command);
	if(result == LOOKUP_PARTIAL) {
		//We need more input.
		return NULL;
	} else if(result == LOOKUP_MATCH) {
		//We found a command. Clear our input buffer, and return it.
		clear_input_buffer(bind);
		return command;
	} else if(result == LOOKUP_INVALID) {
		//No such command. Clear input buffer and abort.
		clear_input_buffer(bind);
		return NULL;
	}

	// Shouldn't reach this point ever, so clean up and reset as a precaution.
	clear_input_buffer(bind);
	return NULL;
}

char* bind_translate_key_event(struct tb_event* event)
{
	char buf[32];
	if(print_event(buf, sizeof buf, event))
		return strdup(buf);
	else
		return NULL;
}
