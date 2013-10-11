#include <stdlib.h>
#include <stdio.h>

#include <libusb.h>

#define error_out(...) \
	do { \
		fprintf(stderr, __VA_ARGS__); \
		goto out; \
	} while (0)

int _verbose = 1;
#define verbose(...) \
	do { \
		if (_verbose) { \
			printf(__VA_ARGS__); \
		} \
	} while (0)

#define CCD_USB_VENDOR_ID  0x0451
#define CCD_USB_PRODUCT_ID 0x16a2

enum {
	VENDOR_GET_STATE = 0xC0,
};

typedef struct {
	libusb_context *usb_context;
	libusb_device **usb_devices;
	libusb_device *ccd_device;
	libusb_device_handle *ccd_device_handle;
} ccd_ctx_t;

int ccd_open(ccd_ctx_t *ctx)
{
	int err;
	ssize_t usb_devcnt;

	ctx->usb_context = NULL;
	ctx->ccd_device = NULL;
	ctx->ccd_device_handle = NULL;
	ctx->usb_devices = NULL;

	err = libusb_init(&ctx->usb_context);
	if (err) {
		error_out("Can't init usb stack (%d)\n", err);
	}

	libusb_set_debug(ctx->usb_context, 0);

	usb_devcnt = libusb_get_device_list(ctx->usb_context, &ctx->usb_devices);
	if (usb_devcnt < 0) {
		err = 1;
		error_out("Can't get device list (%d)\n", err);
	}

	for (int i = 0; i < usb_devcnt; i++) {
		struct libusb_device_descriptor usb_descriptor;

		err = libusb_get_device_descriptor(ctx->usb_devices[i], &usb_descriptor);
		if (err) {
			error_out("Can't get usb descriptor (%d)\n", err);
		}

		if (usb_descriptor.idVendor == CCD_USB_VENDOR_ID &&
		    usb_descriptor.idProduct == CCD_USB_PRODUCT_ID) {
			ctx->ccd_device = ctx->usb_devices[i];
			break;
		}
	}

	if (!ctx->ccd_device) {
		err = 1;
		error_out("Can't find CC Debugger\n");
	}

	verbose("Found CC Debugger\n");

	err = libusb_open(ctx->ccd_device, &ctx->ccd_device_handle);
	if (err) {
		error_out("Can't grab device handle (%d)\n", err);
	}

	if (libusb_kernel_driver_active(ctx->ccd_device_handle, 0) == 1) {
		err = libusb_detach_kernel_driver(ctx->ccd_device_handle, 0);
		if (err) {
			error_out("Can't detach kernel (%d)\n", err);
		}
	}

	err = libusb_claim_interface(ctx->ccd_device_handle, 0);
	if (err) {
		error_out("Can't claim interface (%d)\n", err);
	}

out:
	return err != 0;
}

void ccd_close(ccd_ctx_t *ctx)
{
	if (ctx->ccd_device_handle) {
		libusb_release_interface(ctx->ccd_device_handle, 0);
		libusb_close(ctx->ccd_device_handle);
	}
	if (ctx->usb_devices) {
		libusb_free_device_list(ctx->usb_devices, 0);
	}
	if (ctx->usb_context) {
		libusb_exit(ctx->usb_context);
	}
}

typedef struct __attribute__((packed)) {
	uint16_t unit;
	uint16_t fw_version;
	uint16_t fw_revision;
	uint16_t reserved;
} ccd_info_t;

int ccd_get_info(ccd_ctx_t *ctx, ccd_info_t *info)
{
	int err;

	err = libusb_control_transfer(
		ctx->ccd_device_handle,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		VENDOR_GET_STATE,
		0, 0, (unsigned char *)info, sizeof(ccd_info_t), 0);
	if (err < 0 || err != sizeof(ccd_info_t)) {
		error_out("Can't control the device (%d)\n", err);
	}

	err = 0;

out:
	return err != 0;
}

uint8_t ccd_target_status()
{

}

uint16_t ccd_target_chip_id()
{

}

uint16_t ccd_target_pc()
{

}

int ccd_target_reset()
{

}

int ccd_target_halt()
{

}

int ccd_target_resume()
{

}

int ccd_target_step_instr(uint32_t instr)
{

}

int main(int argc, char const *argv[])
{
	int err;
	ccd_ctx_t ccd_ctx;
	ccd_info_t ccd_info;

	err = ccd_open(&ccd_ctx);
	if (err) {
		goto out;
	}

	err = ccd_get_info(&ccd_ctx, &ccd_info);
	if (err) {
		goto out;
	}

	verbose("Firmware version %d (rev %x)\n",
		ccd_info.fw_version, ccd_info.fw_revision);

	if (ccd_info.unit == 0) {
		error_out("No target found\n");
	}

	verbose("Found CC%x target\n", ccd_info.unit);

out:
	ccd_close(&ccd_ctx);

	return 0;
}