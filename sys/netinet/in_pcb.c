/*
 * cOPyright (c) 2004 Jeffrey M. Hsu.  All rights reserved.
 * Copyright (c) 2004 The DragonFly Project.  All rights reserved.
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
 * Copyright (c) 2004 Jeffrey M. Hsu.  All rights reserved.
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
 * Copyright (c) 1982, 1986, 1991, 1993, 1995
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
 *	@(#)in_pcb.c	8.4 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/in_pcb.c,v 1.59.2.27 2004/01/02 04:06:42 ambrisko Exp $
 * $DragonFly: src/sys/netinet/in_pcb.c,v 1.34.2.1 2005/05/23 18:34:49 dillon Exp $
 */

#include "opt_ipsec.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/limits.h>

#include <vm/vm_zone.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netproto/key/key.h>
#endif

#ifdef FAST_IPSEC
#if defined(IPSEC) || defined(IPSEC_ESP)
#error "Bad idea: don't compile with both IPSEC and FAST_IPSEC!"
#endif

#include <netproto/ipsec/ipsec.h>
#include <netproto/ipsec/key.h>
#define	IPSEC
#endif /* FAST_IPSEC */

struct in_addr zeroin_addr;

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
int ipport_lowfirstauto = IPPORT_RESERVED - 1;	/* 1023 */
int ipport_lowlastauto = IPPORT_RESERVEDSTART;	/* 600 */

int ipport_firstauto = IPPORT_RESERVED;		/* 1024 */
int ipport_lastauto = IPPORT_USERRESERVED;	/* 5000 */

int ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 49152 */
int ipport_hilastauto = IPPORT_HILASTAUTO;	/* 65535 */

static __inline void
RANGECHK(int var, int min, int max)
{
	if (var < min)
		var = min;
	else if (var > max)
		var = max;
}

static int
sysctl_net_ipport_check(SYSCTL_HANDLER_ARGS)
{
	int error;

	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error) {
		RANGECHK(ipport_lowfirstauto, 1, IPPORT_RESERVED - 1);
		RANGECHK(ipport_lowlastauto, 1, IPPORT_RESERVED - 1);

		RANGECHK(ipport_firstauto, IPPORT_RESERVED, USHRT_MAX);
		RANGECHK(ipport_lastauto, IPPORT_RESERVED, USHRT_MAX);

		RANGECHK(ipport_hifirstauto, IPPORT_RESERVED, USHRT_MAX);
		RANGECHK(ipport_hilastauto, IPPORT_RESERVED, USHRT_MAX);
	}
	return (error);
}

SYSCTL_NODE(_net_inet_ip, IPPROTO_IP, portrange, CTLFLAG_RW, 0, "IP Ports");

SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowfirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowfirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, lowlast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lowlastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, first, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_firstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, last, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_lastauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hifirst, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hifirstauto, 0, &sysctl_net_ipport_check, "I", "");
SYSCTL_PROC(_net_inet_ip_portrange, OID_AUTO, hilast, CTLTYPE_INT|CTLFLAG_RW,
	   &ipport_hilastauto, 0, &sysctl_net_ipport_check, "I", "");

/*
 * in_pcb.c: manage the Protocol Control Blocks.
 *
 * NOTE: It is assumed that most of these functions will be called at
 * splnet(). XXX - There are, unfortunately, a few exceptions to this
 * rule that should be fixed.
 *
 * NOTE: The caller should initialize the cpu field to the cpu running the
 * protocol stack associated with this inpcbinfo.
 */

void
in_pcbinfo_init(struct inpcbinfo *pcbinfo)
{
	LIST_INIT(&pcbinfo->pcblisthead);
	pcbinfo->cpu = -1;
}

/*
 * Allocate a PCB and associate it with the socket.
 */
int
in_pcballoc(struct socket *so, struct inpcbinfo *pcbinfo)
{
	struct inpcb *inp;
#ifdef IPSEC
	int error;
#endif

	inp = zalloc(pcbinfo->ipi_zone);
	if (inp == NULL)
		return (ENOBUFS);
	bzero(inp, sizeof *inp);
	inp->inp_gencnt = ++pcbinfo->ipi_gencnt;
	inp->inp_pcbinfo = inp->inp_cpcbinfo = pcbinfo;
	inp->inp_socket = so;
#ifdef IPSEC
	error = ipsec_init_policy(so, &inp->inp_sp);
	if (error != 0) {
		zfree(pcbinfo->ipi_zone, inp);
		return (error);
	}
#endif
#ifdef INET6
	if (INP_SOCKAF(so) == AF_INET6 && ip6_v6only)
		inp->inp_flags |= IN6P_IPV6_V6ONLY;
	if (ip6_auto_flowlabel)
		inp->inp_flags |= IN6P_AUTOFLOWLABEL;
#endif
	so->so_pcb = inp;
	LIST_INSERT_HEAD(&pcbinfo->pcblisthead, inp, inp_list);
	pcbinfo->ipi_count++;
	return (0);
}

