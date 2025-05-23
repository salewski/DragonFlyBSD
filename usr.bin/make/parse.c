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
 * @(#)parse.c	8.3 (Berkeley) 3/19/94
 * $FreeBSD: src/usr.bin/make/parse.c,v 1.75 2005/02/07 11:27:47 harti Exp $
 * $DragonFly: src/usr.bin/make/parse.c,v 1.64 2005/04/01 01:12:55 okumoto Exp $
 */

/*-
 * parse.c --
 *	Functions to parse a makefile.
 *
 *	One function, Parse_Init, must be called before any functions
 *	in this module are used. After that, the function Parse_File is the
 *	main entry point and controls most of the other functions in this
 *	module.
 *
 *	Most important structures are kept in Lsts. Directories for
 *	the #include "..." function are kept in the 'parseIncPath' Lst, while
 *	those for the #include <...> are kept in the 'sysIncPath' Lst. The
 *	targets currently being defined are kept in the 'targets' Lst.
 *
 *	The variables 'curFile.fname' and 'curFile.lineno' are used to track
 *	the name of the current file and the line number in that file so that
 *	error messages can be more meaningful.
 *
 * Interface:
 *	Parse_Init	Initialization function which must be
 *			called before anything else in this module is used.
 *
 *	Parse_File	Function used to parse a makefile. It must
 *			be given the name of the file, which should
 *			already have been opened, and a function
 *			to call to read a character from the file.
 *
 *	Parse_IsVar	Returns TRUE if the given line is a
 *			variable assignment. Used by MainParseArgs
 *			to determine if an argument is a target
 *			or a variable assignment. Used internally
 *			for pretty much the same thing...
 *
 *	Parse_Error	Function called when an error occurs in
 *			parsing. Used by the variable and
 *			conditional modules.
 *
 *	Parse_MainName	Returns a Lst of the main target to create.
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#include "arch.h"
#include "buf.h"
#include "cond.h"
#include "config.h"
#include "dir.h"
#include "for.h"
#include "globals.h"
#include "GNode.h"
#include "job.h"
#include "make.h"
#include "nonints.h"
#include "parse.h"
#include "pathnames.h"
#include "str.h"
#include "suff.h"
#include "targ.h"
#include "util.h"
#include "var.h"

/*
 * These values are returned by ParsePopInput to tell Parse_File whether to
 * CONTINUE parsing, i.e. it had only reached the end of an include file,
 * or if it's DONE.
 */
#define	CONTINUE	1
#define	DONE		0

/* targets we're working on */
static Lst targets = Lst_Initializer(targets);

/* true if currently in a dependency line or its commands */
static Boolean inLine;

static int fatals = 0;

/*
 * The main target to create. This is the first target on the
 * first dependency line in the first makefile.
 */
static GNode *mainNode;

/*
 * Definitions for handling #include specifications
 */
struct IFile {
	char	*fname;		/* name of previous file */
	int	lineno;		/* saved line number */
	FILE	*F;		/* the open stream */
	char	*str;		/* the string when parsing a string */
	char	*ptr;		/* the current pointer when parsing a string */
	TAILQ_ENTRY(IFile) link;/* stack the files */
};

/* stack of IFiles generated by * #includes */
static TAILQ_HEAD(, IFile) includes = TAILQ_HEAD_INITIALIZER(includes);

/* access current file */
#define	CURFILE	(TAILQ_FIRST(&includes))

/* list of directories for "..." includes */
struct Path parseIncPath = TAILQ_HEAD_INITIALIZER(parseIncPath);

/* list of directories for <...> includes */
struct Path sysIncPath = TAILQ_HEAD_INITIALIZER(sysIncPath);

/*
 * specType contains the SPECial TYPE of the current target. It is
 * Not if the target is unspecial. If it *is* special, however, the children
 * are linked as children of the parent but not vice versa. This variable is
 * set in ParseDoDependency
 */
typedef enum {
	Begin,		/* .BEGIN */
	Default,	/* .DEFAULT */
	End,		/* .END */
	Ignore,		/* .IGNORE */
	Includes,	/* .INCLUDES */
	Interrupt,	/* .INTERRUPT */
	Libs,		/* .LIBS */
	MFlags,		/* .MFLAGS or .MAKEFLAGS */
	Main,		/* .MAIN and we don't have anyth. user-spec. to make */
	NoExport,	/* .NOEXPORT */
	Not,		/* Not special */
	NotParallel,	/* .NOTPARALELL */
	Null,		/* .NULL */
	Order,		/* .ORDER */
	Parallel,	/* .PARALLEL */
	ExPath,		/* .PATH */
	Phony,		/* .PHONY */
	Posix,		/* .POSIX */
	Precious,	/* .PRECIOUS */
	ExShell,	/* .SHELL */
	Silent,		/* .SILENT */
	SingleShell,	/* .SINGLESHELL */
	Suffixes,	/* .SUFFIXES */
	Wait,		/* .WAIT */
	Attribute	/* Generic attribute */
} ParseSpecial;

static ParseSpecial specType;
static int waiting;

/*
 * Predecessor node for handling .ORDER. Initialized to NULL when .ORDER
 * seen, then set to each successive source on the line.
 */
static GNode *predecessor;

/*
 * The parseKeywords table is searched using binary search when deciding
 * if a target or source is special. The 'spec' field is the ParseSpecial
 * type of the keyword ("Not" if the keyword isn't special as a target) while
 * the 'op' field is the operator to apply to the list of targets if the
 * keyword is used as a source ("0" if the keyword isn't special as a source)
 */
static struct {
	const char	*name;	/* Name of keyword */
	ParseSpecial	spec;	/* Type when used as a target */
	int		op;	/* Operator when used as a source */
} parseKeywords[] = {
	{ ".BEGIN",		Begin,		0 },
	{ ".DEFAULT",		Default,	0 },
	{ ".END",		End,		0 },
	{ ".EXEC",		Attribute,	OP_EXEC },
	{ ".IGNORE",		Ignore,		OP_IGNORE },
	{ ".INCLUDES",		Includes,	0 },
	{ ".INTERRUPT",		Interrupt,	0 },
	{ ".INVISIBLE",		Attribute,	OP_INVISIBLE },
	{ ".JOIN",		Attribute,	OP_JOIN },
	{ ".LIBS",		Libs,		0 },
	{ ".MAIN",		Main,		0 },
	{ ".MAKE",		Attribute,	OP_MAKE },
	{ ".MAKEFLAGS",		MFlags,		0 },
	{ ".MFLAGS",		MFlags,		0 },
	{ ".NOTMAIN",		Attribute,	OP_NOTMAIN },
	{ ".NOTPARALLEL",	NotParallel,	0 },
	{ ".NO_PARALLEL",	NotParallel,	0 },
	{ ".NULL",		Null,		0 },
	{ ".OPTIONAL",		Attribute,	OP_OPTIONAL },
	{ ".ORDER",		Order,		0 },
	{ ".PARALLEL",		Parallel,	0 },
	{ ".PATH",		ExPath,		0 },
	{ ".PHONY",		Phony,		OP_PHONY },
	{ ".POSIX",		Posix,		0 },
	{ ".PRECIOUS",		Precious,	OP_PRECIOUS },
	{ ".RECURSIVE",		Attribute,	OP_MAKE },
	{ ".SHELL",		ExShell,	0 },
	{ ".SILENT",		Silent,		OP_SILENT },
	{ ".SINGLESHELL",	SingleShell,	0 },
	{ ".SUFFIXES",		Suffixes,	0 },
	{ ".USE",		Attribute,	OP_USE },
	{ ".WAIT",		Wait,		0 },
};

/*-
 *----------------------------------------------------------------------
 * ParseFindKeyword --
 *	Look in the table of keywords for one matching the given string.
 *
 * Results:
 *	The index of the keyword, or -1 if it isn't there.
 *
 * Side Effects:
 *	None
 *----------------------------------------------------------------------
 */
static int
ParseFindKeyword(char *str)
{
	int	start;
	int	end;
	int	cur;
	int	diff;

	start = 0;
	end = (sizeof(parseKeywords) / sizeof(parseKeywords[0])) - 1;

	do {
		cur = start + (end - start) / 2;
		diff = strcmp(str, parseKeywords[cur].name);
		if (diff == 0) {
			return (cur);
		} else if (diff < 0) {
			end = cur - 1;
		} else {
			start = cur + 1;
		}
	} while (start <= end);

	return (-1);
}

/*-
 * Parse_Error  --
 *	Error message abort function for parsing. Prints out the context
 *	of the error (line number and file) as well as the message with
 *	two optional arguments.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	"fatals" is incremented if the level is PARSE_FATAL.
 */
/* VARARGS */
void
Parse_Error(int type, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "\"%s\", line %d: ", CURFILE->fname, CURFILE->lineno);
	if (type == PARSE_WARNING)
		fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	fflush(stderr);
	if (type == PARSE_FATAL)
		fatals += 1;
}

