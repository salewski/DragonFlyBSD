/*
 * Copyright (c) 2003, 2004 Jeffrey M. Hsu.  All rights reserved.
 * Copyright (c) 2003, 2004 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Jeffrey M. Hsu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2003, 2004 Jeffrey M. Hsu.  All rights reserved.
 *
 * License terms: all terms for the DragonFly license above plus the following:
 *
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Jeffrey M. Hsu
 *	for the DragonFly Project.
 *
 *    This requirement may be waived with permission from Jeffrey Hsu.
 *    This requirement will sunset and may be removed on July 8 2005,
 *    after which the standard DragonFly license (as shown above) will
 *    apply.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 *	@(#)tcp_subr.c	8.2 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_subr.c,v 1.73.2.31 2003/01/24 05:11:34 sam Exp $
 * $DragonFly: src/sys/netinet/tcp_subr.c,v 1.45 2005/03/06 05:09:25 hsu Exp $
 */

#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/mpipe.h>
#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/random.h>
#include <sys/in_cksum.h>

#include <vm/vm_zone.h>

#include <net/route.h>
#include <net/if.h>
#include <net/netisr.h>

#define	_IP_VHL
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet6/ip6_var.h>
#include <netinet/ip_icmp.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif
#include <netinet6/ip6protosw.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#endif

#ifdef FAST_IPSEC
#include <netproto/ipsec/ipsec.h>
#ifdef INET6
#include <netproto/ipsec/ipsec6.h>
#endif
#define	IPSEC
#endif

#include <sys/md5.h>

#include <sys/msgport2.h>

#include <machine/smp.h>

struct inpcbinfo tcbinfo[MAXCPU];
struct tcpcbackqhead tcpcbackq[MAXCPU];

int tcp_mssdflt = TCP_MSS;
SYSCTL_INT(_net_inet_tcp, TCPCTL_MSSDFLT, mssdflt, CTLFLAG_RW,
    &tcp_mssdflt, 0, "Default TCP Maximum Segment Size");

#ifdef INET6
int tcp_v6mssdflt = TCP6_MSS;
SYSCTL_INT(_net_inet_tcp, TCPCTL_V6MSSDFLT, v6mssdflt, CTLFLAG_RW,
    &tcp_v6mssdflt, 0, "Default TCP Maximum Segment Size for IPv6");
#endif

#if 0
static int tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
SYSCTL_INT(_net_inet_tcp, TCPCTL_RTTDFLT, rttdflt, CTLFLAG_RW,
    &tcp_rttdflt, 0, "Default maximum TCP Round Trip Time");
#endif

int tcp_do_rfc1323 = 1;
SYSCTL_INT(_net_inet_tcp, TCPCTL_DO_RFC1323, rfc1323, CTLFLAG_RW,
    &tcp_do_rfc1323, 0, "Enable rfc1323 (high performance TCP) extensions");

int tcp_do_rfc1644 = 0;
SYSCTL_INT(_net_inet_tcp, TCPCTL_DO_RFC1644, rfc1644, CTLFLAG_RW,
    &tcp_do_rfc1644, 0, "Enable rfc1644 (TTCP) extensions");

static int tcp_tcbhashsize = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, tcbhashsize, CTLFLAG_RD,
     &tcp_tcbhashsize, 0, "Size of TCP control block hashtable");

static int do_tcpdrain = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, do_tcpdrain, CTLFLAG_RW, &do_tcpdrain, 0,
     "Enable tcp_drain routine for extra help when low on mbufs");

/* XXX JH */
SYSCTL_INT(_net_inet_tcp, OID_AUTO, pcbcount, CTLFLAG_RD,
    &tcbinfo[0].ipi_count, 0, "Number of active PCBs");

static int icmp_may_rst = 1;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, icmp_may_rst, CTLFLAG_RW, &icmp_may_rst, 0,
    "Certain ICMP unreachable messages may abort connections in SYN_SENT");

static int tcp_isn_reseed_interval = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, isn_reseed_interval, CTLFLAG_RW,
    &tcp_isn_reseed_interval, 0, "Seconds between reseeding of ISN secret");

/*
 * TCP bandwidth limiting sysctls.  Note that the default lower bound of
 * 1024 exists only for debugging.  A good production default would be
 * something like 6100.
 */
static int tcp_inflight_enable = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, inflight_enable, CTLFLAG_RW,
    &tcp_inflight_enable, 0, "Enable automatic TCP inflight data limiting");

static int tcp_inflight_debug = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, inflight_debug, CTLFLAG_RW,
    &tcp_inflight_debug, 0, "Debug TCP inflight calculations");

static int tcp_inflight_min = 6144;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, inflight_min, CTLFLAG_RW,
    &tcp_inflight_min, 0, "Lower bound for TCP inflight window");

static int tcp_inflight_max = TCP_MAXWIN << TCP_MAX_WINSHIFT;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, inflight_max, CTLFLAG_RW,
    &tcp_inflight_max, 0, "Upper bound for TCP inflight window");

static int tcp_inflight_stab = 20;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, inflight_stab, CTLFLAG_RW,
    &tcp_inflight_stab, 0, "Slop in maximal packets / 10 (20 = 2 packets)");

static MALLOC_DEFINE(M_TCPTEMP, "tcptemp", "TCP Templates for Keepalives");
static struct malloc_pipe tcptemp_mpipe;

static void tcp_willblock(void);
static void tcp_cleartaocache (void);
static void tcp_notify (struct inpcb *, int);

struct tcp_stats tcpstats_ary[MAXCPU];
#ifdef SMP
static int
sysctl_tcpstats(SYSCTL_HANDLER_ARGS)
{
	int cpu, error = 0;

	for (cpu = 0; cpu < ncpus; ++cpu) {
		if ((error = SYSCTL_OUT(req, &tcpstats_ary[cpu],
					sizeof(struct tcp_stats))))
			break;
		if ((error = SYSCTL_IN(req, &tcpstats_ary[cpu],
				       sizeof(struct tcp_stats))))
			break;
	}

	return (error);
}
SYSCTL_PROC(_net_inet_tcp, TCPCTL_STATS, stats, (CTLTYPE_OPAQUE | CTLFLAG_RW),
    0, 0, sysctl_tcpstats, "S,tcp_stats", "TCP statistics");
#else
SYSCTL_STRUCT(_net_inet_tcp, TCPCTL_STATS, stats, CTLFLAG_RW,
    &tcpstat, tcp_stats, "TCP statistics");
#endif

/*
 * Target size of TCP PCB hash tables. Must be a power of two.
 *
 * Note that this can be overridden by the kernel environment
 * variable net.inet.tcp.tcbhashsize
 */
#ifndef TCBHASHSIZE
#define	TCBHASHSIZE	512
#endif

/*
 * This is the actual shape of what we allocate using the zone
 * allocator.  Doing it this way allows us to protect both structures
 * using the same generation count, and also eliminates the overhead
 * of allocating tcpcbs separately.  By hiding the structure here,
 * we avoid changing most of the rest of the code (although it needs
 * to be changed, eventually, for greater efficiency).
 */
#define	ALIGNMENT	32
#define	ALIGNM1		(ALIGNMENT - 1)
struct	inp_tp {
	union {
		struct	inpcb inp;
		char	align[(sizeof(struct inpcb) + ALIGNM1) & ~ALIGNM1];
	} inp_tp_u;
	struct	tcpcb tcb;
	struct	callout inp_tp_rexmt, inp_tp_persist, inp_tp_keep, inp_tp_2msl;
	struct	callout inp_tp_delack;
};
#undef ALIGNMENT
#undef ALIGNM1

/*
 * Tcp initialization
 */
