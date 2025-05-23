/*-
 * Copyright (c) 2000 Peter Wemm <peter@FreeBSD.org>
 * Copyright (c) 2000 Paul Saab <ps@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/killall/killall.c,v 1.5.2.4 2001/05/19 19:22:49 phk Exp $
 * $DragonFly: src/usr.bin/killall/killall.c,v 1.7 2004/09/14 00:33:53 drhodus Exp $
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <regex.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

static char	*prog;

static void __dead2
usage(void)
{

	fprintf(stderr, "usage: %s [-l] [-v] [-m] [-sig] [-u user] [-t tty] [-c cmd] [cmd]...\n", prog);
	fprintf(stderr, "At least one option or argument to specify processes must be given.\n");
	exit(1);
}

static char *
upper(const char *str)
{
	static char buf[80];
	char *s;

	strlcpy(buf, str, sizeof(buf));
	for (s = buf; *s; s++)
		*s = toupper((unsigned char)*s);
	return buf;
}


static void
printsig(FILE *fp)
{
	const char	*const * p;
	int		cnt;
	int		offset = 0;

	for (cnt = NSIG, p = sys_signame + 1; --cnt; ++p) {
		offset += fprintf(fp, "%s ", upper(*p));
		if (offset >= 75 && cnt > 1) {
			offset = 0;
			fprintf(fp, "\n");
		}
	}
	fprintf(fp, "\n");
}

static void
nosig(char *name)
{

	warnx("unknown signal %s; valid signals:", name);
	printsig(stderr);
	exit(1);
}

int
main(int ac, char **av)
{
	struct kinfo_proc *procs = NULL, *newprocs;
	struct stat	sb;
	struct passwd	*pw;
	regex_t		rgx;
	regmatch_t	pmatch;
	int		i, j;
	char		buf[256];
	char		*user = NULL;
	char		*tty = NULL;
	char		*cmd = NULL;
	int		qflag = 0;
	int		vflag = 0;
	int		sflag = 0;
	int		dflag = 0;
	int		mflag = 0;
	uid_t		uid = 0;
	dev_t		tdev = 0;
	pid_t		mypid;
	char		thiscmd[MAXCOMLEN + 1];
	pid_t		thispid;
	uid_t		thisuid;
	dev_t		thistdev;
	int		sig = SIGTERM;
	const char *const *p;
	char		*ep;
	int		errors = 0;
	int		mib[4];
	size_t		miblen;
	int		st, nprocs;
	size_t		size;
	int		matched;
	int		killed = 0;

	prog = av[0];
	av++;
	ac--;

	while (ac > 0) {
		if (strcmp(*av, "-l") == 0) {
			printsig(stdout);
			exit(0);
		}
		if (strcmp(*av, "-help") == 0)
			usage();
		if (**av == '-') {
			++*av;
			switch (**av) {
			case 'u':
				++*av;
				if (**av == '\0')
					++av;
				--ac;
				user = *av;
				break;
			case 't':
				++*av;
				if (**av == '\0')
					++av;
				--ac;
				tty = *av;
				break;
			case 'c':
				++*av;
				if (**av == '\0')
					++av;
				--ac;
				cmd = *av;
				break;
			case 'q':
				qflag++;
				break;
			case 'v':
				vflag++;
				break;
			case 's':
				sflag++;
				break;
			case 'd':
				dflag++;
				break;
			case 'm':
				mflag++;
				break;
			default:
				if (isalpha((unsigned char)**av)) {
					if (strncasecmp(*av, "sig", 3) == 0)
						*av += 3;
					for (sig = NSIG, p = sys_signame + 1;
					     --sig; ++p)
						if (strcasecmp(*p, *av) == 0) {
							sig = p - sys_signame;
							break;
						}
					if (!sig)
						nosig(*av);
				} else if (isdigit((unsigned char)**av)) {
					sig = strtol(*av, &ep, 10);
					if (!*av || *ep)
						errx(1, "illegal signal number: %s", *av);
					if (sig < 0 || sig >= NSIG)
						nosig(*av);
				} else
					nosig(*av);
			}
			++av;
			--ac;
		} else {
			break;
		}
	}

	if (user == NULL && tty == NULL && cmd == NULL && ac == 0)
		usage();

	if (tty) {
		if (strncmp(tty, "/dev/", 5) == 0)
			snprintf(buf, sizeof(buf), "%s", tty);
		else if (strncmp(tty, "tty", 3) == 0)
			snprintf(buf, sizeof(buf), "/dev/%s", tty);
		else
			snprintf(buf, sizeof(buf), "/dev/tty%s", tty);
		if (stat(buf, &sb) < 0)
			err(1, "stat(%s)", buf);
		if (!S_ISCHR(sb.st_mode))
			errx(1, "%s: not a character device", buf);
		tdev = sb.st_rdev;
		if (dflag)
			printf("ttydev:0x%x\n", tdev);
	}
	if (user) {
		pw = getpwnam(user);
		if (pw == NULL)
			errx(1, "user %s does not exist", user);
		uid = pw->pw_uid;
		if (dflag)
			printf("uid:%d\n", uid);
	} else {
		uid = getuid();
		if (uid != 0) {
			pw = getpwuid(uid);
			if (pw)
				user = pw->pw_name;
			if (dflag)
				printf("uid:%d\n", uid);
		}
	}
	size = 0;
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;
	miblen = 3;

	if (user && mib[2] == KERN_PROC_ALL) {
		mib[2] = KERN_PROC_RUID;
		mib[3] = uid;
		miblen = 4;
	}
	if (tty && mib[2] == KERN_PROC_ALL) {
		mib[2] = KERN_PROC_TTY;
		mib[3] = tdev;
		miblen = 4;
	}

	st = sysctl(mib, miblen, NULL, &size, NULL, 0);
	do {
		size += size / 10;
		newprocs = realloc(procs, size);
		if (newprocs == 0) {
			if (procs)
				free(procs);
			errx(1, "could not reallocate memory");
		}
		procs = newprocs;
		st = sysctl(mib, miblen, procs, &size, NULL, 0);
	} while (st == -1 && errno == ENOMEM);
	if (st == -1)
		err(1, "could not sysctl(KERN_PROC)");
	if (size % sizeof(struct kinfo_proc) != 0) {
		fprintf(stderr, "proc size mismatch (%zu total, %zu chunks)\n",
			size, sizeof(struct kinfo_proc));
		fprintf(stderr, "userland out of sync with kernel, recompile libkvm etc\n");
		exit(1);
	}
	nprocs = size / sizeof(struct kinfo_proc);
	if (dflag)
		printf("nprocs %d\n", nprocs);
	mypid = getpid();

	for (i = 0; i < nprocs; i++) {
		thispid = procs[i].kp_proc.p_pid;
		strncpy(thiscmd, procs[i].kp_thread.td_comm, MAXCOMLEN);
		thiscmd[MAXCOMLEN] = '\0';
		thistdev = procs[i].kp_eproc.e_tdev;
		thisuid = procs[i].kp_eproc.e_ucred.cr_ruid;	/* real uid */

		if (thispid == mypid)
			continue;
		matched = 1;
		if ((int)procs[i].kp_proc.p_pid < 0)
			matched = 0;
		if (user) {
			if (thisuid != uid)
				matched = 0;
		}
		if (tty) {
			if (thistdev != tdev)
				matched = 0;
		}
		if (cmd) {
			if (mflag) {
				if (regcomp(&rgx, cmd,
				    REG_EXTENDED|REG_NOSUB) != 0) {
					mflag = 0;
					warnx("%s: illegal regexp", cmd);
				}
			}
			if (mflag) {
				pmatch.rm_so = 0;
				pmatch.rm_eo = strlen(thiscmd);
				if (regexec(&rgx, thiscmd, 0, &pmatch,
				    REG_STARTEND) != 0)
					matched = 0;
				regfree(&rgx);
			} else {
				if (strcmp(thiscmd, cmd) != 0)
					matched = 0;
			}
		}
		if (matched == 0)
			continue;
		matched = 0;
		for (j = 0; j < ac; j++) {
			if (mflag) {
				if (regcomp(&rgx, av[j],
				    REG_EXTENDED|REG_NOSUB) != 0) {
					mflag = 0;
					warnx("%s: illegal regexp", av[j]);
				}
			}
			if (mflag) {
				pmatch.rm_so = 0;
				pmatch.rm_eo = strlen(thiscmd);
				if (regexec(&rgx, thiscmd, 0, &pmatch,
				    REG_STARTEND) == 0)
					matched = 1;
				regfree(&rgx);
			} else {
				if (strcmp(thiscmd, av[j]) == 0)
					matched = 1;
			}
			if (matched)
				break;
		}
		if (matched == 0)
			continue;
		if (dflag)
			printf("sig:%d, cmd:%s, pid:%d, dev:0x%x uid:%d\n", sig,
			    thiscmd, thispid, thistdev, thisuid);

		if (vflag || sflag)
			printf("kill -%s %d\n", upper(sys_signame[sig]),
			    thispid);

		killed++;
		if (!dflag && !sflag) {
			if (kill(thispid, sig) < 0 /* && errno != ESRCH */ ) {
				warn("kill -%s %d", upper(sys_signame[sig]),
				    thispid);
				errors = 1;
			}
		}
	}
	if (!qflag && killed == 0) {
		fprintf(stderr, "No matching processes %swere found\n",
		    getuid() != 0 ? "belonging to you " : "");
		errors = 1;
	}
	exit(errors);
}
