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
 * @(#)mbufs.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/systat/ip.c,v 1.3.2.1 2001/04/25 12:42:18 ru Exp $
 * $DragonFly: src/usr.bin/systat/ip.c,v 1.5 2004/05/03 15:18:25 hmp Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"
#include "mode.h"

struct stat {
	struct ip_stats i;
	struct udpstat u;
};

static struct stat curstat, initstat, oldstat;

/*-
--0         1         2         3         4         5         6         7
--0123456789012345678901234567890123456789012345678901234567890123456789012345
01          IP Input                           IP Output
02999999999 total packets received   999999999 total packets sent
03999999999 - with bad checksums     999999999 - generated locally
04999999999 - too short for header   999999999 - output drops
05999999999 - too short for data     999999999 output fragments generated
06999999999 - with invalid hlen      999999999 - fragmentation failed
07999999999 - with invalid length    999999999 destinations unreachable
08999999999 - with invalid version   999999999 packets output via raw IP
09999999999 - jumbograms
10999999999 total fragments received           UDP Statistics
11999999999 - fragments dropped      999999999 total input packets
12999999999 - fragments timed out    999999999 - too short for header
13999999999 - packets reassembled ok 999999999 - invalid checksum
14999999999 packets forwarded        999999999 - no checksum
15999999999 - unreachable dests      999999999 - invalid length
16999999999 - redirects generated    999999999 - no socket for dest port
17999999999 option errors            999999999 - no socket for broadcast
18999999999 unwanted multicasts      999999999 - socket buffer full
19999999999 delivered to upper layer 999999999 total output packets
--0123456789012345678901234567890123456789012345678901234567890123456789012345
--0         1         2         3         4         5         6         7
*/

WINDOW *
openip(void)
{
	return (subwin(stdscr, LINES-4-1, 0, 4, 0));
}

void
closeip(WINDOW *w)
{
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
	delwin(w);
}

void
labelip(void)
{
	wmove(wnd, 0, 0); wclrtoeol(wnd);
#define L(row, str) mvwprintw(wnd, row, 10, str)
#define R(row, str) mvwprintw(wnd, row, 45, str);
	L(1, "IP Input");			R(1, "IP Output");
	L(2, "total packets received");		R(2, "total packets sent");
	L(3, "- with bad checksums");		R(3, "- generated locally");
	L(4, "- too short for header");		R(4, "- output drops");
	L(5, "- too short for data");	R(5, "output fragments generated");
	L(6, "- with invalid hlen");	R(6, "- fragmentation failed");
	L(7, "- with invalid length");	R(7, "destinations unreachable");
	L(8, "- with invalid version");	R(8, "packets output via raw IP");
	L(9, "- jumbograms");
	L(10, "total fragments received");	R(10, "UDP Statistics");
	L(11, "- fragments dropped");	R(11, "total input packets");
	L(12, "- fragments timed out");	R(12, "- too short for header");
	L(13, "- packets reassembled ok");	R(13, "- invalid checksum");
	L(14, "packets forwarded");	R(14, "- no checksum");
	L(15, "- unreachable dests");	R(15, "- invalid length");
	L(16, "- redirects generated");	R(16, "- no socket for dest port");
	L(17, "option errors");		R(17, "- no socket for broadcast");
	L(18, "unwanted multicasts");	R(18, "- socket buffer full");
	L(19, "delivered to upper layer");	R(19, "total output packets");
#undef L
#undef R
}

static void
domode(struct stat *ret)
{
	const struct stat *sub;
	double divisor = 1.0;

	switch(currentmode) {
	case display_RATE:
		sub = &oldstat;
		divisor = naptime;
		break;
	case display_DELTA:
		sub = &oldstat;
		break;
	case display_SINCE:
		sub = &initstat;
		break;
	default:
		*ret = curstat;
		return;
	}
#define DO(stat) ret->stat = (double)(curstat.stat - sub->stat) / divisor
	DO(i.ips_total);
	DO(i.ips_badsum);
	DO(i.ips_tooshort);
	DO(i.ips_toosmall);
	DO(i.ips_badhlen);
	DO(i.ips_badlen);
	DO(i.ips_fragments);
	DO(i.ips_fragdropped);
	DO(i.ips_fragtimeout);
	DO(i.ips_forward);
	DO(i.ips_cantforward);
	DO(i.ips_redirectsent);
	DO(i.ips_noproto);
	DO(i.ips_delivered);
	DO(i.ips_localout);
	DO(i.ips_odropped);
	DO(i.ips_reassembled);
	DO(i.ips_fragmented);
	DO(i.ips_ofragments);
	DO(i.ips_cantfrag);
	DO(i.ips_badoptions);
	DO(i.ips_noroute);
	DO(i.ips_badvers);
	DO(i.ips_rawout);
	DO(i.ips_toolong);
	DO(i.ips_notmember);
	DO(u.udps_ipackets);
	DO(u.udps_hdrops);
	DO(u.udps_badsum);
	DO(u.udps_nosum);
	DO(u.udps_badlen);
	DO(u.udps_noport);
	DO(u.udps_noportbcast);
	DO(u.udps_fullsock);
	DO(u.udps_opackets);
#undef DO
}
	
void
showip(void)
{
	struct stat stats;
	u_long totalout;

	domode(&stats);
	totalout = stats.i.ips_forward + stats.i.ips_localout;

#define DO(stat, row, col) \
	mvwprintw(wnd, row, col, "%9lu", stats.stat)

	DO(i.ips_total, 2, 0);
	mvwprintw(wnd, 2, 35, "%9lu", totalout);
	DO(i.ips_badsum, 3, 0);
	DO(i.ips_localout, 3, 35);
	DO(i.ips_tooshort, 4, 0);
	DO(i.ips_odropped, 4, 35);
	DO(i.ips_toosmall, 5, 0);
	DO(i.ips_ofragments, 5, 35);
	DO(i.ips_badhlen, 6, 0);
	DO(i.ips_cantfrag, 6, 35);
	DO(i.ips_badlen, 7, 0);
	DO(i.ips_noroute, 7, 35);
	DO(i.ips_badvers, 8, 0);
	DO(i.ips_rawout, 8, 35);
	DO(i.ips_toolong, 9, 0);
	DO(i.ips_fragments, 10, 0);
	DO(i.ips_fragdropped, 11, 0);
	DO(u.udps_ipackets, 11, 35);
	DO(i.ips_fragtimeout, 12, 0);
	DO(u.udps_hdrops, 12, 35);
	DO(i.ips_reassembled, 13, 0);
	DO(u.udps_badsum, 13, 35);
	DO(i.ips_forward, 14, 0);
	DO(u.udps_nosum, 14, 35);
	DO(i.ips_cantforward, 15, 0);
	DO(u.udps_badlen, 15, 35);
	DO(i.ips_redirectsent, 16, 0);
	DO(u.udps_noport, 16, 35);
	DO(i.ips_badoptions, 17, 0);
	DO(u.udps_noportbcast, 17, 35);
	DO(i.ips_notmember, 18, 0);
	DO(u.udps_fullsock, 18, 35);
	DO(i.ips_delivered, 19, 0);
	DO(u.udps_opackets, 19, 35);
#undef DO
}

int
initip(void)
{
	size_t len;
	int name[4];

	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_IP;
	name[3] = IPCTL_STATS;

	len = 0;
	if (sysctl(name, 4, 0, &len, 0, 0) < 0) {
		error("sysctl getting ip_stats size failed");
		return 0;
	}
	if (len > sizeof curstat.i) {
		error("ip_stats structure has grown--recompile systat!");
		return 0;
	}
	if (sysctl(name, 4, &initstat.i, &len, 0, 0) < 0) {
		error("sysctl getting ip_stats failed");
		return 0;
	}
	name[2] = IPPROTO_UDP;
	name[3] = UDPCTL_STATS;

	len = 0;
	if (sysctl(name, 4, 0, &len, 0, 0) < 0) {
		error("sysctl getting udpstat size failed");
		return 0;
	}
	if (len > sizeof curstat.u) {
		error("ip_stats structure has grown--recompile systat!");
		return 0;
	}
	if (sysctl(name, 4, &initstat.u, &len, 0, 0) < 0) {
		error("sysctl getting udpstat failed");
		return 0;
	}
	oldstat = initstat;
	return 1;
}

void
resetip(void)
{
	size_t len;
	int name[4];

	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_IP;
	name[3] = IPCTL_STATS;

	len = sizeof initstat.i;
	if (sysctl(name, 4, &initstat.i, &len, 0, 0) < 0) {
		error("sysctl getting ip_stat failed");
	}
	name[2] = IPPROTO_UDP;
	name[3] = UDPCTL_STATS;

	len = sizeof initstat.u;
	if (sysctl(name, 4, &initstat.u, &len, 0, 0) < 0) {
		error("sysctl getting udpstat failed");
	}
	oldstat = initstat;
}

void
fetchip(void)
{
	int name[4];
	size_t len;

	oldstat = curstat;
	name[0] = CTL_NET;
	name[1] = PF_INET;
	name[2] = IPPROTO_IP;
	name[3] = IPCTL_STATS;
	len = sizeof curstat.i;

	if (sysctl(name, 4, &curstat.i, &len, 0, 0) < 0)
		return;
	name[2] = IPPROTO_UDP;
	name[3] = UDPCTL_STATS;
	len = sizeof curstat.u;

	if (sysctl(name, 4, &curstat.u, &len, 0, 0) < 0)
		return;
}

