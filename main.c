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

#include "config.h"

#ifdef WITH_RANDR
  #include <xcb/randr.h>
#endif /* WITH_RANDR */
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <err.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "functions.h"

#define ATOM_ESETROOT "ESETROOT_PMAP_ID"
#define ATOM_XSETROOT "_XROOTPMAP_ID"

#define MAXIMUM(x, y) ((x) > (y) ? (x) : (y))

#ifdef WITH_RANDR
xcb_pixmap_t created_pixmap = XCB_BACK_PIXMAP_NONE;
#endif /* WITH_RANDR */

static uint32_t
get_max_rows_per_request(xcb_connection_t *c, xcb_image_t *image, uint32_t n)
{
	uint32_t max_len, max_req_len, row_len, max_height;

	max_req_len = xcb_get_maximum_request_length(c);
	max_len = (max_req_len > n ? n : max_req_len) * 4;
	if (max_len <= sizeof(xcb_put_image_request_t))
		errx(1, "unable to put image on X server");
	max_len -= sizeof(xcb_put_image_request_t);
	row_len = (image->stride + image->scanline_pad - 1) &
	    -image->scanline_pad;
	max_height = max_len / row_len;
	if (max_height < 1)
		errx(1, "unable to put image on X server");
	debug("put image request parameters:\n"
	    "maximum request length allowed for server (32 bits): %u\n"
	    "maximum length for row data: %u\n"
	    "length of rows in image: %u\n"
	    "maximum height to send: %u\n",
	    max_req_len, max_len, row_len, max_height);
	return max_height;
}

static pixman_image_t *
load_pixman_image(xcb_connection_t *c, xcb_screen_t *screen, FILE *fp)
{
	pixman_image_t *pixman_image;

	pixman_image = NULL;

#ifdef WITH_PNG
	if (pixman_image == NULL) {
		rewind(fp);
		pixman_image = load_png(fp);
	}
#endif /* WITH_PNG */
#ifdef WITH_JPEG
	if (pixman_image == NULL) {
		rewind(fp);
		pixman_image = load_jpeg(fp);
	}
#endif /* WITH_JPEG */
#ifdef WITH_XPM
	if (pixman_image == NULL) {
		rewind(fp);
		pixman_image = load_xpm(c, screen, fp);
	}
#endif /* WITH_XPM */

	return pixman_image;
}

static void
load_pixman_images(xcb_connection_t *c, xcb_screen_t *screen,
    wp_option_t *options)
{
	wp_option_t *opt;
	pixman_image_t *img;

	for (opt = options; opt != NULL && opt->filename != NULL; opt++)
		if (opt->buffer->pixman_image == NULL) {
			int height, width;

			debug("loading %s\n", opt->filename);
			img = load_pixman_image(c, screen, opt->buffer->fp);
			if (img == NULL)
				errx(1, "failed to parse %s", opt->filename);
			opt->buffer->pixman_image = img;
			fclose(opt->buffer->fp);

			height = pixman_image_get_height(img);
			width = pixman_image_get_width(img);

			if (height > UINT16_MAX || width > UINT16_MAX)
				errx(1, "%s has illegal dimensions",
				    opt->filename);

			if (opt->trim != NULL) {
				wp_box_t *trim = opt->trim;

				if (height < trim->y_off + trim->height ||
				    width < trim->x_off + trim->width)
					errx(1, "%s is smaller than trim box",
					    opt->filename);
			}
		}
}

static void
tile(pixman_image_t *dest, wp_output_t *output, wp_option_t *option)
{
	pixman_image_t *pixman_image;
	int src_width, src_height, src_x, src_y;
	uint16_t off_x, off_y;

	pixman_image = option->buffer->pixman_image;

	if (option->trim == NULL) {
		src_width = pixman_image_get_width(pixman_image);
		src_height = pixman_image_get_height(pixman_image);
		src_x = 0;
		src_y = 0;
	} else {
		src_width = option->trim->width;
		src_height = option->trim->height;
		src_x = option->trim->x_off;
		src_y = option->trim->y_off;
	}

	/* reset transformation and filter of transform calls */
	pixman_image_set_transform(pixman_image, NULL);
	pixman_image_set_filter(pixman_image, PIXMAN_FILTER_FAST, NULL, 0);

	/*
	 * Manually performs tiling to support separate modes per
	 * screen with RandR. If possible, xwallpaper will let
	 * X do the tiling natively.
         */
	for (off_y = 0; off_y < output->height; off_y += src_height) {
		uint16_t h;

		if (off_y + src_height > output->height)
			h = output->height - off_y;
		else
			h = src_height;

		for (off_x = 0; off_x < output->width; off_x += src_width) {
			uint16_t w;

			if (off_x + src_width > output->width)
				w = output->width - off_x;
			else
				w = src_width;

			debug("tiling %s for %s (area %dx%d+%d+%d)\n",
			    option->filename, output->name != NULL ?
			    output->name : "screen", w, h, off_x, off_y);
			pixman_image_composite(PIXMAN_OP_CONJOINT_SRC,
			    pixman_image, NULL, dest, src_x, src_y, 0, 0,
			    off_x, off_y, w, h);
		}
	}
}