int
in_pcbbind(struct inpcb *inp, struct sockaddr *nam, struct thread *td)
{
	struct socket *so = inp->inp_socket;
	struct proc *p = td->td_proc;
	unsigned short *lastport;
	struct sockaddr_in *sin;
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	u_short lport = 0;
	int wild = 0, reuseport = (so->so_options & SO_REUSEPORT);
	int error, prison = 0;

	KKASSERT(p);

	if (TAILQ_EMPTY(&in_ifaddrhead)) /* XXX broken! */
		return (EADDRNOTAVAIL);
	if (inp->inp_lport != 0 || inp->inp_laddr.s_addr != INADDR_ANY)
		return (EINVAL);	/* already bound */
	if (!(so->so_options & (SO_REUSEADDR|SO_REUSEPORT)))
		wild = 1;    /* neither SO_REUSEADDR nor SO_REUSEPORT is set */
	if (nam != NULL) {
		sin = (struct sockaddr_in *)nam;
		if (nam->sa_len != sizeof *sin)
			return (EINVAL);
#ifdef notdef
		/*
		 * We should check the family, but old programs
		 * incorrectly fail to initialize it.
		 */
		if (sin->sin_family != AF_INET)
			return (EAFNOSUPPORT);
#endif
		if (sin->sin_addr.s_addr != INADDR_ANY &&
		    prison_ip(td, 0, &sin->sin_addr.s_addr))
				return (EINVAL);
		lport = sin->sin_port;
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
			/*
			 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
			 * allow complete duplication of binding if
			 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
			 * and a multicast address is bound on both
			 * new and duplicated sockets.
			 */
			if (so->so_options & SO_REUSEADDR)
				reuseport = SO_REUSEADDR | SO_REUSEPORT;
		} else if (sin->sin_addr.s_addr != INADDR_ANY) {
			sin->sin_port = 0;		/* yech... */
			bzero(&sin->sin_zero, sizeof sin->sin_zero);
			if (ifa_ifwithaddr((struct sockaddr *)sin) == NULL)
				return (EADDRNOTAVAIL);
		}
		if (lport != 0) {
			struct inpcb *t;

			/* GROSS */
			if (ntohs(lport) < IPPORT_RESERVED &&
			    p && suser_cred(p->p_ucred, PRISON_ROOT))
				return (EACCES);
			if (p && p->p_ucred->cr_prison)
				prison = 1;
			if (so->so_cred->cr_uid != 0 &&
			    !IN_MULTICAST(ntohl(sin->sin_addr.s_addr))) {
				t = in_pcblookup_local(inp->inp_pcbinfo,
				    sin->sin_addr, lport,
				    prison ? 0 : INPLOOKUP_WILDCARD);
				if (t &&
				    (!in_nullhost(sin->sin_addr) ||
				     !in_nullhost(t->inp_laddr) ||
				     (t->inp_socket->so_options &
					 SO_REUSEPORT) == 0) &&
				    (so->so_cred->cr_uid !=
				     t->inp_socket->so_cred->cr_uid)) {
#ifdef INET6
					if (!in_nullhost(sin->sin_addr) ||
					    !in_nullhost(t->inp_laddr) ||
					    INP_SOCKAF(so) ==
					    INP_SOCKAF(t->inp_socket))
#endif
					return (EADDRINUSE);
				}
			}
			if (prison && prison_ip(td, 0, &sin->sin_addr.s_addr))
				return (EADDRNOTAVAIL);
			t = in_pcblookup_local(pcbinfo, sin->sin_addr,
			    lport, prison ? 0 : wild);
			if (t && !(reuseport & t->inp_socket->so_options)) {
#ifdef INET6
				if (!in_nullhost(sin->sin_addr) ||
				    !in_nullhost(t->inp_laddr) ||
				    INP_SOCKAF(so) == INP_SOCKAF(t->inp_socket))
#endif
				return (EADDRINUSE);
			}
		}
		inp->inp_laddr = sin->sin_addr;
	}
	if (lport == 0) {
		ushort first, last;
		int count;

		if (inp->inp_laddr.s_addr != INADDR_ANY &&
		    prison_ip(td, 0, &inp->inp_laddr.s_addr )) {
			inp->inp_laddr.s_addr = INADDR_ANY;
			return (EINVAL);
		}
		inp->inp_flags |= INP_ANONPORT;

		if (inp->inp_flags & INP_HIGHPORT) {
			first = ipport_hifirstauto;	/* sysctl */
			last  = ipport_hilastauto;
			lastport = &pcbinfo->lasthi;
		} else if (inp->inp_flags & INP_LOWPORT) {
			if (p &&
			    (error = suser_cred(p->p_ucred, PRISON_ROOT))) {
				inp->inp_laddr.s_addr = INADDR_ANY;
				return (error);
			}
			first = ipport_lowfirstauto;	/* 1023 */
			last  = ipport_lowlastauto;	/* 600 */
			lastport = &pcbinfo->lastlow;
		} else {
			first = ipport_firstauto;	/* sysctl */
			last  = ipport_lastauto;
			lastport = &pcbinfo->lastport;
		}
		/*
		 * Simple check to ensure all ports are not used up causing
		 * a deadlock here.
		 *
		 * We split the two cases (up and down) so that the direction
		 * is not being tested on each round of the loop.
		 */
		if (first > last) {
			/*
			 * counting down
			 */
			count = first - last;

			do {
				if (count-- < 0) {	/* completely used? */
					inp->inp_laddr.s_addr = INADDR_ANY;
					return (EADDRNOTAVAIL);
				}
				--*lastport;
				if (*lastport > first || *lastport < last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo,
				 inp->inp_laddr, lport, wild));
		} else {
			/*
			 * counting up
			 */
			count = last - first;

			do {
				if (count-- < 0) {	/* completely used? */
					inp->inp_laddr.s_addr = INADDR_ANY;
					return (EADDRNOTAVAIL);
				}
				++*lastport;
				if (*lastport < first || *lastport > last)
					*lastport = first;
				lport = htons(*lastport);
			} while (in_pcblookup_local(pcbinfo,
				 inp->inp_laddr, lport, wild));
		}
	}
	inp->inp_lport = lport;
	if (prison_ip(td, 0, &inp->inp_laddr.s_addr)) {
		inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		return (EINVAL);
	}
	if (in_pcbinsporthash(inp) != 0) {
		inp->inp_laddr.s_addr = INADDR_ANY;
		inp->inp_lport = 0;
		return (EAGAIN);
	}
	return (0);
}

