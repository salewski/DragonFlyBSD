/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 * @(#) Copyright (c) 1987, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)ln.c	8.2 (Berkeley) 3/31/94
 * $FreeBSD: src/bin/ln/ln.c,v 1.15.2.4 2002/07/12 07:34:38 tjr Exp $
 * $DragonFly: src/bin/ln/ln.c,v 1.9 2005/03/16 07:08:04 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int	fflag;				/* Unlink existing files. */
static int	hflag;				/* Check new name for symlink first. */
static int	iflag;				/* Interactive mode. */
static int	sflag;				/* Symbolic, not hard, link. */
static int	vflag;				/* Verbose output. */
					/* System link call. */
static int	(*linkf)(const char *, const char *);
static char	linkch;

static int	linkit(const char *, const char *, int);
static void	usage(void);

int
main(int argc, char *argv[])
{
	struct stat sb;
	char *p, *sourcedir;
	int ch, exitval;

	/*
	 * Test for the special case where the utility is called as
	 * "link", for which the functionality provided is greatly
	 * simplified.
	 */
	if ((p = strrchr(argv[0], '/')) == NULL)
		p = argv[0];
	else
		++p;
	if (strcmp(p, "link") == 0) {
		while (getopt(argc, argv, "") != -1)
			usage();
		argc -= optind;
		argv += optind;
		if (argc != 2)
			usage();
		linkf = link;
		exit(linkit(argv[0], argv[1], 0));
	}

	while ((ch = getopt(argc, argv, "fhinsv")) != -1) {
		switch (ch) {
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'h':
		case 'n':
			hflag = 1;
			break;
		case 'i':
			iflag = 1;
			fflag = 0;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	linkf = sflag ? symlink : link;
	linkch = sflag ? '-' : '=';

	switch (argc) {
	case 0:
		usage();
		/* NOTREACHED */
	case 1:				/* ln target */
		exit(linkit(argv[0], ".", 1));
	case 2:				/* ln target source */
		exit(linkit(argv[0], argv[1], 0));
	default:
		;
	}
					/* ln target1 target2 directory */
	sourcedir = argv[argc - 1];
	if (hflag && lstat(sourcedir, &sb) == 0 && S_ISLNK(sb.st_mode)) {
		/*
		 * We were asked not to follow symlinks, but found one at
		 * the target--simulate "not a directory" error
		 */
		errc(1, ENOTDIR, "%s", sourcedir);
	}
	if (stat(sourcedir, &sb) != 0)
		err(1, "%s", sourcedir);
	if (!S_ISDIR(sb.st_mode))
		usage();
	for (exitval = 0; *argv != sourcedir; ++argv)
		exitval |= linkit(*argv, sourcedir, 1);
	exit(exitval);
}

static int
linkit(const char *target, const char *source, int isdir)
{
	struct stat sb;
	const char *p;
	int ch, exists, first;
	char path[PATH_MAX];

	if (!sflag) {
		/* If target doesn't exist, quit now. */
		if (stat(target, &sb) != 0) {
			warn("%s", target);
			return (1);
		}
		/* Only symbolic links to directories. */
		if (S_ISDIR(sb.st_mode)) {
			errno = EISDIR;
			warn("%s", target);
			return (1);
		}
	}

	/*
	 * If the source is a directory (and not a symlink if hflag),
	 * append the target's name.
	 */
	if (isdir ||
	    (lstat(source, &sb) == 0 && S_ISDIR(sb.st_mode)) ||
	    (!hflag && stat(source, &sb) == 0 && S_ISDIR(sb.st_mode))) {
		if ((p = strrchr(target, '/')) == NULL)
			p = target;
		else
			++p;
		if (snprintf(path, sizeof(path), "%s/%s", source, p) >=
		    (int)sizeof(path)) {
			errno = ENAMETOOLONG;
			warn("%s", target);
			return (1);
		}
		source = path;
	}

	exists = (lstat(source, &sb) == 0);
	/*
	 * If the file exists, then unlink it forcibly if -f was specified
	 * and interactively if -i was specified.
	 */
	if (fflag && exists) {
		if (unlink(source) != 0) {
			warn("%s", source);
			return (1);
		}
	} else if (iflag && exists) {
		fflush(stdout);
		fprintf(stderr, "replace %s? ", source);

		first = ch = getchar();
		while(ch != '\n' && ch != EOF)
			ch = getchar();
		if (first != 'y' && first != 'Y') {
			fprintf(stderr, "not replaced\n");
			return (1);
		}

		if (unlink(source) != 0) {
			warn("%s", source);
			return (1);
		}
	}

	/* Attempt the link. */
	if ((*linkf)(target, source) != 0) {
		warn("%s", source);
		return (1);
	}
	if (vflag)
		printf("%s %c> %s\n", source, linkch, target);
	return (0);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: ln [-fhinsv] file1 file2",
	    "       ln [-fhinsv] file ... directory",
	    "       link file1 file2");
	exit(1);
}
