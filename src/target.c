#include <string.h>

#include "target.h"
#include "usb.h"

err_t target_read_config(ccd_ctx_t *ctx, uint8_t *config)
{
	err_t err;
	uint8_t cmd[] = { TARGET_RD_HDR, TARGET_RD_CONFIG };

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = usb_bulk_transfer(ctx->usb, USB_IN, config, sizeof(*config));
	noerr_or_out(err);

out:
	return err;
}

err_t target_write_config(ccd_ctx_t *ctx, uint8_t config)
{
	uint8_t cmd[] = { TARGET_WR_HDR, TARGET_WR_CONFIG, 0 };

	cmd[2] = config;

	return usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
}

err_t target_read_status(ccd_ctx_t *ctx, uint8_t *status)
{
	err_t err;
	uint8_t cmd[] = { TARGET_RD_HDR, TARGET_RD_STATUS };

	err = usb_bulk_transfer(ctx->usb, USB_OUT, cmd, sizeof(cmd));
	noerr_or_out(err);
	
	err = usb_bulk_transfer(ctx->usb, USB_IN, status, sizeof(*status));
	noerr_or_out(err);

out:
	return err;
}

err_t target_erase(ccd_ctx_t *ctx)
{
	uint8_t cmd[] = { TARGET_ERASE_HDR, TARGET_CHIP_ERASE };

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

err_t target_read_memory(
	ccd_ctx_t *ctx, uint16_t addr, uint8_t *data, int size)
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

err_t target_write_memory(
	ccd_ctx_t *ctx, uint16_t addr, const uint8_t *data, int size)
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