void
tcp_init()
{
	struct inpcbporthead *porthashbase;
	u_long porthashmask;
	struct vm_zone *ipi_zone;
	int hashsize = TCBHASHSIZE;
	int cpu;

	/*
	 * note: tcptemp is used for keepalives, and it is ok for an
	 * allocation to fail so do not specify MPF_INT.
	 */
	mpipe_init(&tcptemp_mpipe, M_TCPTEMP, sizeof(struct tcptemp),
		    25, -1, 0, NULL);

	tcp_ccgen = 1;
	tcp_cleartaocache();

	tcp_delacktime = TCPTV_DELACK;
	tcp_keepinit = TCPTV_KEEP_INIT;
	tcp_keepidle = TCPTV_KEEP_IDLE;
	tcp_keepintvl = TCPTV_KEEPINTVL;
	tcp_maxpersistidle = TCPTV_KEEP_IDLE;
	tcp_msl = TCPTV_MSL;
	tcp_rexmit_min = TCPTV_MIN;
	tcp_rexmit_slop = TCPTV_CPU_VAR;

	TUNABLE_INT_FETCH("net.inet.tcp.tcbhashsize", &hashsize);
	if (!powerof2(hashsize)) {
		printf("WARNING: TCB hash size not a power of 2\n");
		hashsize = 512; /* safe default */
	}
	tcp_tcbhashsize = hashsize;
	porthashbase = hashinit(hashsize, M_PCB, &porthashmask);
	ipi_zone = zinit("tcpcb", sizeof(struct inp_tp), maxsockets,
			 ZONE_INTERRUPT, 0);

	for (cpu = 0; cpu < ncpus2; cpu++) {
		in_pcbinfo_init(&tcbinfo[cpu]);
		tcbinfo[cpu].cpu = cpu;
		tcbinfo[cpu].hashbase = hashinit(hashsize, M_PCB,
		    &tcbinfo[cpu].hashmask);
		tcbinfo[cpu].porthashbase = porthashbase;
		tcbinfo[cpu].porthashmask = porthashmask;
		tcbinfo[cpu].wildcardhashbase = hashinit(hashsize, M_PCB,
		    &tcbinfo[cpu].wildcardhashmask);
		tcbinfo[cpu].ipi_zone = ipi_zone;
		TAILQ_INIT(&tcpcbackq[cpu]);
	}

	tcp_reass_maxseg = nmbclusters / 16;
	TUNABLE_INT_FETCH("net.inet.tcp.reass.maxsegments", &tcp_reass_maxseg);

#ifdef INET6
#define	TCP_MINPROTOHDR (sizeof(struct ip6_hdr) + sizeof(struct tcphdr))
#else
#define	TCP_MINPROTOHDR (sizeof(struct tcpiphdr))
#endif
	if (max_protohdr < TCP_MINPROTOHDR)
		max_protohdr = TCP_MINPROTOHDR;
	if (max_linkhdr + TCP_MINPROTOHDR > MHLEN)
		panic("tcp_init");
#undef TCP_MINPROTOHDR

	/*
	 * Initialize TCP statistics.
	 *
	 * It is layed out as an array which is has one element for UP,
	 * and SMP_MAXCPU elements for SMP.  This allows us to retain
	 * the access mechanism from userland for both UP and SMP.
	 */
#ifdef SMP
	for (cpu = 0; cpu < ncpus; ++cpu) {
		bzero(&tcpstats_ary[cpu], sizeof(struct tcp_stats));
	}
#else
	bzero(&tcpstat, sizeof(struct tcp_stats));
#endif

	syncache_init();
	tcp_sack_init();
	tcp_thread_init();
}

void
tcpmsg_service_loop(void *dummy)
{
	struct netmsg *msg;

	while ((msg = lwkt_waitport(&curthread->td_msgport, NULL))) {
		do {
			msg->nm_lmsg.ms_cmd.cm_func(&msg->nm_lmsg);
		} while ((msg = lwkt_getport(&curthread->td_msgport)) != NULL);
		tcp_willblock();
	}
}

static void
tcp_willblock(void)
{
	struct tcpcb *tp;
	int cpu = mycpu->gd_cpuid;

	while ((tp = TAILQ_FIRST(&tcpcbackq[cpu])) != NULL) {
		KKASSERT(tp->t_flags & TF_ONOUTPUTQ);
		tp->t_flags &= ~TF_ONOUTPUTQ;
		TAILQ_REMOVE(&tcpcbackq[cpu], tp, t_outputq);
		tcp_output(tp);
	}
}


/*
 * Fill in the IP and TCP headers for an outgoing packet, given the tcpcb.
 * tcp_template used to store this data in mbufs, but we now recopy it out
 * of the tcpcb each time to conserve mbufs.
 */
void
tcp_fillheaders(struct tcpcb *tp, void *ip_ptr, void *tcp_ptr)
{
	struct inpcb *inp = tp->t_inpcb;
	struct tcphdr *tcp_hdr = (struct tcphdr *)tcp_ptr;

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)ip_ptr;
		ip6->ip6_flow = (ip6->ip6_flow & ~IPV6_FLOWINFO_MASK) |
			(inp->in6p_flowinfo & IPV6_FLOWINFO_MASK);
		ip6->ip6_vfc = (ip6->ip6_vfc & ~IPV6_VERSION_MASK) |
			(IPV6_VERSION & IPV6_VERSION_MASK);
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = sizeof(struct tcphdr);
		ip6->ip6_src = inp->in6p_laddr;
		ip6->ip6_dst = inp->in6p_faddr;
		tcp_hdr->th_sum = 0;
	} else
#endif
	{
		struct ip *ip = (struct ip *) ip_ptr;

		ip->ip_vhl = IP_VHL_BORING;
		ip->ip_tos = 0;
		ip->ip_len = 0;
		ip->ip_id = 0;
		ip->ip_off = 0;
		ip->ip_ttl = 0;
		ip->ip_sum = 0;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst = inp->inp_faddr;
		tcp_hdr->th_sum = in_pseudo(ip->ip_src.s_addr,
				    ip->ip_dst.s_addr,
				    htons(sizeof(struct tcphdr) + IPPROTO_TCP));
	}

	tcp_hdr->th_sport = inp->inp_lport;
	tcp_hdr->th_dport = inp->inp_fport;
	tcp_hdr->th_seq = 0;
	tcp_hdr->th_ack = 0;
	tcp_hdr->th_x2 = 0;
	tcp_hdr->th_off = 5;
	tcp_hdr->th_flags = 0;
	tcp_hdr->th_win = 0;
	tcp_hdr->th_urp = 0;
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Allocates an mbuf and fills in a skeletal tcp/ip header.  The only
 * use for this function is in keepalives, which use tcp_respond.
 */
struct tcptemp *
tcp_maketemplate(struct tcpcb *tp)
{
	struct tcptemp *tmp;

	if ((tmp = mpipe_alloc_nowait(&tcptemp_mpipe)) == NULL)
		return (NULL);
	tcp_fillheaders(tp, &tmp->tt_ipgen, &tmp->tt_t);
	return (tmp);
}

