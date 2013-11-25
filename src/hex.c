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

#include <stdio.h>
#include <strings.h>

#include "hex.h"
#include "tools.h"

static int hexchars2int(const char *chars, int size)
{
	char *numberstr;
	char *endptr;
	int number;

	numberstr = strndup((char *)chars, size);
	number = strtol(numberstr, &endptr, 16);
	free(numberstr);

	return number;
}

static uint8_t checksum(const char *data, int size)
{
	uint8_t sum = 0;

	for (int i = 0; i < size; i++, data+=2) {
		sum += hexchars2int(data, 2);
	}

	sum ^= 0xff;
	sum += 0x01;

	return sum;
}

static char *get_hex_line(FILE *fp)
{
	char *line = NULL;
	size_t size;
	int nchars;

	nchars = getline(&line, &size, fp);
	if (nchars < 0) {
		line = NULL;
		goto out;
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

out:
	return line;
}

err_t hex_parse(ccd_ctx_t *ctx, FILE *fp)
{
	/* 
	 *    HEX format
	 * 
	 * :CCAAAATTD....DSS
	 * CC    -> Byte count
	 * AAAA  -> Address
	 * TT    -> Record type	
	 * D...D -> Data
	 * SS    -> Checksum
	 */

	err_t err = err_failed;
	char *line = NULL;
	static uint8_t buffer[1 << 16];
	uint16_t address_min = (1 << 16) - 1;
	uint16_t address_max = 0;
	const int min_hex_size = 11;
	char *curchar = NULL;
	int line_len = 0;
	uint8_t bytecount = 0;
	uint16_t address_low = 0;
	uint16_t address_high = 0;
	int block_size = 0;
	enum {
		state_getline,
		state_colon,
		state_bytecount,
		state_address,
		state_recordtype,
		state_data,
		state_xaddr,
		state_checksum,
		state_checksum_eof,
		state_done,
	} state;

	state = state_getline;

	while (state != state_done) {
		uint8_t sum;

		switch (state) {
		case state_getline:
			line = get_hex_line(fp);
			if (!line) {
				state = state_done;
			}
			else {
				line_len = strlen(line);
				curchar = line;
				state = state_colon;
			}
			break;

		case state_colon:
			if (line_len < min_hex_size) {
				error_out("HEX line is too short\n");
			}
			if (*curchar != ':') {
				error_out("HEX line doesn't start with ':'\n");
			}
			curchar++;
			state = state_bytecount;
			break;

		case state_bytecount:
			bytecount = hexchars2int(curchar, 2);
			if (bytecount * 2 + min_hex_size != line_len) {
				error_out("Bad HEX byte count\n");
			}
			curchar += 2;
			state = state_address;
			break;

		case state_address:
			address_low = hexchars2int(curchar, 4);
			curchar += 4;
			if (address_low < address_min) {
				address_min = address_low;
			}
			if (address_low + bytecount > address_max) {
				address_max = address_low + bytecount;
			}
			state = state_recordtype;
			break;

		case state_recordtype:
			switch (hexchars2int(curchar, 2)) {
			case 0:
				state = state_data;
				break;
			case 1:
				if (bytecount != 0) {
					error_out("Bad HEX End Of File record\n");
				}
				state = state_checksum_eof;
				break;
			case 2:
			case 3:
				error_out("HEX Segment Address Record not supported\n");
				break;
			case 4:
				state = state_xaddr;
			case 5:
				error_out("HEX Start Linear Address Record not supported\n");
				break;
			default:
				error_out("Unknown HEX Record type\n");
			}
			curchar += 2;
			break;

		case state_data:
			if (address_high != 0) {
				error_out("Extended Linear Address not supported\n");
			}

			for (int i = 0; i < bytecount; i++) {
				uint8_t byte = hexchars2int(curchar, 2);
				curchar += 2;
				buffer[address_low] = byte;
				address_low++;
			}

			state = state_checksum;
			break;

		case state_xaddr:
			if (bytecount != 2) {
				error_out("Bad HEX Extended Record\n");
			}
			address_high = hexchars2int(curchar, 4);
			curchar += 4;
			state = state_checksum;
			break;

		case state_checksum:
		case state_checksum_eof:
			sum = checksum(line + 1, (line_len - 3) / 2);
			if (sum != hexchars2int(curchar, 2)) {
				error_out("HEX checksum doesn't match\n");
			}
			free(line);
			line = NULL;
			
			if (state == state_checksum_eof) {
				state = state_done;
			}
			else {
				state = state_getline;
			}
			break;

		case state_done:
			break;

		default:
			error_out("HEX parsing failed\n");
		}
	}

	log_print("[HEX] Found %dB of code starting at 0x%04x\n", address_max - address_min, address_min);

	block_size = address_max - address_min;
	block_size += (block_size % 4) ? 4 - block_size % 4 : 0;
	err = ccd_write_code(ctx, address_min, buffer, block_size);
	noerr_or_out(err);

out:
	if (line) {
		free(line);
	}

	return err;
}

err_t hex_flash(ccd_ctx_t *ctx, const char *file)
{
	err_t err = err_failed;
	FILE *fp = NULL;

	fp = fopen(file, "r");
	if (!fp) {
		error_out("Can't open %s\n", file);
	}

	err = hex_parse(ctx, fp);
	noerr_or_out(err);

out:
	if (fp) {
		fclose(fp);
	}
	return err;
}
