%{
/*
 * $OpenBSD: scan.l,v 1.14 2003/12/02 09:00:07 otto Exp $
 * $DragonFly: src/usr.bin/bc/scan.l,v 1.1 2004/09/20 04:20:34 dillon Exp $
 */

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
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
#include <stdbool.h>
#include <string.h>

#include "extern.h"
#include "y.tab.h"

int		lineno;

static char	*strbuf = NULL;
static size_t	strbuf_sz = 1;
static bool	dot_seen;

static void	init_strbuf(void);
static void	add_str(const char *);

%}

DIGIT		[0-9A-F]
ALPHA		[a-z_]
ALPHANUM	[a-z_0-9]

%x		comment string number

%%

"/*"		BEGIN(comment);
<comment>{
	"*/"	BEGIN(INITIAL);
	\n	lineno++;
	\*	;
	[^*\n]+	;
	<<EOF>>	fatal("end of file in comment");
}

\"		BEGIN(string); init_strbuf();
<string>{
	[^"\n\\\[\]]+	add_str(yytext);
	\[	add_str("\\[");
	\]	add_str("\\]");
	\\	add_str("\\\\");
	\n	add_str("\n"); lineno++;
	\"	BEGIN(INITIAL); yylval.str = strbuf; return STRING;
	<<EOF>>	fatal("end of file in string");
}

{DIGIT}+	{
			BEGIN(number);
			dot_seen = false;
			init_strbuf();
			add_str(yytext);
		}
\.		{
			BEGIN(number);
			dot_seen = true;
			init_strbuf();
			add_str(".");
		}
<number>{
	{DIGIT}+	add_str(yytext);
	\.	{
			if (dot_seen) {
				BEGIN(INITIAL);
				yylval.str = strbuf;
				unput('.');
				return NUMBER;
			} else {
				dot_seen = true;
				add_str(".");
			}
		}
	\\\n[ \t]*	lineno++;
	[^0-9A-F\.]	{
			BEGIN(INITIAL);
			unput(yytext[0]);
			if (strcmp(strbuf, ".") == 0)
				return DOT;
			else {
				yylval.str = strbuf;
				return NUMBER;
			}
		}
}

"auto"		return AUTO;
"break"		return BREAK;
"continue"	return CONTINUE;
"define"	return DEFINE;
"else"		return ELSE;
"ibase"		return IBASE;
"if"		return IF;
"last"		return DOT;
"for"		return FOR;
"length"	return LENGTH;
"obase"		return OBASE;
"print"		return PRINT;
"quit"		return QUIT;
"return"	return RETURN;
"scale"		return SCALE;
"sqrt"		return SQRT;
"while"		return WHILE;

"^"		return EXPONENT;
"*"		return MULTIPLY;
"/"		return DIVIDE;
"%"		return REMAINDER;

"!"		return BOOL_NOT;
"&&"		return BOOL_AND;
"||"		return BOOL_OR;

"+"		return PLUS;
"-"		return MINUS;

"++"		return INCR;
"--"		return DECR;

"="		yylval.str = ""; return ASSIGN_OP;
"+="		yylval.str = "+"; return ASSIGN_OP;
"-="		yylval.str = "-"; return ASSIGN_OP;
"*="		yylval.str = "*"; return ASSIGN_OP;
"/="		yylval.str = "/"; return ASSIGN_OP;
"%="		yylval.str = "%"; return ASSIGN_OP;
"^="		yylval.str = "^"; return ASSIGN_OP;

"=="		return EQUALS;
"<="		return LESS_EQ;
">="		return GREATER_EQ;
"!="		return UNEQUALS;
"<"		return LESS;
">"		return GREATER;

","		return COMMA;
";"		return SEMICOLON;

"("		return LPAR;
")"		return RPAR;

"["		return LBRACKET;
"]"		return RBRACKET;

"{"		return LBRACE;
"}"		return RBRACE;

{ALPHA}{ALPHANUM}* {
			/* alloc an extra byte for the type marker */
			char *p = malloc(yyleng + 2);
			if (p == NULL)
				err(1, NULL);
			strlcpy(p, yytext, yyleng + 1);
			yylval.astr = p;
			return LETTER;
		}

\\\n		lineno++;
\n		lineno++; return NEWLINE;

#[^\n]*		;
[ \t]		;
<<EOF>>		return QUIT;
.		yyerror("illegal character");

%%

static void
init_strbuf(void)
{
	if (strbuf == NULL) {
		strbuf = malloc(strbuf_sz);
		if (strbuf == NULL)
			err(1, NULL);
	}
	strbuf[0] = '\0';
}

static void
add_str(const char *str)
{
	size_t arglen;

	arglen = strlen(str);

	if (strlen(strbuf) + arglen + 1 > strbuf_sz) {
		size_t newsize;
		char *p;

		newsize = strbuf_sz + arglen + 1;
		p = realloc(strbuf, newsize);
		if (p == NULL) {
			free(strbuf);
			err(1, NULL);
		}
		strbuf_sz = newsize;
		strbuf = p;
	}
	strlcat(strbuf, str, strbuf_sz);
}

void
abort_line(int sig)
{
	if (isatty(fileno(yyin))) {
		YY_FLUSH_BUFFER;
		printf("[\n]P\n");
	}
}
