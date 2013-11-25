#include <string.h>

#include "target.h"
#include "usb.h"

err_t target_read_config(ccd_ctx_t *ctx, uint8_t *config)
{
	err_t err;
	uint8_t cmd[] = { TARGET_RD_HDR, TARGET_RD_CONFIG };

	log_print("[Target] Read config\n");

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = usb_bulk_transfer(ctx->usb, USB_IN, config, sizeof(*config));
	noerr_or_out(err);

	log_print("[Target] config is 0x%02x\n", *config);

out:
	return err;
}

err_t target_write_config(ccd_ctx_t *ctx, uint8_t config)
{
	uint8_t cmd[] = { TARGET_WR_HDR, TARGET_WR_CONFIG, 0 };

	cmd[2] = config;

	log_print("[Target] Write config 0x%02x\n", config);

	return usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
}

err_t target_read_status(ccd_ctx_t *ctx, uint8_t *status)
{
	err_t err;
	uint8_t cmd[] = { TARGET_RD_HDR, TARGET_RD_STATUS };

	log_print("[Target] Read status\n");

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = usb_bulk_transfer(ctx->usb, USB_IN, status, sizeof(*status));
	noerr_or_out(err);

	log_print("[Target] status is 0x%02x\n", *status);

out:
	return err;
}

err_t target_erase(ccd_ctx_t *ctx)
{
	uint8_t cmd[] = { TARGET_ERASE_HDR, TARGET_CHIP_ERASE };

	log_print("[Target] Erase flash\n");

	return usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
}

err_t target_command_add(void **cmd, int *size, void *data, int data_size)
{
	err_t err = err_failed;

	*cmd = realloc(*cmd, *size + data_size);
	if (*cmd == NULL) {
		error_out("Can't allocate memory\n");
	}

	memcpy((uint8_t*)*cmd + *size, data, data_size);
	*size += data_size;

	err = err_none;

out:
	return err;	
}

err_t target_command_init(void **cmd, int *size)
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

err_t target_command_finalize(void **cmd, int *size)
{
	static uint8_t footer[] = { 
		0xd4, 0x57, 0x90, 0xc2, 0x57, 0x75, 0x92, 0x90,
		0x56, 0x74
	};

	return target_command_add(cmd, size, footer, sizeof(footer));
}

err_t target_read_xdata(ccd_ctx_t *ctx, uint16_t addr, uint8_t *data, int size)
{
	err_t err = err_failed;
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

	log_print("[Target] Read %dB of xdata at 0x%04x\n", size, addr);

	err = target_command_init(&cmd, &cmd_size);
	noerr_or_out(err);

	mov_dptr_addr16[4] = addr & 0xff;
	mov_dptr_addr16[3] = addr >> 8;
	err = target_command_add(&cmd, &cmd_size, mov_dptr_addr16, sizeof(mov_dptr_addr16));
	noerr_or_out(err);

	for (int i = 0; i < size; i++) {
		mov_a_dptr[0] = (i == size-1) ? 0x4f : 0x4e;

		err = target_command_add(&cmd, &cmd_size, mov_a_dptr, sizeof(mov_a_dptr));
		noerr_or_out(err);
		err = target_command_add(&cmd, &cmd_size, inc_dptr, sizeof(inc_dptr));
		noerr_or_out(err);
	}

	err = target_command_finalize(&cmd, &cmd_size);
	noerr_or_out(err);

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);

	err = usb_bulk_transfer(ctx->usb, USB_IN, data, size);
	noerr_or_out(err);

	err = err_none;

out:
	if (cmd) {
		free(cmd);
	}

	return err;
}

err_t target_write_xdata(ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size)
{
	err_t err = err_failed;
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

	log_print("[Target] Write %dB of xdata at 0x%04x\n", size, addr);
	log_bytes(data, size);

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

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, cmd_size);
	noerr_or_out(err);

	err = err_none;

out:
	if (cmd) {
		free(cmd);
	}

	return err;
}

static err_t flag_set(ccd_ctx_t *ctx, uint16_t address, uint8_t flag)
{
	err_t err;
	uint8_t flash_ctrl;

	log_print("[Target] Flash set flag 0x%02x\n", flag);

	err = target_read_xdata(ctx, address, &flash_ctrl, sizeof(flash_ctrl));
	noerr_or_out(err);

	flash_ctrl |= flag;
	err = target_write_xdata(ctx, address, &flash_ctrl, sizeof(flash_ctrl));
	noerr_or_out(err);

out:
	return err;
}

static err_t flag_wait_cleared(ccd_ctx_t *ctx, uint16_t address, uint8_t flag)
{
	err_t err;
	uint8_t byte = 0;

	log_print("[Target] Flash wait for flag 0x%02x cleared\n", flag);

	do {
		if (byte) {
			usleep(200);
		}

		err = target_read_xdata(ctx, address, &byte, sizeof(byte));
		noerr_or_out(err);
	} while (byte & flag);

out:
	return err;
}

