/*
 * Copyright (c) 1983, 1993, 1994
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
 * @(#)startdaemon.c	8.2 (Berkeley) 4/17/94
 * $FreeBSD: src/usr.sbin/lpr/common_source/startdaemon.c,v 1.8.2.2 2002/04/26 18:24:40 gad Exp $
 * $DragonFly: src/usr.sbin/lpr/common_source/startdaemon.c,v 1.4 2004/12/18 22:48:03 swildner Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lp.h"
#include "pathnames.h"

extern uid_t	uid, euid;

/*
 * Tell the printer daemon that there are new files in the spool directory.
 */

int
startdaemon(const struct printer *pp)
{
	struct sockaddr_un un;
	int s, n;
	int connectres;
	char c;

	s = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (s < 0) {
		warn("socket");
		return(0);
	}
	memset(&un, 0, sizeof(un));
	un.sun_family = AF_LOCAL;
	strcpy(un.sun_path, _PATH_SOCKETNAME);
#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	seteuid(euid);
	connectres = connect(s, (struct sockaddr *)&un, SUN_LEN(&un));
	seteuid(uid);
	if (connectres < 0) {
		warn("Unable to connect to %s", _PATH_SOCKETNAME);
		warnx("Check to see if the master 'lpd' process is running.");
		close(s);
		return(0);
	}

	/*
	 * Avoid overruns without putting artificial limitations on 
	 * the length.
	 */
	if (writel(s, "\1", pp->printer, "\n", (char *)0) <= 0) {
		warn("write");
		close(s);
		return(0);
	}
	if (read(s, &c, 1) == 1) {
		if (c == '\0') {		/* everything is OK */
			close(s);
			return(1);
		}
		putchar(c);
	}
	while ((n = read(s, &c, 1)) > 0)
		putchar(c);
	close(s);
	return(0);
}
