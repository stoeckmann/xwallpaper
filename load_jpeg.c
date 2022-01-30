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

static void
error_jpg(j_common_ptr common)
{
	wp_err_t *wp_err;

	wp_err = (wp_err_t *)common->err;

	longjmp(wp_err->env, 1);
}

static pixman_image_t *
do_load_jpeg(FILE *fp, struct jpeg_decompress_struct *cinfo, uint32_t **pixels)
{
	wp_err_t wp_err;
	pixman_image_t *img;
	JDIMENSION y, width, height;
	uint32_t *p;
	size_t len;

	cinfo->err = jpeg_std_error(&wp_err.mgr);
	wp_err.mgr.error_exit = error_jpg;

	if (setjmp(wp_err.env)) {
		debug("failed to parse input as (RGB) JPEG\n");
		jpeg_destroy_decompress(cinfo);
		return NULL;
	}

	jpeg_create_decompress(cinfo);
	jpeg_stdio_src(cinfo, fp);
	jpeg_read_header(cinfo, TRUE);

	width = cinfo->image_width;
	height = cinfo->image_height;
	cinfo->out_color_space = JCS_EXT_BGRA;

	jpeg_start_decompress(cinfo);

	SAFE_MUL3(len, width, height, sizeof(**pixels));
	p = *pixels = xmalloc(len);

	if (cinfo->output_components != 4)
		longjmp(wp_err.env, 1);

	for (y = 0; y < height; y++) {
		jpeg_read_scanlines(cinfo, (JSAMPARRAY)&p, 1);
		p += width;
	}

	jpeg_finish_decompress(cinfo);
	jpeg_destroy_decompress(cinfo);

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, *pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}

pixman_image_t *
load_jpeg(FILE *fp)
{
	struct jpeg_decompress_struct cinfo;
	pixman_image_t *img;
	uint32_t *pixels;

	pixels = NULL;
	img = do_load_jpeg(fp, &cinfo, &pixels);
	if (img == NULL)
		free(pixels);
	return img;
}
