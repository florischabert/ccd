#ifndef TOOLS_H
#define TOOLS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum {
	err_none   = 0,
	err_failed = 1,
	err_oom    = 2,
} err_t;

#define error_out(...) \
	do { \
		fprintf(stderr, __VA_ARGS__); \
		goto out; \
	} while (0)

#define noerr_or_out(cond) \
	do { \
		if ((cond)) { \
			goto out; \
		} \
	} while (0)

void log_set(int enable_log);
void log_print(const char *format, ...);
void log_bytes(const uint8_t *data, int size);

#endif
