/*
 * Copyright (c) 1988, 1990, 1993
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
 * @(#) Copyright (c) 1988, 1990, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)shutdown.c	8.4 (Berkeley) 4/28/95
 * $FreeBSD: src/sbin/shutdown/shutdown.c,v 1.21.2.1 2001/07/30 10:38:08 dd Exp $
 * $DragonFly: src/sbin/shutdown/shutdown.c,v 1.6 2005/01/02 01:22:49 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

#ifdef DEBUG
#undef _PATH_NOLOGIN
#define	_PATH_NOLOGIN	"./nologin"
#endif

#define	H		*60*60
#define	M		*60
#define	S		*1
#define	NOLOG_TIME	5*60
struct interval {
	int timeleft, timetowait;
} tlist[] = {
	{ 10 H,  5 H },
	{  5 H,  3 H },
	{  2 H,  1 H },
	{  1 H, 30 M },
	{ 30 M, 10 M },
	{ 20 M, 10 M },
	{ 10 M,  5 M },
	{  5 M,  3 M },
	{  2 M,  1 M },
	{  1 M, 30 S },
	{ 30 S, 30 S },
	{  0  ,  0   }
};
#undef H
#undef M
#undef S

static time_t offset, shuttime;
static int dohalt, dopower, doreboot, killflg, mbuflen, oflag;
static char mbuf[BUFSIZ];
static const char *nosync, *whom;

void badtime(void);
void die_you_gravy_sucking_pig_dog(void);
void finish(int);
void getoffset(char *);
void loop(void);
void nolog(void);
void timeout(int);
void timewarn(int);
void usage(const char *);

int
main(int argc, char **argv)
{
	char *p, *endp;
	struct passwd *pw;
	int arglen, ch, len, readstdin;

#ifndef DEBUG
	if (geteuid())
		errx(1, "NOT super-user");
#endif
	nosync = NULL;
	readstdin = 0;
	while ((ch = getopt(argc, argv, "-hknopr")) != -1)
		switch (ch) {
		case '-':
			readstdin = 1;
			break;
		case 'h':
			dohalt = 1;
			break;
		case 'k':
			killflg = 1;
			break;
		case 'n':
			nosync = "-n";
			break;
		case 'o':
			oflag = 1;
			break;
		case 'p':
			dopower = 1;
			break;
		case 'r':
			doreboot = 1;
			break;
		case '?':
		default:
			usage((char *)NULL);
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage((char *)NULL);

	if (killflg + doreboot + dohalt + dopower > 1)
		usage("incompatible switches -h, -k, -p and -r");

	if (oflag && !(dohalt || dopower || doreboot))
		usage("-o requires -h, -p or -r");

	if (nosync != NULL && !oflag)
		usage("-n requires -o");

	getoffset(*argv++);

	if (*argv) {
		for (p = mbuf, len = sizeof(mbuf); *argv; ++argv) {
			arglen = strlen(*argv);
			if ((len -= arglen) <= 2)
				break;
			if (p != mbuf)
				*p++ = ' ';
			memmove(p, *argv, arglen);
			p += arglen;
		}
		*p = '\n';
		*++p = '\0';
	}

	if (readstdin) {
		p = mbuf;
		endp = mbuf + sizeof(mbuf) - 2;
		for (;;) {
			if (!fgets(p, endp - p + 1, stdin))
				break;
			for (; *p &&  p < endp; ++p);
			if (p == endp) {
				*p = '\n';
				*++p = '\0';
				break;
			}
		}
	}
	mbuflen = strlen(mbuf);

	if (offset)
		printf("Shutdown at %.24s.\n", ctime(&shuttime));
	else
		printf("Shutdown NOW!\n");

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";

#ifdef DEBUG
	putc('\n', stdout);
#else
	setpriority(PRIO_PROCESS, 0, PRIO_MIN);
	{
		int forkpid;

		forkpid = fork();
		if (forkpid == -1)
			err(1, "fork");
		if (forkpid)
			errx(0, "[pid %d]", forkpid);
	}
	setsid();
#endif
	openlog("shutdown", LOG_CONS, LOG_AUTH);
	loop();
	return(0);
}

void
loop(void)
{
	struct interval *tp;
	u_int sltime;
	int logged;

	if (offset <= NOLOG_TIME) {
		logged = 1;
		nolog();
	}
	else
		logged = 0;
	tp = tlist;
	if (tp->timeleft < offset)
		sleep((u_int)(offset - tp->timeleft));
	else {
		while (tp->timeleft && offset < tp->timeleft)
			++tp;
		/*
		 * Warn now, if going to sleep more than a fifth of
		 * the next wait time.
		 */
		if ((sltime = offset - tp->timeleft)) {
			if (sltime > (u_int)(tp->timetowait / 5))
				timewarn(offset);
			sleep(sltime);
		}
	}
	for (;; ++tp) {
		timewarn(tp->timeleft);
		if (!logged && tp->timeleft <= NOLOG_TIME) {
			logged = 1;
			nolog();
		}
		sleep((u_int)tp->timetowait);
		if (!tp->timeleft)
			break;
	}
	die_you_gravy_sucking_pig_dog();
}

