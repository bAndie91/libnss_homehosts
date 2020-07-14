AC_PREREQ([2.69])
AC_INIT([libnss_homehosts], [2.0], [https://github.com/bAndie91/libnss_homehosts/issues], [libnss-homehosts], [https://github.com/bAndie91/libnss_homehosts])

AC_CONFIG_SRCDIR([libnss_homehosts.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

AM_INIT_AUTOMAKE([foreign])
LT_PREREQ([2.4.6])
LT_INIT

LT_LDFLAGS="-version-info 2 -no-undefined"

# Checks for header files.
AC_CHECK_HEADER([arpa/inet.h], [], [AC_MSG_ERROR([Arpa inet header not found])])
AC_CHECK_HEADER([netdb.h], [], [AC_MSG_ERROR([Netdb header not found])])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset strcasecmp strchr])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [enable debug output in homehosts])],
	[debug_enabled=$enableval],
	[debug_enabled=no])
if test "x$debug_enabled" != xno; then
	AC_DEFINE([DEBUG], [1], [Define to 1 to enable debug mode of homehosts])
fi

AC_SUBST(LT_LDFLAGS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
