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
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "p2pd.h"

#define LONG_OPT_OFFSET 10000

enum {
	OPT_HELP,
	OPT_VERBOSE,
	OPT_SILENT,
	OPT_DEBUG,
};

const char short_options[] = ":hv";

#define OPT(_name, _arg, _val) \
	{ .name = _name, .has_arg = _arg, .val = LONG_OPT_OFFSET + _val }
const struct option long_options[] = {
	OPT("help", 0, OPT_HELP),
	OPT("verbose", 0, OPT_VERBOSE),
	OPT("silent", 0, OPT_SILENT),
	OPT("debug", 0, OPT_DEBUG),
	OPT(NULL, 0, 0),
};
#undef OPT

void owfd_p2pd_init_config(struct owfd_p2pd_config *conf)
{
	memset(conf, 0, sizeof(*conf));
}

void owfd_p2pd_clear_config(struct owfd_p2pd_config *conf)
{
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
		"General Options:\n"
		"\t-h, --help                  [off]   Print this help and exit\n"
		"\t-v, --verbose               [off]   Print verbose messages\n"
		"\t    --debug                 [off]   Enable debug mode\n"
		"\t    --silent                [off]   Suppress notices and warnings\n",
		"openwfd_p2pd");
	/*
	 * 80 char line:
	 *       |   10   |    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "12345678901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 * 80 char line starting with tab:
	 *       |10|    20   |    30   |    40   |    50   |    60   |    70   |    80   |
	 *      "\t901234567890123456789012345678901234567890123456789012345678901234567890\n"
	 */
}

int owfd_p2pd_parse_argv(struct owfd_p2pd_config *conf, int argc, char **argv)
{
	int c;
	bool help = false;

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
		}
#undef OPT
	}

	if (optind < argc) {
		fprintf(stderr,
			"unparsed remaining arguments starting with: %s\n",
			argv[optind]);
		return -EINVAL;
	}

	if (help) {
		show_help();
		return -EAGAIN;
	}

	return 0;
}
