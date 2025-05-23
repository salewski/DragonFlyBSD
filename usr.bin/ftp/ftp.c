/* $FreeBSD: src/usr.bin/ftp/ftp.c,v 1.28.2.5 2002/07/25 15:29:18 ume Exp $	*/
/* $DragonFly: src/usr.bin/ftp/Attic/ftp.c,v 1.6 2004/06/19 18:55:48 joerg Exp $	*/
/*	$NetBSD: ftp.c,v 1.29.2.1 1997/11/18 01:01:04 mellon Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
 * @(#)ftp.c	8.6 (Berkeley) 10/27/94
 * $NetBSD: ftp.c,v 1.29.2.1 1997/11/18 01:01:04 mellon Exp $
 * $FreeBSD: src/usr.bin/ftp/ftp.c,v 1.28.2.5 2002/07/25 15:29:18 ume Exp $
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ftp_var.h"

int	data = -1;
int	abrtflag = 0;
jmp_buf	ptabort;
int	ptabflg;
int	ptflag = 0;

FILE	*cin, *cout;

union sockunion {
	struct sockinet {
		u_char si_len;
		u_char si_family;
		u_short si_port;
	} su_si;
	struct sockaddr_in  su_sin;
	struct sockaddr_in6 su_sin6;
};
#define su_len		su_si.si_len
#define su_family	su_si.si_family
#define su_port		su_si.si_port

union sockunion myctladdr, hisctladdr, data_addr;

char *
hookup(const char *host0, char *port)
{
	int s, len, tos, error;
	struct addrinfo hints, *res, *res0;
	static char hostnamebuf[MAXHOSTNAMELEN];
	char *host;

	if (*host0 == '[' && strrchr(host0, ']') != NULL) { /*IPv6 addr in []*/
		strncpy(hostnamebuf, host0 + 1, strlen(host0) - 2);
		hostnamebuf[strlen(host0) - 2] = '\0';
	} else {
		strncpy(hostnamebuf, host0, strlen(host0));
		hostnamebuf[strlen(host0)] = '\0';
	}
	host = hostnamebuf;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_CANONNAME;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		warnx("%s: %s", host, gai_strerror(error));
		if (error == EAI_SYSTEM)
			warnx("%s: %s", host, strerror(errno));
		code = -1;
		return (0);
	}

	res = res0;
	if (res->ai_canonname)
		(void) strncpy(hostnamebuf, res->ai_canonname,
			       sizeof(hostnamebuf));
	hostname = hostnamebuf;
	while (1) {
		/*
		 * make sure that ai_addr is NOT an IPv4 mapped address.
		 * IPv4 mapped address complicates too many things in FTP
		 * protocol handling, as FTP protocol is defined differently
		 * between IPv4 and IPv6.
		 */
		ai_unmapped(res);
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			res = res->ai_next;
			if (res)
				continue;
			warn("socket");
			code = -1;
			return (0);
		}
		if (dobind) {
			struct addrinfo *bindres;
			int binderr = -1;

			for (bindres = bindres0;
			     bindres != NULL;
			     bindres = bindres->ai_next)
				if (bindres->ai_family == res->ai_family)
					break;
			if (bindres == NULL)
				bindres = bindres0;
			binderr = bind(s, bindres->ai_addr,
				       bindres->ai_addrlen);
			if (binderr == -1)
		      {
			res = res->ai_next;
			if (res) {
				(void)close(s);
				continue;
			}
			warn("bind");
			code = -1;
			goto bad;
		      }
		}
		if (connect(s, res->ai_addr, res->ai_addrlen) == 0)
			break;
		if (res->ai_next) {
			char hname[NI_MAXHOST];
			getnameinfo(res->ai_addr, res->ai_addrlen,
				    hname, sizeof(hname) - 1, NULL, 0,
				    NI_NUMERICHOST);
			warn("connect to address %s", hname);
			res = res->ai_next;
			getnameinfo(res->ai_addr, res->ai_addrlen,
				    hname, sizeof(hname) - 1, NULL, 0,
				    NI_NUMERICHOST);
			printf("Trying %s...\n", hname);
			(void)close(s);
			continue;
		}
		warn("connect");
		code = -1;
		goto bad;
	}
	memcpy(&hisctladdr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res0);
	len = sizeof(myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		warn("getsockname");
		code = -1;
		goto bad;
	}
#ifdef IP_TOS
	if (myctladdr.su_family == AF_INET)
      {
	     tos = IPTOS_LOWDELAY;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
      }
