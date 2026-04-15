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

struct opt {
	char s[16];
};

struct optarg {
	int label;

	int error;
	const char *expect;

	const char *name;
	const char *string;
	int integer;

	int i;
	wp_argcv_t arg;
};

static struct opt
opt(const char *label)
{
	struct opt o = { };

	for (size_t i = 0; i + 1 < sizeof(o.s); i++)
		if (i < 2)
			o.s[i] = '-';
		else if (label[i - 2] == '\0')
			break;
		else if (label[i - 2] == '_')
			o.s[i] = '-';
		else
			o.s[i] = label[i - 2];

	return o;
}

static void
optparse_void(struct optarg *oa)
{
}

static void
optparse_string(struct optarg *oa)
{
	if (oa->i >= oa->arg.c) {
		oa->error = OPTION_MISSING;
		return;
	}

	oa->i++;
}

static struct optarg
optarg_next(struct optarg oa)
{
	if (oa.i >= oa.arg.c)
		return (struct optarg) { .label = OPTION_END };

#define OPTION_PARSE(name_, arg_, desc_)				\
	if (strcmp(oa.arg.v[oa.i], opt(#name_).s) == 0) {		\
		oa.label = OPTION_##name_;				\
		oa.name = oa.arg.v[oa.i++];				\
		oa.string = oa.arg.v[oa.i];				\
		oa.expect = #arg_;					\
		optparse_##arg_(&oa);					\
		return oa;						\
	}
OPTIONS(OPTION_PARSE)

	oa.label = OPTION_INVALID;
	oa.error = OPTION_INVALID;
	oa.name = oa.arg.v[oa.i++];
	oa.string = "";

	return oa;
}

static struct optarg
optarg_init(wp_argcv_t args)
{
	return optarg_next((struct optarg) { .i = 1, .arg = args });
}

#define for_each_option(args_)						\
	for (struct optarg oa_ = optarg_init(args_);			\
	     oa_.label != OPTION_END;					\
	     oa_ = optarg_next(oa_))

#define option_label	(oa_.label)
#define option_error	(oa_.error)
#define option_name	(oa_.name)
#define option_string	(oa_.string)
#define option_integer	(oa_.integer)
#define option_expect	(oa_.expect)
