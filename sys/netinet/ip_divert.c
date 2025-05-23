/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 * $FreeBSD: src/sys/netinet/ip_divert.c,v 1.42.2.6 2003/01/23 21:06:45 sam Exp $
 * $DragonFly: src/sys/netinet/ip_divert.c,v 1.21 2005/02/17 14:00:00 joerg Exp $
 */

#include "opt_inet.h"
#include "opt_ipfw.h"
#include "opt_ipdivert.h"
#include "opt_ipsec.h"

#ifndef INET
#error "IPDIVERT requires INET."
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm_zone.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

/*
 * Divert sockets
 */

/*
 * Allocate enough space to hold a full IP packet
 */
#define	DIVSNDQ		(65536 + 100)
#define	DIVRCVQ		(65536 + 100)

/*
 * Divert sockets work in conjunction with ipfw, see the divert(4)
 * manpage for features.
 * Internally, packets selected by ipfw in ip_input() or ip_output(),
 * and never diverted before, are passed to the input queue of the
 * divert socket with a given 'divert_port' number (as specified in
 * the matching ipfw rule), and they are tagged with a 16 bit cookie
 * (representing the rule number of the matching ipfw rule), which
 * is passed to process reading from the socket.
 *
 * Packets written to the divert socket are again tagged with a cookie
 * (usually the same as above) and a destination address.
 * If the destination address is INADDR_ANY then the packet is
 * treated as outgoing and sent to ip_output(), otherwise it is
 * treated as incoming and sent to ip_input().
 * In both cases, the packet is tagged with the cookie.
 *
 * On reinjection, processing in ip_input() and ip_output()
 * will be exactly the same as for the original packet, except that
 * ipfw processing will start at the rule number after the one
 * written in the cookie (so, tagging a packet with a cookie of 0
 * will cause it to be effectively considered as a standard packet).
 */

/* Internal variables */
static struct inpcbinfo divcbinfo;

static u_long	div_sendspace = DIVSNDQ;	/* XXX sysctl ? */
static u_long	div_recvspace = DIVRCVQ;	/* XXX sysctl ? */

/*
 * Initialize divert connection block queue.
 */
void
div_init(void)
{
	in_pcbinfo_init(&divcbinfo);
	/*
	 * XXX We don't use the hash list for divert IP, but it's easier
	 * to allocate a one entry hash list than it is to check all
	 * over the place for hashbase == NULL.
	 */
	divcbinfo.hashbase = hashinit(1, M_PCB, &divcbinfo.hashmask);
	divcbinfo.porthashbase = hashinit(1, M_PCB, &divcbinfo.porthashmask);
	divcbinfo.wildcardhashbase = hashinit(1, M_PCB,
					      &divcbinfo.wildcardhashmask);
	divcbinfo.ipi_zone = zinit("divcb", sizeof(struct inpcb),
				   maxsockets, ZONE_INTERRUPT, 0);
}

/*
 * IPPROTO_DIVERT is not a real IP protocol; don't allow any packets
 * with that protocol number to enter the system from the outside.
 */
void
div_input(struct mbuf *m, ...)
{
	ipstat.ips_noproto++;
	m_freem(m);
}

/*
 * Divert a packet by passing it up to the divert socket at port 'port'.
 *
 * Setup generic address and protocol structures for div_input routine,
 * then pass them along with mbuf chain.
 */
