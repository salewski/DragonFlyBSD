/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)for.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/make/for.c,v 1.35 2005/02/10 14:39:05 harti Exp $
 * $DragonFly: src/usr.bin/make/for.c,v 1.37 2005/03/19 00:19:55 okumoto Exp $
 */

/*-
 * for.c --
 *	Functions to handle loops in a makefile.
 *
 * Interface:
 *	For_Eval 	Evaluate the loop in the passed line.
 *	For_Run		Run accumulated loop
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "dir.h"
#include "for.h"
#include "globals.h"
#include "lst.h"
#include "make.h"
#include "parse.h"
#include "util.h"
#include "var.h"

/*
 * For statements are of the form:
 *
 * .for <variable> in <varlist>
 * ...
 * .endfor
 *
 * The trick is to look for the matching end inside for for loop
 * To do that, we count the current nesting level of the for loops.
 * and the .endfor statements, accumulating all the statements between
 * the initial .for loop and the matching .endfor;
 * then we evaluate the for loop for each variable in the varlist.
 */

static int	forLevel = 0;	/* Nesting level */
static char	*forVar;	/* Iteration variable */
static Buffer	*forBuf;	/* Commands in loop */
static Lst	forLst;		/* List of items */

/*-
 *-----------------------------------------------------------------------
 * For_Eval --
 *	Evaluate the for loop in the passed line. The line
 *	looks like this:
 *	    .for <variable> in <varlist>
 *
 * Results:
 *	TRUE: We found a for loop, or we are inside a for loop
 *	FALSE: We did not find a for loop, or we found the end of the for
 *	       for loop.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
int
For_Eval(char *line)
{
	char	*ptr;
	char	*sub;
	char	*wrd;
	int	level;	/* Level at which to report errors. */

	ptr = line;
	level = PARSE_FATAL;

	if (forLevel == 0) {
		/*
		 * maybe start of a for loop
		 */
		Buffer	*buf;
		size_t	varlen;

		for (ptr++; *ptr && isspace((unsigned char)*ptr); ptr++)
			;
		/*
		 * If we are not in a for loop quickly determine if
		 * the statement is a for.
		 */
		if (ptr[0] != 'f' || ptr[1] != 'o' || ptr[2] != 'r' ||
		    !isspace((unsigned char)ptr[3]))
			return (FALSE);
		ptr += 3;

		/*
		 * we found a for loop, and now we are going to parse it.
		 */
		while (*ptr && isspace((unsigned char)*ptr))
			ptr++;

		/*
		 * Grab the variable
		 */
		buf = Buf_Init(0);
		for (wrd = ptr; *ptr && !isspace((unsigned char)*ptr); ptr++)
			;
		Buf_AppendRange(buf, wrd, ptr);

		forVar = (char *)Buf_GetAll(buf, &varlen);
		if (varlen == 0) {
			/* XXXHB Buf_Destroy(buf, TRUE) */
			Parse_Error(level, "missing variable in for");
			return (0);
		}
		Buf_Destroy(buf, FALSE);

		while (*ptr && isspace((unsigned char)*ptr))
			ptr++;

		/*
		 * Grab the `in'
		 */
		if (ptr[0] != 'i' || ptr[1] != 'n' ||
		    !isspace((unsigned char)ptr[2])) {
			/* XXXHB free(forVar) */
			Parse_Error(level, "missing `in' in for");
			printf("%s\n", ptr);
			return (0);
		}
		ptr += 3;

		while (*ptr && isspace((unsigned char)*ptr))
			ptr++;

		/*
		 * Make a list with the remaining words
		 */
		sub = Buf_Peel(Var_Subst(NULL, ptr, VAR_CMD, FALSE));
		for (ptr = sub; *ptr && isspace((unsigned char)*ptr); ptr++)
			;

		Lst_Init(&forLst);
		buf = Buf_Init(0);
		for (wrd = ptr; *ptr != '\0'; ptr++) {
			if (isspace((unsigned char)*ptr)) {
				Buf_AppendRange(buf, wrd, ptr);
				Lst_AtFront(&forLst, Buf_Peel(buf));

				buf = Buf_Init(0);
				while (*ptr && isspace((unsigned char)*ptr))
					ptr++;
				wrd = ptr--;
			}
		}
		DEBUGF(FOR, ("For: Iterator %s List %s\n", forVar, sub));
		if (ptr - wrd > 0) {
			Buf_AppendRange(buf, wrd, ptr);
			Lst_AtFront(&forLst, Buf_Peel(buf));
		} else {
			Buf_Destroy(buf, TRUE);
		}
		free(sub);

		forBuf = Buf_Init(0);
		forLevel++;
		return (1);
	}

	if (*ptr == '.') {
		/*
		 * Need to check for 'endfor' and 'for' to find the end
		 * of our loop or to find embedded for loops.
		 */
		for (ptr++; *ptr && isspace((unsigned char)*ptr); ptr++)
			;

		if (strncmp(ptr, "endfor", 6) == 0 &&
		    (isspace((unsigned char)ptr[6]) || !ptr[6])) {
			DEBUGF(FOR, ("For: end for %d\n", forLevel));
			if (--forLevel < 0) {
				Parse_Error(level, "for-less endfor");
				return (0);
			}

		} else if (strncmp(ptr, "for", 3) == 0 &&
		    isspace((unsigned char)ptr[3])) {
			forLevel++;
			DEBUGF(FOR, ("For: new loop %d\n", forLevel));
		}
	}

	if (forLevel != 0) {
		/*
		 * Still in loop - append the line
		 */
		Buf_Append(forBuf, line);
		Buf_AddByte(forBuf, (Byte)'\n');
		return (1);
	}

	return (0);
}

/*-
 *-----------------------------------------------------------------------
 * For_Run --
 *	Run the for loop, immitating the actions of an include file
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The values of the variables forLst, forVar and forBuf are freed.
 *
 *-----------------------------------------------------------------------
 */
void
For_Run(int lineno)
{
	Lst		values;	/* list of values for the variable */
	char		*var;	/* the variable's name */
	Buffer		*buf;	/* the contents of the for loop */
	const char	*val;	/* current value of loop variable */
	LstNode		*ln;
	char		*str;

	if (forVar == NULL || forBuf == NULL)
		return;

	/* copy the global variables to have them free for embedded fors */
	var = forVar;
	buf = forBuf;
	Lst_Init(&values);
	Lst_Concat(&values, &forLst, LST_CONCLINK);

	forVar = NULL;
	forBuf = NULL;

	LST_FOREACH(ln, &values) {
		val = Lst_Datum(ln);
		Var_Set(var, val, VAR_GLOBAL);

		DEBUGF(FOR, ("--- %s = %s\n", var, val));
		str = Buf_Peel(Var_Subst(var, Buf_Data(buf),
		    VAR_GLOBAL, FALSE));

		Parse_FromString(str, lineno);
		Var_Delete(var, VAR_GLOBAL);
	}

	free(var);
	Lst_Destroy(&values, free);
	Buf_Destroy(buf, TRUE);
}
