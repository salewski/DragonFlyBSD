/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * @(#)var.c	8.3 (Berkeley) 3/19/94
 * $FreeBSD: src/usr.bin/make/var.c,v 1.83 2005/02/11 10:49:01 harti Exp $
 * $DragonFly: src/usr.bin/make/var.c,v 1.183 2005/03/31 20:39:44 okumoto Exp $
 */

/*-
 * var.c --
 *	Variable-handling functions
 *
 * Interface:
 *	Var_Set		Set the value of a variable in the given
 *			context. The variable is created if it doesn't
 *			yet exist. The value and variable name need not
 *			be preserved.
 *
 *	Var_Append	Append more characters to an existing variable
 *			in the given context. The variable needn't
 *			exist already -- it will be created if it doesn't.
 *			A space is placed between the old value and the
 *			new one.
 *
 *	Var_Exists	See if a variable exists.
 *
 *	Var_Value	Return the value of a variable in a context or
 *			NULL if the variable is undefined.
 *
 *	Var_Subst	Substitute named variable, or all variables if
 *			NULL in a string using
 *			the given context as the top-most one. If the
 *			third argument is non-zero, Parse_Error is
 *			called if any variables are undefined.
 *
 *	Var_Parse	Parse a variable expansion from a string and
 *			return the result and the number of characters
 *			consumed.
 *
 *	Var_Delete	Delete a variable in a context.
 *
 *	Var_Init	Initialize this module.
 *
 * Debugging:
 *	Var_Dump	Print out all variables defined in the given
 *			context.
 *
 * XXX: There's a lot of duplication in these functions.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "config.h"
#include "globals.h"
#include "GNode.h"
#include "make.h"
#include "nonints.h"
#include "parse.h"
#include "str.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/**
 *
 */
typedef struct VarParser {
	const char	*const input;	/* pointer to input string */
	const char	*ptr;		/* current parser pos in input str */
	GNode		*ctxt;
	Boolean		err;
} VarParser;
static char *VarParse(VarParser *, Boolean *);

/*
 * This is a harmless return value for Var_Parse that can be used by Var_Subst
 * to determine if there was an error in parsing -- easier than returning
 * a flag, as things outside this module don't give a hoot.
 */
char	var_Error[] = "";

/*
 * Similar to var_Error, but returned when the 'err' flag for Var_Parse is
 * set false. Why not just use a constant? Well, gcc likes to condense
 * identical string instances...
 */
static char	varNoError[] = "";

/*
 * Internally, variables are contained in four different contexts.
 *	1) the environment. They may not be changed. If an environment
 *	   variable is appended-to, the result is placed in the global
 *	   context.
 *	2) the global context. Variables set in the Makefile are located in
 *	   the global context. It is the penultimate context searched when
 *	   substituting.
 *	3) the command-line context. All variables set on the command line
 *	   are placed in this context. They are UNALTERABLE once placed here.
 *	4) the local context. Each target has associated with it a context
 *	   list. On this list are located the structures describing such
 *	   local variables as $(@) and $(*)
 * The four contexts are searched in the reverse order from which they are
 * listed.
 */
GNode	*VAR_GLOBAL;	/* variables from the makefile */
GNode	*VAR_CMD;	/* variables defined on the command-line */

#define	FIND_CMD	0x1   /* look in VAR_CMD when searching */
#define	FIND_GLOBAL	0x2   /* look in VAR_GLOBAL as well */
#define	FIND_ENV	0x4   /* look in the environment also */

#define	OPEN_PAREN		'('
#define	CLOSE_PAREN		')'
#define	OPEN_BRACE		'{'
#define	CLOSE_BRACE		'}'

/*
 * Create a Var object.
 *
 * @param name		Name of variable.
 * @param value		Value of variable.
 * @param flags		Flags set on variable.
 */
static Var *
VarCreate(const char name[], const char value[], int flags)
{
	Var *v;

	v = emalloc(sizeof(Var));
	v->name	= estrdup(name);
	v->val	= Buf_Init(0);
	v->flags	= flags;

	if (value != NULL) {
		Buf_Append(v->val, value);
	}
	return (v);
}

/*
 * Destroy a Var object.
 *
 * @param v	Object to destroy.
 * @param f     true if internal buffer in Buffer object is to be
 *		removed.
 */
static void
VarDestroy(Var *v, Boolean f)
{

	Buf_Destroy(v->val, f);
	free(v->name);
	free(v);
}

/*
 * Find a variable in a variable list.
 */
static Var *
VarLookup(Lst *vlist, const char *name)
{
	LstNode	*ln;

	LST_FOREACH(ln, vlist)
		if (strcmp(((const Var *)Lst_Datum(ln))->name, name) == 0)
			return (Lst_Datum(ln));
	return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * VarPossiblyExpand --
 *	Expand a variable name's embedded variables in the given context.
 *
 * Results:
 *	The contents of name, possibly expanded.
 *-----------------------------------------------------------------------
 */
static char *
VarPossiblyExpand(const char *name, GNode *ctxt)
{
	Buffer	*buf;

	if (strchr(name, '$') != NULL) {
		buf = Var_Subst(NULL, name, ctxt, 0);
		return (Buf_Peel(buf));
	} else {
		return estrdup(name);
	}
}

/*-
 *-----------------------------------------------------------------------
 * VarFind --
 *	Find the given variable in the given context and any other contexts
 *	indicated.
 *
 *	Flags:
 *		FIND_GLOBAL	set means look in the VAR_GLOBAL context too
 *		FIND_CMD	set means to look in the VAR_CMD context too
 *		FIND_ENV	set means to look in the environment
 *
 * Results:
 *	A pointer to the structure describing the desired variable or
 *	NULL if the variable does not exist.
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
static Var *
VarFind(const char *name, GNode *ctxt, int flags)
{
	Boolean	localCheckEnvFirst;
	LstNode	*ln;
	Var	*var;
	char	*env;

	/*
	 * If the variable name begins with a '.', it could very well be one of
	 * the local ones.  We check the name against all the local variables
	 * and substitute the short version in for 'name' if it matches one of
	 * them.
	 */
	if (name[0] == '.') {
		switch (name[1]) {
		case 'A':
			if (!strcmp(name, ".ALLSRC"))
				name = ALLSRC;
			if (!strcmp(name, ".ARCHIVE"))
				name = ARCHIVE;
			break;
		case 'I':
			if (!strcmp(name, ".IMPSRC"))
				name = IMPSRC;
			break;
		case 'M':
			if (!strcmp(name, ".MEMBER"))
				name = MEMBER;
			break;
		case 'O':
			if (!strcmp(name, ".OODATE"))
				name = OODATE;
			break;
		case 'P':
			if (!strcmp(name, ".PREFIX"))
				name = PREFIX;
			break;
		case 'T':
			if (!strcmp(name, ".TARGET"))
				name = TARGET;
			break;
		default:
			break;
		}
	}

	/*
	 * Note whether this is one of the specific variables we were told
	 * through the -E flag to use environment-variable-override for.
	 */
	localCheckEnvFirst = FALSE;
	LST_FOREACH(ln, &envFirstVars) {
		if (strcmp(Lst_Datum(ln), name) == 0) {
			localCheckEnvFirst = TRUE;
			break;
		}
	}

	/*
	 * First look for the variable in the given context. If it's not there,
	 * look for it in VAR_CMD, VAR_GLOBAL and the environment,
	 * in that order, depending on the FIND_* flags in 'flags'
	 */
	var = VarLookup(&ctxt->context, name);
	if (var != NULL) {
		/* got it */
		return (var);
	}

	/* not there - try command line context */
	if ((flags & FIND_CMD) && (ctxt != VAR_CMD)) {
		var = VarLookup(&VAR_CMD->context, name);
		if (var != NULL)
			return (var);
	}

	/* not there - try global context, but only if not -e/-E */
	if ((flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL) &&
	    !checkEnvFirst && !localCheckEnvFirst) {
		var = VarLookup(&VAR_GLOBAL->context, name);
		if (var != NULL)
			return (var);
	}

	if (!(flags & FIND_ENV))
		/* we were not told to look into the environment */
		return (NULL);

	/* look in the environment */
	if ((env = getenv(name)) != NULL) {
		/* craft this variable from the environment value */
		return (VarCreate(name, env, VAR_FROM_ENV));
	}

	/* deferred check for the environment (in case of -e/-E) */
	if ((checkEnvFirst || localCheckEnvFirst) &&
	    (flags & FIND_GLOBAL) && (ctxt != VAR_GLOBAL)) {
		var = VarLookup(&VAR_GLOBAL->context, name);
		if (var != NULL)
			return (var);
	}
	return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * VarAdd  --
 *	Add a new variable of name name and value val to the given context.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The new variable is placed at the front of the given context
 *	The name and val arguments are duplicated so they may
 *	safely be freed.
 *-----------------------------------------------------------------------
 */
static void
VarAdd(const char *name, const char *val, GNode *ctxt)
{

	Lst_AtFront(&ctxt->context, VarCreate(name, val, 0));
	DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, name, val));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Delete --
 *	Remove a variable from a context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The Var structure is removed and freed.
 *
 *-----------------------------------------------------------------------
 */
void
Var_Delete(const char *name, GNode *ctxt)
{
	LstNode *ln;

	DEBUGF(VAR, ("%s:delete %s\n", ctxt->name, name));
	LST_FOREACH(ln, &ctxt->context) {
		if (strcmp(((const Var *)Lst_Datum(ln))->name, name) == 0) {
			VarDestroy(Lst_Datum(ln), TRUE);
			Lst_Remove(&ctxt->context, ln);
			break;
		}
	}
}

/*-
 *-----------------------------------------------------------------------
 * Var_Set --
 *	Set the variable name to the value val in the given context.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If the variable doesn't yet exist, a new record is created for it.
 *	Else the old value is freed and the new one stuck in its place
 *
 * Notes:
 *	The variable is searched for only in its context before being
 *	created in that context. I.e. if the context is VAR_GLOBAL,
 *	only VAR_GLOBAL->context is searched. Likewise if it is VAR_CMD, only
 *	VAR_CMD->context is searched. This is done to avoid the literally
 *	thousands of unnecessary strcmp's that used to be done to
 *	set, say, $(@) or $(<).
 *-----------------------------------------------------------------------
 */
void
Var_Set(const char *name, const char *val, GNode *ctxt)
{
	Var    *v;
	char   *n;

	/*
	 * We only look for a variable in the given context since anything
	 * set here will override anything in a lower context, so there's not
	 * much point in searching them all just to save a bit of memory...
	 */
	n = VarPossiblyExpand(name, ctxt);
	v = VarFind(n, ctxt, 0);
	if (v == NULL) {
		VarAdd(n, val, ctxt);
	} else {
		Buf_Clear(v->val);
		Buf_Append(v->val, val);

		DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, val));
	}
	/*
	 * Any variables given on the command line are automatically exported
	 * to the environment (as per POSIX standard)
	 */
	if (ctxt == VAR_CMD || (v != NULL && (v->flags & VAR_TO_ENV))) {
		setenv(n, val, 1);
	}
	free(n);
}

