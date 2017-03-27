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

#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#include "functions.h"

void
error_jpg(j_common_ptr common)
{
	errx(1, "failed to parse input file");
}

pixman_image_t *
load_jpeg(FILE *fp)
{
	struct jpeg_error_mgr error_mgr;
	pixman_image_t *img;
	JSAMPLE *p;
	JSAMPARRAY scanline;
	JDIMENSION x, y, width, height;
	struct jpeg_decompress_struct cinfo;
	uint32_t *d, *data;

	cinfo.err = jpeg_std_error(&error_mgr);
	error_mgr.error_exit = error_jpg;
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	width = cinfo.image_width;
	height = cinfo.image_height;

	jpeg_start_decompress(&cinfo);

	scanline = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo,
	    JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);

	d = data = xmalloc(width, height, sizeof(*data));
	for (y = 0; y < height; y++) {
		jpeg_read_scanlines(&cinfo, scanline, 1);
		p = scanline[0];
		for (x = 0; x < width; x++) {
			uint8_t r, g, b;
			r = p[0];
			g = p[1];
			b = p[2];
			*d++ = (0xFF000000) | r << 16 | g << 8 | b;
			p += cinfo.output_components;
		}
	}

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, data,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}