void
tcp_freetemplate(struct tcptemp *tmp)
{
	mpipe_free(&tcptemp_mpipe, tmp);
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == NULL, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection.  If flags are given then we send
 * a message back to the TCP which originated the * segment ti,
 * and discard the mbuf containing it and any other attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 *
 * NOTE: If m != NULL, then ti must point to *inside* the mbuf.
 */
void
tcp_respond(struct tcpcb *tp, void *ipgen, struct tcphdr *th, struct mbuf *m,
	    tcp_seq ack, tcp_seq seq, int flags)
{
	int tlen;
	int win = 0;
	struct route *ro = NULL;
	struct route sro;
	struct ip *ip = ipgen;
	struct tcphdr *nth;
	int ipflags = 0;
	struct route_in6 *ro6 = NULL;
	struct route_in6 sro6;
	struct ip6_hdr *ip6 = ipgen;
#ifdef INET6
	boolean_t isipv6 = (IP_VHL_V(ip->ip_vhl) == 6);
#else
	const boolean_t isipv6 = FALSE;
#endif

	if (tp != NULL) {
		if (!(flags & TH_RST)) {
			win = sbspace(&tp->t_inpcb->inp_socket->so_rcv);
			if (win > (long)TCP_MAXWIN << tp->rcv_scale)
				win = (long)TCP_MAXWIN << tp->rcv_scale;
		}
		if (isipv6)
			ro6 = &tp->t_inpcb->in6p_route;
		else
			ro = &tp->t_inpcb->inp_route;
	} else {
		if (isipv6) {
			ro6 = &sro6;
			bzero(ro6, sizeof *ro6);
		} else {
			ro = &sro;
			bzero(ro, sizeof *ro);
		}
	}
	if (m == NULL) {
		m = m_gethdr(MB_DONTWAIT, MT_HEADER);
		if (m == NULL)
			return;
		tlen = 0;
		m->m_data += max_linkhdr;
		if (isipv6) {
			bcopy(ip6, mtod(m, caddr_t), sizeof(struct ip6_hdr));
			ip6 = mtod(m, struct ip6_hdr *);
			nth = (struct tcphdr *)(ip6 + 1);
		} else {
			bcopy(ip, mtod(m, caddr_t), sizeof(struct ip));
			ip = mtod(m, struct ip *);
			nth = (struct tcphdr *)(ip + 1);
		}
		bcopy(th, nth, sizeof(struct tcphdr));
		flags = TH_ACK;
	} else {
		m_freem(m->m_next);
		m->m_next = NULL;
		m->m_data = (caddr_t)ipgen;
		/* m_len is set later */
		tlen = 0;
#define	xchg(a, b, type) { type t; t = a; a = b; b = t; }
		if (isipv6) {
			xchg(ip6->ip6_dst, ip6->ip6_src, struct in6_addr);
			nth = (struct tcphdr *)(ip6 + 1);
		} else {
			xchg(ip->ip_dst.s_addr, ip->ip_src.s_addr, n_long);
			nth = (struct tcphdr *)(ip + 1);
		}
		if (th != nth) {
			/*
			 * this is usually a case when an extension header
			 * exists between the IPv6 header and the
			 * TCP header.
			 */
			nth->th_sport = th->th_sport;
			nth->th_dport = th->th_dport;
		}
		xchg(nth->th_dport, nth->th_sport, n_short);
#undef xchg
	}
	if (isipv6) {
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_plen = htons((u_short)(sizeof(struct tcphdr) + tlen));
		tlen += sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
	} else {
		tlen += sizeof(struct tcpiphdr);
		ip->ip_len = tlen;
		ip->ip_ttl = ip_defttl;
	}
	m->m_len = tlen;
	m->m_pkthdr.len = tlen;
	m->m_pkthdr.rcvif = (struct ifnet *) NULL;
	nth->th_seq = htonl(seq);
	nth->th_ack = htonl(ack);
	nth->th_x2 = 0;
	nth->th_off = sizeof(struct tcphdr) >> 2;
	nth->th_flags = flags;
	if (tp != NULL)
		nth->th_win = htons((u_short) (win >> tp->rcv_scale));
	else
		nth->th_win = htons((u_short)win);
	nth->th_urp = 0;
	if (isipv6) {
		nth->th_sum = 0;
		nth->th_sum = in6_cksum(m, IPPROTO_TCP,
					sizeof(struct ip6_hdr),
					tlen - sizeof(struct ip6_hdr));
		ip6->ip6_hlim = in6_selecthlim(tp ? tp->t_inpcb : NULL,
					       (ro6 && ro6->ro_rt) ?
						ro6->ro_rt->rt_ifp : NULL);
	} else {
		nth->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons((u_short)(tlen - sizeof(struct ip) + ip->ip_p)));
		m->m_pkthdr.csum_flags = CSUM_TCP;
		m->m_pkthdr.csum_data = offsetof(struct tcphdr, th_sum);
	}
#ifdef TCPDEBUG
	if (tp == NULL || (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_OUTPUT, 0, tp, mtod(m, void *), th, 0);
#endif
	if (isipv6) {
		ip6_output(m, NULL, ro6, ipflags, NULL, NULL,
			   tp ? tp->t_inpcb : NULL);
		if ((ro6 == &sro6) && (ro6->ro_rt != NULL)) {
			RTFREE(ro6->ro_rt);
			ro6->ro_rt = NULL;
		}
	} else {
		ip_output(m, NULL, ro, ipflags, NULL, tp ? tp->t_inpcb : NULL);
		if ((ro == &sro) && (ro->ro_rt != NULL)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = NULL;
		}
	}
}

/*
 * Create a new TCP control block, making an
 * empty reassembly queue and hooking it to the argument
 * protocol control block.  The `inp' parameter must have
 * come from the zone allocator set up in tcp_init().
 */
struct tcpcb *
tcp_newtcpcb(struct inpcb *inp)
{
	struct inp_tp *it;
	struct tcpcb *tp;
#ifdef INET6
	boolean_t isipv6 = ((inp->inp_vflag & INP_IPV6) != 0);
#else
	const boolean_t isipv6 = FALSE;
#endif

	it = (struct inp_tp *)inp;
	tp = &it->tcb;
	bzero(tp, sizeof(struct tcpcb));
	LIST_INIT(&tp->t_segq);
	tp->t_maxseg = tp->t_maxopd = isipv6 ? tcp_v6mssdflt : tcp_mssdflt;

	/* Set up our timeouts. */
	callout_init(tp->tt_rexmt = &it->inp_tp_rexmt);
	callout_init(tp->tt_persist = &it->inp_tp_persist);
	callout_init(tp->tt_keep = &it->inp_tp_keep);
	callout_init(tp->tt_2msl = &it->inp_tp_2msl);
	callout_init(tp->tt_delack = &it->inp_tp_delack);

	if (tcp_do_rfc1323)
		tp->t_flags = (TF_REQ_SCALE | TF_REQ_TSTMP);
	if (tcp_do_rfc1644)
		tp->t_flags |= TF_REQ_CC;
	tp->t_inpcb = inp;	/* XXX */
	tp->t_state = TCPS_CLOSED;
	/*
	 * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
	 * rtt estimate.  Set rttvar so that srtt + 4 * rttvar gives
	 * reasonable initial retransmit time.
	 */
	tp->t_srtt = TCPTV_SRTTBASE;
	tp->t_rttvar =
	    ((TCPTV_RTOBASE - TCPTV_SRTTBASE) << TCP_RTTVAR_SHIFT) / 4;
	tp->t_rttmin = tcp_rexmit_min;
	tp->t_rxtcur = TCPTV_RTOBASE;
	tp->snd_cwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_bwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->snd_ssthresh = TCP_MAXWIN << TCP_MAX_WINSHIFT;
	tp->t_rcvtime = ticks;
	/*
	 * IPv4 TTL initialization is necessary for an IPv6 socket as well,
	 * because the socket may be bound to an IPv6 wildcard address,
	 * which may match an IPv4-mapped IPv6 address.
	 */
	inp->inp_ip_ttl = ip_defttl;
	inp->inp_ppcb = tp;
	tcp_sack_tcpcb_init(tp);
	return (tp);		/* XXX */
}

/*
 * Drop a TCP connection, reporting the specified error.
 * If connection is synchronized, then send a RST to peer.
 */
struct tcpcb *
tcp_drop(struct tcpcb *tp, int errno)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVERCVDSYN(tp->t_state)) {
		tp->t_state = TCPS_CLOSED;
		tcp_output(tp);
		tcpstat.tcps_drops++;
	} else
		tcpstat.tcps_conndrops++;
	if (errno == ETIMEDOUT && tp->t_softerror)
		errno = tp->t_softerror;
	so->so_error = errno;
	return (tcp_close(tp));
}

#ifdef SMP

struct netmsg_remwildcard {
	struct lwkt_msg		nm_lmsg;
	struct inpcb		*nm_inp;
	struct inpcbinfo	*nm_pcbinfo;
#if defined(INET6)
	int			nm_isinet6;
#else
	int			nm_unused01;
#endif
};

/*
 * Wildcard inpcb's on SMP boxes must be removed from all cpus before the
 * inp can be detached.  We do this by cycling through the cpus, ending up
 * on the cpu controlling the inp last and then doing the disconnect.
 */
static int
in_pcbremwildcardhash_handler(struct lwkt_msg *msg0)
{
	struct netmsg_remwildcard *msg = (struct netmsg_remwildcard *)msg0;
	int cpu;

	cpu = msg->nm_pcbinfo->cpu;

	if (cpu == msg->nm_inp->inp_pcbinfo->cpu) {
		/* note: detach removes any wildcard hash entry */
#ifdef INET6
		if (msg->nm_isinet6)
			in6_pcbdetach(msg->nm_inp);
		else
#endif
			in_pcbdetach(msg->nm_inp);
		lwkt_replymsg(&msg->nm_lmsg, 0);
	} else {
		in_pcbremwildcardhash_oncpu(msg->nm_inp, msg->nm_pcbinfo);
		cpu = (cpu + 1) % ncpus2;
		msg->nm_pcbinfo = &tcbinfo[cpu];
		lwkt_forwardmsg(tcp_cport(cpu), &msg->nm_lmsg);
	}
	return (EASYNC);
}

