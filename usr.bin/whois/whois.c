/*
 * Copyright (c) 1980, 1993
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
 * @(#) Copyright (c) 1980, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)whois.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/whois/whois.c,v 1.15.2.11 2003/02/25 20:59:41 roberto Exp $
 * $DragonFly: src/usr.bin/whois/whois.c,v 1.7 2005/02/17 11:53:33 liamfoy Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define ABUSEHOST	"whois.abuse.net"
#define	NICHOST		"whois.crsnic.net"
#define	INICHOST	"whois.networksolutions.com"
#define	DNICHOST	"whois.nic.mil"
#define	GNICHOST	"whois.nic.gov"
#define	ANICHOST	"whois.arin.net"
#define	LNICHOST	"whois.lacnic.net"
#define	KNICHOST	"whois.krnic.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	RUNICHOST	"whois.ripn.net"
#define	MNICHOST	"whois.ra.net"
#define	QNICHOST_TAIL	".whois-servers.net"
#define	SNICHOST	"whois.6bone.net"
#define IANAHOST	"whois.iana.org"
#define GERMNICHOST	"de.whois-servers.net"
#define	BNICHOST	"whois.registro.br"
#define	WHOIS_SERVER_ID	"Whois Server: "
#define	WHOIS_ORG_SERVER_ID	"Registrant Street1:Whois Server:"

#define DEFAULT_PORT	"whois"
#define WHOIS_RECURSE		0x01
#define WHOIS_QUICK		0x02

#define ishost(h) (isalnum(h) || h == '.' || h == '-')

const char *ip_whois[] = { LNICHOST, RNICHOST, PNICHOST, BNICHOST, NULL };
const char *port = DEFAULT_PORT;

static char	*choose_server(const char *);
static struct addrinfo *gethostinfo(const char *host, int exit_on_error);
static void	s_asprintf(char **ret, const char *format, ...);
static void	usage(void);
static void	whois(const char *, const char *, int);

int
main(int argc, char *argv[])
{
	const char *country, *host;
	char *qnichost;
	int ch, flags, use_qnichost;

#ifdef	SOCKS
	SOCKSinit(argv[0]);
#endif

	country = host = qnichost = NULL;
	flags = use_qnichost = 0;
	while ((ch = getopt(argc, argv, "aAbc:dgh:ilkImp:QrR6")) != -1) {
		switch (ch) {
		case 'a':
			host = ANICHOST;
			break;
		case 'A':
			host = PNICHOST;
			break;
		case 'b':
			host = ABUSEHOST;
			break;
		case 'c':
			country = optarg;
			break;
		case 'd':
			host = DNICHOST;
			break;
		case 'g':
			host = GNICHOST;
			break;
		case 'h':
			host = optarg;
			break;
		case 'i':
			host = INICHOST;
			break;
		case 'I':
			host = IANAHOST;
			break;
		case 'k':
			host = KNICHOST;
			break;
		case 'l':
			host = LNICHOST;
			break;
		case 'm':
			host = MNICHOST;
			break;
		case 'p':
			port = optarg;
			break;
		case 'Q':
			flags |= WHOIS_QUICK;
			break;
		case 'r':
			host = RNICHOST;
			break;
		case 'R':
			host = RUNICHOST;
			break;
		case '6':
			host = SNICHOST;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc || (country != NULL && host != NULL))
		usage();

	/*
	 * If no host or country is specified determine the top level domain
	 * from the query.  If the TLD is a number, query ARIN.  Otherwise, use
	 * TLD.whois-server.net.  If the domain does not contain '.', fall
	 * back to NICHOST.
	 */
	if (host == NULL && country == NULL) {
		use_qnichost = 1;
		host = NICHOST;
		if (!(flags & WHOIS_QUICK))
			flags |= WHOIS_RECURSE;
	}
	while (argc-- > 0) {
		if (country != NULL) {
			s_asprintf(&qnichost, "%s%s", country, QNICHOST_TAIL);
			whois(*argv, qnichost, flags);
		} else if (use_qnichost)
			if ((qnichost = choose_server(*argv)) != NULL)
				whois(*argv, qnichost, flags);
		if (qnichost == NULL)
			whois(*argv, host, flags);
		free(qnichost);
		qnichost = NULL;
		argv++;
	}
	exit(0);
}

