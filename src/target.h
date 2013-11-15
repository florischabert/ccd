#ifndef TARGET_H
#define TARGET_H

#include "ccd.h"
#include "tools.h"

enum {
	MEM_CHIP_VERSION = 0x6249,
	MEM_CHIP_ID      = 0x624a,
	MEM_CHIP_INFO    = 0x6276,
};

enum {
	TARGET_RD_HDR      = 0x1f,
	TARGET_RD_CONFIG   = 0x24,
	TARGET_RD_STATUS   = 0x34,

	TARGET_WR_HDR      = 0x4c,
	TARGET_WR_CONFIG   = 0x1d,

	TARGET_ERASE_HDR   = 0x1c,
	TARGET_CHIP_ERASE  = 0x14,

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

err_t target_read_config(ccd_ctx_t *ctx, uint8_t *config);
err_t target_write_config(ccd_ctx_t *ctx, uint8_t config);
err_t target_read_status(ccd_ctx_t *ctx, uint8_t *status);
err_t target_erase(ccd_ctx_t *ctx);

err_t target_command_add(void **cmd, int *size, void *data, int data_size);
err_t target_command_init(void **cmd, int *size);
err_t target_command_finalize(void **cmd, int *size);

err_t target_read_memory(
	ccd_ctx_t *ctx, uint16_t addr, uint8_t *data, int size);
err_t target_write_memory(
	ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size);

#endif
