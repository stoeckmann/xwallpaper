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
#include <xcb/xcb_image.h>

#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"

#define MODE_CENTER	1
#define MODE_SCALE	2
#define MODE_TILE	3
#define MODE_ZOOM	4
#define MODE_ZOOM_CROP	5

#ifndef HAVE_PLEDGE
int
pledge(const char *promises, const char *paths[])
{
	return 0;
}
#endif

static void
usage(void)
{
	fprintf(stderr, "usage: xsetwallpaper [-cstvzZ] [-S screen] file\n");
	exit(1);
}

static void
copy_pixels(xcb_image_t *img, uint32_t *pixels, uint16_t width, uint16_t height)
{
	size_t x, y;
	uint32_t *p;

	p = pixels;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			xcb_image_put_pixel(img, x, y, *p++);
}

static xcb_format_t *
get_format(xcb_connection_t *c, uint8_t depth, uint8_t bpp)
{
	const xcb_setup_t *setup;
	xcb_format_t *formats;
	int i, length;

	setup = xcb_get_setup(c);
	formats = xcb_setup_pixmap_formats(setup);
	length = xcb_setup_pixmap_formats_length(setup);

	for (i = 0; i < length; i++) 
		if (formats[i].depth == depth &&
		    formats[i].bits_per_pixel == bpp)
			return &formats[i];

	errx(1, "failed to get matching format");
}

static xcb_image_t *
create_xcb_image(xcb_connection_t *c, uint16_t width, uint16_t height,
    uint8_t depth)
{
	uint8_t *data;
	const xcb_setup_t *setup;
	xcb_format_t *format;

	setup = xcb_get_setup(c);
	format = get_format(c, depth, 32);
	data = xmalloc(width, height, sizeof(uint32_t));

	return xcb_image_create(width, height, XCB_IMAGE_FORMAT_Z_PIXMAP,
	    format->scanline_pad, format->depth, format->bits_per_pixel, 0,
	    setup->image_byte_order, XCB_IMAGE_ORDER_LSB_FIRST,
	    data, width * height * sizeof(uint32_t), data);
}

static void
transform(xcb_image_t *xcb_image, pixman_image_t *pixman_image, int mode)
{
	pixman_f_transform_t ftransform;
	pixman_transform_t transform;
	int pixman_width, pixman_height;
	uint16_t xcb_width, xcb_height;
	float w_scale, h_scale, scale;
	float translate_x, translate_y;

	pixman_width = pixman_image_get_width(pixman_image);
	pixman_height = pixman_image_get_height(pixman_image);
	xcb_width = xcb_image->width;
	xcb_height = xcb_image->height;

	w_scale = (float)pixman_width / xcb_width;
	h_scale = (float)pixman_height / xcb_height;

	switch (mode) {
		case MODE_CENTER:
			w_scale = 1;
			h_scale = 1;
			break;
		case MODE_ZOOM:
			scale = w_scale < h_scale ? h_scale : w_scale;
			w_scale = scale;
			h_scale = scale;
			break;
		case MODE_ZOOM_CROP:
			scale = w_scale > h_scale ? h_scale : w_scale;
			w_scale = scale;
			h_scale = scale;
		default:
			break;
	}

	translate_x = (pixman_width / w_scale - xcb_width) / 2;
	translate_y = (pixman_height / h_scale - xcb_height) / 2;

	pixman_f_transform_init_identity(&ftransform);
	pixman_f_transform_translate(&ftransform, NULL,
	    translate_x, translate_y);
	pixman_f_transform_scale(&ftransform, NULL, w_scale, h_scale);

	pixman_transform_from_pixman_f_transform(&transform, &ftransform);
	pixman_image_set_transform(pixman_image, &transform);
}

static void
insert_pixman(xcb_image_t *xcb_image, pixman_image_t *pixman_image, int mode)
{
	uint16_t width, height;
	uint32_t *pixels;

	width = xcb_image->width;
	height = xcb_image->height;

	if (mode == MODE_TILE) {
		pixels = pixman_image_get_data(pixman_image);
		copy_pixels(xcb_image, pixels, width, height);
	} else {
		pixman_image_t *tmp;

		pixels = xmalloc(width, height, sizeof(*pixels));

		tmp = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height,
		    pixels, width * sizeof(*pixels));
		if (tmp == NULL)
			errx(1, "failed to create temporary pixman image");
		transform(xcb_image, pixman_image, mode);
		pixman_image_composite(PIXMAN_OP_SRC, pixman_image, NULL, tmp,
		    0, 0, 0, 0, 0, 0, width, height);

		copy_pixels(xcb_image, pixels, width, height);

		free(pixels);
		pixman_image_unref(tmp);
	}
}

static pixman_image_t *
load_pixman_image(FILE *fp)
{
	pixman_image_t *pixman_image;

	pixman_image = load_png(fp);
#ifdef WITH_JPEG
	if (pixman_image == NULL)
		pixman_image = load_jpeg(fp);
#endif /* WITH_JPEG */
	if (pixman_image == NULL)
		errx(1, "failed to parse input file");

	return pixman_image;
}

static int
parse_screen_number(char *s)
{
	char *endptr;
	long int value;

	value = strtol(s, &endptr, 10);
	if (endptr == s || *endptr != '\0' || value < 0 || value > INT_MAX)
		errx(1, "failed to parse screen number: %s", optarg);
	return value;
}

