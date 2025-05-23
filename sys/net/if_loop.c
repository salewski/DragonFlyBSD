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
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_loop.c,v 1.47.2.8 2003/06/01 01:46:11 silby Exp $
 * $DragonFly: src/sys/net/if_loop.c,v 1.15 2005/02/12 01:24:34 joerg Exp $
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */
#include "use_loop.h"

#include "opt_atalk.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/ifq_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#ifdef IPX
#include <netproto/ipx/ipx.h>
#include <netproto/ipx/ipx_if.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef NETATALK
#include <netproto/atalk/at.h>
#include <netproto/atalk/at_var.h>
#endif

int loioctl (struct ifnet *, u_long, caddr_t, struct ucred *);
static void lortrequest (int, struct rtentry *, struct rt_addrinfo *);

static void loopattach (void *);
#ifdef ALTQ
static void lo_altqstart(struct ifnet *);
#endif
PSEUDO_SET(loopattach, if_loop);

int looutput (struct ifnet *ifp,
		struct mbuf *m, struct sockaddr *dst, struct rtentry *rt);

#ifdef TINY_LOMTU
#define	LOMTU	(1024+512)
#elif defined(LARGE_LOMTU)
#define LOMTU	131072
#else
#define LOMTU	16384
#endif

struct	ifnet loif[NLOOP];

/* ARGSUSED */
static void
loopattach(void *dummy)
{
	struct ifnet *ifp;
	int i;

	for (i = 0, ifp = loif; i < NLOOP; i++, ifp++) {
		if_initname(ifp, "lo", i);
		ifp->if_mtu = LOMTU;
		ifp->if_flags = IFF_LOOPBACK | IFF_MULTICAST;
		ifp->if_ioctl = loioctl;
		ifp->if_output = looutput;
		ifp->if_type = IFT_LOOP;
		ifq_set_maxlen(&ifp->if_snd, ifqmaxlen);
		ifq_set_ready(&ifp->if_snd);
#ifdef ALTQ
	        ifp->if_start = lo_altqstart;
#endif
		if_attach(ifp);
		bpfattach(ifp, DLT_NULL, sizeof(u_int));
	}
}

