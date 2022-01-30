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

#include <err.h>
#include <limits.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"

static size_t
add_buffer(wp_buffer_t **bufs, size_t *count, wp_buffer_t buf)
{
	size_t i;

	for (i = 0; i < *count; i++)
		if ((*bufs)[i].st_dev == buf.st_dev &&
		    (*bufs)[i].st_ino == buf.st_ino)
			break;

	if (*count != 0 && i != *count)
		fclose(buf.fp);
	else {
		*bufs = realloc(*bufs, (*count + 1) * sizeof(**bufs));
		if (*bufs == NULL)
			err(1, "failed to allocate memory");
		(*bufs)[(*count)++] = buf;
	}

	return i;
}

static void
add_option(wp_config_t *config, wp_option_t option)
{
	size_t i;
	wp_option_t *o;

	if (option.filename == NULL)
		return;

	for (i = 0; i < config->count; i++)
		if (config->options[i].output != NULL &&
		    strcmp(config->options[i].output, option.output) == 0 &&
		    config->options[i].screen == option.screen)
			break;

	if (i != config->count)
		o = config->options + i;
	else {
		config->options = realloc(config->options,
		    (config->count + 2) * sizeof(*config->options));
		if (config->options == NULL)
			err(1, "failed to allocate memory");
		o = &config->options[config->count++];
		config->options[config->count].filename = NULL;
	}
	*o = option;
}

static void
init_buffers(wp_config_t *config)
{
	wp_buffer_t *buffers, buffer;
	size_t buffers_count, i, len;
	size_t *refs;

	if (config->count == 0)
		return;

	SAFE_MUL(len, config->count, sizeof(*refs));
	refs = xmalloc(len);

	buffers = NULL;
	buffers_count = 0;
	buffer = (wp_buffer_t){ 0 };

	for (i = 0; i < config->count; i++) {
		struct stat st;

		if ((buffer.fp = fopen(config->options[i].filename, "rb"))
		    == NULL)
			err(1, "open '%s' failed", config->options[i].filename);
		if (fstat(fileno(buffer.fp), &st))
			err(1, "stat '%s' failed", config->options[i].filename);
		buffer.st_dev = st.st_dev;
		buffer.st_ino = st.st_ino;

		refs[i] = add_buffer(&buffers, &buffers_count, buffer);
	}

	for (i = 0; i < config->count; i++)
		config->options[i].buffer = &buffers[refs[i]];
	free(refs);
}

static int
parse_mode(char *mode)
{
	if (strcmp(mode, "--center") == 0)
		return MODE_CENTER;
	if (strcmp(mode, "--focus") == 0)
		return MODE_FOCUS;
	if (strcmp(mode, "--maximize") == 0)
		return MODE_MAXIMIZE;
	if (strcmp(mode, "--stretch") == 0)
		return MODE_STRETCH;
	if (strcmp(mode, "--tile") == 0)
		return MODE_TILE;
	if (strcmp(mode, "--zoom") == 0)
		return MODE_ZOOM;
	return -1;
}

static int
parse_screen(char *screen)
{
	char *endptr;
	long value;

	value = strtol(screen, &endptr, 10);
	if (endptr == screen || *endptr != '\0' || value < 0 || value > INT_MAX)
		errx(1, "failed to parse screen number: %s", screen);
	return value;
}

static int
parse_box(char *s, wp_box_t **box)
{
	wp_box_t b;

	switch (sscanf(s, "%hux%hu+%hu+%hu", &b.width, &b.height,
	    &b.x_off, &b.y_off)) {
	case 2:
		b.x_off = 0;
		b.y_off = 0;
		break;
	case 4:
		break;
	default:
		return 1;
		/* NOTREACHED */
	}

	if (UINT16_MAX - b.width < b.x_off ||
	    UINT16_MAX - b.height < b.y_off ||
	    b.width == 0 || b.height == 0)
		return 1;

	*box = xmalloc(sizeof(*box));
	**box = b;

	return 0;
}

wp_config_t *
parse_config(char **argv)
{
	wp_config_t *config;
	wp_option_t last;

	config = xmalloc(sizeof(*config));
	*config = (wp_config_t){
		.options = NULL,
		.count = 0,
		.daemon = 0,
		.source = SOURCE_ATOMS,
		.target = TARGET_ATOMS | TARGET_ROOT
	};

	last = (wp_option_t){ .screen = -1 };

	while (*argv != NULL) {
		if (strcmp(argv[0], "--daemon") == 0) {
			if (has_randr == 0) {
				warnx("--daemon requires RandR");
				return NULL;
			}
			config->daemon = 1;
		} else if (strcmp(argv[0], "--debug") == 0)
			show_debug = 1;
		else if (strcmp(argv[0], "--clear") == 0)
			config->source = 0;
		else if (strcmp(argv[0], "--no-atoms") == 0) {
			config->target &= ~TARGET_ATOMS;
			if (config->target == 0) {
				warnx("--no-atoms conflicts with --no-root");
				return NULL;
			}
		} else if (strcmp(argv[0], "--no-root") == 0) {
			config->target &= ~TARGET_ROOT;
			if (config->target == 0) {
				warnx("--no-root conflicts with --no-atoms");
				return NULL;
			}
		} else if (strcmp(argv[0], "--screen") == 0) {
			if (*++argv == NULL) {
				warnx("missing argument for --screen");
				return NULL;
			}
			last.screen = parse_screen(*argv);
		} else if (strcmp(argv[0], "--output") == 0) {
			if (*++argv == NULL) {
				warnx("missing argument for --output");
				return NULL;
			}
			if (has_randr == 0) {
				warnx("--output requires RandR");
				return NULL;
			}
			add_option(config, last);
			last.trim = NULL;
			last.output = *argv;
		} else if ((last.mode = parse_mode(*argv)) != -1) {
			if (*++argv == NULL) {
				warnx("missing argument for %s", *(argv - 1));
				return NULL;
			}
			last.filename = *argv;
		} else if (strcmp(argv[0], "--no-randr") == 0) {
			if (config->count > 0) {
				warnx("--no-randr conflicts with --output");
				return NULL;
			}
			if (config->daemon) {
				warnx("--daemon requires RandR");
				return NULL;
			}
			has_randr = 0;
		} else if (strcmp(argv[0], "--trim") == 0) {
			if (*++argv == NULL) {
				warnx("missing argument for --trim");
				return NULL;
			}
			if (parse_box(*argv, &last.trim)) {
				warnx("invalid trim box: %s\n", *argv);
				return NULL;
			}
		} else if (strcmp(argv[0], "--version") == 0) {
			puts(VERSION);
			exit(0);
		} else {
			warnx("illegal argument: %s", *argv);
			return NULL;
		}
		++argv;
	}
	if (has_randr == -1 && last.output == NULL)
		last.output = "all";
	add_option(config, last);

	if (!(config->target & TARGET_ATOMS))
		config->source = 0;

	if (config->count == 0 && config->source != 0)
		return NULL;

	init_buffers(config);

	return config;
}
