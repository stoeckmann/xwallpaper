dist_man_MANS = xsetwallpaper.1

bin_PROGRAMS = xsetwallpaper

EXTRA_DIST = LICENSE README.md

xsetwallpaper_SOURCES = functions.h load_png.c main.c options.c outputs.c util.c
xsetwallpaper_CPPFLAGS = @PIXMAN_CFLAGS@ @PNG_CFLAGS@ @XCB_CFLAGS@ 
xsetwallpaper_LDADD = @PIXMAN_LIBS@ @PNG_LIBS@ @XCB_LIBS@

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

AM_CFLAGS = $(CWARNFLAGS)
