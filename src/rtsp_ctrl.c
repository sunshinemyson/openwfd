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
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "shared.h"
#include "shl_ring.h"
#include "rtsp.h"

struct owfd_rtsp_ctrl {
	unsigned long ref;
	void *data;
	int efd;
	int fd;
	owfd_rtsp_ctrl_cb cb;
	struct shl_ring out_ring;

	unsigned int connected : 1;
};

int owfd_rtsp_ctrl_new(struct owfd_rtsp_ctrl **out)
{
	struct owfd_rtsp_ctrl *ctrl;
	int r;

	ctrl = calloc(1, sizeof(*ctrl));
	if (!ctrl)
		return -ENOMEM;
	ctrl->ref = 1;
	ctrl->efd = -1;
	ctrl->fd = -1;

	ctrl->efd = epoll_create1(EPOLL_CLOEXEC);
	if (ctrl->efd < 0) {
		r = -errno;
		goto err_ctrl;
	}

	*out = ctrl;
	return 0;

err_ctrl:
	free(ctrl);
	return r;
}

void owfd_rtsp_ctrl_ref(struct owfd_rtsp_ctrl *ctrl)
{
	if (!ctrl || !ctrl->ref)
		return;

	++ctrl->ref;
}

void owfd_rtsp_ctrl_unref(struct owfd_rtsp_ctrl *ctrl)
{
	if (!ctrl || !ctrl->ref || --ctrl->ref)
		return;

	owfd_rtsp_ctrl_close(ctrl);
	close(ctrl->efd);
	shl_ring_clear(&ctrl->out_ring);
	free(ctrl);
}

void owfd_rtsp_ctrl_set_data(struct owfd_rtsp_ctrl *ctrl, void *data)
{
	ctrl->data = data;
}

void *owfd_rtsp_ctrl_get_data(struct owfd_rtsp_ctrl *ctrl)
{
	return ctrl->data;
}

bool owfd_rtsp_ctrl_is_open(struct owfd_rtsp_ctrl *ctrl)
{
	return ctrl->fd >= 0;
}

bool owfd_rtsp_ctrl_is_connected(struct owfd_rtsp_ctrl *ctrl)
{
	return owfd_rtsp_ctrl_is_open(ctrl) && ctrl->connected;
}

void owfd_rtsp_ctrl_close(struct owfd_rtsp_ctrl *ctrl)
{
	if (!owfd_rtsp_ctrl_is_open(ctrl))
		return;

	close(ctrl->fd);
	ctrl->fd = -1;
	ctrl->connected = 0;
	ctrl->cb = NULL;
	shl_ring_flush(&ctrl->out_ring);
}

int owfd_rtsp_ctrl_open_tcp_fd(struct owfd_rtsp_ctrl *ctrl, int fd,
			       owfd_rtsp_ctrl_cb cb)
{
	struct epoll_event ev;
	int r, set;

	if (owfd_rtsp_ctrl_is_open(ctrl))
		return -EALREADY;
	if (fd < 0)
		return -EINVAL;

	set = fcntl(fd, F_GETFL);
	if (set < 0)
		return -errno;
	r = fcntl(fd, F_SETFL, set | O_NONBLOCK);
	if (r < 0)
		return -errno;

	/* wait for EPOLLOUT as "CONNECTED" event */
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLOUT;
	ev.data.ptr = &ctrl->fd;

	r = epoll_ctl(ctrl->efd, EPOLL_CTL_ADD, fd, &ev);
	if (r < 0)
		return -errno;

	ctrl->fd = fd;
	ctrl->connected = 0;
	ctrl->cb = cb;

	return 0;
}

int owfd_rtsp_ctrl_open_tcp(struct owfd_rtsp_ctrl *ctrl,
			    const struct sockaddr_in6 *src,
			    const struct sockaddr_in6 *dst,
			    owfd_rtsp_ctrl_cb cb)
{
	int r, fd;

	if (owfd_rtsp_ctrl_is_open(ctrl))
		return -EALREADY;

	fd = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0)
		return -errno;

	if (src) {
		r = bind(fd, (struct sockaddr*)src, sizeof(*src));
		if (r < 0) {
			r = -errno;
			close(fd);
			return r;
		}
	}

	r = connect(fd, (struct sockaddr*)dst, sizeof(*dst));
	if (r < 0) {
		r = -errno;
		close(fd);
		return r;
	}

	r = owfd_rtsp_ctrl_open_tcp_fd(ctrl, fd, cb);
	if (r < 0) {
		close(fd);
		return r;
	}

	return 0;
}

int owfd_rtsp_ctrl_get_fd(struct owfd_rtsp_ctrl *ctrl)
{
	return ctrl->efd;
}

