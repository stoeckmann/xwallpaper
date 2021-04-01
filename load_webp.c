/*
 * Copyright (c) 2021 Tobias Stoeckmann <tobias@stoeckmann.org>
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
#include <stdlib.h>

#include <webp/decode.h>

#include "functions.h"


pixman_image_t *
load_webp(FILE *fp)
{
	pixman_image_t *img;
	uint8_t *data, *data_buffer, *p;
  uint32_t *pixel, *pixels;
	size_t data_size;
	unsigned int width, height, ok, len;

  fseek(fp, 0, SEEK_END);
  data_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  data = (uint8_t*)WebPMalloc(data_size + 1); // + 1 for \0 terminator
  if (data == NULL) {
    errx(1, "failed to allocate memory for WebP image data");
    return NULL;
  }

  ok = (fread(data, data_size, 1, fp) == 1);
	if (!ok) {
		errx(1, "failed to read image data from WebP file");
    WebPFree(data);
		return NULL;
	}

	data_buffer = WebPDecodeARGB(data, data_size, &width, &height);

  SAFE_MUL3(len, width, height, sizeof(*pixels));
  pixel = pixels = xmalloc(len);

  p = data_buffer;
  for (int i = 0; i < width * height; i++) {
    *pixel++ = (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | (p[3]);
    p += 4;
  }

  img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, pixels,
      width * sizeof(uint32_t));

  if (img == NULL)
    errx(1, "failed to create pixman image");

  WebPFree(data);
  WebPFree(data_buffer);

	return img;
}
