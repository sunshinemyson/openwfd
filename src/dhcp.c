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

/*
 * Small DHCP Client/Server for OpenWFD
 * Wifi-P2P requires us to use DHCP to set up a private P2P network. As all
 * DHCP daemons available have horrible interfaces for ad-hoc setups, we have
 * this small replacement for all DHCP operations.
 *
 * This program implements a DHCP server and daemon. See --help for usage
 * information. We build on gdhcp from connman as the underlying DHCP protocol
 * implementation. To configure network devices, we actually invoke the "ip"
 * binary.
 *
 * Note that this is a gross hack! We don't intend to provide a fully functional
 * DHCP server or client here. This is only a replacement for the current lack
 * of Wifi-P2P support in common network managers. Once they gain proper
 * support, we will drop this helper!
 *
 * The "ip" invokation is quite fragile and ugly. However, performing these
 * steps directly involves netlink operations and more. As no-one came up with
 * patches, yet, we keep the hack. To anyone trying to fix it: Please, spend
 * this time hacking on NetworkManager, connman and friends instead! If they
 * gain Wifi-P2P support, this whole thing will get trashed.
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
#include <sys/wait.h>
#include <unistd.h>
#include "dhcp.h"
#include "gdhcp/gdhcp.h"
#include "shared.h"
#include "shl_log.h"

struct owfd_dhcp {
	struct owfd_dhcp_config config;
	int ifindex;
	char *iflabel;
	GMainLoop *loop;

	int sfd;
	GIOChannel *sfd_chan;
	guint sfd_id;

	GDHCPClient *client;
	char *client_addr;

	GDHCPServer *server;
};

static int flush_if_addr(struct owfd_dhcp *dhcp)
{
	char *argv[64];
	int i, r;
	pid_t pid, rp;
	sigset_t mask;

	pid = fork();
	if (pid < 0) {
		return log_ERRNO();
	} else if (!pid) {
		/* child */

		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);

		/* redirect stdout to stderr */
		dup2(2, 1);

		i = 0;
		argv[i++] = dhcp->config.ip_binary;
		argv[i++] = "addr";
		argv[i++] = "flush";
		argv[i++] = "dev";
		argv[i++] = dhcp->config.interface;
		if (dhcp->iflabel) {
			argv[i++] = "label";
			argv[i++] = dhcp->iflabel;
		}
		argv[i] = NULL;

		execve(argv[0], argv, environ);
		_exit(1);
	}

	log_info("flushing local if-addr");
	rp = waitpid(pid, &r, 0);
	if (rp != pid) {
		log_error("cannot flush local if-addr via '%s'",
			  dhcp->config.ip_binary);
		return -EFAULT;
	} else if (!WIFEXITED(r)) {
		log_error("flushing local if-addr via '%s' failed",
			  dhcp->config.ip_binary);
		return -EFAULT;
	} else if (WEXITSTATUS(r)) {
		log_error("flushing local if-addr via '%s' failed with: %d",
			  dhcp->config.ip_binary, WEXITSTATUS(r));
		return -EFAULT;
	}

	log_debug("successfully flushed local if-addr via %s",
		  dhcp->config.ip_binary);

	return 0;
}

static int add_if_addr(struct owfd_dhcp *dhcp, char *addr)
{
	char *argv[64];
	int i, r;
	pid_t pid, rp;
	sigset_t mask;

	pid = fork();
	if (pid < 0) {
		return log_ERRNO();
	} else if (!pid) {
		/* child */

		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);

		/* redirect stdout to stderr */
		dup2(2, 1);

		i = 0;
		argv[i++] = dhcp->config.ip_binary;
		argv[i++] = "addr";
		argv[i++] = "add";
		argv[i++] = addr;
		argv[i++] = "dev";
		argv[i++] = dhcp->config.interface;
		if (dhcp->iflabel) {
			argv[i++] = "label";
			argv[i++] = dhcp->iflabel;
		}
		argv[i] = NULL;

		execve(argv[0], argv, environ);
		_exit(1);
	}

	log_info("adding local if-addr %s", addr);
	rp = waitpid(pid, &r, 0);
	if (rp != pid) {
		log_error("cannot set local if-addr %s via '%s'",
			  addr, dhcp->config.ip_binary);
		return -EFAULT;
	} else if (!WIFEXITED(r)) {
		log_error("setting local if-addr %s via '%s' failed",
			  addr, dhcp->config.ip_binary);
		return -EFAULT;
	} else if (WEXITSTATUS(r)) {
		log_error("setting local if-addr %s via '%s' failed with: %d",
			  addr, dhcp->config.ip_binary, WEXITSTATUS(r));
		return -EFAULT;
	}

	log_debug("successfully set local if-addr %s via %s",
		  addr, dhcp->config.ip_binary);

	return 0;
}

