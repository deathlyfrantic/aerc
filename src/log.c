/*
 * log.c - worker debug log handler
 */
#define _POSIX_C_SOURCE 200809L
#include "log.h"
#include "worker.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>

int colored = 1;
enum log_level loglevel_default = L_ERROR;
enum log_level l = L_INFO;

static const char *level_colors[] = {
	[L_ERROR ] = "\x1B[1;31m",
	[L_INFO  ] = "\x1B[1;34m",
	[L_DEBUG ] = "\x1B[1;30m",
};

void init_log(enum log_level level) {
	if (level != L_DEBUG) {
		// command "debuglog" needs to know the user specified log level when
		// turning off debug logging.
		loglevel_default = level;
	}
	l = level;
}

void set_log_level(enum log_level level) {
	l = level;
}

void reset_log_level(void) {
	l = loglevel_default;
}

/*
 * This function has a different signature if compiled in DEBUG mode - we
 * include the filename and line number in that case.
 */
#ifndef NDEBUG
void _worker_log(const char *filename, int line, enum log_level level,
		const char* format, ...) {
#else
void _worker_log(enum log_level level, const char* format, ...) {
#endif
	if (level <= l) {
		unsigned int c = level;
		if (c > sizeof(level_colors) / sizeof(char *)) {
			c = sizeof(level_colors) / sizeof(char *) - 1;
		}

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "%s", level_colors[c]);
		}

		va_list args;
		va_start(args, format);
#ifndef NDEBUG
		char *file = strdup(filename);
		fprintf(stderr, "[%s:%d] ", basename(file), line);
		free(file);
#endif
		vfprintf(stderr, format, args);
		va_end(args);

		if (colored && isatty(STDERR_FILENO)) {
			fprintf(stderr, "\x1B[0m");
		}
		fprintf(stderr, "\n");
	}
}
