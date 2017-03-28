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
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"

static void
usage(void)
{
	fprintf(stderr,
"usage: xsetwallpaper [-screen <screen>] [--output <output>]\n"
"  [--center <file>] [--maximize <file>]  [--stretch <file>]\n"
"  [--tile <file>] [--zoom <file>] [--version]\n");
	exit(1);
}

static int
check_dimensions(pixman_image_t *pixman_image)
{
	int width, height;

	width = pixman_image_get_width(pixman_image);
	height = pixman_image_get_height(pixman_image);

	return (width < 1 || width > UINT16_MAX ||
	    height < 1 || height > UINT16_MAX);
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
tile(pixman_image_t *dest, wp_output_t *output, wp_option_t *option)
{
	pixman_image_t *pixman_image;
	int pixman_width, pixman_height;
	pixman_f_transform_t ftransform;
	pixman_transform_t transform;
	uint16_t off_x, off_y;

	pixman_image = option->buffer->pixman_image;
	pixman_width = pixman_image_get_width(pixman_image);
	pixman_height = pixman_image_get_height(pixman_image);

	/* reset transformation of possible previous transform call */
	pixman_f_transform_init_identity(&ftransform);
	pixman_transform_from_pixman_f_transform(&transform, &ftransform);
	pixman_image_set_transform(pixman_image, &transform);

	/*
	 * Manually performs tiling to support separate modes per
	 * screen with RandR. If possible, xsetwallpaper will let
	 * X do the tiling natively.
         */
	for (off_y = 0; off_y < output->height; off_y += pixman_height) {
		uint16_t h;

		if (off_y + pixman_height > output->height)
			h = output->height - off_y;
		else
			h = pixman_height;

		for (off_x = 0; off_x < output->width; off_x += pixman_width) {
			uint16_t w;

			if (off_x + pixman_width > output->width)
				w = output->width - off_x;
			else
				w = pixman_width;

			pixman_image_composite(PIXMAN_OP_CONJOINT_SRC,
			    pixman_image, NULL, dest, 0, 0, 0, 0,
			    output->x + off_x, output->y + off_y, w, h);
		}
	}
}

static void
transform(pixman_image_t *dest, wp_output_t *output, wp_option_t *option)
{
	pixman_image_t *pixman_image;
	pixman_f_transform_t ftransform;
	pixman_transform_t transform;
	int pixman_width, pixman_height;
	uint16_t xcb_width, xcb_height;
	float w_scale, h_scale, scale;
	float translate_x, translate_y;

	pixman_image = option->buffer->pixman_image;
	pixman_width = pixman_image_get_width(pixman_image);
	pixman_height = pixman_image_get_height(pixman_image);
	xcb_width = output->width;
	xcb_height = output->height;

	w_scale = (float)pixman_width / xcb_width;
	h_scale = (float)pixman_height / xcb_height;

	switch (option->mode) {
		case MODE_CENTER:
			w_scale = 1;
			h_scale = 1;
			break;
		case MODE_MAXIMIZE:
			scale = w_scale < h_scale ? h_scale : w_scale;
			w_scale = scale;
			h_scale = scale;
			break;
		case MODE_ZOOM:
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

	pixman_image_composite(PIXMAN_OP_CONJOINT_SRC, pixman_image, NULL, dest,
	    0, 0, 0, 0, output->x, output->y, output->width, output->height);
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

static void
load_pixman_images(wp_option_t *options)
{
	wp_option_t *option;

	for (option = options; option->filename != NULL; option++) {
		wp_buffer_t *buffer;

		buffer = option->buffer;
		if (buffer->pixman_image == NULL) {
			buffer->pixman_image = load_pixman_image(buffer->fp);
			fclose(buffer->fp);
			if (check_dimensions(buffer->pixman_image))
				errx(1, "%s has illegal dimensions",
				    option->filename);
		}
	}
}

static void
process_output(wp_output_t *output, pixman_image_t *tmp, wp_option_t *option)
{
	if (option->mode == MODE_TILE)
		tile(tmp, output, option);
	else
		transform(tmp, output, option);
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

static int
check_x_tiling(wp_option_t *options)
{
	wp_option_t *option;
	size_t n;

	for (n = 0, option = options; option->filename != NULL; option++, n++)
		if (option->mode != MODE_TILE)
			return 0;
	if (n > 1)
		return 0;

	return options->output == NULL;
}

static void
process_screen(xcb_connection_t *c, xcb_screen_t *screen, int snum,
    wp_option_t *options)
{
	wp_output_t *outputs, tile_output;
	wp_option_t *option;
	uint16_t width, height;
	xcb_image_t *xcb_image;
	uint32_t *pixels;
	pixman_image_t *tmp;

	/* if possible, let X do the tiling */
	if (check_x_tiling(options)) {
		pixman_image_t *pixman_image;

		pixman_image = options->buffer->pixman_image;
		width = pixman_image_get_width(pixman_image);
		height = pixman_image_get_height(pixman_image);

		/* fake an output that fits the picture */
		tile_output.x = 0;
		tile_output.y = 0;
		tile_output.width = width;
		tile_output.height = height;
		tile_output.name = NULL;
		outputs = &tile_output;
	} else {
		width = screen->width_in_pixels;
		height = screen->height_in_pixels;
		outputs = get_outputs(c, screen);
	}
	xcb_image = create_xcb_image(c, width, height, screen->root_depth);

	pixels = xmalloc(width, height, sizeof(*pixels));
	tmp = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, pixels,
	    width * sizeof(*pixels));
	if (tmp == NULL)
		errx(1, "failed to create temporary pixman image");

	for (option = options; option->filename != NULL; option++) {
		wp_output_t *output;

		/* ignore options which are not relevant for this screen */
		if (option->screen != -1 && option->screen != snum)
			continue;

		if (option->output != NULL &&
		    strcmp(option->output, "all") == 0)
			for (output = outputs; output->name != NULL; output++)
				process_output(output, tmp, option);
		else {
			output = get_output(outputs, option->output);
			process_output(output, tmp, option);
		}
	}
	copy_pixels(xcb_image, pixels, width, height);
	set_wallpaper(c, screen, xcb_image);

	free(pixels);
	pixman_image_unref(tmp);
	xcb_image_destroy(xcb_image);
	if (outputs != &tile_output)
		free_outputs(outputs);
}

int
main(int argc, char *argv[])
{
	wp_option_t *options;
	xcb_connection_t *c;
	xcb_screen_iterator_t it;
	int snum;

#ifdef HAVE_PLEDGE
	pledge("dns inet rpath stdio unix", NULL);
#endif

	if (argc < 2 || (options = parse_options(++argv)) == NULL)
		usage();

	c = xcb_connect(NULL, NULL);

#ifdef HAVE_PLEDGE
	pledge("stdio", NULL);
#endif

	load_pixman_images(options);

	init_outputs(c);

	it = xcb_setup_roots_iterator(xcb_get_setup(c));
	for (snum = 0; it.rem; snum++, xcb_screen_next(&it))
		process_screen(c, it.data, snum, options);

	xcb_flush(c);
	xcb_disconnect(c);

	return 0;
}
