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
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include "p2pd.h"
#include "shl_log.h"

struct owfd_p2pd {
	struct owfd_p2pd_config config;
	int efd;
	int sfd;

	struct owfd_p2pd_interface *interface;
	struct owfd_p2pd_dummy *dummy;
};

int owfd_p2pd_ep_add(int efd, int *fd, unsigned int events)
{
	struct epoll_event ev;
	int r;

	memset(&ev, 0, sizeof(ev));
	ev.data.ptr = fd;
	ev.events = events;

	r = epoll_ctl(efd, EPOLL_CTL_ADD, *fd, &ev);
	if (r < 0)
		return log_ERRNO();

	return 0;
}

void owfd_p2pd_ep_update(int efd, int *fd, unsigned int events)
{
	struct epoll_event ev;
	int r;

	memset(&ev, 0, sizeof(ev));
	ev.data.ptr = fd;
	ev.events = events;

	r = epoll_ctl(efd, EPOLL_CTL_MOD, *fd, &ev);
	if (r < 0)
		log_vERRNO();
}

void owfd_p2pd_ep_remove(int efd, int fd)
{
	int r;

	r = epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL);
	if (r < 0)
		log_vERRNO();
}

static int owfd_p2pd_dispatch_sfd(struct owfd_p2pd *p2pd,
				  struct owfd_p2pd_ep *ep)
{
	ssize_t l;
	struct signalfd_siginfo info;
	int r;

	if (ep->ev->data.ptr != &p2pd->sfd)
		return OWFD_P2PD_EP_NOT_HANDLED;

	if (ep->ev->events & (EPOLLHUP | EPOLLERR))
		return log_EPIPE();

	l = read(p2pd->sfd, &info, sizeof(info));
	if (l < 0)
		return log_ERRNO();
	else if (l != sizeof(info))
		return OWFD_P2PD_EP_NOT_HANDLED;

	log_notice("received signal %d: %s",
		   info.ssi_signo, strsignal(info.ssi_signo));

	switch (info.ssi_signo) {
	case SIGCHLD:
		r = owfd_p2pd_interface_dispatch_chld(p2pd->interface, &info);
		if (r != OWFD_P2PD_EP_NOT_HANDLED)
			break;

		r = OWFD_P2PD_EP_HANDLED;
		break;
	case SIGPIPE:
		r = OWFD_P2PD_EP_HANDLED;
		break;
	default:
		r = OWFD_P2PD_EP_QUIT;
		break;
	}

	return r;
}

static int owfd_p2pd_dispatch(struct owfd_p2pd *p2pd)
{
	struct epoll_event evs[64];
	const size_t max = sizeof(evs) / sizeof(*evs);
	struct owfd_p2pd_ep ep;
	int i, n, r;

	n = epoll_wait(p2pd->efd, evs, max, -1);
	if (n < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		else
			return log_ERRNO();
	} else if (n > max) {
		n = max;
	}

	r = 0;
	ep.evs = evs;
	ep.num = n;
	for (i = 0; i < n; ++i) {
		ep.ev = &ep.evs[i];
		if (!ep.ev->data.ptr)
			continue;

		r = owfd_p2pd_dispatch_sfd(p2pd, &ep);
		if (r < 0)
			break;
		else if (r == OWFD_P2PD_EP_HANDLED)
			continue;
		else if (r == OWFD_P2PD_EP_QUIT)
			break;

		r = owfd_p2pd_interface_dispatch(p2pd->interface, &ep);
		if (r < 0)
			break;
		else if (r == OWFD_P2PD_EP_HANDLED)
			continue;
		else if (r == OWFD_P2PD_EP_QUIT)
			break;
	}

	return r;
}

static int owfd_p2pd_run(struct owfd_p2pd *p2pd)
{
	int r;

	while (1) {
		r = owfd_p2pd_dispatch(p2pd);
		if (r < 0)
			return r;
		else if (r == OWFD_P2PD_EP_QUIT)
			break;
	}

	return 0;
}

static void owfd_p2pd_teardown(struct owfd_p2pd *p2pd)
{
	owfd_p2pd_dummy_free(p2pd->dummy);
	owfd_p2pd_interface_free(p2pd->interface);

	if (p2pd->sfd >= 0)
		close(p2pd->sfd);
	if (p2pd->efd >= 0)
		close(p2pd->efd);
}

static void sig_dummy(int sig)
{
}

static int owfd_p2pd_setup(struct owfd_p2pd *p2pd)
{
	static const int sigs[] = {
		SIGINT,
		SIGTERM,
		SIGQUIT,
		SIGHUP,
		SIGCHLD,
		SIGPIPE,
		0
	};
	int r, i;
	sigset_t mask;
	struct sigaction sig;

	p2pd->efd = epoll_create1(EPOLL_CLOEXEC);
	if (p2pd->efd < 0) {
		r = log_ERRNO();
		goto error;
	}

	sigemptyset(&mask);
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = sig_dummy;
	sig.sa_flags = SA_RESTART;

	for (i = 0; sigs[i]; ++i) {
		sigaddset(&mask, sigs[i]);
		r = sigaction(sigs[i], &sig, NULL);
		if (r < 0) {
			r = log_ERRNO();
			goto error;
		}
	}

	r = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (r < 0) {
		r = log_ERRNO();
		goto error;
	}

	p2pd->sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (p2pd->sfd < 0) {
		r = log_ERRNO();
		goto error;
	}

	r = owfd_p2pd_ep_add(p2pd->efd, &p2pd->sfd, EPOLLIN);
	if (r < 0)
		goto error;

	r = owfd_p2pd_interface_new(&p2pd->interface, &p2pd->config,
				    p2pd->efd);
	if (r < 0)
		goto error;

	r = owfd_p2pd_dummy_new(&p2pd->dummy, &p2pd->config, p2pd->interface);
	if (r < 0)
		goto error;

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
	p2pd.sfd = -1;
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
