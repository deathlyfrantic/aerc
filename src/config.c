/*
 * config.c - loads, parses, and owns the aerc configuration
 *
 * Note: much of this code is adapted from sway, which has many contributors.
 * You can see the original file here:
 *
 * https://github.com/SirCmpwn/sway/blob/master/sway/config.c
 *
 * Along with a list of contributors.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include "util/stringop.h"
#include "util/ini.h"
#include "util/list.h"
#include "colors.h"
#include "log.h"
#include "config.h"

struct aerc_config *config = NULL;

static bool file_exists(const char *path) {
	return access(path, R_OK) != -1;
}

static char *get_config_path(const char **config_paths, int len) {
	if (!getenv("XDG_CONFIG_HOME")) {
		char *home = getenv("HOME");
		char *config_home = malloc(strlen(home) + strlen("/.config") + 1);
		strcpy(config_home, home);
		strcat(config_home, "/.config");
		setenv("XDG_CONFIG_HOME", config_home, 1);
		worker_log(L_DEBUG, "Set XDG_CONFIG_HOME to %s", config_home);
		free(config_home);
	}

	wordexp_t p;
	char *path;

	int i;
	for (i = 0; i < len; ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			path = strdup(p.we_wordv[0]);
			wordfree(&p);
			if (file_exists(path)) {
				return path;
			}
		}
	}

	return NULL; // Not reached
}

static bool open_config(const char *path, FILE **f) {
	struct stat sb;
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		return false;
	}

	if (path == NULL) {
		worker_log(L_ERROR, "Unable to find a config file!");
		return false;
	}

	*f = fopen(path, "r");
	return *f != NULL;
}

static int handle_loading_indicator(struct aerc_config *config, const char *value) {
	free_flat_list(config->ui.loading_frames);
	config->ui.loading_frames = split_string(value, ",");
	return 1;
}

static int handle_config_option(void *_config, const char *section,
		const char *key, const char *value) {
	struct aerc_config *config = _config;
	worker_log(L_DEBUG, "Handling [%s]%s=%s", section, key, value);

	struct { const char *section; const char *key; bool *flag; } flags[] = {
		{ "ui", "show-all-headers", &config->ui.show_all_headers },
		{ "ui", "render-sidebar", &config->ui.render_sidebar }
	};
	struct { const char *section; const char *key; char **string; } strings[] = {
		{ "ui", "index-format", &config->ui.index_format },
		{ "ui", "timestamp-format", &config->ui.timestamp_format },
		{ "ui", "render-account-tabs", &config->ui.render_account_tabs },
		{ "ui", "border-style", &config->ui.border_style }
	};
	struct { const char *section; const char *key; int *value; } integers[] = {
		{ "ui", "sidebar-width", &config->ui.sidebar_width }
	};
	struct {
		const char *section;
		const char *key;
		int (*func)(struct aerc_config *config, const char *value);
	} funcs[] = {
		{ "ui", "loading-frames", handle_loading_indicator }
	};

	if (strcmp(section, "colors") == 0) {
		set_color(key, value);
		return 1;
	}

	for (size_t i = 0; i < sizeof(flags) / (sizeof(void *) * 3); ++i) {
		if (strcmp(flags[i].section, section) == 0
				&& strcmp(flags[i].key, key) == 0) {
			bool is_true = false;
			is_true = is_true || strcasecmp(value, "enabled");
			is_true = is_true || strcasecmp(value, "enable");
			is_true = is_true || strcasecmp(value, "true");
			is_true = is_true || strcasecmp(value, "yes");
			is_true = is_true || strcasecmp(value, "on");
			is_true = is_true || strcasecmp(value, "1");
			*flags[i].flag = is_true;
			return 1;
		}
	}

	for (size_t i = 0; i < sizeof(strings) / (sizeof(void *) * 3); ++i) {
		if (strcmp(strings[i].section, section) == 0
				&& strcmp(strings[i].key, key) == 0) {
			if(*strings[i].string) free(*strings[i].string);
			*strings[i].string = strdup(value);
			return 1;
		}
	}

	for (size_t i = 0; i < sizeof(integers) / (sizeof(void *) * 3); ++i) {
		if (strcmp(integers[i].section, section) == 0
				&& strcmp(integers[i].key, key) == 0) {
			char *end;
			int val = (int)strtol(value, &end, 10);
			if (*end) {
				worker_log(L_ERROR, "Invalid value for [%s]%s: %s",
						section, key, value);
				return 0;
			}
			*integers[i].value = val;
			return 1;
		}
	}

	for (size_t i = 0; i < sizeof(funcs) / (sizeof(void *) * 3); ++i) {
		if (strcmp(funcs[i].section, section) == 0
				&& strcmp(funcs[i].key, key) == 0) {
			return funcs[i].func(config, value);
		}
	}

	worker_log(L_ERROR, "Unknown config option [%s]%s", section, key);
	// TODO: Error?
	return 1;
}

static bool load_config(const char *path, struct aerc_config *config) {
	worker_log(L_DEBUG, "Loading config from %s", path);

	FILE *f;
	if (!open_config(path, &f)) {
		worker_log(L_ERROR, "Unable to open %s for reading", path);
		return false;
	}

	int ini = ini_parse_file(f, handle_config_option, config);
	if (ini != 0) {
		worker_log(L_ERROR, "Configuration parsing error on line %d", ini);
	}

	fclose(f);

	return ini == 0;
}

static int account_compare(const void *_account, const void *_name) {
	const struct account_config *account = _account;
	const char *name = _name;
	return strcmp(account->name, name);
}

static int handle_account_option(void *_config, const char *section,
		const char *key, const char *value) {
	struct aerc_config *config = _config;
	worker_log(L_DEBUG, "Handling [%s]%s", section, key);

	int ai = list_seq_find(config->accounts, account_compare, section);
	struct account_config *account;
	if (ai == -1) {
		account = calloc(1, sizeof(struct account_config));
		account->name = strdup(section);
		list_add(config->accounts, account);
	} else {
		account = config->accounts->items[ai];
	}

	if (strcmp(key, "source") == 0) {
		account->source = strdup(value);
	} else {
		struct account_config_extra *extra =
			calloc(1, sizeof(struct account_config_extra));
		extra->key = strdup(key);
		extra->value = strdup(value);
		list_add(account->extras, extra);
	}

	return 1;
}

static bool load_accounts(const char *path, struct aerc_config *config) {
	FILE *f;
	if (!open_config(path, &f)) {
		worker_log(L_ERROR, "Unable to open %s for reading", path);
		return false;
	}

	int ini = ini_parse_file(f, handle_account_option, config);
	if (ini != 0) {
		worker_log(L_ERROR, "Configuration parsing error on line %d", ini);
	}

	fclose(f);

	return ini == 0;
}

static void config_defaults(struct aerc_config *config) {
	config->accounts = create_list();
	config->ui.loading_frames = create_list();
	list_add(config->ui.loading_frames, strdup("..  "));
	list_add(config->ui.loading_frames, strdup(" .. "));
	list_add(config->ui.loading_frames, strdup("  .."));
	list_add(config->ui.loading_frames, strdup(" .. "));
	config->ui.index_format = strdup("%4C %Z %D %-17.17n %s");
	config->ui.timestamp_format = strdup("%F %l:%M %p");
	config->ui.show_all_headers = false;
}

void free_config(struct aerc_config *config) {
	for (int i = 0; i < config->accounts->length; ++i) {
		struct account_config *account = config->accounts->items[i];
		free(account->name);
		free(account->source);
		for (int j = 0; j < account->extras->length; ++j) {
			struct account_config_extra *extra = account->extras->items[i];
			free(extra->key);
			free(extra->value);
			free(extra);
		}
		list_free(account->extras);
		free(account);
	}
	list_free(config->accounts);
	free(config->ui.index_format);
	free(config->ui.timestamp_format);
}

bool load_accounts_config() {
	static const char *account_paths[] = {
		"$HOME/.aerc/accounts.conf",
		"$XDG_CONFIG_HOME/aerc/accounts.conf",
		SYSCONFDIR "/aerc/accounts.conf",
	};

	char* path = get_config_path(account_paths, 3);
	bool success = load_accounts(path, config);
	free(path);
	return success;
}

bool load_main_config(const char *file) {
	static const char *config_paths[] = {
		"$HOME/.aerc/aerc.conf",
		"$XDG_CONFIG_HOME/aerc/aerc.conf",
		SYSCONFDIR "/aerc/aerc.conf",
	};

	char *path;
	if (file != NULL) {
		path = strdup(file);
	} else {
		path = get_config_path(config_paths, 3);
	}

	struct aerc_config *old_config = config;
	config = calloc(1, sizeof(struct aerc_config));

	config_defaults(config);

	bool success = load_config(path, config);

	if (old_config) {
		free_config(old_config);
	}
	if (success) {
		load_accounts_config();
	}

	free(path);

	return success;
}
