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

#ifndef OPENWFD_WFD_DEFS_H
#define OPENWFD_WFD_DEFS_H

#include <endian.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup wfd_defs Wifi-Display Definitions
 * Definitions from the Wifi-Display specification
 *
 * This section contains definitions and constants from the Wifi-Display
 * specification.
 *
 * @{
 */

#define OPENWFD__WFD_PACKED __attribute__((__packed__))

/*
 * IE elements
 */

#define OPENWFD_WFD_IE_ID 0xdd

#define OPENWFD_WFD_IE_OUI_1_0 0x506f9a0a

#define OPENWFD_WFD_IE_DATA_MAX 251

struct openwfd_wfd_ie {
	uint8_t element_id;
	uint8_t length;
	uint32_t oui;
	uint8_t data[];
} OPENWFD__WFD_PACKED;

/*
 * IE subelements
 */

enum openwfd_wfd_ie_sub_type {
	OPENWFD_WFD_IE_SUB_DEV_INFO			= 0,
	OPENWFD_WFD_IE_SUB_ASSOC_BSSID			= 1,
	OPENWFD_WFD_IE_SUB_AUDIO_FORMATS		= 2,
	OPENWFD_WFD_IE_SUB_VIDEO_FORMATS		= 3,
	OPENWFD_WFD_IE_SUB_3D_FORMATS			= 4,
	OPENWFD_WFD_IE_SUB_CONTENT_PROTECT		= 5,
	OPENWFD_WFD_IE_SUB_COUPLED_SINK			= 6,
	OPENWFD_WFD_IE_SUB_EXT_CAP			= 7,
	OPENWFD_WFD_IE_SUB_LOCAL_IP			= 8,
	OPENWFD_WFD_IE_SUB_SESSION_INFO			= 9,
	OPENWFD_WFD_IE_SUB_ALT_MAC			= 10,
	OPENWFD_WFD_IE_SUB_NUM
};

struct openwfd_wfd_ie_sub {
	uint8_t subelement_id;
	uint16_t length;
	uint8_t data[];
} OPENWFD__WFD_PACKED;

/*
 * IE subelement device information
 */

/* role */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_ROLE_MASK			0x0003
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SOURCE			0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PRIMARY_SINK		0x0001
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SECONDARY_SINK		0x0002
#define OPENWFD_WFD_IE_SUB_DEV_INFO_DUAL_ROLE			0x0003

/* coupled sink as source */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SRC_COUPLED_SINK_MASK	0x0004
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SRC_NO_COUPLED_SINK		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SRC_CAN_COUPLED_SINK	0x0004

/* coupled sink as sink */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SINK_COUPLED_SINK_MASK	0x0008
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SINK_NO_COUPLED_SINK	0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_SINK_CAN_COUPLED_SINK	0x0008

/* availability for session establishment */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_AVAILABLE_MASK		0x0030
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NOT_AVAILABLE		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_AVAILABLE			0x0010

/* WFD service discovery */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_WSD_MASK			0x0040
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_WSD			0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_CAN_WSD			0x0040

/* preferred connectivity */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PC_MASK			0x0080
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PREFER_P2P			0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PREFER_TDLS			0x0080

/* content protection */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_CP_MASK			0x0100
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_CP			0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_CAN_CP			0x0100

/* separate time-sync */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_TIME_SYNC_MASK		0x0200
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_TIME_SYNC		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_CAN_TIME_SYNC		0x0200

/* no audio */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_AUDIO_MASK		0x0400
#define OPENWFD_WFD_IE_SUB_DEV_INFO_CAN_AUDIO			0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_AUDIO			0x0400

/* audio only */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_AUDIO_ONLY_MASK		0x0800
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_AUDIO_ONLY		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_AUDIO_ONLY			0x0800

/* persistent TLDS */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PERSIST_TLDS_MASK		0x1000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_PERSIST_TLDS		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_PERSIST_TLDS		0x1000

/* persistent TLDS group re-invoke */
#define OPENWFD_WFD_IE_SUB_DEV_INFO_TLDS_REINVOKE_MASK		0x2000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_NO_TLDS_REINVOKE		0x0000
#define OPENWFD_WFD_IE_SUB_DEV_INFO_TLDS_REINVOKE		0x2000

#define OPENWFD_WFD_IE_SUB_DEV_INFO_DEFAULT_PORT 7236

struct openwfd_wfd_ie_sub_dev_info {
	uint16_t dev_info;
	uint16_t ctrl_port;
	uint16_t max_throughput;
} OPENWFD__WFD_PACKED;

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* OPENWFD_WFD_DEFS_H */
