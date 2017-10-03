#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xwallpaper], [0.2.2], [tobias@stoeckmann.org])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC

# Initialize Automake
AM_INIT_AUTOMAKE([foreign])

# Check for pkg-config packages
PKG_CHECK_MODULES(PIXMAN, [pixman-1 >= 0.32])
PKG_CHECK_MODULES(XCB, [xcb-image >= 0.3.8 xcb-util >= 0.3.8])

# Check for OpenBSD's pledge(2)
AC_CHECK_FUNCS([pledge])

# Check if debug messages are requested
AC_MSG_CHECKING(whether debug messages are requested)
AC_ARG_WITH([debug],
  [AS_HELP_STRING([--with-debug], [enable debug messages])],
  [
   if test "$withval" = no ; then
     debug_messages=no
   else
     debug_messages=yes
   fi
  ],
  [ debug_messages=no ]
)
AC_MSG_RESULT($debug_messages)
AS_IF([test "$debug_messages" = yes],
  [AC_DEFINE(DEBUG,[],[Define to 1 if you want debug messages.])],[]
)

# Check if RandR support is requested
AC_MSG_CHECKING(whether RandR support is requested)
AC_ARG_WITH([randr],
  [AS_HELP_STRING([--without-randr], [disable RandR support])],
  [
   if test "$withval" = no ; then
     randr_support=no
   else
     randr_support=yes
   fi
  ],
  [ randr_support=auto ]
)
AC_MSG_RESULT($randr_support)
if test "$randr_support" != no ; then
  PKG_CHECK_MODULES(RANDR, xcb-randr >= 1.11, [randr_ok="yes"], [randr_ok="no"])
else
  randr_ok="no"
fi
AS_IF([test "$randr_ok" = yes],
  [AC_DEFINE(WITH_RANDR,[],[Define to 1 if you want RandR support.])],[]
)
AM_CONDITIONAL(BUILD_RANDR, [test "$randr_ok" = yes])

# Check if JPEG support is requested
AC_MSG_CHECKING(whether JPEG support is requested)
AC_ARG_WITH([jpeg],
  [AS_HELP_STRING([--without-jpeg], [disable JPEG support])],
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
  jpeg_ok="no"
fi
AS_IF([test "$jpeg_ok" = yes],
  [AC_DEFINE(WITH_JPEG,[],[Define to 1 if you want JPEG support.])],[]
)
AM_CONDITIONAL(BUILD_JPEG, [test "$jpeg_ok" = yes])

# Check if PNG support is requested
AC_MSG_CHECKING(whether PNG support is requested)
AC_ARG_WITH([png],
  [AS_HELP_STRING([--without-png], [disable PNG support])],
  [
   if test "$withval" = no ; then
     png_support=no
   else
     png_support=yes
   fi
  ],
  [ png_support=auto ]
)
AC_MSG_RESULT($png_support)
if test "$png_support" != no ; then
  PKG_CHECK_MODULES(PNG, libpng >= 1.2, [png_ok="yes"], [png_ok="no"])
else
  png_ok="no"
fi
AS_IF([test "$png_ok" = yes],
  [AC_DEFINE(WITH_PNG,[],[Define to 1 if you want PNG support.])],[]
)
AM_CONDITIONAL(BUILD_PNG, [test "$png_ok" = yes])

# Check if XPM support is requested
AC_MSG_CHECKING(whether XPM support is requested)
AC_ARG_WITH([xpm],
  [AS_HELP_STRING([--without-xpm], [disable XPM support])],
  [
   if test "$withval" = no ; then
     xpm_support=no
   else
     xpm_support=yes
   fi
  ],
  [ xpm_support=auto ]
)
AC_MSG_RESULT($xpm_support)
if test "$xpm_support" != no ; then
  PKG_CHECK_MODULES(XPM, xpm >= 3.5, [xpm_ok="yes"], [xpm_ok="no"])
else
  xpm_ok="no"
fi
AS_IF([test "$xpm_ok" = yes],
  [AC_DEFINE(WITH_XPM,[],[Define to 1 if you want XPM support.])],[]
)
AM_CONDITIONAL(BUILD_XPM, [test "$xpm_ok" = yes])

AC_ARG_WITH([zshcompletiondir],
 AS_HELP_STRING([--with-zshcompletiondir=DIR], [Zsh completions directory]),
 [], [with_zshcompletiondir=${datadir}/zsh/vendor-completions])
AC_SUBST([zshcompletiondir], [$with_zshcompletiondir])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