#endif

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */
struct tcpcb *
tcp_close(struct tcpcb *tp)
{
	struct tseg_qent *q;
	struct inpcb *inp = tp->t_inpcb;
	struct socket *so = inp->inp_socket;
	struct rtentry *rt;
	boolean_t dosavessthresh;
#ifdef SMP
	int cpu;
#endif
#ifdef INET6
	boolean_t isipv6 = ((inp->inp_vflag & INP_IPV6) != 0);
	boolean_t isafinet6 = (INP_CHECK_SOCKAF(so, AF_INET6) != 0);
#else
	const boolean_t isipv6 = FALSE;
#endif

	/*
	 * The tp is not instantly destroyed in the wildcard case.  Setting
	 * the state to TCPS_TERMINATING will prevent the TCP stack from
	 * messing with it, though it should be noted that this change may
	 * not take effect on other cpus until we have chained the wildcard
	 * hash removal.
	 *
	 * XXX we currently depend on the BGL to synchronize the tp->t_state
	 * update and prevent other tcp protocol threads from accepting new
	 * connections on the listen socket we might be trying to close down.
	 */
	KKASSERT(tp->t_state != TCPS_TERMINATING);
	tp->t_state = TCPS_TERMINATING;

	/*
	 * Make sure that all of our timers are stopped before we
	 * delete the PCB.
	 */
	callout_stop(tp->tt_rexmt);
	callout_stop(tp->tt_persist);
	callout_stop(tp->tt_keep);
	callout_stop(tp->tt_2msl);
	callout_stop(tp->tt_delack);

	if (tp->t_flags & TF_ONOUTPUTQ) {
		KKASSERT(tp->tt_cpu == mycpu->gd_cpuid);
		TAILQ_REMOVE(&tcpcbackq[tp->tt_cpu], tp, t_outputq);
		tp->t_flags &= ~TF_ONOUTPUTQ;
	}

	/*
	 * If we got enough samples through the srtt filter,
	 * save the rtt and rttvar in the routing entry.
	 * 'Enough' is arbitrarily defined as the 16 samples.
	 * 16 samples is enough for the srtt filter to converge
	 * to within 5% of the correct value; fewer samples and
	 * we could save a very bogus rtt.
	 *
	 * Don't update the default route's characteristics and don't
	 * update anything that the user "locked".
	 */
	if (tp->t_rttupdated >= 16) {
		u_long i = 0;

		if (isipv6) {
			struct sockaddr_in6 *sin6;

			if ((rt = inp->in6p_route.ro_rt) == NULL)
				goto no_valid_rt;
			sin6 = (struct sockaddr_in6 *)rt_key(rt);
			if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
				goto no_valid_rt;
		} else
			if ((rt = inp->inp_route.ro_rt) == NULL ||
			    ((struct sockaddr_in *)rt_key(rt))->
			     sin_addr.s_addr == INADDR_ANY)
				goto no_valid_rt;

		if (!(rt->rt_rmx.rmx_locks & RTV_RTT)) {
			i = tp->t_srtt * (RTM_RTTUNIT / (hz * TCP_RTT_SCALE));
			if (rt->rt_rmx.rmx_rtt && i)
				/*
				 * filter this update to half the old & half
				 * the new values, converting scale.
				 * See route.h and tcp_var.h for a
				 * description of the scaling constants.
				 */
				rt->rt_rmx.rmx_rtt =
				    (rt->rt_rmx.rmx_rtt + i) / 2;
			else
				rt->rt_rmx.rmx_rtt = i;
			tcpstat.tcps_cachedrtt++;
		}
		if (!(rt->rt_rmx.rmx_locks & RTV_RTTVAR)) {
			i = tp->t_rttvar *
			    (RTM_RTTUNIT / (hz * TCP_RTTVAR_SCALE));
			if (rt->rt_rmx.rmx_rttvar && i)
				rt->rt_rmx.rmx_rttvar =
				    (rt->rt_rmx.rmx_rttvar + i) / 2;
			else
				rt->rt_rmx.rmx_rttvar = i;
			tcpstat.tcps_cachedrttvar++;
		}
		/*
		 * The old comment here said:
		 * update the pipelimit (ssthresh) if it has been updated
		 * already or if a pipesize was specified & the threshhold
		 * got below half the pipesize.  I.e., wait for bad news
		 * before we start updating, then update on both good
		 * and bad news.
		 *
		 * But we want to save the ssthresh even if no pipesize is
		 * specified explicitly in the route, because such
		 * connections still have an implicit pipesize specified
		 * by the global tcp_sendspace.  In the absence of a reliable
		 * way to calculate the pipesize, it will have to do.
		 */
		i = tp->snd_ssthresh;
		if (rt->rt_rmx.rmx_sendpipe != 0)
			dosavessthresh = (i < rt->rt_rmx.rmx_sendpipe/2);
		else
			dosavessthresh = (i < so->so_snd.sb_hiwat/2);
		if (dosavessthresh ||
		    (!(rt->rt_rmx.rmx_locks & RTV_SSTHRESH) && (i != 0) &&
		     (rt->rt_rmx.rmx_ssthresh != 0))) {
			/*
			 * convert the limit from user data bytes to
			 * packets then to packet data bytes.
			 */
			i = (i + tp->t_maxseg / 2) / tp->t_maxseg;
			if (i < 2)
				i = 2;
			i *= tp->t_maxseg +
			     (isipv6 ?
			      sizeof(struct ip6_hdr) + sizeof(struct tcphdr) :
			      sizeof(struct tcpiphdr));
			if (rt->rt_rmx.rmx_ssthresh)
				rt->rt_rmx.rmx_ssthresh =
				    (rt->rt_rmx.rmx_ssthresh + i) / 2;
			else
				rt->rt_rmx.rmx_ssthresh = i;
			tcpstat.tcps_cachedssthresh++;
		}
	}

no_valid_rt:
	/* free the reassembly queue, if any */
	while((q = LIST_FIRST(&tp->t_segq)) != NULL) {
		LIST_REMOVE(q, tqe_q);
		m_freem(q->tqe_m);
		FREE(q, M_TSEGQ);
		tcp_reass_qsize--;
	}
	/* throw away SACK blocks in scoreboard*/
	if (TCP_DO_SACK(tp))
		tcp_sack_cleanup(&tp->scb);

	inp->inp_ppcb = NULL;
	soisdisconnected(so);
	/*
	 * Discard the inp.  In the SMP case a wildcard inp's hash (created
	 * by a listen socket or an INADDR_ANY udp socket) is replicated
	 * for each protocol thread and must be removed in the context of
	 * that thread.  This is accomplished by chaining the message
	 * through the cpus.
	 *
	 * If the inp is not wildcarded we simply detach, which will remove
	 * the any hashes still present for this inp.
	 */
#ifdef SMP
	if (inp->inp_flags & INP_WILDCARD_MP) {
		struct netmsg_remwildcard *msg;

		cpu = (inp->inp_pcbinfo->cpu + 1) % ncpus2;
		msg = malloc(sizeof(struct netmsg_remwildcard),
			    M_LWKTMSG, M_INTWAIT);
		lwkt_initmsg(&msg->nm_lmsg, &netisr_afree_rport, 0,
		    lwkt_cmd_func(in_pcbremwildcardhash_handler),
		    lwkt_cmd_op_none);
#ifdef INET6
		msg->nm_isinet6 = isafinet6;
#endif
		msg->nm_inp = inp;
		msg->nm_pcbinfo = &tcbinfo[cpu];
		lwkt_sendmsg(tcp_cport(cpu), &msg->nm_lmsg);
	} else
#endif
	{
		/* note: detach removes any wildcard hash entry */
#ifdef INET6
		if (isafinet6)
			in6_pcbdetach(inp);
		else
#endif
			in_pcbdetach(inp);
	}
	tcpstat.tcps_closed++;
	return (NULL);
}

static __inline void
tcp_drain_oncpu(struct inpcbhead *head)
{
	struct inpcb *inpb;
	struct tcpcb *tcpb;
	struct tseg_qent *te;

	LIST_FOREACH(inpb, head, inp_list) {
		if (inpb->inp_flags & INP_PLACEMARKER)
			continue;
		if ((tcpb = intotcpcb(inpb))) {
			while ((te = LIST_FIRST(&tcpb->t_segq)) != NULL) {
				LIST_REMOVE(te, tqe_q);
				m_freem(te->tqe_m);
				FREE(te, M_TSEGQ);
				tcp_reass_qsize--;
			}
		}
	}
}