static void
transform(pixman_image_t *dest, wp_output_t *output, wp_option_t *option,
    pixman_filter_t filter)
{
	pixman_image_t *pixman_image;
	pixman_f_transform_t ftransform;
	pixman_transform_t transform;
	int mode;
	uint16_t pix_width, pix_height;
	uint16_t src_width, src_height;
	uint16_t xcb_width, xcb_height;
	float w_scale, h_scale, scale;
	float translate_x, translate_y;
	float off_x, off_y;

	mode = option->mode;
	pixman_image = option->buffer->pixman_image;
	pix_width = pixman_image_get_width(pixman_image);
	pix_height = pixman_image_get_height(pixman_image);
	xcb_width = output->width;
	xcb_height = output->height;

	if (option->trim == NULL) {
		src_width = pix_width;
		src_height = pix_height;
		off_x = 0;
		off_y = 0;
	} else {
		src_width = option->trim->width;
		src_height = option->trim->height;
		off_x = (float)option->trim->x_off;
		off_y = (float)option->trim->y_off;
	}

	if (mode == MODE_FOCUS) {
		float target_x, target_y;
		uint16_t target_width, target_height;
		float ratio;

		debug("focus on trim box %hux%hu%+.0f%+.0f of %hux%hu for "
		    "output %hux%hu\n", src_width, src_height, off_x, off_y,
		    pix_width, pix_height, xcb_width, xcb_height);

		ratio = (float)xcb_width / xcb_height;
		debug("output ratio is %f\n", ratio);

		/*
		 * Calculate minimum box to use.
		 *
		 * The minimum box depends solely on the input image dimensions
		 * and will be used if the specified trim box fully fits in it.
		 *
		 * This guarantees that we never zoom in further than needed
		 * even on very small trim boxes.
		 */
		if (pix_width > xcb_width && pix_height > xcb_height) {
			/*
			 * If the input image is larger than output, then use
			 * output dimensions. No zooming in occurs and leads
			 * to best quality.
			 */
			target_width = xcb_width;
			target_height = xcb_height;
		} else {
			/*
			 * At least one dimension of input image is smaller than
			 * the corresponding output dimension. Zooming in is
			 * required to prevent black borders.
			 */
			float rx, ry;

			rx = (float)xcb_width / pix_width;
			ry = (float)xcb_height / pix_height;
			debug("minimum box check: rx = %f, ry = %f\n", rx, ry);

			/* Zoom in and keep aspect ratio of output. */
			if (rx < ry) {
				target_width = MAXIMUM(1, pix_height * ratio);
				target_height = pix_height;
			} else {
				target_width = pix_width;
				target_height = MAXIMUM(1, pix_width / ratio);
			}
		}
		debug("minimum box dimensions are %hux%hu\n", target_width,
		    target_height);

		/*
		 * If trim box fits into minimum box, then use minimum box.
		 * Otherwise it means that box must be zoomed out to cover the
		 * whole trim box. Black borders can occur due to this.
		 */
		if (src_width > target_width || src_height > target_height) {
			float rx, ry;

			rx = (float)src_width / target_width;
			ry = (float)src_height / target_height;
			debug("target box check: rx = %f, ry = %f\n", rx, ry);

			/* Zoom out and keep aspect ratio of output. */
			if (rx < ry) {
				target_width = MAXIMUM(1, src_height * ratio);
				target_height = src_height;
			} else {
				target_width = src_width;
				target_height = MAXIMUM(1, src_width / ratio);
			}
		}
		debug("target box dimensions are %hux%hu\n", target_width,
		    target_height);

		/*
		 * Find proper offsets.
		 *
		 * If the image file lacks enough pixels around current box,
		 * then black borders on output are unfortunate but inevitable.
		 * It is much more important to keep the constraint of having
		 * all pixels within the trim box on output.
		 */
		target_x = MAXIMUM(0, off_x - (target_width - src_width) / 2);
		target_y = MAXIMUM(0, off_y - (target_height - src_height) / 2);

		if (target_width > pix_width - target_x) {
			if (target_width > pix_width)
				target_x = (pix_width - target_width) / 2;
			else
				target_x = pix_width - target_width;
		}
		if (target_height > pix_height - target_y) {
			if (target_height > pix_height)
				target_y = (pix_height - target_height) / 2;
			else
				target_y = pix_height - target_height;
		}

		mode = MODE_MAXIMIZE;
		off_x = target_x;
		off_y = target_y;
		src_width = target_width;
		src_height = target_height;

		debug("final source box is %hux%hu%+.0f%+.0f\n", src_width,
		    src_height, off_x, off_y);
	}

	w_scale = (float)src_width / xcb_width;
	h_scale = (float)src_height / xcb_height;

	switch (mode) {
	case MODE_CENTER:
		filter = PIXMAN_FILTER_FAST;
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

	translate_x = (src_width / w_scale - xcb_width) / 2 + off_x / w_scale;
	translate_y = (src_height / h_scale - xcb_height) / 2 + off_y / h_scale;

	pixman_f_transform_init_translate(&ftransform,
	    translate_x, translate_y);
	if (option->mode != MODE_CENTER)
		pixman_f_transform_scale(&ftransform, NULL, w_scale, h_scale);
	pixman_image_set_filter(pixman_image, filter, NULL, 0);
	pixman_transform_from_pixman_f_transform(&transform, &ftransform);
	pixman_image_set_transform(pixman_image, &transform);

	debug("composing %s for %s (area %dx%d+%d+%d) (mode %d)\n",
	    option->filename, output->name != NULL ? output->name : "screen",
	    output->width, output->height, 0, 0, option->mode);
	pixman_image_composite(PIXMAN_OP_CONJOINT_SRC, pixman_image, NULL, dest,
	    0, 0, 0, 0, 0, 0, output->width, output->height);
}

static void
put_wallpaper(xcb_connection_t *c, xcb_screen_t *screen, wp_output_t *output,
    xcb_image_t *xcb_image, xcb_pixmap_t pixmap, xcb_gcontext_t gc)
{
	uint8_t *data;
	uint32_t h, max_height, row_len, sub_height;
	xcb_image_t *sub;
	uint8_t depth;

	depth = screen->root_depth == 16 ? 16 : 32;

	debug("xcb image (%dx%d) to %s (%dx%d+%d+%d)\n",
	    xcb_image->width, xcb_image->height,
	    output->name != NULL ? output->name : "screen", output->width,
	    output->height, output->x, output->y);

	max_height = get_max_rows_per_request(c, xcb_image, UINT32_MAX / 4);
	if (max_height < xcb_image->height) {
		debug("image exceeds request size limitations\n");

		/* adjust for better performance */
		max_height = get_max_rows_per_request(c, xcb_image, 65536);

		sub_height = max_height;
		sub = xcb_image_create_native(c, xcb_image->width, sub_height,
		    XCB_IMAGE_FORMAT_Z_PIXMAP, depth, NULL, ~0, NULL);
		if (sub == NULL)
			errx(1, "failed to create xcb image");
	} else {
		sub = xcb_image;
		sub_height = xcb_image->height;
	}

	row_len = (xcb_image->stride + xcb_image->scanline_pad - 1) &
	    -xcb_image->scanline_pad;

	data = xcb_image->data;
	for (h = 0; h < xcb_image->height; h += sub_height) {
		if (sub_height > xcb_image->height - h) {
			sub_height = xcb_image->height - h;
			xcb_image_destroy(sub);
			sub = xcb_image_create_native(c,
			    xcb_image->width, sub_height,
			    XCB_IMAGE_FORMAT_Z_PIXMAP, depth, NULL, ~0, NULL);
			if (sub == NULL)
				errx(1, "failed to create xcb image");
		}

		debug("put image (%dx%d+0+%d) to %s (%dx%d+%d+%d)\n",
		    sub->width, sub->height, h,
		    output->name != NULL ? output->name : "screen",
		    sub->width, sub_height, output->x,
		    output->y + h);
		xcb_put_image(c, sub->format, pixmap, gc,
		    sub->width, sub->height, output->x,
		    output->y + h, 0, screen->root_depth,
		    sub->size, data);

		data += row_len * sub_height;
	}

	if (sub != xcb_image)
		xcb_image_destroy(sub);
}

static void
process_output(xcb_connection_t *c, xcb_screen_t *screen, wp_output_t *output,
    wp_option_t *option, xcb_pixmap_t pixmap, xcb_gcontext_t gc)
{
	uint32_t *pixels;
	size_t len, stride;
	xcb_image_t *xcb_image;
	pixman_image_t *pixman_image;
	pixman_format_code_t pixman_format;
	pixman_filter_t filter;
	uint8_t depth;

	depth = screen->root_depth == 16 ? 16 : 32;
	SAFE_MUL(stride, output->width, depth / 8);
	SAFE_MUL(len, output->height, stride);
	pixels = xmalloc(len);

	switch (screen->root_depth) {
	case 16:
		pixman_format = PIXMAN_r5g6b5;
		filter = PIXMAN_FILTER_BEST;
		break;
	case 30:
		pixman_format = PIXMAN_x2r10g10b10;
		filter = PIXMAN_FILTER_NEAREST;
		break;
	default:
		pixman_format = PIXMAN_x8r8g8b8;
		filter = PIXMAN_FILTER_BEST;
		break;
	}

	pixman_image = pixman_image_create_bits(pixman_format, output->width,
	    output->height, pixels, stride);
	if (pixman_image == NULL)
		errx(1, "failed to create temporary pixman image");

	if (option->mode == MODE_TILE)
		tile(pixman_image, output, option);
	else
		transform(pixman_image, output, option, filter);

	xcb_image = xcb_image_create_native(c, output->width, output->height,
	    XCB_IMAGE_FORMAT_Z_PIXMAP, depth, NULL, len, (uint8_t *) pixels);
	if (xcb_image == NULL)
		errx(1, "failed to create xcb image");

	put_wallpaper(c, screen, output, xcb_image, pixmap, gc);

	xcb_image_destroy(xcb_image);
	pixman_image_unref(pixman_image);
	free(pixels);
}

static void
process_atoms(xcb_connection_t *c, xcb_screen_t *screen, xcb_pixmap_t *pixmap,
    xcb_pixmap_t *old_pixmap)
{
	static xcb_void_cookie_t (*delete)(xcb_connection_t *, uint32_t) =
	    xcb_kill_client;
	int deleted, i;
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

	for (i = 0; i < 2; i++)
		if (atom_reply[i] != NULL)
			property_cookie[i] = xcb_get_property(c, 0,
			    screen->root, atom_reply[i]->atom, XCB_ATOM_PIXMAP,
			    0, 1);

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

	deleted = 0;
	if (old[0] != NULL && pixmap != NULL && *old[0] != *pixmap) {
		delete(c, *old[0]);
		deleted = 1;
	}
	if (old[1] != NULL && (old[0] == NULL || *old[0] != *old[1])) {
		delete(c, *old[1]);
		deleted = 1;
	}
	if (deleted)
		delete = xcb_free_pixmap;

	if (old_pixmap != NULL) {
		if (old[0] != NULL && old[1] != NULL && *old[0] == *old[1])
			*old_pixmap = *old[0];
		else
			*old_pixmap = XCB_BACK_PIXMAP_NONE;
	}

	for (i = 0; i < 2; i++) {
		if (pixmap != NULL) {
			if (atom_reply[i] != NULL) {
				if (*pixmap == XCB_BACK_PIXMAP_NONE)
					xcb_delete_property(c,
					    screen->root,
					    atom_reply[i]->atom);
				else
					xcb_change_property(c,
					    XCB_PROP_MODE_REPLACE,
					    screen->root,
					    atom_reply[i]->atom,
					    XCB_ATOM_PIXMAP, 32, 1, pixmap);
			} else
				warnx("failed to update atoms");
		}
		free(property_reply[i]);
		free(atom_reply[i]);
	}
}

static void
process_screen(xcb_connection_t *c, xcb_screen_t *screen, int snum,
    wp_config_t *config)
{
	xcb_pixmap_t pixmap, result;
	xcb_gcontext_t gc;
	xcb_get_geometry_cookie_t geom_cookie;
	xcb_get_geometry_reply_t *geom_reply;
	wp_output_t *outputs, tile_output;
	wp_option_t *opt, *options;
	uint16_t width, height;
	xcb_rectangle_t rectangle;
	int created;

	options = config->options;

	/* let X perform non-randr tiling if requested */
	if (options != NULL && options[0].mode == MODE_TILE &&
	    options[0].output == NULL && options[1].filename == NULL) {
		pixman_image_t *pixman_image = options->buffer->pixman_image;

		/* fake an output that fits the picture */
		width = pixman_image_get_width(pixman_image);
		height = pixman_image_get_height(pixman_image);
		tile_output = (wp_output_t){
			.x = 0,
			.y = 0,
			.width = width,
			.height = height,
			.name = NULL
		};
		outputs = &tile_output;
	} else {
		width = screen->width_in_pixels;
		height = screen->height_in_pixels;
		outputs = get_outputs(c, screen);
	}

	if (config->source == SOURCE_ATOMS) {
		process_atoms(c, screen, NULL, &pixmap);
		if (pixmap != XCB_BACK_PIXMAP_NONE) {
			geom_cookie = xcb_get_geometry(c, pixmap);
			geom_reply = xcb_get_geometry_reply(c, geom_cookie, NULL);
			if (geom_reply == NULL || geom_reply->width != width ||
			    geom_reply->height != height ||
			    geom_reply->depth != screen->root_depth)
				pixmap = XCB_BACK_PIXMAP_NONE;
			free(geom_reply);
		}
	} else
		pixmap = XCB_BACK_PIXMAP_NONE;

	if (pixmap == XCB_BACK_PIXMAP_NONE) {
		debug("creating pixmap (%dx%d)\n", width, height);
		pixmap = xcb_generate_id(c);
#ifdef WITH_RANDR
		if (config->daemon && (config->target & TARGET_ATOMS) &&
		    !xcb_connection_has_error(c))
			created_pixmap = pixmap;
#endif /* WITH_RANDR */
		xcb_create_pixmap(c, screen->root_depth, pixmap, screen->root,
		    width, height);
		gc = xcb_generate_id(c);
		xcb_create_gc(c, gc, pixmap, 0, NULL);
		rectangle = (xcb_rectangle_t){
			.x = 0,
			.y = 0,
			.width = width,
			.height = height
		};
		xcb_poly_fill_rectangle(c, pixmap, gc, 1, &rectangle);
		created = 1;
	} else {
		debug("reusing atom pixmap (%dx%d)\n", width, height);
		gc = xcb_generate_id(c);
		xcb_create_gc(c, gc, pixmap, 0, NULL);
		created = 0;
	}

	for (opt = options; opt != NULL && opt->filename != NULL; opt++) {
		wp_output_t *output;

		/* ignore options which are not relevant for this screen */
		if (opt->screen != -1 && opt->screen != snum)
			continue;

		if (opt->output != NULL &&
		    strcmp(opt->output, "all") == 0)
			for (output = outputs; output->name != NULL; output++)
				process_output(c, screen, output, opt,
				    pixmap, gc);
		else {
			output = get_output(outputs, opt->output);
			if (output != NULL)
				process_output(c, screen, output, opt,
				    pixmap, gc);
		}
	}

	if (options == NULL)
		result = XCB_BACK_PIXMAP_NONE;
	else
		result = pixmap;

	xcb_free_gc(c, gc);
	if (config->target & TARGET_ROOT) {
		/* always set a pixmap, even before clearing */
		xcb_change_window_attributes(c, screen->root,
		    XCB_CW_BACK_PIXMAP, &pixmap);
		if (result == XCB_BACK_PIXMAP_NONE) {
			xcb_change_window_attributes(c, screen->root,
			    XCB_CW_BACK_PIXMAP, &result);
			xcb_free_pixmap(c, pixmap);
		}
	}
	if (config->target & TARGET_ATOMS) {
		process_atoms(c, screen, &result, NULL);
		if (created)
			xcb_set_close_down_mode(c,
			    XCB_CLOSE_DOWN_RETAIN_PERMANENT);
	} else
		xcb_free_pixmap(c, pixmap);
	xcb_request_check(c, xcb_clear_area(c, 0, screen->root, 0, 0, 0, 0));

	if (outputs != &tile_output)
		free_outputs(outputs);
}

static void
usage(void)
{
	fprintf(stderr,
"usage: xwallpaper [--screen <screen>] [--clear] [--daemon] [--debug]\n"
"  [--no-atoms] [--no-randr] [--no-root] [--trim widthxheight[+x+y]]\n"
"  [--output <output>] [--center <file>] [--focus <file>]\n"
"  [--maximize <file>] [--stretch <file>] [--tile <file>] [--zoom <file>]\n"
"  [--version]\n");
	exit(1);
}

#ifdef WITH_RANDR
static void
process_event(wp_config_t *config, xcb_connection_t *c,
    xcb_generic_event_t *event) {
	xcb_randr_screen_change_notify_event_t *randr_event;
	xcb_screen_iterator_t it;
	int snum;

	randr_event = (xcb_randr_screen_change_notify_event_t *)event;
	debug("event received: response_type=%u, sequence=%u\n",
	    randr_event->response_type, randr_event->sequence);
	it = xcb_setup_roots_iterator(xcb_get_setup(c));
	for (snum = 0; it.rem; snum++, xcb_screen_next(&it)) {
		if (it.data->root == randr_event->root) {
			it.data->width_in_pixels = randr_event->width;
			it.data->height_in_pixels = randr_event->height;
			process_screen(c, it.data, snum, config);
		}
	}
	if (xcb_connection_has_error(c))
		warnx("error encountered while setting wallpaper");
}
#endif /* WITH_RANDR */

int
main(int argc, char *argv[])
{
	wp_config_t *config;
	xcb_connection_t *c;
#ifdef WITH_RANDR
	xcb_connection_t *c2;
#endif /* WITH_RANDR */
	xcb_generic_event_t *event;
	xcb_screen_iterator_t it;
	int snum;
#ifdef HAVE_PLEDGE
	if (pledge("dns inet proc rpath stdio unix", NULL) == -1)
		err(1, "pledge");
#endif /* HAVE_PLEDGE */
#ifdef WITH_SECCOMP
	stage1_sandbox();
#endif /* WITH_SECCOMP */
	if (argc < 2 || (config = parse_config(++argv)) == NULL)
		usage();

	if (config->daemon && daemon(0, show_debug) < 0)
		warnx("failed to daemonize");

	c = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(c))
		errx(1, "failed to connect to X server");
#ifdef WITH_RANDR
	if (config->daemon) {
		c2 = xcb_connect(NULL, NULL);
		if (xcb_connection_has_error(c2))
			errx(1, "failed to connect to X server for clean up");
	}
#endif /* WITH_RANDR */
#ifdef HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");
#endif /* HAVE_PLEDGE */
#ifdef WITH_SECCOMP
	stage2_sandbox();
#endif /* WITH_SECCOMP */

	it = xcb_setup_roots_iterator(xcb_get_setup(c));
	/*
	 * Needs a screen for possible XPM color parsing.
	 * Technically this means that it has to be repeated for
	 * every screen, but it's unlikely to even encounter a setup
	 * which has multiple ones. Also the exact colors are parsed
	 * on purpose, so I don't expect a difference.
	 */
	if (it.rem == 0)
		errx(1, "no screen found");
	load_pixman_images(c, it.data, config->options);

	for (snum = 0; it.rem; snum++, xcb_screen_next(&it))
		process_screen(c, it.data, snum, config);

	if (xcb_connection_has_error(c))
		warnx("error encountered while setting wallpaper");

#ifdef WITH_RANDR
	if (config->daemon) {
		it = xcb_setup_roots_iterator(xcb_get_setup(c));
		for (snum = 0; it.rem; snum++, xcb_screen_next(&it))
			xcb_request_check(c, xcb_randr_select_input(c,
			    it.data->root,
			    XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE));

		while ((event = xcb_wait_for_event(c)) != NULL)
			process_event(config, c, event);
	}
#endif /* WITH_RANDR */

	xcb_disconnect(c);

#ifdef WITH_RANDR
	if (config->daemon) {
		if (created_pixmap != XCB_BACK_PIXMAP_NONE) {
			debug("killing X client\n");
			xcb_request_check(c2,
			    xcb_kill_client(c2, created_pixmap));
		}
		if (xcb_connection_has_error(c2))
			debug("failed to kill X client\n");
		xcb_disconnect(c2);
	}
#endif /* WITH_RANDR */

	return 0;
}
