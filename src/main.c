#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>

#include "tools.h"
#include "ccd.h"
#include "hex.h"

typedef struct {
	int verbose;
	int info;
	int erase;
	int verify;
	int slow;
	char *hex_file;
} options_t;

static err_t parse_options(options_t *options, int argc, char * const *argv)
{
	err_t err = err_none;
	int print_usage = 0;
	static struct option long_options[] =
	{
		{"help",    no_argument,       0, 'h'},
		{"verbose", no_argument,       0, 'V'},
		{"info",    no_argument,       0, 'i'},
		{"erase",   no_argument,       0, 'e'},
		{"verify",  no_argument,       0, 'v'},
		{"hex",     required_argument, 0, 'x'},
		{"slow",    required_argument, 0, 's'},
		{0, 0, 0, 0}
	};

	bzero(options, sizeof(options_t));

	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "hViex:vs", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c)
		{
			case 'V':
				options->verbose = 1;
				break;
			case 'i':
				options->info = 1;
				break;
			case 'e':
				options->erase = 1;
				break;
			case 'v':
				options->verify = 1;
				break;
			case 'x':
				options->hex_file = optarg;
				break;
			case 's':
				options->slow = 1;
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
		printf("  -h, --help           \tPrint this help\n");
		printf("      --verbose        \tVerbose mode\n");
		printf("  -i, --info           \tPrint target info\n");
		printf("  -e, --erase          \tErase flash\n");
		printf("  -x, --hex <filename> \tWrite HEX file to flash\n");
		printf("  -v, --verify         \tVerify after write\n");
		printf("  -s, --slow           \tSlow mode\n");

		err = err_failed;
	}

	if (options->verify && !options->hex_file) {
		printf("Can't verify: no file was given\n");
		err = err_failed;
	}

	return err;
}

int main(int argc, char * const *argv)
{
	err_t err;
	ccd_ctx_t *ctx = NULL;
	ccd_fw_info_t fw_info;
	options_t options;

	err = parse_options(&options, argc, argv);
	if (err) {
		goto out_parse;
	}

	if (options.verbose) {
		log_set(1);
	}

	ctx = ccd_open();
	if (!ctx) {
		goto out;
	}

	err = ccd_fw_info(ctx, &fw_info);
	noerr_or_out(err);

	printf("CC-Debugger: FW 0x%04x rev 0x%04x\n", fw_info.fw_id, fw_info.fw_rev);

	if (fw_info.chip == 0) {
		error_out("No target found\n");
	}

	printf("Target: CC%x\n", fw_info.chip);

	err = ccd_enter_debug(ctx, options.slow);
	noerr_or_out(err);

	if (options.info) {
		ccd_target_info_t target_info;
		err = ccd_target_info(ctx, &target_info);
		noerr_or_out(err);

		printf(" Chip ID: 0x%x\n", target_info.chip_id);
		printf(" Chip version: %d\n", target_info.chip_version);
		printf(" Flash size: %d KB\n", target_info.flash_size);
		printf(" SRAM size: %d KB\n", target_info.sram_size);
	}

	if (options.erase) {
		err = ccd_erase(ctx);
		noerr_or_out(err);
	}

	if (options.hex_file) {
		err = hex_flash(ctx, options.hex_file);
		noerr_or_out(err);
	}

	if (options.verify) {
		// err = hex_flash(ctx, options.hex_file);
		// noerr_or_out(err);
	}

	err = ccd_leave_debug(ctx);
	noerr_or_out(err);

out:
	ccd_close(ctx);
out_parse:
	return err ? EXIT_FAILURE : EXIT_SUCCESS;
}