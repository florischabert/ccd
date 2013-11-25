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

#include <stdlib.h>
#include <unistd.h>

#include "ccd.h"
#include "target.h"

static err_t get_state(ccd_ctx_t *ctx, uint8_t *state)
{
	log_print("[CCD] Get target state\n");
	return usb_control_transfer(ctx->usb, USB_IN, VENDOR_STATE, 0, 0, state, sizeof(*state));
}

static err_t set_speed(ccd_ctx_t *ctx, int fast_mode)
{
	log_print("[CCD] Set speed to %s\n", fast_mode ? "fast" : "slow");
	return usb_control_transfer(ctx->usb, USB_OUT, VENDOR_SET_SPEED, !fast_mode, 0, NULL, 0);
}

static err_t reset(ccd_ctx_t *ctx, int debug_mode)
{
	log_print("[CCD] Reset target%s\n", debug_mode ? " in debug mode" : "");
	return usb_control_transfer(ctx->usb, USB_OUT, VENDOR_RESET, 0, debug_mode, NULL, 0);
}

static err_t debug_enter(ccd_ctx_t *ctx)
{
	log_print("[CCD] Enter target debug\n");
	return usb_control_transfer(ctx->usb, USB_OUT, VENDOR_DEBUG, 0, 0, NULL, 0);
}

ccd_ctx_t *ccd_open(void)
{
	ccd_ctx_t *ctx;

	log_print("[CCD] Open device\n");

	ctx = malloc(sizeof(*ctx));
	if (!ctx) {
		error_out("Can't allocate memory\n");
	}

	ctx->usb = usb_open_device(CCD_USB_VENDOR_ID, CCD_USB_PRODUCT_ID);

out:
	if (!ctx->usb) {
		free(ctx);
		ctx = NULL;
	}

	return ctx;
}

void ccd_close(ccd_ctx_t *ctx)
{
	log_print("[CCD] Close device\n");

	if (ctx) {
		usb_close_device(ctx->usb);
		free(ctx);
	}
}

err_t ccd_enter_debug(ccd_ctx_t *ctx, int slow_mode)
{
	err_t err;
	uint8_t state;
	uint8_t config;
	uint8_t cc_status;

	log_print("[CCD] Enter debug mode\n");

	err = get_state(ctx, &state);
	noerr_or_out(err);
	if (state != 0) {
		error_out("Bad state %d", state);
	}

	err = set_speed(ctx, slow_mode ? 0 : 1);
	noerr_or_out(err);
	err = reset(ctx, 1);
	noerr_or_out(err);
	err = debug_enter(ctx);
	noerr_or_out(err);

	err = target_read_config(ctx, &config);
	noerr_or_out(err);

	err = target_write_config(ctx, CONFIG_TIMER_SUSPEND | CONFIG_SOFT_POWER_MODE);
	noerr_or_out(err);

	err = target_read_status(ctx, &cc_status);
	noerr_or_out(err);

	if (cc_status & STATUS_DEBUG_LOCKED) {
		error_out("Target is locked\n");
	}

out:
	return err;
}

err_t ccd_leave_debug(ccd_ctx_t *ctx)
{
	int err;

	log_print("[CCD] Leave debug mode\n");

	err = reset(ctx, 0);
	noerr_or_out(err);

out:
	return err;
}

err_t ccd_reset(ccd_ctx_t *ctx)
{
	return reset(ctx, 0);
}

err_t ccd_fw_info(ccd_ctx_t *ctx, ccd_fw_info_t *info)
{
	log_print("[CCD] Get firmware info\n");
	return usb_control_transfer(ctx->usb, USB_IN, VENDOR_GET_INFO, 0, 0, info, sizeof(*info));
}

err_t ccd_erase(ccd_ctx_t *ctx)
{
	err_t err = err_failed;
	uint8_t cc_status;

	log_print("[CCD] Erase flash\n");

	err = target_erase(ctx);
	noerr_or_out(err);

	do {
		usleep(500);
		err = target_read_status(ctx, &cc_status);
		noerr_or_out(err);
	} while (cc_status & STATUS_ERASE_BUSY);

out:
	return err;
}

err_t ccd_target_info(ccd_ctx_t *ctx, ccd_target_info_t *info)
{
	err_t err = err_failed;
	uint8_t chip_id;
	uint8_t chip_version;
	uint16_t chip_info;
	enum {
		flash_size_mask  = 0x0070,
		flash_size_shift = 4,
		sram_size_mask   = 0x0700,
		sram_size_shift  = 8,
	};

	log_print("[CCD] Get target info\n");

	err = ccd_read_xdata(
		ctx, MEM_CHIP_ID,
		&chip_id, sizeof(chip_id));
	noerr_or_out(err);

	err = ccd_read_xdata(
		ctx, MEM_CHIP_VERSION,
		&chip_version, sizeof(chip_version));
	noerr_or_out(err);

	err = ccd_read_xdata(
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

err_t ccd_read_xdata(ccd_ctx_t *ctx, uint16_t addr, void *data, int size)
{
	err_t err = err_failed;

	log_print("[CCD] Read %dB at 0x%04x in data memory\n", size, addr);

	err = target_read_xdata(ctx, addr, data, size);
	noerr_or_out(err);

out:
	return err;
}

err_t ccd_write_xdata(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size)
{
	err_t err = err_failed;

	log_print("[CCD] Write %dB at 0x%04x in data memory\n", size, addr);

	err = target_write_xdata(ctx, addr, data, size);
	noerr_or_out(err);

out:
	return err;
}

err_t ccd_write_code(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size)
{
	err_t err = err_failed;

	log_print("[CCD] Write %dB at 0x%04x in code memory\n", size, addr);

	err = target_write_flash(ctx, addr, data, size);
	noerr_or_out(err);

	err = target_verify_flash(ctx, addr, data, size);
	noerr_or_out(err);

out:
	return err;
}