/**
 * ParsePushInput
 *
 * Push a new input source onto the input stack. If ptr is NULL
 * the fullname is used to fopen the file. If it is not NULL,
 * ptr is assumed to point to the string to be parsed. If opening the
 * file fails, the fullname is freed.
 */
static void
ParsePushInput(char *fullname, FILE *fp, char *ptr, int lineno)
{
	struct IFile *nf;

	nf = emalloc(sizeof(*nf));
	nf->fname = fullname;
	nf->lineno = lineno;

	if (ptr == NULL) {
		/* the input source is a file */
		if ((nf->F = fp) == NULL) {
			nf->F = fopen(fullname, "r");
			if (nf->F == NULL) {
				Parse_Error(PARSE_FATAL, "Cannot open %s",
				    fullname);
				free(fullname);
				free(nf);
				return;
			}
		}
		nf->str = nf->ptr = NULL;
		Var_Append(".MAKEFILE_LIST", fullname, VAR_GLOBAL);
	} else {
		nf->str = nf->ptr = ptr;
		nf->F = NULL;
	}
	TAILQ_INSERT_HEAD(&includes, nf, link);
}

/**
 * ParsePopInput
 *	Called when EOF is reached in the current file. If we were reading
 *	an include file, the includes stack is popped and things set up
 *	to go back to reading the previous file at the previous location.
 *
 * Results:
 *	CONTINUE if there's more to do. DONE if not.
 *
 * Side Effects:
 *	The old curFile.F is closed. The includes list is shortened.
 *	curFile.lineno, curFile.F, and curFile.fname are changed if
 *	CONTINUE is returned.
 */
static int
ParsePopInput(void)
{
	struct IFile *ifile;	/* the state on the top of the includes stack */

	assert(!TAILQ_EMPTY(&includes));

	ifile = TAILQ_FIRST(&includes);
	TAILQ_REMOVE(&includes, ifile, link);

	free(ifile->fname);
	if (ifile->F != NULL) {
		fclose(ifile->F);
		Var_Append(".MAKEFILE_LIST", "..", VAR_GLOBAL);
	}
	if (ifile->str != NULL) {
		free(ifile->str);
	}
	free(ifile);

	return (TAILQ_EMPTY(&includes) ? DONE : CONTINUE);
}

/*-
 *---------------------------------------------------------------------
 * ParseLinkSrc  --
 *	Link the parent nodes to their new child. Used by
 *	ParseDoDependency. If the specType isn't 'Not', the parent
 *	isn't linked as a parent of the child.
 *
 * Side Effects:
 *	New elements are added to the parents lists of cgn and the
 *	children list of cgn. the unmade field of pgn is updated
 *	to reflect the additional child.
 *---------------------------------------------------------------------
 */
