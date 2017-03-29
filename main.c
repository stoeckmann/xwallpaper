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

#define ATOM_ESETROOT "ESETROOT_PMAP_ID"
#define ATOM_XSETROOT "_XROOTPMAP_ID"

static void
usage(void)
{
	fprintf(stderr,
"usage: xsetwallpaper [-screen <screen>] [--output <output>]\n"
"  [--center <file>] [--maximize <file>]  [--stretch <file>]\n"
"  [--tile <file>] [--zoom <file>] [--version]\n");
	exit(1);
}

static void
tile(pixman_image_t *dest, wp_output_t *output, wp_option_t *option)
{
	pixman_image_t *pixman_image;
	int pixman_width, pixman_height;
	uint16_t off_x, off_y;

	pixman_image = option->buffer->pixman_image;
	pixman_width = pixman_image_get_width(pixman_image);
	pixman_height = pixman_image_get_height(pixman_image);

	/* reset transformation of possible previous transform call */
	pixman_image_set_transform(pixman_image, NULL);

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

	pixman_f_transform_init_translate(&ftransform,
	    translate_x, translate_y);
	if (option->mode != MODE_CENTER)
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
			pixman_image_t *img;

			img = load_pixman_image(buffer->fp);
			buffer->pixman_image = img;
			fclose(buffer->fp);
			if (pixman_image_get_width(img) > UINT16_MAX ||
			    pixman_image_get_height(img) > UINT16_MAX)
				errx(1, "%s has illegal dimensions",
				    option->filename);
		}
	}
}

static void
process_output(wp_output_t *output, pixman_image_t *pixman_bits,
    wp_option_t *option)
{
	if (option->mode == MODE_TILE)
		tile(pixman_bits, output, option);
	else
		transform(pixman_bits, output, option);
}

static void
update_atoms(xcb_connection_t *c, xcb_screen_t *screen, xcb_pixmap_t pixmap)
{
	int i;
	xcb_intern_atom_cookie_t atom_cookie[2];
	xcb_intern_atom_reply_t *atom_reply[2];
	xcb_get_property_cookie_t property_cookie[2];
	xcb_get_property_reply_t *property_reply[2];
	xcb_pixmap_t *old[2];

	atom_cookie[0] = xcb_intern_atom(c, 0,
	    sizeof(ATOM_ESETROOT) - 1, ATOM_ESETROOT);
	atom_cookie[1] = xcb_intern_atom(c, 0,
	    sizeof(ATOM_XSETROOT) - 1, ATOM_XSETROOT);

	for (i = 0; i < 2; i++)
		atom_reply[i] = xcb_intern_atom_reply(c, atom_cookie[i], NULL);

	for (i = 0; i < 2; i++) {
		if (atom_reply[i] != NULL)
			property_cookie[i] = xcb_get_property(c, 0,
			    screen->root, atom_reply[i]->atom, XCB_ATOM_PIXMAP,
			    0, 1);
	}

	for (i = 0; i < 2; i++) {
		if (atom_reply[i] != NULL)
			property_reply[i] =
			    xcb_get_property_reply(c, property_cookie[i], NULL);
		else
			property_reply[i] = NULL;
		if (property_reply[i] != NULL &&
		    property_reply[i]->type == XCB_ATOM_PIXMAP)
			old[i] = xcb_get_property_value(property_reply[i]);
		else
			old[i] = NULL;
	}

	if (old[0] != NULL)
		xcb_kill_client(c, *old[0]);
	if (old[1] != NULL && (old[0] == NULL || *old[0] != *old[1]))
		xcb_kill_client(c, *old[1]);

	for (i = 0; i < 2; i++) {
		if (atom_reply[i] != NULL)
			xcb_change_property(c, XCB_PROP_MODE_REPLACE,
			    screen->root, atom_reply[i]->atom, XCB_ATOM_PIXMAP,
			    32, 1, &pixmap);
		else
			warnx("failed to update atoms");
		free(property_reply[i]);
		free(atom_reply[i]);
	}
}

static void
set_wallpaper(xcb_connection_t *c, xcb_screen_t *screen, xcb_image_t *xcb_image)
{
	xcb_pixmap_t pixmap;
	xcb_gcontext_t gc;

	pixmap = xcb_generate_id(c);
	xcb_create_pixmap(c, screen->root_depth, pixmap, screen->root,
	    xcb_image->width, xcb_image->height);

	gc = xcb_generate_id(c);
	xcb_create_gc(c, gc, pixmap, 0, NULL);

	xcb_put_image(c, xcb_image->format, pixmap, gc, xcb_image->width,
	    xcb_image->height, 0, 0, 0, screen->root_depth, xcb_image->size,
	    xcb_image->data);

	xcb_change_window_attributes(c, screen->root, XCB_CW_BACK_PIXMAP,
	    &pixmap);

	update_atoms(c, screen, pixmap);

	xcb_clear_area(c, 0, screen->root, 0, 0, 0, 0);
	xcb_request_check(c,
	    xcb_set_close_down_mode(c, XCB_CLOSE_DOWN_RETAIN_PERMANENT));
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
	pixman_image_t *pixman_bits;
	size_t len, stride;

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

	SAFE_MUL(stride, width, sizeof(*pixels));
	SAFE_MUL(len, height, stride);
	pixels = calloc(len, 1);
	if (pixels == NULL)
		err(1, "failed to allocate memory");

	pixman_bits = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height,
	    pixels, stride);
	if (pixman_bits == NULL)
		errx(1, "failed to create temporary pixman image");

	for (option = options; option->filename != NULL; option++) {
		wp_output_t *output;

		/* ignore options which are not relevant for this screen */
		if (option->screen != -1 && option->screen != snum)
			continue;

		if (option->output != NULL &&
		    strcmp(option->output, "all") == 0)
			for (output = outputs; output->name != NULL; output++)
				process_output(output, pixman_bits, option);
		else {
			output = get_output(outputs, option->output);
			if (output != NULL)
				process_output(output, pixman_bits, option);
		}
	}

	xcb_image = xcb_image_create_native(c, width, height,
	    XCB_IMAGE_FORMAT_Z_PIXMAP, 32, NULL, len, (uint8_t *) pixels);
	set_wallpaper(c, screen, xcb_image);

	xcb_image_destroy(xcb_image);
	pixman_image_unref(pixman_bits);
	free(pixels);
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

	it = xcb_setup_roots_iterator(xcb_get_setup(c));
	for (snum = 0; it.rem; snum++, xcb_screen_next(&it))
		process_screen(c, it.data, snum, options);

	xcb_flush(c);
	xcb_disconnect(c);

	return 0;
}
