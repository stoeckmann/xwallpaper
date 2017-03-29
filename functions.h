/*
 * Copyright (c) 2017 Tobias Stoeckmann <tobias@stoeckmann.org>
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

#include <pixman.h>
#include <stdio.h>
#include <unistd.h>

#define MODE_CENTER	1
#define MODE_MAXIMIZE	2
#define MODE_STRETCH	3
#define MODE_TILE	4
#define MODE_ZOOM	5

struct wp_buffer {
	FILE		*fp;
	pixman_image_t	*pixman_image;
	dev_t		 st_dev;
	ino_t		 st_ino;
};
typedef struct wp_buffer wp_buffer_t;

struct wp_option {
	wp_buffer_t	*buffer;
	char		*filename;
	int		 mode;
	char		*output;
	int		 screen;
};
typedef struct wp_option wp_option_t;

struct wp_output {
	char *name;
	int16_t x, y;
	uint16_t width, height;
};
typedef struct wp_output wp_output_t;

void		 free_outputs(wp_output_t *);
wp_output_t	*get_output(wp_output_t *, char *);
wp_output_t	*get_outputs(xcb_connection_t *, xcb_screen_t *);
void		 init_outputs(xcb_connection_t *);
pixman_image_t	*load_png(FILE *);
pixman_image_t	*load_jpeg(FILE *);
wp_option_t	*parse_options(char **);
size_t		 safe_mul(size_t, size_t);
size_t		 safe_mul3(size_t, size_t, size_t);
void		*xmalloc(size_t);
