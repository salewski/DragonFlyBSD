/*-
 * Copyright (c) 1980, 1988, 1993
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
 * @(#)optr.c	8.2 (Berkeley) 1/6/94
 * $FreeBSD: src/sbin/dump/optr.c,v 1.9.2.5 2002/02/23 22:32:51 iedowse Exp $
 * $DragonFly: src/sbin/dump/optr.c,v 1.6 2004/12/27 22:36:37 liamfoy Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <utmp.h>

#include "dump.h"
#include "pathnames.h"

void	alarmcatch(/* int, int */);
int	datesort(const void *, const void *);

/*
 *	Query the operator; This previously-fascist piece of code
 *	no longer requires an exact response.
 *	It is intended to protect dump aborting by inquisitive
 *	people banging on the console terminal to see what is
 *	happening which might cause dump to croak, destroying
 *	a large number of hours of work.
 *
 *	Every 2 minutes we reprint the message, alerting others
 *	that dump needs attention.
 */
static	int timeout;
static	char *attnmessage;		/* attention message */

int
query(char *question)
{
	char	replybuffer[64];
	int	back, errcount;
	FILE	*mytty;

	if ((mytty = fopen(_PATH_TTY, "r")) == NULL)
		quit("fopen on %s fails: %s\n", _PATH_TTY, strerror(errno));
	attnmessage = question;
	timeout = 0;
	alarmcatch();
	back = -1;
	errcount = 0;
	do {
		if (fgets(replybuffer, 63, mytty) == NULL) {
			clearerr(mytty);
			if (++errcount > 30)	/* XXX	ugly */
				quit("excessive operator query failures\n");
		} else if (replybuffer[0] == 'y' || replybuffer[0] == 'Y') {
			back = 1;
		} else if (replybuffer[0] == 'n' || replybuffer[0] == 'N') {
			back = 0;
		} else {
			fprintf(stderr, "  DUMP: \"Yes\" or \"No\"?\n");
			fprintf(stderr, "  DUMP: %s: (\"yes\" or \"no\") ",
			    question);
		}
	} while (back < 0);

	/*
	 *	Turn off the alarm, and reset the signal to trap out..
	 */
	alarm(0);
	if (signal(SIGALRM, sig) == SIG_IGN)
		signal(SIGALRM, SIG_IGN);
	fclose(mytty);
	return(back);
}

char lastmsg[BUFSIZ];

/*
 *	Alert the console operator, and enable the alarm clock to
 *	sleep for 2 minutes in case nobody comes to satisfy dump
 */
void
alarmcatch()
{
	if (notify == 0) {
		if (timeout == 0)
			fprintf(stderr, "  DUMP: %s: (\"yes\" or \"no\") ",
			    attnmessage);
		else
			msgtail("\a\a");
	} else {
		if (timeout) {
			msgtail("\n");
			broadcast("");		/* just print last msg */
		}
		fprintf(stderr,"  DUMP: %s: (\"yes\" or \"no\") ", attnmessage);
	}
	signal(SIGALRM, alarmcatch);
	alarm(120);
	timeout = 1;
}

/*
 *	Here if an inquisitive operator interrupts the dump program
 */
void
interrupt(int signo)
{
	msg("Interrupt received.\n");
	if (query("Do you want to abort dump?"))
		dumpabort(0);
}

/*
 *	We now use wall(1) to do the actual broadcasting.
 */
void
broadcast(char *message)
{
	FILE *fp;
	char buf[sizeof(_PATH_WALL) + sizeof(OPGRENT) + 3];

	if (!notify)
		return;

	snprintf(buf, sizeof(buf), "%s -g %s", _PATH_WALL, OPGRENT);
	if ((fp = popen(buf, "w")) == NULL)
		return;

	fputs("\a\a\aMessage from the dump program to all operators\n\nDUMP: NEEDS ATTENTION: ", fp);
	if (lastmsg[0])
		fputs(lastmsg, fp);
	if (message[0])
		fputs(message, fp);

	pclose(fp);
}

/*
 *	Print out an estimate of the amount of time left to do the dump
 */

time_t	tschedule = 0;

void
timeest(void)
{
	double percent;
	time_t	tnow;
	int deltat, hours, mins;

	time(&tnow);
	deltat = (blockswritten == 0) ? 0 : tstart_writing - tnow +
	    (double)(tnow - tstart_writing) / blockswritten * tapesize;
	percent = (blockswritten * 100.0) / tapesize;
	hours = deltat / 3600;
	mins = (deltat % 3600) / 60;

	setproctitle("%s: pass %d: %3.2f%% done, finished in %d:%02d",
	    disk, passno, percent, hours, mins);
	if (tnow >= tschedule) {
		tschedule = tnow + 300;
		if (blockswritten < 500)
			return;
		msg("%3.2f%% done, finished in %d:%02d\n", percent, hours,
		    mins);
	}
}

/*
 * Schedule a printout of the estimate in the next call to timeest().
 */
void
infosch(int signal)
{
	tschedule = 0;
}

void
msg(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	fprintf(stderr, "pid=%d ", getpid());
#endif
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fflush(stdout);
	fflush(stderr);
	vsnprintf(lastmsg, sizeof(lastmsg), fmt, ap);
	va_end(ap);
}