static jmp_buf alarmbuf;

static const char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH,
	NULL
};

extern const char **environ;

void
timewarn(int timeleft)
{
	static int first;
	static char hostname[MAXHOSTNAMELEN + 1];
	FILE *pf;
	char wcmd[MAXPATHLEN + 4];

	if (!first++)
		gethostname(hostname, sizeof(hostname));

	/* undoc -n option to wall suppresses normal wall banner */
	snprintf(wcmd, sizeof(wcmd), "%s -n", _PATH_WALL);
	environ = restricted_environ;
	if (!(pf = popen(wcmd, "w"))) {
		syslog(LOG_ERR, "shutdown: can't find %s: %m", _PATH_WALL);
		return;
	}

	fprintf(pf,
	    "\007*** %sSystem shutdown message from %s@%s ***\007\n",
	    timeleft ? "": "FINAL ", whom, hostname);

	if (timeleft > 10*60)
		fprintf(pf, "System going down at %5.5s\n\n",
		    ctime(&shuttime) + 11);
	else if (timeleft > 59)
		fprintf(pf, "System going down in %d minute%s\n\n",
		    timeleft / 60, (timeleft > 60) ? "s" : "");
	else if (timeleft)
		fprintf(pf, "System going down in 30 seconds\n\n");
	else
		fprintf(pf, "System going down IMMEDIATELY\n\n");

	if (mbuflen)
		fwrite(mbuf, sizeof(*mbuf), mbuflen, pf);

	/*
	 * play some games, just in case wall doesn't come back
	 * probably unnecessary, given that wall is careful.
	 */
	if (!setjmp(alarmbuf)) {
		signal(SIGALRM, timeout);
		alarm((u_int)30);
		pclose(pf);
		alarm((u_int)0);
		signal(SIGALRM, SIG_DFL);
	}
}

void
timeout(int signo __unused)
{
	longjmp(alarmbuf, 1);
}

void
die_you_gravy_sucking_pig_dog(void)
{
	char *empty_environ[] = { NULL };

	syslog(LOG_NOTICE, "%s by %s: %s",
	    doreboot ? "reboot" : dohalt ? "halt" : dopower ? "power-down" : 
	    "shutdown", whom, mbuf);
	sleep(2);

	printf("\r\nSystem shutdown time has arrived\007\007\r\n");
	if (killflg) {
		printf("\rbut you'll have to do it yourself\r\n");
		exit(0);
	}
#ifdef DEBUG
	if (doreboot)
		printf("reboot");
	else if (dohalt)
		printf("halt");
	else if (dopower)
		printf("power-down");
	if (nosync != NULL)
		printf(" no sync");
	printf("\nkill -HUP 1\n");
#else
	if (!oflag) {
		kill(1, doreboot ? SIGINT :	/* reboot */
			dohalt ? SIGUSR1 :	/* halt */
			dopower ? SIGUSR2 :	/* power-down */
			SIGTERM);		/* single-user */
	} else {
		if (doreboot) {
			execle(_PATH_REBOOT, "reboot", "-l", nosync, 
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_REBOOT);
			warn(_PATH_REBOOT);
		}
		else if (dohalt) {
			execle(_PATH_HALT, "halt", "-l", nosync,
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_HALT);
			warn(_PATH_HALT);
		}
		else if (dopower) {
			execle(_PATH_HALT, "halt", "-l", "-p", nosync,
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_HALT);
			warn(_PATH_HALT);
		}
		kill(1, SIGTERM);		/* to single-user */
	}
#endif
	finish(0);
}