static void
ParseLinkSrc(Lst *parents, GNode *cgn)
{
	LstNode	*ln;
	GNode *pgn;

	LST_FOREACH(ln, parents) {
		pgn = Lst_Datum(ln);
		if (Lst_Member(&pgn->children, cgn) == NULL) {
			Lst_AtEnd(&pgn->children, cgn);
			if (specType == Not) {
				Lst_AtEnd(&cgn->parents, pgn);
			}
			pgn->unmade += 1;
		}
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseDoOp  --
 *	Apply the parsed operator to all target nodes. Used in
 *	ParseDoDependency once all targets have been found and their
 *	operator parsed. If the previous and new operators are incompatible,
 *	a major error is taken.
 *
 * Side Effects:
 *	The type field of the node is altered to reflect any new bits in
 *	the op.
 *---------------------------------------------------------------------
 */
static void
ParseDoOp(int op)
{
	GNode	*cohort;
	LstNode	*ln;
	GNode	*gn;

	LST_FOREACH(ln, &targets) {
		gn = Lst_Datum(ln);

		/*
		 * If the dependency mask of the operator and the node don't
		 * match and the node has actually had an operator applied to
		 * it before, and the operator actually has some dependency
		 * information in it, complain.
		 */
		if ((op & OP_OPMASK) != (gn->type & OP_OPMASK) &&
		    !OP_NOP(gn->type) && !OP_NOP(op)) {
			Parse_Error(PARSE_FATAL, "Inconsistent operator for %s",
			    gn->name);
			return;
		}

		if (op == OP_DOUBLEDEP &&
		    (gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
			/*
			 * If the node was the object of a :: operator, we need
			 * to create a new instance of it for the children and
			 * commands on this dependency line. The new instance
			 * is placed on the 'cohorts' list of the initial one
			 * (note the initial one is not on its own cohorts list)
			 * and the new instance is linked to all parents of the
			 * initial instance.
			 */
			cohort = Targ_NewGN(gn->name);

			/*
			 * Duplicate links to parents so graph traversal is
			 * simple. Perhaps some type bits should be duplicated?
			 *
			 * Make the cohort invisible as well to avoid
			 * duplicating it into other variables. True, parents
			 * of this target won't tend to do anything with their
			 * local variables, but better safe than sorry.
			 */
			ParseLinkSrc(&gn->parents, cohort);
			cohort->type = OP_DOUBLEDEP|OP_INVISIBLE;
			Lst_AtEnd(&gn->cohorts, cohort);

			/*
			 * Replace the node in the targets list with the
			 * new copy
			 */
			Lst_Replace(ln, cohort);
			gn = cohort;
		}
		/*
		 * We don't want to nuke any previous flags (whatever they were)
		 * so we just OR the new operator into the old
		 */
		gn->type |= op;
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseDoSrc  --
 *	Given the name of a source, figure out if it is an attribute
 *	and apply it to the targets if it is. Else decide if there is
 *	some attribute which should be applied *to* the source because
 *	of some special target and apply it if so. Otherwise, make the
 *	source be a child of the targets in the list 'targets'
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Operator bits may be added to the list of targets or to the source.
 *	The targets may have a new source added to their lists of children.
 *---------------------------------------------------------------------
 */
static void
ParseDoSrc(int tOp, char *src, Lst *allsrc)
{
	GNode	*gn = NULL;

	if (*src == '.' && isupper ((unsigned char) src[1])) {
		int keywd = ParseFindKeyword(src);
		if (keywd != -1) {
			if(parseKeywords[keywd].op != 0) {
				ParseDoOp(parseKeywords[keywd].op);
				return;
			}
			if (parseKeywords[keywd].spec == Wait) {
				waiting++;
				return;
			}
		}
	}

	switch (specType) {
	  case Main:
		/*
		 * If we have noted the existence of a .MAIN, it means we need
		 * to add the sources of said target to the list of things
		 * to create. The string 'src' is likely to be free, so we
		 * must make a new copy of it. Note that this will only be
		 * invoked if the user didn't specify a target on the command
		 * line. This is to allow #ifmake's to succeed, or something...
		 */
		Lst_AtEnd(&create, estrdup(src));
		/*
		 * Add the name to the .TARGETS variable as well, so the user
		 * can employ that, if desired.
		 */
		Var_Append(".TARGETS", src, VAR_GLOBAL);
		return;

	  case Order:
		/*
		 * Create proper predecessor/successor links between the
		 * previous source and the current one.
		 */
		gn = Targ_FindNode(src, TARG_CREATE);
		if (predecessor != NULL) {
			Lst_AtEnd(&predecessor->successors, gn);
			Lst_AtEnd(&gn->preds, predecessor);
		}
		/*
		 * The current source now becomes the predecessor for the next
		 * one.
		 */
		predecessor = gn;
		break;

	  default:
		/*
		 * If the source is not an attribute, we need to find/create
		 * a node for it. After that we can apply any operator to it
		 * from a special target or link it to its parents, as
		 * appropriate.
		 *
		 * In the case of a source that was the object of a :: operator,
		 * the attribute is applied to all of its instances (as kept in
		 * the 'cohorts' list of the node) or all the cohorts are linked
		 * to all the targets.
		 */
		gn = Targ_FindNode(src, TARG_CREATE);
		if (tOp) {
			gn->type |= tOp;
		} else {
			ParseLinkSrc(&targets, gn);
		}
		if ((gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
			GNode	*cohort;
			LstNode	*ln;

			for (ln = Lst_First(&gn->cohorts); ln != NULL;
			    ln = Lst_Succ(ln)) {
				cohort = Lst_Datum(ln);
				if (tOp) {
					cohort->type |= tOp;
				} else {
					ParseLinkSrc(&targets, cohort);
				}
			}
		}
		break;
	}

	gn->order = waiting;
	Lst_AtEnd(allsrc, gn);
	if (waiting) {
		LstNode	*ln;
		GNode	*p;

		/*
		 * Check if GNodes needs to be synchronized.
		 * This has to be when two nodes are on different sides of a
		 * .WAIT directive.
		 */
		LST_FOREACH(ln, allsrc) {
			p = Lst_Datum(ln);

			if (p->order >= gn->order)
				break;
			/*
			 * XXX: This can cause loops, and loops can cause
			 * unmade targets, but checking is tedious, and the
			 * debugging output can show the problem
			 */
			Lst_AtEnd(&p->successors, gn);
			Lst_AtEnd(&gn->preds, p);
		}
	}
}

static char *
stripvarname(char *cp)
{
	char   *cp2;

	while (isspace((unsigned char)*cp))
		++cp;
	cp2 = cp;
	while (*cp2 && !isspace((unsigned char)*cp2))
		++cp2;
	*cp2 = 0;
	return (cp);
}


/*-
 *---------------------------------------------------------------------
 * ParseDoDependency  --
 *	Parse the dependency line in line.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The nodes of the sources are linked as children to the nodes of the
 *	targets. Some nodes may be created.
 *
 *	We parse a dependency line by first extracting words from the line and
 * finding nodes in the list of all targets with that name. This is done
 * until a character is encountered which is an operator character. Currently
 * these are only ! and :. At this point the operator is parsed and the
 * pointer into the line advanced until the first source is encountered.
 *	The parsed operator is applied to each node in the 'targets' list,
 * which is where the nodes found for the targets are kept, by means of
 * the ParseDoOp function.
 *	The sources are read in much the same way as the targets were except
 * that now they are expanded using the wildcarding scheme of the C-Shell
 * and all instances of the resulting words in the list of all targets
 * are found. Each of the resulting nodes is then linked to each of the
 * targets as one of its children.
 *	Certain targets are handled specially. These are the ones detailed
 * by the specType variable.
 *	The storing of transformation rules is also taken care of here.
 * A target is recognized as a transformation rule by calling
 * Suff_IsTransform. If it is a transformation rule, its node is gotten
 * from the suffix module via Suff_AddTransform rather than the standard
 * Targ_FindNode in the target module.
 *---------------------------------------------------------------------
 */
static void
ParseDoDependency(char *line)
{
	char	*cp;	/* our current position */
	GNode	*gn;	/* a general purpose temporary node */
	int	op;	/* the operator on the line */
	char	savec;	/* a place to save a character */
	Lst	paths;	/* Search paths to alter when parsing .PATH targets */
	int	tOp;	/* operator from special target */
	LstNode	*ln;

	tOp = 0;

	specType = Not;
	waiting = 0;
	Lst_Init(&paths);

	do {
		for (cp = line;
		    *cp && !isspace((unsigned char)*cp) && *cp != '(';
		    cp++) {
			if (*cp == '$') {
				/*
				 * Must be a dynamic source (would have been
				 * expanded otherwise), so call the Var module
				 * to parse the puppy so we can safely advance
				 * beyond it...There should be no errors in this
				 * as they would have been discovered in the
				 * initial Var_Subst and we wouldn't be here.
				 */
				size_t	length = 0;
				Boolean	freeIt;
				char	*result;

				result = Var_Parse(cp, VAR_CMD, TRUE,
				    &length, &freeIt);

				if (freeIt) {
					free(result);
				}
				cp += length - 1;

			} else if (*cp == '!' || *cp == ':') {
				/*
				 * We don't want to end a word on ':' or '!' if
				 * there is a better match later on in the
				 * string (greedy matching).
				 * This allows the user to have targets like:
				 *    fie::fi:fo: fum
				 *    foo::bar:
				 * where "fie::fi:fo" and "foo::bar" are the
				 * targets. In real life this is used for perl5
				 * library man pages where "::" separates an
				 * object from its class. Ie:
				 * "File::Spec::Unix". This behaviour is also
				 * consistent with other versions of make.
				 */
				char *p = cp + 1;

				if (*cp == ':' && *p == ':')
					p++;

				/* Found the best match already. */
				if (*p == '\0' || isspace(*p))
					break;

				p += strcspn(p, "!:");

				/* No better match later on... */
				if (*p == '\0')
					break;
			}
			continue;
		}
		if (*cp == '(') {
			/*
			 * Archives must be handled specially to make sure the
			 * OP_ARCHV flag is set in their 'type' field, for one
			 * thing, and because things like "archive(file1.o
			 * file2.o file3.o)" are permissible. Arch_ParseArchive
			 * will set 'line' to be the first non-blank after the
			 * archive-spec. It creates/finds nodes for the members
			 * and places them on the given list, returning SUCCESS
			 * if all went well and FAILURE if there was an error in
			 * the specification. On error, line should remain
			 * untouched.
			 */
			if (Arch_ParseArchive(&line, &targets, VAR_CMD) !=
			    SUCCESS) {
				Parse_Error(PARSE_FATAL,
				    "Error in archive specification: \"%s\"",
				    line);
				return;
			} else {
				cp = line;
				continue;
			}
		}
		savec = *cp;

		if (!*cp) {
			/*
			 * Ending a dependency line without an operator is a				 * Bozo no-no. As a heuristic, this is also often
			 * triggered by undetected conflicts from cvs/rcs
			 * merges.
			 */
			if (strncmp(line, "<<<<<<", 6) == 0 ||
			    strncmp(line, "======", 6) == 0 ||
			    strncmp(line, ">>>>>>", 6) == 0) {
				Parse_Error(PARSE_FATAL, "Makefile appears to "
				    "contain unresolved cvs/rcs/??? merge "
				    "conflicts");
			} else
				Parse_Error(PARSE_FATAL, "Need an operator");
			return;
		}
		*cp = '\0';
		/*
		 * Have a word in line. See if it's a special target and set
		 * specType to match it.
		 */
		if (*line == '.' && isupper((unsigned char)line[1])) {
			/*
			 * See if the target is a special target that must have
			 * it or its sources handled specially.
			 */
			int keywd = ParseFindKeyword(line);

			if (keywd != -1) {
				if (specType == ExPath &&
				    parseKeywords[keywd].spec != ExPath) {
					Parse_Error(PARSE_FATAL,
					    "Mismatched special targets");
					return;
				}

				specType = parseKeywords[keywd].spec;
				tOp = parseKeywords[keywd].op;

				/*
				 * Certain special targets have special
				 * semantics:
				 *  .PATH	Have to set the dirSearchPath
				 *		variable too
				 *  .MAIN	Its sources are only used if
				 *		nothing has been specified to
				 *		create.
				 *  .DEFAULT    Need to create a node to hang
				 *		commands on, but we don't want
				 *		it in the graph, nor do we want
				 *		it to be the Main Target, so we
				 *		create it, set OP_NOTMAIN and
				 *		add it to the list, setting
				 *		DEFAULT to the new node for
				 *		later use. We claim the node is
				 *		A transformation rule to make
				 *		life easier later, when we'll
				 *		use Make_HandleUse to actually
				 *		apply the .DEFAULT commands.
				 *  .PHONY	The list of targets
				 *  .BEGIN
				 *  .END
				 *  .INTERRUPT	Are not to be considered the
				 *		main target.
				 *  .NOTPARALLEL Make only one target at a time.
				 *  .SINGLESHELL Create a shell for each
				 *		command.
				 *  .ORDER	Must set initial predecessor
				 *		to NULL
				 */
				switch (specType) {
				  case ExPath:
					Lst_AtEnd(&paths, &dirSearchPath);
					break;
				  case Main:
					if (!Lst_IsEmpty(&create)) {
						specType = Not;
					}
					break;
				  case Begin:
				  case End:
				  case Interrupt:
					gn = Targ_FindNode(line, TARG_CREATE);
					gn->type |= OP_NOTMAIN;
					Lst_AtEnd(&targets, gn);
					break;
				  case Default:
					gn = Targ_NewGN(".DEFAULT");
					gn->type |= (OP_NOTMAIN|OP_TRANSFORM);
					Lst_AtEnd(&targets, gn);
					DEFAULT = gn;
					break;
				  case NotParallel:
					maxJobs = 1;
					break;
				  case SingleShell:
					compatMake = 1;
					break;
				  case Order:
					predecessor = NULL;
					break;
				  default:
					break;
				}

			} else if (strncmp(line, ".PATH", 5) == 0) {
				/*
				 * .PATH<suffix> has to be handled specially.
				 * Call on the suffix module to give us a path
				 * to modify.
				 */
				struct Path *path;

				specType = ExPath;
				path = Suff_GetPath(&line[5]);
				if (path == NULL) {
					Parse_Error(PARSE_FATAL, "Suffix '%s' "
					    "not defined (yet)", &line[5]);
					return;
				} else
					Lst_AtEnd(&paths, path);
			}
		}

		/*
		 * Have word in line. Get or create its node and stick it at
		 * the end of the targets list
		 */
		if (specType == Not && *line != '\0') {

			/* target names to be found and added to targets list */
			Lst curTargs = Lst_Initializer(curTargs);

			if (Dir_HasWildcards(line)) {
				/*
				 * Targets are to be sought only in the current
				 * directory, so create an empty path for the
				 * thing. Note we need to use Path_Clear in the
				 * destruction of the path as the Dir module
				 * could have added a directory to the path...
				 */
				struct Path emptyPath =
				    TAILQ_HEAD_INITIALIZER(emptyPath);

				Path_Expand(line, &emptyPath, &curTargs);
				Path_Clear(&emptyPath);

			} else {
				/*
				 * No wildcards, but we want to avoid code
				 * duplication, so create a list with the word
				 * on it.
				 */
				Lst_AtEnd(&curTargs, line);
			}

			while (!Lst_IsEmpty(&curTargs)) {
				char	*targName = Lst_DeQueue(&curTargs);

				if (!Suff_IsTransform (targName)) {
					gn = Targ_FindNode(targName,
					    TARG_CREATE);
				} else {
					gn = Suff_AddTransform(targName);
				}

				Lst_AtEnd(&targets, gn);
			}
		} else if (specType == ExPath && *line != '.' && *line != '\0'){
			Parse_Error(PARSE_WARNING, "Extra target (%s) ignored",
			    line);
		}

		*cp = savec;
		/*
		 * If it is a special type and not .PATH, it's the only
		 * target we allow on this line...
		 */
		if (specType != Not && specType != ExPath) {
			Boolean warnFlag = FALSE;

			while (*cp != '!' && *cp != ':' && *cp) {
				if (*cp != ' ' && *cp != '\t') {
					warnFlag = TRUE;
				}
				cp++;
			}
			if (warnFlag) {
				Parse_Error(PARSE_WARNING,
				    "Extra target ignored");
			}
		} else {
			while (*cp && isspace((unsigned char)*cp)) {
				cp++;
			}
		}
		line = cp;
	} while (*line != '!' && *line != ':' && *line);

	if (!Lst_IsEmpty(&targets)) {
		switch (specType) {
		  default:
			Parse_Error(PARSE_WARNING, "Special and mundane "
			    "targets don't mix. Mundane ones ignored");
			break;
		  case Default:
		  case Begin:
		  case End:
		  case Interrupt:
			/*
			 * These four create nodes on which to hang commands, so
			 * targets shouldn't be empty...
			 */
		  case Not:
			/*
			 * Nothing special here -- targets can be empty if it
			 * wants.
			 */
			break;
		}
	}

	/*
	 * Have now parsed all the target names. Must parse the operator next.
	 * The result is left in op.
	 */
	if (*cp == '!') {
		op = OP_FORCE;
	} else if (*cp == ':') {
		if (cp[1] == ':') {
			op = OP_DOUBLEDEP;
			cp++;
		} else {
			op = OP_DEPENDS;
		}
	} else {
		Parse_Error(PARSE_FATAL, "Missing dependency operator");
		return;
	}

	cp++;			/* Advance beyond operator */

	ParseDoOp(op);

	/*
	 * Get to the first source
	 */
	while (*cp && isspace((unsigned char)*cp)) {
		cp++;
	}
	line = cp;

	/*
	 * Several special targets take different actions if present with no
	 * sources:
	 *	a .SUFFIXES line with no sources clears out all old suffixes
	 *	a .PRECIOUS line makes all targets precious
	 *	a .IGNORE line ignores errors for all targets
	 *	a .SILENT line creates silence when making all targets
	 *	a .PATH removes all directories from the search path(s).
	 */
	if (!*line) {
		switch (specType) {
		  case Suffixes:
			Suff_ClearSuffixes();
			break;
		  case Precious:
			allPrecious = TRUE;
			break;
		  case Ignore:
			ignoreErrors = TRUE;
			break;
		  case Silent:
			beSilent = TRUE;
			break;
		  case ExPath:
			LST_FOREACH(ln, &paths)
			Path_Clear(Lst_Datum(ln));
			break;
		  case Posix:
			Var_Set("%POSIX", "1003.2", VAR_GLOBAL);
			break;
		  default:
			break;
		}

	} else if (specType == MFlags) {
		/*
		 * Call on functions in main.c to deal with these arguments and
		 * set the initial character to a null-character so the loop to
		 * get sources won't get anything
		 */
		Main_ParseArgLine(line, 0);
		*line = '\0';

	} else if (specType == ExShell) {
		if (Job_ParseShell(line) != SUCCESS) {
			Parse_Error(PARSE_FATAL,
			    "improper shell specification");
			return;
		}
		*line = '\0';

	} else if (specType == NotParallel || specType == SingleShell) {
		*line = '\0';
	}

	/*
	* NOW GO FOR THE SOURCES
	*/
	if (specType == Suffixes || specType == ExPath ||
	    specType == Includes || specType == Libs ||
	    specType == Null) {
		while (*line) {
			/*
			 * If the target was one that doesn't take files as its
			 * sources but takes something like suffixes, we take
			 * each space-separated word on the line as a something
			 * and deal with it accordingly.
			 *
			 * If the target was .SUFFIXES, we take each source as
			 * a suffix and add it to the list of suffixes
			 * maintained by the Suff module.
			 *
			 * If the target was a .PATH, we add the source as a
			 * directory to search on the search path.
			 *
			 * If it was .INCLUDES, the source is taken to be the
			 * suffix of files which will be #included and whose
			 * search path should be present in the .INCLUDES
			 * variable.
			 *
			 * If it was .LIBS, the source is taken to be the
			 * suffix of files which are considered libraries and
			 * whose search path should be present in the .LIBS
			 * variable.
			 *
			 * If it was .NULL, the source is the suffix to use
			 * when a file has no valid suffix.
			 */
			char  savech;
			while (*cp && !isspace((unsigned char)*cp)) {
				cp++;
			}
			savech = *cp;
			*cp = '\0';
			switch (specType) {
			  case Suffixes:
				Suff_AddSuffix(line);
				break;
			  case ExPath:
				LST_FOREACH(ln, &paths)
					Path_AddDir(Lst_Datum(ln), line);
				break;
			  case Includes:
				Suff_AddInclude(line);
				break;
			  case Libs:
				Suff_AddLib(line);
				break;
			  case Null:
				Suff_SetNull(line);
				break;
			  default:
				break;
			}
			*cp = savech;
			if (savech != '\0') {
				cp++;
			}
			while (*cp && isspace((unsigned char)*cp)) {
				cp++;
			}
			line = cp;
		}
		Lst_Destroy(&paths, NOFREE);

	} else {
		/* list of sources in order */
		Lst curSrcs = Lst_Initializer(curSrc);

		while (*line) {
			/*
			 * The targets take real sources, so we must beware of
			 * archive specifications (i.e. things with left
			 * parentheses in them) and handle them accordingly.
			 */
			while (*cp && !isspace((unsigned char)*cp)) {
				if (*cp == '(' && cp > line && cp[-1] != '$') {
					/*
					 * Only stop for a left parenthesis if
					 * it isn't at the start of a word
					 * (that'll be for variable changes
					 * later) and isn't preceded by a dollar
					 * sign (a dynamic source).
					 */
					break;
				} else {
					cp++;
				}
			}

			if (*cp == '(') {
				GNode	  *gnp;

				/* list of archive source names after exp. */
				Lst sources = Lst_Initializer(sources);

				if (Arch_ParseArchive(&line, &sources,
				    VAR_CMD) != SUCCESS) {
					Parse_Error(PARSE_FATAL, "Error in "
					    "source archive spec \"%s\"", line);
					return;
				}

				while (!Lst_IsEmpty(&sources)) {
					gnp = Lst_DeQueue(&sources);
					ParseDoSrc(tOp, gnp->name, &curSrcs);
				}
				cp = line;
			} else {
				if (*cp) {
					*cp = '\0';
					cp += 1;
				}

				ParseDoSrc(tOp, line, &curSrcs);
			}
			while (*cp && isspace((unsigned char)*cp)) {
				cp++;
			}
			line = cp;
		}
		Lst_Destroy(&curSrcs, NOFREE);
	}

	if (mainNode == NULL) {
		/*
		 * If we have yet to decide on a main target to make, in the
		 * absence of any user input, we want the first target on
		 * the first dependency line that is actually a real target
		 * (i.e. isn't a .USE or .EXEC rule) to be made.
		 */
		LST_FOREACH(ln, &targets) {
			gn = Lst_Datum(ln);
			if ((gn->type & (OP_NOTMAIN | OP_USE |
			    OP_EXEC | OP_TRANSFORM)) == 0) {
				mainNode = gn;
				Targ_SetMain(gn);
				break;
			}
		}
	}
}

/*-
 *---------------------------------------------------------------------
 * Parse_IsVar  --
 *	Return TRUE if the passed line is a variable assignment. A variable
 *	assignment consists of a single word followed by optional whitespace
 *	followed by either a += or an = operator.
 *	This function is used both by the Parse_File function and main when
 *	parsing the command-line arguments.
 *
 * Results:
 *	TRUE if it is. FALSE if it ain't
 *
 * Side Effects:
 *	none
 *---------------------------------------------------------------------
 */
Boolean
Parse_IsVar(char *line)
{
	Boolean wasSpace = FALSE;	/* set TRUE if found a space */
	Boolean haveName = FALSE;	/* Set TRUE if have a variable name */

	int level = 0;
#define	ISEQOPERATOR(c) \
	((c) == '+' || (c) == ':' || (c) == '?' || (c) == '!')

	/*
	 * Skip to variable name
	 */
	for (; *line == ' ' || *line == '\t'; line++)
		continue;

	for (; *line != '=' || level != 0; line++) {
		switch (*line) {
		  case '\0':
			/*
			 * end-of-line -- can't be a variable assignment.
			 */
			return (FALSE);

		  case ' ':
		  case '\t':
			/*
			 * there can be as much white space as desired so long
			 * as there is only one word before the operator
			*/
			wasSpace = TRUE;
			break;

		  case '(':
		  case '{':
			level++;
			break;

		  case '}':
		  case ')':
			level--;
			break;

		  default:
			if (wasSpace && haveName) {
				if (ISEQOPERATOR(*line)) {
					/*
					 * We must have a finished word
					 */
					if (level != 0)
						return (FALSE);

					/*
					 * When an = operator [+?!:] is found,
					 * the next character must be an = or
					 * it ain't a valid assignment.
					 */
					if (line[1] == '=')
						return (haveName);
#ifdef SUNSHCMD
					/*
					 * This is a shell command
					 */
					if (strncmp(line, ":sh", 3) == 0)
						return (haveName);
#endif
				}
				/*
				 * This is the start of another word, so not
				 * assignment.
				 */
				return (FALSE);

			} else {
				haveName = TRUE;
				wasSpace = FALSE;
			}
			break;
		}
	}

	return (haveName);
}

/*-
 *---------------------------------------------------------------------
 * Parse_DoVar  --
 *	Take the variable assignment in the passed line and do it in the
 *	global context.
 *
 *	Note: There is a lexical ambiguity with assignment modifier characters
 *	in variable names. This routine interprets the character before the =
 *	as a modifier. Therefore, an assignment like
 *	    C++=/usr/bin/CC
 *	is interpreted as "C+ +=" instead of "C++ =".
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	the variable structure of the given variable name is altered in the
 *	global context.
 *---------------------------------------------------------------------
 */
void
Parse_DoVar(char *line, GNode *ctxt)
{
	char	*cp;	/* pointer into line */
	enum {
		VAR_SUBST,
		VAR_APPEND,
		VAR_SHELL,
		VAR_NORMAL
	}	type;	/* Type of assignment */
	char	*opc;	/* ptr to operator character to
			 * null-terminate the variable name */

	/*
	 * Avoid clobbered variable warnings by forcing the compiler
	 * to ``unregister'' variables
	 */
#if __GNUC__
	(void)&cp;
	(void)&line;
#endif

	/*
	 * Skip to variable name
	 */
	while (*line == ' ' || *line == '\t') {
		line++;
	}

	/*
	 * Skip to operator character, nulling out whitespace as we go
	 */
	for (cp = line + 1; *cp != '='; cp++) {
		if (isspace((unsigned char)*cp)) {
			*cp = '\0';
		}
	}
	opc = cp - 1;		/* operator is the previous character */
	*cp++ = '\0';		/* nuke the = */

	/*
	 * Check operator type
	 */
	switch (*opc) {
	  case '+':
		type = VAR_APPEND;
		*opc = '\0';
		break;

	  case '?':
		/*
		 * If the variable already has a value, we don't do anything.
		 */
		*opc = '\0';
		if (Var_Exists(line, ctxt)) {
			return;
		} else {
			type = VAR_NORMAL;
		}
		break;

	  case ':':
		type = VAR_SUBST;
		*opc = '\0';
		break;

	  case '!':
		type = VAR_SHELL;
		*opc = '\0';
		break;

	  default:
#ifdef SUNSHCMD
		while (*opc != ':') {
			if (opc == line)
				break;
			else
				--opc;
		}

		if (strncmp(opc, ":sh", 3) == 0) {
			type = VAR_SHELL;
			*opc = '\0';
			break;
		}
#endif
		type = VAR_NORMAL;
		break;
	}

	while (isspace((unsigned char)*cp)) {
		cp++;
	}

	if (type == VAR_APPEND) {
		Var_Append(line, cp, ctxt);

	} else if (type == VAR_SUBST) {
		/*
		 * Allow variables in the old value to be undefined, but leave
		 * their invocation alone -- this is done by forcing oldVars
		 * to be false.
		 * XXX: This can cause recursive variables, but that's not
		 * hard to do, and this allows someone to do something like
		 *
		 *  CFLAGS = $(.INCLUDES)
		 *  CFLAGS := -I.. $(CFLAGS)
		 *
		 * And not get an error.
		 */
		Boolean oldOldVars = oldVars;

		oldVars = FALSE;

		/*
		 * make sure that we set the variable the first time to nothing
		 * so that it gets substituted!
		 */
		if (!Var_Exists(line, ctxt))
			Var_Set(line, "", ctxt);

		cp = Buf_Peel(Var_Subst(NULL, cp, ctxt, FALSE));

		oldVars = oldOldVars;

		Var_Set(line, cp, ctxt);
		free(cp);

	} else if (type == VAR_SHELL) {
		/*
		 * TRUE if the command needs to be freed, i.e.
		 * if any variable expansion was performed
		 */
		Boolean	freeCmd = FALSE;
		Buffer *buf;
		const char *error;

		if (strchr(cp, '$') != NULL) {
			/*
			 * There's a dollar sign in the command, so perform
			 * variable expansion on the whole thing. The
			 * resulting string will need freeing when we're done,
			 * so set freeCmd to TRUE.
			 */
			cp = Buf_Peel(Var_Subst(NULL, cp, VAR_CMD, TRUE));
			freeCmd = TRUE;
		}

		buf = Cmd_Exec(cp, &error);
		Var_Set(line, Buf_Data(buf), ctxt);
		Buf_Destroy(buf, TRUE);

		if (error)
			Parse_Error(PARSE_WARNING, error, cp);

		if (freeCmd)
			free(cp);

	} else {
		/*
		 * Normal assignment -- just do it.
		 */
		Var_Set(line, cp, ctxt);
	}
}

/*-
 *-----------------------------------------------------------------------
 * ParseHasCommands --
 *	Callback procedure for Parse_File when destroying the list of
 *	targets on the last dependency line. Marks a target as already
 *	having commands if it does, to keep from having shell commands
 *	on multiple dependency lines.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	OP_HAS_COMMANDS may be set for the target.
 *
 *-----------------------------------------------------------------------
 */
static void
ParseHasCommands(void *gnp)
{
	GNode *gn = gnp;

	if (!Lst_IsEmpty(&gn->commands)) {
		gn->type |= OP_HAS_COMMANDS;
	}
}

/*-
 *-----------------------------------------------------------------------
 * Parse_AddIncludeDir --
 *	Add a directory to the path searched for included makefiles
 *	bracketed by double-quotes. Used by functions in main.c
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The directory is appended to the list.
 *
 *-----------------------------------------------------------------------
 */
void
Parse_AddIncludeDir(char *dir)
{

	Path_AddDir(&parseIncPath, dir);
}

/*---------------------------------------------------------------------
 * ParseDoError  --
 *	Handle error directive
 *
 *	The input is the line minus the ".error".  We substitute variables,
 *	print the message and exit(1) or just print a warning if the ".error"
 *	directive is malformed.
 *
 *---------------------------------------------------------------------
 */
static void
ParseDoError(char *errmsg)
{
	Buffer *buf;

	if (!isspace((unsigned char)*errmsg)) {
		Parse_Error(PARSE_WARNING, "invalid syntax: .error%s", errmsg);
		return;
	}

	while (isspace((unsigned char)*errmsg))
		errmsg++;

	buf = Var_Subst(NULL, errmsg, VAR_GLOBAL, FALSE);
	Parse_Error(PARSE_FATAL, "%s", Buf_Data(buf));
	Buf_Destroy(buf, TRUE);

	/* Terminate immediately. */
	exit(1);
}

/*---------------------------------------------------------------------
 * ParseDoWarning  --
 *	Handle warning directive
 *
 *	The input is the line minus the ".warning".  We substitute variables
 *	and print the message or just print a warning if the ".warning"
 *	directive is malformed.
 *
 *---------------------------------------------------------------------
 */
static void
ParseDoWarning(char *warnmsg)
{
	Buffer *buf;

	if (!isspace((unsigned char)*warnmsg)) {
		Parse_Error(PARSE_WARNING, "invalid syntax: .warning%s",
		    warnmsg);
		return;
	}

	while (isspace((unsigned char)*warnmsg))
		warnmsg++;

	buf = Var_Subst(NULL, warnmsg, VAR_GLOBAL, FALSE);
	Parse_Error(PARSE_WARNING, "%s", Buf_Data(buf));
	Buf_Destroy(buf, TRUE);
}

/*-
 *---------------------------------------------------------------------
 * ParseDoInclude  --
 *	Push to another file.
 *
 *	The input is the line minus the #include. A file spec is a string
 *	enclosed in <> or "". The former is looked for only in sysIncPath.
 *	The latter in . and the directories specified by -I command line
 *	options
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A structure is added to the includes Lst and readProc.
 *---------------------------------------------------------------------
 */
static void
ParseDoInclude(char *file)
{
	char	*fullname;	/* full pathname of file */
	char	endc;		/* the character which ends the file spec */
	char	*cp;		/* current position in file spec */
	Boolean	isSystem;	/* TRUE if makefile is a system makefile */
	Buffer	*buf;

	/*
	 * Skip to delimiter character so we know where to look
	 */
	while (*file == ' ' || *file == '\t') {
		file++;
	}

	if (*file != '"' && *file != '<') {
		Parse_Error(PARSE_FATAL,
		    ".include filename must be delimited by '\"' or '<'");
		return;
	}

	/*
	 * Set the search path on which to find the include file based on the
	 * characters which bracket its name. Angle-brackets imply it's
	 * a system Makefile while double-quotes imply it's a user makefile
	 */
	if (*file == '<') {
		isSystem = TRUE;
		endc = '>';
	} else {
		isSystem = FALSE;
		endc = '"';
	}

	/*
	* Skip to matching delimiter
	*/
	for (cp = ++file; *cp && *cp != endc; cp++) {
		continue;
	}

	if (*cp != endc) {
		Parse_Error(PARSE_FATAL,
		    "Unclosed %cinclude filename. '%c' expected", '.', endc);
		return;
	}
	*cp = '\0';

	/*
	 * Substitute for any variables in the file name before trying to
	 * find the thing.
	 */
	buf = Var_Subst(NULL, file, VAR_CMD, FALSE);
	file = Buf_Peel(buf);

	/*
	 * Now we know the file's name and its search path, we attempt to
	 * find the durn thing. A return of NULL indicates the file don't
	 * exist.
	 */
	if (!isSystem) {
		/*
		 * Include files contained in double-quotes are first searched
		 * for relative to the including file's location. We don't want
		 * to cd there, of course, so we just tack on the old file's
		 * leading path components and call Dir_FindFile to see if
		 * we can locate the beast.
		 */
		char	*prefEnd, *Fname;

		/* Make a temporary copy of this, to be safe. */
		Fname = estrdup(CURFILE->fname);

		prefEnd = strrchr(Fname, '/');
		if (prefEnd != (char *)NULL) {
			char	*newName;

			*prefEnd = '\0';
			if (file[0] == '/')
				newName = estrdup(file);
			else
				newName = str_concat(Fname, file, STR_ADDSLASH);
			fullname = Path_FindFile(newName, &parseIncPath);
			if (fullname == NULL) {
				fullname = Path_FindFile(newName,
				    &dirSearchPath);
			}
			free(newName);
			*prefEnd = '/';
		} else {
			fullname = NULL;
		}
		free(Fname);
	} else {
		fullname = NULL;
	}

	if (fullname == NULL) {
		/*
		 * System makefile or makefile wasn't found in same directory as
		 * included makefile. Search for it first on the -I search path,
		 * then on the .PATH search path, if not found in a -I
		 * directory.
		 * XXX: Suffix specific?
		 */
		fullname = Path_FindFile(file, &parseIncPath);
		if (fullname == NULL) {
			fullname = Path_FindFile(file, &dirSearchPath);
		}
	}

	if (fullname == NULL) {
		/*
		 * Still haven't found the makefile. Look for it on the system
		 * path as a last resort.
		 */
		fullname = Path_FindFile(file, &sysIncPath);
	}

	if (fullname == NULL) {
		*cp = endc;
		Parse_Error(PARSE_FATAL, "Could not find %s", file);
		/* XXXHB free(file) */
		return;
	}

	free(file);

	/*
	 * We set up the name of the file to be the absolute
	 * name of the include file so error messages refer to the right
	 * place.
	 */
	ParsePushInput(fullname, NULL, NULL, 0);
}

/*-
 *---------------------------------------------------------------------
 * Parse_FromString  --
 *	Start Parsing from the given string
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A structure is added to the includes Lst and readProc, curFile.lineno,
 *	curFile.fname and curFile.F are altered for the new file
 *---------------------------------------------------------------------
 */
void
Parse_FromString(char *str, int lineno)
{

	DEBUGF(FOR, ("%s\n---- at line %d\n", str, lineno));

	ParsePushInput(estrdup(CURFILE->fname), NULL, str, lineno);
}

#ifdef SYSVINCLUDE
/*-
 *---------------------------------------------------------------------
 * ParseTraditionalInclude  --
 *	Push to another file.
 *
 *	The input is the line minus the "include".  The file name is
 *	the string following the "include".
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A structure is added to the includes Lst and readProc, curFile.lineno,
 *	curFile.fname and curFile.F are altered for the new file
 *---------------------------------------------------------------------
 */
static void
ParseTraditionalInclude(char *file)
{
	char	*fullname;	/* full pathname of file */
	char	*cp;		/* current position in file spec */
	Buffer	*buf;

	/*
	 * Skip over whitespace
	 */
	while (*file == ' ' || *file == '\t') {
		file++;
	}

	if (*file == '\0') {
		Parse_Error(PARSE_FATAL, "Filename missing from \"include\"");
		return;
	}

	/*
	* Skip to end of line or next whitespace
	*/
	for (cp = file; *cp && *cp != '\n' && *cp != '\t' && *cp != ' '; cp++) {
		continue;
	}

	*cp = '\0';

	/*
	 * Substitute for any variables in the file name before trying to
	 * find the thing.
	 */
	buf = Var_Subst(NULL, file, VAR_CMD, FALSE);
	file = Buf_Peel(buf);

	/*
	 * Now we know the file's name, we attempt to find the durn thing.
	 * Search for it first on the -I search path, then on the .PATH
	 * search path, if not found in a -I directory.
	 */
	fullname = Path_FindFile(file, &parseIncPath);
	if (fullname == NULL) {
		fullname = Path_FindFile(file, &dirSearchPath);
	}

	if (fullname == NULL) {
		/*
		 * Still haven't found the makefile. Look for it on the system
		 * path as a last resort.
		 */
		fullname = Path_FindFile(file, &sysIncPath);
	}

	if (fullname == NULL) {
		Parse_Error(PARSE_FATAL, "Could not find %s", file);
		/* XXXHB free(file) */
		return;
	}

	/* XXXHB free(file) */

	/*
	 * We set up the name of the file to be the absolute
	 * name of the include file so error messages refer to the right
	 * place.
	 */
	ParsePushInput(fullname, NULL, NULL, 0);
}
#endif

/*-
 *---------------------------------------------------------------------
 * ParseReadc  --
 *	Read a character from the current file
 *
 * Results:
 *	The character that was read
 *
 * Side Effects:
 *---------------------------------------------------------------------
 */
static int
ParseReadc(void)
{

	if (CURFILE->F != NULL)
		return (fgetc(CURFILE->F));

	if (CURFILE->str != NULL && *CURFILE->ptr != '\0')
		return (*CURFILE->ptr++);

	return (EOF);
}


/*-
 *---------------------------------------------------------------------
 * ParseUnreadc  --
 *	Put back a character to the current file
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *---------------------------------------------------------------------
 */
static void
ParseUnreadc(int c)
{

	if (CURFILE->F != NULL) {
		ungetc(c, CURFILE->F);
		return;
	}
	if (CURFILE->str != NULL) {
		*--(CURFILE->ptr) = c;
		return;
	}
}

/* ParseSkipLine():
 *	Grab the next line unless it begins with a dot (`.') and we're told to
 *	ignore such lines.
 */
static char *
ParseSkipLine(int skip, int keep_newline)
{
	char *line;
	int c, lastc;
	Buffer *buf;

	buf = Buf_Init(MAKE_BSIZE);

	do {
		Buf_Clear(buf);
		lastc = '\0';

		while (((c = ParseReadc()) != '\n' || lastc == '\\')
		    && c != EOF) {
			if (skip && c == '#' && lastc != '\\') {
				/*
				 * let a comment be terminated even by an
				 * escaped \n. This is consistent to comment
				 * handling in ParseReadLine
				 */
				while ((c = ParseReadc()) != '\n' && c != EOF)
					;
				break;
			}
			if (c == '\n') {
				if (keep_newline)
					Buf_AddByte(buf, (Byte)c);
				else
					Buf_ReplaceLastByte(buf, (Byte)' ');
				CURFILE->lineno++;

				while ((c = ParseReadc()) == ' ' || c == '\t')
					continue;

				if (c == EOF)
					break;
			}

			Buf_AddByte(buf, (Byte)c);
			lastc = c;
		}

		if (c == EOF) {
			Parse_Error(PARSE_FATAL,
			    "Unclosed conditional/for loop");
			Buf_Destroy(buf, TRUE);
			return (NULL);
		}

		CURFILE->lineno++;
		Buf_AddByte(buf, (Byte)'\0');
		line = Buf_Data(buf);
	} while (skip == 1 && line[0] != '.');

	Buf_Destroy(buf, FALSE);
	return (line);
}

/*-
 *---------------------------------------------------------------------
 * ParseReadLine --
 *	Read an entire line from the input file. Called only by Parse_File.
 *	To facilitate escaped newlines and what have you, a character is
 *	buffered in 'lastc', which is '\0' when no characters have been
 *	read. When we break out of the loop, c holds the terminating
 *	character and lastc holds a character that should be added to
 *	the line (unless we don't read anything but a terminator).
 *
 * Results:
 *	A line w/o its newline
 *
 * Side Effects:
 *	Only those associated with reading a character
 *---------------------------------------------------------------------
 */
static char *
ParseReadLine(void)
{
	Buffer	*buf;		/* Buffer for current line */
	int	c;		/* the current character */
	int	lastc;		/* The most-recent character */
	Boolean	semiNL;		/* treat semi-colons as newlines */
	Boolean	ignDepOp;	/* TRUE if should ignore dependency operators
				 * for the purposes of setting semiNL */
	Boolean	ignComment;	/* TRUE if should ignore comments (in a
				 * shell command */
	char	*line;		/* Result */
	char	*ep;		/* to strip trailing blanks */
	int	lineno;		/* Saved line # */

	semiNL = FALSE;
	ignDepOp = FALSE;
	ignComment = FALSE;

	/*
	 * Handle special-characters at the beginning of the line. Either a
	 * leading tab (shell command) or pound-sign (possible conditional)
	 * forces us to ignore comments and dependency operators and treat
	 * semi-colons as semi-colons (by leaving semiNL FALSE). This also
	 * discards completely blank lines.
	 */
	for (;;) {
		c = ParseReadc();

		if (c == '\t') {
			ignComment = ignDepOp = TRUE;
			break;
		} else if (c == '\n') {
			CURFILE->lineno++;
		} else if (c == '#') {
			ParseUnreadc(c);
			break;
		} else {
			/*
			 * Anything else breaks out without doing anything
			 */
			break;
		}
	}

	if (c != EOF) {
		lastc = c;
		buf = Buf_Init(MAKE_BSIZE);

		while (((c = ParseReadc()) != '\n' || lastc == '\\') &&
		    c != EOF) {
  test_char:
			switch (c) {
			  case '\n':
				/*
				 * Escaped newline: read characters until a
				 * non-space or an unescaped newline and
				 * replace them all by a single space. This is
				 * done by storing the space over the backslash
				 * and dropping through with the next nonspace.
				 * If it is a semi-colon and semiNL is TRUE,
				 * it will be recognized as a newline in the
				 * code below this...
				 */
				CURFILE->lineno++;
				lastc = ' ';
				while ((c = ParseReadc()) == ' ' || c == '\t') {
					continue;
				}
				if (c == EOF || c == '\n') {
					goto line_read;
				} else {
					/*
					 * Check for comments, semiNL's, etc. --
					 * easier than ParseUnreadc(c);
					 * continue;
					 */
					goto test_char;
				}
				/*NOTREACHED*/
				break;

			  case ';':
				/*
				 * Semi-colon: Need to see if it should be
				 * interpreted as a newline
				 */
				if (semiNL) {
					/*
					 * To make sure the command that may
					 * be following this semi-colon begins
					 * with a tab, we push one back into the
					 * input stream. This will overwrite the
					 * semi-colon in the buffer. If there is
					 * no command following, this does no
					 * harm, since the newline remains in
					 * the buffer and the
					 * whole line is ignored.
					 */
					ParseUnreadc('\t');
					goto line_read;
				}
				break;
			  case '=':
				if (!semiNL) {
					/*
					 * Haven't seen a dependency operator
					 * before this, so this must be a
					 * variable assignment -- don't pay
					 * attention to dependency operators
					 * after this.
					 */
					ignDepOp = TRUE;
				} else if (lastc == ':' || lastc == '!') {
					/*
					 * Well, we've seen a dependency
					 * operator already, but it was the
					 * previous character, so this is really
					 * just an expanded variable assignment.
					 * Revert semi-colons to being just
					 * semi-colons again and ignore any more
					 * dependency operators.
					 *
					 * XXX: Note that a line like
					 * "foo : a:=b" will blow up, but who'd
					 * write a line like that anyway?
					 */
					ignDepOp = TRUE;
					semiNL = FALSE;
				}
				break;
			  case '#':
				if (!ignComment) {
					if (lastc != '\\') {
						/*
						 * If the character is a hash
						 * mark and it isn't escaped
						 * (or we're being compatible),
						 * the thing is a comment.
						 * Skip to the end of the line.
						 */
						do {
							c = ParseReadc();
						} while (c != '\n' && c != EOF);
						goto line_read;
					} else {
						/*
						 * Don't add the backslash.
						 * Just let the # get copied
						 * over.
						 */
						lastc = c;
						continue;
					}
				}
				break;

			  case ':':
			  case '!':
				if (!ignDepOp && (c == ':' || c == '!')) {
					/*
					 * A semi-colon is recognized as a
					 * newline only on dependency lines.
					 * Dependency lines are lines with a
					 * colon or an exclamation point.
					 * Ergo...
					 */
					semiNL = TRUE;
				}
				break;

			  default:
				break;
			}
			/*
			 * Copy in the previous character and save this one in
			 * lastc.
			 */
			Buf_AddByte(buf, (Byte)lastc);
			lastc = c;
		}
  line_read:
		CURFILE->lineno++;

		if (lastc != '\0') {
			Buf_AddByte(buf, (Byte)lastc);
		}
		Buf_AddByte(buf, (Byte)'\0');
		line = Buf_Peel(buf);

		/*
		 * Strip trailing blanks and tabs from the line.
		 * Do not strip a blank or tab that is preceded by
		 * a '\'
		 */
		ep = line;
		while (*ep)
			++ep;
		while (ep > line + 1 && (ep[-1] == ' ' || ep[-1] == '\t')) {
			if (ep > line + 1 && ep[-2] == '\\')
				break;
			--ep;
		}
		*ep = 0;

		if (line[0] == '.') {
			/*
			 * The line might be a conditional. Ask the
			 * conditional module about it and act accordingly
			 */
			switch (Cond_Eval(line, CURFILE->lineno)) {
			  case COND_SKIP:
				/*
				 * Skip to next conditional that evaluates to
				 * COND_PARSE.
				 */
				do {
					free(line);
					line = ParseSkipLine(1, 0);
				} while (line &&
				    Cond_Eval(line, CURFILE->lineno) !=
				    COND_PARSE);
				if (line == NULL)
					break;
				/*FALLTHRU*/

			  case COND_PARSE:
				free(line);
				line = ParseReadLine();
				break;

			  case COND_INVALID:
				if (For_Eval(line)) {
					int ok;
					free(line);
					lineno = CURFILE->lineno;
					do {
						/*
						 * Skip after the matching end
						 */
						line = ParseSkipLine(0, 1);
						if (line == NULL) {
							Parse_Error(PARSE_FATAL,
							    "Unexpected end of"
							    " file in for "
							    "loop.\n");
							break;
						}
						ok = For_Eval(line);
						free(line);
					} while (ok);
					if (line != NULL)
						For_Run(lineno);
					line = ParseReadLine();
				}
				break;

			  default:
				break;
			}
		}
		return (line);

	} else {
		/*
		 * Hit end-of-file, so return a NULL line to indicate this.
		 */
		return (NULL);
	}
}

/*-
 *-----------------------------------------------------------------------
 * ParseFinishLine --
 *	Handle the end of a dependency group.
 *
 * Results:
 *	Nothing.
 *
 * Side Effects:
 *	inLine set FALSE. 'targets' list destroyed.
 *
 *-----------------------------------------------------------------------
 */
static void
ParseFinishLine(void)
{
	const LstNode	*ln;

	if (inLine) {
		LST_FOREACH(ln, &targets) {
			if (((const GNode *)Lst_Datum(ln))->type & OP_TRANSFORM)
				Suff_EndTransform(Lst_Datum(ln));
		}
		Lst_Destroy(&targets, ParseHasCommands);
		inLine = FALSE;
	}
}


/*-
 *---------------------------------------------------------------------
 * Parse_File --
 *	Parse a file into its component parts, incorporating it into the
 *	current dependency graph. This is the main function and controls
 *	almost every other function in this module
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Loads. Nodes are added to the list of all targets, nodes and links
 *	are added to the dependency graph. etc. etc. etc.
 *---------------------------------------------------------------------
 */
void
Parse_File(const char *name, FILE *stream)
{
	char	*cp;	/* pointer into the line */
	char	*line;	/* the line we're working on */
	Buffer	*buf;

	inLine = FALSE;
	fatals = 0;

	ParsePushInput(estrdup(name), stream, NULL, 0);

	do {
		while ((line = ParseReadLine()) != NULL) {
			if (*line == '.') {
				/*
				 * Lines that begin with the special character
				 * are either include or undef directives.
				 */
				for (cp = line + 1; isspace((unsigned char)*cp);
				    cp++) {
					continue;
				}
				if (strncmp(cp, "include", 7) == 0) {
					ParseDoInclude(cp + 7);
					goto nextLine;
				} else if (strncmp(cp, "error", 5) == 0) {
					ParseDoError(cp + 5);
					goto nextLine;
				} else if (strncmp(cp, "warning", 7) == 0) {
					ParseDoWarning(cp + 7);
					goto nextLine;
				} else if (strncmp(cp, "undef", 5) == 0) {
					cp = stripvarname(cp + 5);
					buf = Var_Subst(NULL, cp, VAR_CMD,
					    FALSE);
					cp = Buf_Peel(buf);

					Var_Delete(cp, VAR_GLOBAL);
					goto nextLine;
				} else if (strncmp(cp, "makeenv", 7) == 0) {
					cp = stripvarname(cp + 7);
					Var_SetEnv(cp, VAR_GLOBAL);
					goto nextLine;
				}
			}
			if (*line == '#') {
				/*
				 * If we're this far, the line must be
				 * a comment.
				 */
				goto nextLine;
			}

			if (*line == '\t') {
				/*
				 * If a line starts with a tab, it can only
				 * hope to be a creation command.
				 */
				for (cp = line + 1;
				    isspace((unsigned char)*cp); cp++) {
					continue;
				}
				if (*cp) {
					if (inLine) {
						LstNode	*ln;
						GNode	*gn;

						/*
						 * So long as it's not a blank
						 * line and we're actually in a
						 * dependency spec, add the
						 * command to the list of
						 * commands of all targets in
						 * the dependency spec.
						 */
						LST_FOREACH(ln, &targets) {
							gn = Lst_Datum(ln);

							/*
							 * if target already
							 * supplied, ignore
							 * commands
							 */
							if (!(gn->type &
							    OP_HAS_COMMANDS))
								Lst_AtEnd(&gn->commands, cp);
							else
								Parse_Error(PARSE_WARNING, "duplicate script "
								    "for target \"%s\" ignored", gn->name);
						}
						continue;
					} else {
						Parse_Error(PARSE_FATAL,
						     "Unassociated shell command \"%s\"",
						     cp);
					}
				}
#ifdef SYSVINCLUDE
			} else if (strncmp(line, "include", 7) == 0 &&
			    isspace((unsigned char)line[7]) &&
			    strchr(line, ':') == NULL) {
				/*
				 * It's an S3/S5-style "include".
				 */
				ParseTraditionalInclude(line + 7);
				goto nextLine;
#endif
			} else if (Parse_IsVar(line)) {
				ParseFinishLine();
				Parse_DoVar(line, VAR_GLOBAL);

			} else {
				/*
				 * We now know it's a dependency line so it
				 * needs to have all variables expanded before
				 * being parsed. Tell the variable module to
				 * complain if some variable is undefined...
				 * To make life easier on novices, if the line
				 * is indented we first make sure the line has
				 * a dependency operator in it. If it doesn't
				 * have an operator and we're in a dependency
				 * line's script, we assume it's actually a
				 * shell command and add it to the current
				 * list of targets.
				 */
				cp = line;
				if (isspace((unsigned char)line[0])) {
					while (*cp != '\0' &&
					    isspace((unsigned char)*cp)) {
						cp++;
					}
					if (*cp == '\0') {
						goto nextLine;
					}
				}

				ParseFinishLine();

				buf = Var_Subst(NULL, line, VAR_CMD, TRUE);
				cp = Buf_Peel(buf);

				free(line);
				line = cp;

				/*
				 * Need a non-circular list for the target nodes
				 */
				Lst_Destroy(&targets, NOFREE);
				inLine = TRUE;

				ParseDoDependency(line);
			}

    nextLine:
			free(line);
		}

		/*
		 * Reached EOF, but it may be just EOF of an include file...
		 */
	} while (ParsePopInput() == CONTINUE);

	ParseFinishLine();

	/*
	 * Make sure conditionals are clean
	 */
	Cond_End();

	if (fatals)
		errx(1, "fatal errors encountered -- cannot continue");
}

/*-
 *---------------------------------------------------------------------
 * Parse_Init --
 *	initialize the parsing module
 *
 * Results:
 *	none
 *
 * Side Effects:
 *	the parseIncPath list is initialized...
 *---------------------------------------------------------------------
 */
void
Parse_Init(void)
{

	mainNode = NULL;
}

/*-
 *-----------------------------------------------------------------------
 * Parse_MainName --
 *	Return a Lst of the main target to create for main()'s sake. If
 *	no such target exists, we Punt with an obnoxious error message.
 *
 * Results:
 *	A Lst of the single node to create.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
void
Parse_MainName(Lst *listmain)
{

	if (mainNode == NULL) {
		Punt("no target to make.");
		/*NOTREACHED*/
	} else if (mainNode->type & OP_DOUBLEDEP) {
		Lst_AtEnd(listmain, mainNode);
		Lst_Concat(listmain, &mainNode->cohorts, LST_CONCNEW);
	} else
		Lst_AtEnd(listmain, mainNode);
}
