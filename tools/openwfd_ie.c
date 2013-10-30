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

static void print_sub_dev_info(const struct openwfd_wfd_ie_sub *h,
			       const struct openwfd_wfd_ie_sub_dev_info *p)
{
	if (be16toh(h->length) != 6) {
		print_err("invalid sub-length %u",
			  (unsigned int)be16toh(h->length));
		return;
	}

	print_line("dev_info: %x", be16toh(p->dev_info));
	print_line("ctrl_port: %u", be16toh(p->ctrl_port));
	print_line("max_throughput: %u", be16toh(p->max_throughput));
}

static void print_sub(const struct openwfd_wfd_ie_sub *sub, void *data)
{
	print_line("subelement_id: 0x%x",
		   (unsigned int)sub->subelement_id);
	print_line("length: %u",
		   (unsigned int)be16toh(sub->length));

	switch (sub->subelement_id) {
	case OPENWFD_WFD_IE_SUB_DEV_INFO:
		print_line("type: DEVICE INFO");
		print_sub_dev_info(sub, data);
		break;
	default:
		print_line("unknown sub-element ID %x",
			   (unsigned int)sub->subelement_id);
		break;
	}
}

static void print_ie(const void *data, size_t len)
{
	const struct openwfd_wfd_ie *ie;
	const struct openwfd_wfd_ie_sub *sub;
	const void *h;
	void *col, *c;
	size_t l, sl;

	print_line("IE:");

	indent_in();

	if (!len) {
		print_line("<empty>");
		indent_out();
		return;
	}

	col = NULL;
	while (len > 0) {
		ie = data;

		/* check for valid IE header length */
		if (len < 6) {
			print_err("remaining data too small (%u < 6)",
				  (unsigned int)len);
			goto error;
		}

		/* print IE header */
		print_line("IE BLOCK:");
		indent_in();

		if (ie->element_id == OPENWFD_WFD_IE_ID)
			print_line("element_id: 0x%x (WFD)",
				   (unsigned int)ie->element_id);
		else
			print_line("element_id: 0x%x (UNKNOWN)",
				   (unsigned int)ie->element_id);

		print_line("length: %u", (unsigned int)ie->length);

		if (be32toh(ie->oui) == OPENWFD_WFD_IE_OUI_1_0)
			print_line("oui: 0x%x (WFD-1.0)", (unsigned int)be32toh(ie->oui));
		else
			print_line("oui: 0x%x (UNKNOWN)", (unsigned int)be32toh(ie->oui));

		/* skip header */
		data = ((char*)data) + 6;
		len -= 6;

		/* check that data payload does not exceed buffer */
		if (ie->length > OPENWFD_WFD_IE_DATA_MAX) {
			print_err("IE length too big (%u > %u), aborting",
				  (unsigned int)ie->length,
				  OPENWFD_WFD_IE_DATA_MAX);
			indent_out();
			goto error;
		} else if (ie->length > len) {
			print_err("IE length bigger than remaining data (%u > %u), aborting",
				  (unsigned int)ie->length, len);
			indent_out();
			goto error;
		}

		/* skip block */
		data = ((char*)data) + ie->length;
		len -= ie->length;

		/* abort if unknown */
		if (ie->element_id != OPENWFD_WFD_IE_ID) {
			print_err("IE ID unknown, aborting");
			indent_out();
			goto error;
		} else if (be32toh(ie->oui) != OPENWFD_WFD_IE_OUI_1_0) {
			print_err("WFD IE OUI unknown, aborting");
			indent_out();
			goto error;
		}

		/* iterate over sub-elements */
		l = ie->length;
		h = ie->data;
		while (l > 0) {
			/* If @col is non-NULL, we are collecting IEs. See
			 * below in sub-IE parsing what we do. */
			if (col) {
				if (l >= sl) {
					memcpy(c, h, sl);
					l -= sl;
					h = ((char*)h) + sl;

					indent_in();
					print_sub(sub, col);
					indent_out();

					free(col);
					col = NULL;
				} else {
					print_line("MULTI IE sub-element; delay parsing to next IE");

					memcpy(c, h, l);
					sl -= l;
					l = 0;
					c = ((char*)c) + l;
				}

				if (col || !l)
					break;
			}

			/* parse WFD IE subelement header */
			if (l < 3) {
				print_err("WFD IE subelement header block too small (%u < 3), aborting",
					  (unsigned int)l);
				indent_out();
				goto error;
			}

			sub = h;
			sl = be16toh(sub->length);
			print_line("IE SUBELEMENT(id: %u len: %u):",
				   (unsigned int)sub->subelement_id,
				   (unsigned int)sl);

			/* skip subelement header */
			l -= 3;
			h = ((char*)h) + 3;

			/* if empty payload, skip */
			if (!sl) {
				continue;
			}

			/* allocate memory for sub-element */
			col = malloc(sl);
			if (!col) {
				print_err("out of memory");
				indent_out();
				goto error;
			}

			/*
			 * Collect sub-element
			 * @len is length of total buffer
			 * @l is length of current IE payload
			 * @sl is wanted length of current subelement payload
			 *
			 * @sl may be bigger than @l, in which case we need to
			 * open the next IE and parse it. The next IE _must_
			 * have the same header as our current IE, otherwise
			 * we need to abort.
			 *
			 * We set @c to the current position in @col and start
			 * copying the current payload. If it's enough, we
			 * parse the sub and continue as usual.
			 * If it's not enough, we simply break; and let the
			 * parent IE handler continue. It detects that col is
			 * not NULL and picks up the unfinished sub.
			 */
			c = col;
			if (l >= sl) {
				memcpy(c, h, sl);
				l -= sl;
				h = ((char*)h) + sl;

				indent_in();
				print_sub(sub, col);
				indent_out();

				free(col);
				col = NULL;
			} else {
				print_line("MULTI IE sub-element; delay parsing to next IE");

				memcpy(c, h, l);
				sl -= l;
				l = 0;
				c = ((char*)c) + l;
			}
		}

		indent_out();
	}

	if (col)
		print_err("MULTI IE sub-element not entirely contained in data");

error:
	free(col);
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

struct example {
	struct openwfd_wfd_ie ie1;
	struct openwfd_wfd_ie_sub sub1;
	struct openwfd_wfd_ie_sub_dev_info dev_info;
} OPENWFD__WFD_PACKED;

int main(int argc, char **argv)
{
	struct example s;

	outfile = stdout;

	memset(&s, 0, sizeof(s));
	s.ie1.element_id = OPENWFD_WFD_IE_ID;
	s.ie1.length = sizeof(s.sub1) + sizeof(s.dev_info);
	s.ie1.oui = htobe32(OPENWFD_WFD_IE_OUI_1_0);

	s.sub1.subelement_id = OPENWFD_WFD_IE_SUB_DEV_INFO;
	s.sub1.length = htobe16(sizeof(s.dev_info));
	s.dev_info.dev_info = htobe16(
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
	s.dev_info.ctrl_port = htobe16(OPENWFD_WFD_IE_SUB_DEV_INFO_DEFAULT_PORT);
	s.dev_info.max_throughput = htobe16(200);

	print_ie(&s, sizeof(s));
	print_hex((uint8_t*)&s, sizeof(s));

	return 0;
}
