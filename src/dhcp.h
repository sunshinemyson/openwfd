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

#ifndef OWFD_DHCP_H
#define OWFD_DHCP_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* config handling */

struct owfd_dhcp_config {
	unsigned int verbose : 1;
	unsigned int silent : 1;
	unsigned int debug : 1;

	unsigned int client : 1;
	unsigned int server : 1;

	char *interface;
	char *ip_binary;

	char *local;
	char *gateway;
	char *dns;
	char *subnet;
	char *ip_from;
	char *ip_to;
};

void owfd_dhcp_init_config(struct owfd_dhcp_config *conf);
void owfd_dhcp_clear_config(struct owfd_dhcp_config *conf);
int owfd_dhcp_parse_argv(struct owfd_dhcp_config *conf,
			 int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* OWFD_DHCP_H */
