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
} ccd_fw_info_t;

typedef struct {
	int chip_id;
	int chip_version;
	int flash_size;
	int sram_size;
} ccd_target_info_t;

int ccd_fw_info(ccd_ctx_t *ctx, ccd_fw_info_t *info);
int ccd_target_info(ccd_ctx_t *ctx, ccd_target_info_t *info);
int ccd_reset(ccd_ctx_t *ctx);
int ccd_erase_flash(ccd_ctx_t *ctx);
int ccd_read_memory(ccd_ctx_t *ctx, uint16_t addr, void *data, int size);
int ccd_write_memory(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size);

#pragma mark CCD USB Protocol

#define CCD_USB_VENDOR_ID  0x0451
#define CCD_USB_PRODUCT_ID 0x16a2

enum {
	MEM_CHIP_VERSION = 0x6249,
	MEM_CHIP_ID      = 0x624a,
	MEM_CHIP_INFO    = 0x6276,
};

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

static void log_bytes(uint8_t *data, int size)
{
	for (int i = 0; i < size; i ++) {
		log("%02x ", data[i]);
		if ((i+1) % 16 == 0) {
			log("\n");
		}
	}
	if (size > 0) {
		log("\n");
	}
}

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

	log_bytes(data, size);
	
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

	log("[USB] Bulk Transfer <%s> %dB\n",
		endpoint == USB_IN ? "in" : "out",
		size);

	ret = libusb_bulk_transfer(
		ctx->ccd_device_handle, endpoint | bulk_endpoint,
		(unsigned char *)data, size, &transferred, 0);
	if (ret < 0) {
		error_out("Bulk transfer failed: %s\n", libusb_error_name(ret));
	}
	if (transferred != size) {
		error_out("Transferred %dB instead of %dB\n", transferred, size);
	}

	log_bytes(data, size);

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

static int target_command_add(void **cmd, int *size, void *data, int data_size)
{
	int err = 1;

	*cmd = realloc(*cmd, *size + data_size);
	if (*cmd == NULL) {
		error_out("Can't allocate memory\n");
	}

	memcpy((uint8_t*)*cmd + *size, data, data_size);
	*size += data_size;

	err = 0;

out:
	return err;	
}

static int target_command_init(void **cmd, int *size)
{
	static uint8_t header[] = { 
		0x40, 0x55, 0x00, 0x72, 0x56, 0xe5, 0x92, 0xbe, 
		0x57, 0x75, 0x92, 0x00, 0x74, 0x56, 0xe5, 0x83,
		0x76, 0x56, 0xe5, 0x82
	};

	*cmd = NULL;
	*size = 0;

	return target_command_add(cmd, size, header, sizeof(header));
}

static int target_command_finalize(void **cmd, int *size)
{
	static uint8_t footer[] = { 
		0xd4, 0x57, 0x90, 0xc2, 0x57, 0x75, 0x92, 0x90,
		0x56, 0x74
	};

	return target_command_add(cmd, size, footer, sizeof(footer));
}

static int target_read_memory(
	ccd_ctx_t *ctx, uint16_t addr, uint8_t *data, int size)
{
	int err = 1;
	void *cmd = NULL;
	int cmd_size;

	static uint8_t mov_dptr_addr16[] = {
		0xbe, 0x57,
		0x90, 0x0, 0x0
	};

	static uint8_t mov_a_dptr[] = {
		0x4e, 0x55,
		0xe0
	};

	static uint8_t inc_dptr[] = {
		0x5e, 0x55,
		0xa3
	};

	err = target_command_init(&cmd, &cmd_size);
	noerr_or_out(err);

	mov_dptr_addr16[4] = addr & 0xff;
	mov_dptr_addr16[3] = addr >> 8;
	err = target_command_add(&cmd, &cmd_size, mov_dptr_addr16, sizeof(mov_dptr_addr16));
	noerr_or_out(err);

	for (int i = 0; i < size; i++) {
		mov_a_dptr[0] |= (i == size-1) ? 0x1 : 0;

		err = target_command_add(&cmd, &cmd_size, mov_a_dptr, sizeof(mov_a_dptr));
		noerr_or_out(err);
		err = target_command_add(&cmd, &cmd_size, inc_dptr, sizeof(inc_dptr));
		noerr_or_out(err);
	}

	err = target_command_finalize(&cmd, &cmd_size);
	noerr_or_out(err);

	err = bulk_transfer(ctx, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);

	err = bulk_transfer(ctx, USB_IN, data, size);
	noerr_or_out(err);

	err = 0;

out:
	if (cmd) {
		free(cmd);
	}

	return err;
}