#endif
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		warnx("fdopen failed.");
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void)fclose(cin);
		if (cout)
			(void)fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on))
		< 0 && debug) {
			warn("setsockopt");
		}
	}
#endif /* SO_OOBINLINE */

	return (hostname);
bad:
	(void)close(s);
	return ((char *)0);
}

void
cmdabort(int notused)
{

	alarmtimer(0);
	putchar('\n');
	(void)fflush(stdout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort, 1);
}

/*VARARGS*/
int
command(const char *fmt, ...)
{
	va_list ap;
	int r;
	sig_t oldintr;
	abrtflag = 0;
	if (debug) {
		fputs("---> ", stdout);
		va_start(ap, fmt);
		if (strncmp("PASS ", fmt, 5) == 0)
			fputs("PASS XXXX", stdout);
		else if (strncmp("ACCT ", fmt, 5) == 0)
			fputs("ACCT XXXX", stdout);
		else
			vprintf(fmt, ap);
		va_end(ap);
		putchar('\n');
		(void)fflush(stdout);
	}
	if (cout == NULL) {
		warnx("No control connection for command.");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
	va_start(ap, fmt);
	vfprintf(cout, fmt, ap);
	va_end(ap);
	fputs("\r\n", cout);
	(void)fflush(cout);
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)(SIGINT);
	(void)signal(SIGINT, oldintr);
	return (r);
}

char reply_string[BUFSIZ];		/* first line of previous reply */

int
getreply(int expecteof)
{
	char current_line[BUFSIZ];	/* last line of previous reply */
	int c, n, line;
	int dig;
	int originalcode = 0, continuation = 0;
	sig_t oldintr;
	int pflag = 0;
	char *cp, *pt = pasv;

	oldintr = signal(SIGINT, cmdabort);
	for (line = 0 ;; line++) {
		dig = n = code = 0;
		cp = current_line;
		while ((c = getc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void)fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void)fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void)signal(SIGINT, oldintr);
					code = 221;
					return (0);
				}
				lostpeer();
				if (verbose) {
					puts(
"421 Service not available, remote server has closed connection.");
					(void)fflush(stdout);
				}
				code = 421;
				return (4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				   (dig == 1 || (dig == 5 && verbose == 0)))
					printf("%s:", hostname);
				(void)putchar(c);
			}
			if (dig < 4 && isdigit((unsigned char)c))
				code = code * 10 + (c - '0');
			switch (pflag) {
			case 0:
				if (code == 227 || code == 228) {
					/* result for PASV/LPSV */
					pflag = 1;
					/* fall through */
				} else if (code == 229) {
					/* result for EPSV */
					pflag = 100;
					break;
				} else
					break;
			case 1:
				if (!(dig > 4 && isdigit((unsigned char)c)))
					break;
				pflag = 2;
				/* fall through */
			case 2:
				if (c != '\r' && c != ')' &&
				    pt < &pasv[sizeof(pasv)-1])
					*pt++ = c;
				else {
					*pt = '\0';
					pflag = 3;
				}
				break;
			case 100:
				if (dig > 4 && c == '(')
					pflag = 2;
				break;
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &current_line[sizeof(current_line) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || (verbose > -1 && n == '5')) {
			(void)putchar(c);
			(void)fflush (stdout);
		}
		if (line == 0) {
			size_t len = cp - current_line;

			if (len > sizeof(reply_string))
				len = sizeof(reply_string);

			(void)strncpy(reply_string, current_line, len);
			reply_string[len] = '\0';
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void)signal(SIGINT, oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer();
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)(SIGINT);
		return (n - '0');
	}
}

int
empty(fd_set *mask, int sec)
{
	struct timeval t;

	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	return (select(32, mask, (fd_set *) 0, (fd_set *) 0, &t));
}

jmp_buf	sendabort;

void
abortsend(int notused)
{

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	puts("\nsend aborted\nwaiting for remote to finish abort.");
	(void)fflush(stdout);
	longjmp(sendabort, 1);
}

