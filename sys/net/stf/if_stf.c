/*	$FreeBSD: src/sys/net/if_stf.c,v 1.1.2.11 2003/01/23 21:06:44 sam Exp $	*/
/*	$DragonFly: src/sys/net/stf/if_stf.c,v 1.14 2005/01/26 00:37:39 joerg Exp $	*/
/*	$KAME: if_stf.c,v 1.73 2001/12/03 11:08:30 keiichi Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
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
 * 6to4 interface, based on RFC3056.
 *
 * 6to4 interface is NOT capable of link-layer (I mean, IPv4) multicasting.
 * There is no address mapping defined from IPv6 multicast address to IPv4
 * address.  Therefore, we do not have IFF_MULTICAST on the interface.
 *
 * Due to the lack of address mapping for link-local addresses, we cannot
 * throw packets toward link-local addresses (fe80::x).  Also, we cannot throw
 * packets to link-local multicast addresses (ff02::x).
 *
 * Here are interesting symptoms due to the lack of link-local address:
 *
 * Unicast routing exchange:
 * - RIPng: Impossible.  Uses link-local multicast packet toward ff02::9,
 *   and link-local addresses as nexthop.
 * - OSPFv6: Impossible.  OSPFv6 assumes that there's link-local address
 *   assigned to the link, and makes use of them.  Also, HELLO packets use
 *   link-local multicast addresses (ff02::5 and ff02::6).
 * - BGP4+: Maybe.  You can only use global address as nexthop, and global
 *   address as TCP endpoint address.
 *
 * Multicast routing protocols:
 * - PIM: Hello packet cannot be used to discover adjacent PIM routers.
 *   Adjacent PIM routers must be configured manually (is it really spec-wise
 *   correct thing to do?).
 *
 * ICMPv6:
 * - Redirects cannot be used due to the lack of link-local address.
 *
 * stf interface does not have, and will not need, a link-local address.  
 * It seems to have no real benefit and does not help the above symptoms much.
 * Even if we assign link-locals to interface, we cannot really
 * use link-local unicast/multicast on top of 6to4 cloud (since there's no
 * encapsulation defined for link-local address), and the above analysis does
 * not change.  RFC3056 does not mandate the assignment of link-local address
 * either.
 *
 * 6to4 interface has security issues.  Refer to
 * http://playground.iijlab.net/i-d/draft-itojun-ipv6-transition-abuse-00.txt
 * for details.  The code tries to filter out some of malicious packets.
 * Note that there is no way to be 100% secure.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <sys/malloc.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/if_types.h>
#include "if_stf.h"

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>

#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip_ecn.h>

#include <netinet/ip_encap.h>

#include <machine/stdarg.h>

#include <net/net_osdep.h>

#include <net/bpf.h>

#define IN6_IS_ADDR_6TO4(x)	(ntohs((x)->s6_addr16[0]) == 0x2002)
#define GET_V4(x)	((struct in_addr *)(&(x)->s6_addr16[1]))

struct stf_softc {
	struct ifnet	sc_if;	   /* common area */
	union {
		struct route  __sc_ro4;
		struct route_in6 __sc_ro6; /* just for safety */
	} __sc_ro46;
#define sc_ro	__sc_ro46.__sc_ro4
	const struct encaptab *encap_cookie;
};

static struct stf_softc *stf;

static MALLOC_DEFINE(M_STF, "stf", "6to4 Tunnel Interface");
static int ip_stf_ttl = 40;

extern  struct domain inetdomain;
struct protosw in_stf_protosw =
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  in_stf_input, rip_output,	0,		rip_ctloutput,
  0,
  0,            0,              0,              0,
  &rip_usrreqs
};

