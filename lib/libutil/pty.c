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
 * @(#)pty.c	8.3 (Berkeley) 5/16/94
 * $FreeBSD: src/lib/libutil/pty.c,v 1.10 1999/08/28 00:05:51 peter Exp $
 * $DragonFly: src/lib/libutil/pty.c,v 1.5 2005/03/04 05:22:57 cpressey Exp $
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "libutil.h"

int
openpty(int *amaster, int *aslave, char *name, struct termios *termp,
	struct winsize *winp)
{
	char line[] = "/dev/ptyXX";
	const char *cp1, *cp2;
	int master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrsPQRS"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdefghijklmnopqrstuv"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					return (-1);	/* out of ptys */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR|S_IWUSR|S_IWGRP);
				(void) revoke(line);
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
							TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
							(char *)winp);
					return (0);
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT;	/* out of ptys */
	return (-1);
}

int
forkpty(int *amaster, char *name, struct termios *termp, struct winsize *winp)
{
	int master, slave, pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return (-1);
	switch (pid = fork()) {
	case -1:
		return (-1);
	case 0:
		/*
		 * child
		 */
		(void) close(master);
		login_tty(slave);
		return (0);
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void) close(slave);
	return (pid);
}
