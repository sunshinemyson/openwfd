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
#include "shl_ring.h"
#include "rtsp.h"

enum state {
	STATE_NEW,
	STATE_HEADER,
	STATE_HEADER_NL,
	STATE_BODY,
};

struct owfd_rtsp_decoder {
	void *data;
	owfd_rtsp_decoder_cb cb;

	struct shl_ring ring;
	unsigned int state;
	char last_chr;
	size_t remaining_body;

	size_t header_size;
	struct owfd_rtsp_msg msg;
};

int owfd_rtsp_decoder_new(struct owfd_rtsp_decoder **out,
			  owfd_rtsp_decoder_cb cb)
{
	struct owfd_rtsp_decoder *dec;
	int r;

	dec = calloc(1, sizeof(*dec));
	if (!dec)
		return -ENOMEM;
	dec->cb = cb;

	dec->header_size = 8;
	dec->msg.header = calloc(1, sizeof(char*) * dec->header_size);
	if (!dec->msg.header) {
		r = -ENOMEM;
		goto err_dec;
	}

	dec->msg.header_len = calloc(1, sizeof(size_t) * dec->header_size);
	if (!dec->msg.header_len) {
		r = -ENOMEM;
		goto err_header;
	}

	*out = dec;
	return 0;

err_header:
	free(dec->msg.header);
err_dec:
	free(dec);
	return r;
}

void owfd_rtsp_decoder_free(struct owfd_rtsp_decoder *dec)
{
	size_t i;

	if (!dec)
		return;

	for (i = 0; i < dec->msg.header_num; ++i)
		free(dec->msg.header[i]);

	free(dec->msg.header);
	free(dec->msg.header_len);
	free(dec->msg.body);

	shl_ring_clear(&dec->ring);
	free(dec);
}

void owfd_rtsp_decoder_set_data(struct owfd_rtsp_decoder *dec, void *data)
{
	dec->data = data;
}

void *owfd_rtsp_decoder_get_data(struct owfd_rtsp_decoder *dec)
{
	return dec->data;
}

void owfd_rtsp_decoder_flush(struct owfd_rtsp_decoder *dec)
{
	shl_ring_flush(&dec->ring);
	dec->state = STATE_NEW;
	dec->last_chr = 0;
	dec->remaining_body = 0;
}

static void msg_done(struct owfd_rtsp_decoder *dec)
{
	size_t i;

	if (dec->cb)
		dec->cb(dec, &dec->msg, dec->data);

	for (i = 0; i < dec->msg.header_num; ++i) {
		free(dec->msg.header[i]);
		dec->msg.header[i] = NULL;
		dec->msg.header_len[i] = 0;
	}

	dec->msg.header_num = 0;

	free(dec->msg.body);
	dec->msg.body = NULL;
	dec->msg.body_len = 0;
}

static int push_header_line(struct owfd_rtsp_decoder *dec, char *line,
			    size_t len)
{
	char **h;
	size_t *l;
	size_t n;

	/* keep 1 entry for terminating NULL/0 */
	if (dec->header_size == dec->msg.header_num + 2) {
		n = dec->header_size * 2;
		if (n < dec->header_size)
			return -ENOMEM;

		h = realloc(dec->msg.header, sizeof(char*) * n);
		if (!h)
			return -ENOMEM;
		dec->msg.header = h;

		l = realloc(dec->msg.header_len, sizeof(size_t) * n);
		if (!l)
			return -ENOMEM;
		dec->msg.header_len = l;

		dec->header_size = n;
	}

	dec->msg.header[dec->msg.header_num] = line;
	dec->msg.header_len[dec->msg.header_num] = len;
	++dec->msg.header_num;
	dec->msg.header[dec->msg.header_num] = NULL;
	dec->msg.header_len[dec->msg.header_num] = 0;

	return 0;
}

static size_t sanitize_header_line(struct owfd_rtsp_decoder *dec,
				   char *line, size_t len)
{
	char *src, *dst, c, prev, last_c;
	size_t i;

	src = line;
	dst = line;
	last_c = 0;

	for (i = 0; i < len; ++i) {
		c = *src++;
		prev = last_c;
		last_c = c;

		/* ignore any binary 0 */
		if (c == '\0')
			continue;

		/* turn new-lines/tabs into white-space */
		if (c == '\r' || c == '\n' || c == '\t')
			c = ' ';

		/* trim whitespace */
		if (c == ' ' && prev == ' ')
			continue;

		*dst++ = c;
	}

	/* terminate string with binary zero */
	*dst = 0;

	return dst - line;
}

