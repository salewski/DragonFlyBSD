/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
 * @(#)net.c	8.4 (Berkeley) 4/28/95
 * $FreeBSD: src/usr.bin/finger/net.c,v 1.12.2.4 2002/12/16 08:41:28 roam Exp $
 * $DragonFly: src/usr.bin/finger/net.c,v 1.5 2004/09/03 19:13:23 dillon Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <ctype.h>
#include <db.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include "finger.h"

extern int lflag;		/* XXX finger.h? */
extern int Tflag;		/* XXX finger.h? */
extern sa_family_t family;

static void cleanup(int sig);
static int do_protocol(const char *name, const struct addrinfo *ai);
static void trying(const struct addrinfo *ai);

void
netfinger(char *name)
{
	int error, multi;
	char *host;
	struct addrinfo *ai, *ai0;
	static struct addrinfo hint;

	host = strrchr(name, '@');
	if (host == 0)
		return;
	*host++ = '\0';
	signal(SIGALRM, cleanup);
	alarm(TIME_LIMIT);

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = family;
	hint.ai_socktype = SOCK_STREAM;

	error = getaddrinfo(host, "finger", &hint, &ai0);
	if (error) {
		warnx("%s: %s", host, gai_strerror(error));
		return;
	}

	multi = (ai0->ai_next) != 0;

	/* ai_canonname may not be filled in if the user specified an IP. */
	if (ai0->ai_canonname == 0)
		printf("[%s]\n", host);
	else
		printf("[%s]\n", ai0->ai_canonname);

	for (ai = ai0; ai != 0; ai = ai->ai_next) {
		if (multi)
			trying(ai);

		error = do_protocol(name, ai);
		if (!error)
			break;
	}
	alarm(0);
	freeaddrinfo(ai0);
}

static int
do_protocol(const char *name, const struct addrinfo *ai)
{
	int cnt, line_len, s;
	FILE *fp;
	int c, lastc;
	struct iovec iov[3];
	struct msghdr msg;
	static char slash_w[] = "/W ";
	static char neteol[] = "\r\n";

	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s < 0) {
		warn("socket(%d, %d, %d)", ai->ai_family, ai->ai_socktype,
		     ai->ai_protocol);
		return -1;
	}

	msg.msg_name = (void *)ai->ai_addr;
	msg.msg_namelen = ai->ai_addrlen;
	msg.msg_iov = iov;
	msg.msg_iovlen = 0;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	/* -l flag for remote fingerd  */
	if (lflag) {
		iov[msg.msg_iovlen].iov_base = slash_w;
		iov[msg.msg_iovlen++].iov_len = 3;
	}
	/* send the name followed by <CR><LF> */
	iov[msg.msg_iovlen].iov_base = strdup(name);
	iov[msg.msg_iovlen++].iov_len = strlen(name);
	iov[msg.msg_iovlen].iov_base = neteol;
	iov[msg.msg_iovlen++].iov_len = 2;

	/*
	 * -T disables data-on-SYN: compatibility option to finger broken
	 * hosts.  Also, the implicit-open API is broken on IPv6, so do
	 * the explicit connect there, too.
	 */
	if ((Tflag || ai->ai_addr->sa_family == AF_INET6)
	    && connect(s, ai->ai_addr, ai->ai_addrlen) < 0) {
		warn("connect");
		close(s);
		return -1;
	}

	if (sendmsg(s, &msg, 0) < 0) {
		warn("sendmsg");
		close(s);
		return -1;
	}

	/*
	 * Read from the remote system; once we're connected, we assume some
	 * data.  If none arrives, we hang until the user interrupts.
	 *
	 * If we see a <CR> or a <CR> with the high bit set, treat it as
	 * a newline; if followed by a newline character, only output one
	 * newline.
	 *
	 * Otherwise, all high bits are stripped; if it isn't printable and
	 * it isn't a space, we can simply set the 7th bit.  Every ASCII
	 * character with bit 7 set is printable.
	 */
	lastc = 0;
	if ((fp = fdopen(s, "r")) != NULL) {
		cnt = 0;
		line_len = 0;
		while ((c = getc(fp)) != EOF) {
			if (++cnt > OUTPUT_MAX) {
				printf("\n\n Output truncated at %d bytes...\n",
					cnt - 1);
				break;
			}
			if (c == 0x0d) {
				if (lastc == '\r')	/* ^M^M - skip dupes */
					continue;
				c = '\n';
				lastc = '\r';
			} else {
				if (!(eightflag || isprint(c) || isspace(c))) {
					c &= 0x7f;
					c |= 0x40;
				}
				if (lastc != '\r' || c != '\n')
					lastc = c;
				else {
					lastc = '\n';
					continue;
				}
			}
			putchar(c);
			if (c != '\n' && ++line_len > _POSIX2_LINE_MAX) {
				putchar('\\');
				putchar('\n');
				lastc = '\r';
			}
			if (lastc == '\n' || lastc == '\r')
				line_len = 0;
		}
		if (ferror(fp)) {
			/*
			 * Assume that whatever it was set errno...
			 */
			warn("reading from network");
		}
		if (lastc != '\n')
			putchar('\n');

		fclose(fp);
	}
	return 0;
}

static void
trying(const struct addrinfo *ai)
{
	char buf[NI_MAXHOST];

	if (getnameinfo(ai->ai_addr, ai->ai_addrlen, buf, sizeof buf,
			(char *)0, 0, NI_NUMERICHOST) != 0)
		return;		/* XXX can't happen */

	printf("Trying %s...\n", buf);
}

void
cleanup(int sig __unused)
{
#define	ERRSTR	"Timed out.\n"
	write(STDERR_FILENO, ERRSTR, sizeof ERRSTR);
	exit(1);
}