static int target_write_memory(
	ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size)
{
	int err = 1;
	void *cmd = NULL;
	int cmd_size;

	static uint8_t mov_dptr_addr16[] = {
		0xbe, 0x57,
		0x90, 0x0, 0x0
	};

	static uint8_t mov_a_data[] = {
		0x8e, 0x56,
		0x74, 0x0
	};

	static uint8_t mov_dptr_a[] = {
		0x5e, 0x55,
		0xf0
	};

	static uint8_t inc_dptr[] = {
		0x5e, 0x55,
		0xa3
	};

	err = target_command_init(&cmd, &cmd_size);
	noerr_or_out(err);

	mov_dptr_addr16[4] = addr & 0xff;
	mov_dptr_addr16[3] = addr >> 8;
	err = target_command_add(&cmd, &cmd_size, mov_dptr_addr16, sizeof(mov_dptr_addr16));
	noerr_or_out(err);

	for (int i = 0; i < size; i++) {
		mov_a_data[3] = data[i];

		err = target_command_add(&cmd, &cmd_size, mov_a_data, sizeof(mov_a_data));
		noerr_or_out(err);
		err = target_command_add(&cmd, &cmd_size, mov_dptr_a, sizeof(mov_dptr_a));
		noerr_or_out(err);
		err = target_command_add(&cmd, &cmd_size, inc_dptr, sizeof(inc_dptr));
		noerr_or_out(err);
	}

	err = target_command_finalize(&cmd, &cmd_size);
	noerr_or_out(err);

	err = bulk_transfer(ctx, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);

	err = 0;

out:
	if (cmd) {
		free(cmd);
	}

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

int ccd_fw_info(ccd_ctx_t *ctx, ccd_fw_info_t *info)
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
	if (state != 0) {
		error_out("Bad state %d", state);
	}

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

int ccd_target_info(ccd_ctx_t *ctx, ccd_target_info_t *info)
{
	int err;
	uint8_t chip_id;
	uint8_t chip_version;
	uint16_t chip_info;
	enum {
		flash_size_mask  = 0x0070,
		flash_size_shift = 4,
		sram_size_mask   = 0x0700,
		sram_size_shift  = 8,
	};

	err = ccd_read_memory(
		ctx, MEM_CHIP_ID,
		&chip_id, sizeof(chip_id));
	noerr_or_out(err);

	err = ccd_read_memory(
		ctx, MEM_CHIP_VERSION,
		&chip_version, sizeof(chip_version));
	noerr_or_out(err);

	err = ccd_read_memory(
		ctx, MEM_CHIP_INFO,
		&chip_info, sizeof(chip_info));
	noerr_or_out(err);

	info->chip_id = chip_id;
	info->chip_version = chip_version;
	info->flash_size =
		1 << (4 + ((chip_info & flash_size_mask) >> flash_size_shift));
	info->sram_size = 
		((chip_info & sram_size_mask) >> sram_size_shift) + 1;

out:
	return err;
}

int ccd_read_memory(ccd_ctx_t *ctx, uint16_t addr, void *data, int size)
{
	int err;

	err = reset_debug(ctx);
	noerr_or_out(err);

	err = target_read_memory(ctx, addr, data, size);

	err = reset(ctx);
	noerr_or_out(err);

out:
	return err;
}

int ccd_write_memory(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size)
{
	int err;

	err = reset_debug(ctx);
	noerr_or_out(err);

	err = target_write_memory(ctx, addr, data, size);

	err = reset(ctx);
	noerr_or_out(err);

out:
	return err;
}

#pragma mark App

static int repl_loop(ccd_ctx_t *ctx)
{
	int err = 1;
	size_t size;
	int nchars;
	char *line = NULL;
	char *token = NULL;

	do {
		line = NULL;

		printf("> ");
		nchars = getline(&line, &size, stdin);
		if (nchars < 0) {
			error_out("Can't read line\n");
		}
		line[nchars-1] = '\0';

		token = strtok(line, " ");
		if (!token) {
			// No command
		}
		else if (strcmp("quit", token) == 0) {
			printf("kthxbye\n");
			break;
		}
		else if (strcmp("erase", token) == 0) {
			err = ccd_erase_flash(ctx);
			noerr_or_out(err);

			printf("Flash erased\n");
		}
		else if (strcmp("read", token) == 0) {
			token = strtok(NULL, " ");
			if (!token) {
				printf("No address provided\n");
			}
			else {
				char *endptr;
				uint8_t byte;
				long addr;

				addr = strtol(token, &endptr, 0);
				if (addr < 0 || addr > 0xffff) {
					printf("Address not in range\n");
				}
				else if (strtok(NULL, " ")) {
					printf("Bad format\n");
				}
				else {
					err = ccd_read_memory(ctx, addr, &byte, sizeof(byte));
					noerr_or_out(err);

					printf("%04lx: %02x\n", addr, byte);
				}
			}
		}
		else if (strcmp("write", token) == 0) {
			token = strtok(NULL, " ");
			if (!token) {
				printf("No address provided\n");
			}
			else {
				char *endptr;
				long byte;
				long addr;

				addr = strtol(token, &endptr, 0);
				if (addr < 0 || addr > 0xffff) {
					printf("Address not in range\n");
				}
				else {
					token = strtok(NULL, " ");
					if (!token) {
						printf("Bad format\n");
					}
					else {
						byte = strtol(token, &endptr, 0);
						if (byte < 0 || byte > 0xff) {
							printf("Byte not in range\n");
						}
						else if (strtok(NULL, " ")) {
							printf("Bad format\n");
						}
						else {
							err = ccd_write_memory(ctx, addr, &byte, 1);
							noerr_or_out(err);
						}
					}
				}
			}
		}
		else {
			printf("Bad command\n");
		}

		free(line);
	} while (1);

	err = 0;

out:
	if (line) {
		free(line);
	}

	return err;
}

typedef struct {
	int verbose;
	int erase;
	int info;
	int repl;
} options_t;

static int parse_options(options_t *options, int argc, char * const *argv)
{
	int err = 0;
	int print_usage = 0;
	static struct option long_options[] =
	{
		{"help",    no_argument,       0, 'h'},
		{"verbose", no_argument,       0, 'v'},
		{"erase",   no_argument,       0, 'e'},
		{"info",    no_argument,       0, 'i'},
		{"repl",    no_argument,       0, 'r'},
		{0, 0, 0, 0}
	};

	bzero(options, sizeof(options_t));

	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "vheir", long_options, &option_index);

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
			case 'i':
				options->info = 1;
				break;
			case 'r':
				options->repl = 1;
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
		printf("  -h, --help    \tPrint this help\n");
		printf("  -v, --verbose \tVerbose mode\n");
		printf("  -e, --erase   \tErase flash\n");
		printf("  -i, --info    \tGet target info\n");
		printf("  -r, --repl    \tLaunch REPL\n");

		err = 1;
	}

	return err;
}

