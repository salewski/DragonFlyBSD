/*-
 * Copyright (c) 1980, 1993
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
 * @(#)itime.c	8.1 (Berkeley) 6/5/93
 * $FreeBSD: src/sbin/dump/itime.c,v 1.3.2.1 2001/08/01 06:29:35 obrien Exp $
 * $DragonFly: src/sbin/dump/itime.c,v 1.8 2005/04/02 22:25:32 dillon Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>
#ifdef sunos
#include <sys/vnode.h>

#include <ufs/fsdir.h>
#include <ufs/inode.h>
#include <ufs/fs.h>
#else
#include <vfs/ufs/dinode.h>
#endif

#include <protocols/dumprestore.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

#include "dump.h"

struct dumptime {
	struct	dumpdates dt_value;
	SLIST_ENTRY(dumptime) dt_list;
};
SLIST_HEAD(dthead, dumptime) dthead = SLIST_HEAD_INITIALIZER(dthead);
struct	dumpdates **ddatev = 0;
int	nddates = 0;

static	void dumprecout(FILE *, struct dumpdates *);
static	int getrecord(FILE *, struct dumpdates *);
static	int makedumpdate(struct dumpdates *, char *);
static	void readdumptimes(FILE *);

void
initdumptimes(void)
{
	FILE *df;

	if ((df = fopen(dumpdates, "r")) == NULL) {
		if (errno != ENOENT) {
			msg("WARNING: cannot read %s: %s\n", dumpdates,
			    strerror(errno));
			return;
		}
		/*
		 * Dumpdates does not exist, make an empty one.
		 */
		msg("WARNING: no file `%s', making an empty one\n", dumpdates);
		if ((df = fopen(dumpdates, "w")) == NULL) {
			msg("WARNING: cannot create %s: %s\n", dumpdates,
			    strerror(errno));
			return;
		}
		fclose(df);
		if ((df = fopen(dumpdates, "r")) == NULL) {
			quit("cannot read %s even after creating it: %s\n",
			    dumpdates, strerror(errno));
			/* NOTREACHED */
		}
	}
	flock(fileno(df), LOCK_SH);
	readdumptimes(df);
	fclose(df);
}

static void
readdumptimes(FILE *df)
{
	int i;
	struct dumptime *dtwalk;

	for (;;) {
		dtwalk = (struct dumptime *)calloc(1, sizeof (struct dumptime));
		if (getrecord(df, &(dtwalk->dt_value)) < 0)
			break;
		nddates++;
		SLIST_INSERT_HEAD(&dthead, dtwalk, dt_list);
	}

	/*
	 *	arrayify the list, leaving enough room for the additional
	 *	record that we may have to add to the ddate structure
	 */
	ddatev = (struct dumpdates **)
		calloc((unsigned) (nddates + 1), sizeof (struct dumpdates *));
	dtwalk = SLIST_FIRST(&dthead);
	for (i = nddates - 1; i >= 0; i--, dtwalk = SLIST_NEXT(dtwalk, dt_list))
		ddatev[i] = &dtwalk->dt_value;
}

void
getdumptime(void)
{
	struct dumpdates *ddp;
	int i;
	char *fname;

	fname = disk;
#ifdef FDEBUG
	msg("Looking for name %s in dumpdates = %s for level = %c\n",
		fname, dumpdates, level);
#endif
	spcl.c_ddate = 0;
	lastlevel = '0';

	initdumptimes();
	/*
	 *	Go find the entry with the same name for a lower increment
	 *	and older date
	 */
	ITITERATE(i, ddp) {
		if (strncmp(fname, ddp->dd_name, sizeof (ddp->dd_name)) != 0)
			continue;
		if (ddp->dd_level >= level)
			continue;
		if (ddp->dd_ddate <= spcl.c_ddate)
			continue;
		spcl.c_ddate = ddp->dd_ddate;
		lastlevel = ddp->dd_level;
	}
}

void
putdumptime(void)
{
	FILE *df;
	struct dumpdates *dtwalk;
	int i;
	int fd;
	char *fname;

	if(uflag == 0)
		return;
	if ((df = fopen(dumpdates, "r+")) == NULL)
		quit("cannot rewrite %s: %s\n", dumpdates, strerror(errno));
	fd = fileno(df);
	flock(fd, LOCK_EX);
	fname = disk;
	free((char *)ddatev);
	ddatev = 0;
	nddates = 0;
	readdumptimes(df);
	if (fseek(df, 0L, 0) < 0)
		quit("fseek: %s\n", strerror(errno));
	spcl.c_ddate = 0;
	ITITERATE(i, dtwalk) {
		if (strncmp(fname, dtwalk->dd_name,
				sizeof (dtwalk->dd_name)) != 0)
			continue;
		if (dtwalk->dd_level != level)
			continue;
		goto found;
	}
	/*
	 *	construct the new upper bound;
	 *	Enough room has been allocated.
	 */
	dtwalk = ddatev[nddates] =
		(struct dumpdates *)calloc(1, sizeof (struct dumpdates));
	nddates += 1;
  found:
	strncpy(dtwalk->dd_name, fname, sizeof (dtwalk->dd_name));
	dtwalk->dd_level = level;
	dtwalk->dd_ddate = spcl.c_date;

	ITITERATE(i, dtwalk) {
		dumprecout(df, dtwalk);
	}
	if (fflush(df))
		quit("%s: %s\n", dumpdates, strerror(errno));
	if (ftruncate(fd, ftell(df)))
		quit("ftruncate (%s): %s\n", dumpdates, strerror(errno));
	fclose(df);
	msg("level %c dump on %s", level,
		spcl.c_date == 0 ? "the epoch\n" : ctime(&spcl.c_date));
}

static void
dumprecout(FILE *file, struct dumpdates *what)
{

	if (fprintf(file, DUMPOUTFMT,
		    what->dd_name,
		    what->dd_level,
		    ctime(&what->dd_ddate)) < 0)
		quit("%s: %s\n", dumpdates, strerror(errno));
}

int	recno;

static int
getrecord(FILE *df, struct dumpdates *ddatep)
{
	char tbuf[BUFSIZ];

	recno = 0;
	if ( (fgets(tbuf, sizeof (tbuf), df)) != tbuf)
		return(-1);
	recno++;
	if (makedumpdate(ddatep, tbuf) < 0)
		msg("Unknown intermediate format in %s, line %d\n",
			dumpdates, recno);

#ifdef FDEBUG
	msg("getrecord: %s %c %s", ddatep->dd_name, ddatep->dd_level,
	    ddatep->dd_ddate == 0 ? "the epoch\n" : ctime(&ddatep->dd_ddate));
#endif
	return(0);
}

static int
makedumpdate(struct dumpdates *ddp, char *tbuf)
{
	char un_buf[128];

	sscanf(tbuf, DUMPINFMT, ddp->dd_name, &ddp->dd_level, un_buf);
	ddp->dd_ddate = unctime(un_buf);
	if (ddp->dd_ddate < 0)
		return(-1);
	return(0);
}