/*
 * This function will remove any trailing periods from domain, after which it
 * returns a pointer to newly allocated memory containing the whois server to
 * be queried, or a NULL if the correct server couldn't be determined.  The
 * caller must remember to free(3) the allocated memory.
 */
static char *
choose_server(const char *domain)
{
	char *pos, *retval;

	for (pos = strchr(domain, '\0'); pos > domain && *--pos == '.';)
		*pos = '\0';
	if (*domain == '\0')
		errx(EX_USAGE, "can't search for a null string");
	while (pos > domain && *pos != '.')
		--pos;
	if (pos <= domain)
		return (NULL);
	if (isdigit(*++pos))
		s_asprintf(&retval, "%s", ANICHOST);
	else
		s_asprintf(&retval, "%s%s", pos, QNICHOST_TAIL);
	return (retval);
}

static struct addrinfo *
gethostinfo(const char *host, int exit_on_error)
{
	struct addrinfo hints, *res;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res);
	if (error) {
		if (error == EAI_SERVICE)
			warnx("bad port: %s", port);
		else
			warnx("%s: %s", host, gai_strerror(error));
		if (exit_on_error)
			exit(EX_NOHOST);
		return (NULL);
	}
	return (res);
}

/*
 * Wrapper for asprintf(3) that exits on error.
 */
static void
s_asprintf(char **ret, const char *format, ...)
{

	va_list ap;

	va_start(ap, format);
	if (vasprintf(ret, format, ap) == -1) {
		va_end(ap);
		err(EX_OSERR, "vasprintf()");
	}
	va_end(ap);
}

static void
whois(const char *query, const char *hostname, int flags)
{
	FILE *sfi, *sfo;
	struct addrinfo *hostres, *res;
	char *buf, *host, *nhost, *p;
	int i, s = -1;
	size_t c, len;

	hostres = gethostinfo(hostname, 1);
	for (res = hostres; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0)
			continue;
		if (connect(s, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close(s);
	}
	freeaddrinfo(hostres);
	if (res == NULL)
		err(EX_OSERR, "connect()");

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(EX_OSERR, "fdopen()");
	if (strcmp(hostname, GERMNICHOST) == 0) {
		fprintf(sfo, "-T dn,ace -C US-ASCII %s\r\n", query);
	} else {
		fprintf(sfo, "%s\r\n", query);
	}
	fflush(sfo);
	nhost = NULL;
	while ((buf = fgetln(sfi, &len)) != NULL) {
		while (len > 0 && isspace(buf[len - 1]))
			buf[--len] = '\0';
		printf("%.*s\n", (int)len, buf);

		if ((flags & WHOIS_RECURSE) && nhost == NULL) {
			host = strnstr(buf, WHOIS_SERVER_ID, len);
			if (host != NULL) {
				host += sizeof(WHOIS_SERVER_ID) - 1;
				for (p = host; p < buf + len; p++) {
					if (!ishost(*p)) {
						*p = '\0';
						break;
					}
				}
				s_asprintf(&nhost, "%.*s",
				     (int)(buf + len - host), host);
			} else if ((host =
			    strnstr(buf, WHOIS_ORG_SERVER_ID, len)) != NULL) {
				host += sizeof(WHOIS_ORG_SERVER_ID) - 1;
				for (p = host; p < buf + len; p++) {
					if (!ishost(*p)) {
						*p = '\0';
						break;
					}
				}
				s_asprintf(&nhost, "%.*s",
				    (int)(buf + len - host), host);
			} else if (strcmp(hostname, ANICHOST) == 0) {
				for (c = 0; c <= len; c++)
					buf[c] = tolower((int)buf[c]);
				for (i = 0; ip_whois[i] != NULL; i++) {
					if (strnstr(buf, ip_whois[i], len) !=
					    NULL) {
						s_asprintf(&nhost, "%s",
						    ip_whois[i]);
						break;
					}
				}
			}
		}
	}
	if (nhost != NULL) {
		whois(query, nhost, 0);
		free(nhost);
	}
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: whois [-aAbdgilkImQrR6] [-c country-code | -h hostname] "
	    "[-p port] name ...\n");
	exit(EX_USAGE);
}