int main(int argc, char * const *argv)
{
	int err;
	ccd_ctx_t ctx;
	ccd_fw_info_t fw_info;
	options_t options;

	err = parse_options(&options, argc, argv);
	if (err) {
		goto out_parse;
	}

	printf("Looking for CC-Debugger... \n");

	if (options.verbose) {
		log_set(1);
	}

	err = ccd_open(&ctx);
	if (err) {
		goto out;
	}

	printf("Querying firmware... \n");
	fflush(stdout);

	err = ccd_fw_info(&ctx, &fw_info);
	noerr_or_out(err);

	printf("Version 0x%04x (rev 0x%04x)\n", fw_info.fw_id, fw_info.fw_rev);

	if (fw_info.chip == 0) {
		error_out("No target found\n");
	}

	printf("Found CC%x target\n", fw_info.chip);

	if (options.info) {
		ccd_target_info_t target_info;
		err = ccd_target_info(&ctx, &target_info);
		noerr_or_out(err);

		printf("Chip ID: 0x%x\n", target_info.chip_id);
		printf("Chip version: %d\n", target_info.chip_version);
		printf("Flash size: %d KB\n", target_info.flash_size);
		printf("SRAM size: %d KB\n", target_info.sram_size);
	}

	if (options.erase) {
		err = ccd_erase_flash(&ctx);
		noerr_or_out(err);
	}

	if (options.repl) {
		err = repl_loop(&ctx);
		noerr_or_out(err);
	}

	err = 0;

out:
	ccd_close(&ctx);
out_parse:
	return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
