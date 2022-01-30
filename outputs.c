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

#include <xcb/xcb.h>
#ifdef WITH_RANDR
  #include <xcb/randr.h>
#endif /* WITH_RANDR */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"

#ifdef WITH_RANDR
int has_randr = -1;
#else
int has_randr = 0;
#endif /* WITH_RANDR */

void
free_outputs(wp_output_t *outputs)
{
	wp_output_t *output;

	for (output = outputs; output->name != NULL; output++)
		free(output->name);
	free(outputs);
}

wp_output_t *
get_output(wp_output_t *outputs, char *name)
{
	wp_output_t *output;

	for (output = outputs; output->name != NULL; output++)
		if (name != NULL && strcmp(output->name, name) == 0)
			return output;

	if (name != NULL) {
		warnx("output %s was not found/disconnected, ignoring", name);
		return NULL;
	}

	return output;
}

#ifdef WITH_RANDR
static int
check_randr(xcb_connection_t *c)
{
	const xcb_query_extension_reply_t *reply;

	reply = xcb_get_extension_data(c, &xcb_randr_id);
	return reply != NULL && reply->present;
}

static wp_output_t *
get_randr_outputs(xcb_connection_t *c, xcb_screen_t *screen)
{
	wp_output_t *outputs;
	xcb_randr_get_screen_resources_cookie_t resources_cookie;
	xcb_randr_get_screen_resources_reply_t *resources_reply;
	xcb_randr_output_t *xcb_outputs;
	int i, j, len;
	size_t n;

	resources_cookie = xcb_randr_get_screen_resources(c, screen->root);
	resources_reply = xcb_randr_get_screen_resources_reply(c,
	    resources_cookie, NULL);

	xcb_outputs = xcb_randr_get_screen_resources_outputs(resources_reply);
	len = xcb_randr_get_screen_resources_outputs_length(resources_reply);
	if (len < 1)
		errx(1, "failed to retrieve randr outputs");

	SAFE_MUL(n, (size_t)len + 1, sizeof(*outputs));
	outputs = xmalloc(n);

	j = 0;
	for (i = 0; i < len; i++) {
		xcb_randr_get_output_info_cookie_t output_cookie;
		xcb_randr_get_output_info_reply_t *output_reply;
		xcb_randr_get_crtc_info_cookie_t crtc_cookie;
		xcb_randr_get_crtc_info_reply_t *crtc_reply;
		int name_len;
		uint8_t *name;

		output_cookie = xcb_randr_get_output_info(c, xcb_outputs[i],
		    XCB_CURRENT_TIME);
		output_reply = xcb_randr_get_output_info_reply(c, output_cookie,
		    NULL);

		if (output_reply->connection != XCB_RANDR_CONNECTION_CONNECTED ||
		    output_reply->crtc == XCB_NONE)
			continue;

		crtc_cookie = xcb_randr_get_crtc_info(c, output_reply->crtc,
		    XCB_CURRENT_TIME);
		crtc_reply = xcb_randr_get_crtc_info_reply(c, crtc_cookie,
		    NULL);

		name = xcb_randr_get_output_info_name(output_reply);
		name_len = xcb_randr_get_output_info_name_length(output_reply);

		outputs[j] = (wp_output_t){
			.x = crtc_reply->x,
			.y = crtc_reply->y,
			.width = crtc_reply->width,
			.height = crtc_reply->height
		};
		outputs[j].name = xmalloc((size_t)name_len + 1);
		memcpy(outputs[j].name, name, name_len);
		outputs[j].name[name_len] = '\0';

		debug("output detected: %s, %dx%d+%d+%d\n", outputs[j].name,
		    outputs[j].width, outputs[j].height, outputs[j].x,
		    outputs[j].y);
		j++;
	}

	outputs[j] = (wp_output_t){
		.name = NULL,
		.x = 0,
		.y = 0,
		.width = screen->width_in_pixels,
		.height = screen->height_in_pixels
	};
	debug("(randr) screen dimensions: %dx%d+%d+%d\n", outputs[j].width,
	    outputs[j].height, outputs[j].x, outputs[j].y);
	return outputs;
}
#endif /* WITH_RANDR */

wp_output_t *
get_outputs(xcb_connection_t *c, xcb_screen_t *screen)
{
	wp_output_t *outputs;
#ifdef WITH_RANDR
	if (has_randr == -1)
		has_randr = check_randr(c);
	if (has_randr)
		return get_randr_outputs(c, screen);
#endif /* WITH_RANDR */
	outputs = xmalloc(sizeof(*outputs));

	outputs[0] = (wp_output_t){
		.name = NULL,
		.x = 0,
		.y = 0,
		.width = screen->width_in_pixels,
		.height = screen->height_in_pixels
	};
	debug("(no randr) screen dimensions: %dx%d+%d+%d\n",
	    outputs[0].width, outputs[0].height, outputs[0].x, outputs[0].y);
	return outputs;
}
