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
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#include "p2pd.h"
#include "shared.h"
#include "shl_log.h"
#include "wpa.h"

struct owfd_p2pd_dummy {
	struct owfd_p2pd_config *config;
	struct owfd_p2pd_interface *iface;
};

static void dummy_event_fn(struct owfd_p2pd_interface *iface,
			   struct owfd_wpa_event *ev,
			   void *data)
{
	struct owfd_p2pd_dummy *dummy = data;
}

int owfd_p2pd_dummy_new(struct owfd_p2pd_dummy **out,
			struct owfd_p2pd_config *config,
			struct owfd_p2pd_interface *iface)
{
	struct owfd_p2pd_dummy *dummy;
	int r;

	dummy = calloc(1, sizeof(*dummy));
	if (!dummy)
		return -ENOMEM;

	dummy->config = config;
	dummy->iface = iface;

	r = owfd_p2pd_interface_register_event_fn(dummy->iface,
						  dummy_event_fn,
						  dummy);
	if (r < 0)
		goto err_dummy;

	*out = dummy;
	return 0;

err_dummy:
	free(dummy);
	return r;
}

void owfd_p2pd_dummy_free(struct owfd_p2pd_dummy *dummy)
{
	if (!dummy)
		return;

	owfd_p2pd_interface_unregister_event_fn(dummy->iface,
						dummy_event_fn,
						dummy);
	free(dummy);
}
