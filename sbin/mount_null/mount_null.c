/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 * @(#) Copyright (c) 1992, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)mount_null.c	8.6 (Berkeley) 4/26/95
 * $FreeBSD: src/sbin/mount_null/mount_null.c,v 1.13 1999/10/09 11:54:11 phk Exp $
 * $DragonFly: src/sbin/mount_null/mount_null.c,v 1.7 2005/04/02 22:00:18 dillon Exp $
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <vfs/nullfs/null.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_NULL
};

static int	subdir(const char *, const char *);
static void	usage(void) __dead2;

int
main(int argc, char **argv)
{
	struct null_args args;
	int ch, mntflags;
	char source[MAXPATHLEN];
	char target[MAXPATHLEN];
	struct vfsconf vfc;
	int error;

	mntflags = 0;
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch(ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	/* resolve target and source with realpath(3) */
	checkpath(argv[0], target);
	checkpath(argv[1], source);

	if (subdir(target, source) || subdir(source, target))
		errx(EX_USAGE, "%s (%s) and %s are not distinct paths",
		    argv[0], target, argv[1]);

	args.target = target;

	error = getvfsbyname("null", &vfc);
	if (error && vfsisloadable("null")) {
		if(vfsload("null"))
			err(EX_OSERR, "vfsload(null)");
		endvfsent();
		error = getvfsbyname("null", &vfc);
	}
	if (error)
		errx(EX_OSERR, "null/loopback filesystem is not available");

	if (mount(vfc.vfc_name, source, mntflags, &args))
		err(1, NULL);
	exit(0);
}

static int
subdir(const char *p, const char *dir)
{
	int l;

	l = strlen(dir);
	if (l <= 1)
		return (1);

	if ((strncmp(p, dir, l) == 0) && (p[l] == '/' || p[l] == '\0'))
		return (1);

	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: mount_null [-o options] target_fs mount_point\n");
	exit(1);
}
