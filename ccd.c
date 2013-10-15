#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <libusb-1.0/libusb.h>

#pragma mark App Helpers

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

#pragma mark CC-Debugger USB Protocol

#define CCD_USB_VENDOR_ID  0x0451
#define CCD_USB_PRODUCT_ID 0x16a2

#define CCD_BULK_ENDPOINT 0x4

enum {
	VENDOR_GET_INFO  = 0xc0, // IN
	VENDOR_STATE     = 0xc6, // IN
	VENDOR_SET_SPEED = 0xcf, // OUT, value = is_slow
	VENDOR_RESET     = 0xc9, // OUT, index = is_debug
	VENDOR_DEBUG     = 0xc5, // OUT
};

enum {
	TARGET_READ_HDR    = 0x1f,
	TARGET_RD_CONFIG   = 0x24,
	TARGET_READ_STATUS = 0x34,

	TARGET_ERASE_HDR   = 0x1c,
	TARGET_CHIP_ERASE  = 0x14,

	TARGET_WRITE_HDR   = 0x4c,
	TARGET_WR_CONFIG   = 0x1d,

	TARGET_GET_PC      = 0x28, 
	TARGET_SET_HW_BR   = 0x3f,
	TARGET_HALT        = 0x44,
	TARGET_RESUME      = 0x4c,
	TARGET_DBG_INSTR   = 0x57,
	TARGET_STEP_INSTR  = 0x5c,
	TARGET_GET_BM      = 0x67,
	TARGET_GET_CHIP_ID = 0x68,
	TARGET_BURST_WRITE = 0x80,
};

enum {
	CONFIG_SOFT_POWER_MODE = 0x20,
	CONFIG_TIMERS_OFF      = 0x08,
	CONFIG_DMA_PAUSE       = 0x04,
	CONFIG_TIMER_SUSPEND   = 0x02,
};

enum {
	STATUS_ERASE_BUSY        = 0x80,
	STATUS_PCON_IDLE         = 0x40,
	STATUS_CPU_HALTED        = 0x20,
	STATUS_PM_ACTIVE         = 0x10,
	STATUS_HALT_STATUS       = 0x08,
	STATUS_DEBUG_LOCKED      = 0x04,
	STATUS_OSCILLATOR_STABLE = 0x02,
	STATUS_STACK_OVERFLOW    = 0x01,
};

#pragma mark USB Helpers

typedef struct {
	libusb_context *usb_context;
	libusb_device **usb_devices;
	libusb_device *ccd_device;
	libusb_device_handle *ccd_device_handle;
} ccd_ctx_t;

static int control_transfer(
	ccd_ctx_t *ctx, int endpoint, int request,
	int value, int index, void *data, int size)
{
	int err;

	verbose("%s control transfer 0x%02x, %d, %d, %dB\n", 
		endpoint == LIBUSB_ENDPOINT_IN ? "IN" : "OUT",
		request, value, index, size);

	err = libusb_control_transfer(
		ctx->ccd_device_handle,
		LIBUSB_REQUEST_TYPE_VENDOR | endpoint,
		request,
		value, index, (unsigned char *)data, size, 0);
	if (err < 0 || err != size) {
		error_out("Control transfer failed: %s\n", libusb_error_name(err));
	}
	
	if (size) {
		for (int i = 0; i < size; i++) {
			verbose("%02x ", ((uint8_t*)data)[i]);
		}
		verbose("\n");
	}

	err = 0;

out:
	return err != 0;
}

static int control_transfer_in(
	ccd_ctx_t *ctx, int request, int value, int index, void *data, int size)
{
	return control_transfer(
		ctx, LIBUSB_ENDPOINT_IN, request, value, index, data, size);
}

static int control_transfer_out(
	ccd_ctx_t *ctx, int request, int value, int index, void *data, int size)
{
	return control_transfer(
		ctx, LIBUSB_ENDPOINT_OUT, request, value, index,  data, size);
}