/*
 * Var_SetEnv --
 *	Set the VAR_TO_ENV flag on a variable
 */
void
Var_SetEnv(const char *name, GNode *ctxt)
{
	Var    *v;

	v = VarFind(name, ctxt, FIND_CMD | FIND_GLOBAL | FIND_ENV);
	if (v == NULL) {
		Error("Cannot set environment flag on non-existant variable %s", name);
	} else {
		if ((v->flags & VAR_TO_ENV) == 0) {
			v->flags |= VAR_TO_ENV;
			setenv(v->name, Buf_Data(v->val), 1);
		}
	}
}

/*-
 *-----------------------------------------------------------------------
 * Var_Append --
 *	The variable of the given name has the given value appended to it in
 *	the given context.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	If the variable doesn't exist, it is created. Else the strings
 *	are concatenated (with a space in between).
 *
 * Notes:
 *	Only if the variable is being sought in the global context is the
 *	environment searched.
 *	XXX: Knows its calling circumstances in that if called with ctxt
 *	an actual target, it will only search that context since only
 *	a local variable could be being appended to. This is actually
 *	a big win and must be tolerated.
 *-----------------------------------------------------------------------
 */
void
Var_Append(const char *name, const char *val, GNode *ctxt)
{
	Var	*v;
	char	*n;

	n = VarPossiblyExpand(name, ctxt);
	v = VarFind(n, ctxt, (ctxt == VAR_GLOBAL) ? FIND_ENV : 0);
	if (v == NULL) {
		VarAdd(n, val, ctxt);

	} else if (v->flags & VAR_FROM_ENV) {
		Buf_AddByte(v->val, (Byte)' ');
		Buf_Append(v->val, val);
		DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, Buf_Data(v->val)));
		/*
		 * If the original variable came from the
		 * environment, we have to install it in the global
		 * context (we could place it in the environment, but
		 * then we should provide a way to export other
		 * variables...)
		 */
		v->flags &= ~VAR_FROM_ENV;
		Lst_AtFront(&ctxt->context, v);

	} else {
		Buf_AddByte(v->val, (Byte)' ');
		Buf_Append(v->val, val);
		DEBUGF(VAR, ("%s:%s = %s\n", ctxt->name, n, Buf_Data(v->val)));
	}
	free(n);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Exists --
 *	See if the given variable exists.
 *
 * Results:
 *	TRUE if it does, FALSE if it doesn't
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
Boolean
Var_Exists(const char *name, GNode *ctxt)
{
	Var	*v;
	char	*n;

	n = VarPossiblyExpand(name, ctxt);
	v = VarFind(n, ctxt, FIND_CMD | FIND_GLOBAL | FIND_ENV);
	if (v == NULL) {
		free(n);
		return (FALSE);
	} else if (v->flags & VAR_FROM_ENV) {
		VarDestroy(v, TRUE);
		free(n);
		return (TRUE);
	} else {
		free(n);
		return (TRUE);
	}
}