void
msgtail(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
quit(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr,"  DUMP: ");
#ifdef TDEBUG
	fprintf(stderr, "pid=%d ", getpid());
#endif
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fflush(stdout);
	fflush(stderr);
	dumpabort(0);
}

/*
 *	Tell the operator what has to be done;
 *	we don't actually do it
 */

struct fstab *
allocfsent(struct fstab *fs)
{
	struct fstab *new;

	new = (struct fstab *)malloc(sizeof (*fs));
	if (new == NULL ||
	    (new->fs_file = strdup(fs->fs_file)) == NULL ||
	    (new->fs_type = strdup(fs->fs_type)) == NULL ||
	    (new->fs_spec = strdup(fs->fs_spec)) == NULL)
		quit("%s\n", strerror(errno));
	new->fs_passno = fs->fs_passno;
	new->fs_freq = fs->fs_freq;
	return (new);
}

struct	pfstab {
	SLIST_ENTRY(pfstab) pf_list;
	struct	fstab *pf_fstab;
};

static	SLIST_HEAD(, pfstab) table;

void
getfstab(void)
{
	struct fstab *fs;
	struct pfstab *pf;

	if (setfsent() == 0) {
		msg("Can't open %s for dump table information: %s\n",
		    _PATH_FSTAB, strerror(errno));
		return;
	}
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_type, FSTAB_RW) &&
		    strcmp(fs->fs_type, FSTAB_RO) &&
		    strcmp(fs->fs_type, FSTAB_RQ))
			continue;
		fs = allocfsent(fs);
		if ((pf = (struct pfstab *)malloc(sizeof (*pf))) == NULL)
			quit("%s\n", strerror(errno));
		pf->pf_fstab = fs;
		SLIST_INSERT_HEAD(&table, pf, pf_list);
	}
	endfsent();
}

/*
 * Search in the fstab for a file name.
 * This file name can be either the special or the path file name.
 *
 * The file name can omit the leading '/'.
 */
struct fstab *
fstabsearch(char *key)
{
	struct pfstab *pf;
	struct fstab *fs;
	char *rn;

	SLIST_FOREACH(pf, &table, pf_list) {
		fs = pf->pf_fstab;
		if (strcmp(fs->fs_file, key) == 0 ||
		    strcmp(fs->fs_spec, key) == 0)
			return (fs);
		rn = rawname(fs->fs_spec);
		if (rn != NULL && strcmp(rn, key) == 0)
			return (fs);
		if (key[0] != '/') {
			if (*fs->fs_spec == '/' &&
			    strcmp(fs->fs_spec + 1, key) == 0)
				return (fs);
			if (*fs->fs_file == '/' &&
			    strcmp(fs->fs_file + 1, key) == 0)
				return (fs);
		}
	}
	return (NULL);
}

/*
 *	Tell the operator what to do
 *
 * char arg: w ==> just what to do; W ==> most recent dumps 
 */
void
lastdump(int arg)
{
	int i;
	struct fstab *dt;
	struct dumpdates *dtwalk;
	char *lastname, *date;
	int dumpme;
	time_t tnow;
	struct tm *tlast;

	time(&tnow);
	getfstab();		/* /etc/fstab input */
	initdumptimes();	/* /etc/dumpdates input */
	qsort((char *) ddatev, nddates, sizeof(struct dumpdates *), datesort);

	if (arg == 'w')
		printf("Dump these file systems:\n");
	else
		printf("Last dump(s) done (Dump '>' file systems):\n");
	lastname = "??";
	ITITERATE(i, dtwalk) {
		if (strncmp(lastname, dtwalk->dd_name,
		    sizeof(dtwalk->dd_name)) == 0)
			continue;
		date = (char *)ctime(&dtwalk->dd_ddate);
		date[16] = '\0';	/* blast away seconds and year */
		lastname = dtwalk->dd_name;
		dt = fstabsearch(dtwalk->dd_name);
		dumpme = (dt != NULL && dt->fs_freq != 0);
		if (dumpme) {
		    tlast = localtime(&dtwalk->dd_ddate);
		    dumpme = tnow > (dtwalk->dd_ddate - (tlast->tm_hour * 3600)
				     - (tlast->tm_min * 60) - tlast->tm_sec
				     + (dt->fs_freq * 86400));
		};
		if (arg != 'w' || dumpme)
			printf(
			    "%c %8s\t(%6s) Last dump: Level %c, Date %s\n",
			    dumpme && (arg != 'w') ? '>' : ' ',
			    dtwalk->dd_name,
			    dt ? dt->fs_file : "",
			    dtwalk->dd_level,
			    date);
	}
}

int
datesort(const void *a1, const void *a2)
{
	struct dumpdates *d1 = *(struct dumpdates **)a1;
	struct dumpdates *d2 = *(struct dumpdates **)a2;
	int diff;

	diff = strncmp(d1->dd_name, d2->dd_name, sizeof(d1->dd_name));
	if (diff == 0)
		return (d2->dd_ddate - d1->dd_ddate);
	return (diff);
}
