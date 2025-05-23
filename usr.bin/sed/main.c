/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
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
 * @(#) Copyright (c) 1992, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)main.c	8.2 (Berkeley) 1/3/94
 * $FreeBSD: src/usr.bin/sed/main.c,v 1.9.2.7 2002/08/06 10:03:29 fanf Exp $
 * $DragonFly: src/usr.bin/sed/main.c,v 1.3 2003/10/04 20:36:50 hmp Exp $
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "extern.h"

/*
 * Linked list of units (strings and files) to be compiled
 */
struct s_compunit {
	struct s_compunit *next;
	enum e_cut {CU_FILE, CU_STRING} type;
	char *s;			/* Pointer to string or fname */
};

/*
 * Linked list pointer to compilation units and pointer to current
 * next pointer.
 */
static struct s_compunit *script, **cu_nextp = &script;

/*
 * Linked list of files to be processed
 */
struct s_flist {
	char *fname;
	struct s_flist *next;
};

/*
 * Linked list pointer to files and pointer to current
 * next pointer.
 */
static struct s_flist *files, **fl_nextp = &files;

static FILE *curfile;		/* Current open file */

int aflag, eflag, nflag;
int rflags = 0;
static int rval;		/* Exit status */

/*
 * Current file and line number; line numbers restart across compilation
 * units, but span across input files.
 */
const char *fname;		/* File name. */
const char *inplace;		/* Inplace edit file extension. */
u_long linenum;

static void add_compunit(enum e_cut, char *);
static void add_file(char *);
static int inplace_edit(char **);
static void usage(void);

int
main(int argc, char **argv)
{
	int c, fflag;
	char *temp_arg;

	(void) setlocale(LC_ALL, "");

	fflag = 0;
	inplace = NULL;

	while ((c = getopt(argc, argv, "Eae:f:i:n")) != -1)
		switch (c) {
		case 'E':
			rflags = REG_EXTENDED;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'e':
			eflag = 1;
			if ((temp_arg = malloc(strlen(optarg) + 2)) == NULL)
				err(1, "malloc");
			strcpy(temp_arg, optarg);
			strcat(temp_arg, "\n");
			add_compunit(CU_STRING, temp_arg);
			break;
		case 'f':
			fflag = 1;
			add_compunit(CU_FILE, optarg);
			break;
		case 'i':
			inplace = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		default:
		case '?':
			usage();
		}
	argc -= optind;
	argv += optind;

	/* First usage case; script is the first arg */
	if (!eflag && !fflag && *argv) {
		add_compunit(CU_STRING, *argv);
		argv++;
	}

	compile();

	/* Continue with first and start second usage */
	if (*argv)
		for (; *argv; argv++)
			add_file(*argv);
	else
		add_file(NULL);
	process();
	cfclose(prog, NULL);
	if (fclose(stdout))
		err(1, "stdout");
	exit(rval);
}

static void
usage(void)
{
	(void)fprintf(stderr, "%s\n%s\n",
		"usage: sed script [-Ean] [-i extension] [file ...]",
		"       sed [-an] [-i extension] [-e script] ... [-f script_file] ... [file ...]");
	exit(1);
}

/*
 * Like fgets, but go through the chain of compilation units chaining them
 * together.  Empty strings and files are ignored.
 */
char *
cu_fgets(char *buf, int n, int *more)
{
	static enum {ST_EOF, ST_FILE, ST_STRING} state = ST_EOF;
	static FILE *f;		/* Current open file */
	static char *s;		/* Current pointer inside string */
	static char string_ident[30];
	char *p;

again:
	switch (state) {
	case ST_EOF:
		if (script == NULL) {
			if (more != NULL)
				*more = 0;
			return (NULL);
		}
		linenum = 0;
		switch (script->type) {
		case CU_FILE:
			if ((f = fopen(script->s, "r")) == NULL)
				err(1, "%s", script->s);
			fname = script->s;
			state = ST_FILE;
			goto again;
		case CU_STRING:
			if ((snprintf(string_ident,
			    sizeof(string_ident), "\"%s\"", script->s)) >=
			    sizeof(string_ident) - 1)
				(void)strcpy(string_ident +
				    sizeof(string_ident) - 6, " ...\"");
			fname = string_ident;
			s = script->s;
			state = ST_STRING;
			goto again;
		}
	case ST_FILE:
		if ((p = fgets(buf, n, f)) != NULL) {
			linenum++;
			if (linenum == 1 && buf[0] == '#' && buf[1] == 'n')
				nflag = 1;
			if (more != NULL)
				*more = !feof(f);
			return (p);
		}
		script = script->next;
		(void)fclose(f);
		state = ST_EOF;
		goto again;
	case ST_STRING:
		if (linenum == 0 && s[0] == '#' && s[1] == 'n')
			nflag = 1;
		p = buf;
		for (;;) {
			if (n-- <= 1) {
				*p = '\0';
				linenum++;
				if (more != NULL)
					*more = 1;
				return (buf);
			}
			switch (*s) {
			case '\0':
				state = ST_EOF;
				if (s == script->s) {
					script = script->next;
					goto again;
				} else {
					script = script->next;
					*p = '\0';
					linenum++;
					if (more != NULL)
						*more = 0;
					return (buf);
				}
			case '\n':
				*p++ = '\n';
				*p = '\0';
				s++;
				linenum++;
				if (more != NULL)
					*more = 0;
				return (buf);
			default:
				*p++ = *s++;
			}
		}
	}
	/* NOTREACHED */
	return (NULL);
}

/*
 * Like fgets, but go through the list of files chaining them together.
 * Set len to the length of the line.
 */
int
mf_fgets(SPACE *sp, enum e_spflag spflag)
{
	size_t len;
	char *p;
	int c;
	static int firstfile;

	if (curfile == NULL) {
		/* stdin? */
		if (files->fname == NULL) {
			if (inplace != NULL)
				errx(1, "-i may not be used with stdin");
			curfile = stdin;
			fname = "stdin";
		}
		firstfile = 1;
	}

	for (;;) {
		if (curfile != NULL && (c = getc(curfile)) != EOF) {
			(void)ungetc(c, curfile);
			break;
		}
		/* If we are here then either eof or no files are open yet */
		if (curfile == stdin) {
			sp->len = 0;
			return (0);
		}
		if (curfile != NULL) {
			fclose(curfile);
		}
		if (firstfile == 0) {
			files = files->next;
		} else
			firstfile = 0;
		if (files == NULL) {
			sp->len = 0;
			return (0);
		}
		if (inplace != NULL) {
			if (inplace_edit(&files->fname) == -1)
				continue;
		}
		fname = files->fname;
		if ((curfile = fopen(fname, "r")) == NULL) {
			warn("%s", fname);
			rval = 1;
			continue;
		}
		if (inplace != NULL && *inplace == '\0')
			unlink(fname);
	}
	/*
	 * We are here only when curfile is open and we still have something
	 * to read from it.
	 *
	 * Use fgetln so that we can handle essentially infinite input data.
	 * Can't use the pointer into the stdio buffer as the process space
	 * because the ungetc() can cause it to move.
	 */
	p = fgetln(curfile, &len);
	if (ferror(curfile))
		errx(1, "%s: %s", fname, strerror(errno ? errno : EIO));
	if (len != 0 && p[len - 1] == '\n')
		len--;
	cspace(sp, p, len, spflag);

	linenum++;

	return (1);
}

/*
 * Add a compilation unit to the linked list
 */
static void
add_compunit(enum e_cut type, char *s)
{
	struct s_compunit *cu;

	if ((cu = malloc(sizeof(struct s_compunit))) == NULL)
		err(1, "malloc");
	cu->type = type;
	cu->s = s;
	cu->next = NULL;
	*cu_nextp = cu;
	cu_nextp = &cu->next;
}

/*
 * Add a file to the linked list
 */
static void
add_file(char *s)
{
	struct s_flist *fp;

	if ((fp = malloc(sizeof(struct s_flist))) == NULL)
		err(1, "malloc");
	fp->next = NULL;
	*fl_nextp = fp;
	fp->fname = s;
	fl_nextp = &fp->next;
}

/*
 * Modify a pointer to a filename for inplace editing and reopen stdout
 */
static int
inplace_edit(char **filename)
{
	struct stat orig;
	char backup[MAXPATHLEN];

	if (lstat(*filename, &orig) == -1)
		err(1, "lstat");
	if ((orig.st_mode & S_IFREG) == 0) {
		warnx("cannot inplace edit %s, not a regular file", *filename);
		return -1;
	}

	if (*inplace == '\0') {
		/*
		 * This is a bit of a hack: we use mkstemp() to avoid the
		 * mktemp() link-time warning, although mktemp() would fit in
		 * this context much better. We're only interested in getting
		 * a name for use in the rename(); there aren't any security
		 * issues here that don't already exist in relation to the
		 * original file and its directory.
		 */
		int fd;
		strlcpy(backup, *filename, sizeof(backup));
		strlcat(backup, ".XXXXXXXXXX", sizeof(backup));
		fd = mkstemp(backup);
		if (fd == -1)
			errx(1, "could not create backup of %s", *filename);
		else
			close(fd);
	} else {
		strlcpy(backup, *filename, sizeof(backup));
		strlcat(backup, inplace, sizeof(backup));
	}

	if (rename(*filename, backup) == -1)
		err(1, "rename(\"%s\", \"%s\")", *filename, backup);
	if (freopen(*filename, "w", stdout) == NULL)
		err(1, "open(\"%s\")", *filename);
	if (fchmod(fileno(stdout), orig.st_mode) == -1)
		err(1, "chmod(\"%s\")", *filename);
	*filename = strdup(backup);
	if (*filename == NULL)
		err(1, "malloc");
	return 0;
}

int
lastline(void)
{
	int ch;

	if (files->next != NULL)
		return (0);
	if ((ch = getc(curfile)) == EOF)
		return (1);
	ungetc(ch, curfile);
	return (0);
}
