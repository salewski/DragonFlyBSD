/*
 * Copyright (c) 1989, 1993, 1994
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
 * @(#) Copyright (c) 1989, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)chmod.c	8.8 (Berkeley) 4/1/94
 * $FreeBSD: src/bin/chmod/chmod.c,v 1.16.2.6 2002/10/18 01:36:38 trhodes Exp $
 * $DragonFly: src/bin/chmod/chmod.c,v 1.8 2004/10/31 16:18:37 liamfoy Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage (void);

int
main(int argc, char **argv)
{
	FTS *ftsp;
	FTSENT *p;
	mode_t newmode;
	void *set;
	int Hflag, Lflag, Pflag, Rflag, ch, fflag;
	int fts_options, hflag, rval, vflag;
	char *mode;
	int (*change_mode) (const char *, mode_t);

	Hflag = Lflag = Pflag = Rflag = fflag = hflag = vflag = 0;
	while ((ch = getopt(argc, argv, "HLPRXfghorstuvwx")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V (and probably POSIX.2) the -h option
			 * causes chmod to change the mode of the symbolic
			 * link.  4.4BSD's symbolic links didn't have modes,
			 * so it was an undocumented noop.  In FreeBSD 3.0,
			 * lchmod(2) is introduced and this option does real
			 * work.
			 */
			hflag = 1;
			break;
		/*
		 * XXX
		 * "-[rwx]" are valid mode commands.  If they are the entire
		 * argument, getopt has moved past them, so decrement optind.
		 * Regardless, we're done argument processing.
		 */
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
		}
done:	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	fts_options = FTS_PHYSICAL;
	if (Rflag) {
		if (hflag)
			errx(1,
		"the -R and -h options may not be specified together.");
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	}

	if (hflag)
		change_mode = lchmod;
	else
		change_mode = chmod;

	mode = *argv;
	errno = 0;
	if ((set = setmode(mode)) == NULL) {
		if (!errno)
			errx(1, "invalid file mode: %s", mode);
		else
			/* malloc for setmode() failed */
			err(1, "setmode failed");
	}

	if ((ftsp = fts_open(++argv, fts_options, 0)) == NULL)
		err(1, NULL);
	for (rval = 0; (p = fts_read(ftsp)) != NULL;) {
		switch (p->fts_info) {
		case FTS_D:			/* Change it at FTS_DP. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chmod, continue. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:			/* Ignore. */
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			if (!hflag)
				continue;
			/* else */
			/* FALLTHROUGH */
		default:
			break;
		}
		newmode = getmode(set, p->fts_statp->st_mode);
		if ((newmode & ALLPERMS) == (p->fts_statp->st_mode & ALLPERMS))
			continue;
		if ((*change_mode)(p->fts_accpath, newmode) && !fflag) {
			warn("%s", p->fts_path);
			rval = 1;
		} else {
		    	if (vflag)
				printf("%s\n", p->fts_accpath);
		}
	}
	if (errno)
		err(1, "fts_read");
	free(set);
	exit(rval);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: chmod [-fhv] [-R [-H | -L | -P]] mode file ...\n");
	exit(1);
}