/*-
 *-----------------------------------------------------------------------
 * Var_Value --
 *	Return the value of the named variable in the given context
 *
 * Results:
 *	The value if the variable exists, NULL if it doesn't
 *
 * Side Effects:
 *	None
 *-----------------------------------------------------------------------
 */
char *
Var_Value(const char *name, GNode *ctxt, char **frp)
{
	Var	*v;
	char	*n;
	char	*p;

	n = VarPossiblyExpand(name, ctxt);
	v = VarFind(n, ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v == NULL) {
		p = NULL;
		*frp = NULL;
	} else if (v->flags & VAR_FROM_ENV) {
		p = Buf_Data(v->val);
		*frp = p;
		VarDestroy(v, FALSE);
	} else {
		p = Buf_Data(v->val);
		*frp = NULL;
	}
	free(n);
	return (p);
}

/*-
 *-----------------------------------------------------------------------
 * VarModify --
 *	Modify each of the words of the passed string using the given
 *	function. Used to implement all modifiers.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *
 * Side Effects:
 *	Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 *
 *-----------------------------------------------------------------------
 */
static char *
VarModify(const char *str, VarModifyProc *modProc, void *datum)
{
	char	**av;		/* word list [first word does not count] */
	int	ac;
	Buffer	*buf;		/* Buffer for the new string */
	Boolean	addSpace;	/* TRUE if need to add a space to the buffer
				 * before adding the trimmed word */
	int	i;

	av = brk_string(str, &ac, FALSE);

	buf = Buf_Init(0);

	addSpace = FALSE;
	for (i = 1; i < ac; i++)
		addSpace = (*modProc)(av[i], addSpace, buf, datum);

	return (Buf_Peel(buf));
}

/*-
 *-----------------------------------------------------------------------
 * VarSortWords --
 *	Sort the words in the string.
 *
 * Input:
 *	str		String whose words should be sorted
 *	cmp		A comparison function to control the ordering
 *
 * Results:
 *	A string containing the words sorted
 *
 * Side Effects:
 *	Uses brk_string() so it invalidates any previous call to
 *	brk_string().
 *
 *-----------------------------------------------------------------------
 */
static char *
VarSortWords(const char *str, int (*cmp)(const void *, const void *))
{
	char	**av;
	int	ac;
	Buffer	*buf;
	int	i;

	av = brk_string(str, &ac, FALSE);
	qsort(av + 1, ac - 1, sizeof(char *), cmp);

	buf = Buf_Init(0);
	for (i = 1; i < ac; i++) {
		Buf_Append(buf, av[i]);
		Buf_AddByte(buf, (Byte)((i < ac - 1) ? ' ' : '\0'));
	}

	return (Buf_Peel(buf));
}

static int
SortIncreasing(const void *l, const void *r)
{

	return (strcmp(*(const char* const*)l, *(const char* const*)r));
}

/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	Pass through the tstr looking for 1) escaped delimiters,
 *	'$'s and backslashes (place the escaped character in
 *	uninterpreted) and 2) unescaped $'s that aren't before
 *	the delimiter (expand the variable substitution).
 *	Return the expanded string or NULL if the delimiter was missing
 *	If pattern is specified, handle escaped ampersands, and replace
 *	unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *	If flags is specified and the last character of the pattern is a
 *	$ set the VAR_MATCH_END bit of flags.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
