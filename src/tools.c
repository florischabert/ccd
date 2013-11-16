#include "tools.h"

static int _log_enabled = 0;

void log_set(int enable_log)
{
	_log_enabled = enable_log;
}

void log_print(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	if (_log_enabled) {
		vfprintf(stderr, format, args);
	}

	va_end(args);
}

void log_bytes(const uint8_t *data, int size)
{
	for (int i = 0; i < size; i ++) {
		 log_print("%02x ", data[i]);
		if ((i+1) % 16 == 0) {
			 log_print("\n");
		}
	}
	if (size > 0) {
		 log_print("\n");
	}
}
