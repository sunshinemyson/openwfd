/*
 * SHL - Log/Debug Interface
 *
 * Copyright (c) 2010-2013 David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "shl_log.h"

/*
 * Locking
 * Dummies to implement locking. If we ever want lock-protected logging, these
 * need to be provided by the user.
 */

static inline void log_lock()
{
}

static inline void log_unlock()
{
}

/*
 * Time Management
 * We print seconds and microseconds since application start for each
 * log-message.
 */

static struct timeval log__ftime;

static void log__time(long long *sec, long long *usec)
{
	struct timeval t;

	/* The first log message sets the start-time. As this is unlocked, the
	 * worst thing that can happen is that multiple messages set it. We
	 * don't care for that and just accept short time-diffs in that case.
	 * What we have to check is for negative time-diffs, though. Simply
	 * revert them. */

	if (log__ftime.tv_sec == 0 && log__ftime.tv_usec == 0) {
		gettimeofday(&log__ftime, NULL);
		*sec = 0;
		*usec = 0;
	} else {
		gettimeofday(&t, NULL);
		*sec = t.tv_sec - log__ftime.tv_sec;
		*usec = (long long)t.tv_usec - (long long)log__ftime.tv_usec;
		if (*usec < 0) {
			*sec -= 1;
			if (*sec < 0)
				*sec = 0;
			*usec = 1000000 + *usec;
		}
	}
}

/*
 * Default Values
 * Several logging-parameters may be omitted by applications. To provide sane
 * default values we provide constants here.
 *
 * LOG_SUBSYSTEM: By default no subsystem is specified
 */

const char *LOG_SUBSYSTEM = NULL;

/*
 * Max Severity
 * Messages with severities between log_max_sev and LOG_SEV_NUM (exclusive)
 * are not logged, but discarded.
 */

unsigned int log_max_sev = LOG_NOTICE;

/*
 * Forward declaration so we can use the locked-versions in other functions
 * here. Be careful to avoid deadlocks, though.
 * Also set default log-subsystem to "log" for all logging inside this API.
 */

static void log__submit(const char *file,
			int line,
			const char *func,
			const char *subs,
			unsigned int sev,
			const char *format,
			va_list args);

#define LOG_SUBSYSTEM "log"

/*
 * Basic logger
 * The log__submit function writes the message into the current log-target. It
 * must be called with log__mutex locked.
 * By default the current time elapsed since the first message was logged is
 * prepended to the message. file, line and func information are appended to the
 * message if sev == LOG_DEBUG.
 * The subsystem, if not NULL, is prepended as "SUBS: " to the message and a
 * newline is always appended by default. Multiline-messages are not allowed.
 */

static const char *log__sev2str[LOG_SEV_NUM] = {
	[LOG_DEBUG] = "DEBUG",
	[LOG_INFO] = "INFO",
	[LOG_NOTICE] = "NOTICE",
	[LOG_WARNING] = "WARNING",
	[LOG_ERROR] = "ERROR",
	[LOG_CRITICAL] = "CRITICAL",
	[LOG_ALERT] = "ALERT",
	[LOG_FATAL] = "FATAL",
};

static void log__submit(const char *file,
			int line,
			const char *func,
			const char *subs,
			unsigned int sev,
			const char *format,
			va_list args)
{
	int saved_errno = errno;
	const char *prefix = NULL;
	FILE *out;
	long long sec, usec;

	out = stderr;
	log__time(&sec, &usec);

	if (sev < LOG_SEV_NUM && sev > log_max_sev)
		return;

	if (sev < LOG_SEV_NUM)
		prefix = log__sev2str[sev];

	if (prefix) {
		if (subs)
			fprintf(out, "[%.4lld.%.6lld] %s: %s: ",
				sec, usec, prefix, subs);
		else
			fprintf(out, "[%.4lld.%.6lld] %s: ",
				sec, usec, prefix);
	} else {
		if (subs)
			fprintf(out, "[%.4lld.%.6lld] %s: ", sec, usec, subs);
		else
			fprintf(out, "[%.4lld.%.6lld] ", sec, usec);
	}

	errno = saved_errno;
	vfprintf(out, format, args);

	if (sev == LOG_DEBUG) {
		if (!func)
			func = "<unknown>";
		if (!file)
			file = "<unknown>";
		if (line < 0)
			line = 0;
		fprintf(out, " (%s() in %s:%d)\n", func, file, line);
	} else {
		fprintf(out, "\n");
	}
}

void log_submit(const char *file,
		int line,
		const char *func,
		const char *subs,
		unsigned int sev,
		const char *format,
		va_list args)
{
	int saved_errno = errno;

	log_lock();
	errno = saved_errno;
	log__submit(file, line, func, subs, sev, format, args);
	log_unlock();

	errno = saved_errno;
}

void log_format(const char *file,
		int line,
		const char *func,
		const char *subs,
		unsigned int sev,
		const char *format,
		...)
{
	int saved_errno = errno;
	va_list list;

	va_start(list, format);
	log_lock();
	errno = saved_errno;
	log__submit(file, line, func, subs, sev, format, list);
	log_unlock();
	va_end(list);

	errno = saved_errno;
}

void log_llog(void *data,
	      const char *file,
	      int line,
	      const char *func,
	      const char *subs,
	      unsigned int sev,
	      const char *format,
	      va_list args)
{
	log_submit(file, line, func, subs, sev, format, args);
}
