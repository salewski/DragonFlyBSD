/*
 * Copyright (c) 1988, 1993, 1994
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
 * @(#) Copyright (c) 1988, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)kill.c	8.4 (Berkeley) 4/28/95
 * $FreeBSD: src/bin/kill/kill.c,v 1.11.2.2 2002/07/28 10:19:57 tjr Exp $
 * $DragonFly: src/bin/kill/kill.c,v 1.7 2005/03/05 19:41:38 liamfoy Exp $
 */

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void	nosig(const char *);
static void	printsignals(FILE *);
static int	signame_to_signum(const char *);
static void	usage(void);

int
main(int argc, char **argv)
{
	int errors, numsig;
	pid_t pid;
	char *ep;

	if (argc < 2)
		usage();

	numsig = SIGTERM;

	argc--, argv++;
	if (strcmp(*argv, "-l") == 0) {
		argc--, argv++;
		if (argc > 1)
			usage();
		if (argc == 1) {
			if (!isdigit(**argv))
				usage();
			numsig = strtol(*argv, &ep, 10);
			if (**argv == '\0' || *ep != '\0')
				errx(1, "illegal signal number: %s", *argv);
			if (numsig >= 128)
				numsig -= 128;
			if (numsig <= 0 || numsig >= sys_nsig)
				nosig(*argv);
			printf("%s\n", sys_signame[numsig]);
			exit(0);
		}
		printsignals(stdout);
		exit(0);
	}

	if (strcmp(*argv, "-s") == 0) {
		argc--, argv++;
		if (argc < 1) {
			warnx("option requires an argument -- s");
			usage();
		}
		if (strcmp(*argv, "0") != 0) {
			if ((numsig = signame_to_signum(*argv)) < 0)
				nosig(*argv);
		} else
			numsig = 0;
		argc--, argv++;
	} else if (**argv == '-' && *(*argv + 1) != '-') {
		++*argv;
		if (isalpha(**argv)) {
			if ((numsig = signame_to_signum(*argv)) < 0)
				nosig(*argv);
		} else if (isdigit(**argv)) {
			numsig = strtol(*argv, &ep, 10);
			if (**argv == '\0' || *ep != '\0')
				errx(1, "illegal signal number: %s", *argv);
			if (numsig < 0 || numsig >= sys_nsig)
				nosig(*argv);
		} else
			nosig(*argv);
		argc--, argv++;
	}

	if (argc > 0 && strncmp(*argv, "--", 2) == 0)
		argc--, argv++;

	if (argc == 0)
		usage();

	for (errors = 0; argc; argc--, argv++) {
		pid = (pid_t)strtol(*argv, &ep, 10);
		if (**argv == '\0' || *ep != '\0') {
			warnx("illegal process id: %s", *argv);
			errors = 1;
		} else if (kill(pid, numsig) == -1) {
			warn("%s", *argv);
			errors = 1;
		}
	}

	exit(errors);
}

static int
signame_to_signum(const char *sig)
{
	int n;

	if (strncasecmp(sig, "sig", 3) == 0)
		sig += 3;
	for (n = 1; n < sys_nsig; n++) {
		if (strcasecmp(sys_signame[n], sig) == 0)
			return(n);
	}
	return(-1);
}

static void
nosig(const char *name)
{
	warnx("unknown signal %s; valid signals:", name);
	printsignals(stderr);
	exit(1);
}

static void
printsignals(FILE *fp)
{
	int n;

	for (n = 1; n < sys_nsig; n++) {
		fprintf(fp, "%s", sys_signame[n]);
		if (n == (sys_nsig / 2) || n == (sys_nsig - 1))
			fprintf(fp, "\n");
		else
			fprintf(fp, " ");
	}
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: kill [-s signal_name] pid ...",
		"       kill -l [exit_status]",
		"       kill -signal_name pid ...",
		"       kill -signal_number pid ...");
	exit(1);
}
