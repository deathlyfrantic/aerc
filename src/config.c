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
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wordexp.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "util/ini.h"
#include "util/list.h"
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

static int handle_config_option(void *_config, const char *section,
		const char *key, const char *value) {
	struct aerc_config *config = _config;
	worker_log(L_DEBUG, "Handling [%s]%s=%s", section, key, value);

	struct { const char *section; const char *key; bool *flag; } flags[] = {
		{ "ui", "show-all-headers", &config->ui.show_all_headers }
	};
	struct { const char *section; const char *key; char **string; } strings[] = {
		{ "ui", "index-format", &config->ui.index_format },
		{ "ui", "timestamp-format", &config->ui.timestamp_format }
	};

	for (size_t i = 0; i < sizeof(flags) / (sizeof(void *) * 3); ++i) {
		if (strcmp(flags[i].section, section) == 0
				&& strcmp(flags[i].key, key) == 0) {
			bool is_true = false;
			is_true = is_true || strcasecmp(value, "true");
			is_true = is_true || strcasecmp(value, "yes");
			is_true = is_true || strcasecmp(value, "enabled");
			is_true = is_true || strcasecmp(value, "1");
			*flags[i].flag = is_true;
			return 1;
		}
	}

	for (size_t i = 0; i < sizeof(strings) / (sizeof(void *) * 3); ++i) {
		if (strcmp(strings[i].section, section) == 0
				&& strcmp(strings[i].key, key) == 0) {
			*strings[i].string = strdup(value);
			return 1;
		}
	}

	worker_log(L_ERROR, "Unknown config option [%s] %s", section, key);
	return 0;
}

static bool load_config(const char *path, struct aerc_config *config) {
	worker_log(L_INFO, "Loading config from %s", path);

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
	const struct account_config *account = account;
	const char *name = name;
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

	return load_accounts(get_config_path(account_paths, 3), config);
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

	return success;
}
