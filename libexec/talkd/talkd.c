/*
 * Copyright (c) 1983, 1993
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
 * @(#) Copyright (c) 1983, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)talkd.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/libexec/talkd/talkd.c,v 1.11.2.1 2001/10/18 12:30:42 des Exp $
 * $DragonFly: src/libexec/talkd/talkd.c,v 1.3 2003/11/14 03:54:31 dillon Exp $
 */

/*
 * The top level of the daemon, the format is heavily borrowed
 * from rwhod.c. Basically: find out who and where you are;
 * disconnect all descriptors and ttys, and then endless
 * loop on waiting for and processing requests
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <protocols/talkd.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

CTL_MSG		request;
CTL_RESPONSE	response;

int	sockt;
int	debug = 0;
long	lastmsgtime;

char    hostname[MAXHOSTNAMELEN];

#define TIMEOUT 30
#define MAXIDLE 120

void process_request (CTL_MSG *, CTL_RESPONSE *);
void timeout();

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register CTL_MSG *mp = &request;
	int cc;

#ifdef NOTDEF
	/*
	 * removed so ntalkd can run in tty sandbox
	 */
	if (getuid())
		errx(1, "getuid: not super-user");
#endif
	openlog("talkd", LOG_PID, LOG_DAEMON);
	if (gethostname(hostname, sizeof(hostname) - 1) < 0) {
		syslog(LOG_ERR, "gethostname: %m");
		_exit(1);
	}
	hostname[sizeof(hostname) - 1] = '\0';
	if (chdir(_PATH_DEV) < 0) {
		syslog(LOG_ERR, "chdir: %s: %m", _PATH_DEV);
		_exit(1);
	}
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
		debug = 1;
	signal(SIGALRM, timeout);
	alarm(TIMEOUT);
	for (;;) {
		cc = recv(0, (char *)mp, sizeof (*mp), 0);
		if (cc != sizeof (*mp)) {
			if (cc < 0 && errno != EINTR)
				syslog(LOG_WARNING, "recv: %m");
			continue;
		}
		lastmsgtime = time(0);
		process_request(mp, &response);
		/* can block here, is this what I want? */
		mp->ctl_addr.sa_family = htons(mp->ctl_addr.sa_family);
		cc = sendto(sockt, (char *)&response,
		    sizeof (response), 0, (struct sockaddr *)&mp->ctl_addr,
		    sizeof (mp->ctl_addr));
		if (cc != sizeof (response))
			syslog(LOG_WARNING, "sendto: %m");
	}
}

void
timeout()
{
	int save_errno = errno;

	if (time(0) - lastmsgtime >= MAXIDLE)
		_exit(0);
	alarm(TIMEOUT);
	errno = save_errno;
}