/*
 *   Transform old in_pcbconnect() into an inner subroutine for new
 *   in_pcbconnect(): Do some validity-checking on the remote
 *   address (in mbuf 'nam') and then determine local host address
 *   (i.e., which interface) to use to access that remote host.
 *
 *   This preserves definition of in_pcbconnect(), while supporting a
 *   slightly different version for T/TCP.  (This is more than
 *   a bit of a kludge, but cleaning up the internal interfaces would
 *   have forced minor changes in every protocol).
 */
int
in_pcbladdr(inp, nam, plocal_sin)
	struct inpcb *inp;
	struct sockaddr *nam;
	struct sockaddr_in **plocal_sin;
{
	struct in_ifaddr *ia;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;

	if (nam->sa_len != sizeof *sin)
		return (EINVAL);
	if (sin->sin_family != AF_INET)
		return (EAFNOSUPPORT);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	if (!TAILQ_EMPTY(&in_ifaddrhead)) {
		ia = TAILQ_FIRST(&in_ifaddrhead);
		/*
		 * If the destination address is INADDR_ANY,
		 * use the primary local address.
		 * If the supplied address is INADDR_BROADCAST,
		 * and the primary interface supports broadcast,
		 * choose the broadcast address for that interface.
		 */
		if (sin->sin_addr.s_addr == INADDR_ANY)
			sin->sin_addr = IA_SIN(ia)->sin_addr;
		else if (sin->sin_addr.s_addr == (u_long)INADDR_BROADCAST &&
		    (ia->ia_ifp->if_flags & IFF_BROADCAST))
			sin->sin_addr = satosin(&ia->ia_broadaddr)->sin_addr;
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		struct route *ro;

		ia = (struct in_ifaddr *)NULL;
		/*
		 * If route is known or can be allocated now,
		 * our src addr is taken from the i/f, else punt.
		 * Note that we should check the address family of the cached
		 * destination, in case of sharing the cache with IPv6.
		 */
		ro = &inp->inp_route;
		if (ro->ro_rt &&
		    (!(ro->ro_rt->rt_flags & RTF_UP) ||
		     ro->ro_dst.sa_family != AF_INET ||
		     satosin(&ro->ro_dst)->sin_addr.s_addr !=
				      sin->sin_addr.s_addr ||
		     inp->inp_socket->so_options & SO_DONTROUTE)) {
			RTFREE(ro->ro_rt);
			ro->ro_rt = (struct rtentry *)NULL;
		}
		if (!(inp->inp_socket->so_options & SO_DONTROUTE) && /*XXX*/
		    (ro->ro_rt == (struct rtentry *)NULL ||
		    ro->ro_rt->rt_ifp == (struct ifnet *)NULL)) {
			/* No route yet, so try to acquire one */
			bzero(&ro->ro_dst, sizeof(struct sockaddr_in));
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			((struct sockaddr_in *) &ro->ro_dst)->sin_addr =
				sin->sin_addr;
			rtalloc(ro);
		}
		/*
		 * If we found a route, use the address
		 * corresponding to the outgoing interface
		 * unless it is the loopback (in case a route
		 * to our address on another net goes to loopback).
		 */
		if (ro->ro_rt && !(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
			ia = ifatoia(ro->ro_rt->rt_ifa);
		if (ia == NULL) {
			u_short fport = sin->sin_port;

			sin->sin_port = 0;
			ia = ifatoia(ifa_ifwithdstaddr(sintosa(sin)));
			if (ia == NULL)
				ia = ifatoia(ifa_ifwithnet(sintosa(sin)));
			sin->sin_port = fport;
			if (ia == NULL)
				ia = TAILQ_FIRST(&in_ifaddrhead);
			if (ia == NULL)
				return (EADDRNOTAVAIL);
		}
		/*
		 * If the destination address is multicast and an outgoing
		 * interface has been set as a multicast option, use the
		 * address of that interface as our source address.
		 */
		if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)) &&
		    inp->inp_moptions != NULL) {
			struct ip_moptions *imo;
			struct ifnet *ifp;

			imo = inp->inp_moptions;
			if (imo->imo_multicast_ifp != NULL) {
				ifp = imo->imo_multicast_ifp;
				TAILQ_FOREACH(ia, &in_ifaddrhead, ia_link)
					if (ia->ia_ifp == ifp)
						break;
				if (ia == NULL)
					return (EADDRNOTAVAIL);
			}
		}
		/*
		 * Don't do pcblookup call here; return interface in plocal_sin
		 * and exit to caller, that will do the lookup.
		 */
		*plocal_sin = &ia->ia_addr;

	}
	return (0);
}

