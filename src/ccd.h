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
err_t ccd_read_code(ccd_ctx_t *ctx, uint16_t addr, void *data, int size);
err_t ccd_write_code(ccd_ctx_t *ctx, uint16_t addr, const void *data, int size);

#endif
