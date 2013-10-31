#
# OpenWFD - Open-Source Wifi-Display Implementation
# Copyright (c) 2013 David Herrmann <dh.herrmann@gmail.com>
#

#
# Global Configurations and Initializations
#

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory
AUTOMAKE_OPTIONS = color-tests
.DELETE_ON_ERROR:

include_HEADERS =
EXTRA_DIST = \
	README \
	COPYING \
	NEWS
CLEANFILES =
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA =
TPHONY =

TESTS =
bin_PROGRAMS =
check_PROGRAMS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =

#
# Default CFlags
# Make all files include "config.h" by default. This shouldn't cause any
# problems and we cannot forget to include it anymore.
#
# Also make the linker discard all unused symbols.
#

AM_CFLAGS = \
	-Wall \
	-pipe \
	-fno-common \
	-ffast-math \
	-fdiagnostics-show-option \
	-fno-strict-aliasing \
	-fvisibility=hidden \
	-ffunction-sections \
	-fdata-sections
AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I $(srcdir)/src
AM_LDFLAGS = \
	-Wl,--as-needed \
	-Wl,--gc-sections \
	-Wl,-z,relro \
	-Wl,-z,now

#
# Additional parameters
# TODO: these should be configurable in configure.ac
#

AM_CPPFLAGS += \
	-DBUILD_ENABLE_DEBUG \
	"-DBUILD_BINDIR_WPA_SUPPLICANT=\"/bin\""

#
# SHL - Static Helper Library
# The SHL subsystem contains several small code pieces used all over the place.
#

noinst_LTLIBRARIES += libshl.la

libshl_la_SOURCES = \
	src/shl_llog.h \
	src/shl_log.h \
	src/shl_log.c \
	src/shl_ring.h \
	src/shl_ring.c
libshl_la_CPPFLAGS = $(AM_CPPFLAGS)
libshl_la_LDFLAGS = $(AM_LDFLAGS)
libshl_la_LIBADD = $(AM_LIBADD)

#
# owfd - shared helpers
#

noinst_LTLIBRARIES += libowfd.la

libowfd_la_SOURCES = \
	src/rtsp.h \
	src/rtsp_ctrl.c \
	src/rtsp_decoder.c \
	src/shared.h \
	src/shared.c \
	src/wpa_ctrl.h \
	src/wpa_ctrl.c
libowfd_la_CPPFLAGS = $(AM_CPPFLAGS)
libowfd_la_LDFLAGS = $(AM_LDFLAGS)
libowfd_la_LIBADD = $(AM_LIBADD)

#
# openwfd_p2pd
#

bin_PROGRAMS += openwfd_p2pd

openwfd_p2pd_SOURCES = \
	src/p2pd.h \
	src/p2pd.c \
	src/p2pd_config.c \
	src/p2pd_interface.c

openwfd_p2pd_CPPFLAGS = $(AM_CPPFLAGS)
openwfd_p2pd_LDADD = \
	libowfd.la \
	libshl.la
openwfd_p2pd_LDFLAGS = $(AM_LDFLAGS)

#
# Tools
#

check_PROGRAMS += \
	openwfd_ie

openwfd_ie_SOURCES = tools/openwfd_ie.c
openwfd_ie_CPPFLAGS = $(AM_CPPFLAGS)
openwfd_ie_LDADD =
openwfd_ie_LDFLAGS = $(AM_LDFLAGS)

#
# Tests
#

if BUILD_HAVE_CHECK
check_PROGRAMS += \
	test_rtsp
TESTS += \
	test_rtsp
endif

test_sources = \
	test/test_common.h
test_libs = \
	libowfd.la \
	libshl.la \
	$(CHECK_LIBS)
test_cflags = \
	$(AM_CPPFLAGS) \
	$(CHECK_CFLAGS)
test_lflags = \
	$(AM_LDFLAGS)

test_rtsp_SOURCES = test/test_rtsp.c $(test_sources)
test_rtsp_CPPFLAGS = $(test_cflags)
test_rtsp_LDADD = $(test_libs)
test_rtsp_LDFLAGS = $(test_lflags)

#
# Phony targets
#

.PHONY: $(TPHONY)

#
# Empty .SECONDARY target causes alle intermediate files to be treated as
# secondary files. That is, they don't get deleted after make finished.
#

.SECONDARY:
