/*-
 * Copyright (c) 1980, 1992, 1993
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
 * @(#)netcmds.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/systat/netcmds.c,v 1.9 1999/08/28 01:06:04 peter Exp $
 * $DragonFly: src/usr.bin/systat/netcmds.c,v 1.5 2004/09/03 20:38:01 dillon Exp $
 */

/*
 * Common network command support routines.
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "systat.h"
#include "extern.h"

#define	streq(a,b)	(strcmp(a,b)==0)

static	struct hitem {
	struct	in_addr addr;
	int	onoff;
} *hosts;

int nports, nhosts, protos;

static void changeitems(char *, int);
static int selectproto(char *);
static void showprotos(void);
static int selectport(long, int);
static void showports(void);
static int selecthost(struct in_addr *, int);
static void showhosts(void);

int
netcmd(char *cmd, char *args)
{

	if (prefix(cmd, "proto")) {
		if (*args == '\0') {
			move(CMDLINE, 0);
			clrtoeol();
			addstr("which proto?");
		} else if (!selectproto(args)) {
			error("%s: Unknown protocol.", args);
		}
		return (1);
	}
	if (prefix(cmd, "ignore") || prefix(cmd, "display")) {
		changeitems(args, prefix(cmd, "display"));
		return (1);
	}
	if (prefix(cmd, "reset")) {
		selectproto(0);
		selecthost(0, 0);
		selectport(-1, 0);
		return (1);
	}
	if (prefix(cmd, "show")) {
		move(CMDLINE, 0); clrtoeol();
		if (*args == '\0') {
			showprotos();
			showhosts();
			showports();
			return (1);
		}
		if (prefix(args, "protos"))
			showprotos();
		else if (prefix(args, "hosts"))
			showhosts();
		else if (prefix(args, "ports"))
			showports();
		else
			addstr("show what?");
		return (1);
	}
	return (0);
}


static void
changeitems(char *args, int onoff)
{
	register char *cp;
	struct servent *sp;
	struct hostent *hp;
	struct in_addr in;

	cp = strchr(args, '\n');
	if (cp)
		*cp = '\0';
	for (;;args = cp) {
		for (cp = args; *cp && isspace(*cp); cp++)
			;
		args = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - args == 0)
			break;
		sp = getservbyname(args,
		    protos == TCP ? "tcp" : protos == UDP ? "udp" : 0);
		if (sp) {
			selectport(sp->s_port, onoff);
			continue;
		}
		hp = gethostbyname(args);
		if (hp == 0) {
			in.s_addr = inet_addr(args);
			if (in.s_addr == -1) {
				error("%s: unknown host or port", args);
				continue;
			}
		} else
			in = *(struct in_addr *)hp->h_addr;
		selecthost(&in, onoff);
	}
}

static int
selectproto(char *proto)
{

	if (proto == 0 || streq(proto, "all"))
		protos = TCP | UDP;
	else if (streq(proto, "tcp"))
		protos = TCP;
	else if (streq(proto, "udp"))
		protos = UDP;
	else
		return (0);

	return (protos);
}

static void
showprotos(void)
{

	if ((protos&TCP) == 0)
		addch('!');
	addstr("tcp ");
	if ((protos&UDP) == 0)
		addch('!');
	addstr("udp ");
}

static	struct pitem {
	long	port;
	int	onoff;
} *ports;

static int
selectport(long port, int onoff)
{
	register struct pitem *p;

	if (port == -1) {
		if (ports == 0)
			return (0);
		free((char *)ports), ports = 0;
		nports = 0;
		return (1);
	}
	for (p = ports; p < ports+nports; p++)
		if (p->port == port) {
			p->onoff = onoff;
			return (0);
		}
	if (nports == 0)
		ports = (struct pitem *)malloc(sizeof (*p));
	else
		ports = (struct pitem *)realloc(ports, (nports+1)*sizeof (*p));
	p = &ports[nports++];
	p->port = port;
	p->onoff = onoff;
	return (1);
}

int
checkport(register struct inpcb *inp)
{
	register struct pitem *p;

	if (ports)
	for (p = ports; p < ports+nports; p++)
		if (p->port == inp->inp_lport || p->port == inp->inp_fport)
			return (p->onoff);
	return (1);
}

static void
showports(void)
{
	register struct pitem *p;
	struct servent *sp;

	for (p = ports; p < ports+nports; p++) {
		sp = getservbyport(p->port,
		    protos == TCP|UDP ? 0 : protos == TCP ? "tcp" : "udp");
		if (!p->onoff)
			addch('!');
		if (sp)
			printw("%s ", sp->s_name);
		else
			printw("%d ", p->port);
	}
}

static int
selecthost(struct in_addr *in, int onoff)
{
	register struct hitem *p;

	if (in == 0) {
		if (hosts == 0)
			return (0);
		free((char *)hosts), hosts = 0;
		nhosts = 0;
		return (1);
	}
	for (p = hosts; p < hosts+nhosts; p++)
		if (p->addr.s_addr == in->s_addr) {
			p->onoff = onoff;
			return (0);
		}
	if (nhosts == 0)
		hosts = (struct hitem *)malloc(sizeof (*p));
	else
		hosts = (struct hitem *)realloc(hosts, (nhosts+1)*sizeof (*p));
	p = &hosts[nhosts++];
	p->addr = *in;
	p->onoff = onoff;
	return (1);
}

int
checkhost(register struct inpcb *inp)
{
	register struct hitem *p;

	if (hosts)
	for (p = hosts; p < hosts+nhosts; p++)
		if (p->addr.s_addr == inp->inp_laddr.s_addr ||
		    p->addr.s_addr == inp->inp_faddr.s_addr)
			return (p->onoff);
	return (1);
}

static void
showhosts(void)
{
	register struct hitem *p;
	struct hostent *hp;

	for (p = hosts; p < hosts+nhosts; p++) {
		hp = gethostbyaddr((char *)&p->addr, sizeof (p->addr), AF_INET);
		if (!p->onoff)
			addch('!');
		printw("%s ", hp ? hp->h_name : (char *)inet_ntoa(p->addr));
	}
}
