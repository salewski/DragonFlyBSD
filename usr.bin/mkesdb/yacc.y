/*	$NetBSD: src/usr.bin/mkesdb/yacc.y,v 1.3 2004/01/02 12:09:48 itojun Exp $	*/
/*	$DragonFly: src/usr.bin/mkesdb/yacc.y,v 1.1 2005/03/11 20:17:11 joerg Exp $ */

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
#include <sys/queue.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "citrus_namespace.h"
#include "citrus_types.h"
#include "citrus_region.h"
#include "citrus_esdb_file.h"
#include "citrus_db_hash.h"
#include "citrus_db_factory.h"
#include "citrus_lookup_factory.h"

#include "ldef.h"

extern FILE	*yyin;

static int			debug = 0, num_csids = 0;
static char			*output = NULL;
static char			*name, *encoding, *variable;
static uint32_t			invalid;
static int			use_invalid = 0;
static struct named_csid_list	named_csids;

static void	dump_file(void);
static void	register_named_csid(char *, uint32_t);
static void	set_prop_string(const char *, char **, char **);
static void	set_invalid(uint32_t);
%}
%union {
	uint32_t	i_value;
	char		*s_value;
}

%token			R_NAME R_ENCODING R_VARIABLE R_DEFCSID R_INVALID
%token			R_LN
%token <i_value>	L_IMM
%token <s_value>	L_STRING

%%

file		: property
		{ dump_file(); }

property	: /* empty */
		| property R_LN
		| property name R_LN
		| property encoding R_LN
		| property variable R_LN
		| property defcsid R_LN
		| property invalid R_LN

name		: R_NAME L_STRING
		{
			set_prop_string("NAME", &name, &$2);
		}

encoding	: R_ENCODING L_STRING
		{
			set_prop_string("ENCODING", &encoding, &$2);
		}
variable	: R_VARIABLE L_STRING
		{
			set_prop_string("VARIABLE", &variable, &$2);
		}
defcsid		: R_DEFCSID L_STRING L_IMM
		{
			register_named_csid($2, $3);
			$2 = NULL;
		}
invalid		: R_INVALID L_IMM
		{
			set_invalid($2);
		}
%%

int
yyerror(const char *s)
{
	fprintf(stderr, "%s in %d\n", s, line_number);

	return (0);
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
	int ret;
	FILE *fp;
	struct _db_factory *df;
	struct _region data;
	struct named_csid *csid;
	char buf[100];
	int i;
	void *serialized;
	size_t size;

	ret = 0;
	if (!name) {
		fprintf(stderr, "NAME is mandatory.\n");
		ret = 1;
	}
	if (!encoding) {
		fprintf(stderr, "ENCODING is mandatory.\n");
		ret = 1;
	}
	if (ret)
		exit(1);

	/*
	 * build database
	 */
	CHKERR(ret, _db_factory_create, (&df, _db_hash_std, NULL));

	/* store version */
	CHKERR(ret, _db_factory_add32_by_s, (df, _CITRUS_ESDB_SYM_VERSION,
					     _CITRUS_ESDB_VERSION));

	/* store encoding */
	CHKERR(ret, _db_factory_addstr_by_s, (df, _CITRUS_ESDB_SYM_ENCODING,
					      encoding));

	/* store variable */
	if (variable)
		CHKERR(ret, _db_factory_addstr_by_s,
		       (df, _CITRUS_ESDB_SYM_VARIABLE, variable));

	/* store invalid */
	if (use_invalid)
		CHKERR(ret, _db_factory_add32_by_s, (df,
						     _CITRUS_ESDB_SYM_INVALID,
						     invalid));

	/* store num of charsets */
	CHKERR(ret, _db_factory_add32_by_s, (df, _CITRUS_ESDB_SYM_NUM_CHARSETS,
					     num_csids));
	i=0;
	STAILQ_FOREACH(csid, &named_csids, ci_entry) {
		snprintf(buf, sizeof(buf), _CITRUS_ESDB_SYM_CSNAME_PREFIX "%d",
		    i);
		CHKERR(ret, _db_factory_addstr_by_s,
		       (df, buf, csid->ci_symbol));
		snprintf(buf, sizeof(buf), _CITRUS_ESDB_SYM_CSID_PREFIX "%d",
		    i);
		CHKERR(ret, _db_factory_add32_by_s, (df, buf, csid->ci_csid));
		i++;
	}

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
	CHKERR(ret, _db_factory_serialize, (df, _CITRUS_ESDB_MAGIC, &data));
	if (fwrite(serialized, size, 1, fp) != 1)
		err(EXIT_FAILURE, "fwrite");

	fclose(fp);
}

static void
set_prop_string(const char *res, char **store, char **data)
{
	char buf[256];

	if (*store) {
		snprintf(buf, sizeof(buf),
			 "%s is duplicated. ignored the one", res);
		yyerror(buf);
		return;
	}

	*store = *data;
	*data = NULL;
}

static void
set_invalid(uint32_t inv)
{
	invalid = inv;
	use_invalid = 1;
}

static void
register_named_csid(char *sym, uint32_t val)
{
	struct named_csid *csid;

	STAILQ_FOREACH(csid, &named_csids, ci_entry) {
		if (strcmp(csid->ci_symbol, sym) == 0) {
			yyerror("multiply defined CSID");
			exit(1);
		}
	}

	csid = malloc(sizeof(*csid));
	if (csid == NULL) {
		perror("malloc");
		exit(1);
	}
	csid->ci_symbol = sym;
	csid->ci_csid = val;
	STAILQ_INSERT_TAIL(&named_csids, csid, ci_entry);
	num_csids++;
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
	if (ret)
		errx(EXIT_FAILURE, "%s\n", strerror(ret));
}

static void
usage(void)
{
	errx(EXIT_FAILURE,
	     "usage:\n"
	     "\t%s [-o outfile] [infile]\n"
	     "\t%s -m [-o outfile] [infile]",
	     getprogname(), getprogname());
}

int
main(int argc, char **argv)
{
	int ch;
	FILE *in = NULL;
	int mkdb = 0;

	while ((ch=getopt(argc, argv, "do:m")) != EOF) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'o':
			output = strdup(optarg);
			break;
		case 'm':
			mkdb = 1;
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
	else {
		STAILQ_INIT(&named_csids);
		yyin=in;
		yyparse();
	}

	return (0);
}
