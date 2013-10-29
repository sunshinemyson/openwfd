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
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#include "p2pd.h"
#include "shared.h"
#include "shl_log.h"
#include "wpa_ctrl.h"

struct owfd_p2pd_interface {
	struct owfd_wpa_ctrl *wpa;
	struct owfd_p2pd_config *config;
	int wpa_fd;
	pid_t pid;
};

static void wpa_event(struct owfd_wpa_ctrl *wpa, void *buf,
		      size_t len, void *data);

/*
 * Execute wpa_supplicant. This is called after fork(). It shall initialize the
 * environment for wpa_supplicant and then execve() it. This should not return.
 * If it does, exit(1) is called for this child.
 */
static void run_child(struct owfd_p2pd_interface *iface)
{
	char *argv[64];
	int i, r;
	sigset_t mask;

	sigemptyset(&mask);
	sigprocmask(SIG_SETMASK, &mask, NULL);

	/* redirect stdout to stderr for wpa_supplicant */
	r = dup2(2, 1);
	if (r < 0)
		return;

	/* initialize wpa_supplicant args */
	i = 0;
	argv[i++] = iface->config->wpa_binary;
	argv[i++] = "-Dnl80211";
	argv[i++] = "-qq";
	argv[i++] = "-C";
	argv[i++] = iface->config->wpa_ctrldir;
	argv[i++] = "-i";
	argv[i++] = iface->config->interface;
	argv[i] = NULL;

	/* execute wpa_supplicant; if it fails, the caller issues exit(1) */
	execve(argv[0], argv, environ);
}

/*
 * Test whether child with pid @pid is still running. This is a non-blocking
 * call which drains the ECHLD queue in case it already died!
 */
static bool is_child_alive(pid_t pid)
{
	return !waitpid(pid, NULL, WNOHANG);
}

/*
 * Wait for wpa_supplicant startup.
 * We create an inotify-fd to wait for creation of /run/wpa_supplicant/wlan1.
 * Then we wait for wlan1 to be opened and after that we open it ourselves.
 *
 * Note that inotify-fds must always be created before testing the condition.
 * Otherwise, there's a race between testing the condition and starting the
 * inotify-watch.
 */
