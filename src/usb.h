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

#ifndef USB_H
#define USB_H

#include "tools.h"

#define CCD_USB_VENDOR_ID  0x0451
#define CCD_USB_PRODUCT_ID 0x16a2

enum {
	VENDOR_GET_INFO  = 0xc0, // IN
	VENDOR_STATE     = 0xc6, // IN
	VENDOR_SET_SPEED = 0xcf, // OUT, value = is_slow
	VENDOR_RESET     = 0xc9, // OUT, index = is_debug
	VENDOR_DEBUG     = 0xc5, // OUT
};

typedef enum {
	USB_IN,
	USB_OUT,
} usb_endpoint_t;

typedef struct usb_ctx_t usb_ctx_t;

usb_ctx_t *usb_open_device(int vendor_id, int product_id);
void usb_close_device(usb_ctx_t *ctx);

err_t usb_control_transfer(
	usb_ctx_t *ctx, usb_endpoint_t endpoint,
	int request, int value, int index, void *data, int size);

err_t usb_bulk_transfer(
	usb_ctx_t *ctx, usb_endpoint_t endpoint, void *data, int size);

#endif