static int connect_done(struct owfd_rtsp_ctrl *ctrl)
{
	if (ctrl->connected)
		return 0;

	ctrl->connected = 1;
	if (ctrl->cb)
		ctrl->cb(ctrl, NULL, 0, ctrl->data);

	/* callback may have called *_close() and *_open_*() */
	return ctrl->connected ? 0 : -EPIPE;
}

static int recv_all(struct owfd_rtsp_ctrl *ctrl)
{
	ssize_t l;
	char buf[4096];
	size_t rounds;

	rounds = 128;
	do {
		l = read(ctrl->fd, buf, sizeof(buf));
		if (l < 0) {
			if (errno != EAGAIN && errno != EINTR)
				return -errno;
		} else if (l > 0) {
			if (l > sizeof(buf))
				l = sizeof(buf);

			if (ctrl->cb)
				ctrl->cb(ctrl, buf, l, ctrl->data);
		}
	} while (--rounds && l > 0 && ctrl->connected);

	return ctrl->connected ? 0 : -EPIPE;
}

static int send_all(struct owfd_rtsp_ctrl *ctrl)
{
	struct epoll_event ev;
	bool done = true;
	struct iovec vec[2];
	size_t n, sum;
	ssize_t l;
	int r;

	n = shl_ring_peek(&ctrl->out_ring, vec);
	if (n > 0) {
		sum = vec[0].iov_len;
		if (n > 1)
			sum += vec[1].iov_len;

		l = writev(ctrl->fd, vec, n);
		if (l < 0 && errno != EAGAIN && errno != EINTR)
			return -errno;
		else if (l < (ssize_t)sum)
			done = false;
	}

	if (done) {
		memset(&ev, 0, sizeof(ev));
		ev.events = EPOLLHUP | EPOLLERR | EPOLLIN;
		ev.data.ptr = &ctrl->fd;

		r = epoll_ctl(ctrl->efd, EPOLL_CTL_MOD, ctrl->fd, &ev);
		if (r < 0)
			return -errno;
	}

	return 0;
}

static int dispatch_ctrl(struct owfd_rtsp_ctrl *ctrl, struct epoll_event *e)
{
	int r;

	if (e->events & EPOLLIN) {
		r = connect_done(ctrl);
		if (r < 0)
			return r;
		r = recv_all(ctrl);
		if (r < 0)
			return r;
	}

	if (e->events & EPOLLOUT) {
		r = connect_done(ctrl);
		if (r < 0)
			return r;
		r = send_all(ctrl);
		if (r < 0)
			return r;
	}

	if (e->events & (EPOLLHUP | EPOLLERR))
		return -EPIPE;

	return 0;
}

int owfd_rtsp_ctrl_dispatch(struct owfd_rtsp_ctrl *ctrl, int timeout)
{
	struct epoll_event evs[1];
	const size_t max = sizeof(evs) / sizeof(*evs);
	int n, r;

	if (!owfd_rtsp_ctrl_is_open(ctrl))
		return -ENODEV;

	n = epoll_wait(ctrl->efd, evs, max, timeout);
	if (n < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		else
			return -errno;
	} else if (!n) {
		return 0;
	} else if (n > max) {
		n = max;
	}

	if (evs[0].data.ptr != &ctrl->fd)
		return 0;

	r = dispatch_ctrl(ctrl, evs);
	if (r < 0)
		owfd_rtsp_ctrl_close(ctrl);

	return r;
}

int owfd_rtsp_ctrl_send(struct owfd_rtsp_ctrl *ctrl,
			const char *buf, size_t len)
{
	struct epoll_event ev;
	bool empty;
	int r;

	if (!owfd_rtsp_ctrl_is_open(ctrl))
		return -ENODEV;

	empty = !shl_ring_peek(&ctrl->out_ring, NULL);

	r = shl_ring_push(&ctrl->out_ring, buf, len);
	if (r >= 0 && empty) {
		memset(&ev, 0, sizeof(ev));
		ev.events = EPOLLHUP | EPOLLERR | EPOLLIN | EPOLLOUT;
		ev.data.ptr = &ctrl->fd;

		r = epoll_ctl(ctrl->efd, EPOLL_CTL_MOD, ctrl->fd, &ev);
		if (r < 0)
			r = -errno;
	}

	return r;
}

int owfd_rtsp_ctrl_vsendf(struct owfd_rtsp_ctrl *ctrl,
			  const char *format, va_list args)
{
	int r;
	char *buf;

	r = vasprintf(&buf, format, args);
	if (r < 0)
		return -ENOMEM;

	r = owfd_rtsp_ctrl_send(ctrl, buf, (size_t)r);
	free(buf);

	return r;
}

int owfd_rtsp_ctrl_sendf(struct owfd_rtsp_ctrl *ctrl,
			 const char *format, ...)
{
	va_list args;
	int r;

	va_start(args, format);
	r = owfd_rtsp_ctrl_vsendf(ctrl, format, args);
	va_end(args);

	return r;
}