/*
 * Outer subroutine:
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(struct inpcb *inp, struct sockaddr *nam, struct thread *td)
{
	struct sockaddr_in *if_sin;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	struct sockaddr_in sa;
	struct ucred *cr = td->td_proc ? td->td_proc->p_ucred : NULL;
	int error;

	if (cr && cr->cr_prison != NULL && in_nullhost(inp->inp_laddr)) {
		bzero(&sa, sizeof sa);
		sa.sin_addr.s_addr = htonl(cr->cr_prison->pr_ip);
		sa.sin_len = sizeof sa;
		sa.sin_family = AF_INET;
		error = in_pcbbind(inp, (struct sockaddr *)&sa, td);
		if (error)
			return (error);
	}

	/* Call inner routine to assign local interface address. */
	if ((error = in_pcbladdr(inp, nam, &if_sin)) != 0)
		return (error);

	if (in_pcblookup_hash(inp->inp_cpcbinfo, sin->sin_addr, sin->sin_port,
	    inp->inp_laddr.s_addr ? inp->inp_laddr : if_sin->sin_addr,
	    inp->inp_lport, FALSE, NULL) != NULL) {
		return (EADDRINUSE);
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, (struct sockaddr *)NULL, td);
			if (error)
				return (error);
		}
		inp->inp_laddr = if_sin->sin_addr;
	}
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	in_pcbinsconnhash(inp);
	return (0);
}

void
in_pcbdisconnect(inp)
	struct inpcb *inp;
{

	inp->inp_faddr.s_addr = INADDR_ANY;
	inp->inp_fport = 0;
	in_pcbremconnhash(inp);
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
}