static void client_lease_fn(GDHCPClient *client, gpointer data)
{
	struct owfd_dhcp *dhcp = data;
	char *addr, *a, *subnet = NULL;
	GList *l;
	int r;

	log_info("lease available");

	addr = g_dhcp_client_get_address(client);
	log_info("lease: address: %s", addr);

	l = g_dhcp_client_get_option(client, G_DHCP_SUBNET);
	for ( ; l; l = l->next) {
		subnet = subnet ? : (char*)l->data;
		log_info("lease: subnet: %s", (char*)l->data);
	}

	l = g_dhcp_client_get_option(client, G_DHCP_DNS_SERVER);
	for ( ; l; l = l->next)
		log_info("lease: dns-server: %s", (char*)l->data);

	l = g_dhcp_client_get_option(client, G_DHCP_ROUTER);
	for ( ; l; l = l->next)
		log_info("lease: router: %s", (char*)l->data);

	if (!addr) {
		log_error("lease without IP address");
		goto error;
	}
	if (!subnet) {
		log_warning("lease without subnet mask, using 24");
		subnet = "24";
	}

	r = asprintf(&a, "%s/%s", addr, subnet);
	if (r < 0) {
		log_vENOMEM();
		g_free(addr);
		goto error;
	} else {
		g_free(addr);
	}

	if (dhcp->client_addr && !strcmp(dhcp->client_addr, a)) {
		log_info("given address already set");
		free(a);
	} else {
		free(dhcp->client_addr);
		dhcp->client_addr = a;

		r = flush_if_addr(dhcp);
		if (r < 0) {
			log_error("cannot flush addr on local interface %s",
				  dhcp->config.interface);
			goto error;
		}

		r = add_if_addr(dhcp, dhcp->client_addr);
		if (r < 0) {
			log_error("cannot set parameters on local interface %s",
				  dhcp->config.interface);
			goto error;
		}
	}

	return;

error:
	g_main_loop_quit(dhcp->loop);
}

static void client_no_lease_fn(GDHCPClient *client, gpointer data)
{
	struct owfd_dhcp *dhcp = data;

	log_error("no lease available");
	g_main_loop_quit(dhcp->loop);
}

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
	int r;

	if (dhcp->config.client) {
		log_info("running dhcp client on %s via '%s'",
			 dhcp->config.interface, dhcp->config.ip_binary);

		r = g_dhcp_client_start(dhcp->client, NULL);
		if (r != 0) {
			log_error("cannot start DHCP client: %d", r);
			return -EFAULT;
		}
	} else {
		log_info("running dhcp server on %s via '%s'",
			 dhcp->config.interface, dhcp->config.ip_binary);
	}

	g_main_loop_run(dhcp->loop);

	return 0;
}

