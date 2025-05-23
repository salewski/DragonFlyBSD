/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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
 * @(#) Copyright (c) 1980, 1991, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)main.c	8.6 (Berkeley) 5/1/95
 * $FreeBSD: src/sbin/dump/main.c,v 1.20.2.9 2003/01/25 18:54:59 dillon Exp $
 * $DragonFly: src/sbin/dump/main.c,v 1.9 2004/12/27 22:36:37 liamfoy Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef sunos
#include <sys/vnode.h>

#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <vfs/ufs/dinode.h>
#include <vfs/ufs/fs.h>
#endif

#include <protocols/dumprestore.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dump.h"
#include "pathnames.h"

#ifndef SBOFF
#define SBOFF (SBLOCK * DEV_BSIZE)
#endif

int	notify = 0;	/* notify operator flag */
int	blockswritten = 0;	/* number of blocks written on current tape */
int	tapeno = 0;	/* current tape number */
int	density = 0;	/* density in bytes/0.1" " <- this is for hilit19 */
int	ntrec = NTREC;	/* # tape blocks in each tape record */
int	cartridge = 0;	/* Assume non-cartridge tape */
int	dokerberos = 0;	/* Use Kerberos authentication */
int	cachesize = 0;	/* block cache size (in bytes) */
long	dev_bsize = 1;	/* recalculated below */
long	blocksperfile;	/* output blocks per file */
char	*host = NULL;	/* remote host (if any) */

static long numarg(char *, long, long);
static void obsolete(int *, char **[]);
static void usage(void);

int
main(int argc, char **argv)
{
	struct stat sb;
	ino_t ino;
	int dirty;
	struct dinode *dp;
	struct	fstab *dt;
	char *map;
	int ch;
	int i, anydirskipped, bflag = 0, Tflag = 0, honorlevel = 1;
	int just_estimate = 0;
	ino_t maxino;

	spcl.c_date = 0;
	time((time_t *)&spcl.c_date);

	tsize = 0;	/* Default later, based on 'c' option for cart tapes */
	if ((tape = getenv("TAPE")) == NULL)
		tape = _PATH_DEFTAPE;
	dumpdates = _PATH_DUMPDATES;
	temp = _PATH_DTMP;
	if (TP_BSIZE / DEV_BSIZE == 0 || TP_BSIZE % DEV_BSIZE != 0)
		quit("TP_BSIZE must be a multiple of DEV_BSIZE\n");
	level = '0';

	if (argc < 2)
		usage();

	obsolete(&argc, &argv);
#ifdef KERBEROS
#define optstring "0123456789aB:b:cd:f:h:kns:ST:uWwD:C:"
#else
#define optstring "0123456789aB:b:cd:f:h:ns:ST:uWwD:C:"
#endif
	while ((ch = getopt(argc, argv, optstring)) != -1)
#undef optstring
		switch (ch) {
		/* dump level */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			level = ch;
			break;

		case 'a':		/* `auto-size', Write to EOM. */
			unlimited = 1;
			break;

		case 'B':		/* blocks per output file */
			blocksperfile = numarg("number of blocks per file",
			    1L, 0L);
			break;

		case 'b':		/* blocks per tape write */
			ntrec = numarg("number of blocks per write",
			    1L, 1000L);
			break;

		case 'c':		/* Tape is cart. not 9-track */
			cartridge = 1;
			break;

		case 'd':		/* density, in bits per inch */
			density = numarg("density", 10L, 327670L) / 10;
			if (density >= 625 && !bflag)
				ntrec = HIGHDENSITYTREC;
			break;

		case 'f':		/* output file */
			tape = optarg;
			break;

		case 'D':
			dumpdates = optarg;
			break;

		case 'C':
			cachesize = numarg("cachesize", 0, 0) * 1024 * 1024;
			break;

		case 'h':
			honorlevel = numarg("honor level", 0L, 10L);
			break;

#ifdef KERBEROS
		case 'k':
			dokerberos = 1;
			break;
#endif

		case 'n':		/* notify operators */
			notify = 1;
			break;

		case 's':		/* tape size, feet */
			tsize = numarg("tape size", 1L, 0L) * 12 * 10;
			break;

		case 'S':               /* exit after estimating # of tapes */
			just_estimate = 1;
			break;

		case 'T':		/* time of last dump */
			spcl.c_ddate = unctime(optarg);
			if (spcl.c_ddate < 0) {
				fprintf(stderr, "bad time \"%s\"\n", optarg);
				exit(X_STARTUP);
			}
			Tflag = 1;
			lastlevel = '?';
			break;

		case 'u':		/* update /etc/dumpdates */
			uflag = 1;
			break;

		case 'W':		/* what to do */
		case 'w':
			lastdump(ch);
			exit(X_FINOK);	/* do nothing else */

		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "Must specify disk or filesystem\n");
		exit(X_STARTUP);
	}
	disk = *argv++;
	argc--;
	if (argc >= 1) {
		fprintf(stderr, "Unknown arguments to dump:");
		while (argc--)
			fprintf(stderr, " %s", *argv++);
		fprintf(stderr, "\n");
		exit(X_STARTUP);
	}
	if (Tflag && uflag) {
	        fprintf(stderr, "You cannot use the T and u flags together.\n");
		exit(X_STARTUP);
	}
	if (strcmp(tape, "-") == 0) {
		pipeout++;
		tape = "standard output";
	}

	if (blocksperfile)
		blocksperfile = blocksperfile / ntrec * ntrec; /* round down */
	else if (!unlimited) {
		/*
		 * Determine how to default tape size and density
		 *
		 *         	density				tape size
		 * 9-track	1600 bpi (160 bytes/.1")	2300 ft.
		 * 9-track	6250 bpi (625 bytes/.1")	2300 ft.
		 * cartridge	8000 bpi (100 bytes/.1")	1700 ft.
		 *						(450*4 - slop)
		 * hilit19 hits again: "
		 */
		if (density == 0)
			density = cartridge ? 100 : 160;
		if (tsize == 0)
			tsize = cartridge ? 1700L*120L : 2300L*120L;
	}

	if (strchr(tape, ':')) {
		host = tape;
		tape = strchr(host, ':');
		*tape++ = '\0';
#ifdef RDUMP
		if (strchr(tape, '\n')) {
		    fprintf(stderr, "invalid characters in tape\n");
		    exit(X_STARTUP);
		}
		if (rmthost(host) == 0)
			exit(X_STARTUP);
#else
		fprintf(stderr, "remote dump not enabled\n");
		exit(X_STARTUP);
#endif
	}
	setuid(getuid()); /* rmthost() is the only reason to be setuid */

	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, sig);
	if (signal(SIGTRAP, SIG_IGN) != SIG_IGN)
		signal(SIGTRAP, sig);
	if (signal(SIGFPE, SIG_IGN) != SIG_IGN)
		signal(SIGFPE, sig);
	if (signal(SIGBUS, SIG_IGN) != SIG_IGN)
		signal(SIGBUS, sig);
	if (signal(SIGSEGV, SIG_IGN) != SIG_IGN)
		signal(SIGSEGV, sig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, sig);
	if (signal(SIGINT, interrupt) == SIG_IGN)
		signal(SIGINT, SIG_IGN);

	getfstab();		/* /etc/fstab snarfed */
	/*
	 *	disk can be either the full special file name,
	 *	the suffix of the special file name,
	 *	the special name missing the leading '/',
	 *	the file system name with or without the leading '/'.
	 */
	dt = fstabsearch(disk);
	if (dt != NULL) {
		disk = rawname(dt->fs_spec);
		strncpy(spcl.c_dev, dt->fs_spec, NAMELEN);
		strncpy(spcl.c_filesys, dt->fs_file, NAMELEN);
	} else {
		strncpy(spcl.c_dev, disk, NAMELEN);
		strncpy(spcl.c_filesys, "an unlisted file system", NAMELEN);
	}
	spcl.c_dev[NAMELEN-1]='\0';
	spcl.c_filesys[NAMELEN-1]='\0';
	strcpy(spcl.c_label, "none");
	gethostname(spcl.c_host, NAMELEN);
	spcl.c_level = level - '0';
	spcl.c_type = TS_TAPE;
	if (!Tflag)
	        getdumptime();		/* /etc/dumpdates snarfed */

	msg("Date of this level %c dump: %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime(&spcl.c_date));
 	msg("Date of last level %c dump: %s", lastlevel,
		spcl.c_ddate == 0 ? "the epoch\n" : ctime(&spcl.c_ddate));
	msg("Dumping %s ", disk);
	if (dt != NULL)
		msgtail("(%s) ", dt->fs_file);
	if (host)
		msgtail("to %s on host %s\n", tape, host);
	else
		msgtail("to %s\n", tape);

	if ((diskfd = open(disk, O_RDONLY)) < 0)
		err(X_STARTUP, "Cannot open %s", disk);
	if (fstat(diskfd, &sb) != 0)
		err(X_STARTUP, "%s: stat", disk);
	if (S_ISDIR(sb.st_mode))
		errx(X_STARTUP, "%s: unknown file system", disk);
	sync();
	sblock = (struct fs *)sblock_buf;
	bread(SBOFF, (char *) sblock, SBSIZE);
	if (sblock->fs_magic != FS_MAGIC)
		quit("bad sblock magic number\n");
	dev_bsize = sblock->fs_fsize / fsbtodb(sblock, 1);
	dev_bshift = ffs(dev_bsize) - 1;
	if (dev_bsize != (1 << dev_bshift))
		quit("dev_bsize (%ld) is not a power of 2", dev_bsize);
	tp_bshift = ffs(TP_BSIZE) - 1;
	if (TP_BSIZE != (1 << tp_bshift))
		quit("TP_BSIZE (%d) is not a power of 2", TP_BSIZE);
#ifdef FS_44INODEFMT
	if (sblock->fs_inodefmt >= FS_44INODEFMT)
		spcl.c_flags |= DR_NEWINODEFMT;
#endif
	maxino = sblock->fs_ipg * sblock->fs_ncg;
	mapsize = roundup(howmany(maxino, NBBY), TP_BSIZE);
	usedinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpdirmap = (char *)calloc((unsigned) mapsize, sizeof(char));
	dumpinomap = (char *)calloc((unsigned) mapsize, sizeof(char));
	tapesize = 3 * (howmany(mapsize * sizeof(char), TP_BSIZE) + 1);

	nonodump = spcl.c_level < honorlevel;

	passno = 1;
	setproctitle("%s: pass 1: regular files", disk);
	msg("mapping (Pass I) [regular files]\n");
	anydirskipped = mapfiles(maxino, &tapesize);

	passno = 2;
	setproctitle("%s: pass 2: directories", disk);
	msg("mapping (Pass II) [directories]\n");
	while (anydirskipped) {
		anydirskipped = mapdirs(maxino, &tapesize);
	}

	if (pipeout || unlimited) {
		tapesize += 10;	/* 10 trailer blocks */
		msg("estimated %ld tape blocks.\n", tapesize);
	} else {
		double fetapes;

		if (blocksperfile)
			fetapes = (double) tapesize / blocksperfile;
		else if (cartridge) {
			/* Estimate number of tapes, assuming streaming stops at
			   the end of each block written, and not in mid-block.
			   Assume no erroneous blocks; this can be compensated
			   for with an artificially low tape size. */
			fetapes =
			(	  (double) tapesize	/* blocks */
				* TP_BSIZE	/* bytes/block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  (double) tapesize	/* blocks */
				* (1.0/ntrec)	/* streaming-stops per block */
				* 15.48		/* 0.1" / streaming-stop " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		} else {
			/* Estimate number of tapes, for old fashioned 9-track
			   tape */
			int tenthsperirg = (density == 625) ? 3 : 7;
			fetapes =
			(	  (double) tapesize	/* blocks */
				* TP_BSIZE	/* bytes / block */
				* (1.0/density)	/* 0.1" / byte " */
			  +
				  (double) tapesize	/* blocks */
				* (1.0/ntrec)	/* IRG's / block */
				* tenthsperirg	/* 0.1" / IRG " */
			) * (1.0 / tsize );	/* tape / 0.1" " */
		}
		etapes = fetapes;		/* truncating assignment */
		etapes++;
		/* count the dumped inodes map on each additional tape */
		tapesize += (etapes - 1) *
			(howmany(mapsize * sizeof(char), TP_BSIZE) + 1);
		tapesize += etapes + 10;	/* headers + 10 trailer blks */
		msg("estimated %ld tape blocks on %3.2f tape(s).\n",
		    tapesize, fetapes);
	}

        /*
         * If the user only wants an estimate of the number of
         * tapes, exit now.
         */
        if (just_estimate)
                exit(0);

	/*
	 * Allocate tape buffer.
	 */
	if (!alloctape())
		quit(
	"can't allocate tape buffers - try a smaller blocking factor.\n");

	startnewtape(1);
	time((time_t *)&(tstart_writing));
	dumpmap(usedinomap, TS_CLRI, maxino - 1);

	passno = 3;
	setproctitle("%s: pass 3: directories", disk);
	msg("dumping (Pass III) [directories]\n");
	dirty = 0;		/* XXX just to get gcc to shut up */
	for (map = dumpdirmap, ino = 1; ino < maxino; ino++) {
		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip directory inodes deleted and maybe reallocated
		 */
		dp = getino(ino);
		if ((dp->di_mode & IFMT) != IFDIR)
			continue;
		dumpino(dp, ino);
	}

	passno = 4;
	setproctitle("%s: pass 4: regular files", disk);
	msg("dumping (Pass IV) [regular files]\n");
	for (map = dumpinomap, ino = 1; ino < maxino; ino++) {
		int mode;

		if (((ino - 1) % NBBY) == 0)	/* map is offset by 1 */
			dirty = *map++;
		else
			dirty >>= 1;
		if ((dirty & 1) == 0)
			continue;
		/*
		 * Skip inodes deleted and reallocated as directories.
		 */
		dp = getino(ino);
		mode = dp->di_mode & IFMT;
		if (mode == IFDIR)
			continue;
		dumpino(dp, ino);
	}

	time(&tend_writing);
	spcl.c_type = TS_END;
	for (i = 0; i < ntrec; i++)
		writeheader(maxino - 1);
	if (pipeout)
		msg("DUMP: %ld tape blocks\n", (long)spcl.c_tapea);
	else
		msg("DUMP: %ld tape blocks on %d volume%s\n",
		    (long)spcl.c_tapea, spcl.c_volume,
		    (spcl.c_volume == 1) ? "" : "s");

	/* report dump performance, avoid division through zero */
	if (tend_writing - tstart_writing == 0)
		msg("finished in less than a second\n");
	else
		msg("finished in %ld seconds, throughput %ld KBytes/sec\n",
		    (long)(tend_writing - tstart_writing),
		    spcl.c_tapea / (tend_writing - tstart_writing));

	putdumptime();
	trewind();
	broadcast("DUMP IS DONE!\a\a\n");
	msg("DUMP IS DONE\n");
	Exit(X_FINOK);
	/* NOTREACHED */
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: dump [-0123456789ac"
#ifdef KERBEROS
		"k"
#endif
		"nSu] [-B records] [-b blocksize] [-D dumpdates]\n"
		"            [-d density] [-f file ] [-h level] [-s feet]\n"
		"            [-T date] [-C cachesizeMB] filesystem\n"
		"       dump [-W | -w]\n");
	exit(X_STARTUP);
}

/*
 * Pick up a numeric argument.  It must be nonnegative and in the given
 * range (except that a vmax of 0 means unlimited).
 */
static long
numarg(char *meaning, long vmin, long vmax)
{
	char *p;
	long val;

	val = strtol(optarg, &p, 10);
	if (*p)
		errx(1, "illegal %s -- %s", meaning, optarg);
	if (val < vmin || (vmax && val > vmax))
		errx(1, "%s must be between %ld and %ld", meaning, vmin, vmax);
	return (val);
}

void
sig(int signo)
{
	switch(signo) {
	case SIGALRM:
	case SIGBUS:
	case SIGFPE:
	case SIGHUP:
	case SIGTERM:
	case SIGTRAP:
		if (pipeout)
			quit("Signal on pipe: cannot recover\n");
		msg("Rewriting attempted as response to unknown signal.\n");
		fflush(stderr);
		fflush(stdout);
		close_rewind();
		exit(X_REWRITE);
		/* NOTREACHED */
	case SIGSEGV:
		msg("SIGSEGV: ABORTING!\n");
		signal(SIGSEGV, SIG_DFL);
		kill(0, SIGSEGV);
		/* NOTREACHED */
	}
}

char *
rawname(char *cp)
{
	static char rawbuf[MAXPATHLEN];
	char *dp;
	struct stat sb;

	if (stat(cp, &sb) == 0) {
		/*
		 * If the name already refers to a raw device, return
		 * it immediately without tampering.
		 */
		if ((sb.st_mode & S_IFMT) == S_IFCHR)
			return (cp);
	}

	dp = strrchr(cp, '/');

	if (dp == NULL)
		return (NULL);
	*dp = '\0';
	strncpy(rawbuf, cp, MAXPATHLEN - 1);
	rawbuf[MAXPATHLEN-1] = '\0';
	*dp = '/';
	strncat(rawbuf, "/r", MAXPATHLEN - 1 - strlen(rawbuf));
	strncat(rawbuf, dp + 1, MAXPATHLEN - 1 - strlen(rawbuf));
	return (rawbuf);
}

/*
 * obsolete --
 *	Change set of key letters and ordered arguments into something
 *	getopt(3) will like.
 */
static void
obsolete(int *argcp, char ***argvp)
{
	int argc, flags;
	char *ap, **argv, *flagsp, **nargv, *p;

	/* Setup. */
	argv = *argvp;
	argc = *argcp;

	/* Return if no arguments or first argument has leading dash. */
	ap = argv[1];
	if (argc == 1 || *ap == '-')
		return;

	/* Allocate space for new arguments. */
	if ((*argvp = nargv = malloc((argc + 1) * sizeof(char *))) == NULL ||
	    (p = flagsp = malloc(strlen(ap) + 2)) == NULL)
		err(1, NULL);

	*nargv++ = *argv;
	argv += 2;

	for (flags = 0; *ap; ++ap) {
		switch (*ap) {
		case 'B':
		case 'b':
		case 'd':
		case 'f':
		case 'D':
		case 'C':
		case 'h':
		case 's':
		case 'T':
			if (*argv == NULL) {
				warnx("option requires an argument -- %c", *ap);
				usage();
			}
			if ((nargv[0] = malloc(strlen(*argv) + 2 + 1)) == NULL)
				err(1, NULL);
			nargv[0][0] = '-';
			nargv[0][1] = *ap;
			strcpy(&nargv[0][2], *argv);
			++argv;
			++nargv;
			break;
		default:
			if (!flags) {
				*p++ = '-';
				flags = 1;
			}
			*p++ = *ap;
			break;
		}
	}

	/* Terminate flags. */
	if (flags) {
		*p = '\0';
		*nargv++ = flagsp;
	}

	/* Copy remaining arguments. */
	while ((*nargv++ = *argv++));

	/* Update argument count. */
	*argcp = nargv - *argvp - 1;
}
