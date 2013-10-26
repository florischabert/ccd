#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>

#include <libusb-1.0/libusb.h>

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

static int _log_enabled = 0;

static void log_set(int enable_log)
{
	_log_enabled = enable_log;
}

static void log(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	if (_log_enabled) {
		vfprintf(stderr, format, args);
	}

	va_end(args);
}

#pragma mark CCD-Debugger API

typedef struct {
	libusb_context *usb_context;
	libusb_device **usb_devices;
	libusb_device *ccd_device;
	libusb_device_handle *ccd_device_handle;
} ccd_ctx_t;

int ccd_open(ccd_ctx_t *ctx);
void ccd_close(ccd_ctx_t *ctx);

typedef struct __attribute__((packed)) {
	uint16_t chip;
	uint16_t fw_id;
	uint16_t fw_rev;
	uint16_t dontknow;
} ccd_info_t;
int ccd_get_info(ccd_ctx_t *ctx, ccd_info_t *info);

int ccd_reset(ccd_ctx_t *ctx);
int ccd_erase_flash(ccd_ctx_t *ctx);
int ccd_read_memory(ccd_ctx_t *ctx, uint16_t addr, void *data, int size);
int ccd_write_memory(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size, uint8_t *ret);

#pragma mark CCD USB Protocol

#define CCD_USB_VENDOR_ID  0x0451
#define CCD_USB_PRODUCT_ID 0x16a2

#define MEM_CHIP_VERSION   0x6249
#define MEM_CHIP_INFO      0x6276

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

typedef enum {
	USB_IN  = LIBUSB_ENDPOINT_IN ,
	USB_OUT = LIBUSB_ENDPOINT_OUT,
} usb_endpoint_t;

static int control_transfer(
	ccd_ctx_t *ctx, usb_endpoint_t endpoint,
	int request, int value, int index, void *data, int size)
{
	int err = 1;
	int ret;

	log("[USB] Control Transfer <%s> %dB req=0x%02x <val=0x%02x, idx=0x%02x>\n", 
		endpoint == USB_IN ? "in" : "out", 
		size, request, value, index);

	ret = libusb_control_transfer(
		ctx->ccd_device_handle,
		LIBUSB_REQUEST_TYPE_VENDOR | endpoint,
		request,
		value, index, (unsigned char *)data, size, 0);
	if (ret < 0 || ret != size) {
		error_out("Control transfer failed: %s\n", libusb_error_name(ret));
	}

	if (size > 0) {
		for (int i = 0; i < size; i ++) {
			log("%02x ", ((uint8_t*)data)[i]);
			if ((i+1) % 16 == 0) {
				log("\n");
			}
		}
		log("\n");
	}
	
	err = 0;

out:
	return err;
}

static int bulk_transfer(
	ccd_ctx_t *ctx, usb_endpoint_t endpoint, void *data, int size)
{
	const int bulk_endpoint = 0x4;

	int err = 1;
	int ret;
	int transferred;

	log("[USB] Control Transfer <%s> %dB\n",
		endpoint == USB_IN ? "in" : "out",
		size);

	ret = libusb_bulk_transfer(
		ctx->ccd_device_handle, endpoint | bulk_endpoint,
		(unsigned char *)data, size, &transferred, 0);
	if (ret < 0 || transferred != size) {
		error_out("Bulk transfer failed: %s\n", libusb_error_name(ret));
	}

	if (size > 0) {
		for (int i = 0; i < size; i ++) {
			log("%02x ", ((uint8_t*)data)[i]);
			if ((i+1) % 16 == 0) {
				log("\n");
			}
		}
		log("\n");
	}

	err = 0;

out:
	return ret;
}

#pragma mark CC Target Helpers

