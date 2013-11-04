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
#include <getopt.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "dhcp.h"

#define LONG_OPT_OFFSET 10000

enum {
	OPT_HELP,
	OPT_VERBOSE,
	OPT_SILENT,
	OPT_DEBUG,

	OPT_CLIENT,
	OPT_SERVER,

	OPT_INTERFACE,
	OPT_IP_BINARY,

	OPT_LOCAL,
	OPT_GATEWAY,
	OPT_DNS,
	OPT_SUBNET,
	OPT_IP_FROM,
	OPT_IP_TO,
};

const char short_options[] = ":hvcsi:";

#define OPT(_name, _arg, _val) \
	{ .name = _name, .has_arg = _arg, .val = LONG_OPT_OFFSET + _val }
const struct option long_options[] = {
	OPT("help", 0, OPT_HELP),
	OPT("verbose", 0, OPT_VERBOSE),
	OPT("silent", 0, OPT_SILENT),
	OPT("debug", 0, OPT_DEBUG),

	OPT("client", 0, OPT_CLIENT),
	OPT("server", 0, OPT_SERVER),

	OPT("interface", 1, OPT_INTERFACE),
	OPT("ip-binary", 1, OPT_IP_BINARY),

	OPT("local", 1, OPT_LOCAL),
	OPT("gateway", 1, OPT_GATEWAY),
	OPT("dns", 1, OPT_DNS),
	OPT("subnet", 1, OPT_SUBNET),
	OPT("ip-from", 1, OPT_IP_FROM),
	OPT("ip-to", 1, OPT_IP_TO),

	OPT(NULL, 0, 0),
};
#undef OPT

void owfd_dhcp_init_config(struct owfd_dhcp_config *conf)
{
	memset(conf, 0, sizeof(*conf));
}

void owfd_dhcp_clear_config(struct owfd_dhcp_config *conf)
{
	free(conf->interface);
	free(conf->ip_binary);

	free(conf->local);
	free(conf->gateway);
	free(conf->dns);
	free(conf->subnet);
	free(conf->ip_from);
	free(conf->ip_to);
}

static void show_help(void)
{
	/*
	 * Usage/Help information
	 * This should be scaled to a maximum of 80 characters per line:
	 *
	 * 80 char line:
	 *       |   10   |    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "12345678901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 * 80 char line starting with tab:
	 *       |10|    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "\t901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 */
	fprintf(stderr,
		"Usage:\n"
		"\t%1$s [options]\n"
		"\t%1$s -h [options]\n"
		"\n"
		"All addresses must be given as IPv6 address. If you want to pass an IPv4\n"
		"address, use '::FFFF:<ipv4>' as usual.\n"
		"\n"
		"General Options:\n"
		"\t-h, --help                  [off]   Print this help and exit\n"
		"\t-v, --verbose               [off]   Print verbose messages\n"
		"\t    --debug                 [off]   Enable debug mode\n"
		"\t    --silent                [off]   Suppress notices and warnings\n"
		"\n"
		"Modus Options:\n"
		"\t-c, --client                [off]   Run as DHCP client\n"
		"\t-s, --server                [off]   Run as DHCP server\n"
		"\n"
		"Network Options:\n"
		"\t-i, --interface <wlan0>     []      Wireless interface to run on\n"
		"\t    --ip-binary </path>     [%2$s]\n"
		"\t                                    Path to 'ip' binary\n"
		"\n"
		"Server Options:\n"
		"\t    --local <addr>          []      Local IPv6 address\n"
		"\t    --gateway <addr>        []      Gateway IPv6 address\n"
		"\t    --dns <addr>            []      DNS-Server IPv6 address\n"
		"\t    --subnet <mask>         []      Subnet mask\n"
		"\t    --ip-from <addr>        []      Server IPv6-range start address\n"
		"\t    --ip-to <addr>          []      Server IPv6-range end address\n"
		, "openwfd_dhcp",
		BUILD_BINDIR_IP "/ip");
	/*
	 * 80 char line:
	 *       |   10   |    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "12345678901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 * 80 char line starting with tab:
	 *       |10|    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "\t901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 */
}