void
in_pcbdetach(inp)
	struct inpcb *inp;
{
	struct socket *so = inp->inp_socket;
	struct inpcbinfo *ipi = inp->inp_pcbinfo;

#ifdef IPSEC
	ipsec4_delete_pcbpolicy(inp);
#endif /*IPSEC*/
	inp->inp_gencnt = ++ipi->ipi_gencnt;
	in_pcbremlists(inp);
	so->so_pcb = 0;
	sofree(so);
	if (inp->inp_options)
		m_free(inp->inp_options);
	if (inp->inp_route.ro_rt)
		rtfree(inp->inp_route.ro_rt);
	ip_freemoptions(inp->inp_moptions);
	inp->inp_vflag = 0;
	zfree(ipi->ipi_zone, inp);
}

/*
 * The calling convention of in_setsockaddr() and in_setpeeraddr() was
 * modified to match the pru_sockaddr() and pru_peeraddr() entry points
 * in struct pr_usrreqs, so that protocols can just reference then directly
 * without the need for a wrapper function.  The socket must have a valid
 * (i.e., non-nil) PCB, but it should be impossible to get an invalid one
 * except through a kernel programming error, so it is acceptable to panic
 * (or in this case trap) if the PCB is invalid.  (Actually, we don't trap
 * because there actually /is/ a programming error somewhere... XXX)
 */
int
in_setsockaddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	struct inpcb *inp;
	struct sockaddr_in *sin;

	/*
	 * Do the malloc first in case it blocks.
	 */
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		M_WAITOK | M_ZERO);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof *sin;

	s = splnet();
	inp = so->so_pcb;
	if (!inp) {
		splx(s);
		free(sin, M_SONAME);
		return (ECONNRESET);
	}
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
	splx(s);

	*nam = (struct sockaddr *)sin;
	return (0);
}

int
in_setpeeraddr(so, nam)
	struct socket *so;
	struct sockaddr **nam;
{
	int s;
	struct inpcb *inp;
	struct sockaddr_in *sin;

	/*
	 * Do the malloc first in case it blocks.
	 */
	MALLOC(sin, struct sockaddr_in *, sizeof *sin, M_SONAME,
		M_WAITOK | M_ZERO);
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof *sin;

	s = splnet();
	inp = so->so_pcb;
	if (!inp) {
		splx(s);
		free(sin, M_SONAME);
		return (ECONNRESET);
	}
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;
	splx(s);

	*nam = (struct sockaddr *)sin;
	return (0);
}

void
in_pcbnotifyall(head, faddr, errno, notify)
	struct inpcbhead *head;
	struct in_addr faddr;
	void (*notify) (struct inpcb *, int);
{
	struct inpcb *inp, *ninp;
	int s;

	/*
	 * note: if INP_PLACEMARKER is set we must ignore the rest of
	 * the structure and skip it.
	 */
	s = splnet();
	LIST_FOREACH_MUTABLE(inp, head, inp_list, ninp) {
		if (inp->inp_flags & INP_PLACEMARKER)
			continue;
#ifdef INET6
		if (!(inp->inp_vflag & INP_IPV4))
			continue;
#endif
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    inp->inp_socket == NULL)
			continue;
		(*notify)(inp, errno);		/* can remove inp from list! */
	}
	splx(s);
}

void
in_pcbpurgeif0(head, ifp)
	struct inpcb *head;
	struct ifnet *ifp;
{
	struct inpcb *inp;
	struct ip_moptions *imo;
	int i, gap;

