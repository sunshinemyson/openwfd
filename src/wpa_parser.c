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

	switch (ev->type) {
	case OWFD_WPA_EVENT_P2P_DEVICE_FOUND:
		free(ev->p.p2p_device_found.name);
		break;
	case OWFD_WPA_EVENT_P2P_PROV_DISC_SHOW_PIN:
		free(ev->p.p2p_prov_disc_show_pin.pin);
		break;
	default:
		break;
	}

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

static char *tokenize(const char *src, size_t *num)
{
	char *buf, *dst;
	char last_c;
	size_t n;
	bool quoted, escaped;

	buf = malloc(strlen(src) + 1);
	if (!buf)
		return NULL;

	dst = buf;
	last_c = 0;
	n = 0;
	*dst = 0;
	quoted = 0;
	escaped = 0;

	for ( ; *src; ++src) {
		if (quoted) {
			if (escaped) {
				escaped = 0;
				last_c = *src;
				*dst++ = last_c;
			} else if (*src == '\'') {
				quoted = 0;
			} else if (*src == '\\') {
				escaped = 1;
			} else {
				last_c = *src;
				*dst++ = last_c;
			}
		} else {
			if (*src == ' ' ||
			    *src == '\n' ||
			    *src == '\t' ||
			    *src == '\r') {
				if (last_c) {
					*dst++ = 0;
					++n;
				}
				last_c = 0;
			} else if (*src == '\'') {
				quoted = 1;
				escaped = 0;
				last_c = *src;
			} else {
				last_c = *src;
				*dst++ = last_c;
			}
		}
	}

	if (last_c) {
		*dst = 0;
		++n;
	}

	*num = n;
	return buf;
}

static int parse_mac(char *buf, const char *src)
{
	int r, a1, a2, a3, a4, a5, a6;

	if (strlen(src) > 17)
		return -EINVAL;

	r = sscanf(src, "%2x:%2x:%2x:%2x:%2x:%2x",
		   &a1, &a2, &a3, &a4, &a5, &a6);
	if (r != 6)
		return -EINVAL;

	strcpy(buf, src);
	return 0;
}

static int parse_ap_sta_connected(struct owfd_wpa_event *ev,
				  char *tokens, size_t num)
{
	int r;

	if (num < 1)
		return -EINVAL;

	r = parse_mac(ev->p.ap_sta_connected.mac, tokens);
	if (r < 0)
		return r;

	return 0;
}

static int parse_ap_sta_disconnected(struct owfd_wpa_event *ev,
				     char *tokens, size_t num)
{
	int r;

	if (num < 1)
		return -EINVAL;

	r = parse_mac(ev->p.ap_sta_disconnected.mac, tokens);
	if (r < 0)
		return r;

	return 0;
}

static int parse_p2p_device_found(struct owfd_wpa_event *ev,
				  char *tokens, size_t num)
{
	int r;
	size_t i;

	if (num < 2)
		return -EINVAL;

	r = parse_mac(ev->p.p2p_device_found.peer_mac, tokens);
	if (r < 0)
		return r;

	tokens += strlen(tokens) +  1;
	for (i = 1; i < num; ++i, tokens += strlen(tokens) + 1) {
		if (strncmp(tokens, "name=", 5))
			continue;

		ev->p.p2p_device_found.name = strdup(&tokens[5]);
		if (!ev->p.p2p_device_found.name)
			return -ENOMEM;

		break;
	}

	return 0;
}

static int parse_p2p_prov_disc_show_pin(struct owfd_wpa_event *ev,
					char *tokens, size_t num)
{
	int r;

	if (num < 2)
		return -EINVAL;

	r = parse_mac(ev->p.p2p_prov_disc_show_pin.peer_mac, tokens);
	if (r < 0)
		return r;

	tokens += strlen(tokens) +  1;
	ev->p.p2p_prov_disc_show_pin.pin = strdup(tokens);
	if (!ev->p.p2p_prov_disc_show_pin.pin)
		return -ENOMEM;

	return 0;
}

static int parse_p2p_prov_disc_enter_pin(struct owfd_wpa_event *ev,
					 char *tokens, size_t num)
{
	int r;

	if (num < 1)
		return -EINVAL;

	r = parse_mac(ev->p.p2p_prov_disc_enter_pin.peer_mac, tokens);
	if (r < 0)
		return r;

	return 0;
}

static int parse_p2p_prov_disc_pbc_req(struct owfd_wpa_event *ev,
				       char *tokens, size_t num)
{
	int r;

	if (num < 1)
		return -EINVAL;

	r = parse_mac(ev->p.p2p_prov_disc_pbc_req.peer_mac, tokens);
	if (r < 0)
		return r;

	return 0;
}

static int parse_p2p_prov_disc_pbc_resp(struct owfd_wpa_event *ev,
					char *tokens, size_t num)
{
	int r;

	if (num < 1)
		return -EINVAL;

	r = parse_mac(ev->p.p2p_prov_disc_pbc_resp.peer_mac, tokens);
	if (r < 0)
		return r;

	return 0;
}

int owfd_wpa_event_parse(struct owfd_wpa_event *ev, const char *event)
{
	const char *t;
	char *end, *tokens = NULL;
	size_t num;
	struct event_type *code;
	int r;

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
	if (!ev->raw) {
		r = -ENOMEM;
		goto error;
	}

	tokens = tokenize(ev->raw, &num);
	if (!tokens) {
		r = -ENOMEM;
		goto error;
	}

	switch (ev->type) {
	case OWFD_WPA_EVENT_AP_STA_CONNECTED:
		r = parse_ap_sta_connected(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_AP_STA_DISCONNECTED:
		r = parse_ap_sta_disconnected(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_P2P_DEVICE_FOUND:
		r = parse_p2p_device_found(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_P2P_PROV_DISC_SHOW_PIN:
		r = parse_p2p_prov_disc_show_pin(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_P2P_PROV_DISC_ENTER_PIN:
		r = parse_p2p_prov_disc_enter_pin(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_REQ:
		r = parse_p2p_prov_disc_pbc_req(ev, tokens, num);
		break;
	case OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_RESP:
		r = parse_p2p_prov_disc_pbc_resp(ev, tokens, num);
		break;
	default:
		r = 0;
		break;
	}

	free(tokens);

	if (r < 0)
		goto error;

	return 0;

unknown:
	ev->type = OWFD_WPA_EVENT_UNKNOWN;
	return 0;

error:
	owfd_wpa_event_reset(ev);
	return r;
}
