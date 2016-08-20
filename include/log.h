#ifndef _LOG_H
#define _LOG_H

enum log_level {
	L_ERROR = 1,
	L_INFO = 2,
	L_DEBUG = 3
};

void init_log(enum log_level level);

#ifndef NDEBUG
void _worker_log(const char *filename, int line, enum log_level level,
		const char* format, ...) __attribute__((format(printf,4,5)));
#define worker_log(VERBOSITY, FMT, ...) \
	_worker_log(__FILE__, __LINE__, VERBOSITY, FMT, ##__VA_ARGS__)
#else
void _worker_log(enum log_level level, const char* format, ...)
	__attribute__((format(printf,2,3)));
#define worker_log(VERBOSITY, FMT, ...) \
	_worker_log(VERBOSITY, FMT, ##__VA_ARGS__)
#endif

#endif
