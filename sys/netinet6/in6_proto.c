/*	$FreeBSD: src/sys/netinet6/in6_proto.c,v 1.6.2.9 2003/01/24 05:11:35 sam Exp $	*/
/*	$DragonFly: src/sys/netinet6/in6_proto.c,v 1.9 2005/03/04 02:21:49 hsu Exp $	*/
/*	$KAME: in6_proto.c,v 1.91 2001/05/27 13:28:35 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)in_proto.c	8.1 (Berkeley) 6/10/93
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip_encap.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet6/tcp6_var.h>
#include <netinet6/raw_ip6.h>
#include <netinet6/udp6_var.h>
#include <netinet6/pim6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_prefix.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netinet6/ah.h>
#ifdef INET6
#include <netinet6/ah6.h>
#endif
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#ifdef INET6
#include <netinet6/esp6.h>
#endif
#endif
#include <netinet6/ipcomp.h>
#ifdef INET6
#include <netinet6/ipcomp6.h>
#endif
#endif /* IPSEC */

#ifdef FAST_IPSEC
#include <netproto/ipsec/ipsec6.h>
#define	IPSEC
#define	IPSEC_ESP
#define	ah6_input	ipsec6_common_input
#define	esp6_input	ipsec6_common_input
#define	ipcomp6_input	ipsec6_common_input
#endif /* FAST_IPSEC */

#include <netinet6/ip6protosw.h>

#include <net/net_osdep.h>

/*
 * TCP/IP protocol family: IP6, ICMP6, UDP, TCP.
 */

extern	struct domain inet6domain;
static struct pr_usrreqs nousrreqs;

#define PR_LISTEN	0
#define PR_ABRTACPTDIS	0

struct ip6protosw inet6sw[] = {
{ 0,		&inet6domain,	IPPROTO_IPV6,	0,
  0,		0,		0,		0,
  cpu0_soport,
  ip6_init,	0,		frag6_slowtimo,	frag6_drain,
  &nousrreqs,
},
{ SOCK_DGRAM,	&inet6domain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp6_input,	0,		udp6_ctlinput,	ip6_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &udp6_usrreqs,
},
{ SOCK_STREAM,	&inet6domain,	IPPROTO_TCP,	PR_CONNREQUIRED|PR_WANTRCVD|PR_LISTEN,
  tcp6_input,	0,		tcp6_ctlinput,	tcp_ctloutput,
  cpu0_soport,
#ifdef INET	/* don't call initialization and timeout routines twice */
  0,		0,		0,		tcp_drain,
#else
  tcp_init,	tcp_fasttimo,	tcp_slowtimo,	tcp_drain,
#endif
  &tcp6_usrreqs,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip6_input,	rip6_output,	rip6_ctlinput,	rip6_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip6_usrreqs
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ICMPV6,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  icmp6_input,	rip6_output,	rip6_ctlinput,	rip6_ctloutput,
  cpu0_soport,
  icmp6_init,	icmp6_fasttimo,	0,		0,
  &rip6_usrreqs
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_DSTOPTS, PR_ATOMIC|PR_ADDR,
  dest6_input,	0,	 	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ROUTING, PR_ATOMIC|PR_ADDR,
  route6_input,	0,	 	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_FRAGMENT, PR_ATOMIC|PR_ADDR,
  frag6_input,	0,	 	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
#ifdef IPSEC
{ SOCK_RAW,	&inet6domain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ah6_input,	0,		0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs,
},
#ifdef IPSEC_ESP
{ SOCK_RAW,	&inet6domain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp6_input,	0,
  esp6_ctlinput,
  0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs,
},
#endif
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp6_input, 0,	 	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs,
},
#endif /* IPSEC */
#ifdef INET
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap6_input,	rip6_output, 	0,		rip6_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip6_usrreqs
},
#endif /* INET */
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap6_input, rip6_output,	0,		rip6_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip6_usrreqs
},
{ SOCK_RAW,     &inet6domain,	IPPROTO_PIM,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  pim6_input,	rip6_output,	0,              rip6_ctloutput,
  cpu0_soport,
  0,            0,              0,              0,
  &rip6_usrreqs
},
/* raw wildcard */
{ SOCK_RAW,	&inet6domain,	0,		PR_ATOMIC|PR_ADDR,
  rip6_input,	rip6_output,	0,		rip6_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip6_usrreqs
},
};

extern int in6_inithead (void **, int);

struct domain inet6domain = {
	AF_INET6, "internet6", NULL, NULL, NULL,
	(struct protosw *)inet6sw,
	(struct protosw *)&inet6sw[sizeof(inet6sw)/sizeof(inet6sw[0])],
	SLIST_ENTRY_INITIALIZER,
	in6_inithead, offsetof(struct sockaddr_in6, sin6_addr) << 3,
	sizeof(struct sockaddr_in6), in6_domifattach, in6_domifdetach
};

