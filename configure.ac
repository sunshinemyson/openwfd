#
# OpenWFD - Open-Source Wifi-Display Implementation
# Copyright (c) 2013 David Herrmann <dh.herrmann@gmail.com>
#

AC_PREREQ(2.68)

AC_INIT([openwfd],
        [1],
        [http://bugs.freedesktop.org/enter_bug.cgi?product=openwfd],
        [openwfd],
        [http://www.freedesktop.org/wiki/Software/openwfd])
AC_CONFIG_SRCDIR([src/openwfd/wfd.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER(config.h)
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects dist-xz no-dist-gzip tar-pax -Wall -Werror -Wno-portability])
AM_SILENT_RULES([yes])

AC_SUBST(PACKAGE_DESCRIPTION, ["Open-Source Wifi-Display Implementation"])

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_SED
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_GREP
AC_PROG_AWK

LT_PREREQ(2.2)
LT_INIT

#
# Test for gdhcp dependencies
#

PKG_CHECK_MODULES([GDHCP], [glib-2.0])
AC_SUBST(GDHCP_CFLAGS)
AC_SUBST(GDHCP_LIBS)

#
# Test for dhcp dependencies
#

PKG_CHECK_MODULES([DHCP], [glib-2.0])
AC_SUBST(DHCP_CFLAGS)
AC_SUBST(DHCP_LIBS)

#
# Test for "check" which we use for our test-suite. If not found, we disable
# all tests.
#

PKG_CHECK_MODULES([CHECK], [check],
                  [have_check=yes], [have_check=no])
AC_SUBST(CHECK_CFLAGS)
AC_SUBST(CHECK_LIBS)
AM_CONDITIONAL([BUILD_HAVE_CHECK], [test "x$have_check" = "xyes"])

#
# Makefile vars
# After everything is configured, we create all makefiles.
#

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

#
# Configuration output
# Show configuration to the user so they can check whether everything was
# configured as expected.
#

AC_MSG_NOTICE([Build configuration:

               prefix: $prefix
          exec-prefix: $exec_prefix
               libdir: $libdir
           includedir: $includedir

  Miscellaneous Options:
       building tests: $have_check

        Run "${MAKE-make}" to start compilation process])