typedef struct {
	int is_dma0;
	uint8_t configs[4][8];
} dma_config_t;

static void dma_config_init(ccd_ctx_t *ctx, dma_config_t *config)
{
	(void)ctx;
	config->is_dma0 = -1;
}

static err_t dma_config_channel(
	ccd_ctx_t *ctx, dma_config_t *config, int channel,
	uint16_t srcaddr, int incsrc, uint16_t dstaddr, int incdst,
	int size, int dma_trigger, int dma_tmode)
{
	err_t err = err_failed;
	(void)ctx;

	if (config->is_dma0 != -1) {
		if ((channel == 0 && !config->is_dma0) ||
		    (channel != 0 && config->is_dma0)) {
			error_out("Can't use DMA0 and DMA1-4 with the same config\n");
		}
	}

	config->is_dma0 = channel == 0;

	if (channel != 0) {
		channel--;
	}

	config->configs[channel][0] = srcaddr >> 8;
	config->configs[channel][1] = srcaddr & 0xff;
	config->configs[channel][2] = dstaddr >> 8;
	config->configs[channel][3] = dstaddr & 0xff;
	config->configs[channel][4] = size >> 8;
	config->configs[channel][5] = size & 0xff;
	config->configs[channel][6] = dma_tmode | dma_trigger;
	config->configs[channel][7] = DMA_PRIO_HIGH;
	config->configs[channel][7] |= incsrc ? DMA_SRC_INC_1 : 0;
	config->configs[channel][7] |= incdst ? DMA_DST_INC_1 : 0;

	err = err_none;

out:
	return err;	
}

static err_t dma_config_commit(
	ccd_ctx_t *ctx, dma_config_t *config, uint16_t temp_addr)
{
	err_t err = err_failed;
	uint8_t val;
	int dma_addr_low = config->is_dma0 ? DMA0_ADDR_LOW : DMA14_ADDR_LOW;
	int dma_addr_high = config->is_dma0 ? DMA0_ADDR_HIGH : DMA14_ADDR_HIGH;

	if (config->is_dma0 == -1) {
		error_out("Can't commit before DMA config is done\n");
	}

	err = target_write_xdata(ctx, temp_addr, (uint8_t *)config->configs, sizeof(config->configs));
	noerr_or_out(err);

	val = temp_addr & 0xff;
	err = target_write_xdata(ctx, dma_addr_low, &val, sizeof(val));
	noerr_or_out(err);

	val = temp_addr >> 8;
	err = target_write_xdata(ctx, dma_addr_high, &val, sizeof(val));
	noerr_or_out(err);

out:
	return err;
}

static err_t dma_arm(ccd_ctx_t *ctx, int channel)
{
	err_t err;
	uint8_t val;

	log_print("[Target] Arm dma channel %d\n", channel);

	val = 1 << channel;
	err = target_write_xdata(ctx, DMA_ARM, &val, sizeof(val));
	noerr_or_out(err);

out:
	return err;
}

static err_t dma_request(ccd_ctx_t *ctx, int channel)
{
	err_t err;
	uint8_t val;

	log_print("[Target] Request DMA on channel %d\n", channel);

	val = 1 << channel;
	err = target_write_xdata(ctx, DMA_REQ, &val, sizeof(val));
	noerr_or_out(err);

out:
	return err;
}

static err_t dma_wait_completion(ccd_ctx_t *ctx, int channel)
{
	err_t err;

	log_print("[Target] Wait for DMA completion on channel %d\n", channel);

	err = flag_wait_cleared(ctx, DMA_IRQ, 1 << channel);
	noerr_or_out(err);

out:
	return err;
}

static err_t rng_seed(ccd_ctx_t *ctx, uint16_t seed)
{
	err_t err;
	uint8_t val;

	log_print("[Target] Set RNG seed to 0x%02x\n", seed);

	val = seed >> 8;
	err = target_write_xdata(ctx, RNG_DATA_LOW, &val, sizeof(val));
	noerr_or_out(err);

	val = seed & 0xff;
	err = target_write_xdata(ctx, RNG_DATA_LOW, &val, sizeof(val));
	noerr_or_out(err);

out:
	return err;
}

static err_t rng_get_crc16(ccd_ctx_t *ctx, uint16_t *crc16)
{
	err_t err;
	uint8_t val;

	log_print("[Target] Get RNG CRC value\n");

	err = target_read_xdata(ctx, RNG_DATA_LOW, &val, sizeof(val));
	noerr_or_out(err);

	*crc16 = val;

	err = target_read_xdata(ctx, RNG_DATA_HIGH, &val, sizeof(val));
	noerr_or_out(err);

	*crc16 |= val << 8;

out:
	return err;
}