static int bulk_transfer(ccd_ctx_t *ctx, int endpoint, void *data, int size)
{
	int err;
	int transferred;

	verbose("%s bulk transfer %dB\n", 
		endpoint == LIBUSB_ENDPOINT_IN ? "IN" : "OUT", size);

	err = libusb_bulk_transfer(
		ctx->ccd_device_handle, endpoint | CCD_BULK_ENDPOINT,
		(unsigned char *)data, size, &transferred, 0);
	if (err < 0 || transferred != size) {
		error_out("Bulk transfer failed: %s\n", libusb_error_name(err));
	}

	if (size) {
		for (int i = 0; i < size; i++) {
			verbose("%02x ", ((uint8_t*)data)[i]);
		}
		verbose("\n");
	}

	err = 0;

out:
	return err != 0;
}

static int bulk_transfer_in(ccd_ctx_t *ctx, void *data, int size)
{
	return bulk_transfer(ctx, LIBUSB_ENDPOINT_IN, data, size);
}

static int bulk_transfer_out(ccd_ctx_t *ctx, void *data, int size)
{
	return bulk_transfer(ctx, LIBUSB_ENDPOINT_OUT,  data, size);
}

#pragma mark CC-Debugger API

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
		error_out("Can't init usb stack: %s\n", libusb_error_name(err));
	}

	libusb_set_debug(ctx->usb_context, 0);

	usb_devcnt = libusb_get_device_list(ctx->usb_context, &ctx->usb_devices);
	if (usb_devcnt < 0) {
		err = 1;
		error_out("Can't get device list: %s\n", libusb_error_name(err));
	}

	for (int i = 0; i < usb_devcnt; i++) {
		struct libusb_device_descriptor usb_descriptor;

		err = libusb_get_device_descriptor(ctx->usb_devices[i], &usb_descriptor);
		if (err) {
			error_out("Can't get usb descriptor: %s\n", libusb_error_name(err));
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
		error_out("Can't grab device handle: %s\n", libusb_error_name(err));
	}

	if (libusb_kernel_driver_active(ctx->ccd_device_handle, 0) == 1) {
		err = libusb_detach_kernel_driver(ctx->ccd_device_handle, 0);
		if (err) {
			error_out("Can't detach kernel: %s\n", libusb_error_name(err));
		}
	}

	err = libusb_claim_interface(ctx->ccd_device_handle, 0);
	if (err) {
		error_out("Can't claim interface: %s\n", libusb_error_name(err));
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
	uint16_t chip;
	uint16_t fw_id;
	uint16_t fw_rev;
	uint16_t dontknow;
} ccd_info_t;

int ccd_get_info(ccd_ctx_t *ctx, ccd_info_t *info)
{
	return control_transfer_in(ctx, VENDOR_GET_INFO, 0, 0, info, sizeof(*info));
}

int ccd_state(ccd_ctx_t *ctx, uint8_t *byte)
{
	return control_transfer_in(ctx, VENDOR_STATE, 0, 0, byte, sizeof(*byte));
}

typedef enum {
	ccd_speed_slow = 1,
	ccd_speed_fast = 0
} ccd_speed_t;

int ccd_set_speed(ccd_ctx_t *ctx, ccd_speed_t speed)
{
	return control_transfer_out(ctx, VENDOR_SET_SPEED, speed, 0, NULL, 0);
}

typedef enum {
	ccd_mode_debug = 1,
	ccd_mode_nodebug = 0
} ccd_mode_t;

int ccd_reset(ccd_ctx_t *ctx, ccd_mode_t debug)
{
	return control_transfer_out(ctx, VENDOR_RESET, 0, debug, NULL, 0);
}

int ccd_debug(ccd_ctx_t *ctx)
{
	return control_transfer_out(ctx, VENDOR_DEBUG, 0, 0, NULL, 0);
}

#pragma mark CC Target Helpers

static int target_read_config(ccd_ctx_t *ctx, uint8_t *config)
{
	int err;
	uint8_t cmd[] = { TARGET_READ_HDR, TARGET_RD_CONFIG };

	err = bulk_transfer_out(ctx, cmd, sizeof(cmd));
	if (err) {
		goto out;
	}
	
	err = bulk_transfer_in(ctx, config, sizeof(*config));
	if (err) {
		goto out;
	}

out:
	return err;
}

static int target_write_config(ccd_ctx_t *ctx, uint8_t config)
{
	uint8_t cmd[] = { TARGET_WRITE_HDR, TARGET_WR_CONFIG, 0 };

	cmd[2] = config;

	return bulk_transfer_out(ctx, cmd, sizeof(cmd));
}

static int target_read_state(ccd_ctx_t *ctx, uint8_t *state)
{
	int err;
	uint8_t cmd[] = { TARGET_READ_HDR, TARGET_READ_STATUS };

	err = bulk_transfer_out(ctx, cmd, sizeof(cmd));
	if (err) {
		goto out;
	}
	
	err = bulk_transfer_in(ctx, state, sizeof(*state));
	if (err) {
		goto out;
	}

out:
	return err;
}

static int target_erase(ccd_ctx_t *ctx)
{
	uint8_t cmd[] = { TARGET_ERASE_HDR, TARGET_CHIP_ERASE };

	return bulk_transfer_out(ctx, cmd, sizeof(cmd));
}

static int target_erase_is_busy(ccd_ctx_t *ctx)
{
	uint8_t state;
	target_read_state(ctx, &state);

	return state & STATUS_ERASE_BUSY;
}

static int target_read_memory(ccd_ctx_t *ctx, uint16_t address, uint8_t *out)
{
	int err;
	uint8_t cmd[] = { 
		0x40, 0x55, 0x00, 0x72, 0x56, 0xe5, 0x92, 0xbe,
		0x57, 0x75, 0x92, 0x00, 0x74, 0x56, 0xe5, 0x83,
		0x76, 0x56, 0xe5, 0x82, 0xbe, 0x57, 0x90, 0x62,
		0x49, 0x4F, 0x55, 0xe0, 0x5e, 0x55, 0xa3, 0xd4,
		0x57, 0x90, 0xc2, 0x57, 0x75, 0x92, 0x90, 0x56,
		0x74
	};
	err = bulk_transfer_out(ctx, cmd, sizeof(cmd));
	if (err) {
		goto out;
	}

	err = bulk_transfer_in(ctx, out, sizeof(*out));
	if (err) {
		goto out;
	}

out:
	return err;
}

static int target_something(ccd_ctx_t *ctx, uint16_t address, uint16_t *out)
{
	int err;
	uint8_t cmd[] = { 
		0x40, 0x55, 0x00, 0x72, 0x56, 0xe5, 0x92, 0xbe, 
		0x57, 0x75, 0x92, 0x00, 0x74, 0x56, 0xe5, 0x83,
		0x76, 0x56, 0xe5, 0x82, 0xbe, 0x57, 0x90, 0x62, 
		0x76, 0x4e, 0x55, 0xe0, 0x5e, 0x55, 0xa3, 0x4f,
		0x55, 0xe0, 0x5e, 0x55, 0xa3, 0xd4, 0x57, 0x90, 
		0xc2, 0x57, 0x75, 0x92, 0x90, 0x56, 0x74
	};
	err = bulk_transfer_out(ctx, cmd, sizeof(cmd));
	if (err) {
		goto out;
	}

	err = bulk_transfer_in(ctx, out, sizeof(*out));
	if (err) {
		goto out;
	}

out:
	return err;
}

#pragma mark CC Target API

int ccd_erase_flash(ccd_ctx_t *ctx)
{
	uint8_t byte;
	uint16_t halfw;

	ccd_state(ctx, &byte);
	ccd_set_speed(ctx, ccd_speed_slow);
	ccd_reset(ctx, ccd_mode_debug);
	ccd_debug(ctx);

	target_read_config(ctx, &byte);

	target_write_config(ctx, CONFIG_TIMER_SUSPEND | CONFIG_SOFT_POWER_MODE);

	target_read_memory(ctx, 0x0, &byte);

	target_something(ctx, 0x0, &halfw);

	target_erase(ctx);

	while (target_erase_is_busy(ctx)) {
		usleep(200);
	}

	ccd_reset(ctx, ccd_mode_nodebug);

	return 0;
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

	verbose("Firmware version %04x (rev %04x)\n",
		ccd_info.fw_id, ccd_info.fw_rev);

	if (ccd_info.chip == 0) {
		error_out("No target found\n");
	}

	verbose("Found CC%x target\n", ccd_info.chip);

	ccd_erase_flash(&ccd_ctx);

out:
	ccd_close(&ccd_ctx);

	return 0;
}