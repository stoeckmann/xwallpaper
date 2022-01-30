/*
 * Copyright (c) 2022 Tobias Stoeckmann <tobias@stoeckmann.org>
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

typedef struct wp_buffer {
	FILE		*fp;
	pixman_image_t	*pixman_image;
	dev_t		 st_dev;
	ino_t		 st_ino;
} wp_buffer_t;

typedef struct wp_option {
	wp_buffer_t	*buffer;
	char		*filename;
	int		 mode;
	char		*output;
	int		 screen;
	wp_box_t	*trim;
} wp_option_t;

typedef struct wp_config {
	wp_option_t	*options;
	size_t		 count;
	int		 daemon;
	int		 source;
	int		 target;
} wp_config_t;

typedef struct wp_output {
	char *name;
	int16_t x, y;
	uint16_t width, height;
} wp_output_t;

extern int	 has_randr;
extern int	 show_debug;

void		 debug(const char *, ...);
void		 free_outputs(wp_output_t *);
wp_output_t	*get_output(wp_output_t *, char *);
wp_output_t	*get_outputs(xcb_connection_t *, xcb_screen_t *);
pixman_image_t	*load_jpeg(FILE *);
pixman_image_t	*load_png(FILE *);
pixman_image_t	*load_xpm(xcb_connection_t *, xcb_screen_t *, FILE *);
wp_config_t	*parse_config(char **);
void		 stage1_sandbox(void);
void		 stage2_sandbox(void);
void		*xmalloc(size_t);
