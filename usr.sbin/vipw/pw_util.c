/*-
 * Copyright (c) 1990, 1993, 1994
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
 * @(#)pw_util.c	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/usr.sbin/vipw/pw_util.c,v 1.17.2.4 2002/09/04 15:28:10 des Exp $
 * $DragonFly: src/usr.sbin/vipw/pw_util.c,v 1.3 2004/12/18 22:48:14 swildner Exp $
 */

/*
 * This file is used by all the "password" programs; vipw(8), chpass(1),
 * and passwd(1).
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pw_util.h"

extern char *tempname;
static pid_t editpid = -1;
static int lockfd;
char *mppath = _PATH_PWD;
char *masterpasswd = _PATH_MASTERPASSWD;

void
pw_cont(sig)
	int sig;
{

	if (editpid != -1)
		kill(editpid, sig);
}

void
pw_init()
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CPU, &rlim);
	setrlimit(RLIMIT_FSIZE, &rlim);
	setrlimit(RLIMIT_STACK, &rlim);
	setrlimit(RLIMIT_DATA, &rlim);
	setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	setrlimit(RLIMIT_CORE, &rlim);

	/* Turn off signals. */
	signal(SIGALRM, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGCONT, pw_cont);

	/* Create with exact permissions. */
	umask(0);
}

int
pw_lock()
{
	/*
	 * If the master password file doesn't exist, the system is hosed.
	 * Might as well try to build one.  Set the close-on-exec bit so
	 * that users can't get at the encrypted passwords while editing.
	 * Open should allow flock'ing the file; see 4.4BSD.	XXX
	 */
	for (;;) {
		struct stat st;

		lockfd = open(masterpasswd, O_RDONLY, 0);
		if (lockfd < 0 || fcntl(lockfd, F_SETFD, 1) == -1)
			err(1, "%s", masterpasswd);
		if (flock(lockfd, LOCK_EX|LOCK_NB))
			errx(1, "the password db file is busy");

		/*
		 * If the password file was replaced while we were trying to
		 * get the lock, our hardlink count will be 0 and we have to
		 * close and retry.
		 */
		if (fstat(lockfd, &st) < 0)
			errx(1, "fstat() failed");
		if (st.st_nlink != 0)
			break;
		close(lockfd);
		lockfd = -1;
	}
	return (lockfd);
}

int
pw_tmp()
{
	static char path[MAXPATHLEN];
	int fd;
	char *p;

	if ((p = strrchr(masterpasswd, '/')) == NULL)
		strcpy(path, "pw.XXXXXX");
	else
		if (snprintf(path, sizeof path, "%.*s/pw.XXXXXX",
		    (int)(p - masterpasswd), masterpasswd) >= sizeof path)
			errx(1, "%s: path too long", masterpasswd);
	if ((fd = mkstemp(path)) == -1)
		err(1, "%s", path);
	tempname = path;
	return (fd);
}

int
pw_mkdb(username)
char *username;
{
	int pstat;
	pid_t pid;

	fflush(stderr);
	if (!(pid = fork())) {
		if(!username) {
			warnx("rebuilding the database...");
			execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", "-d", mppath,
			    tempname, NULL);
		} else {
			warnx("updating the database...");
			execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", "-d", mppath,
			    "-u", username, tempname, NULL);
		}
		pw_error(_PATH_PWD_MKDB, 1, 1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return (0);
	warnx("done");
	return (1);
}

void
pw_edit(notsetuid)
	int notsetuid;
{
	int pstat;
	char *p, *editor;

	if (!(editor = getenv("EDITOR")))
		editor = _PATH_VI;
	if ((p = strrchr(editor, '/')))
		++p;
	else
		p = editor;

	if (!(editpid = fork())) {
		if (notsetuid) {
			setgid(getgid());
			setuid(getuid());
		}
		errno = 0;
		execlp(editor, p, tempname, NULL);
		_exit(errno);
	}
	for (;;) {
		editpid = waitpid(editpid, (int *)&pstat, WUNTRACED);
		errno = WEXITSTATUS(pstat);
		if (editpid == -1)
			pw_error(editor, 1, 1);
		else if (WIFSTOPPED(pstat))
			raise(WSTOPSIG(pstat));
		else if (WIFEXITED(pstat) && errno == 0)
			break;
		else
			pw_error(editor, 1, 1);
	}
	editpid = -1;
}

void
pw_prompt()
{
	int c, first;

	printf("re-edit the password file? [y]: ");
	fflush(stdout);
	first = c = getchar();
	while (c != '\n' && c != EOF)
		c = getchar();
	if (first == 'n')
		pw_error(NULL, 0, 0);
}

void
pw_error(name, err, eval)
	char *name;
	int err, eval;
{
#ifdef YP
	extern int _use_yp;
#endif /* YP */
	if (err) {
		if (name != NULL)
			warn("%s", name);
		else
			warn(NULL);
	}
#ifdef YP
	if (_use_yp)
		warnx("NIS information unchanged");
	else
#endif /* YP */
	warnx("%s: unchanged", masterpasswd);
	unlink(tempname);
	exit(eval);
}
