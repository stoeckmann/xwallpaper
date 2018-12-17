zshcompletiondir=@zshcompletiondir@

dist_man_MANS = xwallpaper.1

bin_PROGRAMS = xwallpaper

dist_zshcompletion_DATA = _xwallpaper

EXTRA_DIST = LICENSE README.md _xwallpaper

xwallpaper_SOURCES = functions.h debug.c main.c options.c outputs.c util.c
xwallpaper_CPPFLAGS = @PIXMAN_CFLAGS@ @XCB_CFLAGS@
xwallpaper_LDADD = @PIXMAN_LIBS@ @XCB_LIBS@

if BUILD_SECCOMP
xwallpaper_SOURCES += seccomp.c
xwallpaper_CPPFLAGS += @SECCOMP_CFLAGS@
xwallpaper_LDADD += @SECCOMP_LIBS@
else
EXTRA_DIST += seccomp.c
endif

if BUILD_RANDR
xwallpaper_CPPFLAGS += @RANDR_CFLAGS@
xwallpaper_LDADD += @RANDR_LIBS@
endif

if BUILD_JPEG
xwallpaper_SOURCES += load_jpeg.c
xwallpaper_CPPFLAGS += @JPEG_CFLAGS@
xwallpaper_LDADD += @JPEG_LIBS@
else
EXTRA_DIST += load_jpeg.c
endif

if BUILD_PNG
xwallpaper_SOURCES += load_png.c
xwallpaper_CPPFLAGS += @PNG_CFLAGS@
xwallpaper_LDADD += @PNG_LIBS@
else
EXTRA_DIST += load_png.c
endif

if BUILD_XPM
xwallpaper_SOURCES += load_xpm.c
xwallpaper_CPPFLAGS += @XPM_CFLAGS@
xwallpaper_LDADD += @XPM_LIBS@
else
EXTRA_DIST += load_xpm.c
endif

AM_CFLAGS = $(CWARNFLAGS)
