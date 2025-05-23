/*	$NetBSD: src/usr.bin/mkcsmapper/yacc.y,v 1.5 2004/01/05 19:20:10 itojun Exp $	*/
/*	$DragonFly: src/usr.bin/mkcsmapper/yacc.y,v 1.1 2005/03/11 20:17:11 joerg Exp $ */

%{
/*-
 * Copyright (c)2003 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>

#include "ldef.h"

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_mapper_std_file.h"
#include "citrus_region.h"
#include "citrus_db_factory.h"
#include "citrus_db_hash.h"
#include "citrus_lookup_factory.h"
#include "citrus_pivot_factory.h"

extern FILE *	yyin;

int			debug = 0;
static char		*output = NULL;
static void		*table = NULL;
static size_t		table_size;
static char		*map_name;
static int		map_type;
static zone_t		src_zone;
static u_int32_t	colmask, rowmask;
static u_int32_t	dst_invalid, dst_ilseq, oob_mode, dst_unit_bits;
static void		(*putfunc)(void *, size_t, u_int32_t) = 0;

static u_int32_t	src_next;
static int		next_valid;

static u_int32_t	done_flag = 0;
#define DF_TYPE			0x00000001
#define DF_NAME			0x00000002
#define DF_SRC_ZONE		0x00000004
#define DF_DST_INVALID		0x00000008
#define DF_DST_ILSEQ		0x00000010
#define DF_DST_UNIT_BITS	0x00000020
#define DF_OOB_MODE		0x00000040

static void	dump_file(void);
static void	setup_map(void);
static void	set_type(int);
static void	set_name(char *);
static void	set_src_zone(const zone_t *);
static void	set_dst_invalid(u_int32_t);
static void	set_dst_ilseq(u_int32_t);
static void	set_dst_unit_bits(u_int32_t);
static void	set_oob_mode(u_int32_t);
static void	calc_next(void);
static int	check_src(u_int32_t, u_int32_t);
static void	store(const linear_zone_t *, u_int32_t, int);
static void	put8(void *, size_t, u_int32_t);
static void	put16(void *, size_t, u_int32_t);
static void	put32(void *, size_t, u_int32_t);
%}

%union {
	u_int32_t	i_value;
	char		*s_value;
	zone_t		z_value;
	linear_zone_t	lz_value;
}

%token			R_TYPE R_NAME R_SRC_ZONE R_DST_UNIT_BITS
%token			R_DST_INVALID R_DST_ILSEQ
%token			R_BEGIN_MAP R_END_MAP R_INVALID R_ROWCOL
%token			R_ILSEQ R_OOB_MODE
%token			R_LN
%token <i_value>	L_IMM
%token <s_value>	L_STRING

%type <z_value>		zone
%type <lz_value>	src
%type <i_value>		dst types oob_mode_sel

%%

file		: property mapping lns
		{ dump_file(); }

property	: /* empty */
		| property R_LN
		| property name
		| property type
		| property src_zone
		| property dst_invalid
		| property dst_ilseq
		| property dst_unit_bits
		| property oob_mode

name		: R_NAME L_STRING { set_name($2); $2 = NULL; }
type		: R_TYPE types { set_type($2); }
types		: R_ROWCOL { $$ = R_ROWCOL; }
src_zone	: R_SRC_ZONE zone { set_src_zone(&$2); }
zone		: L_IMM '-' L_IMM {
			$$.row_begin = $$.row_end = 0;
			$$.col_begin = $1; $$.col_end = $3;
			$$.col_bits = 32;
		}
		| L_IMM '-' L_IMM '/' L_IMM '-' L_IMM '/' L_IMM {
			$$.row_begin = $1; $$.row_end = $3;
			$$.col_begin = $5; $$.col_end = $7;
			$$.col_bits = $9;
		}

dst_invalid	: R_DST_INVALID L_IMM { set_dst_invalid($2); }
dst_ilseq	: R_DST_ILSEQ L_IMM { set_dst_ilseq($2); }
dst_unit_bits	: R_DST_UNIT_BITS L_IMM { set_dst_unit_bits($2); }
oob_mode	: R_OOB_MODE oob_mode_sel { set_oob_mode($2); }

oob_mode_sel	: R_INVALID { $$ = _CITRUS_MAPPER_STD_OOB_NONIDENTICAL; }
		| R_ILSEQ { $$ = _CITRUS_MAPPER_STD_OOB_ILSEQ; }

mapping		: begin_map map_elems R_END_MAP
begin_map	: R_BEGIN_MAP lns { setup_map(); }

map_elems	: /* empty */
		| map_elems map_elem lns

map_elem	: src '=' dst
		{ store(&$1, $3, 0); }
		| src '=' L_IMM '-'
		{ store(&$1, $3, 1); }
dst		: L_IMM
		{
			$$ = $1;
		}
		| R_INVALID
		{
			$$ = dst_invalid;
		}
		| R_ILSEQ
		{
			$$ = dst_ilseq;
		}

src		: /* empty */
		{
			if (!next_valid) {
				yyerror("cannot omit src");
			}
			$$.begin = $$.end = src_next;
			calc_next();
		}
		| L_IMM
		{
			if (check_src($1, $1)) {
				yyerror("illegal zone");
			}
			$$.begin = $$.end = $1;
			src_next = $1;
			calc_next();
		}
		| L_IMM '-' L_IMM
		{
			if (check_src($1, $3)) {
				yyerror("illegal zone");
			}
			$$.begin = $1; $$.end = $3;
			src_next = $3;
			calc_next();
		}
		| '-' L_IMM
		{
			if (!next_valid) {
				yyerror("cannot omit src");
			}
			if (check_src(src_next, $2)) {
				yyerror("illegal zone");
			}
			$$.begin = src_next; $$.end = $2;
			src_next = $2;
			calc_next();
		}
lns		: R_LN
		| lns R_LN

%%

static void
warning(const char *s)
{
	fprintf(stderr, "%s in %d\n", s, line_number);
}

int
yyerror(const char *s)
{
	warning(s);
	exit(1);
}

void
put8(void *ptr, size_t ofs, u_int32_t val)
{
	*((u_int8_t *)ptr + ofs) = val;
}

void
put16(void *ptr, size_t ofs, u_int32_t val)
{
	u_int16_t oval = htons(val);
	memcpy((u_int16_t *)ptr + ofs, &oval, 2);
}

void
put32(void *ptr, size_t ofs, u_int32_t val)
{
	u_int32_t oval = htonl(val);
	memcpy((u_int32_t *)ptr + ofs, &oval, 4);
}

static void
alloc_table(void)
{
	size_t i;
	u_int32_t val = 0;

	table_size =
	    (src_zone.row_end-src_zone.row_begin + 1) *
	    (src_zone.col_end-src_zone.col_begin + 1);
	table = malloc(table_size*dst_unit_bits / 8);
	if (!table) {
		perror("malloc");
		exit(1);
	}

	switch (oob_mode) {
	case _CITRUS_MAPPER_STD_OOB_NONIDENTICAL:
		val = dst_invalid;
		break;
	case _CITRUS_MAPPER_STD_OOB_ILSEQ:
		val = dst_ilseq;
		break;
	default:
		;
	}
	for (i = 0; i < table_size; i++)
		(*putfunc)(table, i, val);
}

static void
setup_map(void)
{

	if ((done_flag & DF_SRC_ZONE)==0) {
		fprintf(stderr, "SRC_ZONE is mandatory.\n");
		exit(1);
	}
	if ((done_flag & DF_DST_UNIT_BITS)==0) {
		fprintf(stderr, "DST_UNIT_BITS is mandatory.\n");
		exit(1);
	}

	if ((done_flag & DF_DST_INVALID) == 0)
		dst_invalid = 0xFFFFFFFF;
	if ((done_flag & DF_DST_ILSEQ) == 0)
		dst_ilseq = 0xFFFFFFFE;
	if ((done_flag & DF_OOB_MODE) == 0)
		oob_mode = _CITRUS_MAPPER_STD_OOB_NONIDENTICAL;

	alloc_table();
}

static void
create_rowcol_info(struct _region *r)
{
	void *ptr;
	size_t ofs;

	ofs = 0;
	ptr = malloc(_CITRUS_MAPPER_STD_ROWCOL_INFO_SIZE);
	if (ptr==NULL)
		err(EXIT_FAILURE, "malloc");

	put32(ptr, ofs, src_zone.col_bits); ofs++;
	put32(ptr, ofs, dst_invalid); ofs++;
	put32(ptr, ofs, src_zone.row_begin); ofs++;
	put32(ptr, ofs, src_zone.row_end); ofs++;
	put32(ptr, ofs, src_zone.col_begin); ofs++;
	put32(ptr, ofs, src_zone.col_end); ofs++;
	put32(ptr, ofs, dst_unit_bits); ofs++;
	put32(ptr, ofs, 0); /* pad */

	_region_init(r, ptr, _CITRUS_MAPPER_STD_ROWCOL_INFO_SIZE);
}

static void
create_rowcol_ext_ilseq_info(struct _region *r)
{
	void *ptr;
	size_t ofs;

	ofs = 0;
	ptr = malloc(_CITRUS_MAPPER_STD_ROWCOL_EXT_ILSEQ_SIZE);
	if (ptr==NULL)
		err(EXIT_FAILURE, "malloc");

	put32(ptr, ofs, oob_mode); ofs++;
	put32(ptr, ofs, dst_ilseq); ofs++;

	_region_init(r, ptr, _CITRUS_MAPPER_STD_ROWCOL_EXT_ILSEQ_SIZE);
}

#define CHKERR(ret, func, a)						\
do {									\
	ret = func a;							\
	if (ret)							\
		errx(EXIT_FAILURE, "%s: %s", #func, strerror(ret));	\
} while (/*CONSTCOND*/0)

static void
dump_file(void)
{
	FILE *fp;
	int ret;
	struct _db_factory *df;
	struct _region data;
	void *serialized;
	size_t size;

	/*
	 * build database
	 */
	CHKERR(ret, _db_factory_create, (&df, _db_hash_std, NULL));

	/* store type */
	CHKERR(ret, _db_factory_addstr_by_s,
	       (df, _CITRUS_MAPPER_STD_SYM_TYPE,
		_CITRUS_MAPPER_STD_TYPE_ROWCOL));

	/* store info */
	create_rowcol_info(&data);
	CHKERR(ret, _db_factory_add_by_s,
	       (df, _CITRUS_MAPPER_STD_SYM_INFO, &data, 1));

	/* ilseq extension */
	create_rowcol_ext_ilseq_info(&data);
	CHKERR(ret, _db_factory_add_by_s,
	       (df, _CITRUS_MAPPER_STD_SYM_ROWCOL_EXT_ILSEQ, &data, 1));

	/* store table */
	_region_init(&data, table, table_size*dst_unit_bits/8);
	CHKERR(ret, _db_factory_add_by_s,
	       (df, _CITRUS_MAPPER_STD_SYM_TABLE, &data, 1));

	/*
	 * dump database to file
	 */
	if (output)
		fp = fopen(output, "wb");
	else
		fp = stdout;

	if (fp == NULL) {
		perror("fopen");
		exit(1);
	}

	/* dump database body */
	size = _db_factory_calc_size(df);
	serialized = malloc(size);
	_region_init(&data, serialized, size);
	CHKERR(ret, _db_factory_serialize,
	       (df, _CITRUS_MAPPER_STD_MAGIC, &data));
	if (fwrite(serialized, size, 1, fp) != 1)
		err(EXIT_FAILURE, "fwrite");

	fclose(fp);
}

static void
/*ARGSUSED*/
set_type(int type)
{

	if (done_flag & DF_TYPE) {
		warning("TYPE is duplicated. ignored this one");
		return;
	}

	map_type = type;

	done_flag |= DF_TYPE;
}
static void
/*ARGSUSED*/
set_name(char *str)
{

	if (done_flag & DF_NAME) {
		warning("NAME is duplicated. ignored this one");
		return;
	}

	map_name = str;

	done_flag |= DF_NAME;
}
static void
set_src_zone(const zone_t *zone)
{

	if (done_flag & DF_SRC_ZONE) {
		warning("SRC_ZONE is duplicated. ignored this one");
		return;
	}

	/* sanity check */
	if (zone->col_bits < 1 || zone->col_bits > 32) {
		goto bad;
	}

	if (zone->col_bits != 32)
		colmask = (1 << zone->col_bits )- 1;
	else
		colmask = ~0;
	rowmask = ~colmask;
	if (zone->col_begin > zone->col_end ||
	    zone->row_begin > zone->row_end ||
	    (zone->col_begin & rowmask) != 0 ||
	    (zone->col_end & rowmask) != 0 ||
	    ((zone->row_begin << zone->col_bits) & colmask) != 0 ||
	    ((zone->row_end << zone->col_bits) & colmask) != 0) {
bad:
		yyerror("Illegal argument for SRC_ZONE");
	}

	src_zone = *zone;

	done_flag |= DF_SRC_ZONE;

	return;
}
static void
set_dst_invalid(u_int32_t val)
{

	if (done_flag & DF_DST_INVALID) {
		warning("DST_INVALID is duplicated. ignored this one");
		return;
	}

	dst_invalid = val;

	done_flag |= DF_DST_INVALID;
}
static void
set_dst_ilseq(u_int32_t val)
{

	if (done_flag & DF_DST_ILSEQ) {
		warning("DST_ILSEQ is duplicated. ignored this one");
		return;
	}

	dst_ilseq = val;

	done_flag |= DF_DST_ILSEQ;
}
static void
set_oob_mode(u_int32_t val)
{

	if (done_flag & DF_OOB_MODE) {
		warning("OOB_MODE is duplicated. ignored this one");
		return;
	}

	oob_mode = val;

	done_flag |= DF_OOB_MODE;
}
static void
set_dst_unit_bits(u_int32_t val)
{

	if (done_flag & DF_DST_UNIT_BITS) {
		warning("DST_UNIT_BITS is duplicated. ignored this one");
		return;
	}

	switch (val) {
	case 8:
		putfunc = &put8;
		dst_unit_bits = val;
		break;
	case 16:
		putfunc = &put16;
		dst_unit_bits = val;
		break;
	case 32:
		putfunc = &put32;
		dst_unit_bits = val;
		break;
	default:
		yyerror("Illegal argument for DST_UNIT_BITS");
	}
	done_flag |= DF_DST_UNIT_BITS;
}
static void
calc_next(void)
{
	src_next++;
	if (check_src(src_next, src_next))
		next_valid = 0;
	else
		next_valid = 1;
}
static int
check_src(u_int32_t begin, u_int32_t end)
{
	u_int32_t b_row = 0, e_row = 0, b_col, e_col;

	b_col = begin & colmask;
	e_col = end & colmask;
	if (src_zone.col_bits != 32) {
		b_row = begin >> src_zone.col_bits;
		e_row = end >> src_zone.col_bits;
	}

	if (b_row != e_row ||
	    b_row < src_zone.row_begin ||
	    b_row > src_zone.row_end ||
	    e_row < src_zone.row_begin ||
	    e_row > src_zone.row_end ||
	    b_col < src_zone.col_begin ||
	    b_col > src_zone.col_end ||
	    e_col < src_zone.col_begin ||
	    e_col > src_zone.col_end ||
	    b_col>e_col) {
		return (-1);
	}
	return (0);
}
static void
store(const linear_zone_t *lz, u_int32_t dst, int inc)
{
	u_int32_t row=0, col, ofs, i;

	if (src_zone.col_bits != 32)
		row = lz->begin >> src_zone.col_bits;
	col = lz->begin & colmask;
	ofs = (row-src_zone.row_begin) *
	    (src_zone.col_end-src_zone.col_begin + 1) +
	    (col-src_zone.col_begin);
	for (i = lz->end - lz->begin + 1; i > 0; i--) {
		(*putfunc)(table, ofs++, dst);
		if (inc)
			dst++;
	}
}

static void
do_mkdb(FILE *in)
{
	int ret;
	FILE *out;

        /* dump DB to file */
	if (output)
		out = fopen(output, "wb");
	else
		out = stdout;

	if (out==NULL)
		err(EXIT_FAILURE, "fopen");

	ret = _lookup_factory_convert(out, in);
	fclose(out);
	if (ret && output)
		unlink(output); /* dump failure */
}

static void
do_mkpv(FILE *in)
{
	int ret;
	FILE *out;

        /* dump pivot to file */
	if (output)
		out = fopen(output, "wb");
	else
		out = stdout;

	if (out==NULL)
		err(EXIT_FAILURE, "fopen");

	ret = _pivot_factory_convert(out, in);
	fclose(out);
	if (ret && output)
		unlink(output); /* dump failure */
	if (ret)
		errx(EXIT_FAILURE, "%s\n", strerror(ret));
}

static void
usage(void)
{
	warnx("usage: \n"
	      "\t%s [-d] [-o outfile] [infile]\n"
	      "\t%s -m [-d] [-o outfile] [infile]\n"
	      "\t%s -p [-d] [-o outfile] [infile]\n",
	      getprogname(), getprogname(), getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	FILE *in = NULL;
	int mkdb = 0, mkpv = 0;

	while ((ch = getopt(argc, argv, "do:mp")) != EOF) {
		switch (ch) {
		case 'd':
			debug=1;
			break;
		case 'o':
			output = strdup(optarg);
			break;
		case 'm':
			mkdb = 1;
			break;
		case 'p':
			mkpv = 1;
			break;
		default:
			usage();
		}
	}

	argc-=optind;
	argv+=optind;
	switch (argc) {
	case 0:
		in = stdin;
		break;
	case 1:
		in = fopen(argv[0], "r");
		if (!in)
			err(EXIT_FAILURE, argv[0]);
		break;
	default:
		usage();
	}

	if (mkdb)
		do_mkdb(in);
	else if (mkpv)
		do_mkpv(in);
	else {
		yyin = in;
		yyparse();
	}

	return (0);
}