static int stfmodevent (module_t, int, void *);
static int stf_encapcheck (const struct mbuf *, int, int, void *);
static struct in6_ifaddr *stf_getsrcifa6 (struct ifnet *);
static int stf_output (struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
static int stf_checkaddr4 (struct stf_softc *, struct in_addr *,
	struct ifnet *);
static int stf_checkaddr6 (struct stf_softc *, struct in6_addr *,
	struct ifnet *);
static void stf_rtrequest (int, struct rtentry *, struct rt_addrinfo *);
static int stf_ioctl (struct ifnet *, u_long, caddr_t, struct ucred *);

static int
stfmodevent(mod, type, data)
	module_t mod;
	int type;
	void *data;
{
	struct stf_softc *sc;
	int err;
	const struct encaptab *p;

	switch (type) {
	case MOD_LOAD:
		stf = malloc(sizeof(struct stf_softc), M_STF, M_WAITOK);
		bzero(stf, sizeof(struct stf_softc));
		sc = stf;

		bzero(sc, sizeof(*sc));
		if_initname(&(sc->sc_if), "stf", 0);

		p = encap_attach_func(AF_INET, IPPROTO_IPV6, stf_encapcheck,
		    (void *)&in_stf_protosw, sc);
		if (p == NULL) {
			printf("%s: attach failed\n", if_name(&sc->sc_if));
			return (ENOMEM);
		}
		sc->encap_cookie = p;

		sc->sc_if.if_mtu    = IPV6_MMTU;
		sc->sc_if.if_flags  = 0;
		sc->sc_if.if_ioctl  = stf_ioctl;
		sc->sc_if.if_output = stf_output;
		sc->sc_if.if_type   = IFT_STF;
#if 0
		/* turn off ingress filter */
		sc->sc_if.if_flags  |= IFF_LINK2;
#endif
		sc->sc_if.if_snd.ifq_maxlen = IFQ_MAXLEN;
		if_attach(&sc->sc_if);
		bpfattach(&sc->sc_if, DLT_NULL, sizeof(u_int));
		break;
	case MOD_UNLOAD:
		sc = stf;
		bpfdetach(&sc->sc_if);
		if_detach(&sc->sc_if);
		err = encap_detach(sc->encap_cookie);
		KASSERT(err == 0, ("Unexpected error detaching encap_cookie"));
		free(sc, M_STF);
		break;
	}

	return (0);
}

static moduledata_t stf_mod = {
	"if_stf",
	stfmodevent,
	0
};

DECLARE_MODULE(if_stf, stf_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

static int
stf_encapcheck(m, off, proto, arg)
	const struct mbuf *m;
	int off;
	int proto;
	void *arg;
{
	struct ip ip;
	struct in6_ifaddr *ia6;
	struct stf_softc *sc;
	struct in_addr a, b;

	sc = (struct stf_softc *)arg;
	if (sc == NULL)
		return 0;

	if ((sc->sc_if.if_flags & IFF_UP) == 0)
		return 0;

	/* IFF_LINK0 means "no decapsulation" */
	if ((sc->sc_if.if_flags & IFF_LINK0) != 0)
		return 0;

	if (proto != IPPROTO_IPV6)
		return 0;

	m_copydata(m, 0, sizeof(ip), (caddr_t)&ip);

	if (ip.ip_v != 4)
		return 0;

	ia6 = stf_getsrcifa6(&sc->sc_if);
	if (ia6 == NULL)
		return 0;

	/*
	 * check if IPv4 dst matches the IPv4 address derived from the
	 * local 6to4 address.
	 * success on: dst = 10.1.1.1, ia6->ia_addr = 2002:0a01:0101:...
	 */
	if (bcmp(GET_V4(&ia6->ia_addr.sin6_addr), &ip.ip_dst,
	    sizeof(ip.ip_dst)) != 0)
		return 0;

	/*
	 * check if IPv4 src matches the IPv4 address derived from the
	 * local 6to4 address masked by prefixmask.
	 * success on: src = 10.1.1.1, ia6->ia_addr = 2002:0a00:.../24
	 * fail on: src = 10.1.1.1, ia6->ia_addr = 2002:0b00:.../24
	 */
	bzero(&a, sizeof(a));
	a.s_addr = GET_V4(&ia6->ia_addr.sin6_addr)->s_addr;
	a.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	b = ip.ip_src;
	b.s_addr &= GET_V4(&ia6->ia_prefixmask.sin6_addr)->s_addr;
	if (a.s_addr != b.s_addr)
		return 0;

	/* stf interface makes single side match only */
	return 32;
}

static struct in6_ifaddr *
stf_getsrcifa6(ifp)
	struct ifnet *ifp;
{
	struct ifaddr *ia;
	struct in_ifaddr *ia4;
	struct sockaddr_in6 *sin6;
	struct in_addr in;

	TAILQ_FOREACH(ia, &ifp->if_addrlist, ifa_list) {
		if (ia->ifa_addr == NULL)
			continue;
		if (ia->ifa_addr->sa_family != AF_INET6)
			continue;
		sin6 = (struct sockaddr_in6 *)ia->ifa_addr;
		if (!IN6_IS_ADDR_6TO4(&sin6->sin6_addr))
			continue;

		bcopy(GET_V4(&sin6->sin6_addr), &in, sizeof(in));
		LIST_FOREACH(ia4, INADDR_HASH(in.s_addr), ia_hash)
			if (ia4->ia_addr.sin_addr.s_addr == in.s_addr)
				break;
		if (ia4 == NULL)
			continue;

		return (struct in6_ifaddr *)ia;
	}

	return NULL;
}

static int
stf_output(ifp, m, dst, rt)
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct stf_softc *sc;
	struct sockaddr_in6 *dst6;
	struct in_addr *in4;
	struct sockaddr_in *dst4;
	u_int8_t tos;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct in6_ifaddr *ia6;
	static const uint32_t af = AF_INET6;

	sc = (struct stf_softc*)ifp;
	dst6 = (struct sockaddr_in6 *)dst;

	/* just in case */
	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return ENETDOWN;
	}

	/*
	 * If we don't have an ip4 address that match my inner ip6 address,
	 * we shouldn't generate output.  Without this check, we'll end up
	 * using wrong IPv4 source.
	 */
	ia6 = stf_getsrcifa6(ifp);
	if (ia6 == NULL) {
		m_freem(m);
		return ENETDOWN;
	}

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m)
			return ENOBUFS;
	}
	ip6 = mtod(m, struct ip6_hdr *);
	tos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;

	/*
	 * Pickup the right outer dst addr from the list of candidates.
	 * ip6_dst has priority as it may be able to give us shorter IPv4 hops.
	 */
	if (IN6_IS_ADDR_6TO4(&ip6->ip6_dst))
		in4 = GET_V4(&ip6->ip6_dst);
	else if (IN6_IS_ADDR_6TO4(&dst6->sin6_addr))
		in4 = GET_V4(&dst6->sin6_addr);
	else {
		m_freem(m);
		return ENETUNREACH;
	}

	if (ifp->if_bpf)
		bpf_ptap(ifp->if_bpf, m, &af, sizeof(af));

	M_PREPEND(m, sizeof(struct ip), MB_DONTWAIT);
	if (m && m->m_len < sizeof(struct ip))
		m = m_pullup(m, sizeof(struct ip));
	if (m == NULL)
		return ENOBUFS;
	ip = mtod(m, struct ip *);

	bzero(ip, sizeof(*ip));

	bcopy(GET_V4(&((struct sockaddr_in6 *)&ia6->ia_addr)->sin6_addr),
	    &ip->ip_src, sizeof(ip->ip_src));
	bcopy(in4, &ip->ip_dst, sizeof(ip->ip_dst));
	ip->ip_p = IPPROTO_IPV6;
	ip->ip_ttl = ip_stf_ttl;
	ip->ip_len = m->m_pkthdr.len;	/*host order*/
	if (ifp->if_flags & IFF_LINK1)
		ip_ecn_ingress(ECN_ALLOWED, &ip->ip_tos, &tos);
	else
		ip_ecn_ingress(ECN_NOCARE, &ip->ip_tos, &tos);

	dst4 = (struct sockaddr_in *)&sc->sc_ro.ro_dst;
	if (dst4->sin_family != AF_INET ||
	    bcmp(&dst4->sin_addr, &ip->ip_dst, sizeof(ip->ip_dst)) != 0) {
		/* cache route doesn't match */
		dst4->sin_family = AF_INET;
		dst4->sin_len = sizeof(struct sockaddr_in);
		bcopy(&ip->ip_dst, &dst4->sin_addr, sizeof(dst4->sin_addr));
		if (sc->sc_ro.ro_rt) {
			RTFREE(sc->sc_ro.ro_rt);
			sc->sc_ro.ro_rt = NULL;
		}
	}

	if (sc->sc_ro.ro_rt == NULL) {
		rtalloc(&sc->sc_ro);
		if (sc->sc_ro.ro_rt == NULL) {
			m_freem(m);
			return ENETUNREACH;
		}
	}

	return ip_output(m, NULL, &sc->sc_ro, 0, NULL, NULL);
}

