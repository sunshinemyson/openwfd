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

#ifndef OWFD_RTSP_H
#define OWFD_RTSP_H

#include <netinet/in.h>
#include <netinet/ip.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* common definitions */

struct owfd_rtsp_msg {
	size_t header_num;
	char **header;
	size_t *header_len;
	void *body;
	size_t body_len;
};

/* rtsp control channel */

struct owfd_rtsp_ctrl;

typedef void (*owfd_rtsp_ctrl_cb) (struct owfd_rtsp_ctrl *ctrl,
				   char *buf, size_t len, void *data);

int owfd_rtsp_ctrl_new(struct owfd_rtsp_ctrl **out);
void owfd_rtsp_ctrl_ref(struct owfd_rtsp_ctrl *ctrl);
void owfd_rtsp_ctrl_unref(struct owfd_rtsp_ctrl *ctrl);

void owfd_rtsp_ctrl_set_data(struct owfd_rtsp_ctrl *ctrl, void *data);
void *owfd_rtsp_ctrl_get_data(struct owfd_rtsp_ctrl *ctrl);

bool owfd_rtsp_ctrl_is_open(struct owfd_rtsp_ctrl *ctrl);
bool owfd_rtsp_ctrl_is_connected(struct owfd_rtsp_ctrl *ctrl);
void owfd_rtsp_ctrl_close(struct owfd_rtsp_ctrl *ctrl);
int owfd_rtsp_ctrl_open_tcp_fd(struct owfd_rtsp_ctrl *ctrl, int fd,
			       owfd_rtsp_ctrl_cb cb);
int owfd_rtsp_ctrl_open_tcp(struct owfd_rtsp_ctrl *ctrl,
			    const struct sockaddr_in6 *src,
			    const struct sockaddr_in6 *dst,
			    owfd_rtsp_ctrl_cb cb);

int owfd_rtsp_ctrl_get_fd(struct owfd_rtsp_ctrl *ctrl);
int owfd_rtsp_ctrl_dispatch(struct owfd_rtsp_ctrl *ctrl, int timeout);

int owfd_rtsp_ctrl_send(struct owfd_rtsp_ctrl *ctrl,
			const char *buf, size_t len);
int owfd_rtsp_ctrl_vsendf(struct owfd_rtsp_ctrl *ctrl,
			  const char *format, va_list args);
int owfd_rtsp_ctrl_sendf(struct owfd_rtsp_ctrl *ctrl,
			 const char *format, ...);

/* rtsp decoder */

struct owfd_rtsp_decoder;

typedef void (*owfd_rtsp_decoder_cb) (struct owfd_rtsp_decoder *dec,
				      struct owfd_rtsp_msg *msg,
				      void *data);

int owfd_rtsp_decoder_new(struct owfd_rtsp_decoder **out,
			  owfd_rtsp_decoder_cb cb);
void owfd_rtsp_decoder_free(struct owfd_rtsp_decoder *dec);

void owfd_rtsp_decoder_set_data(struct owfd_rtsp_decoder *dec, void *data);
void *owfd_rtsp_decoder_get_data(struct owfd_rtsp_decoder *dec);

void owfd_rtsp_decoder_flush(struct owfd_rtsp_decoder *dec);
int owfd_rtsp_decoder_feed(struct owfd_rtsp_decoder *dec,
			   const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OWFD_RTSP_H */
