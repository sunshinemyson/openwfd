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

#ifndef OWFD_WPA_CTRL_H
#define OWFD_WPA_CTRL_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct owfd_wpa_ctrl;

typedef void (*owfd_wpa_ctrl_cb) (struct owfd_wpa_ctrl *wpa, void *buf,
				  size_t len, void *data);

int owfd_wpa_ctrl_new(struct owfd_wpa_ctrl **out);
void owfd_wpa_ctrl_ref(struct owfd_wpa_ctrl *wpa);
void owfd_wpa_ctrl_unref(struct owfd_wpa_ctrl *wpa);

void owfd_wpa_ctrl_set_data(struct owfd_wpa_ctrl *wpa, void *data);
void *owfd_wpa_ctrl_get_data(struct owfd_wpa_ctrl *wpa);

int owfd_wpa_ctrl_open(struct owfd_wpa_ctrl *wpa, const char *ctrl_path,
		       owfd_wpa_ctrl_cb cb);
void owfd_wpa_ctrl_close(struct owfd_wpa_ctrl *wpa);
bool owfd_wpa_ctrl_is_open(struct owfd_wpa_ctrl *wpa);

int owfd_wpa_ctrl_get_fd(struct owfd_wpa_ctrl *wpa);
int owfd_wpa_ctrl_dispatch(struct owfd_wpa_ctrl *wpa, int timeout);

int owfd_wpa_ctrl_request(struct owfd_wpa_ctrl *wpa, const void *cmd,
			  size_t cmd_len, void *reply, size_t *reply_len,
			  int timeout);
int owfd_wpa_ctrl_request_ok(struct owfd_wpa_ctrl *wpa, const void *cmd,
			     size_t cmd_len, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* OWFD_WPA_CTRL_H */