DOMAIN_SET(inet6);

/*
 * Internet configuration info
 */
#ifndef	IPV6FORWARDING
#ifdef GATEWAY6
#define	IPV6FORWARDING	1	/* forward IP6 packets not for us */
#else
#define	IPV6FORWARDING	0	/* don't forward IP6 packets not for us */
#endif /* GATEWAY6 */
#endif /* !IPV6FORWARDING */

#ifndef	IPV6_SENDREDIRECTS
#define	IPV6_SENDREDIRECTS	1
#endif

int	ip6_forwarding = IPV6FORWARDING;	/* act as router? */
int	ip6_sendredirects = IPV6_SENDREDIRECTS;
int	ip6_defhlim = IPV6_DEFHLIM;
int	ip6_defmcasthlim = IPV6_DEFAULT_MULTICAST_HOPS;
int	ip6_accept_rtadv = 0;	/* "IPV6FORWARDING ? 0 : 1" is dangerous */
int	ip6_maxfragpackets;	/* initialized in frag6.c:frag6_init() */
int	ip6_log_interval = 5;
int	ip6_hdrnestlimit = 50;	/* appropriate? */
int	ip6_dad_count = 1;	/* DupAddrDetectionTransmits */
u_int32_t ip6_flow_seq;
int	ip6_auto_flowlabel = 1;
int	ip6_gif_hlim = 0;
int	ip6_use_deprecated = 1;	/* allow deprecated addr (RFC2462 5.5.4) */
int	ip6_rr_prune = 5;	/* router renumbering prefix
				 * walk list every 5 sec.    */
int	ip6_v6only = 1;

u_int32_t ip6_id = 0UL;
int	ip6_keepfaith = 0;
time_t	ip6_log_time = (time_t)0L;

/* icmp6 */
/*
 * BSDI4 defines these variables in in_proto.c...
 * XXX: what if we don't define INET? Should we define pmtu6_expire
 * or so? (jinmei@kame.net 19990310)
 */
int pmtu_expire = 60*10;
int pmtu_probe = 60*2;

/* raw IP6 parameters */
/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPV6SNDQ	8192
#define	RIPV6RCVQ	8192

u_long	rip6_sendspace = RIPV6SNDQ;
u_long	rip6_recvspace = RIPV6RCVQ;

/* ICMPV6 parameters */
int	icmp6_rediraccept = 1;		/* accept and process redirects */
int	icmp6_redirtimeout = 10 * 60;	/* 10 minutes */
int	icmp6errppslim = 100;		/* 100pps */
int	icmp6_nodeinfo = 3;		/* enable/disable NI response */

/* UDP on IP6 parameters */
int	udp6_sendspace = 9216;		/* really max datagram size */
int	udp6_recvspace = 40 * (1024 + sizeof(struct sockaddr_in6));
					/* 40 1K datagrams */

/*
 * sysctl related items.
 */
SYSCTL_NODE(_net,	PF_INET6,	inet6,	CTLFLAG_RW,	0,
	"Internet6 Family");

/* net.inet6 */
SYSCTL_NODE(_net_inet6,	IPPROTO_IPV6,	ip6,	CTLFLAG_RW, 0,	"IP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_ICMPV6,	icmp6,	CTLFLAG_RW, 0,	"ICMP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_UDP,	udp6,	CTLFLAG_RW, 0,	"UDP6");
SYSCTL_NODE(_net_inet6,	IPPROTO_TCP,	tcp6,	CTLFLAG_RW, 0,	"TCP6");
#ifdef IPSEC
SYSCTL_NODE(_net_inet6,	IPPROTO_ESP,	ipsec6,	CTLFLAG_RW, 0,	"IPSEC6");
#endif /* IPSEC */

/* net.inet6.ip6 */
static int
sysctl_ip6_temppltime(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	int old;

	error = SYSCTL_OUT(req, arg1, sizeof(int));
	if (error || !req->newptr)
		return (error);
	old = ip6_temp_preferred_lifetime;
	error = SYSCTL_IN(req, arg1, sizeof(int));
	if (ip6_temp_preferred_lifetime <
	    ip6_desync_factor + ip6_temp_regen_advance) {
		ip6_temp_preferred_lifetime = old;
		return(EINVAL);
	}
	return(error);
}

static int
sysctl_ip6_tempvltime(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	int old;

	error = SYSCTL_OUT(req, arg1, sizeof(int));
	if (error || !req->newptr)
		return (error);
	old = ip6_temp_valid_lifetime;
	error = SYSCTL_IN(req, arg1, sizeof(int));
	if (ip6_temp_valid_lifetime < ip6_temp_preferred_lifetime) {
		ip6_temp_preferred_lifetime = old;
		return(EINVAL);
	}
	return(error);
}

