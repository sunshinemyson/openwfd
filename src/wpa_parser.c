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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shared.h"
#include "wpa.h"

void owfd_wpa_event_init(struct owfd_wpa_event *ev)
{
	memset(ev, 0, sizeof(*ev));
}

void owfd_wpa_event_reset(struct owfd_wpa_event *ev)
{
	free(ev->raw);

	memset(ev, 0, sizeof(*ev));
}

static const struct event_type {
	const char *name;
	size_t len;
	unsigned int code;
} event_list[] = {

#define EVENT(_name, _suffix) { \
		.name = _name, \
		.len = sizeof(_name) - 1, \
		.code = OWFD_WPA_EVENT_ ## _suffix \
	}

	/* MUST BE ORDER ALPHABETICALLY FOR BINARY SEARCH! */

	EVENT("AP-STA-CONNECTED", AP_STA_CONNECTED),
	EVENT("AP-STA-DISCONNECTED", AP_STA_DISCONNECTED),
	EVENT("P2P-DEVICE-FOUND", P2P_DEVICE_FOUND),
	EVENT("P2P-GO-NEG-FAILURE", P2P_GO_NEG_FAILURE),
	EVENT("P2P-GO-NEG-REQUEST", P2P_GO_NEG_REQUEST),
	EVENT("P2P-GO-NEG-SUCCESS", P2P_GO_NEG_SUCCESS),
	EVENT("P2P-GROUP-FORMATION-FAILURE", P2P_GROUP_FORMATION_FAILURE),
	EVENT("P2P-GROUP-FORMATION-SUCCESS", P2P_GROUP_FORMATION_SUCCESS),
	EVENT("P2P-GROUP-REMOVED", P2P_GROUP_REMOVED),
	EVENT("P2P-GROUP-STARTED", P2P_GROUP_STARTED),
	EVENT("P2P-INVITATION-RECEIVED", P2P_INVITATION_RECEIVED),
	EVENT("P2P-INVITATION-RESULT", P2P_INVITATION_RESULT),
	EVENT("P2P-PROV-DISC-ENTER-PIN", P2P_PROV_DISC_ENTER_PIN),
	EVENT("P2P-PROV-DISC-PBC-REQ", P2P_PROV_DISC_PBC_REQ),
	EVENT("P2P-PROV-DISC-PBC-RESP", P2P_PROV_DISC_PBC_RESP),
	EVENT("P2P-PROV-DISC-SHOW-PIN", P2P_PROV_DISC_SHOW_PIN),
	EVENT("P2P-SERV-DISC-REQ", P2P_SERV_DISC_REQ),
	EVENT("P2P-SERV-DISC-RESP", P2P_SERV_DISC_RESP),

#undef EVENT
};

static int event_comp(const void *key, const void *type)
{
	const struct event_type *t;
	const char *k;
	int r;

	k = key;
	t = type;

	r = strncmp(k, t->name, t->len);
	if (r)
		return r;

	if (k[t->len] != 0 && k[t->len] != ' ')
		return 1;

	return 0;
}

int owfd_wpa_event_parse(struct owfd_wpa_event *ev, const char *event)
{
	const char *t;
	char *end;
	struct event_type *code;

	owfd_wpa_event_reset(ev);

	if (*event == '<') {
		t = strchr(event, '>');
		if (!t)
			goto unknown;

		++t;
		ev->priority = strtoul(event + 1, &end, 10);
		if (ev->priority >= OWFD_WPA_EVENT_P_COUNT ||
		    end + 1 != t ||
		    event[1] == '=' ||
		    event[1] == '-')
			ev->priority = OWFD_WPA_EVENT_P_MSGDUMP;
	} else {
		t = event;
		ev->priority = OWFD_WPA_EVENT_P_MSGDUMP;
	}

	code = bsearch(t, event_list,
		       sizeof(event_list) / sizeof(*event_list),
		       sizeof(*event_list),
		       event_comp);
	if (!code)
		goto unknown;

	ev->type = code->code;
	t += code->len;
	while (*t == ' ')
		++t;

	ev->raw = strdup(t);
	if (!ev->raw)
		goto oom;

	switch (ev->type) {
	}

	return 0;

unknown:
	ev->type = OWFD_WPA_EVENT_UNKNOWN;
	return 0;

oom:
	owfd_wpa_event_reset(ev);
	return -ENOMEM;
}