void
divert_packet(struct mbuf *m, int incoming, int port, int rule)
{
	struct sockaddr_in divsrc = { sizeof divsrc, AF_INET };
	struct ip *ip;
	struct inpcb *inp;
	struct socket *sa;
	u_int16_t nport;

	/* Sanity check */
	KASSERT(port != 0, ("%s: port=0", __func__));

	divsrc.sin_port = rule;		/* record matching rule */

	/* Assure header */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		return;
	ip = mtod(m, struct ip *);

	/*
	 * Record receive interface address, if any.
	 * But only for incoming packets.
	 */
	divsrc.sin_addr.s_addr = 0;
	if (incoming) {
		struct ifaddr *ifa;

		/* Sanity check */
		KASSERT((m->m_flags & M_PKTHDR), ("%s: !PKTHDR", __func__));

		/* Find IP address for receive interface */
		TAILQ_FOREACH(ifa, &m->m_pkthdr.rcvif->if_addrhead, ifa_link) {
			if (ifa->ifa_addr == NULL)
				continue;
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			divsrc.sin_addr =
			    ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
			break;
		}
	}
	/*
	 * Record the incoming interface name whenever we have one.
	 */
	if (m->m_pkthdr.rcvif) {
		/*
		 * Hide the actual interface name in there in the
		 * sin_zero array. XXX This needs to be moved to a
		 * different sockaddr type for divert, e.g.
		 * sockaddr_div with multiple fields like
		 * sockaddr_dl. Presently we have only 7 bytes
		 * but that will do for now as most interfaces
		 * are 4 or less + 2 or less bytes for unit.
		 * There is probably a faster way of doing this,
		 * possibly taking it from the sockaddr_dl on the iface.
		 * This solves the problem of a P2P link and a LAN interface
		 * having the same address, which can result in the wrong
		 * interface being assigned to the packet when fed back
		 * into the divert socket. Theoretically if the daemon saves
		 * and re-uses the sockaddr_in as suggested in the man pages,
		 * this iface name will come along for the ride.
		 * (see div_output for the other half of this.)
		 */
		snprintf(divsrc.sin_zero, sizeof divsrc.sin_zero,
			 m->m_pkthdr.rcvif->if_xname);
	}

	/* Put packet on socket queue, if any */
	sa = NULL;
	nport = htons((u_int16_t)port);
	LIST_FOREACH(inp, &divcbinfo.pcblisthead, inp_list) {
		if (inp->inp_flags & INP_PLACEMARKER)
			continue;
		if (inp->inp_lport == nport)
			sa = inp->inp_socket;
	}
	if (sa) {
		if (sbappendaddr(&sa->so_rcv, (struct sockaddr *)&divsrc, m,
				 (struct mbuf *)NULL) == 0)
			m_freem(m);
		else
			sorwakeup(sa);
	} else {
		m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}
}

/*
 * Deliver packet back into the IP processing machinery.
 *
 * If no address specified, or address is 0.0.0.0, send to ip_output();
 * otherwise, send to ip_input() and mark as having been received on
 * the interface with that address.
 */
static int
div_output(struct socket *so, struct mbuf *m,
	struct sockaddr_in *sin, struct mbuf *control)
{
	int error = 0;
	struct m_hdr divert_tag;

	/*
	 * Prepare the tag for divert info. Note that a packet
	 * with a 0 tag in mh_data is effectively untagged,
	 * so we could optimize that case.
	 */
	divert_tag.mh_type = MT_TAG;
	divert_tag.mh_flags = PACKET_TAG_DIVERT;
	divert_tag.mh_next = m;
	divert_tag.mh_data = 0;		/* the matching rule # */
	m->m_pkthdr.rcvif = NULL;	/* XXX is it necessary ? */

	if (control)
		m_freem(control);		/* XXX */

	/* Loopback avoidance and state recovery */
	if (sin) {
		int i;

		divert_tag.mh_data = (caddr_t)(int)sin->sin_port;
		/*
		 * Find receive interface with the given name, stuffed
		 * (if it exists) in the sin_zero[] field.
		 * The name is user supplied data so don't trust its size
		 * or that it is zero terminated.
		 */
		for (i = 0; sin->sin_zero[i] && i < sizeof sin->sin_zero; i++)
			;
		if ( i > 0 && i < sizeof sin->sin_zero)
			m->m_pkthdr.rcvif = ifunit(sin->sin_zero);
	}

	/* Reinject packet into the system as incoming or outgoing */
	if (!sin || sin->sin_addr.s_addr == 0) {
		struct inpcb *const inp = so->so_pcb;
		struct ip *const ip = mtod(m, struct ip *);

		/*
		 * Don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash
		 */
		if (((ip->ip_hl != (sizeof *ip) >> 2) && inp->inp_options) ||
		     ((u_short)ntohs(ip->ip_len) > m->m_pkthdr.len)) {
			error = EINVAL;
			goto cantsend;
		}

		/* Convert fields to host order for ip_output() */
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);

