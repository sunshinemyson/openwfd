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

#ifndef OWFD_WPA_H
#define OWFD_WPA_H

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* wpa ctrl */

struct owfd_wpa_ctrl;

typedef void (*owfd_wpa_ctrl_cb) (struct owfd_wpa_ctrl *wpa, void *buf,
				  size_t len, void *data);

int owfd_wpa_ctrl_new(struct owfd_wpa_ctrl **out);
void owfd_wpa_ctrl_ref(struct owfd_wpa_ctrl *wpa);
void owfd_wpa_ctrl_unref(struct owfd_wpa_ctrl *wpa);

void owfd_wpa_ctrl_set_data(struct owfd_wpa_ctrl *wpa, void *data);
void *owfd_wpa_ctrl_get_data(struct owfd_wpa_ctrl *wpa);

int owfd_wpa_ctrl_open(struct owfd_wpa_ctrl *wpa, const char *ctrl_path,
		       owfd_wpa_ctrl_cb cb);
void owfd_wpa_ctrl_close(struct owfd_wpa_ctrl *wpa);
bool owfd_wpa_ctrl_is_open(struct owfd_wpa_ctrl *wpa);

int owfd_wpa_ctrl_get_fd(struct owfd_wpa_ctrl *wpa);
void owfd_wpa_ctrl_set_sigmask(struct owfd_wpa_ctrl *wpa,
			       const sigset_t *mask);
int owfd_wpa_ctrl_dispatch(struct owfd_wpa_ctrl *wpa, int timeout);

int owfd_wpa_ctrl_request(struct owfd_wpa_ctrl *wpa, const void *cmd,
			  size_t cmd_len, void *reply, size_t *reply_len,
			  int timeout);
int owfd_wpa_ctrl_request_ok(struct owfd_wpa_ctrl *wpa, const void *cmd,
			     size_t cmd_len, int timeout);

/* wpa parser */

enum owfd_wpa_event_type {
	OWFD_WPA_EVENT_UNKNOWN,
	OWFD_WPA_EVENT_AP_STA_CONNECTED,
	OWFD_WPA_EVENT_AP_STA_DISCONNECTED,
	OWFD_WPA_EVENT_P2P_DEVICE_FOUND,
	OWFD_WPA_EVENT_P2P_GO_NEG_REQUEST,
	OWFD_WPA_EVENT_P2P_GO_NEG_SUCCESS,
	OWFD_WPA_EVENT_P2P_GO_NEG_FAILURE,
	OWFD_WPA_EVENT_P2P_GROUP_FORMATION_SUCCESS,
	OWFD_WPA_EVENT_P2P_GROUP_FORMATION_FAILURE,
	OWFD_WPA_EVENT_P2P_GROUP_STARTED,
	OWFD_WPA_EVENT_P2P_GROUP_REMOVED,
	OWFD_WPA_EVENT_P2P_PROV_DISC_SHOW_PIN,
	OWFD_WPA_EVENT_P2P_PROV_DISC_ENTER_PIN,
	OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_REQ,
	OWFD_WPA_EVENT_P2P_PROV_DISC_PBC_RESP,
	OWFD_WPA_EVENT_P2P_SERV_DISC_REQ,
	OWFD_WPA_EVENT_P2P_SERV_DISC_RESP,
	OWFD_WPA_EVENT_P2P_INVITATION_RECEIVED,
	OWFD_WPA_EVENT_P2P_INVITATION_RESULT,
	OWFD_WPA_EVENT_COUNT,
};

enum owfd_wpa_event_priority {
	OWFD_WPA_EVENT_P_MSGDUMP,
	OWFD_WPA_EVENT_P_DEBUG,
	OWFD_WPA_EVENT_P_INFO,
	OWFD_WPA_EVENT_P_WARNING,
	OWFD_WPA_EVENT_P_ERROR,
	OWFD_WPA_EVENT_P_COUNT
};

#define OWFD_WPA_EVENT_MAC_STRLEN 18

struct owfd_wpa_event {
	unsigned int type;
	unsigned int priority;
	char *raw;

	union owfd_wpa_event_payload {
		struct owfd_wpa_event_ap_sta_connected {
			char mac[OWFD_WPA_EVENT_MAC_STRLEN];
		} ap_sta_connected;
		struct owfd_wpa_event_ap_sta_disconnected {
			char mac[OWFD_WPA_EVENT_MAC_STRLEN];
		} ap_sta_disconnected;
		struct owfd_wpa_event_p2p_device_found {
			char peer_mac[OWFD_WPA_EVENT_MAC_STRLEN];
			char *name;
		} p2p_device_found;
		struct owfd_wpa_event_p2p_prov_disc_show_pin {
			char peer_mac[OWFD_WPA_EVENT_MAC_STRLEN];
			char *pin;
		} p2p_prov_disc_show_pin;
		struct owfd_wpa_event_p2p_prov_disc_enter_pin {
			char peer_mac[OWFD_WPA_EVENT_MAC_STRLEN];
		} p2p_prov_disc_enter_pin;
		struct owfd_wpa_event_p2p_prov_disc_pbc_req {
			char peer_mac[OWFD_WPA_EVENT_MAC_STRLEN];
		} p2p_prov_disc_pbc_req;
		struct owfd_wpa_event_p2p_prov_disc_pbc_resp {
			char peer_mac[OWFD_WPA_EVENT_MAC_STRLEN];
		} p2p_prov_disc_pbc_resp;
	} p;
};

void owfd_wpa_event_init(struct owfd_wpa_event *ev);
void owfd_wpa_event_reset(struct owfd_wpa_event *ev);
int owfd_wpa_event_parse(struct owfd_wpa_event *ev, const char *event);
const char *owfd_wpa_event_name(unsigned int type);

#ifdef __cplusplus
}
#endif

#endif /* OWFD_WPA_H */
