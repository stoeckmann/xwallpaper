dist_man_MANS = xsetwallpaper.1

bin_PROGRAMS = xsetwallpaper

EXTRA_DIST = LICENSE README.md

xsetwallpaper_SOURCES = functions.h main.c options.c outputs.c util.c
xsetwallpaper_CPPFLAGS = @PIXMAN_CFLAGS@ @XCB_CFLAGS@
xsetwallpaper_LDADD = @PIXMAN_LIBS@ @XCB_LIBS@

if BUILD_RANDR
xsetwallpaper_CPPFLAGS += @RANDR_CFLAGS@
xsetwallpaper_LDADD += @RANDR_LIBS@
endif

if BUILD_JPEG
xsetwallpaper_SOURCES += load_jpeg.c
xsetwallpaper_CPPFLAGS += @JPEG_CFLAGS@
xsetwallpaper_LDADD += @JPEG_LIBS@
else
EXTRA_DIST += load_jpeg.c
endif

if BUILD_PNG
xsetwallpaper_SOURCES += load_png.c
xsetwallpaper_CPPFLAGS += @PNG_CFLAGS@
xsetwallpaper_LDADD += @PNG_LIBS@
else
EXTRA_DIST += load_png.c
endif

if BUILD_XPM
xsetwallpaper_SOURCES += load_xpm.c
xsetwallpaper_CPPFLAGS += @XPM_CFLAGS@
xsetwallpaper_LDADD += @XPM_LIBS@
else
EXTRA_DIST += load_xpm.c
endif

AM_CFLAGS = $(CWARNFLAGS)
