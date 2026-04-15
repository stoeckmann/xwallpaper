/*
 * Copyright (c) 2025 Tobias Stoeckmann <tobias@stoeckmann.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <xcb/xcb.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <pixman.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"

#define MODE_CENTER	1
#define MODE_FOCUS	2
#define MODE_MAXIMIZE	3
#define MODE_STRETCH	4
#define MODE_TILE	5
#define MODE_ZOOM	6

#define SOURCE_ATOMS	1

#define TARGET_ATOMS	1
#define TARGET_ROOT	2

#define OPTIONS(o)							\
o(help,     void,     "prints help and exits")				\
o(center,   file,     "centers the input file on the output")		\
o(clear,    void,     "initializes screen with a black background")	\
o(daemon,   void,     "keeps xwallpaper running in background")		\
o(debug,    void,     "displays debug messages on standard error")	\
o(desktop,  number,   "desktop by its _NET_CURRENT_DESKTOP number")	\
o(focus,    file,     "trim box guaranteed to be visible on output")	\
o(maximize, file,     "maximizes to fit without cropping")		\
o(no_atoms, void,     "atoms for pseudo transparency are not updated")	\
o(no_randr, void,     "uses the whole output inputs")			\
o(no_root,  void,     "background of the root window is not updated")	\
o(output,   output,   "output for subsequent wallpaper files")		\
o(screen,   number,   "screen by its screen number")			\
o(stretch,  file,     "stretches file to fully cover output")		\
o(tile,     file,     "tiles file repeatedly")				\
o(trim,     geometry, "area of interest in source file")		\
o(version,  void,     "prints version and exits")			\
o(zoom,     file,     "zooms file to fit output with cropping")

enum {
	OPTION_END = 0,
	OPTION_OK = 0,
	OPTION_INVALID,
	OPTION_MISSING,
	OPTION_NOTNUMBER,
#define OPTION_LABEL_ENUM(name_, arg_, desc_) OPTION_##name_,
OPTIONS(OPTION_LABEL_ENUM)
};

#define SAFE_MUL(res, x, y) do {					 \
	if ((y) != 0 && SIZE_MAX / (y) < (x))				 \
		errx(1, "memory allocation would exceed system limits"); \
	res = (x) * (y);						 \
} while (0)

#define SAFE_MUL3(res, x, y, z) do {	\
	SAFE_MUL(res, (x), (z));	\
	SAFE_MUL(res, res, (y));	\
} while (0)

typedef struct wp_box {
	uint16_t	width;
	uint16_t	height;
	uint16_t	x_off;
	uint16_t	y_off;
} wp_box_t;

typedef struct wp_argcv {
	int		  c;
	char		**v;
} wp_argcv_t;

typedef struct wp_config {
	wp_argcv_t	argcv;
} wp_config_t;

typedef struct wp_output {
	char *name;
	int16_t x, y;
	uint16_t width, height;
} wp_output_t;

typedef struct wp_match {
	int mode;
	const char *modename;
	const char *filename;

	const char *trim;

	int screen;
	int desktop;
	const char *output;
} wp_match_t;

extern int	 has_randr;
extern int	 show_debug;

void		 debug(const char *, ...);
wp_output_t	*get_outputs(xcb_connection_t *, xcb_screen_t *);
pixman_image_t	*load_jpeg(FILE *);
pixman_image_t	*load_png(FILE *);
pixman_image_t	*load_xpm(xcb_connection_t *, xcb_screen_t *, FILE *);
int		 parse_box(const char *s, wp_box_t *box);
wp_match_t	 option_match_wallpaper(wp_argcv_t argcv,
			int screen, int desktop, const char *output);
wp_config_t	 parse_config(int argc, char **argv);
void		 stage1_sandbox(void);
void		 stage2_sandbox(void);
void		*xmalloc(size_t);

int source_option(wp_argcv_t argcv);
int target_option(wp_argcv_t argcv);
int has_daemon_option(wp_argcv_t argcv);
int has_desktop_option(wp_argcv_t argcv);
