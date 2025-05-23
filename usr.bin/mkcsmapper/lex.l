/*	$NetBSD: src/usr.bin/mkcsmapper/lex.l,v 1.3 2003/10/27 00:12:43 lukem Exp $	*/
/*	$DragonFly: src/usr.bin/mkcsmapper/lex.l,v 1.1 2005/03/11 20:17:11 joerg Exp $ */

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

#include <sys/endian.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldef.h"
#include "yacc.h"

int line_number = 1;
%}

%x	COMMENT

%%

[ \t]+	{ }
#.*[\n]|"//".*[\n]|[\n]	{ line_number++; return (R_LN); }

"/*"		{ BEGIN COMMENT; }
<COMMENT>"*/"	{ BEGIN 0; }
<COMMENT>[\n]	{ line_number++; }
<COMMENT>.	{ }
<COMMENT><<EOF>>	{
		yyerror("unexpected file end (unterminate comment)\n");
		exit(1);
	}

"="|"/"|"-"	{ return (int)yytext[0]; }

([1-9][0-9]*)|(0[0-9]*)|(0[xX][0-9A-Fa-f]+)	{
		yylval.i_value = strtoul(yytext, NULL, 0);
		return L_IMM;
	}

"TYPE"		{ return (R_TYPE); }
"NAME"		{ return (R_NAME); }
"SRC_ZONE"	{ return (R_SRC_ZONE); }
"DST_INVALID"	{ return (R_DST_INVALID); }
"DST_ILSEQ"	{ return (R_DST_ILSEQ); }
"DST_UNIT_BITS"	{ return (R_DST_UNIT_BITS); }
"BEGIN_MAP"	{ return (R_BEGIN_MAP); }
"END_MAP"	{ return (R_END_MAP); }
"INVALID"	{ return (R_INVALID); }
"ILSEQ"		{ return (R_ILSEQ); }
"OOB_MODE"	{ return (R_OOB_MODE); }
"ROWCOL"	{ return (R_ROWCOL); }

\"([^\"\n]*(\\\")?)*\"|\'([^\'\n]*(\\\')?)*\'	{
		size_t len;
		len = strlen(yytext);
		yylval.s_value = malloc(len-1);
		strlcpy(yylval.s_value, yytext+1, len-1);
		return L_STRING;
	}
[^ =/\-0-9\t\n][^ \t\n]*	{
		yylval.s_value = strdup(yytext);
		return L_STRING;
	}

%%

#ifndef yywrap
int
yywrap(void)
{
	return (1);
}
#endif
