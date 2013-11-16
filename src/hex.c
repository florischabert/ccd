#include <stdio.h>
#include <strings.h>

#include "hex.h"
#include "tools.h"

err_t hex_flash(ccd_ctx_t *ctx, const char *file)
{
	err_t err = err_failed;
	FILE *fp = NULL;
	int line_number = 0;
	char *line = NULL;
	int end_of_file_record = 0;
	uint8_t *data;

	fp = fopen(file, "r");
	if (!fp) {
		error_out("Can't open %s\n", file);
	}

	do {
		size_t size;
		int nchars;
		char *curchar;
		char *number;
		char *endptr;
		uint8_t bytecount;
		uint16_t address_high = 0;
		uint16_t address;
		line = NULL;

		line_number++;
		nchars = getline(&line, &size, fp);
		if (nchars < 0) {
			break;
		}

		do {
			char c = line[nchars-1];
			if (c == '\r' || c == '\n') {
				line[nchars-1] = '\0';
				nchars--;
			}
			else {
				break;
			}
		} while (nchars > 0);

		curchar = line;

		if (end_of_file_record) {
			error_out("Found End Of Line Record before the end of the file\n");
		}

		// HEX format:
		// :CCAAAATTD....DSS
		// CC    -> Byte count
		// AAAA  -> Address
		// TT    -> Record type
		// D...D -> Data
		// SS    -> Checksum

		if (strlen(line) < 11) {
			error_out("Line %d is too short\n", line_number);
		}
		if (*curchar != ':') {
			error_out("No start code on line %d\n", line_number);
		}
		curchar++;

		// Byte count
		number = strndup(curchar, 2);
		if (!number) {
			error_out("Out of memory\n");
		}
		bytecount = strtol(number, &endptr, 16);
		free(number);
		if (bytecount * 2 + 11 != strlen(line)) {
			error_out("Wrong byte count on line %d\n", line_number);
		}
		curchar += 2;

		// Address
		number = strndup(curchar, 4);
		if (!number) {
			error_out("Out of memory\n");
		}
		address = strtol(number, &endptr, 16);
		free(number);
		curchar += 4;

		// Record type
		if (*curchar != '0') {
			error_out("Unknown record type on line %d\n", line_number);
		}
		curchar++;

		switch (*curchar++) {
			case '0':
				data = malloc(bytecount);
				for (int i = 0; i < bytecount; i++) {
					uint8_t byte;

					number = strndup(curchar, 2);
					if (!number) {
						error_out("Out of memory\n");
					}
					byte = strtol(number, &endptr, 16);
					free(number);
					curchar += 2;

					data[i] = byte;
				}

				if (address_high == 0) {
					err = ccd_write_code(ctx, address, data, bytecount);
					noerr_or_out(err);
				}
				else {

				}

				free(data);
				break;
			case '1':
				end_of_file_record = 1;
				if (bytecount != 0) {
					error_out("Bad End Of File record\n");
				}
				break;
			case '4':
				if (bytecount != 2) {
					error_out("Bad End Of File record\n");
				}
				number = strndup(curchar, 4);
				if (!number) {
					error_out("Out of memory\n");
				}
				address_high = strtol(number, &endptr, 16);
				free(number);
				break;
			case '2':
			case '3':
				error_out("Record type not supported\n");
				break;
			case '5':
				printf("Ignoring Start Linear Address Record\n");
				break;
			default:
				error_out("Unknown record type on line %d\n", line_number);
		}

		// TODO: checksum

		free(line);
	} while (1);

	err = err_none;
out:
	if (line) {
		free(line);
	}
	if (fp) {
		fclose(fp);
	}

	return err;
}