int
looutput(
	struct ifnet *ifp,
	struct mbuf *m,
	struct sockaddr *dst,
	struct rtentry *rt)
{
	struct mbuf *n;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("looutput no HDR");

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}
	/*
	 * KAME requires that the packet to be contiguous on the
	 * mbuf.  We need to make that sure.
	 * this kind of code should be avoided.
	 *
	 * XXX: KAME may no longer need contiguous packets.  Once
	 * that has been verified, the following code _should_ be
	 * removed.
	 */
	if (m && m->m_next != NULL) {

		n = m_defrag(m, MB_DONTWAIT);

		if (n == NULL) {
			m_freem(m);
			return (ENOBUFS);
		} else {
			m = n;
		}
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
#if 1	/* XXX */
	switch (dst->sa_family) {
	case AF_INET:
	case AF_INET6:
	case AF_IPX:
	case AF_NS:
	case AF_APPLETALK:
		break;
	default:
		printf("looutput: af=%d unexpected\n", dst->sa_family);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
#endif
	return (if_simloop(ifp, m, dst->sa_family, 0));
}

/*
 * if_simloop()
 *
 * This function is to support software emulation of hardware loopback,
 * i.e., for interfaces with the IFF_SIMPLEX attribute. Since they can't
 * hear their own broadcasts, we create a copy of the packet that we
 * would normally receive via a hardware loopback.
 *
 * This function expects the packet to include the media header of length hlen.
 */
int
if_simloop(struct ifnet *ifp, struct mbuf *m, int af, int hlen)
{
	int isr;

	KASSERT((m->m_flags & M_PKTHDR) != 0, ("if_simloop: no HDR"));
	m->m_pkthdr.rcvif = ifp;

	/* BPF write needs to be handled specially */
	if (af == AF_UNSPEC) {
		KASSERT(m->m_len >= sizeof(int), ("if_simloop: m_len"));
		af = *(mtod(m, int *));
		m->m_len -= sizeof(int);
		m->m_pkthdr.len -= sizeof(int);
		m->m_data += sizeof(int);
	}

	if (ifp->if_bpf) {
		if (ifp->if_bpf->bif_dlt == DLT_NULL) {
			uint32_t bpf_af = (uint32_t)af;
			bpf_ptap(ifp->if_bpf, m, &bpf_af, 4);
		}
		else {
			bpf_mtap(ifp->if_bpf, m);
		}
	}

	/* Strip away media header */
	if (hlen > 0) {
		m_adj(m, hlen);
#ifdef __alpha__
		/* The alpha doesn't like unaligned data.
		 * We move data down in the first mbuf */
		if (mtod(m, vm_offset_t) & 3) {
			KASSERT(hlen >= 3, ("if_simloop: hlen too small"));
			bcopy(m->m_data, 
			    (char *)(mtod(m, vm_offset_t) 
				- (mtod(m, vm_offset_t) & 3)),
			    m->m_len);
			mtod(m,vm_offset_t) -= (mtod(m, vm_offset_t) & 3);
		}
#endif
	}
 
#ifdef ALTQ
	/*
	 * altq for loop is just for debugging.
	 * only used when called for loop interface (not for
	 * a simplex interface).
	 */
	if (ifq_is_enabled(&ifp->if_snd) && ifp->if_start == lo_altqstart) {
		struct altq_pktattr pktattr;
		int32_t *afp;
	        int error, s;

		/*
		 * if the queueing discipline needs packet classification,
		 * do it before prepending link headers.
		 */
		ifq_classify(&ifp->if_snd, m, af, &pktattr);

		M_PREPEND(m, sizeof(int32_t), MB_DONTWAIT);
		if (m == 0)
			return(ENOBUFS);
		afp = mtod(m, int32_t *);
		*afp = (int32_t)af;

	        s = splimp();
		error = ifq_enqueue(&ifp->if_snd, m, &pktattr);
		(*ifp->if_start)(ifp);
		splx(s);
		return (error);
	}
#endif /* ALTQ */

	/* Deliver to upper layer protocol */
	switch (af) {
#ifdef INET
	case AF_INET:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		m->m_flags |= M_LOOP;
		isr = NETISR_IPV6;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		isr = NETISR_IPX;
		break;
#endif
#ifdef NS
	case AF_NS:
		isr = NETISR_NS;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		isr = NETISR_ATALK2;
		break;
#endif
	default:
		printf("if_simloop: can't handle af=%d\n", af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_queue(isr, m);
	return (0);
}

#ifdef ALTQ
static void
lo_altqstart(struct ifnet *ifp)
{
	struct mbuf *m;
	int32_t af, *afp;
	int s, isr;
	
	while (1) {
		s = splimp();
		m = ifq_dequeue(&ifp->if_snd);
		splx(s);
		if (m == NULL)
			return;

		afp = mtod(m, int32_t *);
		af = *afp;
		m_adj(m, sizeof(int32_t));

		switch (af) {
#ifdef INET
		case AF_INET:
			isr = NETISR_IP;
			break;
#endif
#ifdef INET6
		case AF_INET6:
			m->m_flags |= M_LOOP;
			isr = NETISR_IPV6;
			break;
#endif
#ifdef IPX
		case AF_IPX:
			isr = NETISR_IPX;
			break;
#endif
#ifdef NS
		case AF_NS:
			isr = NETISR_NS;
			break;
#endif
#ifdef ISO
		case AF_ISO:
			isr = NETISR_ISO;
			break;
#endif
#ifdef NETATALK
		case AF_APPLETALK:
			isr = NETISR_ATALK2;
			break;
#endif
		default:
			printf("lo_altqstart: can't handle af%d\n", af);
			m_freem(m);
			return;
		}

		ifp->if_ipackets++;
		ifp->if_ibytes += m->m_pkthdr.len;
		netisr_queue(isr, m);
	}
}
#endif /* ALTQ */

/* ARGSUSED */
static void
lortrequest(int cmd, struct rtentry *rt, struct rt_addrinfo *info)
{
	if (rt) {
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu; /* for ISO */
		/*
		 * For optimal performance, the send and receive buffers
		 * should be at least twice the MTU plus a little more for
		 * overhead.
		 */
		rt->rt_rmx.rmx_recvpipe = rt->rt_rmx.rmx_sendpipe = 3 * LOMTU;
	}
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
loioctl(struct ifnet *ifp, u_long cmd, caddr_t data, struct ucred *cr)
{
	struct ifaddr *ifa;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP | IFF_RUNNING;
		ifa = (struct ifaddr *)data;
		ifa->ifa_rtrequest = lortrequest;
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCSIFFLAGS:
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
