#define _POSIX_C_SOURCE 201112LL

#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <termbox.h>

#include "bind.h"
#include "util/stringop.h"

// bind_node is a node in a trie of all the valid binds
struct bind_node {
	char *key; // the key combination of this node
	char *command; // if node is a leaf, the command, else NULL
	list_t *suffixes; // the children of this node
};

static const char *valid_prefixes[] = {"Ctrl+", "Meta+"};
static const char *valid_special_stubs[] = {
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

static const struct {
	const char *name;
	int key;
} key_name_pairs[] = {
	{"F1", TB_KEY_F1},
	{"F2", TB_KEY_F2},
	{"F3", TB_KEY_F3},
	{"F4", TB_KEY_F4},
	{"F5", TB_KEY_F5},
	{"F6", TB_KEY_F6},
	{"F7", TB_KEY_F7},
	{"F8", TB_KEY_F8},
	{"F9", TB_KEY_F9},
	{"F10", TB_KEY_F10},
	{"F11", TB_KEY_F11},
	{"F12", TB_KEY_F12},
	{"Left", TB_KEY_ARROW_LEFT},
	{"Right", TB_KEY_ARROW_RIGHT},
	{"Up", TB_KEY_ARROW_UP},
	{"Down", TB_KEY_ARROW_DOWN},
	{"Enter", TB_KEY_ENTER},
	{"Backspace", TB_KEY_BACKSPACE},
	{"Backspace", TB_KEY_BACKSPACE2},
	{"PageUp", TB_KEY_PGUP},
	{"PageDown", TB_KEY_PGDN},
	{"Home", TB_KEY_HOME},
	{"End", TB_KEY_END},
	{"Tab", TB_KEY_TAB},
	{"Insert", TB_KEY_INSERT},
	{"Delete", TB_KEY_DELETE},
	{"Ctrl+~", TB_KEY_CTRL_TILDE},
	{"Ctrl+a", TB_KEY_CTRL_A},
	{"Ctrl+b", TB_KEY_CTRL_B},
	{"Ctrl+c", TB_KEY_CTRL_C},
	{"Ctrl+d", TB_KEY_CTRL_D},
	{"Ctrl+e", TB_KEY_CTRL_E},
	{"Ctrl+f", TB_KEY_CTRL_F},
	{"Ctrl+g", TB_KEY_CTRL_G},
	{"Ctrl+h", TB_KEY_CTRL_H},
	{"Ctrl+i", TB_KEY_CTRL_I},
	{"Ctrl+j", TB_KEY_CTRL_J},
	{"Ctrl+k", TB_KEY_CTRL_K},
	{"Ctrl+l", TB_KEY_CTRL_L},
	{"Ctrl+m", TB_KEY_CTRL_M},
	{"Ctrl+n", TB_KEY_CTRL_N},
	{"Ctrl+o", TB_KEY_CTRL_O},
	{"Ctrl+p", TB_KEY_CTRL_P},
	{"Ctrl+q", TB_KEY_CTRL_Q},
	{"Ctrl+r", TB_KEY_CTRL_R},
	{"Ctrl+s", TB_KEY_CTRL_S},
	{"Ctrl+t", TB_KEY_CTRL_T},
	{"Ctrl+u", TB_KEY_CTRL_U},
	{"Ctrl+v", TB_KEY_CTRL_V},
	{"Ctrl+w", TB_KEY_CTRL_W},
	{"Ctrl+x", TB_KEY_CTRL_X},
	{"Ctrl+y", TB_KEY_CTRL_Y},
	{"Ctrl+z", TB_KEY_CTRL_Z},
	{"Ctrl+[", TB_KEY_CTRL_LSQ_BRACKET},
	{"Ctrl+]", TB_KEY_CTRL_RSQ_BRACKET},
	{"Ctrl+\\", TB_KEY_CTRL_BACKSLASH},
	{"Ctrl+/", TB_KEY_CTRL_SLASH},
};

void add_default_bindings(struct bind *binds) {
	bind_add(binds, "q", ":quit<Enter>");
	bind_add(binds, "Ctrl+c", ":quit<Enter>");

	bind_add(binds, "h", ":previous-mailbox<Enter>");
	bind_add(binds, "Left", ":previous-mailbox<Enter>");

	bind_add(binds, "j", ":next-message<Enter>");
	bind_add(binds, "Down", ":next-message<Enter>");
	bind_add(binds, "Ctrl+d", ":next-message --scroll 50%<Enter>");
	bind_add(binds, "PageDown", ":next-message --scroll 100%<Enter>");

	bind_add(binds, "k", ":previous-message<Enter>");
	bind_add(binds, "Up", ":previous-message<Enter>");
	bind_add(binds, "Ctrl+u", ":previous-message --scroll 50%<Enter>");
	bind_add(binds, "PageUp", ":previous-message --scroll 100%<Enter>");

	bind_add(binds, "l", ":next-mailbox<Enter>");
	bind_add(binds, "Right", ":next-mailbox<Enter>");

	bind_add(binds, "J", ":next-folder<Enter>");
	bind_add(binds, "K", ":previous-folder<Enter>");
}

static void init_bind_node(struct bind_node *bn) {
	bn->key = NULL;
	bn->command = NULL;
	bn->suffixes = create_list();
}

static void destroy_bind_node(struct bind_node *bn) {
	for (size_t i = 0; i < bn->suffixes->length; ++i) {
		destroy_bind_node(bn->suffixes->items[i]);
	}
	free(bn->key);
	free(bn->command);
	list_free(bn->suffixes);
}

static int is_valid_key(const char *key) {
	// Advance past any valid prefixes like Ctrl+ or Meta+
	for (size_t i = 0;
	    i < sizeof(valid_prefixes) / sizeof(valid_prefixes[0]);
	    ++i) {
		if (is_prefix_of(valid_prefixes[i], key)) {
			key += strlen(valid_prefixes[i]);
		}
	}

	// Check Shift+ as well, but if it's used, only special stubs may follow
	// to avoid Shift+W vs W tautology
	int must_be_special = 0;
	if (is_prefix_of("Shift+", key)) {
		key += strlen("Shift+");
		must_be_special = 1;
	}

	// Check if we're left with a valid regular key stub
	if (!must_be_special) {
		// Anything printable is valid, except '=' which can't go in a .ini file
		if (strlen(key) == 1 && isgraph(*key) && *key != '=') {
			return 1;
		}

		// Special case: 'Eq' as a alias for '='
		if (strcmp(key, "Eq") == 0) {
			return 1;
		}
	}

	// Maybe it's a special stub
	for (size_t i = 0;
		i < sizeof(valid_special_stubs) / sizeof(valid_special_stubs[0]);
		++i) {
		// Not one of our special stubs? Can only be invalid.
		if (strcmp(valid_special_stubs[i], key) == 0) {
			return 1;
		}
	}

	return 0;
}

static int is_valid_keys(const char *keys) {
	if (!keys || strlen(keys) == 0) {
		return 0;
	}

	int valid = 1;
	list_t *parts = split_string(keys, " ");

	for (size_t i = 0; i < parts->length; ++i) {
		if (!is_valid_key(parts->items[i])) {
			valid = 0;
			break;
		}
	}

	free_flat_list(parts);
	return valid;
}

void init_bind(struct bind* bind) {
	bind->keys = create_list();
	bind->binds = malloc(sizeof(struct bind_node));
	init_bind_node(bind->binds);
}

void destroy_bind(struct bind* bind) {
	destroy_bind_node(bind->binds);
	free(bind->binds);
}

static int compare_node_key(const struct bind_node* node, const char *key) {
	return strcmp(node->key, key);
}

enum bind_result bind_add(struct bind* bind, const char* keys, const char* command) {
	if (!command) {
		return BIND_INVALID_COMMAND;
	}

	if (!keys) {
		return BIND_INVALID_KEYS;
	}

	char *dirty_keys = strdup(keys);
	strip_quotes(dirty_keys);

	if (!is_valid_keys(keys)) {
		free(dirty_keys);
		return BIND_INVALID_KEYS;
	}

	// Split the key up into its individual parts
	list_t *parts = split_string(dirty_keys, " ");

	//Prepare our return code
	int result = BIND_SUCCESS;

	// Traverse the trie
	struct bind_node *node = bind->binds;
	for (size_t i = 0; i < parts->length; ++i) {
		// If we've reached a node that already has a command, there's a conflict
		if (node->command) {
			result = BIND_INVALID_COMMAND;
			break;
		}

		// Find / create a child with the current key
		struct bind_node *next_node = NULL;
		int child_index = list_seq_find(node->suffixes, (void*)&compare_node_key, parts->items[i]);
		if (child_index == -1) {
			// Create our new node
			next_node = malloc(sizeof(struct bind_node));
			init_bind_node(next_node);
			next_node->key = strdup(parts->items[i]);
			list_add(node->suffixes, next_node);
		} else {
			next_node = node->suffixes->items[child_index];
		}

		// We've now found the correct node for this key

		// Check if the node has a command
		if (next_node->command) {
			if (i + 1 < parts->length) {
				// If we're not at the end, it's a conflict
				result = BIND_CONFLICTS;
				break;
			} else {
				// Otherwise we just need to overwrite the existing bind.
				next_node->command = strdup(command);
				result = BIND_SUCCESS;
				break;
			}
		}

		if (i + 1 == parts->length) {
			// this is the last key part, try to insert command
			// but first, make sure there's no children
			if (next_node->suffixes->length > 0) {
				result = BIND_CONFLICTS;
			} else {
				next_node->command = strdup(command);
			}
		} else {
			// Otherwise, move down the trie
			node = next_node;
		}
	}

	free(dirty_keys);
	free_flat_list(parts);
	return result;
}

static int print_event(char *buf, size_t len, struct tb_event *event) {
	const char *str = NULL;

	// Build prefix first:
	const char *prefix = event->mod & TB_MOD_ALT ? "Meta+" : "";

	// Try plain old character input
	if (event->ch) {
		// Alias '=' into Eq, and ' ' into Space
		if (event->ch == '=') {
			return snprintf(buf, len, "%sEq", prefix);
		} else if (event->ch == ' ') {
			return snprintf(buf, len, "%sSpace", prefix);
		} else {
			return snprintf(buf, len, "%s%c", prefix, event->ch);
		}
	}

	// Try to find a special key
	for (size_t i = 0; i < sizeof(key_name_pairs) / sizeof(*key_name_pairs); ++i) {
		if (event->key == key_name_pairs[i].key) {
			str = key_name_pairs[i].name;
			break;
		}
	}
	// Was it one of the special keys?
	if (str) {
		return snprintf(buf, len, "%s%s", prefix, str);
	}

	// No idea what it's meant to be - give up
	buf[0] = 0;
	return 0;
}

enum lookup_result {
	LOOKUP_PARTIAL,
	LOOKUP_INVALID,
	LOOKUP_MATCH,
};

static enum lookup_result lookup_binding(struct bind_node *node, list_t *keys, const char **out_str) {
	for (size_t part = 0; part < keys->length; ++part) {
		const char* cur_key = keys->items[part];
		int found = 0;
		for (size_t i = 0; i < node->suffixes->length; ++i) {
			struct bind_node* cur_node = node->suffixes->items[i];
			if (strcmp(cur_node->key, cur_key) == 0) {
				node = node->suffixes->items[i];
				found = 1;
				break;
			}
		}
		if (!found) {
			return LOOKUP_INVALID;
		}
	}

	if (node->command) {
		*out_str = node->command;
		return LOOKUP_MATCH;
	}
	return LOOKUP_PARTIAL;
}

char* bind_input_buffer(struct bind *bind) {
	return join_list(bind->keys, " ");
}

static void clear_input_buffer(struct bind *bind) {
	free_flat_list(bind->keys);
	bind->keys = create_list();
}

const char* bind_handle_key_event(struct bind *bind, struct tb_event *event) {
	if (event->key == TB_KEY_ESC) {
		clear_input_buffer(bind);
		return NULL;
	}

	char buffer[128];
	print_event(buffer, sizeof(buffer), event);

	list_add(bind->keys, strdup(buffer));

	const char *command = NULL;
	enum lookup_result result = lookup_binding(bind->binds, bind->keys, &command);
	if (result == LOOKUP_PARTIAL) {
		return NULL;
	} else if (result == LOOKUP_MATCH) {
		clear_input_buffer(bind);
		return command;
	} else if (result == LOOKUP_INVALID) {
		clear_input_buffer(bind);
		return NULL;
	}

	// Should not happen
	clear_input_buffer(bind);
	return NULL;
}

char *bind_translate_key_event(struct tb_event *event) {
	char buf[32];
	if (print_event(buf, sizeof(buf), event)) {
		return strdup(buf);
	} else {
		return NULL;
	}
}

struct tb_event *bind_translate_key_name(const char *key) {
	int meta = 0;
	// Check for 'Meta+' prefix
	if (is_prefix_of("Meta+", key)) {
		meta = 1;
		key += strlen("Meta+");
	}

	// Check if we've got 'Meta+' and a regular keypress
	if (meta == 1) {
		// Alias Eq and Space
		if (strcmp(key, "Eq") == 0) {
			key = "=";
		} else if (strcmp(key, "Space") == 0) {
			key = " ";
		}

		// If we've only got a single character - it was a regular keypress
		if (strlen(key) == 1) {
			struct tb_event *e = calloc(1, sizeof(struct tb_event));
			e->type = TB_EVENT_KEY;
			e->mod = TB_MOD_ALT; // We're always 'meta' at this point
			e->ch = key[0];
			return e;
		}
	}

	for (size_t i = 0; i < sizeof(key_name_pairs) / sizeof(*key_name_pairs); ++i) {
		if (strcmp(key_name_pairs[i].name, key) == 0) {
			struct tb_event *e = calloc(1, sizeof(struct tb_event));
			e->type = TB_EVENT_KEY;
			e->mod = meta ? TB_MOD_ALT : 0;
			e->key = key_name_pairs[i].key;
			return e;
		}
	}

	// No matches, give up
	return NULL;
}
