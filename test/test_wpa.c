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

#include "test_common.h"

static void parse(struct owfd_wpa_event *ev, const char *event)
{
	int r;

	owfd_wpa_event_init(ev);
	r = owfd_wpa_event_parse(ev, event);
	ck_assert(!r);
	ck_assert(ev->priority < OWFD_WPA_EVENT_P_COUNT);
}

static const char *event_list[] = {
	[OWFD_WPA_EVENT_UNKNOWN]			= "",
	[OWFD_WPA_EVENT_AP_STA_CONNECTED]		= "AP-STA-CONNECTED",
	[OWFD_WPA_EVENT_AP_STA_DISCONNECTED]		= "AP-STA-DISCONNECTED",
	[OWFD_WPA_EVENT_P2P_DEVICE_FOUND]		= "P2P-DEVICE-FOUND",
	[OWFD_WPA_EVENT_P2P_GO_NEG_REQUEST]		= "P2P-GO-NEG-REQUEST",
	[OWFD_WPA_EVENT_P2P_GO_NEG_SUCCESS]		= "P2P-GO-NEG-SUCCESS",
	[OWFD_WPA_EVENT_P2P_GO_NEG_FAILURE]		= "P2P-GO-NEG-FAILURE",
	[OWFD_WPA_EVENT_P2P_GROUP_FORMATION_SUCCESS]	= "P2P-GROUP-FORMATION-SUCCESS",
	[OWFD_WPA_EVENT_P2P_GROUP_FORMATION_FAILURE]	= "P2P-GROUP-FORMATION-FAILURE",
	[OWFD_WPA_EVENT_P2P_GROUP_STARTED]		= "P2P-GROUP-STARTED",
	[OWFD_WPA_EVENT_P2P_GROUP_REMOVED]		= "P2P-GROUP-REMOVED",
	[OWFD_WPA_EVENT_P2P_PROV_DISC_SHOW_PIN]		= "P2P-PROV-DISC-SHOW-PIN",
	[OWFD_WPA_EVENT_P2P_PROV_DISC_ENTER_PIN]	= "P2P-PROV-DISC-ENTER-PIN",
	[OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_REQ]		= "P2P-PROV-DISC-PBC-REQ",
	[OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_RESP]		= "P2P-PROV-DISC-PBC-RESP",
	[OWFD_WPA_EVENT_P2P_SERV_DISC_REQ]		= "P2P-SERV-DISC-REQ",
	[OWFD_WPA_EVENT_P2P_SERV_DISC_RESP]		= "P2P-SERV-DISC-RESP",
	[OWFD_WPA_EVENT_P2P_INVITATION_RECEIVED]	= "P2P-INVITATION-RECEIVED",
	[OWFD_WPA_EVENT_P2P_INVITATION_RESULT]		= "P2P-INVITATION-RESULT",
	[OWFD_WPA_EVENT_COUNT] = NULL
};

START_TEST(test_wpa_parser)
{
	struct owfd_wpa_event ev;
	int i;

	parse(&ev, "");
	ck_assert(ev.type == OWFD_WPA_EVENT_UNKNOWN);

	parse(&ev, "asdf");
	ck_assert(ev.type == OWFD_WPA_EVENT_UNKNOWN);

	for (i = 0; i < OWFD_WPA_EVENT_COUNT; ++i) {
		ck_assert_msg(event_list[i] != NULL, "event %d missing", i);
		parse(&ev, event_list[i]);
		ck_assert_msg(ev.type == i, "event %d invalid", i);
	}

	parse(&ev, "<5>AP-STA-CONNECTED");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_MSGDUMP);
	ck_assert(ev.type == OWFD_WPA_EVENT_AP_STA_CONNECTED);

	parse(&ev, "<4>AP-STA-CONNECTED");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_ERROR);
	ck_assert(ev.type == OWFD_WPA_EVENT_AP_STA_CONNECTED);

	parse(&ev, "<4>AP-STA-CONNECTED2");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_ERROR);
	ck_assert(ev.type == OWFD_WPA_EVENT_UNKNOWN);

	parse(&ev, "<4asdf>AP-STA-CONNECTED");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_MSGDUMP);
	ck_assert(ev.type == OWFD_WPA_EVENT_AP_STA_CONNECTED);

	parse(&ev, "<4>AP-STA-CONNECTED something else");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_ERROR);
	ck_assert(ev.type == OWFD_WPA_EVENT_AP_STA_CONNECTED);
	ck_assert(ev.raw != NULL);
	ck_assert(!strcmp(ev.raw, "something else"));

	parse(&ev, "<4>AP-STA something else");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_ERROR);
	ck_assert(ev.type == OWFD_WPA_EVENT_UNKNOWN);
	ck_assert(!ev.raw);

	parse(&ev, "<4>AP-STA-CONNECTED");
	ck_assert(ev.priority == OWFD_WPA_EVENT_P_ERROR);
	ck_assert(ev.type == OWFD_WPA_EVENT_AP_STA_CONNECTED);
	ck_assert(ev.raw != NULL);
	ck_assert(!*ev.raw);
}
END_TEST

TEST_DEFINE_CASE(parser)
	TEST(test_wpa_parser)
TEST_END_CASE

TEST_DEFINE(
	TEST_SUITE(wpa,
		TEST_CASE(parser),
		TEST_END
	)
)