static Buffer *
VarGetPattern(VarParser *vp, int delim, int *flags, VarPattern *patt)
{
	Buffer		*buf;

	buf = Buf_Init(0);

	/*
	 * Skim through until the matching delimiter is found; pick up
	 * variable substitutions on the way. Also allow backslashes to quote
	 * the delimiter, $, and \, but don't touch other backslashes.
	 */
	while (*vp->ptr != '\0') {
		if (*vp->ptr == delim) {
			return (buf);

		} else if ((vp->ptr[0] == '\\') &&
		    ((vp->ptr[1] == delim) ||
		     (vp->ptr[1] == '\\') ||
		     (vp->ptr[1] == '$') ||
		     (vp->ptr[1] == '&' && patt != NULL))) {
			vp->ptr++;		/* consume backslash */
			Buf_AddByte(buf, (Byte)vp->ptr[0]);
			vp->ptr++;

		} else if (vp->ptr[0] == '$') {
			if (vp->ptr[1] == delim) {
				if (flags == NULL) {
					Buf_AddByte(buf, (Byte)vp->ptr[0]);
					vp->ptr++;
				} else {
					/*
					 * Unescaped $ at end of patt =>
					 * anchor patt at end.
					 */
					*flags |= VAR_MATCH_END;
					vp->ptr++;
				}
			} else {
				VarParser	subvp = {
					vp->ptr,
					vp->ptr,
					vp->ctxt,
					vp->err
				};
				char   *rval;
				Boolean rfree;

				/*
				 * If unescaped dollar sign not
				 * before the delimiter, assume it's
				 * a variable substitution and
				 * recurse.
				 */
				rval = VarParse(&subvp, &rfree);
				Buf_Append(buf, rval);
				if (rfree)
					free(rval);
				vp->ptr = subvp.ptr;
			}
		} else if (vp->ptr[0] == '&' && patt != NULL) {
			Buf_AppendBuf(buf, patt->lhs);
			vp->ptr++;
		} else {
			Buf_AddByte(buf, (Byte)vp->ptr[0]);
			vp->ptr++;
		}
	}

	Buf_Destroy(buf, TRUE);
	return (NULL);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Quote --
 *	Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_Quote(const char *str)
{
	Buffer *buf;
	/* This should cover most shells :-( */
	static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";

	buf = Buf_Init(MAKE_BSIZE);
	for (; *str; str++) {
		if (strchr(meta, *str) != NULL)
			Buf_AddByte(buf, (Byte)'\\');
		Buf_AddByte(buf, (Byte)*str);
	}

	return (Buf_Peel(buf));
}

/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	An error gets printed.
 *
 *-----------------------------------------------------------------------
 */
void
VarREError(int err, regex_t *pat, const char *str)
{
	char   *errbuf;
	int     errlen;

	errlen = regerror(err, pat, 0, 0);
	errbuf = emalloc(errlen);
	regerror(err, pat, errbuf, errlen);
	Error("%s: %s", str, errbuf);
	free(errbuf);
}

/**
 * Make sure this variable is fully expanded.
 */
static char *
VarExpand(Var *v, VarParser *vp)
{
	char	*value;
	char	*result;

	if (v->flags & VAR_IN_USE) {
		Fatal("Variable %s is recursive.", v->name);
		/* NOTREACHED */
	}

	v->flags |= VAR_IN_USE;

	/*
	 * Before doing any modification, we have to make sure the
	 * value has been fully expanded. If it looks like recursion
	 * might be necessary (there's a dollar sign somewhere in the
	 * variable's value) we just call Var_Subst to do any other
	 * substitutions that are necessary. Note that the value
	 * returned by Var_Subst will have been
	 * dynamically-allocated, so it will need freeing when we
	 * return.
	 */
	value = Buf_Data(v->val);
	if (strchr(value, '$') == NULL) {
		result = strdup(value);
	} else {
		Buffer	*buf;

		buf = Var_Subst(NULL, value, vp->ctxt, vp->err);
		result = Buf_Peel(buf);
	}

	v->flags &= ~VAR_IN_USE;

	return (result);
}

/**
 * Select only those words in value that match the modifier.
 */
static char *
modifier_M(VarParser *vp, const char value[], char endc)
{
	char	*patt;
	char	*ptr;
	char	*newValue;
	char	modifier;

	modifier = vp->ptr[0];
	vp->ptr++;	/* consume 'M' or 'N' */

	/*
	 * Compress the \:'s out of the pattern, so allocate enough
	 * room to hold the uncompressed pattern and compress the
	 * pattern into that space.
	 */
	patt = estrdup(vp->ptr);
	ptr = patt;
	while (vp->ptr[0] != '\0') {
		if (vp->ptr[0] == endc || vp->ptr[0] == ':') {
			break;
		}
		if (vp->ptr[0] == '\\' &&
		    (vp->ptr[1] == endc || vp->ptr[1] == ':')) {
			vp->ptr++;	/* consume backslash */
		}
		*ptr = vp->ptr[0];
		ptr++;
		vp->ptr++;
	}
	*ptr = '\0';

	if (modifier == 'M') {
		newValue = VarModify(value, VarMatch, patt);
	} else {
		newValue = VarModify(value, VarNoMatch, patt);
	}
	free(patt);

	return (newValue);
}

/**
 * Substitute the replacement string for the pattern.  The substitution
 * is applied to each word in value.
 */
static char *
modifier_S(VarParser *vp, const char value[], Var *v)
{
	VarPattern	patt;
	char		delim;
	char		*newValue;

	patt.flags = 0;

	vp->ptr++;		/* consume 'S' */

	delim = *vp->ptr;	/* used to find end of pattern */
	vp->ptr++;		/* consume 1st delim */

	/*
	 * If pattern begins with '^', it is anchored to the start of the
	 * word -- skip over it and flag pattern.
	 */
	if (*vp->ptr == '^') {
		patt.flags |= VAR_MATCH_START;
		vp->ptr++;
	}

	patt.lhs = VarGetPattern(vp, delim, &patt.flags, NULL);
	if (patt.lhs == NULL) {
		/*
		 * LHS didn't end with the delim, complain and exit.
		 */
		Fatal("Unclosed substitution for %s (%c missing)",
		    v->name, delim);
	}

	vp->ptr++;	/* consume 2nd delim */

	patt.rhs = VarGetPattern(vp, delim, NULL, &patt);
	if (patt.rhs == NULL) {
		/*
		 * RHS didn't end with the delim, complain and exit.
		 */
		Fatal("Unclosed substitution for %s (%c missing)",
		    v->name, delim);
	}

	vp->ptr++;	/* consume last delim */

	/*
	 * Check for global substitution. If 'g' after the final delimiter,
	 * substitution is global and is marked that way.
	 */
	if (vp->ptr[0] == 'g') {
		patt.flags |= VAR_SUB_GLOBAL;
		vp->ptr++;
	}

	/*
	 * Global substitution of the empty string causes an infinite number
	 * of matches, unless anchored by '^' (start of string) or '$' (end
	 * of string). Catch the infinite substitution here. Note that flags
	 * can only contain the 3 bits we're interested in so we don't have
	 * to mask unrelated bits. We can test for equality.
	 */
	if (Buf_Size(patt.lhs) == 0 && patt.flags == VAR_SUB_GLOBAL)
		Fatal("Global substitution of the empty string");

	newValue = VarModify(value, VarSubstitute, &patt);

	/*
	 * Free the two strings.
	 */
	free(patt.lhs);
	free(patt.rhs);

	return (newValue);
}

static char *
modifier_C(VarParser *vp, char value[], Var *v)
{
	VarPattern	patt;
	char		delim;
	int		error;
	char		*newValue;

	patt.flags = 0;

	vp->ptr++;		/* consume 'C' */

	delim = *vp->ptr;	/* delimiter between sections */

	vp->ptr++;		/* consume 1st delim */

	patt.lhs = VarGetPattern(vp, delim, NULL, NULL);
	if (patt.lhs == NULL) {
		Fatal("Unclosed substitution for %s (%c missing)",
		     v->name, delim);
	}

	vp->ptr++;		/* consume 2st delim */

	patt.rhs = VarGetPattern(vp, delim, NULL, NULL);
	if (patt.rhs == NULL) {
		Fatal("Unclosed substitution for %s (%c missing)",
		     v->name, delim);
	}

	vp->ptr++;		/* consume last delim */

	switch (*vp->ptr) {
	case 'g':
		patt.flags |= VAR_SUB_GLOBAL;
		vp->ptr++;		/* consume 'g' */
		break;
	case '1':
		patt.flags |= VAR_SUB_ONE;
		vp->ptr++;		/* consume '1' */
		break;
	default:
		break;
	}

	error = regcomp(&patt.re, Buf_Data(patt.lhs), REG_EXTENDED);
	if (error) {
		VarREError(error, &patt.re, "RE substitution error");
		free(patt.rhs);
		free(patt.lhs);
		return (var_Error);
	}

	patt.nsub = patt.re.re_nsub + 1;
	if (patt.nsub < 1)
		patt.nsub = 1;
	if (patt.nsub > 10)
		patt.nsub = 10;
	patt.matches = emalloc(patt.nsub * sizeof(regmatch_t));

	newValue = VarModify(value, VarRESubstitute, &patt);

	regfree(&patt.re);
	free(patt.matches);
	free(patt.rhs);
	free(patt.lhs);

	return (newValue);
}

static char *
sysVvarsub(VarParser *vp, char startc, Var *v, const char value[])
{
#ifdef SYSVVARSUB
	/*
	 * This can either be a bogus modifier or a System-V substitution
	 * command.
	 */
	char		endc;
	VarPattern	patt;
	Boolean		eqFound;
	int		cnt;
	char		*newStr;
	const char	*cp;

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	patt.flags = 0;

	/*
	 * First we make a pass through the string trying to verify it is a
	 * SYSV-make-style translation: it must be: <string1>=<string2>)
	 */
	eqFound = FALSE;
	cp = vp->ptr;
	cnt = 1;
	while (*cp != '\0' && cnt) {
		if (*cp == '=') {
			eqFound = TRUE;
			/* continue looking for endc */
		} else if (*cp == endc)
			cnt--;
		else if (*cp == startc)
			cnt++;
		if (cnt)
			cp++;
	}

	if (*cp == endc && eqFound) {
		/*
		 * Now we break this sucker into the lhs and rhs.
		 */
		patt.lhs = VarGetPattern(vp, '=', &patt.flags, NULL);
		if (patt.lhs == NULL) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, '=');
		}
		vp->ptr++;	/* consume '=' */

		patt.rhs = VarGetPattern(vp, endc, NULL, &patt);
		if (patt.rhs == NULL) {
			Fatal("Unclosed substitution for %s (%c missing)",
			      v->name, endc);
		}

		/*
		 * SYSV modifications happen through the whole string. Note
		 * the pattern is anchored at the end.
		 */
		newStr = VarModify(value, VarSYSVMatch, &patt);

		free(patt.lhs);
		free(patt.rhs);
	} else