static int
stf_checkaddr4(sc, in, inifp)
	struct stf_softc *sc;
	struct in_addr *in;
	struct ifnet *inifp;	/* incoming interface */
{
	struct in_ifaddr *ia4;

	/*
	 * reject packets with the following address:
	 * 224.0.0.0/4 0.0.0.0/8 127.0.0.0/8 255.0.0.0/8
	 */
	if (IN_MULTICAST(ntohl(in->s_addr)))
		return -1;
	switch ((ntohl(in->s_addr) & 0xff000000) >> 24) {
	case 0: case 127: case 255:
		return -1;
	}

	/*
	 * reject packets with broadcast
	 */
	for (ia4 = TAILQ_FIRST(&in_ifaddrhead);
	     ia4;
	     ia4 = TAILQ_NEXT(ia4, ia_link))
	{
		if ((ia4->ia_ifa.ifa_ifp->if_flags & IFF_BROADCAST) == 0)
			continue;
		if (in->s_addr == ia4->ia_broadaddr.sin_addr.s_addr)
			return -1;
	}

	/*
	 * perform ingress filter
	 */
	if (sc && (sc->sc_if.if_flags & IFF_LINK2) == 0 && inifp) {
		struct sockaddr_in sin;
		struct rtentry *rt;

		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_addr = *in;
		rt = rtpurelookup((struct sockaddr *)&sin);
		if (!rt || rt->rt_ifp != inifp) {
#if 0
			log(LOG_WARNING, "%s: packet from 0x%x dropped "
			    "due to ingress filter\n", if_name(&sc->sc_if),
			    (u_int32_t)ntohl(sin.sin_addr.s_addr));
#endif
			if (rt)
				rtfree(rt);
			return -1;
		}
		rtfree(rt);
	}

