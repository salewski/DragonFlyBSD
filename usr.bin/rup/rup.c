/*-
 * Copyright (c) 1993, John Brezak
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
 * $FreeBSD: src/usr.bin/rup/rup.c,v 1.11.2.2 2001/07/02 23:43:04 mikeh Exp $
 * $DragonFly: src/usr.bin/rup/rup.c,v 1.4 2005/02/23 17:44:18 liamfoy Exp $
 */
#include <sys/param.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#undef FSHIFT			/* Use protocol's shift and scale values */
#undef FSCALE
#include <rpcsvc/rstat.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define HOST_WIDTH 15

struct host_list {
	struct host_list *next;
	struct in_addr addr;
} *hosts;

static int
search_host(struct in_addr addr)
{
	struct host_list *hp;

	if (hosts == NULL)
		return(0);

	for (hp = hosts; hp != NULL; hp = hp->next) {
		if (hp->addr.s_addr == addr.s_addr)
			return(1);
	}
	return(0);
}

static void
remember_host(struct in_addr addr)
{
	struct host_list *hp;

	if ((hp = malloc(sizeof(struct host_list))) == NULL)
		err(1, "malloc failed");
	hp->addr.s_addr = addr.s_addr;
	hp->next = hosts;
	hosts = hp;
}

static int
rstat_reply(char *replyp, struct sockaddr_in *raddrp)
{
	struct tm *tmp_time;
	struct tm host_time;
	struct tm host_uptime;
	char days_buf[16];
	char hours_buf[16];
	struct hostent *hp;
	char *host;
	statstime *host_stat = (statstime *)replyp;

	if (search_host(raddrp->sin_addr))
		return(0);

	hp = gethostbyaddr((char *)&raddrp->sin_addr.s_addr,
			   sizeof(struct in_addr), AF_INET);
	if (hp)
		host = hp->h_name;
	else
		host = inet_ntoa(raddrp->sin_addr);

	/* truncate hostname to fit nicely into field */
	if (strlen(host) > HOST_WIDTH)
		host[HOST_WIDTH] = '\0';

	printf("%-*s\t", HOST_WIDTH, host);

	tmp_time = localtime((time_t *)&host_stat->curtime.tv_sec);
	host_time = *tmp_time;

	host_stat->curtime.tv_sec -= host_stat->boottime.tv_sec;

	tmp_time = gmtime((time_t *)&host_stat->curtime.tv_sec);
	host_uptime = *tmp_time;

	#define updays (host_stat->curtime.tv_sec  / 86400)
	if (host_uptime.tm_yday != 0)
		snprintf(days_buf, sizeof days_buf, "%3d day%s, ", updays,
			(updays > 1) ? "s" : "");
	else
		days_buf[0] = '\0';

	if (host_uptime.tm_hour != 0)
		snprintf(hours_buf, sizeof hours_buf, "%2d:%02d, ",
			host_uptime.tm_hour, host_uptime.tm_min);
	else if (host_uptime.tm_min != 0)
		snprintf(hours_buf, sizeof hours_buf, "%2d mins, ",
			 host_uptime.tm_min);
	else if (host_stat->curtime.tv_sec < 60)
		snprintf(hours_buf, sizeof hours_buf, "%2d secs, ",
			 host_uptime.tm_sec);
	else
		hours_buf[0] = '\0';

	printf(" %2d:%02d%cm  up %9.9s%9.9s load average: %.2f %.2f %.2f\n",
		(host_time.tm_hour % 12) ? host_time.tm_hour % 12 : 12,
		host_time.tm_min,
		(host_time.tm_hour >= 12) ? 'p' : 'a',
		days_buf,
		hours_buf,
		(double)host_stat->avenrun[0]/FSCALE,
		(double)host_stat->avenrun[1]/FSCALE,
		(double)host_stat->avenrun[2]/FSCALE);

	remember_host(raddrp->sin_addr);
	return(0);
}

static int
onehost(const char *host)
{
	CLIENT *rstat_clnt;
	statstime host_stat;
	struct sockaddr_in addr;
	struct hostent *hp;
	struct timeval tv;

	hp = gethostbyname(host);
	if (hp == NULL) {
		herror("rup");
		return(-1);
	}

	rstat_clnt = clnt_create(host, RSTATPROG, RSTATVERS_TIME, "udp");
	if (rstat_clnt == NULL) {
		clnt_pcreateerror(host);
		return(-1);
	}

	bzero(&host_stat, sizeof(host_stat));
	tv.tv_sec = 15;	/* XXX ??? */
	tv.tv_usec = 0;
	if (clnt_call(rstat_clnt, RSTATPROC_STATS, xdr_void, NULL, (xdrproc_t)xdr_statstime, &host_stat, tv) != RPC_SUCCESS) {
		clnt_perror(rstat_clnt, host);
		clnt_destroy(rstat_clnt);
		return(-1);
	}

	addr.sin_addr.s_addr = *(int *)hp->h_addr;
	rstat_reply((char *)&host_stat, &addr);
	clnt_destroy(rstat_clnt);
	return (0);
}

static void
allhosts(void)
{
	statstime host_stat;
	enum clnt_stat clnt_stat;

	clnt_stat = clnt_broadcast(RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
				   xdr_void, NULL,
				   (xdrproc_t)xdr_statstime, (char *)&host_stat, rstat_reply);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT)
		errx(1, "%s", clnt_sperrno(clnt_stat));
}

static void
usage(void)
{
	fprintf(stderr, "usage: rup [hosts ...]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		default:
			usage();
			/*NOTREACHED*/
		}

	setlinebuf(stdout);
	if (argc == optind)
		allhosts();
	else {
		for (; optind < argc; optind++)
			(void) onehost(argv[optind]);
	}
	exit(0);
}