#endif
	{
		Error("Unknown modifier '%c'\n", *vp->ptr);
		vp->ptr++;
		while (*vp->ptr != '\0') {
			if (*vp->ptr == endc && *vp->ptr == ':') {
				break;
			}
			vp->ptr++;
		}
		newStr = var_Error;
	}

	return (newStr);
}

/*
 * Now we need to apply any modifiers the user wants applied.
 * These are:
 *	:M<pattern>
 *		words which match the given <pattern>.
 *		<pattern> is of the standard file
 *		wildcarding form.
 *	:S<d><pat1><d><pat2><d>[g]
 *		Substitute <pat2> for <pat1> in the value
 *	:C<d><pat1><d><pat2><d>[g]
 *		Substitute <pat2> for regex <pat1> in the value
 *	:H	Substitute the head of each word
 *	:T	Substitute the tail of each word
 *	:E	Substitute the extension (minus '.') of
 *		each word
 *	:R	Substitute the root of each word
 *		(pathname minus the suffix).
 *	:lhs=rhs
 *		Like :S, but the rhs goes to the end of
 *		the invocation.
 *	:U	Converts variable to upper-case.
 *	:L	Converts variable to lower-case.
 *
 * XXXHB update this comment or remove it and point to the man page.
 */
static char *
ParseModifier(VarParser *vp, char startc, Var *v, Boolean *freeResult)
{
	char	*value;
	char	endc;

	value = VarExpand(v, vp);
	*freeResult = TRUE;

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	vp->ptr++;	/* consume first colon */

	while (*vp->ptr != '\0') {
		char	*newStr;	/* New value to return */

		if (*vp->ptr == endc) {
			return (value);
		}

		DEBUGF(VAR, ("Applying :%c to \"%s\"\n", *vp->ptr, value));
		switch (*vp->ptr) {
		case 'N':
		case 'M':
			newStr = modifier_M(vp, value, endc);
			break;
		case 'S':
			newStr = modifier_S(vp, value, v);
			break;
		case 'C':
			newStr = modifier_C(vp, value, v);
			break;
		default:
			if (vp->ptr[1] != endc && vp->ptr[1] != ':') {
#ifdef SUNSHCMD
				if ((vp->ptr[0] == 's') &&
				    (vp->ptr[1] == 'h') &&
				    (vp->ptr[2] == endc || vp->ptr[2] == ':')) {
					const char	*error;
					Buffer		*buf;

					buf = Cmd_Exec(value, &error);
					newStr = Buf_Peel(buf);

					if (error)
						Error(error, value);
					vp->ptr += 2;
				} else
#endif
				{
					newStr = sysVvarsub(vp, startc, v, value);
				}
				break;
			}

			switch (vp->ptr[0]) {
			case 'L':
				{
				const char	*cp;
				Buffer		*buf;
				buf = Buf_Init(MAKE_BSIZE);
				for (cp = value; *cp; cp++)
					Buf_AddByte(buf, (Byte)tolower(*cp));

				newStr = Buf_Peel(buf);

				vp->ptr++;
				break;
				}
			case 'O':
				newStr = VarSortWords(value, SortIncreasing);
				vp->ptr++;
				break;
			case 'Q':
				newStr = Var_Quote(value);
				vp->ptr++;
				break;
			case 'T':
				newStr = VarModify(value, VarTail, NULL);
				vp->ptr++;
				break;
			case 'U':
				{
				const char	*cp;
				Buffer		*buf;
				buf = Buf_Init(MAKE_BSIZE);
				for (cp = value; *cp; cp++)
					Buf_AddByte(buf, (Byte)toupper(*cp));

				newStr = Buf_Peel(buf);

				vp->ptr++;
				break;
				}
			case 'H':
				newStr = VarModify(value, VarHead, NULL);
				vp->ptr++;
				break;
			case 'E':
				newStr = VarModify(value, VarSuffix, NULL);
				vp->ptr++;
				break;
			case 'R':
				newStr = VarModify(value, VarRoot, NULL);
				vp->ptr++;
				break;
			default:
				newStr = sysVvarsub(vp, startc, v, value);
				break;
			}
			break;
		}

		DEBUGF(VAR, ("Result is \"%s\"\n", newStr));
		if (*freeResult) {
			free(value);
		}

		value = newStr;
		*freeResult = (value == var_Error) ? FALSE : TRUE;

		if (vp->ptr[0] == ':') {
			vp->ptr++;	/* consume colon */
		}
	}

	return (value);
}