void
sendrequest(const char *cmd, const char *local, const char *remote,
            int printnames)
{
	struct stat st;
	int c, d;
	FILE *fin, *dout;
	int (*closefunc)(FILE *);
	sig_t oldinti, oldintr, oldintp;
	volatile off_t hashbytes;
	char *lmode, *bufp;
	int oprogress;
	static size_t bufsize;
	static char *buf;

#ifdef __GNUC__			/* XXX: to shut up gcc warnings */
	(void)&fin;
	(void)&dout;
	(void)&closefunc;
	(void)&oldinti;
	(void)&oldintr;
	(void)&oldintp;
	(void)&lmode;
#endif

	hashbytes = mark;
	direction = "sent";
	dout = NULL;
	bytes = 0;
	filesize = -1;
	oprogress = progress;
	if (verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy) {
		proxtrans(cmd, local, remote);
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	oldinti = NULL;
	lmode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		code = -1;
		goto cleanupsend;
	}
	oldintr = signal(SIGINT, abortsend);
	oldinti = signal(SIGINFO, psummary);
	if (strcmp(local, "-") == 0) {
		fin = stdin;
		progress = 0;
	} else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			warn("%s", local + 1);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGPIPE, oldintp);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			goto cleanupsend;
		}
		progress = 0;
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			warn("local: %s", local);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			goto cleanupsend;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode)) {
			printf("%s: not a plain file.\n", local);
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			fclose(fin);
			code = -1;
			goto cleanupsend;
		}
		filesize = st.st_size;
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		if (oldintp)
			(void)signal(SIGPIPE, oldintp);
		code = -1;
		if (closefunc != NULL)
			(*closefunc)(fin);
		goto cleanupsend;
	}
	if (fstat(fileno(fin), &st) < 0 || st.st_blksize < BUFSIZ)
		st.st_blksize = BUFSIZ;
	if (st.st_blksize > bufsize) {
		if (buf)
			(void)free(buf);
		buf = malloc((unsigned)st.st_blksize);
		if (buf == NULL) {
			warn("malloc");
			bufsize = 0;
			goto abort;
		}
		bufsize = st.st_blksize;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		int rc;

		rc = -1;
		switch (curtype) {
		case TYPE_A:
			rc = fseek(fin, (long) restart_point, SEEK_SET);
			break;
		case TYPE_I:
		case TYPE_L:
			rc = lseek(fileno(fin), restart_point, SEEK_SET);
			break;
		}
		if (rc < 0) {
			warn("local: %s", local);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
		if (command("REST %qd", (long long) restart_point) !=
		    CONTINUE) {
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
		lmode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			if (oldintp)
				(void)signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			goto cleanupsend;
		}
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;
	progressmeter(-1);
	oldintp = signal(SIGPIPE, SIG_IGN);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(fin), buf, bufsize)) > 0) {
			bytes += c;
			for (bufp = buf; c > 0; c -= d, bufp += d)
				if ((d = write(fileno(dout), bufp, c)) <= 0)
					break;
			if (hash && (!progress || filesize < 0) ) {
				while (bytes >= hashbytes) {
					(void)putchar('#');
					hashbytes += mark;
				}
				(void)fflush(stdout);
			}
		}
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putchar('#');
			(void)putchar('\n');
			(void)fflush(stdout);
		}
		if (c < 0)
			warn("local: %s", local);
		if (d < 0) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putchar('#');
					(void)fflush(stdout);
					hashbytes += mark;
				}
				if (ferror(dout))
					break;
				(void)putc('\r', dout);
				bytes++;
			}
			(void)putc(c, dout);
			bytes++;
#if 0	/* this violates RFC */
			if (c == '\r') {
				(void)putc('\0', dout);
				bytes++;
			}
#endif
		}
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putchar('#');
			(void)putchar('\n');
			(void)fflush(stdout);
		}
		if (ferror(fin))
			warn("local: %s", local);
		if (ferror(dout)) {
			if (errno != EPIPE)
				warn("netout");
			bytes = -1;
		}
		break;
	}
	progressmeter(1);
	if (closefunc != NULL)
		(*closefunc)(fin);
	(void)fclose(dout);
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer(0);
	goto cleanupsend;
abort:
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (dout)
		(void)fclose(dout);
	(void)getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (bytes > 0)
		ptransfer(0);
cleanupsend:
	progress = oprogress;
	restart_point = 0;
}

jmp_buf	recvabort;

void
abortrecv(int notused)
{

	alarmtimer(0);
	mflag = 0;
	abrtflag = 0;
	puts("\nreceive aborted\nwaiting for remote to finish abort.");
	(void)fflush(stdout);
	longjmp(recvabort, 1);
}