static int target_read_config(ccd_ctx_t *ctx, uint8_t *config)
{
	int err;
	uint8_t cmd[] = { TARGET_READ_HDR, TARGET_RD_CONFIG };

	err = bulk_transfer(ctx, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = bulk_transfer(ctx, USB_IN, config, sizeof(*config));
	noerr_or_out(err);

out:
	return err;
}

static int target_write_config(ccd_ctx_t *ctx, uint8_t config)
{
	uint8_t cmd[] = { TARGET_WRITE_HDR, TARGET_WR_CONFIG, 0 };

	cmd[2] = config;

	return bulk_transfer(ctx, USB_OUT, cmd, sizeof(cmd));
}

static int target_status(ccd_ctx_t *ctx, uint8_t *status)
{
	int err;
	uint8_t cmd[] = { TARGET_READ_HDR, TARGET_READ_STATUS };

	err = bulk_transfer(ctx, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = bulk_transfer(ctx, USB_IN, status, sizeof(*status));
	noerr_or_out(err);

out:
	return err;
}

static int target_erase(ccd_ctx_t *ctx)
{
	uint8_t cmd[] = { TARGET_ERASE_HDR, TARGET_CHIP_ERASE };

	return bulk_transfer(ctx, USB_OUT, cmd, sizeof(cmd));
}

static int target_read_memory(
	ccd_ctx_t *ctx, uint16_t addr, uint8_t *data, int size)
{
	uint8_t header[] = { 
		0x40, 0x55, 0x00, 0x72, 0x56, 0xe5, 0x92, 0xbe, 
		0x57, 0x75, 0x92, 0x00, 0x74, 0x56, 0xe5, 0x83,
		0x76, 0x56, 0xe5, 0x82
	};

	uint8_t footer[] = { 
		0xd4, 0x57, 0x90, 0xc2, 0x57, 0x75, 0x92, 0x90,
		0x56, 0x74
	};

	uint8_t mov_dptr_addr16[] = {
		0xbe, 0x57,
		0x90, 0x0, 0x0
	};

	uint8_t mov_a_dptr[] = {
		0x4e, 0x55,
		0xe0
	};

	uint8_t inc_dptr[] = {
		0x5e, 0x55,
		0xa3
	};

	int err;
	uint8_t *cmd;
	int cmd_size = 0;

	cmd_size += sizeof(header);
	cmd_size += sizeof(mov_dptr_addr16);
	cmd_size += size * sizeof(mov_a_dptr);
	cmd_size += size * sizeof(inc_dptr);
	cmd_size += sizeof(footer);

	cmd = malloc(cmd_size);

	mov_dptr_addr16[3] = addr & 0xff;
	mov_dptr_addr16[4] = addr >> 8;

	memcpy(cmd, header, sizeof(header));
	memcpy(cmd, mov_dptr_addr16, sizeof(mov_dptr_addr16));
	for (int i = 0; i < size; i++) {
		memcpy(cmd, mov_a_dptr, sizeof(mov_a_dptr));
		memcpy(cmd, inc_dptr, sizeof(inc_dptr));
	}
	memcpy(cmd, footer, sizeof(footer));

	err = bulk_transfer(ctx, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);
	
	free(cmd);

	err = bulk_transfer(ctx, USB_IN, data, size);
	noerr_or_out(err);

out:
	return err;
}

static int target_write_memory(
	ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size, uint8_t *ret)
{
	uint8_t header[] = { 
		0x40, 0x55, 0x00, 0x72, 0x56, 0xe5, 0x92, 0xbe, 
		0x57, 0x75, 0x92, 0x00, 0x74, 0x56, 0xe5, 0x83,
		0x76, 0x56, 0xe5, 0x82
	};

	uint8_t footer[] = { 
		0xd4, 0x57, 0x90, 0xc2, 0x57, 0x75, 0x92, 0x90,
		0x56, 0x74
	};

	uint8_t mov_dptr_addr16[] = {
		0xbe, 0x57,
		0x90, 0x0, 0x0
	};

	uint8_t mov_a_data[] = {
		0x8e, 0x56,
		0x74, 0x0
	};

	uint8_t mov_dptr_a[] = {
		0x5e, 0x55,
		0xf0
	};

	uint8_t inc_dptr[] = {
		0x5e, 0x55,
		0xa3
	};

	int err;
	uint8_t *cmd;
	int cmd_size = 0;

	cmd_size += sizeof(header);
	cmd_size += sizeof(mov_dptr_addr16);
	cmd_size += size * sizeof(mov_a_data);
	cmd_size += size * sizeof(mov_dptr_a);
	cmd_size += size * sizeof(inc_dptr);
	cmd_size += sizeof(footer);

	cmd = malloc(cmd_size);

	mov_dptr_addr16[3] = addr & 0xff;
	mov_dptr_addr16[4] = addr >> 8;

	memcpy(cmd, header, sizeof(header));
	memcpy(cmd, mov_dptr_addr16, sizeof(mov_dptr_addr16));
	for (int i = 0; i < size; i++) {
		mov_dptr_addr16[3] = data[i];
		memcpy(cmd, mov_a_data, sizeof(mov_a_data));
		memcpy(cmd, mov_dptr_a, sizeof(mov_dptr_a));
		memcpy(cmd, inc_dptr, sizeof(inc_dptr));
	}
	memcpy(cmd, footer, sizeof(footer));

	err = bulk_transfer(ctx, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);
	
	free(cmd);

	err = bulk_transfer(ctx, USB_IN, ret, sizeof(*ret));
	noerr_or_out(err);

out:
	return err;
}


#pragma mark CC-Debugger Helpers

static int get_state(ccd_ctx_t *ctx, uint8_t *state)
{
	return control_transfer(ctx, USB_IN, VENDOR_STATE, 0, 0, state, sizeof(*state));
}

static int set_speed(ccd_ctx_t *ctx, int fast_mode)
{
	return control_transfer(ctx, USB_OUT, VENDOR_SET_SPEED, !fast_mode, 0, NULL, 0);
}

static int reset_debug(ccd_ctx_t *ctx)
{
	return control_transfer(ctx, USB_OUT, VENDOR_RESET, 0, 1, NULL, 0);
}

static int reset(ccd_ctx_t *ctx)
{
	return control_transfer(ctx, USB_OUT, VENDOR_RESET, 0, 0, NULL, 0);
}

static int debug_enter(ccd_ctx_t *ctx)
{
	return control_transfer(ctx, USB_OUT, VENDOR_DEBUG, 0, 0, NULL, 0);
}

#pragma mark CC-Debugger Implementation

int ccd_open(ccd_ctx_t *ctx)
{
	int err = 1;
	int ret;
	ssize_t usb_devcnt;

	ctx->usb_context = NULL;
	ctx->ccd_device = NULL;
	ctx->ccd_device_handle = NULL;
	ctx->usb_devices = NULL;

	log("[USB] Opening connection\n");

	ret = libusb_init(&ctx->usb_context);
	if (ret) {
		error_out("Can't init usb stack: %s\n", libusb_error_name(ret));
	}

	usb_devcnt = libusb_get_device_list(ctx->usb_context, &ctx->usb_devices);
	if (usb_devcnt < 0) {
		error_out("Can't get device list: %s\n", libusb_error_name(usb_devcnt));
	}

	for (int i = 0; i < usb_devcnt; i++) {
		struct libusb_device_descriptor usb_descriptor;

		ret = libusb_get_device_descriptor(ctx->usb_devices[i], &usb_descriptor);
		if (ret) {
			error_out("Can't get usb descriptor: %s\n", libusb_error_name(ret));
		}

		if (usb_descriptor.idVendor == CCD_USB_VENDOR_ID &&
		    usb_descriptor.idProduct == CCD_USB_PRODUCT_ID) {
			ctx->ccd_device = ctx->usb_devices[i];
			break;
		}
	}

	if (!ctx->ccd_device) {
		error_out("Can't find CC Debugger\n");
	}

	ret = libusb_open(ctx->ccd_device, &ctx->ccd_device_handle);
	if (ret) {
		error_out("Can't grab device handle: %s\n", libusb_error_name(ret));
	}

	if (libusb_kernel_driver_active(ctx->ccd_device_handle, 0) == 1) {
		ret = libusb_detach_kernel_driver(ctx->ccd_device_handle, 0);
		if (ret) {
			error_out("Can't detach kernel: %s\n", libusb_error_name(ret));
		}
	}

	ret = libusb_claim_interface(ctx->ccd_device_handle, 0);
	if (ret) {
		error_out("Can't claim interface: %s\n", libusb_error_name(ret));
	}

	err = 0;

out:
	return err;
}

void ccd_close(ccd_ctx_t *ctx)
{
	log("[USB] Closing connection\n");

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

int ccd_reset(ccd_ctx_t *ctx)
{
	return reset(ctx);
}

int ccd_get_info(ccd_ctx_t *ctx, ccd_info_t *info)
{
	return control_transfer(ctx, USB_IN, VENDOR_GET_INFO, 0, 0, info, sizeof(*info));
}

int ccd_erase_flash(ccd_ctx_t *ctx)
{
	int err;
	uint8_t state;
	uint8_t config;
	uint8_t cc_status;

	err = get_state(ctx, &state);
	noerr_or_out(err);
	err = set_speed(ctx, 0);
	noerr_or_out(err);
	err = reset_debug(ctx);
	noerr_or_out(err);
	err = debug_enter(ctx);
	noerr_or_out(err);

	err = target_read_config(ctx, &config);
	noerr_or_out(err);

	err = target_write_config(ctx, CONFIG_TIMER_SUSPEND | CONFIG_SOFT_POWER_MODE);
	noerr_or_out(err);

	// target_read_memory(ctx, MEM_CHIP_VERSION, &byte);
	// target_read_memory(ctx, MEM_CHIP_INFO, &halfw);

	err = target_erase(ctx);
	noerr_or_out(err);

	do {
		usleep(200);
		err = target_status(ctx, &cc_status);
		noerr_or_out(err);
	} while (cc_status & STATUS_ERASE_BUSY);

	err = reset(ctx);
	noerr_or_out(err);

	err = 0;

out:
	return err;
}

int ccd_read_memory(ccd_ctx_t *ctx, uint16_t addr, void *data, int size)
{
	return target_read_memory(ctx, addr, data, size);
}

int ccd_write_memory(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size, uint8_t *ret)
{
	return target_write_memory(ctx, addr, data, size, ret);
}

#pragma mark App

typedef struct {
	int verbose;
	int erase;
} options_t;

int parse_options(options_t *options, int argc, char * const *argv)
{
	int err = 0;
	int print_usage = 0;

	options->verbose = 0;
	options->erase = 0;

	while (1)
	{
		static struct option long_options[] =
		{
			{"help",    no_argument, 0, 'h'},
			{"verbose", no_argument, 0, 'v'},
			{"erase",   no_argument, 0, 'e'},
			{0, 0, 0, 0}
		};

		int option_index = 0;
		int c = getopt_long (argc, argv, "vhe", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c)
		{
			case 'v':
				options->verbose = 1;
				break;
			case 'e':
				options->erase = 1;
				break;
			case '?':
				err = 1;
				break;
			case 'h':
			default:
				print_usage = 1;
		}
	}

	if (print_usage) {
		printf("Usage: %s [options]\n", argv[0]);
		printf("Options:\n");
		printf("  -h, --help   \tPrint this help\n");
		printf("  -v, --verbose\tVerbose mode\n");
		printf("  -e, --erase  \tErase flash\n");

		err = 1;
	}

	return err;
}

int main(int argc, char * const *argv)
{
	int err;
	ccd_ctx_t ccd_ctx;
	ccd_info_t ccd_info;
	options_t options;

	err = parse_options(&options, argc, argv);
	if (err) {
		goto out_parse;
	}

	printf("Looking for CC-Debugger... \n");

	if (options.verbose) {
		log_set(1);
	}

	err = ccd_open(&ccd_ctx);
	if (err) {
		goto out;
	}

	printf("Querying firmware... \n");
	fflush(stdout);

	err = ccd_get_info(&ccd_ctx, &ccd_info);
	noerr_or_out(err);

	printf("Version 0x%04x (rev 0x%04x)\n", ccd_info.fw_id, ccd_info.fw_rev);

	if (ccd_info.chip == 0) {
		error_out("No target found\n");
	}

	printf("Found CC%x target\n", ccd_info.chip);

	if (options.erase) {
		err = ccd_erase_flash(&ccd_ctx);
		noerr_or_out(err);
	}

	err = 0;

out:
	ccd_close(&ccd_ctx);
out_parse:
	return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