static int wait_for_wpa(struct owfd_p2pd_interface *iface,
			const char *file, const sigset_t *mask)
{
	int fd, r, w;
	int64_t t, start;
	struct pollfd fds[1];
	char ev[sizeof(struct inotify_event) + 1024];
	struct timespec ts;

	/* create inotify-fd */
	fd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
	if (fd < 0)
		return log_ERRNO();

	/* set 10s timeout and init poll-events */
	t = 10LL * 1000LL * 1000LL;
	fds[0].fd = fd;
	fds[0].events = POLLHUP | POLLERR | POLLIN;

	/* add /run/wpa_supplicant watch */
	w = inotify_add_watch(fd, iface->config->wpa_ctrldir,
			      IN_CREATE | IN_MOVED_TO | IN_ONLYDIR);
	if (w < 0) {
		r = log_ERRNO();
		goto err_close;
	}

	/* verify wpa_supplicant is still alive */
	if (!is_child_alive(iface->pid)) {
		log_error("wpa_supplicant died unexpectedly");
		r = -ENODEV;
		goto err_close;
	}

	/* If /run/wpa_supplicant/<iface> does not exist, start waiting for
	 * inotify events. Otherwise, skip waiting. */
	if (access(file, F_OK) < 0) {
		while (1) {
			start = get_time_us();
			fds[0].revents = 0;

			/* poll for inotify events; max 100ms per round */
			us_to_timespec(&ts,
				       (t > 100 * 1000LL) ? 100 * 1000LL : t);
			r = ppoll(fds, 1, &ts, mask);
			if (r < 0) {
				r = log_ERRNO();
				goto err_close;
			} else if (r == 1 && fds[0].revents & (POLLHUP | POLLERR)) {
				r = log_EPIPE();
				goto err_close;
			}

			/* update timeout */
			t = t - (get_time_us() - start);
			if (t < 0)
				t = 0;

			/* verify wpa_supplicant is still alive */
			if (!is_child_alive(iface->pid)) {
				log_error("wpa_supplicant died unexpectedly");
				r = -ENODEV;
				goto err_close;
			}

			/* bail out if ctrl-sock is avilable */
			if (!access(file, F_OK))
				break;

			/* drain input queue */
			read(fd, ev, sizeof(ev));

			/* check for timeout and then continue polling */
			if (t <= 0) {
				r = -ETIMEDOUT;
				log_error("waiting for wpa_supplicant startup timed out");
				goto err_close;
			}
		}
	}

	/* remove directory watch */
	inotify_rm_watch(fd, w);

	/* add socket watch */
	w = inotify_add_watch(fd, file,
			      IN_OPEN | IN_DELETE_SELF | IN_MOVE_SELF);
	if (w < 0) {
		r = log_ERRNO();
		goto err_close;
	}

	/* verify wpa_supplicant is still alive */
	if (!is_child_alive(iface->pid)) {
		log_error("wpa_supplicant died unexpectedly");
		r = -ENODEV;
		goto err_close;
	}

	/* try opening socket and bail out if we succeed */
	r = owfd_wpa_ctrl_open(iface->wpa, file, wpa_event);
	if (r >= 0)
		goto done;

	/* Socket is not initialized by wpa_supplicant, yet. Start polling for
	 * inotify events and wait until _someone_ opens it. */
	while (1) {
		start = get_time_us();
		fds[0].revents = 0;

		/* poll for events; max 100ms per round */
		us_to_timespec(&ts, (t > 100 * 1000LL) ? 100 * 1000LL : t);
		r = ppoll(fds, 1, &ts, mask);
		if (r < 0) {
			r = log_ERRNO();
			goto err_close;
		} else if (r == 1 && fds[0].revents & (POLLHUP | POLLERR)) {
			r = log_EPIPE();
			goto err_close;
		}

		/* update timeout */
		t = t - (get_time_us() - start);
		if (t < 0)
			t = 0;

		/* verify wpa_supplicant is still alive */
		if (!is_child_alive(iface->pid)) {
			log_error("wpa_supplicant died unexpectedly");
			r = -ENODEV;
			goto err_close;
		}

		/* retry opening socket */
		r = owfd_wpa_ctrl_open(iface->wpa, file, wpa_event);
		if (r >= 0)
			goto done;

		/* drain input queue */
		read(fd, ev, sizeof(ev));

		/* check for timeout and then continue polling */
		if (t <= 0) {
			r = -ETIMEDOUT;
			log_error("waiting for wpa_supplicant startup timed out");
			goto err_close;
		}
	}

done:
	r = 0;
err_close:
	close(fd);
	return r;
}

/*
 * Fork and exec wpa_supplicant. This waits for wpa_supplicant startup and
 * connects the wpa_ctrl socket. If this returns (even with an error), you
 * _must_ call kill_wpa() to stop the wpa_supplicant process later.
 */
static int fork_wpa(struct owfd_p2pd_interface *iface)
{
	pid_t pid;
	int r;
	char *ctrl;
	sigset_t mask;

	pid = fork();
	if (pid < 0) {
		return log_ERRNO();
	} else if (!pid) {
		/* child */
		run_child(iface);
		exit(1);
	}

	/* parent; wait for control-socket to be available */

	iface->pid = pid;

	r = asprintf(&ctrl, "%s/%s", iface->config->wpa_ctrldir,
		     iface->config->interface);
	if (r < 0)
		return -ENOMEM;

	log_info("waiting for wpa_supplicant startup on: %s", ctrl);

	/* allow fatal signals during blocking startup */
	sigemptyset(&mask);
	sigaddset(&mask, SIGPIPE);
	owfd_wpa_ctrl_set_sigmask(iface->wpa, &mask);

	r = wait_for_wpa(iface, ctrl, &mask);
	if (r < 0) {
		log_error("wpa_supplicant startup failed");
		free(ctrl);
		return r;
	}

	free(ctrl);
	return 0;
}