static int OOM(void)
{
	fprintf(stderr, "out of memory\n");
	return -ENOMEM;
}

static int verify_address(const char *argname, const char *argval)
{
	int r;
	struct in6_addr addr;

	if (!argval) {
		fprintf(stderr, "no value given for %s\n", argname);
		return -EINVAL;
	}

	r = inet_pton(AF_INET6, argval, &addr);
	if (r != 1) {
		fprintf(stderr, "invalid IPv6 address for %s\n", argname);
		return -EINVAL;
	}

	return 0;
}

int owfd_dhcp_parse_argv(struct owfd_dhcp_config *conf, int argc, char **argv)
{
	int c;
	bool help = false;
	char *t;
	int r;

	opterr = 0;
	while (1) {
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c <= 0) {
			break;
		} else if (c == ':') {
			fprintf(stderr, "missing argument for: %s\n",
				argv[optind - 1]);
			return -EINVAL;
		} else if (c == '?') {
			if (optopt && optopt < LONG_OPT_OFFSET)
				fprintf(stderr, "unknown argument: -%c\n",
					optopt);
			else if (!optopt)
				fprintf(stderr, "unknown argument: %s\n",
					argv[optind - 1]);
			else
				fprintf(stderr, "option takes no arg: %s\n",
					argv[optind - 1]);
			return -EINVAL;
		}

#define OPT(_num) LONG_OPT_OFFSET + _num
		switch (c) {
		case 'h':
		case OPT(OPT_HELP):
			help = true;
			break;
		case 'v':
		case OPT(OPT_VERBOSE):
			conf->verbose = 1;
			break;
		case OPT(OPT_SILENT):
			conf->silent = 1;
			break;
		case OPT(OPT_DEBUG):
			conf->debug = 1;
			break;

		case 'c':
		case OPT(OPT_CLIENT):
			conf->server = 0;
			conf->client = 1;
			break;
		case 's':
		case OPT(OPT_SERVER):
			conf->client = 0;
			conf->server = 1;
			break;

		case 'i':
		case OPT(OPT_INTERFACE):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->interface);
			conf->interface = t;
			break;
		case OPT(OPT_IP_BINARY):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->ip_binary);
			conf->ip_binary = t;
			break;

		case OPT(OPT_LOCAL):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->local);
			conf->local = t;
			break;
		case OPT(OPT_GATEWAY):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->gateway);
			conf->gateway = t;
			break;
		case OPT(OPT_DNS):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->dns);
			conf->dns = t;
			break;
		case OPT(OPT_SUBNET):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->subnet);
			conf->subnet = t;
			break;
		case OPT(OPT_IP_FROM):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->ip_from);
			conf->ip_from = t;
			break;
		case OPT(OPT_IP_TO):
			t = strdup(optarg);
			if (!t)
				return OOM();
			free(conf->ip_to);
			conf->ip_to = t;
			break;
		}
#undef OPT
	}

	if (help) {
		show_help();
		return -EAGAIN;
	}

	if (optind < argc) {
		fprintf(stderr,
			"unparsed remaining arguments starting with: %s\n",
			argv[optind]);
		return -EINVAL;
	}

	if (!conf->client && !conf->server) {
		fprintf(stderr,
			"no --client or --server given\n");
		return -EINVAL;
	}

	if (!conf->interface) {
		fprintf(stderr, "no interface given, use: -i <iface>\n");
		return -EINVAL;
	}

	if (!conf->ip_binary) {
		conf->ip_binary = strdup(BUILD_BINDIR_IP "/ip");
		if (!conf->ip_binary)
			return OOM();
	}

	if (conf->server) {
		r = verify_address("--local", conf->local);
		if (r < 0)
			return r;
		r = verify_address("--gateway", conf->gateway);
		if (r < 0)
			return r;
		r = verify_address("--dns", conf->dns);
		if (r < 0)
			return r;
		r = verify_address("--subnet", conf->subnet);
		if (r < 0)
			return r;
		r = verify_address("--ip-from", conf->ip_from);
		if (r < 0)
			return r;
		r = verify_address("--ip-to", conf->ip_to);
		if (r < 0)
			return r;
	}

	return 0;
}