static void
set_atom(char *name, xcb_pixmap_t pixmap, xcb_connection_t *c,
    xcb_screen_t *screen)
{
	xcb_intern_atom_cookie_t atom_cookie;
	xcb_intern_atom_reply_t *atom_reply;

	atom_cookie = xcb_intern_atom_unchecked(c, 0, strlen(name), name);
	atom_reply = xcb_intern_atom_reply(c, atom_cookie, NULL);
	if (atom_reply != NULL) {
		xcb_get_property_cookie_t property_cookie;
		xcb_get_property_reply_t *property_reply;

		property_cookie = xcb_get_property_unchecked(c, 0, screen->root,
		    atom_reply->atom, XCB_ATOM_PIXMAP, 0, 1);
		xcb_request_check(c, xcb_change_property(c,
		    XCB_PROP_MODE_REPLACE, screen->root, atom_reply->atom,
		    XCB_ATOM_PIXMAP, 32, 1, &pixmap));
		property_reply = xcb_get_property_reply(c, property_cookie,
		    NULL);
		if (property_reply && property_reply->value_len) {
			xcb_pixmap_t *old;

			old = xcb_get_property_value(property_reply);
			if (old != NULL)
				xcb_request_check(c, xcb_kill_client(c, *old));
		}
		free(property_reply);
		free(atom_reply);
	}
}

static void
set_wallpaper(xcb_connection_t *c, xcb_screen_t *screen, xcb_image_t *xcb_image)
{
	uint32_t gc_values[2];
	uint32_t cw_values[1];
	xcb_pixmap_t pixmap;
	xcb_gcontext_t gc;

	pixmap = xcb_generate_id(c);
	xcb_create_pixmap(c, screen->root_depth, pixmap, screen->root,
	    xcb_image->width, xcb_image->height);

	gc_values[0] = screen->black_pixel;
	gc_values[1] = screen->white_pixel;
	gc = xcb_generate_id(c);
	xcb_create_gc(c, gc, pixmap, XCB_GC_FOREGROUND | XCB_GC_BACKGROUND,
	    gc_values);

	xcb_put_image(c, xcb_image->format, pixmap, gc, xcb_image->width,
	    xcb_image->height, 0, 0, 0, screen->root_depth, xcb_image->size,
	    xcb_image->data);

	cw_values[0] = pixmap;
	xcb_change_window_attributes(c, screen->root, XCB_CW_BACK_PIXMAP,
	    cw_values);

	xcb_clear_area(c, 0, screen->root, 0, 0, 0, 0);
	xcb_request_check(c,
	    xcb_set_close_down_mode(c, XCB_CLOSE_DOWN_RETAIN_PERMANENT));

	set_atom("ESETROOT_PMAP_ID", pixmap, c, screen);
	set_atom("_XROOTPMAP_ID", pixmap, c, screen);
}

static void
validate_dimensions(pixman_image_t *pixman_image)
{
	int width, height;

	width = pixman_image_get_width(pixman_image);
	height = pixman_image_get_height(pixman_image);
	if (width < 1 || width > UINT16_MAX ||
	    height < 1 || height > UINT16_MAX)
		errx(1, "image is too large");
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	pixman_image_t *pixman_image;
	int ch, i, mode, screen_nbr;
	xcb_connection_t *c;
	const xcb_setup_t *setup;
	xcb_screen_iterator_t it;

	mode = MODE_ZOOM_CROP;
	screen_nbr = -1;
	while ((ch = getopt(argc, argv, "csS:tvzZ")) != -1)
		switch (ch) {
		case 'c':
			mode = MODE_CENTER;
			break;
		case 's':
			mode = MODE_SCALE;
			break;
		case 'S':
			screen_nbr = parse_screen_number(optarg);
			break;
		case 't':
			mode = MODE_TILE;
			break;
		case 'v':
			puts(VERSION);
			return 0;
		case 'z':
			mode = MODE_ZOOM;
			break;
		case 'Z':
			mode = MODE_ZOOM_CROP;
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc != 1)
		usage();

	/* open before pledging that no further files are opened */
	if ((fp = fopen(argv[0], "r")) == NULL)
		err(1, "failed to open %s", argv[0]);

	pledge("dns inet rpath stdio unix", NULL);

	c = xcb_connect(NULL, NULL);

	pledge("stdio", NULL);

	pixman_image = load_pixman_image(fp);

	/* input file handle is not needed any longer */
	fclose(fp);

	validate_dimensions(pixman_image);

	setup = xcb_get_setup(c);
	it = xcb_setup_roots_iterator(setup);

	for (i = 0; it.rem; i++, xcb_screen_next(&it))
		if (screen_nbr == -1 || screen_nbr == i) {
			uint16_t width, height;
			uint8_t depth;
			xcb_image_t *xcb_image;
			xcb_screen_t *screen;

			screen = it.data;
			depth = screen->root_depth;
			if (mode == MODE_TILE) {
				width = pixman_image_get_width(pixman_image);
				height = pixman_image_get_height(pixman_image);
			} else {
				width = screen->width_in_pixels;
				height = screen->height_in_pixels;
			}

			xcb_image = create_xcb_image(c, width, height, depth);
			insert_pixman(xcb_image, pixman_image, mode);
			set_wallpaper(c, screen, xcb_image);
			xcb_image_destroy(xcb_image);
		}

	xcb_flush(c);
	xcb_disconnect(c);

	return 0;
}

