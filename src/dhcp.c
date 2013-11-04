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

#include <arpa/inet.h>
#include <errno.h>
#include <glib.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>
#include "dhcp.h"
#include "gdhcp/gdhcp.h"
#include "shared.h"
#include "shl_log.h"

struct owfd_dhcp {
	struct owfd_dhcp_config config;
	int ifindex;
	GMainLoop *loop;

	int sfd;
	GIOChannel *sfd_chan;
	guint sfd_id;
};

static gboolean owfd_dhcp_sfd_fn(GIOChannel *chan, GIOCondition mask,
				 gpointer data)
{
	struct owfd_dhcp *dhcp = data;
	ssize_t l;
	struct signalfd_siginfo info;

	if (mask & (G_IO_HUP | G_IO_ERR)) {
		log_vEPIPE();
		g_main_loop_quit(dhcp->loop);
		return FALSE;
	}

	l = read(dhcp->sfd, &info, sizeof(info));
	if (l < 0) {
		log_vERRNO();
		g_main_loop_quit(dhcp->loop);
		return FALSE;
	} else if (l != sizeof(info)) {
		log_vEFAULT();
		return TRUE;
	}

	log_notice("received signal %d: %s",
		   info.ssi_signo, strsignal(info.ssi_signo));

	g_main_loop_quit(dhcp->loop);
	return FALSE;
}

static int owfd_dhcp_run(struct owfd_dhcp *dhcp)
{
	g_main_loop_run(dhcp->loop);

	return 0;
}

static void owfd_dhcp_teardown(struct owfd_dhcp *dhcp)
{
	if (dhcp->sfd >= 0) {
		g_source_remove(dhcp->sfd_id);
		g_io_channel_unref(dhcp->sfd_chan);
		close(dhcp->sfd);
	}

	if (dhcp->loop)
		g_main_loop_unref(dhcp->loop);
}

static void sig_dummy(int sig)
{
}

static int owfd_dhcp_setup(struct owfd_dhcp *dhcp)
{
	static const int sigs[] = {
		SIGINT,
		SIGTERM,
		SIGQUIT,
		SIGHUP,
		SIGPIPE,
		0
	};
	int r, i;
	sigset_t mask;
	struct sigaction sig;

	if (geteuid())
		log_warning("not running as uid=0, dhcp might not work");

	dhcp->ifindex = if_name_to_index(dhcp->config.interface);
	if (dhcp->ifindex < 0) {
		r = -EINVAL;
		log_error("cannot find interface %s (%d)",
			  dhcp->config.interface, dhcp->ifindex);
		goto error;
	}

	dhcp->loop = g_main_loop_new(NULL, FALSE);

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

	dhcp->sfd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
	if (dhcp->sfd < 0) {
		r = log_ERRNO();
		goto error;
	}

	dhcp->sfd_chan = g_io_channel_unix_new(dhcp->sfd);
	dhcp->sfd_id = g_io_add_watch(dhcp->sfd_chan,
				      G_IO_HUP | G_IO_ERR | G_IO_IN,
				      owfd_dhcp_sfd_fn,
				      dhcp);

	return 0;

error:
	owfd_dhcp_teardown(dhcp);
	return r;
}

int main(int argc, char **argv)
{
	struct owfd_dhcp dhcp;
	int r;

	memset(&dhcp, 0, sizeof(dhcp));
	dhcp.sfd = -1;
	owfd_dhcp_init_config(&dhcp.config);

	r = owfd_dhcp_parse_argv(&dhcp.config, argc, argv);
	if (r < 0)
		goto err_out;

	if (dhcp.config.debug)
		log_max_sev = LOG_DEBUG;
	else if (dhcp.config.verbose)
		log_max_sev = LOG_INFO;
	else if (dhcp.config.silent)
		log_max_sev = LOG_ERROR;

	if (dhcp.config.silent)
		log_debug("-");
	else
		log_format(LOG_DEFAULT_BASE, NULL, LOG_SEV_NUM,
			   "openwfd_dhcp - revision %s %s %s",
			   "some-rev-TODO-xyz", __DATE__, __TIME__);

	log_info("initializing");
	r = owfd_dhcp_setup(&dhcp);
	if (r < 0)
		goto err_conf;

	log_info("running");
	r = owfd_dhcp_run(&dhcp);

	owfd_dhcp_teardown(&dhcp);
err_conf:
	owfd_dhcp_clear_config(&dhcp.config);
	if (r < 0) {
		errno = -r;
		log_error("initialization failed (%d): %m", r);
	}
	log_info("exiting");
err_out:
	return -r;
}
