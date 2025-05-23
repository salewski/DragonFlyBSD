/*
 * Copyright (c) 1987, 1988, 1993
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
 * @(#) Copyright (c) 1987, 1988, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)time.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/time/time.c,v 1.14.2.5 2002/06/28 08:35:15 tjr Exp $
 * $DragonFly: src/usr.bin/time/time.c,v 1.11 2005/03/04 16:54:37 liamfoy Exp $
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <kinfo.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void humantime(FILE *, long, long);
static void usage(void);

static char decimal_point;

int
main(int argc, char **argv)
{
	pid_t pid;
	int aflag, ch, hflag, lflag, status, pflag;
	int exit_on_sig;
	struct timeval before, after;
	struct rusage ru;
	FILE *out = stderr;
	const char *ofn = NULL;

	setlocale(LC_NUMERIC, "");
	decimal_point = localeconv()->decimal_point[0];

	aflag = hflag = lflag = pflag = 0;
	while ((ch = getopt(argc, argv, "ahlo:p")) != -1)
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'o':
			ofn = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		default:
			usage();
		}

	if (!(argc -= optind))
		exit(0);
	argv += optind;

	if (ofn) {
	        if ((out = fopen(ofn, aflag ? "a" : "w")) == NULL)
		        err(1, "%s", ofn);
		setvbuf(out, (char *)NULL, _IONBF, (size_t)0);
	}

	if (gettimeofday(&before, (struct timezone *)NULL) == -1)
		err(1, "gettimeofday failed");
	switch (pid = fork()) {
	case -1:			/* error */
		err(1, "could not fork");
		/* NOTREACHED */
	case 0:				/* child */
		execvp(*argv, argv);
		err(errno == ENOENT ? 127 : 126, "%s", *argv);
		/* NOTREACHED */
	}
	/* parent */
	if (signal(SIGINT, SIG_IGN) == SIG_ERR)
		err(1, "signal failed");	
	if (signal(SIGQUIT, SIG_IGN) == SIG_ERR)
		err(1, "signal failed");
	while (wait4(pid, &status, 0, &ru) != pid)		/* XXX use waitpid */
		;
	if (gettimeofday(&after, (struct timezone *)NULL) == -1)
		err(1, "gettimeofday failed");
	if (!WIFEXITED(status))
		warnx("command terminated abnormally");
	exit_on_sig = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
	after.tv_sec -= before.tv_sec;
	after.tv_usec -= before.tv_usec;
	if (after.tv_usec < 0)
		after.tv_sec--, after.tv_usec += 1000000;
	if (pflag) {
		/* POSIX wants output that must look like
		"real %f\nuser %f\nsys %f\n" and requires
		at least two digits after the radix. */
		fprintf(out, "real %ld%c%02ld\n",
			after.tv_sec, decimal_point,
			after.tv_usec / 10000);
		fprintf(out, "user %ld%c%02ld\n",
			ru.ru_utime.tv_sec, decimal_point,
			ru.ru_utime.tv_usec / 10000);
		fprintf(out, "sys %ld%c%02ld\n",
			ru.ru_stime.tv_sec, decimal_point,
			ru.ru_stime.tv_usec / 10000);
	} else if (hflag) {
		humantime(out, after.tv_sec, after.tv_usec / 10000);
		fprintf(out, " real\t");
		humantime(out, ru.ru_utime.tv_sec, ru.ru_utime.tv_usec / 10000);
		fprintf(out, " user\t");
		humantime(out, ru.ru_stime.tv_sec, ru.ru_stime.tv_usec / 10000);
		fprintf(out, " sys\n");
	} else {
		fprintf(out, "%9ld%c%02ld real ",
			after.tv_sec, decimal_point,
			after.tv_usec / 10000);
		fprintf(out, "%9ld%c%02ld user ",
			ru.ru_utime.tv_sec, decimal_point,
			ru.ru_utime.tv_usec / 10000);
		fprintf(out, "%9ld%c%02ld sys\n",
			ru.ru_stime.tv_sec, decimal_point,
			ru.ru_stime.tv_usec / 10000);
	}
	if (lflag) {
		int hz;
		u_long ticks;

		if (kinfo_get_sched_stathz(&hz))
			err(1, "kinfo_get_sched_stathz");
		ticks = hz * (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
		     hz * (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000000;

		/*
		 * If our round-off on the tick calculation still puts us at 0,
		 * then always assume at least one tick.
		 */
		if (ticks == 0)
			ticks = 1;

		fprintf(out, "%10ld  %s\n",
			ru.ru_maxrss, "maximum resident set size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_ixrss / ticks, "average shared memory size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_idrss / ticks, "average unshared data size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_isrss / ticks, "average unshared stack size");
		fprintf(out, "%10ld  %s\n",
			ru.ru_minflt, "page reclaims");
		fprintf(out, "%10ld  %s\n",
			ru.ru_majflt, "page faults");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nswap, "swaps");
		fprintf(out, "%10ld  %s\n",
			ru.ru_inblock, "block input operations");
		fprintf(out, "%10ld  %s\n",
			ru.ru_oublock, "block output operations");
		fprintf(out, "%10ld  %s\n",
			ru.ru_msgsnd, "messages sent");
		fprintf(out, "%10ld  %s\n",
			ru.ru_msgrcv, "messages received");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nsignals, "signals received");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nvcsw, "voluntary context switches");
		fprintf(out, "%10ld  %s\n",
			ru.ru_nivcsw, "involuntary context switches");
	}
	/*
	 * If the child has exited on a signal, exit on the same
	 * signal, too, in order to reproduce the child's exit
	 * status.  However, avoid actually dumping core from
	 * current process.
	 */
	if (exit_on_sig) {
		if (signal(exit_on_sig, SIG_DFL) == SIG_ERR)
			warn("signal failed");
		else
			kill(getpid(), exit_on_sig);
	}
	exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: time [-al] [-h|-p] [-o file] utility [argument ...]\n");
	exit(1);
}

static void
humantime(FILE *out, long sec, long usec)
{
	long days, hrs, mins;

	days = sec / (60L * 60 * 24);
	sec %= (60L * 60 * 24);
	hrs = sec / (60L * 60);
	sec %= (60L * 60);
	mins = sec / 60;
	sec %= 60;

	fprintf(out, "\t");
	if (days)
		fprintf(out, "%ldd", days);
	if (hrs)
		fprintf(out, "%ldh", hrs);
	if (mins)
		fprintf(out, "%ldm", mins);
	fprintf(out, "%ld%c%02lds", sec, decimal_point, usec);
}