static int parse_header_line(struct owfd_rtsp_decoder *dec,
			     char *line)
{
	unsigned long l;
	char *e;

	if (!strncasecmp(line, "content-length:", 15)) {
		l = strtoul(&line[15], &e, 10);
		if (!line[15] || *e)
			return -EINVAL;

		if (dec->remaining_body && dec->remaining_body != l)
			return -EINVAL;

		dec->remaining_body = l;
	}

	return 0;
}

static int finish_header_line(struct owfd_rtsp_decoder *dec, size_t rlen)
{
	char *line;
	size_t l;
	int r;

	l = rlen;
	line = shl_ring_copy(&dec->ring, &l);
	if (!line)
		return -ENOMEM;

	shl_ring_pull(&dec->ring, rlen);

	l = sanitize_header_line(dec, line, l);
	r = parse_header_line(dec, line);
	if (r < 0) {
		free(line);
		return r;
	}

	r = push_header_line(dec, line, l);
	if (r < 0) {
		free(line);
		return r;
	}

	return 0;
}

static ssize_t feed_char_new(struct owfd_rtsp_decoder *dec,
			     char ch, size_t rlen)
{
	switch (ch) {
	case '\r':
	case '\n':
	case '\t':
	case ' ':
		/* If no msg has been started, yet, we ignore LWS for
		 * compatibility reasons. Note that they're actually not
		 * allowed, but should be ignored by implementations. */
		++rlen;
		break;
	default:
		/* Clear any pending data in the ring-buffer and then just
		 * push the data into the buffer. Any data except LWS is fine
		 * here. */
		dec->state = STATE_HEADER;
		dec->remaining_body = 0;

		shl_ring_pull(&dec->ring, rlen);
		rlen = 1;
		break;
	}

	return rlen;
}

static ssize_t feed_char_header(struct owfd_rtsp_decoder *dec,
				char ch, size_t rlen)
{
	int r;

	switch (ch) {
	case '\r':
		if (dec->last_chr == '\r' || dec->last_chr == '\n') {
			/* \r\r means empty new-line. We actually allow \r\r\n,
			 * too. \n\r means empty new-line, too, but might also
			 * be finished off as \n\r\n so go to STATE_HEADER_NL
			 * to optionally complete the new-line.
			 * However, if the body is empty, we need to finish the
			 * msg early as there might be no \n coming.. */
			dec->state = STATE_HEADER_NL;

			/* First finish the last header line if any. Don't
			 * include the current \r as it is already part of the
			 * empty following line. */
			r = finish_header_line(dec, rlen);
			if (r < 0)
				return r;
			rlen = 0;

			/* No remaining body. Finish message! */
			if (!dec->remaining_body)
				msg_done(dec);

			++rlen;
		} else {
			/* '\r' following any character just means newline
			 * (optionally followed by \n). We don't do anything as
			 * it might be a continuation line. */
			++rlen;
		}
		break;
	case '\n':
		if (dec->last_chr == '\n') {
			/* We got \n\n, which means we need to finish the
			 * current header-line. If there's no remaining body,
			 * we immediately finish the message and got to
			 * STATE_NEW. Otherwise, we go to STATE_BODY
			 * straight. */

			/* don't include second \n in header-line */
			r = finish_header_line(dec, rlen);
			if (r < 0)
				return r;
			rlen = 0;

			dec->state = STATE_BODY;
			if (!dec->remaining_body) {
				dec->state = STATE_NEW;
				msg_done(dec);
			}

			/* discard \n */
			shl_ring_pull(&dec->ring, 1);
		} else if (dec->last_chr == '\r') {
			/* We got an \r\n. We cannot finish the header line as
			 * it might be a continuation line. Next character
			 * decides what to do. Don't do anything here.
			 * \r\n\r cannot happen here as it is handled by
			 * STATE_HEADER_NL. */
			++rlen;
		} else {
			/* Same as above, we cannot finish the line as it
			 * might be a continuation line. Do nothing. */
			++rlen;
		}
		break;
	case '\t':
	case ' ':
		/* Whitespace. Simply push into buffer and don't do anything.
		 * In case of a continuation line, nothing has to be done,
		 * either. */
		++rlen;
		break;
	default:
		/* Last line was already completed and this is no whitespace,
		 * thus it's not a continuation line. Finish the line. */
		if (dec->last_chr == '\r' || dec->last_chr == '\n') {
			/* don't include new char in line */
			r = finish_header_line(dec, rlen);
			if (r < 0)
				return r;
			rlen = 0;
		}

		/* Push character into new line. Nothing to be done. */
		++rlen;
		break;
	}

	return rlen;
}

