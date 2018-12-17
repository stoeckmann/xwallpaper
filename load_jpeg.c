/*
 * Copyright (c) 2017-2018 Tobias Stoeckmann <tobias@stoeckmann.org>
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

#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#include "functions.h"

typedef struct wp_err {
	struct jpeg_error_mgr	mgr;
	jmp_buf			env;
} wp_err_t;

void
error_jpg(j_common_ptr common)
{
	wp_err_t *wp_err;

	wp_err = (wp_err_t *)common->err;

	longjmp(wp_err->env, 1);
}

static void
scan_cmyk(struct jpeg_decompress_struct *cinfo, JSAMPARRAY scanline,
    uint32_t *pixels)
{
	JDIMENSION x, y, width, height;
	JSAMPLE *p;

	width = cinfo->image_width;
	height = cinfo->image_height;

	for (y = 0; y < height; y++) {
		jpeg_read_scanlines(cinfo, scanline, 1);
		p = scanline[0];
		for (x = 0; x < width; x++) {
			uint8_t c, m, y, k;

			c = 255 - p[0];
			m = 255 - p[1];
			y = 255 - p[2];
			k = 255 - p[3];

			*pixels++ = (255 - (c + k)) << 16 |
			    (255 - (m + k)) << 8 |
			    (255 - (y + k));
			p += cinfo->output_components;
		}
	}
}

static void
scan_gray(struct jpeg_decompress_struct *cinfo, JSAMPARRAY scanline,
    uint32_t *pixels)
{
	JDIMENSION x, y, width, height;
	JSAMPLE *p;

	width = cinfo->image_width;
	height = cinfo->image_height;

	for (y = 0; y < height; y++) {
		jpeg_read_scanlines(cinfo, scanline, 1);
		p = scanline[0];
		for (x = 0; x < width; x++) {
			*pixels++ = p[0] << 16 | p[0] << 8 | p[0];
			p += cinfo->output_components;
		}
	}
}

static void
scan_rgb(struct jpeg_decompress_struct *cinfo, JSAMPARRAY scanline,
    uint32_t *pixels)
{
	JDIMENSION x, y, width, height;
	JSAMPLE *p;

	width = cinfo->image_width;
	height = cinfo->image_height;

	for (y = 0; y < height; y++) {
		jpeg_read_scanlines(cinfo, scanline, 1);
		p = scanline[0];
		for (x = 0; x < width; x++) {
			*pixels++ = p[0] << 16 | p[1] << 8 | p[2];
			p += cinfo->output_components;
		}
	}
}

pixman_image_t *
load_jpeg(FILE *fp)
{
	wp_err_t wp_err;
	pixman_image_t *img;
	JSAMPARRAY scanline;
	JDIMENSION width, height;
	struct jpeg_decompress_struct cinfo;
	uint32_t *pixels;
	size_t len;

	pixels = NULL;
	cinfo.err = jpeg_std_error(&wp_err.mgr);
	wp_err.mgr.error_exit = error_jpg;

	if (setjmp(wp_err.env)) {
		debug("failed to parse input as JPEG\n");
		jpeg_destroy_decompress(&cinfo);
		free(pixels);
		return NULL;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	width = cinfo.image_width;
	height = cinfo.image_height;

	jpeg_start_decompress(&cinfo);

	scanline = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
	    JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);

	SAFE_MUL3(len, width, height, sizeof(*pixels));
	pixels = xmalloc(len);

	if (cinfo.jpeg_color_space == JCS_YCbCr)
		cinfo.out_color_space = JCS_RGB;
	else if (cinfo.jpeg_color_space == JCS_YCCK)
		cinfo.out_color_space = JCS_CMYK;

	switch (cinfo.out_color_space) {
		case JCS_GRAYSCALE:
			scan_gray(&cinfo, scanline, pixels);
			break;
		case JCS_CMYK:
			scan_cmyk(&cinfo, scanline, pixels);
			break;
		default:
			if (cinfo.output_components < 3)
				longjmp(wp_err.env, 1);
			cinfo.out_color_space = JCS_RGB;
			scan_rgb(&cinfo, scanline, pixels);
			break;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}
