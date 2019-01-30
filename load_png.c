/*
 * Copyright (c) 2019 Tobias Stoeckmann <tobias@stoeckmann.org>
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
#include <png.h>
#include <stdint.h>
#include <stdlib.h>

#include "functions.h"

#define HEADER_LEN	4

static int
is_png(FILE *fp)
{
	int valid;
	uint8_t header[HEADER_LEN];

	valid = fread(header, 1, HEADER_LEN, fp) == HEADER_LEN &&
	    png_sig_cmp(header, 0, HEADER_LEN) == 0;
	rewind(fp);

	return valid;
}

pixman_image_t *
load_png(FILE *fp)
{
	pixman_image_t *img;
	png_bytepp rows;
	uint32_t *pixel, *pixels;
	int trans;
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 x, y, width, height;
	size_t len;
	unsigned char *p;

	if (!is_png(fp))
		return NULL;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    NULL, NULL, NULL);
	if (png_ptr == NULL)
		errx(1, "failed to initialize png struct");

	if (setjmp(png_jmpbuf(png_ptr))) {
		debug("failed to parse file as PNG\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		debug("failed to initialize png info");
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return NULL;
	}

	png_init_io(png_ptr, fp);

	trans = PNG_TRANSFORM_EXPAND;
	trans |= PNG_TRANSFORM_GRAY_TO_RGB;
	trans |= PNG_TRANSFORM_STRIP_16;
	trans |= PNG_TRANSFORM_STRIP_ALPHA;
	png_read_png(png_ptr, info_ptr, trans, NULL);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	rows = png_get_rows(png_ptr, info_ptr);

	SAFE_MUL3(len, width, height, sizeof(*pixels));
	pixel = pixels = xmalloc(len);
	for (y = 0; y < height; y++)
		for (p = rows[y], x = 0; x < width; x++, p += 3)
			*pixel++ = (p[0] << 16) | (p[1] << 8) | p[2];

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}