		/* Send packet to output processing */
		ipstat.ips_rawout++;			/* XXX */
		error = ip_output((struct mbuf *)&divert_tag,
			    inp->inp_options, &inp->inp_route,
			    (so->so_options & SO_DONTROUTE) |
			    IP_ALLOWBROADCAST | IP_RAWOUTPUT,
			    inp->inp_moptions, NULL);
	} else {
		if (m->m_pkthdr.rcvif == NULL) {
			/*
			 * No luck with the name, check by IP address.
			 * Clear the port and the ifname to make sure
			 * there are no distractions for ifa_ifwithaddr.
			 */
			struct	ifaddr *ifa;

			bzero(sin->sin_zero, sizeof sin->sin_zero);
			sin->sin_port = 0;
			ifa = ifa_ifwithaddr((struct sockaddr *) sin);
			if (ifa == NULL) {
				error = EADDRNOTAVAIL;
				goto cantsend;
			}
			m->m_pkthdr.rcvif = ifa->ifa_ifp;
		}
		ip_input((struct mbuf *)&divert_tag);
	}

	return error;

cantsend:
	m_freem(m);
	return error;
}

static int
div_attach(struct socket *so, int proto, struct pru_attach_info *ai)
{
	struct inpcb *inp;
	int error, s;

	inp  = so->so_pcb;
	if (inp)
		panic("div_attach");
	if ((error = suser_cred(ai->p_ucred, NULL_CRED_OKAY)) != 0)
		return error;

	error = soreserve(so, div_sendspace, div_recvspace, ai->sb_rlimit);
	if (error)
		return error;
	s = splnet();
	error = in_pcballoc(so, &divcbinfo);
	splx(s);
	if (error)
		return error;
	inp = (struct inpcb *)so->so_pcb;
	inp->inp_ip_p = proto;
	inp->inp_vflag |= INP_IPV4;
	inp->inp_flags |= INP_HDRINCL;
	/*
	 * The socket is always "connected" because
	 * we always know "where" to send the packet.
	 */
	so->so_state |= SS_ISCONNECTED;
	return 0;
}

static int
div_detach(struct socket *so)
{
	struct inpcb *inp;

	inp = so->so_pcb;
	if (inp == NULL)
		panic("div_detach");
	in_pcbdetach(inp);
	return 0;
}

static int
div_abort(struct socket *so)
{
	soisdisconnected(so);
	return div_detach(so);
}

static int
div_disconnect(struct socket *so)
{
	if (!(so->so_state & SS_ISCONNECTED))
		return ENOTCONN;
	return div_abort(so);
}

static int
div_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp;
	int s;
	int error;

	s = splnet();
	inp = so->so_pcb;
	/* in_pcbbind assumes that nam is a sockaddr_in
	 * and in_pcbbind requires a valid address. Since divert
	 * sockets don't we need to make sure the address is
	 * filled in properly.
	 * XXX -- divert should not be abusing in_pcbind
	 * and should probably have its own family.
	 */
	if (nam->sa_family != AF_INET)
		error = EAFNOSUPPORT;
	else {
		((struct sockaddr_in *)nam)->sin_addr.s_addr = INADDR_ANY;
		error = in_pcbbind(inp, nam, td);
	}
	splx(s);
	return error;
}

static int
div_shutdown(struct socket *so)
{
	socantsendmore(so);
	return 0;
}

static int
div_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct thread *td)
{
	/* Packet must have a header (but that's about it) */
	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL) {
		ipstat.ips_toosmall++;
		m_freem(m);
		return EINVAL;
	}

	/* Send packet */
	return div_output(so, m, (struct sockaddr_in *)nam, control);
}

SYSCTL_DECL(_net_inet_divert);
SYSCTL_PROC(_net_inet_divert, OID_AUTO, pcblist, CTLFLAG_RD, &divcbinfo, 0,
	    in_pcblist_global, "S,xinpcb", "List of active divert sockets");

struct pr_usrreqs div_usrreqs = {
	div_abort, pru_accept_notsupp, div_attach, div_bind,
	pru_connect_notsupp, pru_connect2_notsupp, in_control, div_detach,
	div_disconnect, pru_listen_notsupp, in_setpeeraddr, pru_rcvd_notsupp,
	pru_rcvoob_notsupp, div_send, pru_sense_null, div_shutdown,
	in_setsockaddr, sosend, soreceive, sopoll
};