	return 0;
}

static int
stf_checkaddr6(sc, in6, inifp)
	struct stf_softc *sc;
	struct in6_addr *in6;
	struct ifnet *inifp;	/* incoming interface */
{
	/*
	 * check 6to4 addresses
	 */
	if (IN6_IS_ADDR_6TO4(in6))
		return stf_checkaddr4(sc, GET_V4(in6), inifp);

	/*
	 * reject anything that look suspicious.  the test is implemented
	 * in ip6_input too, but we check here as well to
	 * (1) reject bad packets earlier, and
	 * (2) to be safe against future ip6_input change.
	 */
	if (IN6_IS_ADDR_V4COMPAT(in6) || IN6_IS_ADDR_V4MAPPED(in6))
		return -1;

	return 0;
}

void
in_stf_input(struct mbuf *m, ...)
{
	struct stf_softc *sc;
	struct ip *ip;
	struct ip6_hdr *ip6;
	u_int8_t otos, itos;
	struct ifnet *ifp;
	int off, proto;
	static const uint32_t af = AF_INET6;
	__va_list ap;

	__va_start(ap, m);
	off = __va_arg(ap, int);
	proto = __va_arg(ap, int);
	__va_end(ap);

	if (proto != IPPROTO_IPV6) {
		m_freem(m);
		return;
	}

	ip = mtod(m, struct ip *);

	sc = (struct stf_softc *)encap_getarg(m);

	if (sc == NULL || (sc->sc_if.if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	ifp = &sc->sc_if;

	/*
	 * perform sanity check against outer src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr4(sc, &ip->ip_dst, NULL) < 0 ||
	    stf_checkaddr4(sc, &ip->ip_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	otos = ip->ip_tos;
	m_adj(m, off);

	if (m->m_len < sizeof(*ip6)) {
		m = m_pullup(m, sizeof(*ip6));
		if (!m)
			return;
	}
	ip6 = mtod(m, struct ip6_hdr *);

	/*
	 * perform sanity check against inner src/dst.
	 * for source, perform ingress filter as well.
	 */
	if (stf_checkaddr6(sc, &ip6->ip6_dst, NULL) < 0 ||
	    stf_checkaddr6(sc, &ip6->ip6_src, m->m_pkthdr.rcvif) < 0) {
		m_freem(m);
		return;
	}

	itos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
	if ((ifp->if_flags & IFF_LINK1) != 0)
		ip_ecn_egress(ECN_ALLOWED, &otos, &itos);
	else
		ip_ecn_egress(ECN_NOCARE, &otos, &itos);
	ip6->ip6_flow &= ~htonl(0xff << 20);
	ip6->ip6_flow |= htonl((u_int32_t)itos << 20);

	m->m_pkthdr.rcvif = ifp;

	if (ifp->if_bpf)
		bpf_ptap(ifp->if_bpf, m, &af, sizeof(af));

	/*
	 * Put the packet to the network layer input queue according to the
	 * specified address family.
	 * See net/if_gif.c for possible issues with packet processing
	 * reorder due to extra queueing.
	 */
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	netisr_dispatch(NETISR_IPV6, m);
}

/* ARGSUSED */
static void
stf_rtrequest(cmd, rt, info)
	int cmd;
	struct rtentry *rt;
	struct rt_addrinfo *info;
{

	if (rt)
		rt->rt_rmx.rmx_mtu = IPV6_MMTU;
}

static int
stf_ioctl(ifp, cmd, data, cr)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
	struct ucred *cr;
{
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct sockaddr_in6 *sin6;
	int error;

	error = 0;
	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		if (ifa == NULL || ifa->ifa_addr->sa_family != AF_INET6) {
			error = EAFNOSUPPORT;
			break;
		}
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_6TO4(&sin6->sin6_addr)) {
			ifa->ifa_rtrequest = stf_rtrequest;
			ifp->if_flags |= IFF_UP;
		} else
			error = EINVAL;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		if (ifr && ifr->ifr_addr.sa_family == AF_INET6)
			;
		else
			error = EAFNOSUPPORT;
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}
