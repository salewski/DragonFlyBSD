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
 *	@(#)in_proto.c	8.2 (Berkeley) 2/9/95
 * $FreeBSD: src/sys/netinet/in_proto.c,v 1.53.2.7 2003/08/24 08:24:38 hsu Exp $
 * $DragonFly: src/sys/netinet/in_proto.c,v 1.12 2005/03/04 02:21:48 hsu Exp $
 */

#include "opt_ipdivert.h"
#include "opt_ipx.h"
#include "opt_mrouting.h"
#include "opt_ipsec.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp_var.h>
#ifdef PIM
#include <netinet/pim_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_encap.h>


/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ah.h>
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#endif
#include <netinet6/ipcomp.h>
#endif /* IPSEC */

#ifdef FAST_IPSEC
#include <netproto/ipsec/ipsec.h>
#endif /* FAST_IPSEC */

#ifdef IPXIP
#include <netproto/ipx/ipx_ip.h>
#endif

#ifdef NSIP
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <net/netisr.h>		/* for cpu0_soport */

extern	struct domain inetdomain;
static	struct pr_usrreqs nousrreqs;

struct protosw inetsw[] = {
{ 0,		&inetdomain,	0,		0,
  0,		0,		0,		0,
  cpu0_soport,
  ip_init,	0,		ip_slowtimo,	ip_drain,
  &nousrreqs
},
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp_input,	0,		udp_ctlinput,	ip_ctloutput,
  udp_soport,
  udp_init,	0,		0,		0,
  &udp_usrreqs
},
{ SOCK_STREAM,	&inetdomain,	IPPROTO_TCP,
	PR_CONNREQUIRED|PR_IMPLOPCL|PR_WANTRCVD,
  tcp_input,	0,		tcp_ctlinput,	tcp_ctloutput,
  tcp_soport,
  tcp_init,	0,		tcp_slowtimo,	tcp_drain,
  &tcp_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip_input,	0,		rip_ctlinput,	rip_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ICMP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  icmp_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_IGMP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  igmp_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  igmp_init,	igmp_fasttimo,	igmp_slowtimo,	0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RSVP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  rsvp_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
#ifdef IPSEC
{ SOCK_RAW,	&inetdomain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ah4_input,	0,		0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
#ifdef IPSEC_ESP
{ SOCK_RAW,	&inetdomain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp4_input,	0,		0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
#endif
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp4_input, 0,		0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
#endif /* IPSEC */
#ifdef FAST_IPSEC
{ SOCK_RAW,	&inetdomain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ipsec4_common_input,	0,	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  ipsec4_common_input,	0,	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipsec4_common_input,	0,	0,		0,
  cpu0_soport,
  0,		0,		0,		0,
  &nousrreqs
},
#endif /* FAST_IPSEC */
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_MOBILE,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip_usrreqs
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_GRE,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip_usrreqs
},
# ifdef INET6
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  encap4_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  encap_init,	0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef IPDIVERT
{ SOCK_RAW,	&inetdomain,	IPPROTO_DIVERT,	PR_ATOMIC|PR_ADDR,
  div_input,	0,		0,		ip_ctloutput,
  cpu0_soport,
  div_init,	0,		0,		0,
  &div_usrreqs,
},
#endif
#ifdef IPXIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  ipxip_input,	0,		ipxip_ctlinput,	0,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef NSIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  idpip_input,	0,		nsip_ctlinput,	0,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef PIM
{ SOCK_RAW,	&inetdomain,	IPPROTO_PIM,	PR_ATOMIC|PR_ADDR|PR_LASTHDR,
  pim_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#ifdef NPFSYNC
{ SOCK_RAW,	&inetdomain,	IPPROTO_PFSYNC,	PR_ATOMIC|PR_ADDR,
  pfsync_input,	0,		0,		rip_ctloutput,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif	/* NPFSYNC */
	/* raw wildcard */
{ SOCK_RAW,	&inetdomain,	0,		PR_ATOMIC|PR_ADDR,
  rip_input,	0,		0,		rip_ctloutput,
  cpu0_soport,
  rip_init,	0,		0,		0,
  &rip_usrreqs
},
};

struct domain inetdomain = {
	AF_INET, "internet", NULL, NULL, NULL,
	inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])],
	SLIST_ENTRY_INITIALIZER,
	in_inithead, 32, sizeof(struct sockaddr_in),
};

DOMAIN_SET(inet);

SYSCTL_NODE(_net,      PF_INET,		inet,	CTLFLAG_RW, 0,
	"Internet Family");

SYSCTL_NODE(_net_inet, IPPROTO_IP,	ip,	CTLFLAG_RW, 0,	"IP");
SYSCTL_NODE(_net_inet, IPPROTO_ICMP,	icmp,	CTLFLAG_RW, 0,	"ICMP");
SYSCTL_NODE(_net_inet, IPPROTO_UDP,	udp,	CTLFLAG_RW, 0,	"UDP");
SYSCTL_NODE(_net_inet, IPPROTO_TCP,	tcp,	CTLFLAG_RW, 0,	"TCP");
SYSCTL_NODE(_net_inet, IPPROTO_IGMP,	igmp,	CTLFLAG_RW, 0,	"IGMP");
#ifdef FAST_IPSEC
/* XXX no protocol # to use, pick something "reserved" */
SYSCTL_NODE(_net_inet, 253,		ipsec,	CTLFLAG_RW, 0,	"IPSEC");
SYSCTL_NODE(_net_inet, IPPROTO_AH,	ah,	CTLFLAG_RW, 0,	"AH");
SYSCTL_NODE(_net_inet, IPPROTO_ESP,	esp,	CTLFLAG_RW, 0,	"ESP");
SYSCTL_NODE(_net_inet, IPPROTO_IPCOMP,	ipcomp,	CTLFLAG_RW, 0,	"IPCOMP");
SYSCTL_NODE(_net_inet, IPPROTO_IPIP,	ipip,	CTLFLAG_RW, 0,	"IPIP");
#else
#ifdef IPSEC
SYSCTL_NODE(_net_inet, IPPROTO_AH,	ipsec,	CTLFLAG_RW, 0,	"IPSEC");
#endif /* IPSEC */
#endif /* !FAST_IPSEC */
SYSCTL_NODE(_net_inet, IPPROTO_RAW,	raw,	CTLFLAG_RW, 0,	"RAW");
#ifdef IPDIVERT
SYSCTL_NODE(_net_inet, IPPROTO_DIVERT,	divert,	CTLFLAG_RW, 0,	"DIVERT");
#endif
#ifdef PIM
SYSCTL_NODE(_net_inet, IPPROTO_PIM,    pim,    CTLFLAG_RW, 0,  "PIM");
#endif
