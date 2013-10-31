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

#include "test_common.h"

static int received;

struct expect {
	size_t times;
	struct owfd_rtsp_msg msg;
};

static void test_rtsp_decoder_event(struct owfd_rtsp_decoder *dec,
				    struct owfd_rtsp_msg *msg,
				    void *data)
{
	static int pos, num;
	struct expect expect[] = {
		{
			.times = 5,
			.msg = {
				.header_num = 1,
				.header = (char*[]){
					"some-header",
					NULL,
				},
				.header_len = (size_t[]){
					11,
					0,
				},
				.body = NULL,
				.body_len = 0,
			},
		},
		{
			.times = 6,
			.msg = {
				.header_num = 1,
				.header = (char*[]){
					"content-length:10",
					NULL,
				},
				.header_len = (size_t[]){
					17,
					0,
				},
				.body = "0123456789",
				.body_len = 10,
			},
		},
		{
			.times = 2,
			.msg = {
				.header_num = 3,
				.header = (char*[]){
					"some-head: buhu",
					"content-length:10",
					"more-header: bing-bung",
					NULL,
				},
				.header_len = (size_t[]){
					15,
					17,
					22,
					0,
				},
				.body = "0123456789",
				.body_len = 10,
			},
		},
	};
	struct expect *e;
	struct owfd_rtsp_msg *m;
	size_t i;

	ck_assert(data == TEST_INVALID_PTR);
	++received;

	ck_assert(pos < (sizeof(expect) / sizeof(*expect)));
	e = &expect[pos];
	m = &e->msg;
	ck_assert(++num <= e->times);
	if (num == e->times) {
		++pos;
		num = 0;
	}

	/* print message */
	if (1) {
		fprintf(stderr, "Message:\n");
		fprintf(stderr, "  header_num: %zu:\n", msg->header_num);
		for (i = 0; i < msg->header_num; ++i)
			fprintf(stderr, "    header (%zu): %s\n",
				msg->header_len[i], msg->header[i]);
		fprintf(stderr, "  body (%zu): %s\n",
			msg->body_len, (char*)msg->body);
	}

	/* compare messages */
	ck_assert(m->header_num == msg->header_num);
	ck_assert(!msg->header[msg->header_num]);
	ck_assert(!msg->header_len[msg->header_num]);

	for (i = 0; i < m->header_num; ++i) {
		ck_assert(!strcmp(m->header[i], msg->header[i]));
		ck_assert(m->header_len[i] == msg->header_len[i]);
	}

	ck_assert(!!m->body == !!msg->body);
	ck_assert(m->body_len == msg->body_len);
	ck_assert(!msg->body || !((char*)msg->body)[msg->body_len]);
	ck_assert(!msg->body || !memcmp(m->body, msg->body, msg->body_len));
}

static void feed(struct owfd_rtsp_decoder *d, const char *b, size_t len)
{
	int r;

	r = owfd_rtsp_decoder_feed(d, b, len);
	ck_assert(r >= 0);
}

#define FEED(_d, _str) feed((_d), (_str), sizeof(_str) - 1)

START_TEST(test_rtsp_decoder)
{
	struct owfd_rtsp_decoder *d;
	int r, sent = 0;

	r = owfd_rtsp_decoder_new(&d, NULL);
	ck_assert(r >= 0);
	owfd_rtsp_decoder_free(d);

	r = owfd_rtsp_decoder_new(&d, test_rtsp_decoder_event);
	ck_assert(r >= 0);

	owfd_rtsp_decoder_set_data(d, TEST_INVALID_PTR);
	ck_assert(owfd_rtsp_decoder_get_data(d) == TEST_INVALID_PTR);

	ck_assert(received == sent);

	FEED(d, "some-header\r\n\r\n");
	++sent;
	ck_assert(received == sent);

	FEED(d, "some-header\r\r");
	++sent;
	ck_assert(received == sent);

	FEED(d, "some-header\n\n");
	++sent;
	ck_assert(received == sent);

	FEED(d, "some-header\n\r");
	++sent;
	ck_assert(received == sent);

	FEED(d, "some-header\r\n\n");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\r\r0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\n\n0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\n\r0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\n\r\n0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\r\n\n0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "content-length:10\r\n\r\n0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "some-head: buhu\ncontent-length:10\r\nmore-header:  bing-\0bung \r\n\n0123456789");
	++sent;
	ck_assert(received == sent);

	FEED(d, "  \t\n \t some-head: \n\t\r buhu     \ncontent-length:10\r\nmore-header:  bing-\0bung \r\n\n0123456789");
	++sent;
	ck_assert(received == sent);

	owfd_rtsp_decoder_free(d);
}
END_TEST

TEST_DEFINE_CASE(decoder)
	TEST(test_rtsp_decoder)
TEST_END_CASE

TEST_DEFINE(
	TEST_SUITE(rtsp,
		TEST_CASE(decoder),
		TEST_END
	)
)