#ifdef SMP
struct netmsg_tcp_drain {
	struct lwkt_msg		nm_lmsg;
	struct inpcbhead	*nm_head;
};

static int
tcp_drain_handler(lwkt_msg_t lmsg)
{
	struct netmsg_tcp_drain *nm = (void *)lmsg;

	tcp_drain_oncpu(nm->nm_head);
	lwkt_replymsg(lmsg, 0);
	return(EASYNC);
}
#endif

void
tcp_drain()
{
#ifdef SMP
	int cpu;
#endif

	if (!do_tcpdrain)
		return;

	/*
	 * Walk the tcpbs, if existing, and flush the reassembly queue,
	 * if there is one...
	 * XXX: The "Net/3" implementation doesn't imply that the TCP
	 *	reassembly queue should be flushed, but in a situation
	 *	where we're really low on mbufs, this is potentially
	 *	useful.
	 */
#ifdef SMP
	for (cpu = 0; cpu < ncpus2; cpu++) {
		struct netmsg_tcp_drain *msg;

		if (cpu == mycpu->gd_cpuid) {
			tcp_drain_oncpu(&tcbinfo[cpu].pcblisthead);
		} else {
			msg = malloc(sizeof(struct netmsg_tcp_drain),
				    M_LWKTMSG, M_NOWAIT);
			if (msg == NULL)
				continue;
			lwkt_initmsg(&msg->nm_lmsg, &netisr_afree_rport, 0,
				lwkt_cmd_func(tcp_drain_handler),
				lwkt_cmd_op_none);
			msg->nm_head = &tcbinfo[cpu].pcblisthead;
			lwkt_sendmsg(tcp_cport(cpu), &msg->nm_lmsg);
		}
	}
#else
	tcp_drain_oncpu(&tcbinfo[0].pcblisthead);
#endif
}

/*
 * Notify a tcp user of an asynchronous error;
 * store error as soft error, but wake up user
 * (for now, won't do anything until can select for soft error).
 *
 * Do not wake up user since there currently is no mechanism for
 * reporting soft errors (yet - a kqueue filter may be added).
 */
static void
tcp_notify(struct inpcb *inp, int error)
{
	struct tcpcb *tp = intotcpcb(inp);

	/*
	 * Ignore some errors if we are hooked up.
	 * If connection hasn't completed, has retransmitted several times,
	 * and receives a second error, give up now.  This is better
	 * than waiting a long time to establish a connection that
	 * can never complete.
	 */
	if (tp->t_state == TCPS_ESTABLISHED &&
	     (error == EHOSTUNREACH || error == ENETUNREACH ||
	      error == EHOSTDOWN)) {
		return;
	} else if (tp->t_state < TCPS_ESTABLISHED && tp->t_rxtshift > 3 &&
	    tp->t_softerror)
		tcp_drop(tp, error);
	else
		tp->t_softerror = error;
#if 0
	wakeup(&so->so_timeo);
	sorwakeup(so);
	sowwakeup(so);
#endif
}

static int
tcp_pcblist(SYSCTL_HANDLER_ARGS)
{
	int error, i, n;
	struct inpcb *marker;
	struct inpcb *inp;
	inp_gen_t gencnt;
	globaldata_t gd;
	int origcpu, ccpu;

	error = 0;
	n = 0;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == NULL) {
		for (ccpu = 0; ccpu < ncpus; ++ccpu) {
			gd = globaldata_find(ccpu);
			n += tcbinfo[gd->gd_cpuid].ipi_count;
		}
		req->oldidx = (n + n/8 + 10) * sizeof(struct xtcpcb);
		return (0);
	}

	if (req->newptr != NULL)
		return (EPERM);

	marker = malloc(sizeof(struct inpcb), M_TEMP, M_WAITOK|M_ZERO);
	marker->inp_flags |= INP_PLACEMARKER;

	/*
	 * OK, now we're committed to doing something.  Run the inpcb list
	 * for each cpu in the system and construct the output.  Use a
	 * list placemarker to deal with list changes occuring during
	 * copyout blockages (but otherwise depend on being on the correct
	 * cpu to avoid races).
	 */
	origcpu = mycpu->gd_cpuid;
	for (ccpu = 1; ccpu <= ncpus && error == 0; ++ccpu) {
		globaldata_t rgd;
		caddr_t inp_ppcb;
		struct xtcpcb xt;
		int cpu_id;

		cpu_id = (origcpu + ccpu) % ncpus;
		if ((smp_active_mask & (1 << cpu_id)) == 0)
			continue;
		rgd = globaldata_find(cpu_id);
		lwkt_setcpu_self(rgd);

		/* indicate change of CPU */
		cpu_mb1();

		gencnt = tcbinfo[cpu_id].ipi_gencnt;
		n = tcbinfo[cpu_id].ipi_count;

		LIST_INSERT_HEAD(&tcbinfo[cpu_id].pcblisthead, marker, inp_list);
		i = 0;
		while ((inp = LIST_NEXT(marker, inp_list)) != NULL && i < n) {
			/*
			 * process a snapshot of pcbs, ignoring placemarkers
			 * and using our own to allow SYSCTL_OUT to block.
			 */
			LIST_REMOVE(marker, inp_list);
			LIST_INSERT_AFTER(inp, marker, inp_list);

			if (inp->inp_flags & INP_PLACEMARKER)
				continue;
			if (inp->inp_gencnt > gencnt)
				continue;
			if (prison_xinpcb(req->td, inp))
				continue;

			xt.xt_len = sizeof xt;
			bcopy(inp, &xt.xt_inp, sizeof *inp);
			inp_ppcb = inp->inp_ppcb;
			if (inp_ppcb != NULL)
				bcopy(inp_ppcb, &xt.xt_tp, sizeof xt.xt_tp);
			else
				bzero(&xt.xt_tp, sizeof xt.xt_tp);
			if (inp->inp_socket)
				sotoxsocket(inp->inp_socket, &xt.xt_socket);
			if ((error = SYSCTL_OUT(req, &xt, sizeof xt)) != 0)
				break;
			++i;
		}
		LIST_REMOVE(marker, inp_list);
		if (error == 0 && i < n) {
			bzero(&xt, sizeof xt);
			xt.xt_len = sizeof xt;
			while (i < n) {
				error = SYSCTL_OUT(req, &xt, sizeof xt);
				if (error)
					break;
				++i;
			}
		}
	}

	/*
	 * Make sure we are on the same cpu we were on originally, since
	 * higher level callers expect this.  Also don't pollute caches with
	 * migrated userland data by (eventually) returning to userland
	 * on a different cpu.
	 */
	lwkt_setcpu_self(globaldata_find(origcpu));
	free(marker, M_TEMP);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, TCPCTL_PCBLIST, pcblist, CTLFLAG_RD, 0, 0,
	    tcp_pcblist, "S,xtcpcb", "List of active TCP connections");

static int
tcp_getcred(SYSCTL_HANDLER_ARGS)
{
	struct sockaddr_in addrs[2];
	struct inpcb *inp;
	int cpu;
	int error, s;

	error = suser(req->td);
	if (error != 0)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof addrs);
	if (error != 0)
		return (error);
	s = splnet();

	cpu = tcp_addrcpu(addrs[1].sin_addr.s_addr, addrs[1].sin_port,
	    addrs[0].sin_addr.s_addr, addrs[0].sin_port);
	inp = in_pcblookup_hash(&tcbinfo[cpu], addrs[1].sin_addr,
	    addrs[1].sin_port, addrs[0].sin_addr, addrs[0].sin_port, 0, NULL);
	if (inp == NULL || inp->inp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = SYSCTL_OUT(req, inp->inp_socket->so_cred, sizeof(struct ucred));
out:
	splx(s);
	return (error);
}

SYSCTL_PROC(_net_inet_tcp, OID_AUTO, getcred, (CTLTYPE_OPAQUE | CTLFLAG_RW),
    0, 0, tcp_getcred, "S,ucred", "Get the ucred of a TCP connection");