#define	ATOI2(p)	(p[0] - '0') * 10 + (p[1] - '0'); p += 2;

void
getoffset(char *timearg)
{
	struct tm *lt;
	char *p;
	time_t now;
	int this_year;

	time(&now);

	if (!strcasecmp(timearg, "now")) {		/* now */
		offset = 0;
		shuttime = now;
		return;
	}

	if (*timearg == '+') {				/* +minutes */
		if (!isdigit(*++timearg))
			badtime();
		if ((offset = atoi(timearg) * 60) < 0)
			badtime();
		shuttime = now + offset;
		return;
	}

	/* handle hh:mm by getting rid of the colon */
	for (p = timearg; *p; ++p)
		if (!isascii(*p) || !isdigit(*p)) {
			if (*p == ':' && strlen(p) == 3) {
				p[0] = p[1];
				p[1] = p[2];
				p[2] = '\0';
			}
			else
				badtime();
		}

	unsetenv("TZ");					/* OUR timezone */
	lt = localtime(&now);				/* current time val */

	switch(strlen(timearg)) {
	case 10:
		this_year = lt->tm_year;
		lt->tm_year = ATOI2(timearg);
		/*
		 * check if the specified year is in the next century.
		 * allow for one year of user error as many people will
		 * enter n - 1 at the start of year n.
		 */
		if (lt->tm_year < (this_year % 100) - 1)
			lt->tm_year += 100;
		/* adjust for the year 2000 and beyond */
		lt->tm_year += (this_year - (this_year % 100));
		/* FALLTHROUGH */
	case 8:
		lt->tm_mon = ATOI2(timearg);
		if (--lt->tm_mon < 0 || lt->tm_mon > 11)
			badtime();
		/* FALLTHROUGH */
	case 6:
		lt->tm_mday = ATOI2(timearg);
		if (lt->tm_mday < 1 || lt->tm_mday > 31)
			badtime();
		/* FALLTHROUGH */
	case 4:
		lt->tm_hour = ATOI2(timearg);
		if (lt->tm_hour < 0 || lt->tm_hour > 23)
			badtime();
		lt->tm_min = ATOI2(timearg);
		if (lt->tm_min < 0 || lt->tm_min > 59)
			badtime();
		lt->tm_sec = 0;
		if ((shuttime = mktime(lt)) == -1)
			badtime();
		if ((offset = shuttime - now) < 0)
			errx(1, "that time is already past.");
		break;
	default:
		badtime();
	}
}

#define	NOMSG	"\n\nNO LOGINS: System going down at "
void
nolog(void)
{
	int logfd;
	char *ct;

	unlink(_PATH_NOLOGIN);	/* in case linked to another file */
	signal(SIGINT, finish);
	signal(SIGHUP, finish);
	signal(SIGQUIT, finish);
	signal(SIGTERM, finish);
	if ((logfd = open(_PATH_NOLOGIN, O_WRONLY|O_CREAT|O_TRUNC,
	    0664)) >= 0) {
		write(logfd, NOMSG, sizeof(NOMSG) - 1);
		ct = ctime(&shuttime);
		write(logfd, ct + 11, 5);
		write(logfd, "\n\n", 2);
		write(logfd, mbuf, strlen(mbuf));
		close(logfd);
	}
}

void
finish(int signo __unused)
{
	if (!killflg)
		unlink(_PATH_NOLOGIN);
	exit(0);
}

void
badtime(void)
{
	errx(1, "bad time format");
}

void
usage(const char *cp)
{
	if (cp != NULL)
		warnx("%s", cp);
	fprintf(stderr,
	    "usage: shutdown [-] [-h | -p | -r | -k] [-o [-n]]"
	    " time [warning-message ...]\n");
	exit(1);
}