	for (inp = head; inp != NULL; inp = LIST_NEXT(inp, inp_list)) {
		if (inp->inp_flags & INP_PLACEMARKER)
			continue;
		imo = inp->inp_moptions;
		if ((inp->inp_vflag & INP_IPV4) && imo != NULL) {
			/*
			 * Unselect the outgoing interface if it is being
			 * detached.
			 */
			if (imo->imo_multicast_ifp == ifp)
				imo->imo_multicast_ifp = NULL;

			/*
			 * Drop multicast group membership if we joined
			 * through the interface being detached.
			 */
			for (i = 0, gap = 0; i < imo->imo_num_memberships;
			    i++) {
				if (imo->imo_membership[i]->inm_ifp == ifp) {
					in_delmulti(imo->imo_membership[i]);
					gap++;
				} else if (gap != 0)
					imo->imo_membership[i - gap] =
					    imo->imo_membership[i];
			}
			imo->imo_num_memberships -= gap;
		}
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(struct inpcb *inp)
{
	struct rtentry *rt;
	struct rt_addrinfo rtinfo;

	if ((rt = inp->inp_route.ro_rt)) {
		bzero(&rtinfo, sizeof(struct rt_addrinfo));
		rtinfo.rti_info[RTAX_DST] = rt_key(rt);
		rtinfo.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		rtinfo.rti_info[RTAX_NETMASK] = rt_mask(rt);
		rtinfo.rti_flags = rt->rt_flags;
		rt_missmsg(RTM_LOSING, &rtinfo, rt->rt_flags, 0);
		if (rt->rt_flags & RTF_DYNAMIC)
			rtrequest1(RTM_DELETE, &rtinfo, NULL);
		inp->inp_route.ro_rt = NULL;
		rtfree(rt);
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 */
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in_rtchange(inp, errno)
	struct inpcb *inp;
	int errno;
{
	if (inp->inp_route.ro_rt) {
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = NULL;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
}

/*
 * Lookup a PCB based on the local address and port.
 */
struct inpcb *
in_pcblookup_local(pcbinfo, laddr, lport_arg, wild_okay)
	struct inpcbinfo *pcbinfo;
	struct in_addr laddr;
	u_int lport_arg;
	int wild_okay;
{
	struct inpcb *inp;
	int matchwild = 3, wildcard;
	u_short lport = lport_arg;

	struct inpcbporthead *porthash;
	struct inpcbport *phd;
	struct inpcb *match = NULL;

	/*
	 * Best fit PCB lookup.
	 *
	 * First see if this local port is in use by looking on the
	 * port hash list.
	 */
	porthash = &pcbinfo->porthashbase[INP_PCBPORTHASH(lport,
	    pcbinfo->porthashmask)];
	LIST_FOREACH(phd, porthash, phd_hash) {
		if (phd->phd_port == lport)
			break;
	}
	if (phd != NULL) {
		/*
		 * Port is in use by one or more PCBs. Look for best
		 * fit.
		 */
		LIST_FOREACH(inp, &phd->phd_pcblist, inp_portlist) {
			wildcard = 0;
#ifdef INET6
			if ((inp->inp_vflag & INP_IPV4) == 0)
				continue;
#endif
			if (inp->inp_faddr.s_addr != INADDR_ANY)
				wildcard++;
			if (inp->inp_laddr.s_addr != INADDR_ANY) {
				if (laddr.s_addr == INADDR_ANY)
					wildcard++;
				else if (inp->inp_laddr.s_addr != laddr.s_addr)
					continue;
			} else {
				if (laddr.s_addr != INADDR_ANY)
					wildcard++;
			}
			if (wildcard && !wild_okay)
				continue;
			if (wildcard < matchwild) {
				match = inp;
				matchwild = wildcard;
				if (matchwild == 0) {
					break;
				}
			}
		}
	}
	return (match);
}

/*
 * Lookup PCB in hash list.
 */
struct inpcb *
in_pcblookup_hash(pcbinfo, faddr, fport_arg, laddr, lport_arg, wildcard, ifp)
	struct inpcbinfo *pcbinfo;
	struct in_addr faddr, laddr;
	u_int fport_arg, lport_arg;
	boolean_t wildcard;
	struct ifnet *ifp;
{
	struct inpcbhead *head;
	struct inpcb *inp;
	u_short fport = fport_arg, lport = lport_arg;

	/*
	 * First look for an exact match.
	 */
	head = &pcbinfo->hashbase[INP_PCBCONNHASH(faddr.s_addr, fport,
	    laddr.s_addr, lport, pcbinfo->hashmask)];
	LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
		if (!(inp->inp_vflag & INP_IPV4))
			continue;
#endif
		if (in_hosteq(inp->inp_faddr, faddr) &&
		    in_hosteq(inp->inp_laddr, laddr) &&
		    inp->inp_fport == fport && inp->inp_lport == lport) {
			/* found */
			return (inp);
		}
	}

	if (wildcard) {
		struct inpcb *local_wild = NULL;
#ifdef INET6
		struct inpcb *local_wild_mapped = NULL;
#endif
		struct inpcontainer *ic;
		struct inpcontainerhead *chead;

		chead = &pcbinfo->wildcardhashbase[
		    INP_PCBWILDCARDHASH(lport, pcbinfo->wildcardhashmask)];
		LIST_FOREACH(ic, chead, ic_list) {
			inp = ic->ic_inp;
#ifdef INET6
			if (!(inp->inp_vflag & INP_IPV4))
				continue;
#endif
			if (inp->inp_lport == lport) {
				if (ifp && ifp->if_type == IFT_FAITH &&
				    !(inp->inp_flags & INP_FAITH))
					continue;
				if (inp->inp_laddr.s_addr == laddr.s_addr)
					return (inp);
				if (inp->inp_laddr.s_addr == INADDR_ANY) {
#ifdef INET6
					if (INP_CHECK_SOCKAF(inp->inp_socket,
							     AF_INET6))
						local_wild_mapped = inp;
					else
#endif
						local_wild = inp;
				}
			}
		}
#ifdef INET6
		if (local_wild == NULL)
			return (local_wild_mapped);
#endif
		return (local_wild);
	}

	/*
	 * Not found.
	 */
	return (NULL);
}

/*
 * Insert PCB into connection hash table.
 */
void
in_pcbinsconnhash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_cpcbinfo;
	struct inpcbhead *bucket;
	u_int32_t hashkey_faddr, hashkey_laddr;

#ifdef INET6
	if (inp->inp_vflag & INP_IPV6) {
		hashkey_faddr = inp->in6p_faddr.s6_addr32[3] /* XXX JH */;
		hashkey_laddr = inp->in6p_laddr.s6_addr32[3] /* XXX JH */;
	} else {
#endif
		hashkey_faddr = inp->inp_faddr.s_addr;
		hashkey_laddr = inp->inp_laddr.s_addr;
#ifdef INET6
	}
#endif

	KASSERT(!(inp->inp_flags & INP_CONNECTED), ("already on hash list"));
	inp->inp_flags |= INP_CONNECTED;

	/*
	 * Insert into the connection hash table.
	 */
	bucket = &pcbinfo->hashbase[INP_PCBCONNHASH(hashkey_faddr,
	    inp->inp_fport, hashkey_laddr, inp->inp_lport, pcbinfo->hashmask)];
	LIST_INSERT_HEAD(bucket, inp, inp_hash);
}

/*
 * Remove PCB from connection hash table.
 */
void
in_pcbremconnhash(struct inpcb *inp)
{
	KASSERT(inp->inp_flags & INP_CONNECTED, ("inp not connected"));
	LIST_REMOVE(inp, inp_hash);
	inp->inp_flags &= ~INP_CONNECTED;
}

/*
 * Insert PCB into port hash table.
 */
int
in_pcbinsporthash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	struct inpcbporthead *pcbporthash;
	struct inpcbport *phd;

	/*
	 * Insert into the port hash table.
	 */
	pcbporthash = &pcbinfo->porthashbase[
	    INP_PCBPORTHASH(inp->inp_lport, pcbinfo->porthashmask)];

	/* Go through port list and look for a head for this lport. */
	LIST_FOREACH(phd, pcbporthash, phd_hash)
		if (phd->phd_port == inp->inp_lport)
			break;

	/* If none exists, malloc one and tack it on. */
	if (phd == NULL) {
		MALLOC(phd, struct inpcbport *, sizeof(struct inpcbport),
		    M_PCB, M_INTWAIT | M_NULLOK);
		if (phd == NULL)
			return (ENOBUFS); /* XXX */
		phd->phd_port = inp->inp_lport;
		LIST_INIT(&phd->phd_pcblist);
		LIST_INSERT_HEAD(pcbporthash, phd, phd_hash);
	}

	inp->inp_phd = phd;
	LIST_INSERT_HEAD(&phd->phd_pcblist, inp, inp_portlist);

	return (0);
}

void
in_pcbinswildcardhash_oncpu(struct inpcb *inp, struct inpcbinfo *pcbinfo)
{
	struct inpcontainer *ic;
	struct inpcontainerhead *bucket;

	bucket = &pcbinfo->wildcardhashbase[
	    INP_PCBWILDCARDHASH(inp->inp_lport, pcbinfo->wildcardhashmask)];

	ic = malloc(sizeof(struct inpcontainer), M_TEMP, M_INTWAIT);
	ic->ic_inp = inp;
	LIST_INSERT_HEAD(bucket, ic, ic_list);
}

/*
 * Insert PCB into wildcard hash table.
 */
void
in_pcbinswildcardhash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
	
	KKASSERT(pcbinfo != NULL);

	in_pcbinswildcardhash_oncpu(inp, pcbinfo);
	inp->inp_flags |= INP_WILDCARD;
}

void
in_pcbremwildcardhash_oncpu(struct inpcb *inp, struct inpcbinfo *pcbinfo)
{
	struct inpcontainer *ic;
	struct inpcontainerhead *head;

	/* find bucket */
	head = &pcbinfo->wildcardhashbase[
	    INP_PCBWILDCARDHASH(inp->inp_lport, pcbinfo->wildcardhashmask)];

	LIST_FOREACH(ic, head, ic_list) {
		if (ic->ic_inp == inp)
			goto found;
	}
	return;			/* not found! */

found:
	LIST_REMOVE(ic, ic_list);	/* remove container from bucket chain */
	free(ic, M_TEMP);		/* deallocate container */
}

/*
 * Remove PCB from wildcard hash table.
 */
void
in_pcbremwildcardhash(struct inpcb *inp)
{
	struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;

	KASSERT(inp->inp_flags & INP_WILDCARD, ("inp not wildcard"));
	in_pcbremwildcardhash_oncpu(inp, pcbinfo);
	inp->inp_flags &= ~INP_WILDCARD;
}

/*
 * Remove PCB from various lists.
 */
void
in_pcbremlists(inp)
	struct inpcb *inp;
{
	if (inp->inp_lport) {
		struct inpcbport *phd = inp->inp_phd;

		LIST_REMOVE(inp, inp_portlist);
		if (LIST_FIRST(&phd->phd_pcblist) == NULL) {
			LIST_REMOVE(phd, phd_hash);
			free(phd, M_PCB);
		}
	}
	if (inp->inp_flags & INP_WILDCARD) {
		in_pcbremwildcardhash(inp);
	} else if (inp->inp_flags & INP_CONNECTED) {
		in_pcbremconnhash(inp);
	}
	LIST_REMOVE(inp, inp_list);
	inp->inp_pcbinfo->ipi_count--;
}

int
prison_xinpcb(struct thread *td, struct inpcb *inp)
{
	struct ucred *cr;

	if (td->td_proc == NULL)
		return (0);
	cr = td->td_proc->p_ucred;
	if (cr->cr_prison == NULL)
		return (0);
	if (ntohl(inp->inp_laddr.s_addr) == cr->cr_prison->pr_ip)
		return (0);
	return (1);
}

int
in_pcblist_global(SYSCTL_HANDLER_ARGS)
{
	struct inpcbinfo *pcbinfo = arg1;
	struct inpcb *inp, *marker;
	struct xinpcb xi;
	int error, i, n;
	inp_gen_t gencnt;

	/*
	 * The process of preparing the TCB list is too time-consuming and
	 * resource-intensive to repeat twice on every request.
	 */
	if (req->oldptr == NULL) {
		n = pcbinfo->ipi_count;
		req->oldidx = (n + n/8 + 10) * sizeof(struct xinpcb);
		return 0;
	}

	if (req->newptr != NULL)
		return EPERM;

	/*
	 * OK, now we're committed to doing something.  Re-fetch ipi_count
	 * after obtaining the generation count.
	 */
	gencnt = pcbinfo->ipi_gencnt;
	n = pcbinfo->ipi_count;

	marker = malloc(sizeof(struct inpcb), M_TEMP, M_WAITOK|M_ZERO);
	marker->inp_flags |= INP_PLACEMARKER;
	LIST_INSERT_HEAD(&pcbinfo->pcblisthead, marker, inp_list);

	i = 0;
	error = 0;

	while ((inp = LIST_NEXT(marker, inp_list)) != NULL && i < n) {
		LIST_REMOVE(marker, inp_list);
		LIST_INSERT_AFTER(inp, marker, inp_list);

		if (inp->inp_flags & INP_PLACEMARKER)
			continue;
		if (inp->inp_gencnt > gencnt)
			continue;
		if (prison_xinpcb(req->td, inp))
			continue;
		bzero(&xi, sizeof xi);
		xi.xi_len = sizeof xi;
		bcopy(inp, &xi.xi_inp, sizeof *inp);
		if (inp->inp_socket)
			sotoxsocket(inp->inp_socket, &xi.xi_socket);
		if ((error = SYSCTL_OUT(req, &xi, sizeof xi)) != 0)
			break;
		++i;
	}
	LIST_REMOVE(marker, inp_list);
	if (error == 0 && i < n) {
		bzero(&xi, sizeof xi);
		xi.xi_len = sizeof xi;
		while (i < n) {
			error = SYSCTL_OUT(req, &xi, sizeof xi);
			++i;
		}
	}
	free(marker, M_TEMP);
	return(error);
}