SYSCTL_INT(_net_inet6_ip6, IPV6CTL_FORWARDING,
	forwarding, CTLFLAG_RW, 	&ip6_forwarding,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_SENDREDIRECTS,
	redirect, CTLFLAG_RW,		&ip6_sendredirects,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DEFHLIM,
	hlim, CTLFLAG_RW,		&ip6_defhlim,	0, "");
SYSCTL_STRUCT(_net_inet6_ip6, IPV6CTL_STATS, stats, CTLFLAG_RD,
	&ip6stat, ip6stat, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_MAXFRAGPACKETS,
	maxfragpackets, CTLFLAG_RW,	&ip6_maxfragpackets,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_ACCEPT_RTADV,
	accept_rtadv, CTLFLAG_RW,	&ip6_accept_rtadv,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_KEEPFAITH,
	keepfaith, CTLFLAG_RW,		&ip6_keepfaith,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_LOG_INTERVAL,
	log_interval, CTLFLAG_RW,	&ip6_log_interval,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_HDRNESTLIMIT,
	hdrnestlimit, CTLFLAG_RW,	&ip6_hdrnestlimit,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DAD_COUNT,
	dad_count, CTLFLAG_RW,	&ip6_dad_count,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_AUTO_FLOWLABEL,
	auto_flowlabel, CTLFLAG_RW,	&ip6_auto_flowlabel,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_DEFMCASTHLIM,
	defmcasthlim, CTLFLAG_RW,	&ip6_defmcasthlim,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_GIF_HLIM,
	gifhlim, CTLFLAG_RW,	&ip6_gif_hlim,			0, "");
SYSCTL_STRING(_net_inet6_ip6, IPV6CTL_KAME_VERSION,
	kame_version, CTLFLAG_RD,	__KAME_VERSION,		0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_USE_DEPRECATED,
	use_deprecated, CTLFLAG_RW,	&ip6_use_deprecated,	0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_RR_PRUNE,
	rr_prune, CTLFLAG_RW,	&ip6_rr_prune,			0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_USETEMPADDR,
	use_tempaddr, CTLFLAG_RW, &ip6_use_tempaddr,		0, "");
SYSCTL_OID(_net_inet6_ip6, IPV6CTL_TEMPPLTIME, temppltime,
	   CTLTYPE_INT|CTLFLAG_RW, &ip6_temp_preferred_lifetime, 0,
	   sysctl_ip6_temppltime, "I", "");
SYSCTL_OID(_net_inet6_ip6, IPV6CTL_TEMPVLTIME, tempvltime,
	   CTLTYPE_INT|CTLFLAG_RW, &ip6_temp_valid_lifetime, 0,
	   sysctl_ip6_tempvltime, "I", "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_V6ONLY,
	v6only,	CTLFLAG_RW,	&ip6_v6only,			0, "");
SYSCTL_INT(_net_inet6_ip6, IPV6CTL_AUTO_LINKLOCAL,
	auto_linklocal, CTLFLAG_RW, &ip6_auto_linklocal,	0, "");
SYSCTL_STRUCT(_net_inet6_ip6, IPV6CTL_RIP6STATS, rip6stats, CTLFLAG_RD,
	&rip6stat, rip6stat, "");

/* net.inet6.icmp6 */
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_REDIRACCEPT,
	rediraccept, CTLFLAG_RW,	&icmp6_rediraccept,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_REDIRTIMEOUT,
	redirtimeout, CTLFLAG_RW,	&icmp6_redirtimeout,	0, "");
SYSCTL_STRUCT(_net_inet6_icmp6, ICMPV6CTL_STATS, stats, CTLFLAG_RD,
	&icmp6stat, icmp6stat, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_PRUNE,
	nd6_prune, CTLFLAG_RW,		&nd6_prune,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_DELAY,
	nd6_delay, CTLFLAG_RW,		&nd6_delay,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_UMAXTRIES,
	nd6_umaxtries, CTLFLAG_RW,	&nd6_umaxtries,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_MMAXTRIES,
	nd6_mmaxtries, CTLFLAG_RW,	&nd6_mmaxtries,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_USELOOPBACK,
	nd6_useloopback, CTLFLAG_RW,	&nd6_useloopback, 0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_NODEINFO,
	nodeinfo, CTLFLAG_RW,	&icmp6_nodeinfo,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ERRPPSLIMIT,
	errppslimit, CTLFLAG_RW,	&icmp6errppslim,	0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_MAXNUDHINT,
	nd6_maxnudhint, CTLFLAG_RW,	&nd6_maxnudhint, 0, "");
SYSCTL_INT(_net_inet6_icmp6, ICMPV6CTL_ND6_DEBUG,
	nd6_debug, CTLFLAG_RW,	&nd6_debug,		0, "");
