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

#ifndef CCD_H
#define CCD_H

#include "usb.h"
#include "tools.h"

typedef struct ccd_ctx_t {
	usb_ctx_t *usb;
} ccd_ctx_t;

ccd_ctx_t *ccd_open(void);
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

err_t ccd_enter_debug(ccd_ctx_t *ctx, int slow_mode);
err_t ccd_leave_debug(ccd_ctx_t *ctx);

err_t ccd_fw_info(ccd_ctx_t *ctx, ccd_fw_info_t *info);
err_t ccd_target_info(ccd_ctx_t *ctx, ccd_target_info_t *info);
err_t ccd_reset(ccd_ctx_t *ctx);
err_t ccd_erase(ccd_ctx_t *ctx);

err_t ccd_read_xdata(ccd_ctx_t *ctx, uint16_t addr, void *data, int size);
err_t ccd_write_xdata(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size);
err_t ccd_write_code(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size);

#endif