static err_t flash_setup(ccd_ctx_t *ctx, uint16_t addr)
{
	err_t err;
	uint8_t val;

	log_print("[Target] Flash setup at 0x%04x\n", addr);

	val = addr & 0xff;
	err = target_write_xdata(ctx, FLASH_ADDR_LOW, &val, sizeof(val));
	noerr_or_out(err);

	val = addr >> 8;
	err = target_write_xdata(ctx, FLASH_ADDR_HIGH, &val, sizeof(val));
	noerr_or_out(err);

out:
	return err;
}

static err_t burst_write(ccd_ctx_t *ctx, const uint8_t *data, int size)
{
	err_t err;
	uint8_t cmd[3] = { TARGET_BURST_HDR, TARGET_BURST_WRITE, 0x00 };

	cmd[1] |= size >> 8;
	cmd[2] = size & 0xff;

	log_print("[Target] Burst write %dB\n", size);

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, 3);
	noerr_or_out(err);

	err = usb_bulk_transfer(ctx->usb, USB_OUT, (void *)data, size);
	noerr_or_out(err);

out:
	return err;
}

err_t target_write_flash(ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size)
{
	err_t err = err_failed;
	const int block_size = 1024;
	const uint16_t temp_config_addr = 0x0800;
	const uint16_t temp_data_addr = 0x0000;
	static dma_config_t dma_config;

	log_print("[Target] Write %dB to flash at 0x%04x\n", size, addr);
	log_bytes(data, size);

	dma_config_init(ctx, &dma_config);

	if (size % 4) {
		error_out("Flash writing requires blocks of 4 bytes\n");
	}

	while (size) {
		int current_size = size;

		if (size > block_size) {
			current_size = block_size;
		}

		// DMA from usb burst write to temp address
		err = dma_config_channel(
			ctx, &dma_config, 1,
			DEBUG_WRITE_DATA, 0, temp_data_addr, 1,
			current_size, DMA_TRIG_DEBUG, DMA_TMODE_SINGLE);
		noerr_or_out(err);

		// DMA from temp address to flash
		err = dma_config_channel(
			ctx, &dma_config, 2,
			temp_data_addr, 1, FLASH_WRITE_DATA, 0,
			current_size, DMA_TRIG_FLASH, DMA_TMODE_SINGLE);
		noerr_or_out(err);

		// Start burst write DMA
		err = dma_config_commit(ctx, &dma_config, temp_config_addr);
		noerr_or_out(err);

		err = dma_arm(ctx, 1);
		noerr_or_out(err);

		err = burst_write(ctx, data, current_size);
		noerr_or_out(err);

		// Start Flash DMA
		err = flash_setup(ctx, addr);
		noerr_or_out(err);

		err = flag_wait_cleared(ctx, FLASH_CONTROL, FLASH_BUSY);
		noerr_or_out(err);

		err = dma_arm(ctx, 2);
		noerr_or_out(err);

		err = flag_set(ctx, FLASH_CONTROL, FLASH_WRITE);
		noerr_or_out(err);

		err = flag_wait_cleared(ctx, FLASH_CONTROL, FLASH_WRITE);
		noerr_or_out(err);

		size -= current_size;
	}

out:
	return err;
}

uint16_t compute_crc16(const uint8_t *data, int size, uint16_t init)
{
	uint16_t crc16 = init;

	for (int i = 0; i < size; i++) {
		uint8_t byte = *data++;
		for (int b = 7; b >= 0; b-- ) {
			uint8_t in_sum = ((crc16 & 0x8000) >> 15) ^ ((byte >> b) & 1);
			crc16 = (crc16 << 1) | in_sum;
			crc16 = crc16 ^ (in_sum << 15);
			crc16 = crc16 ^ (in_sum << 2);
		}
	}

	return crc16;
}

err_t target_verify_flash(ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size)
{
	err_t err = err_failed;
	const uint16_t seed = 0xffff;
	const uint16_t temp_config_addr = 0x0800;
	static dma_config_t dma_config;
	uint16_t crc16_target;
	uint16_t crc16_host;

	dma_config_init(ctx, &dma_config);

	// DMA from flash to RNG
	err = dma_config_channel(
		ctx, &dma_config, 0,
		XDATA_FLASH + addr, 1, RNG_DATA_HIGH, 0,
		size, 0, DMA_TMODE_BLOCK);
	noerr_or_out(err);

	err = dma_config_commit(ctx, &dma_config, temp_config_addr);
	noerr_or_out(err);

	err = rng_seed(ctx, seed);
	noerr_or_out(err);

	err = dma_arm(ctx, 0);
	noerr_or_out(err);

	err = dma_request(ctx, 0);
	noerr_or_out(err);

	err = dma_wait_completion(ctx, 4);
	noerr_or_out(err);

	err = rng_get_crc16(ctx, &crc16_target);
	noerr_or_out(err);

	crc16_host = compute_crc16(data, size, seed);

	if (crc16_host != crc16_target) {
		err = err_failed;
		error_out("Flashing failed: checksum mismatch (0x%04x != 0x%04x)\n", crc16_host, crc16_target);
	}

out:
	return err;
}