void
recvrequest(const char *cmd, const char *local, const char *remote,
            const char *lmode, int printnames, int ignorespecial)
{
	FILE *fout, *din;
	int (*closefunc)(FILE *);
	sig_t oldinti, oldintr, oldintp;
	int c, d;
	volatile int is_retr, tcrflag, bare_lfs;
	static size_t bufsize;
	static char *buf;
	volatile off_t hashbytes;
	struct stat st;
	time_t mtime;
	struct timeval tval[2];
	int oprogress;
	int opreserve;

#ifdef __GNUC__			/* XXX: to shut up gcc warnings */
	(void)&local;
	(void)&fout;
	(void)&din;
	(void)&closefunc;
	(void)&oldinti;
	(void)&oldintr;
	(void)&oldintp;
#endif

	fout = NULL;
	din = NULL;
	oldinti = NULL;
	hashbytes = mark;
	direction = "received";
	bytes = 0;
	bare_lfs = 0;
	filesize = -1;
	oprogress = progress;
	opreserve = preserve;
	is_retr = (strcmp(cmd, "RETR") == 0);
	if (is_retr && verbose && printnames) {
		if (local && (ignorespecial || *local != '-'))
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy && is_retr) {
		proxtrans(cmd, local, remote);
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void)getreply(0);
		}
		if (data >= 0) {
			(void)close(data);
			data = -1;
		}
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		progress = oprogress;
		preserve = opreserve;
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	oldinti = signal(SIGINFO, psummary);
	if (ignorespecial || (strcmp(local, "-") && *local != '|')) {
		if (access(local, W_OK) < 0) {
			char *dir = strrchr(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir == local ? "/" : dir ? local : ".", W_OK);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, 0600) < 0) {
				warn("local: %s", local);
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void)signal(SIGINT, oldintr);
				(void)signal(SIGINFO, oldinti);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else {
		if (curtype != type)
			changetype(type, 0);
		filesize = remotesize(remote, 0);
	}
	if (initconn()) {
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST %qd", (long long) restart_point) != CONTINUE)
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void)signal(SIGINT, oldintr);
			(void)signal(SIGINFO, oldinti);
			return;
		}
	}
	din = dataconn("r");
	if (din == NULL)
		goto abort;
	if (!ignorespecial && strcmp(local, "-") == 0) {
		fout = stdout;
		progress = 0;
		preserve = 0;
	} else if (!ignorespecial && *local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			warn("%s", local+1);
			goto abort;
		}
		progress = 0;
		preserve = 0;
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			warn("local: %s", local);
			goto abort;
		}
		closefunc = fclose;
	}
	if (fstat(fileno(fout), &st) < 0 || st.st_blksize < BUFSIZ)
		st.st_blksize = BUFSIZ;
	if (st.st_blksize > bufsize) {
		if (buf)
			(void)free(buf);
		buf = malloc((unsigned)st.st_blksize);
		if (buf == NULL) {
			warn("malloc");
			bufsize = 0;
			goto abort;
		}
		bufsize = st.st_blksize;
	}
	if (!S_ISREG(st.st_mode)) {
		progress = 0;
		preserve = 0;
	}
	progressmeter(-1);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (is_retr && restart_point &&
		    lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			warn("local: %s", local);
			progress = oprogress;
			preserve = opreserve;
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		errno = d = 0;
		while ((c = read(fileno(din), buf, bufsize)) > 0) {
			if ((d = write(fileno(fout), buf, c)) != c)
				break;
			bytes += c;
			if (hash && (!progress || filesize < 0)) {
				while (bytes >= hashbytes) {
					(void)putchar('#');
					hashbytes += mark;
				}
				(void)fflush(stdout);
			}
		}
		if (hash && (!progress || filesize < 0) && bytes > 0) {
			if (bytes < mark)
				(void)putchar('#');
			(void)putchar('\n');
			(void)fflush(stdout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				warn("local: %s", local);
			else
				warnx("%s: short write", local);
		}
		break;

	case TYPE_A:
		if (is_retr && restart_point) {
			int ch;
			long i, n;

			if (fseek(fout, 0L, SEEK_SET) < 0)
				goto done;
			n = (long)restart_point;
			for (i = 0; i++ < n;) {
				if ((ch = getc(fout)) == EOF)
					goto done;
				if (ch == '\n')
					i++;
			}
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
done:
				warn("local: %s", local);
				progress = oprogress;
				preserve = opreserve;
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}
		while ((c = getc(din)) != EOF) {
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (!progress || filesize < 0) &&
				    (bytes >= hashbytes)) {
					(void)putchar('#');
					(void)fflush(stdout);
					hashbytes += mark;
				}
				bytes++;
				if ((c = getc(din)) != '\n' || tcrflag) {
					if (ferror(fout))
						goto break2;
					(void)putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void)putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (bare_lfs) {
			printf(
"WARNING! %d bare linefeeds received in ASCII mode.\n", bare_lfs);
			puts("File may not have transferred correctly.");
		}
		if (hash && (!progress || filesize < 0)) {
			if (bytes < hashbytes)
				(void)putchar('#');
			(void)putchar('\n');
			(void)fflush(stdout);
		}
		if (ferror(din)) {
			if (errno != EPIPE)
				warn("netin");
			bytes = -1;
		}
		if (ferror(fout))
			warn("local: %s", local);
		break;
	}
	progressmeter(1);
	progress = oprogress;
	preserve = opreserve;
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)fclose(din);
	(void)getreply(0);
	if (bytes >= 0 && is_retr) {
		if (bytes > 0)
			ptransfer(0);
		if (preserve && (closefunc == fclose)) {
			mtime = remotemodtime(remote, 0);
			if (mtime != -1) {
				(void)gettimeofday(&tval[0],
				    (struct timezone *)0);
				tval[1].tv_sec = mtime;
				tval[1].tv_usec = 0;
				if (utimes(local, tval) == -1) {
					printf(
				"Can't change modification time on %s to %s",
					    local, asctime(localtime(&mtime)));
				}
			}
		}
	}
	return;

