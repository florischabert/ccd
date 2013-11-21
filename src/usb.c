#include <libusb-1.0/libusb.h>
#include <stdlib.h>

#include "usb.h"

struct usb_ctx_t {
	libusb_context *context;
	libusb_device **devices;
	libusb_device *device;
	libusb_device_handle *device_handle;
};

usb_ctx_t *usb_open_device(int vendor_id, int product_id)
{
	usb_ctx_t *ctx = NULL;
	int ret;
	int err = 1;
	ssize_t usb_devcnt;

	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		error_out("Can't allocate memory\n");
	}

	ctx->context = NULL;
	ctx->device = NULL;
	ctx->device_handle = NULL;
	ctx->devices = NULL;

	log_print("[USB] Opening connection\n");

	ret = libusb_init(&ctx->context);
	if (ret) {
		error_out("Can't init usb stack: %s\n", libusb_error_name(ret));
	}

	usb_devcnt = libusb_get_device_list(ctx->context, &ctx->devices);
	if (usb_devcnt < 0) {
		error_out("Can't get device list: %s\n", libusb_error_name(usb_devcnt));
	}

	for (int i = 0; i < usb_devcnt; i++) {
		struct libusb_device_descriptor usb_descriptor;

		ret = libusb_get_device_descriptor(ctx->devices[i], &usb_descriptor);
		if (ret) {
			error_out("Can't get usb descriptor: %s\n", libusb_error_name(ret));
		}

		if (usb_descriptor.idVendor == vendor_id &&
		    usb_descriptor.idProduct == product_id) {
			ctx->device = ctx->devices[i];
			break;
		}
	}

	if (!ctx->device) {
		error_out("Can't find device\n");
	}

	ret = libusb_open(ctx->device, &ctx->device_handle);
	if (ret) {
		error_out("Can't grab device handle: %s\n", libusb_error_name(ret));
	}

	if (libusb_kernel_driver_active(ctx->device_handle, 0) == 1) {
		ret = libusb_detach_kernel_driver(ctx->device_handle, 0);
		if (ret) {
			error_out("Can't detach kernel: %s\n", libusb_error_name(ret));
		}
	}

	ret = libusb_claim_interface(ctx->device_handle, 0);
	if (ret) {
		error_out("Can't claim interface: %s\n", libusb_error_name(ret));
	}

	err = 0;

out:
	if (err) {
		free(ctx);
		ctx = NULL;
	}
	return ctx;
}

void usb_close_device(usb_ctx_t *ctx)
{
	log_print("[USB] Closing connection\n");

	if (ctx) {
		if (ctx->device_handle) {
			libusb_release_interface(ctx->device_handle, 0);
			libusb_close(ctx->device_handle);
		}
		if (ctx->devices) {
			libusb_free_device_list(ctx->devices, 0);
		}
		if (ctx->context) {
			libusb_exit(ctx->context);
		}

		free(ctx);
	}
}


err_t usb_control_transfer(
	usb_ctx_t *ctx, usb_endpoint_t endpoint,
	int request, int value, int index, void *data, int size)
{
	err_t err = err_failed;
	int ret;

	log_print("[USB] Control Transfer <%s> %dB req=0x%02x <val=0x%02x, idx=0x%02x>\n", 
		endpoint == USB_IN ? "in" : "out", 
		size, request, value, index);

	ret = libusb_control_transfer(
		ctx->device_handle,
		((endpoint == USB_IN) ? LIBUSB_ENDPOINT_IN : LIBUSB_ENDPOINT_OUT) | LIBUSB_REQUEST_TYPE_VENDOR,
		request,
		value, index, (unsigned char *)data, size, 1000);
	if (ret < 0 || ret != size) {
		error_out("Control transfer failed: %s\n", libusb_error_name(ret));
	}

	log_bytes(data, size);
	
	err = err_none;

out:
	return err;
}

err_t usb_bulk_transfer(
	usb_ctx_t *ctx, usb_endpoint_t endpoint, void *data, int size)
{
	const int bulk_endpoint = 0x4;

	err_t err = err_failed;
	int ret;
	int transferred;

	log_print("[USB] Bulk Transfer <%s> %dB\n",
		endpoint == USB_IN ? "in" : "out",
		size);

	ret = libusb_bulk_transfer(
		ctx->device_handle,
		((endpoint == USB_IN) ? LIBUSB_ENDPOINT_IN : LIBUSB_ENDPOINT_OUT) | bulk_endpoint,
		(unsigned char *)data, size, &transferred, 1000);
	if (ret < 0) {
		error_out("Bulk transfer failed: %s\n", libusb_error_name(ret));
	}
	if (transferred != size) {
		error_out("Bulk transfer failer: transferred %dB instead of %dB\n", transferred, size);
	}

	log_bytes(data, size);

	err = err_none;

out:
	return err;
}
