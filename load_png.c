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

static pixman_image_t *
do_load_png(FILE *fp, png_structp *png_ptr, png_infop *info_ptr,
    uint32_t **pixels)
{
	pixman_image_t *img;
	png_bytepp rows;
	uint32_t *p;
	png_byte type, depth;
	png_uint_32 y, width, height;
	size_t len;

	if (!is_png(fp))
		return NULL;

	*png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    NULL, NULL, NULL);
	if (*png_ptr == NULL)
		errx(1, "failed to initialize png struct");

	if (setjmp(png_jmpbuf(*png_ptr))) {
		debug("failed to parse file as PNG\n");
		png_destroy_read_struct(png_ptr, info_ptr, NULL);
		return NULL;
	}

	*info_ptr = png_create_info_struct(*png_ptr);
	if (*info_ptr == NULL) {
		debug("failed to initialize png info");
		png_destroy_read_struct(png_ptr, NULL, NULL);
		return NULL;
	}

	png_init_io(*png_ptr, fp);

	png_read_info(*png_ptr, *info_ptr);
	width = png_get_image_width(*png_ptr, *info_ptr);
	height = png_get_image_height(*png_ptr, *info_ptr);
	type = png_get_color_type(*png_ptr, *info_ptr);
	depth = png_get_bit_depth(*png_ptr, *info_ptr);
#if defined(PNG_READ_INTERLACING_SUPPORTED)
	png_set_interlace_handling(*png_ptr);
#endif /* PNG_READ_INTERLACING_SUPPORTED */

	switch (type) {
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		if (depth < 8)
			png_set_expand_gray_1_2_4_to_8(*png_ptr);
		else if (depth == 16)
			png_set_strip_16(*png_ptr);
		png_set_gray_to_rgb(*png_ptr);
		break;
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(*png_ptr);
		if (png_get_valid(*png_ptr, *info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(*png_ptr);
		break;
	default:
		if (depth == 16)
			png_set_strip_16(*png_ptr);
		break;
	}
	if (!(type & PNG_COLOR_MASK_ALPHA))
		png_set_filler(*png_ptr, 0xff, PNG_FILLER_AFTER);
	if (type & PNG_COLOR_MASK_COLOR)
		png_set_bgr(*png_ptr);
	png_read_update_info(*png_ptr, *info_ptr);

	SAFE_MUL3(len, width, height, sizeof(**pixels));
	p = *pixels = xmalloc(len);

	SAFE_MUL(len, height, sizeof(*rows));
	rows = xmalloc(len);
	for (y = 0; y < height; y++) {
		rows[y] = (png_bytep)p;
		p += width;
	}
	png_read_image(*png_ptr, rows);
	free(rows);

	png_destroy_read_struct(png_ptr, info_ptr, NULL);

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, *pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}

pixman_image_t *
load_png(FILE *fp)
{
	png_structp png_ptr;
	png_infop info_ptr;
	pixman_image_t *img;
	uint32_t *pixels;

	pixels = NULL;
	img = do_load_png(fp, &png_ptr, &info_ptr, &pixels);
	if (img == NULL)
		free(pixels);
	return img;
}
