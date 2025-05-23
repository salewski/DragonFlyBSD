%x string name charmap defn nchar subs subs2
%{
/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/colldef/scan.l,v 1.11.2.2 2002/10/11 10:43:45 ache Exp $
 * $DragonFly: src/usr.bin/colldef/scan.l,v 1.3 2005/03/02 07:16:23 joerg Exp $
 */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "common.h"
#include "y.tab.h"

int line_no = 1, save_no, fromsubs;
static u_char buffer[BUFSIZE], *buf_ptr;
FILE *map_fp;
YY_BUFFER_STATE main_buf, map_buf;
#ifdef FLEX_DEBUG
YYSTYPE yylval;
#endif /* FLEX_DEBUG */
int yylex(void);

%}
%%
<INITIAL,charmap,nchar,subs,subs2>[ \t]+      ;
<subs2>\"               { buf_ptr = buffer; BEGIN(string); }
<subs>\<                { buf_ptr = buffer; fromsubs = 1; BEGIN(name); }
<INITIAL>\<             { buf_ptr = buffer; fromsubs = 0; BEGIN(name); }
^#.*\n			line_no++;
^\n			line_no++;
<INITIAL>\\\n           line_no++;
<INITIAL,nchar,subs>\\t      { yylval.ch = '\t'; return CHAR; }
<INITIAL,nchar,subs>\\n      { yylval.ch = '\n'; return CHAR; }
<INITIAL,nchar,subs>\\b      { yylval.ch = '\b'; return CHAR; }
<INITIAL,nchar,subs>\\f      { yylval.ch = '\f'; return CHAR; }
<INITIAL,nchar,subs>\\v      { yylval.ch = '\v'; return CHAR; }
<INITIAL,nchar,subs>\\r      { yylval.ch = '\r'; return CHAR; }
<INITIAL,nchar,subs>\\a      { yylval.ch = '\a'; return CHAR; }
<subs2>\n               {
	line_no++;
	BEGIN(INITIAL);
	return '\n';
}
<INITIAL,nchar>\n       {
	line_no++;
	if (map_fp != NULL) {
		buf_ptr = buffer;
		BEGIN(defn);
	}
	return '\n';
}
<INITIAL>[;,{}()]       return *yytext;
<INITIAL>substitute     { BEGIN(subs); return SUBSTITUTE; }
<subs>with              { BEGIN(subs2); return WITH; }
<INITIAL>order          return ORDER;
<INITIAL>charmap        BEGIN(charmap);
<INITIAL>;[ \t]*\.\.\.[ \t]*;   return RANGE;
<INITIAL,nchar,subs>\\[0-7]{3}       {
	u_int v;

	sscanf(&yytext[1], "%o", &v);
	yylval.ch = (u_char)v;
	return CHAR;
}
<INITIAL,nchar,subs>\\x[0-9a-fA-F]{2}   {
	u_int v;

	sscanf(&yytext[2], "%x", &v);
	yylval.ch = (u_char)v;
	return CHAR;
}
<INITIAL,nchar,subs>\\. { yylval.ch = yytext[1]; return CHAR; }
<INITIAL,nchar,subs>.   { yylval.ch = *yytext; return CHAR; }
<defn>^#.*\n            line_no++;
<defn>[ \t]+            {
	if (buf_ptr == buffer)
		errx(EX_UNAVAILABLE, "map expected near line %u of %s",
		     line_no, map_name);
	*buf_ptr = '\0';
	strcpy(yylval.str, buffer);
	BEGIN(nchar);
	return DEFN;
}
<name>\/\/              {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "name buffer overflow near line %u, character '/'",
		     line_no);
	*buf_ptr++ = '/';
}
<name>\/\>              {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "name buffer overflow near line %u, character '>'",
		     line_no);
	*buf_ptr++ = '>';
}
<string>\\\"		{
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\"'",
		     line_no);
	*buf_ptr++ = '"';
}
<name>\>		{
	u_int i;

	if (buf_ptr == buffer)
		errx(EX_UNAVAILABLE, "non-empty name expected near line %u",
		     line_no);
	*buf_ptr = '\0';
	for (i = 0; i <= UCHAR_MAX; i++) {
		if (strcmp(charmap_table[i], buffer) == 0)
			goto findit;
	}
	errx(EX_UNAVAILABLE, "name <%s> not 'charmap'-defined near line %u",
		buffer, line_no);
 findit:
	yylval.ch = i;
	if (fromsubs)
		BEGIN(subs);
	else
		BEGIN(INITIAL);
	return CHAR;
}
<string>\"		{
	*buf_ptr = '\0';
	strcpy(yylval.str, buffer);
	BEGIN(subs2);
	return STRING;
}
<name,defn>.            {
	const char *s = (map_fp != NULL) ? map_name : "input";

	if (!isascii(*yytext) || !isprint(*yytext))
		errx(EX_UNAVAILABLE, "non-ASCII or non-printable character 0x%02x not allowed in the map/name near line %u of %s",
		     *yytext, line_no, s);
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "map/name buffer overflow near line %u of %s, character '%c'",
		     line_no, s, *yytext);
	*buf_ptr++ = *yytext;
}
<string>\\t             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\t'",
		     line_no);
	*buf_ptr++ = '\t';
}
<string>\\b             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\b'",
		     line_no);
	*buf_ptr++ = '\b';
}
<string>\\f             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\f'",
		     line_no);
	*buf_ptr++ = '\f';
}
<string>\\v             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\v'",
		     line_no);
	*buf_ptr++ = '\v';
}
<string>\\n             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\n'",
		     line_no);
	*buf_ptr++ = '\n';
}
<string>\\r             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\r'",
		     line_no);
	*buf_ptr++ = '\r';
}
<string>\\a             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '\\a'",
		     line_no);
	*buf_ptr++ = '\a';
}
<name,string,defn>\n            {
	const char *s = (map_fp != NULL) ? map_name : "input";

	errx(EX_UNAVAILABLE, "unterminated map/name/string near line %u of %s", line_no, s);
}
<name,string,nchar><<EOF>>      {
	const char *s = (map_fp != NULL) ? map_name : "input";

	errx(EX_UNAVAILABLE, "premature EOF in the name/string/char near line %u of %s", line_no, s);
}
<string>\\x[0-9a-f]{2}          {
	u_int v;

	sscanf(&yytext[2], "%x", &v);
	*buf_ptr++ = (u_char)v;
}
<string>\\[0-7]{3}              {
	u_int v;

	sscanf(&yytext[1], "%o", &v);
	*buf_ptr++ = (u_char)v;
}
<string>\\.             {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '%c'",
		     line_no, yytext[1]);
	*buf_ptr++ = yytext[1];
}
<string>.               {
	if(buf_ptr >= buffer + sizeof(buffer) - 1)
		errx(EX_UNAVAILABLE, "string buffer overflow near line %u, character '%c'",
		     line_no, *yytext);
	*buf_ptr++ = *yytext;
}
<charmap>[^ \t\n]+	{
	strcat(map_name, "/");
	strcat(map_name, yytext);
	if((map_fp = fopen(map_name, "r")) == NULL)
		err(EX_UNAVAILABLE, "can't open 'charmap' file %s",
		    map_name);
	save_no = line_no;
	line_no = 1;
	map_buf = yy_new_buffer(map_fp, YY_BUF_SIZE);
	main_buf = YY_CURRENT_BUFFER;
	yy_switch_to_buffer(map_buf);
	buf_ptr = buffer;
	BEGIN(defn);
}
<charmap>\n             {
	errx(EX_UNAVAILABLE, "'charmap' file name expected near line %u",
	     line_no);
}
<charmap><<EOF>>        {
	errx(EX_UNAVAILABLE, "'charmap' file name expected near line %u",
	     line_no);
}
<INITIAL,defn><<EOF>>                 {
	if(map_fp != NULL) {
		if (buf_ptr != buffer)
			errx(EX_UNAVAILABLE, "premature EOF in the map near line %u of %s", line_no, map_name);
		yy_switch_to_buffer(main_buf);
		yy_delete_buffer(map_buf);
		fclose(map_fp);
		map_fp = NULL;
		line_no = save_no;
		BEGIN(INITIAL);
	} else
		yyterminate();
}
%%
#ifdef FLEX_DEBUG
main()
{
	while(yylex())
		;
	return 0;
}
#endif /* FLEX_DEBUG */
