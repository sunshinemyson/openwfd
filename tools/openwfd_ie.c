/*
 * OpenWFD - Open-Source Wifi-Display Implementation
 *
 * Copyright (c) 2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <endian.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openwfd/wfd.h>

static size_t indent = 0;
static FILE *outfile = NULL;

static void print_line(const char *format, ...)
{
	va_list args;
	size_t i;

	for (i = 0; i < indent; ++i)
		fprintf(outfile, "    ");

	va_start(args, format);
	vfprintf(outfile, format, args);
	va_end(args);
	fprintf(outfile, "\n");
}

static void print_err(const char *format, ...)
{
	va_list args;

	fprintf(outfile, "ERROR: ");
	va_start(args, format);
	vfprintf(outfile, format, args);
	va_end(args);
	fprintf(outfile, "\n");
}

static void indent_in(void)
{
	++indent;
}

static void indent_out(void)
{
	--indent;
}

static void print_sub_dev_info(const struct openwfd_wfd_ie_sub_header *h)
{
	const struct openwfd_wfd_ie_sub_dev_info *sub;

	if (be16toh(h->length) != 6) {
		print_err("invalid sub-length %u",
			  (unsigned int)be16toh(h->length));
		return;
	}

	sub = (void*)h;
	print_line("dev_info: %x", be16toh(sub->dev_info));
	print_line("ctrl_port: %u", be16toh(sub->ctrl_port));
	print_line("max_throughput: %u", be16toh(sub->max_throughput));
}

static void print_sub(const struct openwfd_wfd_ie_sub_header *h)
{
	print_line("subelement_id: 0x%x",
		   (unsigned int)h->subelement_id);
	print_line("length: %u",
		   (unsigned int)be16toh(h->length));

	switch (h->subelement_id) {
	case OPENWFD_WFD_IE_SUB_DEV_INFO:
		print_line("type: DEVICE INFO");
		print_sub_dev_info(h);
		break;
	default:
		print_line("unsupported sub-element ID %x",
			   (unsigned int)h->subelement_id);
		break;
	}
}

static void print_ie(const struct openwfd_wfd_ie *ies, size_t num)
{
	const struct openwfd_wfd_ie *ie;
	const struct openwfd_wfd_ie_sub_header *h;
	const uint8_t *data;
	uint8_t *t;
	size_t length, l;

	print_line("WFD IE:");

	if (!num) {
		print_line("empty IE");
		return;
	}

	indent_in();
	ie = ies;

	print_line("element_id: 0x%x", (unsigned int)ie->element_id);
	print_line("length: %u", (unsigned int)ie->length);

	data = ie->data;
	length = ie->length;
	if (length < 4) {
		print_err("invalid IE, length < 4");
		goto error;
	}

	print_line("oui: 0x%x", (unsigned int)be32toh(ie->oui));
	length -= 4;

	while (length) {
		if (length < 3) {
			print_err("invalid IE, rest-length < 3 (%u)",
				  length);
			goto error;
		}

		h = (void*)data;
		l = be16toh(h->length);

		print_line("subelement:");

		t = malloc(l + 3);
		if (!t) {
			print_err("out of memory for subelement of size %u",
				   (unsigned int)l);
			goto error;
		}

		if (l > length) {
			print_err("multi-IE sub-elements not yet supported");
			goto error;
		}

		memcpy(t, data, l + 3);
		length -= l + 3;
		data += l + 3;

		indent_in();
		print_sub((void*)t);
		indent_out();
	}

error:
	indent_out();
	print_line("");
}

static void print_hex(const uint8_t *d, size_t len)
{
	size_t i;

	fprintf(outfile, "IE hex (len: %u):\n", (unsigned int)len);

	for (i = 0; i < len; ++i) {
		fprintf(outfile, " %02x", (unsigned int)d[i]);
		if (i && !((i + 1) % 16))
			fprintf(outfile, "\n");
	}
	if (i % 16)
		fprintf(outfile, "\n");

	fprintf(outfile, "END of IE\n");
}

int main(int argc, char **argv)
{
	struct openwfd_wfd_ie ie;
	struct openwfd_wfd_ie_sub_dev_info *sub;

	outfile = stdout;

	memset(&ie, 0, sizeof(ie));
	ie.element_id = OPENWFD_WFD_IE_ID;
	ie.length = 4;
	ie.oui = htobe32(OPENWFD_WFD_IE_OUI_1_0);

	sub = (void*)ie.data;
	ie.length += 9;
	sub->header.subelement_id = OPENWFD_WFD_IE_SUB_DEV_INFO;
	sub->header.length = htobe16(6);
	sub->dev_info = htobe16(
			OPENWFD_WFD_IE_SUB_DEV_INFO_PRIMARY_SINK |
			OPENWFD_WFD_IE_SUB_DEV_INFO_SRC_NO_COUPLED_SINK |
			OPENWFD_WFD_IE_SUB_DEV_INFO_SINK_NO_COUPLED_SINK |
			OPENWFD_WFD_IE_SUB_DEV_INFO_AVAILABLE |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_WSD |
			OPENWFD_WFD_IE_SUB_DEV_INFO_PREFER_P2P |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_CP |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_TIME_SYNC |
			OPENWFD_WFD_IE_SUB_DEV_INFO_CAN_AUDIO |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_AUDIO_ONLY |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_PERSIST_TLDS |
			OPENWFD_WFD_IE_SUB_DEV_INFO_NO_TLDS_REINVOKE
		);
	sub->ctrl_port = htobe16(OPENWFD_WFD_IE_SUB_DEV_INFO_DEFAULT_PORT);
	sub->max_throughput = htobe16(200);

	print_ie(&ie, 1);
	print_hex((uint8_t*)&ie, ie.length + 2);

	return 0;
}
