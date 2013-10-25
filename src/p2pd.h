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

#ifndef OWFD_P2PD_H
#define OWFD_P2PD_H

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct owfd_p2pd_config {
	unsigned int verbose : 1;
	unsigned int silent : 1;
	unsigned int debug : 1;
};

void owfd_p2pd_init_config(struct owfd_p2pd_config *conf);
void owfd_p2pd_clear_config(struct owfd_p2pd_config *conf);
int owfd_p2pd_parse_argv(struct owfd_p2pd_config *conf, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* OPENWFD_P2PD_H */
