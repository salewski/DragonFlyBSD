/*
 * Copyright (c) 1992, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *      @(#)mount_cd9660.c	8.7 (Berkeley) 5/1/95
 *
 * @(#) Copyright (c) 1992, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)mount_cd9660.c	8.7 (Berkeley) 5/1/95
 * $FreeBSD: src/sbin/mount_cd9660/mount_cd9660.c,v 1.15.2.3 2001/03/14 12:05:01 bp Exp $
 * $DragonFly: src/sbin/mount_cd9660/mount_cd9660.c,v 1.5 2005/04/02 21:43:15 dillon Exp $
 */

#include <sys/cdio.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/../vfs/isofs/cd9660/cd9660_mount.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ "extatt", 0, ISOFSMNT_EXTATT, 1 },
	{ "gens", 0, ISOFSMNT_GENS, 1 },
	{ "rrip", 1, ISOFSMNT_NORRIP, 1 },
	{ "joliet", 1, ISOFSMNT_NOJOLIET, 1 },
	{ "strictjoliet", 1, ISOFSMNT_BROKENJOLIET, 1 },
	MOPT_NULL
};

static int	get_ssector(const char *dev);
static void	usage(void);

int
main(int argc, char **argv)
{
	struct iso_args args;
	int ch, mntflags, opts;
	char *dev, *dir, mntpath[MAXPATHLEN];
	struct vfsconf vfc;
	int error, verbose;

	mntflags = opts = verbose = 0;
	memset(&args, 0, sizeof args);
	args.ssector = -1;
	while ((ch = getopt(argc, argv, "begjo:rs:v")) != -1)
		switch (ch) {
		case 'b':
			opts |= ISOFSMNT_BROKENJOLIET;
			break;
		case 'e':
			opts |= ISOFSMNT_EXTATT;
			break;
		case 'g':
			opts |= ISOFSMNT_GENS;
			break;
		case 'j':
			opts |= ISOFSMNT_NOJOLIET;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &opts);
			break;
		case 'r':
			opts |= ISOFSMNT_NORRIP;
			break;
		case 's':
			args.ssector = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	dev = argv[0];
	dir = argv[1];

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	checkpath(dir, mntpath);
	rmslashes(dev, dev);

#define DEFAULT_ROOTUID	-2
	/*
	 * ISO 9660 filesystems are not writeable.
	 */
	mntflags |= MNT_RDONLY;
	args.export.ex_flags = MNT_EXRDONLY;
	args.fspec = dev;
	args.export.ex_root = DEFAULT_ROOTUID;
	args.flags = opts;

	if (args.ssector == -1) {
		/*
		 * The start of the session has not been specified on
		 * the command line.  If we can successfully read the
		 * TOC of a CD-ROM, use the last data track we find.
		 * Otherwise, just use 0, in order to mount the very
		 * first session.  This is compatible with the
		 * historic behaviour of mount_cd9660(8).  If the user
		 * has specified -s <ssector> above, we don't get here
		 * and leave the user's will.
		 */
		if ((args.ssector = get_ssector(dev)) == -1) {
			if (verbose)
				printf("could not determine starting sector, "
				       "using very first session\n");
			args.ssector = 0;
		} else if (verbose)
			printf("using starting sector %d\n", args.ssector);
	}

	error = getvfsbyname("cd9660", &vfc);
	if (error && vfsisloadable("cd9660")) {
		if (vfsload("cd9660"))
			err(EX_OSERR, "vfsload(cd9660)");
		endvfsent();	/* flush cache */
		error = getvfsbyname("cd9660", &vfc);
	}
	if (error)
		errx(1, "cd9660 filesystem is not available");

	if (mount(vfc.vfc_name, mntpath, mntflags, &args) < 0)
		err(1, "%s", args.fspec);
	exit(0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: mount_cd9660 [-egrv] [-o options] [-s startsector] special node\n");
	exit(EX_USAGE);
}

static int
get_ssector(const char *dev)
{
	struct ioc_toc_header h;
	struct ioc_read_toc_entry t;
	struct cd_toc_entry toc_buffer[100];
	int fd, ntocentries, i;

	if ((fd = open(dev, O_RDONLY)) == -1)
		return -1;
	if (ioctl(fd, CDIOREADTOCHEADER, &h) == -1) {
		close(fd);
		return -1;
	}

	ntocentries = h.ending_track - h.starting_track + 1;
	if (ntocentries > 100) {
		/* unreasonable, only 100 allowed */
		close(fd);
		return -1;
	}
	t.address_format = CD_LBA_FORMAT;
	t.starting_track = 0;
	t.data_len = ntocentries * sizeof(struct cd_toc_entry);
	t.data = toc_buffer;

	if (ioctl(fd, CDIOREADTOCENTRYS, (char *) &t) == -1) {
		close(fd);
		return -1;
	}
	close(fd);
	
	for (i = ntocentries - 1; i >= 0; i--)
		if ((toc_buffer[i].control & 4) != 0)
			/* found a data track */
			break;
	if (i < 0)
		return -1;

	return ntohl(toc_buffer[i].addr.lba);
}