static ssize_t feed_char_body(struct owfd_rtsp_decoder *dec,
			      char ch, size_t rlen)
{
	char *line;
	size_t l;

	/* If remaining_body was already 0, the message had no body. Note that
	 * messages without body are finished early, so no need to call
	 * msg_done() here. Simply forward @ch to STATE_NEW.
	 * @rlen is usually 0. We don't care and forward it, too. */
	if (!dec->remaining_body) {
		dec->state = STATE_NEW;
		return feed_char_new(dec, ch, rlen);
	}

	/* *any* character is allowed as body */
	++rlen;
	if (!--dec->remaining_body) {
		/* full body received, copy it and go to STATE_NEW */
		l = (size_t)rlen;
		line = shl_ring_copy(&dec->ring, &l);
		if (!line)
			return -ENOMEM;

		dec->msg.body = line;
		dec->msg.body_len = l;
		msg_done(dec);

		dec->state = STATE_NEW;
		shl_ring_pull(&dec->ring, rlen);
		rlen = 0;
	}

	return rlen;
}

static ssize_t feed_char_header_nl(struct owfd_rtsp_decoder *dec,
				   char ch, size_t rlen)
{
	/* STATE_HEADER_NL means we received an empty line ending with \r. The
	 * standard requires a following \n but advises implementations to
	 * accept \r on itself, too.
	 * What we do is to parse a \n as end-of-header and any character as
	 * end-of-header plus start-of-body. Note that we discard anything in
	 * the ring-buffer that has already been parsed (which normally can
	 * only be a single \r or \r\n). */

	if (ch == '\n') {
		/* discard parsed \r plus new \n */
		shl_ring_pull(&dec->ring, rlen + 1);
		rlen = 0;

		dec->state = STATE_BODY;
		if (!dec->remaining_body)
			dec->state = STATE_NEW;

		return rlen;
	} else {
		/* discard parsed \r and push @ch into body */
		shl_ring_pull(&dec->ring, rlen);
		rlen = 0;

		dec->state = STATE_BODY;
		return feed_char_body(dec, ch, rlen);
	}
}

static ssize_t feed_char(struct owfd_rtsp_decoder *dec, char ch, size_t rlen)
{
	ssize_t r;

	switch (dec->state) {
	case STATE_NEW:
		r = feed_char_new(dec, ch, rlen);
		break;
	case STATE_HEADER:
		r = feed_char_header(dec, ch, rlen);
		break;
	case STATE_HEADER_NL:
		r = feed_char_header_nl(dec, ch, rlen);
		break;
	case STATE_BODY:
		r = feed_char_body(dec, ch, rlen);
		break;
	}

	return r;
}

int owfd_rtsp_decoder_feed(struct owfd_rtsp_decoder *dec,
			   const char *buf, size_t len)
{
	size_t rlen, i;
	ssize_t l;
	int r;

	rlen = shl_ring_length(&dec->ring);
	r = shl_ring_push(&dec->ring, buf, len);
	if (r < 0)
		return -ENOMEM;

	for (i = 0; i < len; ++i) {
		l = feed_char(dec, buf[i], rlen);
		if (l < 0) {
			r = l;
			break;
		}

		rlen = l;
		dec->last_chr = buf[i];
	}

	if (r < 0) {
		/* ring buffer may be corrupted, flush it */
		owfd_rtsp_decoder_flush(dec);
	}

	return r;
}
