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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "p2pd.h"
#include "shl_log.h"
#include "wpa_ctrl.h"

struct owfd_p2pd {
	struct owfd_p2pd_config config;
	int efd;
};

static int owfd_p2pd_run(struct owfd_p2pd *p2pd)
{
	return 0;
}

static void owfd_p2pd_teardown(struct owfd_p2pd *p2pd)
{
	if (p2pd->efd >= 0)
		close(p2pd->efd);
}

static int owfd_p2pd_setup(struct owfd_p2pd *p2pd)
{
	int r;

	p2pd->efd = epoll_create1(EPOLL_CLOEXEC);
	if (p2pd->efd < 0) {
		r = log_ERRNO();
		goto error;
	}

	return 0;

error:
	owfd_p2pd_teardown(p2pd);
	return r;
}

int main(int argc, char **argv)
{
	struct owfd_p2pd p2pd;
	int r;

	memset(&p2pd, 0, sizeof(p2pd));
	p2pd.efd = -1;
	owfd_p2pd_init_config(&p2pd.config);

	r = owfd_p2pd_parse_argv(&p2pd.config, argc, argv);
	if (r < 0)
		goto err_out;

	if (p2pd.config.debug)
		log_max_sev = LOG_DEBUG;
	else if (p2pd.config.verbose)
		log_max_sev = LOG_INFO;
	else if (p2pd.config.silent)
		log_max_sev = LOG_ERROR;

	if (p2pd.config.silent)
		log_debug("-");
	else
		log_format(LOG_DEFAULT_BASE, NULL, LOG_SEV_NUM,
			   "openwfd_p2p - revision %s %s %s",
			   "some-rev-TODO-xyz", __DATE__, __TIME__);

	log_info("initializing");
	r = owfd_p2pd_setup(&p2pd);
	if (r < 0)
		goto err_conf;

	log_info("running");
	r = owfd_p2pd_run(&p2pd);

	owfd_p2pd_teardown(&p2pd);
err_conf:
	owfd_p2pd_clear_config(&p2pd.config);
	if (r < 0) {
		errno = -r;
		log_error("initialization failed (%d): %m", r);
	}
	log_info("exiting");
err_out:
	return -r;
}
