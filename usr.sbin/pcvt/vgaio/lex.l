%{
/*
 * Copyright (c) 1994 Joerg Wunsch
 *
 * All rights reserved.
 *
 * This program is free software.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Joerg Wunsch
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ident "$FreeBSD: src/usr.sbin/pcvt/vgaio/lex.l,v 1.5 1999/09/06 07:39:30 peter Exp $"
#ident "$DragonFly: src/usr.sbin/pcvt/vgaio/Attic/lex.l,v 1.2 2003/06/17 04:29:59 dillon Exp $"

/*
 * $Log: lex.l,v $
 * Revision 1.1  1994/03/29  02:47:20  mycroft
 * pcvt 3.0, with some performance enhancements by Joerg Wunsch and me.
 *
 * Revision 1.2  1994/01/08  17:42:58  j
 * cleanup
 * made multiple commands per line work
 * wrote man page
 *
 * Revision 1.3  21.12.1994 -hm
 * Added mi command for accessing the misc out register
 */

#include <stdio.h>
#include <stdlib.h>

#include "vgaio.h"
#include "y.tab.h"

extern YYSTYPE yylval;

%}

D	[0-9a-fA-F]

%%

cr|CR	{ return CR; }

ar|AR	{ return AR; }

gr|GR	{ return GR; }

sr|SR	{ return SR; }

mi|MI	{ return MI; }

{D}({D}*)  { sscanf(yytext, "%x", &yylval.num); return NUM; }

[ \t]	{ /* ignore */ }

\n	{ return NEWLINE; }

.	{ return yytext[0]; }
