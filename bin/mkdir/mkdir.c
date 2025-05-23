/*
 * Copyright (c) 1983, 1992, 1993
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
 * @(#) Copyright (c) 1983, 1992, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)mkdir.c	8.2 (Berkeley) 1/25/94
 * $FreeBSD: src/bin/mkdir/mkdir.c,v 1.19.2.2 2001/08/01 04:42:37 obrien Exp $
 * $DragonFly: src/bin/mkdir/mkdir.c,v 1.6 2004/11/07 20:54:51 eirikn Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static int	build(char *, mode_t);
static void	usage(void);

int vflag;

int
main(int argc, char **argv)
{
	int ch, exitval, success, pflag;
	void *set;
	mode_t omode;
	char *mode;

	omode = pflag = 0;
	mode = NULL;
	while ((ch = getopt(argc, argv, "m:pv")) != -1)
		switch(ch) {
		case 'm':
			mode = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;
	if (argv[0] == NULL)
		usage();

	if (mode == NULL) {
		omode = S_IRWXU | S_IRWXG | S_IRWXO;
	} else {
		errno = 0;
		if ((set = setmode(mode)) == NULL) {
			if (!errno)
				errx(1, "invalid file mode: %s", mode);
			else
				/* A malloc(3) error, not a invaild file mode. */	
				err(1, "setmode failed");
		}
		omode = getmode(set, S_IRWXU | S_IRWXG | S_IRWXO);
		free(set);
	}

	for (exitval = 0; *argv != NULL; ++argv) {
		success = 1;
		if (pflag) {
			if (build(*argv, omode))
				success = 0;
		} else if (mkdir(*argv, omode) < 0) {
			if (errno == ENOTDIR || errno == ENOENT)
				warn("%s", dirname(*argv));
			else
				warn("%s", *argv);
			success = 0;
		} else if (vflag)
			printf("%s\n", *argv);
		
		if (!success)
			exitval = 1;
		/*
		 * The mkdir() and umask() calls both honor only the low
		 * nine bits, so if you try to set a mode including the
		 * sticky, setuid, setgid bits you lose them.  Don't do
		 * this unless the user has specifically requested a mode,
		 * as chmod will (obviously) ignore the umask.
		 */
		if (success && mode != NULL && chmod(*argv, omode) == -1) {
			warn("%s", *argv);
			exitval = 1;
		}
	}
	exit(exitval);
}

static int
build(char *path, mode_t omode)
{
	struct stat sb;
	mode_t numask, oumask;
	int first, last, retval;
	char *p;

	p = path;
	oumask = 0;
	retval = 0;
	if (p[0] == '/')		/* Skip leading '/'. */
		++p;
	for (first = 1, last = 0; !last ; ++p) {
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != '/')
			continue;
		*p = '\0';
		if (p[1] == '\0')
			last = 1;
		if (first) {
			/*
			 * POSIX 1003.2:
			 * For each dir operand that does not name an existing
			 * directory, effects equivalent to those cased by the
			 * following command shall occcur:
			 *
			 * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
			 *    mkdir [-m mode] dir
			 *
			 * We change the user's umask and then restore it,
			 * instead of doing chmod's.
			 */
			oumask = umask(0);
			numask = oumask & ~(S_IWUSR | S_IXUSR);
			umask(numask);
			first = 0;
		}
		if (last)
			umask(oumask);
		if (stat(path, &sb)) {
			if (errno != ENOENT ||
			    mkdir(path, last ? omode : 
				  S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
				warn("%s", path);
				retval = 1;
				break;
			} else if (vflag)
				printf("%s\n", path);
		}
		else if ((sb.st_mode & S_IFMT) != S_IFDIR) {
			if (last)
				errno = EEXIST;
			else
				errno = ENOTDIR;
			warn("%s", path);
			retval = 1;
			break;
		}
		if (!last)
		    *p = '/';
	}
	if (!first && !last)
		umask(oumask);
	return (retval);
}

static void
usage(void)
{

	fprintf(stderr, "usage: mkdir [-pv] [-m mode] directory ...\n");
	exit (EX_USAGE);
}