abort:

/* abort using RFC959 recommended IP,SYNC sequence */

	progress = oprogress;
	preserve = opreserve;
	if (oldintp)
		(void)signal(SIGPIPE, oldintp);
	(void)signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void)signal(SIGINT, oldintr);
		(void)signal(SIGINFO, oldinti);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void)close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void)fclose(din);
	if (bytes > 0)
		ptransfer(0);
	(void)signal(SIGINT, oldintr);
	(void)signal(SIGINFO, oldinti);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
int
initconn(void)
{
	char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;
	int error, ports;
	u_int af;
	u_int hal, h[16];
	u_int pal, prt[2];
	char *pasvcmd;

#ifdef INET6
	if (myctladdr.su_family == AF_INET6
	 && (IN6_IS_ADDR_LINKLOCAL(&myctladdr.su_sin6.sin6_addr)
	  || IN6_IS_ADDR_SITELOCAL(&myctladdr.su_sin6.sin6_addr))) {
		warnx("use of scoped address can be troublesome");
	}
#endif

	if (passivemode) {
#ifdef __GNUC__			/* XXX: to shut up gcc warnings */
		(void)&pasvcmd;
#endif
		data_addr = myctladdr;
		data = socket(data_addr.su_family, SOCK_STREAM, 0);
		if (data < 0) {
			warn("socket");
			return (1);
		}
		if (dobind) {
			struct addrinfo *bindres;
			int binderr = -1;

			for (bindres = bindres0;
			     bindres != NULL;
			     bindres = bindres->ai_next)
				if (bindres->ai_family == data_addr.su_family)
					break;
			if (bindres == NULL)
				bindres = bindres0;
			binderr = bind(data, bindres->ai_addr,
				       bindres->ai_addrlen);
			if (binderr == -1)
		     {
			warn("bind");
			goto bad;
		     }
		}
		if ((options & SO_DEBUG) &&
		    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			       sizeof(on)) < 0)
			warn("setsockopt (ignored)");
		switch (data_addr.su_family) {
		case AF_INET:
			if (try_epsv) {
				int overbose;

				overbose = verbose;
				if (debug == 0)
					verbose = -1;
				result = command(pasvcmd = "EPSV");
				verbose = overbose;
				if (code / 10 == 22 && code != 229) {
					puts("wrong server: EPSV return code must be 229");
					result = COMPLETE + 1;
				}
			} else
				result = COMPLETE + 1;
			if (result != COMPLETE) {
				try_epsv = 0;
				result = command(pasvcmd = "PASV");
			}
			break;
#ifdef INET6
		case AF_INET6:
			result = command(pasvcmd = "EPSV");
			if (code / 10 == 22 && code != 229) {
				puts("wrong server: EPSV return code must be 229");
				result = COMPLETE + 1;
			}
			if (result != COMPLETE)
				result = command(pasvcmd = "LPSV");
			break;
#endif
		default:
			result = COMPLETE + 1;
		}
		if (result != COMPLETE) {
			puts("Passive mode refused.");
			goto bad;
		}

#define pack2(var, offset) \
	(((var[(offset) + 0] & 0xff) << 8) | ((var[(offset) + 1] & 0xff) << 0))
#define pack4(var, offset) \
    (((var[(offset) + 0] & 0xff) << 24) | ((var[(offset) + 1] & 0xff) << 16) \
     | ((var[(offset) + 2] & 0xff) << 8) | ((var[(offset) + 3] & 0xff) << 0))
		/*
		 * What we've got at this point is a string of comma
		 * separated one-byte unsigned integer values.
		 * In PASV case,
		 * The first four are the an IP address. The fifth is
		 * the MSB of the port number, the sixth is the LSB.
		 * From that we'll prepare a sockaddr_in.
		 * In other case, the format is more complicated.
		 */
		if (strcmp(pasvcmd, "PASV") == 0) {
			if (code / 10 == 22 && code != 227) {
				puts("wrong server: return code must be 227");
				error = 1;
				goto bad;
			}
			error = sscanf(pasv, "%d,%d,%d,%d,%d,%d",
			       &h[0], &h[1], &h[2], &h[3],
			       &prt[0], &prt[1]);
			if (error == 6) {
				error = 0;
				data_addr.su_sin.sin_addr.s_addr =
					htonl(pack4(h, 0));
			} else
				error = 1;
		} else if (strcmp(pasvcmd, "LPSV") == 0) {
			if (code / 10 == 22 && code != 228) {
				puts("wrong server: return code must be 228");
				error = 1;
				goto bad;
			}
			switch (data_addr.su_family) {
			case AF_INET:
				error = sscanf(pasv,
"%d,%d,%d,%d,%d,%d,%d,%d,%d",
				       &af, &hal,
				       &h[0], &h[1], &h[2], &h[3],
				       &pal, &prt[0], &prt[1]);
				if (error == 9 && af == 4 && hal == 4 && pal == 2) {
					error = 0;
					data_addr.su_sin.sin_addr.s_addr =
						htonl(pack4(h, 0));
				} else
					error = 1;
				break;
#ifdef INET6
			case AF_INET6:
				error = sscanf(pasv,
"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
					       &af, &hal,
					       &h[0], &h[1], &h[2], &h[3],
					       &h[4], &h[5], &h[6], &h[7],
					       &h[8], &h[9], &h[10], &h[11],
					       &h[12], &h[13], &h[14], &h[15],
					       &pal, &prt[0], &prt[1]);
				if (error != 21 || af != 6 || hal != 16 || pal != 2) {
					error = 1;
					break;
				}

				error = 0;
			    {
				u_int32_t *p32;
				p32 = (u_int32_t *)&data_addr.su_sin6.sin6_addr;
				p32[0] = htonl(pack4(h, 0));
				p32[1] = htonl(pack4(h, 4));
				p32[2] = htonl(pack4(h, 8));
				p32[3] = htonl(pack4(h, 12));
			    }
				break;
#endif
			default:
				error = 1;
			}
		} else if (strcmp(pasvcmd, "EPSV") == 0) {
			char delim[4];

			prt[0] = 0;
			if (code / 10 == 22 && code != 229) {
				puts("wrong server: return code must be 229");
				error = 1;
				goto bad;
			}
			error = sscanf(pasv, "%c%c%c%d%c",
				&delim[0], &delim[1], &delim[2],
				&prt[1], &delim[3]);
			if (error != 5) {
				error = 1;
				goto epsv_done;
			}
			if (delim[0] != delim[1] || delim[0] != delim[2]
			 || delim[0] != delim[3]) {
				error = 1;
				goto epsv_done;
			}

			data_addr = hisctladdr;
			/* quickhack */
			prt[0] = (prt[1] & 0xff00) >> 8;
			prt[1] &= 0xff;
			error = 0;
epsv_done:
			;
		} else
			error = 1;

		if (error) {
			puts(
"Passive mode address scan failure. Shouldn't happen!");
			goto bad;
		};

		data_addr.su_port = htons(pack2(prt, 0));

		if (connect(data, (struct sockaddr *)&data_addr,
			    data_addr.su_len) < 0) {
			warn("connect");
			goto bad;
		}
#ifdef IP_TOS
		if (data_addr.su_family == AF_INET)
	      {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	      }
#endif
		return (0);
	}

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.su_port = 0;	/* let system pick one */
	if (data != -1)
		(void)close(data);
	data = socket(data_addr.su_family, SOCK_STREAM, 0);
	if (data < 0) {
		warn("socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
				sizeof(on)) < 0) {
			warn("setsockopt (reuse address)");
			goto bad;
		}