/*
 * Stop running wpa_supplicant. This tries to send a synchronous TERMINATE
 * message. If it fails, we send a signal to stop the child.
 */
static void kill_wpa(struct owfd_p2pd_interface *iface)
{
	int r;

	if (iface->pid <= 0)
		return;

	if (owfd_wpa_ctrl_is_open(iface->wpa)) {
		r = owfd_wpa_ctrl_request_ok(iface->wpa, "TERMINATE", 9, -1);
		if (r >= 0) {
			log_info("wpa_supplicant acknowledged termination request");
			return;
		}

		/* verify wpa_supplicant is still alive */
		if (!is_child_alive(iface->pid)) {
			log_info("wpa_supplicant already exited");
			return;
		}

		log_error("cannot send termination request to wpa_supplicant (%d)", r);
	}

	log_info("sending SIGTERM to wpa_supplicant");
	r = kill(iface->pid, SIGTERM);
	if (r < 0)
		log_error("cannot send SIGTERM to wpa_supplicant (%d)", r);
}

int owfd_p2pd_interface_new(struct owfd_p2pd_interface **out,
			    struct owfd_p2pd_config *conf, int efd)
{
	struct owfd_p2pd_interface *iface;
	int r;

	log_info("using interface: %s", conf->interface);

	iface = calloc(1, sizeof(*iface));
	if (!iface)
		return log_ENOMEM();
	iface->config = conf;

	r = owfd_wpa_ctrl_new(&iface->wpa);
	if (r < 0) {
		errno = -r;
		log_vERRNO();
		goto err_iface;
	}

	r = fork_wpa(iface);
	if (r < 0)
		goto err_kill;

	iface->wpa_fd = owfd_wpa_ctrl_get_fd(iface->wpa);
	r = owfd_p2pd_ep_add(efd, &iface->wpa_fd, EPOLLIN);
	if (r < 0)
		goto err_kill;

	*out = iface;
	return 0;

err_kill:
	kill_wpa(iface);
	owfd_wpa_ctrl_close(iface->wpa);
	owfd_wpa_ctrl_unref(iface->wpa);
err_iface:
	free(iface);
	return r;
}

void owfd_p2pd_interface_free(struct owfd_p2pd_interface *iface)
{
	if (!iface)
		return;

	kill_wpa(iface);
	owfd_wpa_ctrl_close(iface->wpa);
	owfd_wpa_ctrl_unref(iface->wpa);
	free(iface);
}

int owfd_p2pd_interface_dispatch(struct owfd_p2pd_interface *iface,
				 struct owfd_p2pd_ep *ep)
{
	int r;

	if (ep->ev->data.ptr != &iface->wpa_fd)
		return OWFD_P2PD_EP_NOT_HANDLED;

	r = owfd_wpa_ctrl_dispatch(iface->wpa, 0);
	if (r < 0)
		return r;

	return OWFD_P2PD_EP_HANDLED;;
}

int owfd_p2pd_interface_dispatch_chld(struct owfd_p2pd_interface *iface,
				      struct signalfd_siginfo *info)
{
	if (iface->pid <= 0 || info->ssi_pid != iface->pid)
		return OWFD_P2PD_EP_NOT_HANDLED;

	log_info("wpa_supplicant exited");

	owfd_wpa_ctrl_close(iface->wpa);
	iface->pid = 0;

	return OWFD_P2PD_EP_QUIT;
}

static void wpa_event(struct owfd_wpa_ctrl *wpa, void *buf,
		      size_t len, void *data)
{
}