#ifdef INET6
static int
tcp6_getcred(SYSCTL_HANDLER_ARGS)
{
	struct sockaddr_in6 addrs[2];
	struct inpcb *inp;
	int error, s;
	boolean_t mapped = FALSE;

	error = suser(req->td);
	if (error != 0)
		return (error);
	error = SYSCTL_IN(req, addrs, sizeof addrs);
	if (error != 0)
		return (error);
	if (IN6_IS_ADDR_V4MAPPED(&addrs[0].sin6_addr)) {
		if (IN6_IS_ADDR_V4MAPPED(&addrs[1].sin6_addr))
			mapped = TRUE;
		else
			return (EINVAL);
	}
	s = splnet();
	if (mapped) {
		inp = in_pcblookup_hash(&tcbinfo[0],
		    *(struct in_addr *)&addrs[1].sin6_addr.s6_addr[12],
		    addrs[1].sin6_port,
		    *(struct in_addr *)&addrs[0].sin6_addr.s6_addr[12],
		    addrs[0].sin6_port,
		    0, NULL);
	} else {
		inp = in6_pcblookup_hash(&tcbinfo[0],
		    &addrs[1].sin6_addr, addrs[1].sin6_port,
		    &addrs[0].sin6_addr, addrs[0].sin6_port,
		    0, NULL);
	}
	if (inp == NULL || inp->inp_socket == NULL) {
		error = ENOENT;
		goto out;
	}
	error = SYSCTL_OUT(req, inp->inp_socket->so_cred, sizeof(struct ucred));
out:
	splx(s);
	return (error);
}

SYSCTL_PROC(_net_inet6_tcp6, OID_AUTO, getcred, (CTLTYPE_OPAQUE | CTLFLAG_RW),
	    0, 0,
	    tcp6_getcred, "S,ucred", "Get the ucred of a TCP6 connection");
#endif

void
tcp_ctlinput(int cmd, struct sockaddr *sa, void *vip)
{
	struct ip *ip = vip;
	struct tcphdr *th;
	struct in_addr faddr;
	struct inpcb *inp;
	struct tcpcb *tp;
	void (*notify)(struct inpcb *, int) = tcp_notify;
	tcp_seq icmpseq;
	int arg, cpu, s;

	if ((unsigned)cmd >= PRC_NCMDS || inetctlerrmap[cmd] == 0) {
		return;
	}

	faddr = ((struct sockaddr_in *)sa)->sin_addr;
	if (sa->sa_family != AF_INET || faddr.s_addr == INADDR_ANY)
		return;

	arg = inetctlerrmap[cmd];
	if (cmd == PRC_QUENCH) {
		notify = tcp_quench;
	} else if (icmp_may_rst &&
		   (cmd == PRC_UNREACH_ADMIN_PROHIB ||
		    cmd == PRC_UNREACH_PORT ||
		    cmd == PRC_TIMXCEED_INTRANS) &&
		   ip != NULL) {
		notify = tcp_drop_syn_sent;
	} else if (cmd == PRC_MSGSIZE) {
		struct icmp *icmp = (struct icmp *)
		    ((caddr_t)ip - offsetof(struct icmp, icmp_ip));

		arg = ntohs(icmp->icmp_nextmtu);
		notify = tcp_mtudisc;
	} else if (PRC_IS_REDIRECT(cmd)) {
		ip = NULL;
		notify = in_rtchange;
	} else if (cmd == PRC_HOSTDEAD) {
		ip = NULL;
	}

	if (ip != NULL) {
		s = splnet();
		th = (struct tcphdr *)((caddr_t)ip +
				       (IP_VHL_HL(ip->ip_vhl) << 2));
		cpu = tcp_addrcpu(faddr.s_addr, th->th_dport,
				  ip->ip_src.s_addr, th->th_sport);
		inp = in_pcblookup_hash(&tcbinfo[cpu], faddr, th->th_dport,
					ip->ip_src, th->th_sport, 0, NULL);
		if ((inp != NULL) && (inp->inp_socket != NULL)) {
			icmpseq = htonl(th->th_seq);
			tp = intotcpcb(inp);
			if (SEQ_GEQ(icmpseq, tp->snd_una) &&
			    SEQ_LT(icmpseq, tp->snd_max))
				(*notify)(inp, arg);
		} else {
			struct in_conninfo inc;

			inc.inc_fport = th->th_dport;
			inc.inc_lport = th->th_sport;
			inc.inc_faddr = faddr;
			inc.inc_laddr = ip->ip_src;
#ifdef INET6
			inc.inc_isipv6 = 0;
#endif
			syncache_unreach(&inc, th);
		}
		splx(s);
	} else {
		for (cpu = 0; cpu < ncpus2; cpu++) {
			in_pcbnotifyall(&tcbinfo[cpu].pcblisthead, faddr, arg,
					notify);
		}
	}
}

#ifdef INET6
void
tcp6_ctlinput(int cmd, struct sockaddr *sa, void *d)
{
	struct tcphdr th;
	void (*notify) (struct inpcb *, int) = tcp_notify;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	struct ip6ctlparam *ip6cp = NULL;
	const struct sockaddr_in6 *sa6_src = NULL;
	int off;
	struct tcp_portonly {
		u_int16_t th_sport;
		u_int16_t th_dport;
	} *thp;
	int arg;

	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	arg = 0;
	if (cmd == PRC_QUENCH)
		notify = tcp_quench;
	else if (cmd == PRC_MSGSIZE) {
		struct ip6ctlparam *ip6cp = d;
		struct icmp6_hdr *icmp6 = ip6cp->ip6c_icmp6;

		arg = ntohl(icmp6->icmp6_mtu);
		notify = tcp_mtudisc;
	} else if (!PRC_IS_REDIRECT(cmd) &&
		 ((unsigned)cmd > PRC_NCMDS || inet6ctlerrmap[cmd] == 0)) {
		return;
	}

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		sa6_src = ip6cp->ip6c_src;
	} else {
		m = NULL;
		ip6 = NULL;
		off = 0;	/* fool gcc */
		sa6_src = &sa6_any;
	}

	if (ip6 != NULL) {
		struct in_conninfo inc;
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof *thp)
			return;

		bzero(&th, sizeof th);
		m_copydata(m, off, sizeof *thp, (caddr_t)&th);

		in6_pcbnotify(&tcbinfo[0].pcblisthead, sa, th.th_dport,
		    (struct sockaddr *)ip6cp->ip6c_src,
		    th.th_sport, cmd, arg, notify);

		inc.inc_fport = th.th_dport;
		inc.inc_lport = th.th_sport;
		inc.inc6_faddr = ((struct sockaddr_in6 *)sa)->sin6_addr;
		inc.inc6_laddr = ip6cp->ip6c_src->sin6_addr;
		inc.inc_isipv6 = 1;
		syncache_unreach(&inc, &th);
	} else
		in6_pcbnotify(&tcbinfo[0].pcblisthead, sa, 0,
		    (const struct sockaddr *)sa6_src, 0, cmd, arg, notify);
}
#endif

/*
 * Following is where TCP initial sequence number generation occurs.
 *
 * There are two places where we must use initial sequence numbers:
 * 1.  In SYN-ACK packets.
 * 2.  In SYN packets.
 *
 * All ISNs for SYN-ACK packets are generated by the syncache.  See
 * tcp_syncache.c for details.
 *
 * The ISNs in SYN packets must be monotonic; TIME_WAIT recycling
 * depends on this property.  In addition, these ISNs should be
 * unguessable so as to prevent connection hijacking.  To satisfy
 * the requirements of this situation, the algorithm outlined in
 * RFC 1948 is used to generate sequence numbers.
 *
 * Implementation details:
 *
 * Time is based off the system timer, and is corrected so that it
 * increases by one megabyte per second.  This allows for proper
 * recycling on high speed LANs while still leaving over an hour
 * before rollover.
 *
 * net.inet.tcp.isn_reseed_interval controls the number of seconds
 * between seeding of isn_secret.  This is normally set to zero,
 * as reseeding should not be necessary.
 *
 */

#define	ISN_BYTES_PER_SECOND 1048576

u_char isn_secret[32];
int isn_last_reseed;
MD5_CTX isn_ctx;

