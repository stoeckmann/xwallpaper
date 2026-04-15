/*
 * Copyright (c) 2025 Tobias Stoeckmann <tobias@stoeckmann.org>
 * Copyright (c) 2026 Fredrik Noring
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "functions.h"

enum {
#define OPTION_ARG_ENUM(name_, arg_, desc_) OPTION_ARG_##name_,
OPTIONS(OPTION_ARG_ENUM)
};

struct optarg;

static void optparse_string(struct optarg *oa);
static void optparse_number(struct optarg *oa);

#define optparse_file		optparse_string
#define optparse_output		optparse_string
#define optparse_geometry	optparse_string

#include "argparse.h"

static void
optparse_number(struct optarg *oa)
{
	char *endptr;
	long value;

	optparse_string(oa);

	if (oa->error)
		return;

	if (strcmp(oa->string, "all") == 0) {
		oa->integer = -1;	/* "all" is a special keyword for -1 */
		return;
	}

	value = strtol(oa->string, &endptr, 10);

	if (endptr == oa->string || *endptr != '\0' ||
			value < 0 || value > INT_MAX)
		oa->error = OPTION_NOTNUMBER;

	oa->integer = value;
}

#define OPTION_PREDICATE(name_)						\
	int has_##name_##_option(wp_argcv_t argcv)			\
	{								\
		for_each_option (argcv)					\
			if (option_label == OPTION_##name_)		\
				return 1;				\
		return 0;						\
	}

OPTION_PREDICATE(help)
OPTION_PREDICATE(clear)
OPTION_PREDICATE(daemon)
OPTION_PREDICATE(debug)
OPTION_PREDICATE(no_atoms)
OPTION_PREDICATE(no_randr)
OPTION_PREDICATE(no_root)
OPTION_PREDICATE(version)

int
source_option(wp_argcv_t argcv)
{
	return !has_clear_option(argcv) && !has_no_atoms_option(argcv);
}

int
target_option(wp_argcv_t argcv)
{
	return (!has_no_atoms_option(argcv) ? TARGET_ATOMS : 0) |
	       (!has_no_root_option(argcv)  ? TARGET_ROOT  : 0);
}

int
has_desktop_option(wp_argcv_t argcv)
{
	for_each_option (argcv)
		if (option_label == OPTION_desktop)
			return 1;

	return 0;
}

int
parse_box(const char *s, wp_box_t *box)
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
		return 0;
		/* NOTREACHED */
	}

	if (UINT16_MAX - b.width < b.x_off ||
	    UINT16_MAX - b.height < b.y_off ||
	    b.width == 0 || b.height == 0)
		return 0;

	*box = b;

	return 1;
}

static
int screen_desktop_output_match(wp_match_t m,
	int screen, int desktop, const char *output)
{
	/* -1 acts as a match "all" wildcard for screen and desktop */

	if (m.screen >= 0 && screen >= 0 && m.screen != screen)
		return 0;

	if (m.desktop >= 0 && desktop >= 0 && m.desktop != desktop)
		return 0;

	if (strcmp(m.output, "all") == 0 || strcmp(output, "all") == 0)
		return 1;

	return strcmp(m.output, output) == 0;
}

wp_match_t
option_match_wallpaper(wp_argcv_t argcv,
	int screen, int desktop, const char *output)
{
	wp_match_t m = {
		.screen = -1,
		.desktop = -1,
		.output = "all",
	};
	wp_match_t best = m;

	for_each_option (argcv)
		switch (option_label) {
		case OPTION_center:
		case OPTION_focus:
		case OPTION_maximize:
		case OPTION_stretch:
		case OPTION_tile:
		case OPTION_zoom:
			m.mode = option_label;
			m.modename = &option_name[2];
			m.filename = option_string;

			if (screen_desktop_output_match(m,
					screen, desktop, output))
				best = m;
			break;

		case OPTION_trim:
			m.trim = option_string;
			break;

		case OPTION_screen:
			m.trim = NULL;
			m.screen = option_integer;
			m.desktop = -1;
			m.output = "all";
			break;

		case OPTION_desktop:
			m.trim = NULL;
			m.desktop = option_integer;
			m.output = "all";
			break;

		case OPTION_output:
			m.trim = NULL;
			m.output = option_string;
			break;
		}

	return best;
}

static void
option_verify(wp_argcv_t argcv)
{
	for_each_option (argcv)
		switch (option_error) {
		case OPTION_OK:
			continue;
		case OPTION_INVALID:
			errx(EXIT_FAILURE, "invalid option: %s\n"
				"Try the '--help' option for usage.\n",
				option_name);
		case OPTION_MISSING:
			errx(EXIT_FAILURE, "%s: missing argument, expected <%s>\n"
				"Try the '--help' option for usage.\n",
				option_name, option_expect);
		case OPTION_NOTNUMBER:
			errx(EXIT_FAILURE, "%s: argument not a number: %s\n"
				"Try the '--help' option for usage.\n",
				option_name, option_string);
		default:
			errx(EXIT_FAILURE, "unknown error\n");
		}

	if (has_daemon_option(argcv) && !has_randr)
		errx(EXIT_FAILURE, "--daemon requires RandR\n");
}

static void
help(void)
{
	printf("usage: xwallpaper [options]...\n\noptions:\n\n");

#define OPTION_HELP(name_, arg_, desc_)					\
	if (strcmp(#arg_, "void") == 0)					\
		printf("  %-21s  %s\n", opt(#name_).s, desc_);		\
	else								\
		printf("  %-10s %-10s  %s\n", opt(#name_).s, "<"#arg_">", desc_);
OPTIONS(OPTION_HELP)

	exit(EXIT_SUCCESS);
}

wp_config_t
parse_config(int argc, char **argv)
{
	wp_argcv_t argcv = { .c = argc, .v = argv };

	if (has_help_option(argcv))
		help();

	if (has_version_option(argcv)) {
		puts(VERSION);
		exit(EXIT_SUCCESS);
	}

	if (has_no_randr_option(argcv))
		has_randr = 0;

	option_verify(argcv);

	return (wp_config_t) { .argcv = argcv };
}