static char *
ParseRestModifier(VarParser *vp, char startc, Buffer *buf, Boolean *freeResult)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	char		*value;

	vname = Buf_GetAll(buf, &vlen);

	v = VarFind(vname, vp->ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		value = ParseModifier(vp, startc, v, freeResult);

		if (v->flags & VAR_FROM_ENV) {
			VarDestroy(v, TRUE);
		}
		return (value);
	}

	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {
		size_t  consumed;
		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for the
		 * modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		value = ParseModifier(vp, startc, v, freeResult);
		if (*freeResult) {
			free(value);
		}
		VarDestroy(v, TRUE);

		consumed = vp->ptr - vp->input + 1;
		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set when
		 * dynamic sources are expanded.
		 */
		if (vlen == 1 ||
		    (vlen == 2 && (vname[1] == 'F' || vname[1] == 'D'))) {
			if (strchr("!%*@", vname[0]) != NULL) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
		if (vlen > 2 &&
		    vname[0] == '.' &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}

		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if (vlen == 2 &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			name[0] = vname[0];
			name[1] = '\0';

			v = VarFind(name, vp->ctxt, 0);
			if (v != NULL) {
				value = ParseModifier(vp, startc, v, freeResult);
				return (value);
			}
		}

		/*
		 * Still need to get to the end of the variable
		 * specification, so kludge up a Var structure for the
		 * modifications
		 */
		v = VarCreate(vname, NULL, VAR_JUNK);
		value = ParseModifier(vp, startc, v, freeResult);
		if (*freeResult) {
			free(value);
		}
		VarDestroy(v, TRUE);

		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);
	}
}