#ifdef IP_PORTRANGE
	if (data_addr.su_family == AF_INET)
      {
	
	ports = restricted_data_ports ? IP_PORTRANGE_HIGH : IP_PORTRANGE_DEFAULT;
	if (setsockopt(data, IPPROTO_IP, IP_PORTRANGE, (char *)&ports,
		       sizeof(ports)) < 0)
	    warn("setsockopt PORTRANGE (ignored)");
      }
#endif
#ifdef INET6
#ifdef IPV6_PORTRANGE
	if (data_addr.su_family == AF_INET6) {
		ports = restricted_data_ports ? IPV6_PORTRANGE_HIGH
			: IPV6_PORTRANGE_DEFAULT;
		if (setsockopt(data, IPPROTO_IPV6, IPV6_PORTRANGE,
			       (char *)&ports, sizeof(ports)) < 0)
		  warn("setsockopt PORTRANGE (ignored)");
	}
#endif
#endif
	if (bind(data, (struct sockaddr *)&data_addr, data_addr.su_len) < 0) {
		warn("bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on,
			sizeof(on)) < 0)
		warn("setsockopt (ignored)");
	len = sizeof(data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		warn("getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		warn("listen");
	if (sendport) {
		char hname[INET6_ADDRSTRLEN];
		int af;
		struct sockaddr_in data_addr4;
		union sockunion *daddr;

#ifdef INET6
		if (data_addr.su_family == AF_INET6 &&
		    IN6_IS_ADDR_V4MAPPED(&data_addr.su_sin6.sin6_addr)) {
			memset(&data_addr4, 0, sizeof(data_addr4));
			data_addr4.sin_len = sizeof(struct sockaddr_in);
			data_addr4.sin_family = AF_INET;
			data_addr4.sin_port = data_addr.su_port;
			memcpy((caddr_t)&data_addr4.sin_addr,
			       (caddr_t)&data_addr.su_sin6.sin6_addr.s6_addr[12],
			       sizeof(struct in_addr));
			daddr = (union sockunion *)&data_addr4;
		} else
#endif
		daddr = &data_addr;



#define	UC(b)	(((int)b)&0xff)

		switch (daddr->su_family) {
#ifdef INET6
		case AF_INET6:
			af = (daddr->su_family == AF_INET) ? 1 : 2;
			if (daddr->su_family == AF_INET6)
				daddr->su_sin6.sin6_scope_id = 0;
			if (getnameinfo((struct sockaddr *)daddr,
					daddr->su_len, hname,
					sizeof(hname) - 1, NULL, 0,
					NI_NUMERICHOST)) {
				result = ERROR;
			} else {
				result = command("EPRT |%d|%s|%d|",
					af, hname, ntohs(daddr->su_port));
			}
			break;
#endif
		default:
			result = COMPLETE + 1;
			break;
		}
		if (result == COMPLETE)
			goto skip_port;

		p = (char *)&daddr->su_port;
		switch (daddr->su_family) {
		case AF_INET:
			a = (char *)&daddr->su_sin.sin_addr;
			result = command("PORT %d,%d,%d,%d,%d,%d",
					 UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
					 UC(p[0]), UC(p[1]));
			break;
#ifdef INET6
		case AF_INET6:
			a = (char *)&daddr->su_sin6.sin6_addr;
			result = command(
"LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
					 6, 16,
					 UC(a[0]),UC(a[1]),UC(a[2]),UC(a[3]),
					 UC(a[4]),UC(a[5]),UC(a[6]),UC(a[7]),
					 UC(a[8]),UC(a[9]),UC(a[10]),UC(a[11]),
					 UC(a[12]),UC(a[13]),UC(a[14]),UC(a[15]),
					 2, UC(p[0]), UC(p[1]));
			break;
#endif
		default:
			result = COMPLETE + 1; /* xxx */
		}
	skip_port:
		
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#ifdef IP_TOS
	if (data_addr.su_family == AF_INET)
      {
	on = IPTOS_THROUGHPUT;
	if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
      }
#endif
	return (0);
bad:
	(void)close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
}

FILE *
dataconn(const char *lmode)
{
	union sockunion from;
	int s, fromlen, tos;

	fromlen = myctladdr.su_len;

	if (passivemode)
		return (fdopen(data, lmode));

	s = accept(data, (struct sockaddr *) &from, &fromlen);
	if (s < 0) {
		warn("accept");
		(void)close(data), data = -1;
		return (NULL);
	}
	(void)close(data);
	data = s;
#ifdef IP_TOS
	if (data_addr.su_family == AF_INET)
      {
	tos = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		warn("setsockopt TOS (ignored)");
      }
#endif
	return (fdopen(data, lmode));
}

void
psummary(int notused)
{

	if (bytes > 0)
		ptransfer(1);
}

void
psabort(int notused)
{

	alarmtimer(0);
	abrtflag++;
}

void
pswitch(int flag)
{
	sig_t oldintr;
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
		union sockunion mctl;
		union sockunion hctl;
		FILE *in;
		FILE *out;
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void)strncpy(ip->name, hostname, sizeof(ip->name) - 1);
		ip->name[sizeof(ip->name) - 1] = '\0';
	} else
		ip->name[0] = '\0';
	hostname = op->name;
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void)strncpy(ip->nti, ntin, sizeof(ip->nti) - 1);
	(ip->nti)[sizeof(ip->nti) - 1] = '\0';
	(void)strcpy(ntin, op->nti);
	(void)strncpy(ip->nto, ntout, sizeof(ip->nto) - 1);
	(ip->nto)[sizeof(ip->nto) - 1] = '\0';
	(void)strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void)strncpy(ip->mi, mapin, sizeof(ip->mi) - 1);
	(ip->mi)[sizeof(ip->mi) - 1] = '\0';
	(void)strcpy(mapin, op->mi);
	(void)strncpy(ip->mo, mapout, sizeof(ip->mo) - 1);
	(ip->mo)[sizeof(ip->mo) - 1] = '\0';
	(void)strcpy(mapout, op->mo);
	(void)signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