static void owfd_dhcp_teardown(struct owfd_dhcp *dhcp)
{
	if (dhcp->config.client) {
		if (dhcp->client) {
			g_dhcp_client_stop(dhcp->client);

			if (dhcp->client_addr) {
				flush_if_addr(dhcp);
				free(dhcp->client_addr);
			}

			g_dhcp_client_unref(dhcp->client);
		}
	} else {
		if (dhcp->server) {
			g_dhcp_server_unref(dhcp->server);
		}
	}

	if (dhcp->sfd >= 0) {
		g_source_remove(dhcp->sfd_id);
		g_io_channel_unref(dhcp->sfd_chan);
		close(dhcp->sfd);
	}

	if (dhcp->loop)
		g_main_loop_unref(dhcp->loop);

	free(dhcp->iflabel);
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
	GDHCPClientError cerr;
	GDHCPServerError serr;

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

	if (dhcp->config.client) {
		r = asprintf(&dhcp->iflabel, "%s:openwfd",
			     dhcp->config.interface);
		if (r < 0) {
			r = log_ERRNO();
			goto error;
		}

		dhcp->client = g_dhcp_client_new(G_DHCP_IPV4, dhcp->ifindex,
						 &cerr);
		if (!dhcp->client) {
			r = -EINVAL;

			switch (cerr) {
			case G_DHCP_CLIENT_ERROR_INTERFACE_UNAVAILABLE:
				log_error("cannot create GDHCP client: interface %s unavailable",
					  dhcp->config.interface);
				break;
			case G_DHCP_CLIENT_ERROR_INTERFACE_IN_USE:
				log_error("cannot create GDHCP client: interface %s in use",
					  dhcp->config.interface);
				break;
			case G_DHCP_CLIENT_ERROR_INTERFACE_DOWN:
				log_error("cannot create GDHCP client: interface %s down",
					  dhcp->config.interface);
				break;
			case G_DHCP_CLIENT_ERROR_NOMEM:
				r = log_ENOMEM();
				break;
			case G_DHCP_CLIENT_ERROR_INVALID_INDEX:
				log_error("cannot create GDHCP client: invalid interface %s",
					  dhcp->config.interface);
				break;
			case G_DHCP_CLIENT_ERROR_INVALID_OPTION:
				log_error("cannot create GDHCP client: invalid options");
				break;
			default:
				log_error("cannot create GDHCP client (%d)",
					  cerr);
				break;
			}

			goto error;
		}

		g_dhcp_client_set_send(dhcp->client, G_DHCP_HOST_NAME,
				       "<hostname>");

		g_dhcp_client_set_request(dhcp->client, G_DHCP_SUBNET);
		g_dhcp_client_set_request(dhcp->client, G_DHCP_DNS_SERVER);
		g_dhcp_client_set_request(dhcp->client, G_DHCP_ROUTER);

		g_dhcp_client_register_event(dhcp->client,
					     G_DHCP_CLIENT_EVENT_LEASE_AVAILABLE,
					     client_lease_fn, dhcp);
		g_dhcp_client_register_event(dhcp->client,
					     G_DHCP_CLIENT_EVENT_NO_LEASE,
					     client_no_lease_fn, dhcp);
	} else {
		dhcp->server = g_dhcp_server_new(G_DHCP_IPV4, dhcp->ifindex,
						 &serr);
		if (!dhcp->server) {
			r = -EINVAL;

			switch(serr) {
			case G_DHCP_SERVER_ERROR_INTERFACE_UNAVAILABLE:
				log_error("cannot create GDHCP server: interface %s unavailable",
					  dhcp->config.interface);
				break;
			case G_DHCP_SERVER_ERROR_INTERFACE_IN_USE:
				log_error("cannot create GDHCP server: interface %s in use",
					  dhcp->config.interface);
				break;
			case G_DHCP_SERVER_ERROR_INTERFACE_DOWN:
				log_error("cannot create GDHCP server: interface %s down",
					  dhcp->config.interface);
				break;
			case G_DHCP_SERVER_ERROR_NOMEM:
				r = log_ENOMEM();
				break;
			case G_DHCP_SERVER_ERROR_INVALID_INDEX:
				log_error("cannot create GDHCP server: invalid interface %s",
					  dhcp->config.interface);
				break;
			case G_DHCP_SERVER_ERROR_INVALID_OPTION:
				log_error("cannot create GDHCP server: invalid options");
				break;
			case G_DHCP_SERVER_ERROR_IP_ADDRESS_INVALID:
				log_error("cannot create GDHCP server: invalid ip address");
				break;
			default:
				log_error("cannot create GDHCP server (%d)",
					  serr);
				break;
			}

			goto error;
		}
	}

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