tcp_seq
tcp_new_isn(struct tcpcb *tp)
{
	u_int32_t md5_buffer[4];
	tcp_seq new_isn;

	/* Seed if this is the first use, reseed if requested. */
	if ((isn_last_reseed == 0) || ((tcp_isn_reseed_interval > 0) &&
	     (((u_int)isn_last_reseed + (u_int)tcp_isn_reseed_interval*hz)
		< (u_int)ticks))) {
		read_random_unlimited(&isn_secret, sizeof isn_secret);
		isn_last_reseed = ticks;
	}

	/* Compute the md5 hash and return the ISN. */
	MD5Init(&isn_ctx);
	MD5Update(&isn_ctx, (u_char *)&tp->t_inpcb->inp_fport, sizeof(u_short));
	MD5Update(&isn_ctx, (u_char *)&tp->t_inpcb->inp_lport, sizeof(u_short));
#ifdef INET6
	if (tp->t_inpcb->inp_vflag & INP_IPV6) {
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->in6p_faddr,
			  sizeof(struct in6_addr));
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->in6p_laddr,
			  sizeof(struct in6_addr));
	} else
#endif
	{
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_faddr,
			  sizeof(struct in_addr));
		MD5Update(&isn_ctx, (u_char *) &tp->t_inpcb->inp_laddr,
			  sizeof(struct in_addr));
	}
	MD5Update(&isn_ctx, (u_char *) &isn_secret, sizeof(isn_secret));
	MD5Final((u_char *) &md5_buffer, &isn_ctx);
	new_isn = (tcp_seq) md5_buffer[0];
	new_isn += ticks * (ISN_BYTES_PER_SECOND / hz);
	return (new_isn);
}

/*
 * When a source quench is received, close congestion window
 * to one segment.  We will gradually open it again as we proceed.
 */
void
tcp_quench(struct inpcb *inp, int errno)
{
	struct tcpcb *tp = intotcpcb(inp);

	if (tp != NULL)
		tp->snd_cwnd = tp->t_maxseg;
}

/*
 * When a specific ICMP unreachable message is received and the
 * connection state is SYN-SENT, drop the connection.  This behavior
 * is controlled by the icmp_may_rst sysctl.
 */
void
tcp_drop_syn_sent(struct inpcb *inp, int errno)
{
	struct tcpcb *tp = intotcpcb(inp);

	if ((tp != NULL) && (tp->t_state == TCPS_SYN_SENT))
		tcp_drop(tp, errno);
}

/*
 * When a `need fragmentation' ICMP is received, update our idea of the MSS
 * based on the new value in the route.  Also nudge TCP to send something,
 * since we know the packet we just sent was dropped.
 * This duplicates some code in the tcp_mss() function in tcp_input.c.
 */
void
tcp_mtudisc(struct inpcb *inp, int mtu)
{
	struct tcpcb *tp = intotcpcb(inp);
	struct rtentry *rt;
	struct socket *so = inp->inp_socket;
	int maxopd, mss;
#ifdef INET6
	boolean_t isipv6 = ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0);
#else
	const boolean_t isipv6 = FALSE;
#endif

	if (tp == NULL)
		return;

	/*
	 * If no MTU is provided in the ICMP message, use the
	 * next lower likely value, as specified in RFC 1191.
	 */
	if (mtu == 0) {
		int oldmtu;

		oldmtu = tp->t_maxopd + 
		    (isipv6 ?
		     sizeof(struct ip6_hdr) + sizeof(struct tcphdr) :
		     sizeof(struct tcpiphdr));
		mtu = ip_next_mtu(oldmtu, 0);
	}

	if (isipv6)
		rt = tcp_rtlookup6(&inp->inp_inc);
	else
		rt = tcp_rtlookup(&inp->inp_inc);
	if (rt != NULL) {
		struct rmxp_tao *taop = rmx_taop(rt->rt_rmx);

		if (rt->rt_rmx.rmx_mtu != 0 && rt->rt_rmx.rmx_mtu < mtu)
			mtu = rt->rt_rmx.rmx_mtu;

		maxopd = mtu -
		    (isipv6 ?
		     sizeof(struct ip6_hdr) + sizeof(struct tcphdr) :
		     sizeof(struct tcpiphdr));

		/*
		 * XXX - The following conditional probably violates the TCP
		 * spec.  The problem is that, since we don't know the
		 * other end's MSS, we are supposed to use a conservative
		 * default.  But, if we do that, then MTU discovery will
		 * never actually take place, because the conservative
		 * default is much less than the MTUs typically seen
		 * on the Internet today.  For the moment, we'll sweep
		 * this under the carpet.
		 *
		 * The conservative default might not actually be a problem
		 * if the only case this occurs is when sending an initial
		 * SYN with options and data to a host we've never talked
		 * to before.  Then, they will reply with an MSS value which
		 * will get recorded and the new parameters should get
		 * recomputed.  For Further Study.
		 */
		if (taop->tao_mssopt != 0 && taop->tao_mssopt < maxopd)
			maxopd = taop->tao_mssopt;
	} else
		maxopd = mtu -
		    (isipv6 ?
		     sizeof(struct ip6_hdr) + sizeof(struct tcphdr) :
		     sizeof(struct tcpiphdr));

	if (tp->t_maxopd <= maxopd)
		return;
	tp->t_maxopd = maxopd;

	mss = maxopd;
	if ((tp->t_flags & (TF_REQ_TSTMP | TF_RCVD_TSTMP | TF_NOOPT)) ==
			   (TF_REQ_TSTMP | TF_RCVD_TSTMP))
		mss -= TCPOLEN_TSTAMP_APPA;

	if ((tp->t_flags & (TF_REQ_CC | TF_RCVD_CC | TF_NOOPT)) ==
			   (TF_REQ_CC | TF_RCVD_CC))
		mss -= TCPOLEN_CC_APPA;

	/* round down to multiple of MCLBYTES */
#if	(MCLBYTES & (MCLBYTES - 1)) == 0    /* test if MCLBYTES power of 2 */
	if (mss > MCLBYTES)
		mss &= ~(MCLBYTES - 1);	
#else
	if (mss > MCLBYTES)
		mss = (mss / MCLBYTES) * MCLBYTES;
#endif

	if (so->so_snd.sb_hiwat < mss)
		mss = so->so_snd.sb_hiwat;

	tp->t_maxseg = mss;
	tp->t_rtttime = 0;
	tp->snd_nxt = tp->snd_una;
	tcp_output(tp);
	tcpstat.tcps_mturesent++;
}

/*
 * Look-up the routing entry to the peer of this inpcb.  If no route
 * is found and it cannot be allocated the return NULL.  This routine
 * is called by TCP routines that access the rmx structure and by tcp_mss
 * to get the interface MTU.
 */
struct rtentry *
tcp_rtlookup(struct in_conninfo *inc)
{
	struct route *ro = &inc->inc_route;

	if (ro->ro_rt == NULL || !(ro->ro_rt->rt_flags & RTF_UP)) {
		/* No route yet, so try to acquire one */
		if (inc->inc_faddr.s_addr != INADDR_ANY) {
			/*
			 * unused portions of the structure MUST be zero'd
			 * out because rtalloc() treats it as opaque data
			 */
			bzero(&ro->ro_dst, sizeof(struct sockaddr_in));
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
			    inc->inc_faddr;
			rtalloc(ro);
		}
	}
	return (ro->ro_rt);
}

#ifdef INET6
struct rtentry *
tcp_rtlookup6(struct in_conninfo *inc)
{
	struct route_in6 *ro6 = &inc->inc6_route;

	if (ro6->ro_rt == NULL || !(ro6->ro_rt->rt_flags & RTF_UP)) {
		/* No route yet, so try to acquire one */
		if (!IN6_IS_ADDR_UNSPECIFIED(&inc->inc6_faddr)) {
			/*
			 * unused portions of the structure MUST be zero'd
			 * out because rtalloc() treats it as opaque data
			 */
			bzero(&ro6->ro_dst, sizeof(struct sockaddr_in6));
			ro6->ro_dst.sin6_family = AF_INET6;
			ro6->ro_dst.sin6_len = sizeof(struct sockaddr_in6);
			ro6->ro_dst.sin6_addr = inc->inc6_faddr;
			rtalloc((struct route *)ro6);
		}
	}
	return (ro6->ro_rt);
}
#endif

#ifdef IPSEC
/* compute ESP/AH header size for TCP, including outer IP header. */
size_t
ipsec_hdrsiz_tcp(struct tcpcb *tp)
{
	struct inpcb *inp;
	struct mbuf *m;
	size_t hdrsiz;
	struct ip *ip;
	struct tcphdr *th;

	if ((tp == NULL) || ((inp = tp->t_inpcb) == NULL))
		return (0);
	MGETHDR(m, MB_DONTWAIT, MT_DATA);
	if (!m)
		return (0);

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

		th = (struct tcphdr *)(ip6 + 1);
		m->m_pkthdr.len = m->m_len =
		    sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
		tcp_fillheaders(tp, ip6, th);
		hdrsiz = ipsec6_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	} else
