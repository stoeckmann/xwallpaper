#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xsetwallpaper], [0.1.0], [tobias@stoeckmann.org])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Initialize Automake
AM_INIT_AUTOMAKE([foreign])

# Check for pkg-config packages
PKG_CHECK_MODULES(PIXMAN, [pixman-1 >= 0.34])
PKG_CHECK_MODULES(PNG, [libpng >= 1.6])
PKG_CHECK_MODULES(XCB, [xcb-util >= 0.4 xcb-image >= 0.4])

# Check for OpenBSD's pledge(2)
AC_CHECK_FUNCS([pledge])

# Check if JPEG support is requested
AC_MSG_CHECKING(for JPEG support)
AC_ARG_WITH([jpeg],
  [AS_HELP_STRING([--without-jpeg], [Disable JPEG support])],
  [
   if test "$withval" = no ; then
     jpeg_support=no
   else
     jpeg_support=yes
   fi
  ],
  [ jpeg_support=auto ]
)
AC_MSG_RESULT($jpeg_support)
if test "$jpeg_support" != no ; then
  PKG_CHECK_MODULES(JPEG, libjpeg >= 1.5, [jpeg_ok="yes"], [jpeg_ok="no"])
else
  jpeg_ok="ok"
fi
AS_IF([test "$jpeg_ok" = yes],
  [AC_DEFINE(WITH_JPEG,[],[With JPEG support])],[]
)
AM_CONDITIONAL(BUILD_JPEG, [test "$jpeg_ok" = yes])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