void
abortpt(int notused)
{

	alarmtimer(0);
	putchar('\n');
	(void)fflush(stdout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

void
proxtrans(const char *cmd, const char *local, const char *remote)
{
	sig_t oldintr;
	int prox_type, nfnd;
	volatile int secndflag;
	char *cmd2;
	fd_set mask;

#ifdef __GNUC__			/* XXX: to shut up gcc warnings */
	(void)&oldintr;
	(void)&cmd2;
#endif

	oldintr = NULL;
	secndflag = 0;
	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (try_epsv && command("EPSV") != COMPLETE)
		try_epsv = 0;
	if (!try_epsv && command("PASV") != COMPLETE) {
		puts("proxy server does not support third party transfers.");
		return;
	}
	pswitch(0);
	if (!connected) {
		puts("No primary connection.");
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void)signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void)getreply(0);
	pswitch(0);
	(void)getreply(0);
	(void)signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void)signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void)signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void)signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				warn("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer();
		}
		(void)getreply(0);
		(void)getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void)signal(SIGINT, oldintr);
}

void
reset(int argc, char **argv)
{
	fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd > 0) {
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 0)) < 0) {
			warn("reset");
			code = -1;
			lostpeer();
		}
		else if (nfnd) {
			(void)getreply(0);
		}
	}
}

