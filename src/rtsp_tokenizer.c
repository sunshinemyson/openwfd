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

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "shared.h"
#include "rtsp.h"

ssize_t owfd_rtsp_tokenize(const char *line, char **out)
{
	char *t, *dst, c, prev, last_c;
	const char *src;
	size_t l, num, len;
	bool quoted, escaped;

	/* we need at most twice as much space for all the terminating 0s */
	len = strlen(line);
	l = len * 2 + 1;
	if (l <= len)
		return -ENOMEM;

	t = malloc(l);
	if (!t)
		return -ENOMEM;

	/* set t[0]=0 in case strlen(line)==0 */
	*t = 0;

	num = 0;
	src = line;
	dst = t;
	quoted = 0;
	escaped = 0;
	prev = 0;
	last_c = 0;

	for ( ; *src; ++src) {
		c = *src;
		prev = last_c;
		last_c = c;

		if (quoted) {
			if (escaped) {
				if (c == '\\') {
					*dst++ = '\\';
				} else if (c == '"') {
					*dst++ = '"';
				} else if (c == 'n') {
					*dst++ = '\n';
				} else if (c == 'r') {
					*dst++ = '\r';
				} else if (c == 't') {
					*dst++ = '\t';
				} else if (c == 'a') {
					*dst++ = '\a';
				} else if (c == 'f') {
					*dst++ = '\f';
				} else if (c == 'v') {
					*dst++ = '\v';
				} else if (c == 'b') {
					*dst++ = '\b';
				} else if (c == 'e') {
					*dst++ = 0x1b;	/* ESC */
				} else if (c == '0' || c == 0) {
					/* drop binary zero escape "\0" */
					--dst;
				} else {
					*dst++ = c;
				}

				escaped = 0;
			} else {
				if (c == '"') {
					*dst++ = 0;
					++num;
					quoted = 0;
					last_c = 0;
				} else if (c == '\\') {
					escaped = 1;
				} else if (c == 0) {
					/* skip */
				} else {
					*dst++ = c;
				}
			}
		} else {
			if (c == '"') {
				if (prev) {
					*dst++ = 0;
					++num;
				}

				quoted = 1;
				escaped = 0;
				last_c = 0;
			} else if (c == 0) {
				/* skip */
			} else if (c == ' ') {
				if (prev) {
					*dst++ = 0;
					++num;
				}
				last_c = 0;
			} else if (c == '(' ||
				   c == ')' ||
				   c == '[' ||
				   c == ']' ||
				   c == '{' ||
				   c == '}' ||
				   c == '<' ||
				   c == '>' ||
				   c == '@' ||
				   c == ',' ||
				   c == ';' ||
				   c == ':' ||
				   c == '\\' ||
				   c == '/' ||
				   c == '?' ||
				   c == '=') {
				if (prev) {
					*dst++ = 0;
					++num;
				}

				*dst++ = c;
				*dst++ = 0;
				++num;
				last_c = 0;
			} else if (c <= 31 || c == 127) {
				/* ignore CTLs */
				if (prev) {
					*dst++ = 0;
					++num;
				}
				last_c = 0;
			} else {
				*dst++ = c;
			}
		}
	}

	prev = last_c;

	if (prev) {
		*dst++ = 0;
		++num;
	}

	*out = t;
	return num;
}
