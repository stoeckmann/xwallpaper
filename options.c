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
		memcpy(&(*bufs)[(*count)++], &buf, sizeof(buf));
	}

	return i;
}

static void
add_option(wp_option_t **opts, size_t *count, wp_option_t opt)
{
	size_t i;
	wp_option_t *o;

	if (opt.filename == NULL)
		return;

	for (i = 0; i < *count; i++)
		if ((*opts)[i].output != NULL &&
		    strcmp((*opts)[i].output, opt.output) == 0 &&
		    (*opts)[i].screen == opt.screen)
			break;

	if (*count != 0 && i != *count)
		o = &(*opts)[i];
	else {
		*opts = realloc(*opts, (*count + 2) * sizeof(**opts));
		if (*opts == NULL)
			err(1, "failed to allocate memory");
		o = &(*opts)[(*count)++];
		(*opts)[*count].filename = NULL;
	}

	memcpy(o, &opt, sizeof(opt));
}

static void
init_buffers(wp_option_t *options, size_t options_count)
{
	wp_buffer_t *buffers, buffer;
	size_t buffers_count, i, len;
	size_t *refs;

	SAFE_MUL(len, options_count, sizeof(*refs));
	refs = xmalloc(len);

	buffers = NULL;
	buffers_count = 0;
	memset(&buffer, 0, sizeof(buffer));

	for (i = 0; i < options_count; i++) {
		struct stat st;

		if ((buffer.fp = fopen(options[i].filename, "r")) == NULL)
			err(1, "failed to open %s", options[i].filename);
		if (fstat(fileno(buffer.fp), &st))
			err(1, "failed to stat %s", options[i].filename);
		buffer.st_dev = st.st_dev;
		buffer.st_ino = st.st_ino;

		refs[i] = add_buffer(&buffers, &buffers_count, buffer);
	}

	for (i = 0; i < options_count; i++)
		options[i].buffer = &buffers[refs[i]];
	free(refs);
}

static int
parse_mode(char *mode)
{
	if (strcmp(mode, "--center") == 0)
		return MODE_CENTER;
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
	long int value;

	value = strtol(screen, &endptr, 10);
	if (endptr == screen || *endptr != '\0' || value < 0 || value > INT_MAX)
		errx(1, "failed to parse screen number: %s", optarg);
	return value;
}

wp_option_t *
parse_options(char **argv)
{
	wp_option_t *options, last;
	size_t count;

	options = NULL;
	count = 0;

	memset(&last, 0, sizeof(last));
	last.screen = -1;

	while (*argv != NULL) {
		if (strcmp(argv[0], "--screen") == 0) {
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
			add_option(&options, &count, last);
			last.output = *argv;
		} else if ((last.mode = parse_mode(*argv)) != -1) {
			if (*++argv == NULL) {
				warnx("missing argument for %s", *argv);
				return NULL;
			}
			last.filename = *argv;
		} else if (strcmp(argv[0], "--version") == 0) {
			puts(VERSION);
			exit(0);
		} else {
			warnx("illegal argument: %s", *argv);
			return NULL;
		}
		++argv;
	}
	add_option(&options, &count, last);

	init_buffers(options, count);

	return options;
}
