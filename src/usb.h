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
