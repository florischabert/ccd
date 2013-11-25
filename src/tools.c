/**
 * @section LICENSE
 * Copyright (c) 2013, Floris Chabert. All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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
	if (size > 0 && size % 16 != 0) {
		 log_print("\n");
	}
}
