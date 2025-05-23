/*-
 * Copyright (c) 1988, 1993
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
 * @(#)logwtmp.c	8.1 (Berkeley) 6/4/93
 * $FreeBSD: src/lib/libutil/logwtmp.c,v 1.14 2000/01/15 03:26:54 shin Exp $
 * $DragonFly: src/lib/libutil/logwtmp.c,v 1.4 2005/03/04 04:31:11 cpressey Exp $
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "libutil.h"

/* wrapper for KAME-special getnameinfo() */
#ifndef NI_WITHSCOPEID
#define	NI_WITHSCOPEID	0
#endif


void
logwtmp(const char *line, const char *name, const char *host)
{
	struct utmp ut;
	struct stat buf;
	char   fullhost[MAXHOSTNAMELEN];
	int fd;
	
	strncpy(fullhost, host, sizeof(fullhost) - 1);	
	fullhost[sizeof(fullhost) - 1] = '\0';
	trimdomain(fullhost, UT_HOSTSIZE);
	host = fullhost;

	if (strlen(host) > UT_HOSTSIZE) {
		int error;
		struct addrinfo hints, *res;

		bzero(&hints, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_CANONNAME;
		error = getaddrinfo(host, NULL, &hints, &res);
		if (error != 0 || res->ai_addr == NULL)
			host = "invalid hostname";
		else {
			error = getnameinfo(res->ai_addr, res->ai_addrlen,
					  fullhost, strlen(fullhost), NULL, 0,
					  NI_NUMERICHOST|NI_WITHSCOPEID);
			if (error != 0) {
			  fprintf(stderr, "%d", error);
				host = "invalid hostname";
			}
		}
	}

	if ((fd = open(_PATH_WTMP, O_WRONLY|O_APPEND, 0)) < 0)
		return;
	if (fstat(fd, &buf) == 0) {
		(void) strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void) strncpy(ut.ut_name, name, sizeof(ut.ut_name));
		(void) strncpy(ut.ut_host, host, sizeof(ut.ut_host));
		(void) time(&ut.ut_time);
		if (write(fd, (char *)&ut, sizeof(struct utmp)) !=
		    sizeof(struct utmp))
			(void) ftruncate(fd, buf.st_size);
	}
	(void) close(fd);
}