#endif
	{
		ip = mtod(m, struct ip *);
		th = (struct tcphdr *)(ip + 1);
		m->m_pkthdr.len = m->m_len = sizeof(struct tcpiphdr);
		tcp_fillheaders(tp, ip, th);
		hdrsiz = ipsec4_hdrsiz(m, IPSEC_DIR_OUTBOUND, inp);
	}

	m_free(m);
	return (hdrsiz);
}
#endif

/*
 * Return a pointer to the cached information about the remote host.
 * The cached information is stored in the protocol specific part of
 * the route metrics.
 */
struct rmxp_tao *
tcp_gettaocache(struct in_conninfo *inc)
{
	struct rtentry *rt;

#ifdef INET6
	if (inc->inc_isipv6)
		rt = tcp_rtlookup6(inc);
	else
#endif
		rt = tcp_rtlookup(inc);

	/* Make sure this is a host route and is up. */
	if (rt == NULL ||
	    (rt->rt_flags & (RTF_UP | RTF_HOST)) != (RTF_UP | RTF_HOST))
		return (NULL);

	return (rmx_taop(rt->rt_rmx));
}

/*
 * Clear all the TAO cache entries, called from tcp_init.
 *
 * XXX
 * This routine is just an empty one, because we assume that the routing
 * routing tables are initialized at the same time when TCP, so there is
 * nothing in the cache left over.
 */
static void
tcp_cleartaocache()
{
}

/*
 * TCP BANDWIDTH DELAY PRODUCT WINDOW LIMITING
 *
 * This code attempts to calculate the bandwidth-delay product as a
 * means of determining the optimal window size to maximize bandwidth,
 * minimize RTT, and avoid the over-allocation of buffers on interfaces and
 * routers.  This code also does a fairly good job keeping RTTs in check
 * across slow links like modems.  We implement an algorithm which is very
 * similar (but not meant to be) TCP/Vegas.  The code operates on the
 * transmitter side of a TCP connection and so only effects the transmit
 * side of the connection.
 *
 * BACKGROUND:  TCP makes no provision for the management of buffer space
 * at the end points or at the intermediate routers and switches.  A TCP
 * stream, whether using NewReno or not, will eventually buffer as
 * many packets as it is able and the only reason this typically works is
 * due to the fairly small default buffers made available for a connection
 * (typicaly 16K or 32K).  As machines use larger windows and/or window
 * scaling it is now fairly easy for even a single TCP connection to blow-out
 * all available buffer space not only on the local interface, but on
 * intermediate routers and switches as well.  NewReno makes a misguided
 * attempt to 'solve' this problem by waiting for an actual failure to occur,
 * then backing off, then steadily increasing the window again until another
 * failure occurs, ad-infinitum.  This results in terrible oscillation that
 * is only made worse as network loads increase and the idea of intentionally
 * blowing out network buffers is, frankly, a terrible way to manage network
 * resources.
 *
 * It is far better to limit the transmit window prior to the failure
 * condition being achieved.  There are two general ways to do this:  First
 * you can 'scan' through different transmit window sizes and locate the
 * point where the RTT stops increasing, indicating that you have filled the
 * pipe, then scan backwards until you note that RTT stops decreasing, then
 * repeat ad-infinitum.  This method works in principle but has severe
 * implementation issues due to RTT variances, timer granularity, and
 * instability in the algorithm which can lead to many false positives and
 * create oscillations as well as interact badly with other TCP streams
 * implementing the same algorithm.
 *
 * The second method is to limit the window to the bandwidth delay product
 * of the link.  This is the method we implement.  RTT variances and our
 * own manipulation of the congestion window, bwnd, can potentially
 * destabilize the algorithm.  For this reason we have to stabilize the
 * elements used to calculate the window.  We do this by using the minimum
 * observed RTT, the long term average of the observed bandwidth, and
 * by adding two segments worth of slop.  It isn't perfect but it is able
 * to react to changing conditions and gives us a very stable basis on
 * which to extend the algorithm.
 */
void
tcp_xmit_bandwidth_limit(struct tcpcb *tp, tcp_seq ack_seq)
{
	u_long bw;
	u_long bwnd;
	int save_ticks;
	int delta_ticks;

	/*
	 * If inflight_enable is disabled in the middle of a tcp connection,
	 * make sure snd_bwnd is effectively disabled.
	 */
	if (!tcp_inflight_enable) {
		tp->snd_bwnd = TCP_MAXWIN << TCP_MAX_WINSHIFT;
		tp->snd_bandwidth = 0;
		return;
	}

	/*
	 * Validate the delta time.  If a connection is new or has been idle
	 * a long time we have to reset the bandwidth calculator.
	 */
	save_ticks = ticks;
	delta_ticks = save_ticks - tp->t_bw_rtttime;
	if (tp->t_bw_rtttime == 0 || delta_ticks < 0 || delta_ticks > hz * 10) {
		tp->t_bw_rtttime = ticks;
		tp->t_bw_rtseq = ack_seq;
		if (tp->snd_bandwidth == 0)
			tp->snd_bandwidth = tcp_inflight_min;
		return;
	}
	if (delta_ticks == 0)
		return;

	/*
	 * Sanity check, plus ignore pure window update acks.
	 */
	if ((int)(ack_seq - tp->t_bw_rtseq) <= 0)
		return;

	/*
	 * Figure out the bandwidth.  Due to the tick granularity this
	 * is a very rough number and it MUST be averaged over a fairly
	 * long period of time.  XXX we need to take into account a link
	 * that is not using all available bandwidth, but for now our
	 * slop will ramp us up if this case occurs and the bandwidth later
	 * increases.
	 */
	bw = (int64_t)(ack_seq - tp->t_bw_rtseq) * hz / delta_ticks;
	tp->t_bw_rtttime = save_ticks;
	tp->t_bw_rtseq = ack_seq;
	bw = ((int64_t)tp->snd_bandwidth * 15 + bw) >> 4;

	tp->snd_bandwidth = bw;

	/*
	 * Calculate the semi-static bandwidth delay product, plus two maximal
	 * segments.  The additional slop puts us squarely in the sweet
	 * spot and also handles the bandwidth run-up case.  Without the
	 * slop we could be locking ourselves into a lower bandwidth.
	 *
	 * Situations Handled:
	 *	(1) Prevents over-queueing of packets on LANs, especially on
	 *	    high speed LANs, allowing larger TCP buffers to be
	 *	    specified, and also does a good job preventing
	 *	    over-queueing of packets over choke points like modems
	 *	    (at least for the transmit side).
	 *
	 *	(2) Is able to handle changing network loads (bandwidth
	 *	    drops so bwnd drops, bandwidth increases so bwnd
	 *	    increases).
	 *
	 *	(3) Theoretically should stabilize in the face of multiple
	 *	    connections implementing the same algorithm (this may need
	 *	    a little work).
	 *
	 *	(4) Stability value (defaults to 20 = 2 maximal packets) can
	 *	    be adjusted with a sysctl but typically only needs to be on
	 *	    very slow connections.  A value no smaller then 5 should
	 *	    be used, but only reduce this default if you have no other
	 *	    choice.
	 */

#define	USERTT	((tp->t_srtt + tp->t_rttbest) / 2)
	bwnd = (int64_t)bw * USERTT / (hz << TCP_RTT_SHIFT) +
	       tcp_inflight_stab * (int)tp->t_maxseg / 10;
#undef USERTT

	if (tcp_inflight_debug > 0) {
		static int ltime;
		if ((u_int)(ticks - ltime) >= hz / tcp_inflight_debug) {
			ltime = ticks;
			printf("%p bw %ld rttbest %d srtt %d bwnd %ld\n",
				tp, bw, tp->t_rttbest, tp->t_srtt, bwnd);
		}
	}
	if ((long)bwnd < tcp_inflight_min)
		bwnd = tcp_inflight_min;
	if (bwnd > tcp_inflight_max)
		bwnd = tcp_inflight_max;
	if ((long)bwnd < tp->t_maxseg * 2)
		bwnd = tp->t_maxseg * 2;
	tp->snd_bwnd = bwnd;
}