char *
gunique(const char *local)
{
	static char new[MAXPATHLEN];
	char *cp = strrchr(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp == local ? "/" : cp ? local : ".", W_OK);
	if (cp)
		*cp = '/';
	if (d < 0) {
		warn("local: %s", local);
		return ((char *) 0);
	}
	(void)strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			puts("runique: can't find unique file name.");
			return ((char *) 0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, F_OK)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return (new);
}

void
abort_remote(FILE *din)
{
	char buf[BUFSIZ];
	int nfnd;
	fd_set mask;

	if (cout == NULL) {
		warnx("Lost control connection for abort.");
		if (ptabflg)
			code = -1;
		lostpeer();
		return;
	}
	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	sprintf(buf, "%c%c%c", IAC, IP, IAC);
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		warn("abort");
	fprintf(cout, "%cABOR\r\n", DM);
	(void)fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din) {
		FD_SET(fileno(din), &mask);
	}
	if ((nfnd = empty(&mask, 10)) <= 0) {
		if (nfnd < 0) {
			warn("abort");
		}
		if (ptabflg)
			code = -1;
		lostpeer();
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			/* LOOP */;
	}
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void)getreply(0);
	}
	(void)getreply(0);
}

void
ai_unmapped(struct addrinfo *ai)
{
	struct sockaddr_in6 *sin6;
	struct sockaddr_in sin;

	if (ai->ai_family != AF_INET6)
		return;
	if (ai->ai_addrlen != sizeof(struct sockaddr_in6) ||
	    sizeof(sin) > ai->ai_addrlen)
		return;
	sin6 = (struct sockaddr_in6 *)ai->ai_addr;
	if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(struct sockaddr_in);
	memcpy(&sin.sin_addr, &sin6->sin6_addr.s6_addr[12],
	    sizeof(sin.sin_addr));
	sin.sin_port = sin6->sin6_port;

	ai->ai_family = AF_INET;
	memcpy(ai->ai_addr, &sin, sin.sin_len);
	ai->ai_addrlen = sin.sin_len;
}
