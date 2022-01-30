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

#include <sys/types.h>
#include <sys/stat.h>

#include <X11/xpm.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "functions.h"

pixman_image_t *
load_xpm(xcb_connection_t *c, xcb_screen_t *screen, FILE *fp)
{
	pixman_image_t *img;
	XpmImage xpm_image;
	XpmInfo xpm_info;
	XpmColor *color;
	uint8_t *colormap;
	uint32_t *pixel, *pixels;
	struct stat st;
	int fd;
	char *buf;
	size_t j, clen, len;
	unsigned int *d, i, width, height;

	if ((fd = fileno(fp)) == -1 || fstat(fd, &st) ||
	    st.st_size < 0 || (uintmax_t)st.st_size > (uintmax_t)SIZE_MAX) {
		debug("failed to handle size of XPM file\n");
		return NULL;
	}

	buf = xmalloc((size_t)st.st_size);
	if (fread(buf, (size_t)st.st_size, 1, fp) != 1 ||
	    XpmCreateXpmImageFromBuffer(buf, &xpm_image, &xpm_info)) {
		debug("failed to parse XPM file\n");
		free(buf);
		return NULL;
	}
	free(buf);
	XpmFreeXpmInfo(&xpm_info);

	SAFE_MUL(clen, 3, xpm_image.ncolors);
	colormap = xmalloc(clen);
	for (i = 0, j = 0; i < xpm_image.ncolors; i++) {
		char *s;
		uint16_t r, g, b;

		color = &xpm_image.colorTable[i];
		if (color->c_color != NULL)
			s = color->c_color;
		else if (color->g_color != NULL)
			s = color->g_color;
		else if (color->g4_color != NULL)
			s = color->g4_color;
		else if (color->m_color != NULL)
			s = color->m_color;
		else
			s = NULL;

		if (s == NULL || strcmp(s, "None") == 0)
			s = "#000000";

		if (!xcb_aux_parse_color(s, &r, &g, &b)) {
			xcb_lookup_color_cookie_t color_cookie;
			xcb_lookup_color_reply_t *color_reply;

			color_cookie = xcb_lookup_color(c,
			    screen->default_colormap, strlen(s), s);
			color_reply = xcb_lookup_color_reply(c,
			    color_cookie, NULL);
			if (color_reply != NULL) {
				r = color_reply->exact_red;
				g = color_reply->exact_green;
				b = color_reply->exact_blue;
				free(color_reply);
			} else
				r = g = b = 0;
		}
		colormap[j++] = r >> 8;
		colormap[j++] = g >> 8;
		colormap[j++] = b >> 8;
	}

	width = xpm_image.width;
	height = xpm_image.height;
	SAFE_MUL3(len, width, height, sizeof(*pixels));
	pixel = pixels = xmalloc(len);

	d = xpm_image.data;
	for (i = 0; i < width * height; i++) {
		unsigned char *p;

		p = &colormap[*(d++) * 3];
		if (p > colormap + clen - 3)
			*pixel++ = 0;
		else
			*pixel++ = (p[0] << 16) | (p[1] << 8) | p[2];
	}

	free(colormap);
	XpmFreeXpmImage(&xpm_image);

	img = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height, pixels,
	    width * sizeof(uint32_t));
	if (img == NULL)
		errx(1, "failed to create pixman image");

	return img;
}