static char *
ParseRestEnd(VarParser *vp, Buffer *buf, Boolean *freeResult)
{
	const char	*vname;
	size_t		vlen;
	Var		*v;
	char		*value;

	vname = Buf_GetAll(buf, &vlen);

	v = VarFind(vname, vp->ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		value = VarExpand(v, vp);

		if (v->flags & VAR_FROM_ENV) {
			VarDestroy(v, TRUE);
		}

		*freeResult = TRUE;
		return (value);
	}

	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {
		size_t	consumed = vp->ptr - vp->input + 1;

		/*
		 * If substituting a local variable in a non-local context,
		 * assume it's for dynamic source stuff. We have to handle
		 * this specially and return the longhand for the variable
		 * with the dollar sign escaped so it makes it back to the
		 * caller. Only four of the local variables are treated
		 * specially as they are the only four that will be set when
		 * dynamic sources are expanded.
		 */
		if (vlen == 1 ||
		    (vlen == 2 && (vname[1] == 'F' || vname[1] == 'D'))) {
			if (strchr("!%*@", vname[0]) != NULL) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
		if (vlen > 2 &&
		    vname[0] == '.' &&
		    isupper((unsigned char)vname[1])) {
			if ((strncmp(vname, ".TARGET", vlen - 1) == 0) ||
			    (strncmp(vname, ".ARCHIVE", vlen - 1) == 0) ||
			    (strncmp(vname, ".PREFIX", vlen - 1) == 0) ||
			    (strncmp(vname, ".MEMBER", vlen - 1) == 0)) {
				value = emalloc(consumed + 1);
				strncpy(value, vp->input, consumed);
				value[consumed] = '\0';

				*freeResult = TRUE;
				return (value);
			}
		}
	} else {
		/*
		 * Check for D and F forms of local variables since we're in
		 * a local context and the name is the right length.
		 */
		if (vlen == 2 &&
		    (vname[1] == 'F' || vname[1] == 'D') &&
		    (strchr("!%*<>@", vname[0]) != NULL)) {
			char	name[2];

			name[0] = vname[0];
			name[1] = '\0';

			v = VarFind(name, vp->ctxt, 0);
			if (v != NULL) {
				char	*val;
				/*
				 * No need for nested expansion or anything,
				 * as we're the only one who sets these
				 * things and we sure don't put nested
				 * invocations in them...
				 */
				val = Buf_Data(v->val);

				if (vname[1] == 'D') {
					val = VarModify(val, VarHead, NULL);
				} else {
					val = VarModify(val, VarTail, NULL);
				}

				*freeResult = TRUE;
				return (val);
			}
		}
	}

	*freeResult = FALSE;
	return (vp->err ? var_Error : varNoError);
}

/**
 * Parse a multi letter variable name, and return it's value.
 */
static char *
VarParseLong(VarParser *vp, Boolean *freeResult)
{
	Buffer		*buf;
	char		startc;
	char		endc;
	char		*value;

	buf = Buf_Init(MAKE_BSIZE);

	startc = vp->ptr[0];
	vp->ptr++;		/* consume opening paren or brace */

	endc = (startc == OPEN_PAREN) ? CLOSE_PAREN : CLOSE_BRACE;

	/*
	 * Process characters until we reach an end character or a colon,
	 * replacing embedded variables as we go.
	 */
	while (*vp->ptr != '\0') {
		if (*vp->ptr == endc) {
			value = ParseRestEnd(vp, buf, freeResult);
			vp->ptr++;	/* consume closing paren or brace */
			Buf_Destroy(buf, TRUE);
			return (value);

		} else if (*vp->ptr == ':') {
			value = ParseRestModifier(vp, startc, buf, freeResult);
			vp->ptr++;	/* consume closing paren or brace */
			Buf_Destroy(buf, TRUE);
			return (value);

		} else if (*vp->ptr == '$') {
			VarParser	subvp = {
				vp->ptr,
				vp->ptr,
				vp->ctxt,
				vp->err
			};
			char	*rval;
			Boolean	rfree;

			rval = VarParse(&subvp, &rfree);
			if (rval == var_Error) {
				Fatal("Error expanding embedded variable.");
			}
			Buf_Append(buf, rval);
			if (rfree)
				free(rval);
			vp->ptr = subvp.ptr;
		} else {
			Buf_AddByte(buf, (Byte)*vp->ptr);
			vp->ptr++;
		}
	}

	/* If we did not find the end character, return var_Error */
	Buf_Destroy(buf, TRUE);
	*freeResult = FALSE;
	return (var_Error);
}

/**
 * Parse a single letter variable name, and return it's value.
 */
static char *
VarParseShort(VarParser *vp, Boolean *freeResult)
{
	char	vname[2];
	Var	*v;
	char	*value;

	vname[0] = vp->ptr[0];
	vname[1] = '\0';

	vp->ptr++;	/* consume single letter */

	v = VarFind(vname, vp->ctxt, FIND_ENV | FIND_GLOBAL | FIND_CMD);
	if (v != NULL) {
		value = VarExpand(v, vp);

		if (v->flags & VAR_FROM_ENV) {
			VarDestroy(v, TRUE);
		}

		*freeResult = TRUE;
		return (value);
	}

	/*
	 * If substituting a local variable in a non-local context, assume
	 * it's for dynamic source stuff. We have to handle this specially
	 * and return the longhand for the variable with the dollar sign
	 * escaped so it makes it back to the caller. Only four of the local
	 * variables are treated specially as they are the only four that
	 * will be set when dynamic sources are expanded.
	 */
	if ((vp->ctxt == VAR_CMD) || (vp->ctxt == VAR_GLOBAL)) {

		/* XXX: It looks like $% and $! are reversed here */
		switch (vname[0]) {
		case '@':
			*freeResult = TRUE;
			return (estrdup("$(.TARGET)"));
		case '%':
			*freeResult = TRUE;
			return (estrdup("$(.ARCHIVE)"));
		case '*':
			*freeResult = TRUE;
			return (estrdup("$(.PREFIX)"));
		case '!':
			*freeResult = TRUE;
			return (estrdup("$(.MEMBER)"));
		default:
			*freeResult = FALSE;
			return (vp->err ? var_Error : varNoError);
		}
	}

	/* Variable name was not found. */
	*freeResult = FALSE;
	return (vp->err ? var_Error : varNoError);
}

static char *
VarParse(VarParser *vp, Boolean *freeResult)
{

	vp->ptr++;	/* consume '$' or last letter of conditional */

	if (vp->ptr[0] == '\0') {
		/* Error, there is only a dollar sign in the input string. */
		*freeResult = FALSE;
		return (vp->err ? var_Error : varNoError);

	} else if (vp->ptr[0] == OPEN_PAREN || vp->ptr[0] == OPEN_BRACE) {
		/* multi letter variable name */
		return (VarParseLong(vp, freeResult));

	} else {
		/* single letter variable name */
		return (VarParseShort(vp, freeResult));
	}
}

/*-
 *-----------------------------------------------------------------------
 * Var_Parse --
 *	Given the start of a variable invocation, extract the variable
 *	name and find its value, then modify it according to the
 *	specification.
 *
 * Results:
 *	The value of the variable or var_Error if the specification
 *	is invalid.  The number of characters in the specification
 *	is placed in the variable pointed to by consumed.  (for
 *	invalid specifications, this is just 2 to skip the '$' and
 *	the following letter, or 1 if '$' was the last character
 *	in the string).  A Boolean in *freeResult telling whether the
 *	returned string should be freed by the caller.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
char *
Var_Parse(const char input[], GNode *ctxt, Boolean err,
	size_t *consumed, Boolean *freeResult)
{
	VarParser	vp = {
		input,
		input,
		ctxt,
		err
	};
	char		*value;

	value = VarParse(&vp, freeResult);
	*consumed += vp.ptr - vp.input;
	return (value);
}

/*-
 *-----------------------------------------------------------------------
 * Var_Subst  --
 *	Substitute for all variables in the given string in the given context
 *	If err is TRUE, Parse_Error will be called when an undefined
 *	variable is encountered.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None. The old string must be freed by the caller
 *-----------------------------------------------------------------------
 */
Buffer *
Var_Subst(const char *var, const char *str, GNode *ctxt, Boolean err)
{
	Boolean errorReported;
	Buffer *buf;		/* Buffer for forming things */

	/*
	 * Set TRUE if an error has already been reported to prevent a
	 * plethora of messages when recursing. XXXHB this comment sounds
	 * wrong.
	 */
	errorReported = FALSE;

	buf = Buf_Init(0);
	while (*str) {
		if (var == NULL && (str[0] == '$') && (str[1] == '$')) {
			/*
			 * A dollar sign may be escaped either with another
			 * dollar sign. In such a case, we skip over the
			 * escape character and store the dollar sign into
			 * the buffer directly.
			 */
			Buf_AddByte(buf, (Byte)str[0]);
			str += 2;

		} else if (str[0] == '$') {
			/*
			 * Variable invocation.
			 */
			if (var != NULL) {
				int     expand;
				for (;;) {
					if (str[1] == OPEN_PAREN || str[1] == OPEN_BRACE) {
						size_t  ln;
						const char *p = str + 2;

						/*
						 * Scan up to the end of the
						 * variable name.
						 */
						while (*p != '\0' &&
						       *p != ':' &&
						       *p != CLOSE_PAREN &&
						       *p != CLOSE_BRACE &&
						       *p != '$') {
							++p;
						}

						/*
						 * A variable inside the
						 * variable. We cannot expand
						 * the external variable yet,
						 * so we try again with the
						 * nested one
						 */
						if (*p == '$') {
							Buf_AppendRange(buf, str, p);
							str = p;
							continue;
						}
						ln = p - (str + 2);
						if (var[ln] == '\0' && strncmp(var, str + 2, ln) == 0) {
							expand = TRUE;
						} else {
							/*
							 * Not the variable
							 * we want to expand,
							 * scan until the
							 * next variable
							 */
							while (*p != '$' && *p != '\0')
								p++;

							Buf_AppendRange(buf, str, p);
							str = p;
							expand = FALSE;
						}
					} else {
						/*
						 * Single letter variable
						 * name
						 */
						if (var[1] == '\0' && var[0] == str[1]) {
							expand = TRUE;
						} else {
							Buf_AddBytes(buf, 2, (const Byte *) str);
							str += 2;
							expand = FALSE;
						}
					}
					break;
				}
				if (!expand)
					continue;
			}
		    {
			VarParser	subvp = {
				str,
				str,
				ctxt,
				err
			};
			char	*rval;
			Boolean	rfree;

			rval = VarParse(&subvp, &rfree);

			/*
			 * When we come down here, val should either point to
			 * the value of this variable, suitably modified, or
			 * be NULL. Length should be the total length of the
			 * potential variable invocation (from $ to end
			 * character...)
			 */
			if (rval == var_Error || rval == varNoError) {
				/*
				 * If performing old-time variable
				 * substitution, skip over the variable and
				 * continue with the substitution. Otherwise,
				 * store the dollar sign and advance str so
				 * we continue with the string...
				 */
				if (oldVars) {
					str = subvp.ptr;
				} else if (err) {
					/*
					 * If variable is undefined, complain
					 * and skip the variable. The
					 * complaint will stop us from doing
					 * anything when the file is parsed.
					 */
					if (!errorReported) {
						Parse_Error(PARSE_FATAL,
							    "Undefined variable \"%.*s\"", subvp.ptr - subvp.input, str);
					}
					str = subvp.ptr;
					errorReported = TRUE;
				} else {
					Buf_AddByte(buf, (Byte)*str);
					str += 1;
				}
			} else {
				/*
				 * We've now got a variable structure to
				 * store in. But first, advance the string
				 * pointer.
				 */
				str = subvp.ptr;

				/*
				 * Copy all the characters from the variable
				 * value straight into the new string.
				 */
				Buf_Append(buf, rval);
				if (rfree) {
					free(rval);
				}
			}
		    }
		} else {
			/*
			 * Skip as many characters as possible -- either to
			 * the end of the string or to the next dollar sign
			 * (variable invocation).
			 */
			const char *cp = str;

			do {
				str++;
			} while (str[0] != '$' && str[0] != '\0');

			Buf_AppendRange(buf, cp, str);
		}
	}

	return (buf);
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetTail --
 *	Return the tail from each of a list of words. Used to set the
 *	System V local variables.
 *
 * Results:
 *	The resulting string.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetTail(char *file)
{

	return (VarModify(file, VarTail, (void *)NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_GetHead --
 *	Find the leading components of a (list of) filename(s).
 *	XXX: VarHead does not replace foo by ., as (sun) System V make
 *	does.
 *
 * Results:
 *	The leading components.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
char *
Var_GetHead(char *file)
{

	return (VarModify(file, VarHead, (void *)NULL));
}

/*-
 *-----------------------------------------------------------------------
 * Var_Init --
 *	Initialize the module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The VAR_CMD and VAR_GLOBAL contexts are created
 *-----------------------------------------------------------------------
 */
void
Var_Init(void)
{

	VAR_GLOBAL = Targ_NewGN("Global");
	VAR_CMD = Targ_NewGN("Command");
}

/*-
 *-----------------------------------------------------------------------
 * Var_Dump --
 *	print all variables in a context
 *-----------------------------------------------------------------------
 */
void
Var_Dump(const GNode *ctxt)
{
	const LstNode	*ln;
	const Var	*v;

	LST_FOREACH(ln, &ctxt->context) {
		v = Lst_Datum(ln);
		printf("%-16s = %s\n", v->name, Buf_Data(v->val));
	}
}
