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
 *	From: @(#)tcp_usrreq.c	8.2 (Berkeley) 1/3/94
 * $FreeBSD: src/sys/netinet/tcp_usrreq.c,v 1.51.2.17 2002/10/11 11:46:44 ume Exp $
 * $DragonFly: src/sys/netinet/tcp_usrreq.c,v 1.35 2005/04/05 07:08:52 dillon Exp $
 */

#include "opt_ipsec.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/globaldata.h>
#include <sys/thread.h>

#include <sys/mbuf.h>
#ifdef INET6
#include <sys/domain.h>
#endif /* INET6 */
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <sys/msgport2.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#ifdef INET6
#include <netinet/ip6.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

/*
 * TCP protocol interface to socket abstraction.
 */
extern	char *tcpstates[];	/* XXX ??? */

static int	tcp_attach (struct socket *, struct pru_attach_info *);
static int	tcp_connect (struct tcpcb *, struct sockaddr *,
				 struct thread *);
#ifdef INET6
static int	tcp6_connect (struct tcpcb *, struct sockaddr *,
				 struct thread *);
#endif /* INET6 */
static struct tcpcb *
		tcp_disconnect (struct tcpcb *);
static struct tcpcb *
		tcp_usrclosed (struct tcpcb *);

#ifdef TCPDEBUG
#define	TCPDEBUG0	int ostate = 0
#define	TCPDEBUG1()	ostate = tp ? tp->t_state : 0
#define	TCPDEBUG2(req)	if (tp && (so->so_options & SO_DEBUG)) \
				tcp_trace(TA_USER, ostate, tp, 0, 0, req)
#else
#define	TCPDEBUG0
#define	TCPDEBUG1()
#define	TCPDEBUG2(req)
#endif

/*
 * TCP attaches to socket via pru_attach(), reserving space,
 * and an internet control block.
 */
static int
tcp_usr_attach(struct socket *so, int proto, struct pru_attach_info *ai)
{
	int s = splnet();
	int error;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp = 0;
	TCPDEBUG0;

	TCPDEBUG1();
	if (inp) {
		error = EISCONN;
		goto out;
	}

	error = tcp_attach(so, ai);
	if (error)
		goto out;

	if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		so->so_linger = TCP_LINGERTIME;
	tp = sototcpcb(so);
out:
	TCPDEBUG2(PRU_ATTACH);
	splx(s);
	return error;
}

/*
 * pru_detach() detaches the TCP protocol from the socket.
 * If the protocol state is non-embryonic, then can't
 * do this directly: have to initiate a pru_disconnect(),
 * which may finish later; embryonic TCB's can just
 * be discarded here.
 */
static int
tcp_usr_detach(struct socket *so)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
	TCPDEBUG0;

	if (inp == NULL) {
		splx(s);
		return EINVAL;	/* XXX */
	}

	/*
	 * It's possible for the tcpcb (tp) to disconnect from the inp due
	 * to tcp_drop()->tcp_close() being called.  This may occur *after*
	 * the detach message has been queued so we may find a NULL tp here.
	 */
	if ((tp = intotcpcb(inp)) != NULL) {
		TCPDEBUG1();
		tp = tcp_disconnect(tp);
		TCPDEBUG2(PRU_DETACH);
	}
	splx(s);
	return error;
}

#define	COMMON_START()	TCPDEBUG0; \
			do { \
				     if (inp == 0) { \
					     splx(s); \
					     return EINVAL; \
				     } \
				     tp = intotcpcb(inp); \
				     TCPDEBUG1(); \
		     } while(0)

#define COMMON_END(req)	out: TCPDEBUG2(req); splx(s); return error; goto out


/*
 * Give the socket an address.
 */
static int
tcp_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;

	COMMON_START();

	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	sinp = (struct sockaddr_in *)nam;
	if (sinp->sin_family == AF_INET &&
	    IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
		error = EAFNOSUPPORT;
		goto out;
	}
	error = in_pcbbind(inp, nam, td);
	if (error)
		goto out;
	COMMON_END(PRU_BIND);

}

#ifdef INET6
static int
tcp6_usr_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6p;

	COMMON_START();

	/*
	 * Must check for multicast addresses and disallow binding
	 * to them.
	 */
	sin6p = (struct sockaddr_in6 *)nam;
	if (sin6p->sin6_family == AF_INET6 &&
	    IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr)) {
		error = EAFNOSUPPORT;
		goto out;
	}
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6p->sin6_addr))
			inp->inp_vflag |= INP_IPV4;
		else if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
			struct sockaddr_in sin;

			in6_sin6_2_sin(&sin, sin6p);
			inp->inp_vflag |= INP_IPV4;
			inp->inp_vflag &= ~INP_IPV6;
			error = in_pcbbind(inp, (struct sockaddr *)&sin, td);
			goto out;
		}
	}
	error = in6_pcbbind(inp, nam, td);
	if (error)
		goto out;
	COMMON_END(PRU_BIND);
}
#endif /* INET6 */

#ifdef SMP
struct netmsg_inswildcard {
	struct lwkt_msg		nm_lmsg;
	struct inpcb		*nm_inp;
	struct inpcbinfo	*nm_pcbinfo;
};

static int
in_pcbinswildcardhash_handler(struct lwkt_msg *msg0)
{
	struct netmsg_inswildcard *msg = (struct netmsg_inswildcard *)msg0;

	in_pcbinswildcardhash_oncpu(msg->nm_inp, msg->nm_pcbinfo);
	lwkt_replymsg(&msg->nm_lmsg, 0);
	return (EASYNC);
}
#endif

/*
 * Prepare to accept connections.
 */
static int
tcp_usr_listen(struct socket *so, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
#ifdef SMP
	int cpu;
#endif

	COMMON_START();
	if (inp->inp_lport == 0) {
		error = in_pcbbind(inp, NULL, td);
		if (error != 0)
			goto out;
	}

	tp->t_state = TCPS_LISTEN;
#ifdef SMP
	/*
	 * We have to set the flag because we can't have other cpus
	 * messing with our inp's flags.
	 */
	inp->inp_flags |= INP_WILDCARD_MP;
	for (cpu = 0; cpu < ncpus2; cpu++) {
		struct netmsg_inswildcard *msg;

		if (cpu == mycpu->gd_cpuid) {
			in_pcbinswildcardhash(inp);
			continue;
		}

		msg = malloc(sizeof(struct netmsg_inswildcard), M_LWKTMSG,
		    M_INTWAIT);
		lwkt_initmsg(&msg->nm_lmsg, &netisr_afree_rport, 0,
		    lwkt_cmd_func(in_pcbinswildcardhash_handler),
		    lwkt_cmd_op_none);
		msg->nm_inp = inp;
		msg->nm_pcbinfo = &tcbinfo[cpu];
		lwkt_sendmsg(tcp_cport(cpu), &msg->nm_lmsg);
	}
#else
	in_pcbinswildcardhash(inp);
#endif
	COMMON_END(PRU_LISTEN);
}

#ifdef INET6
static int
tcp6_usr_listen(struct socket *so, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
#ifdef SMP
	int cpu;
#endif

	COMMON_START();
	if (inp->inp_lport == 0) {
		if (!(inp->inp_flags & IN6P_IPV6_V6ONLY))
			inp->inp_vflag |= INP_IPV4;
		else
			inp->inp_vflag &= ~INP_IPV4;
		error = in6_pcbbind(inp, (struct sockaddr *)0, td);
	}
	if (error == 0)
		tp->t_state = TCPS_LISTEN;
#ifdef SMP
	/*
	 * We have to set the flag because we can't have other cpus
	 * messing with our inp's flags.
	 */
	inp->inp_flags |= INP_WILDCARD_MP;
	for (cpu = 0; cpu < ncpus2; cpu++) {
		struct netmsg_inswildcard *msg;

		if (cpu == mycpu->gd_cpuid) {
			in_pcbinswildcardhash(inp);
			continue;
		}

		msg = malloc(sizeof(struct netmsg_inswildcard), M_LWKTMSG,
		    M_INTWAIT);
		lwkt_initmsg(&msg->nm_lmsg, &netisr_afree_rport, 0,
		    lwkt_cmd_func(in_pcbinswildcardhash_handler),
		    lwkt_cmd_op_none);
		msg->nm_inp = inp;
		msg->nm_pcbinfo = &tcbinfo[cpu];
		lwkt_sendmsg(tcp_cport(cpu), &msg->nm_lmsg);
	}
#else
	in_pcbinswildcardhash(inp);
#endif
	COMMON_END(PRU_LISTEN);
}
#endif /* INET6 */

/*
 * Initiate connection to peer.
 * Create a template for use in transmissions on this connection.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, and seed output sequence space.
 * Send initial segment on connection.
 */
static int
tcp_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
	struct sockaddr_in *sinp;

	COMMON_START();

	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	sinp = (struct sockaddr_in *)nam;
	if (sinp->sin_family == AF_INET
	    && IN_MULTICAST(ntohl(sinp->sin_addr.s_addr))) {
		error = EAFNOSUPPORT;
		goto out;
	}

	prison_remote_ip(td, 0, &sinp->sin_addr.s_addr);

	if ((error = tcp_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output(tp);
	COMMON_END(PRU_CONNECT);
}

#ifdef INET6
static int
tcp6_usr_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
	struct sockaddr_in6 *sin6p;

	COMMON_START();

	/*
	 * Must disallow TCP ``connections'' to multicast addresses.
	 */
	sin6p = (struct sockaddr_in6 *)nam;
	if (sin6p->sin6_family == AF_INET6
	    && IN6_IS_ADDR_MULTICAST(&sin6p->sin6_addr)) {
		error = EAFNOSUPPORT;
		goto out;
	}

	if (IN6_IS_ADDR_V4MAPPED(&sin6p->sin6_addr)) {
		struct sockaddr_in sin;

		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) != 0) {
			error = EINVAL;
			goto out;
		}

		in6_sin6_2_sin(&sin, sin6p);
		inp->inp_vflag |= INP_IPV4;
		inp->inp_vflag &= ~INP_IPV6;
		if ((error = tcp_connect(tp, (struct sockaddr *)&sin, td)) != 0)
			goto out;
		error = tcp_output(tp);
		goto out;
	}
	inp->inp_vflag &= ~INP_IPV4;
	inp->inp_vflag |= INP_IPV6;
	inp->inp_inc.inc_isipv6 = 1;
	if ((error = tcp6_connect(tp, nam, td)) != 0)
		goto out;
	error = tcp_output(tp);
	COMMON_END(PRU_CONNECT);
}
#endif /* INET6 */

/*
 * Initiate disconnect from peer.
 * If connection never passed embryonic stage, just drop;
 * else if don't need to let data drain, then can just drop anyways,
 * else have to begin TCP shutdown process: mark socket disconnecting,
 * drain unread data, state switch to reflect user close, and
 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
 * when peer sends FIN and acks ours.
 *
 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
 */
static int
tcp_usr_disconnect(struct socket *so)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;

	COMMON_START();
	tp = tcp_disconnect(tp);
	COMMON_END(PRU_DISCONNECT);
}

/*
 * Accept a connection.  Essentially all the work is
 * done at higher levels; just return the address
 * of the peer, storing through addr.
 */
static int
tcp_usr_accept(struct socket *so, struct sockaddr **nam)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp = NULL;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED) {
		error = ECONNABORTED;
		goto out;
	}
	if (inp == 0) {
		splx(s);
		return (EINVAL);
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	in_setpeeraddr(so, nam);
	COMMON_END(PRU_ACCEPT);
}

#ifdef INET6
static int
tcp6_usr_accept(struct socket *so, struct sockaddr **nam)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp = NULL;
	TCPDEBUG0;

	if (so->so_state & SS_ISDISCONNECTED) {
		error = ECONNABORTED;
		goto out;
	}
	if (inp == 0) {
		splx(s);
		return (EINVAL);
	}
	tp = intotcpcb(inp);
	TCPDEBUG1();
	in6_mapped_peeraddr(so, nam);
	COMMON_END(PRU_ACCEPT);
}
#endif /* INET6 */
/*
 * Mark the connection as being incapable of further output.
 */
static int
tcp_usr_shutdown(struct socket *so)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;

	COMMON_START();
	socantsendmore(so);
	tp = tcp_usrclosed(tp);
	if (tp)
		error = tcp_output(tp);
	COMMON_END(PRU_SHUTDOWN);
}

/*
 * After a receive, possibly send window update to peer.
 */
static int
tcp_usr_rcvd(struct socket *so, int flags)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;

	COMMON_START();
	tcp_output(tp);
	COMMON_END(PRU_RCVD);
}

/*
 * Do a send by putting data in output queue and updating urgent
 * marker if URG set.  Possibly send more data.  Unlike the other
 * pru_*() routines, the mbuf chains are our responsibility.  We
 * must either enqueue them or free them.  The other pru_* routines
 * generally are caller-frees.
 */
static int
tcp_usr_send(struct socket *so, int flags, struct mbuf *m,
	     struct sockaddr *nam, struct mbuf *control, struct thread *td)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;
#ifdef INET6
	int isipv6;
#endif
	TCPDEBUG0;

	if (inp == NULL) {
		/*
		 * OOPS! we lost a race, the TCP session got reset after
		 * we checked SS_CANTSENDMORE, eg: while doing uiomove or a
		 * network interrupt in the non-splnet() section of sosend().
		 */
		if (m)
			m_freem(m);
		if (control)
			m_freem(control);
		error = ECONNRESET;	/* XXX EPIPE? */
		tp = NULL;
		TCPDEBUG1();
		goto out;
	}
#ifdef INET6
	isipv6 = nam && nam->sa_family == AF_INET6;
#endif /* INET6 */
	tp = intotcpcb(inp);
	TCPDEBUG1();
	if (control) {
		/* TCP doesn't do control messages (rights, creds, etc) */
		if (control->m_len) {
			m_freem(control);
			if (m)
				m_freem(m);
			error = EINVAL;
			goto out;
		}
		m_freem(control);	/* empty control, just free it */
	}
	if(!(flags & PRUS_OOB)) {
		sbappendstream(&so->so_snd, m);
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
			else
#endif /* INET6 */
			error = tcp_connect(tp, nam, td);
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}

		if (flags & PRUS_EOF) {
			/*
			 * Close the send side of the connection after
			 * the data is sent.
			 */
			socantsendmore(so);
			tp = tcp_usrclosed(tp);
		}
		if (tp != NULL) {
			if (flags & PRUS_MORETOCOME)
				tp->t_flags |= TF_MORETOCOME;
			error = tcp_output(tp);
			if (flags & PRUS_MORETOCOME)
				tp->t_flags &= ~TF_MORETOCOME;
		}
	} else {
		if (sbspace(&so->so_snd) < -512) {
			m_freem(m);
			error = ENOBUFS;
			goto out;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappendstream(&so->so_snd, m);
		if (nam && tp->t_state < TCPS_SYN_SENT) {
			/*
			 * Do implied connect if not yet connected,
			 * initialize window to default value, and
			 * initialize maxseg/maxopd using peer's cached
			 * MSS.
			 */
#ifdef INET6
			if (isipv6)
				error = tcp6_connect(tp, nam, td);
			else
#endif /* INET6 */
			error = tcp_connect(tp, nam, td);
			if (error)
				goto out;
			tp->snd_wnd = TTCP_CLIENT_SND_WND;
			tcp_mss(tp, -1);
		}
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_flags |= TF_FORCE;
		error = tcp_output(tp);
		tp->t_flags &= ~TF_FORCE;
	}
	COMMON_END((flags & PRUS_OOB) ? PRU_SENDOOB :
		   ((flags & PRUS_EOF) ? PRU_SEND_EOF : PRU_SEND));
}

/*
 * Abort the TCP.
 */
static int
tcp_usr_abort(struct socket *so)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;

	COMMON_START();
	tp = tcp_drop(tp, ECONNABORTED);
	COMMON_END(PRU_ABORT);
}

/*
 * Receive out-of-band data.
 */
static int
tcp_usr_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	int s = splnet();
	int error = 0;
	struct inpcb *inp = so->so_pcb;
	struct tcpcb *tp;

	COMMON_START();
	if ((so->so_oobmark == 0 &&
	     (so->so_state & SS_RCVATMARK) == 0) ||
	    so->so_options & SO_OOBINLINE ||
	    tp->t_oobflags & TCPOOB_HADDATA) {
		error = EINVAL;
		goto out;
	}
	if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
		error = EWOULDBLOCK;
		goto out;
	}
	m->m_len = 1;
	*mtod(m, caddr_t) = tp->t_iobc;
	if ((flags & MSG_PEEK) == 0)
		tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
	COMMON_END(PRU_RCVOOB);
}

/* xxx - should be const */
struct pr_usrreqs tcp_usrreqs = {
	tcp_usr_abort, tcp_usr_accept, tcp_usr_attach, tcp_usr_bind,
	tcp_usr_connect, pru_connect2_notsupp, in_control, tcp_usr_detach,
	tcp_usr_disconnect, tcp_usr_listen, in_setpeeraddr, tcp_usr_rcvd,
	tcp_usr_rcvoob, tcp_usr_send, pru_sense_null, tcp_usr_shutdown,
	in_setsockaddr, sosend, soreceive, sopoll
};

#ifdef INET6
struct pr_usrreqs tcp6_usrreqs = {
	tcp_usr_abort, tcp6_usr_accept, tcp_usr_attach, tcp6_usr_bind,
	tcp6_usr_connect, pru_connect2_notsupp, in6_control, tcp_usr_detach,
	tcp_usr_disconnect, tcp6_usr_listen, in6_mapped_peeraddr, tcp_usr_rcvd,
	tcp_usr_rcvoob, tcp_usr_send, pru_sense_null, tcp_usr_shutdown,
	in6_mapped_sockaddr, sosend, soreceive, sopoll
};
#endif /* INET6 */

static int
tcp_connect_oncpu(struct tcpcb *tp, struct sockaddr_in *sin,
		  struct sockaddr_in *if_sin)
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct tcpcb *otp;
	struct rmxp_tao *taop;
	struct rmxp_tao tao_noncached;

	oinp = in_pcblookup_hash(&tcbinfo[mycpu->gd_cpuid],
	    sin->sin_addr, sin->sin_port,
	    inp->inp_laddr.s_addr != INADDR_ANY ?
		inp->inp_laddr : if_sin->sin_addr,
	    inp->inp_lport, 0, NULL);
	if (oinp != NULL) {
		if (oinp != inp && (otp = intotcpcb(oinp)) != NULL &&
		    otp->t_state == TCPS_TIME_WAIT &&
		    (ticks - otp->t_starttime) < tcp_msl &&
		    (otp->t_flags & TF_RCVD_CC))
			tcp_close(otp);
		else
			return (EADDRINUSE);
	}
	if (inp->inp_laddr.s_addr == INADDR_ANY)
		inp->inp_laddr = if_sin->sin_addr;
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	inp->inp_cpcbinfo = &tcbinfo[mycpu->gd_cpuid];
	in_pcbinsconnhash(inp);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	tcpstat.tcps_connattempt++;
	tp->t_state = TCPS_SYN_SENT;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);
	tp->iss = tcp_new_isn(tp);
	tcp_sendseqinit(tp);

	/*
	 * Generate a CC value for this connection and
	 * check whether CC or CCnew should be used.
	 */
	if ((taop = tcp_gettaocache(&tp->t_inpcb->inp_inc)) == NULL) {
		taop = &tao_noncached;
		bzero(taop, sizeof *taop);
	}

	tp->cc_send = CC_INC(tcp_ccgen);
	if (taop->tao_ccsent != 0 &&
	    CC_GEQ(tp->cc_send, taop->tao_ccsent)) {
		taop->tao_ccsent = tp->cc_send;
	} else {
		taop->tao_ccsent = 0;
		tp->t_flags |= TF_SENDCCNEW;
	}

	return (0);
}

#ifdef SMP

struct netmsg_tcp_connect {
	struct lwkt_msg		nm_lmsg;
	struct tcpcb		*nm_tp;
	struct sockaddr_in	*nm_sin;
	struct sockaddr_in	*nm_ifsin;
};

static int
tcp_connect_handler(lwkt_msg_t lmsg)
{
	struct netmsg_tcp_connect *msg = (void *)lmsg;
	int error;

	error = tcp_connect_oncpu(msg->nm_tp, msg->nm_sin, msg->nm_ifsin);
	lwkt_replymsg(lmsg, error);
	return(EASYNC);
}

#endif

/*
 * Common subroutine to open a TCP connection to remote host specified
 * by struct sockaddr_in in mbuf *nam.  Call in_pcbbind to assign a local
 * port number if needed.  Call in_pcbladdr to do the routing and to choose
 * a local host address (interface).  If there is an existing incarnation
 * of the same connection in TIME-WAIT state and if the remote host was
 * sending CC options and if the connection duration was < MSL, then
 * truncate the previous TIME-WAIT state and proceed.
 * Initialize connection parameters and enter SYN-SENT state.
 */
static int
tcp_connect(struct tcpcb *tp, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = tp->t_inpcb;
	struct sockaddr_in *sin = (struct sockaddr_in *)nam;
	struct sockaddr_in *if_sin;
	int error;
#ifdef SMP
	lwkt_port_t port;
#endif

	if (inp->inp_lport == 0) {
		error = in_pcbbind(inp, (struct sockaddr *)NULL, td);
		if (error)
			return (error);
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 */
	error = in_pcbladdr(inp, nam, &if_sin);
	if (error)
		return (error);

#ifdef SMP
	port = tcp_addrport(sin->sin_addr.s_addr, sin->sin_port,
	    inp->inp_laddr.s_addr ?
		inp->inp_laddr.s_addr : if_sin->sin_addr.s_addr,
	    inp->inp_lport);

	if (port->mp_td != curthread) {
		struct netmsg_tcp_connect msg;

		lwkt_initmsg(&msg.nm_lmsg, &curthread->td_msgport, 0,
		    lwkt_cmd_func(tcp_connect_handler), lwkt_cmd_op_none);
		msg.nm_tp = tp;
		msg.nm_sin = sin;
		msg.nm_ifsin = if_sin;
		error = lwkt_domsg(port, &msg.nm_lmsg);
	} else
#endif
		error = tcp_connect_oncpu(tp, sin, if_sin);

	return (error);
}

#ifdef INET6
static int
tcp6_connect(struct tcpcb *tp, struct sockaddr *nam, struct thread *td)
{
	struct inpcb *inp = tp->t_inpcb, *oinp;
	struct socket *so = inp->inp_socket;
	struct tcpcb *otp;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
	struct in6_addr *addr6;
	struct rmxp_tao *taop;
	struct rmxp_tao tao_noncached;
	int error;

	if (inp->inp_lport == 0) {
		error = in6_pcbbind(inp, (struct sockaddr *)0, td);
		if (error)
			return error;
	}

	/*
	 * Cannot simply call in_pcbconnect, because there might be an
	 * earlier incarnation of this same connection still in
	 * TIME_WAIT state, creating an ADDRINUSE error.
	 */
	error = in6_pcbladdr(inp, nam, &addr6);
	if (error)
		return error;
	oinp = in6_pcblookup_hash(inp->inp_cpcbinfo,
				  &sin6->sin6_addr, sin6->sin6_port,
				  IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr) ?
				      addr6 : &inp->in6p_laddr,
				  inp->inp_lport,  0, NULL);
	if (oinp) {
		if (oinp != inp && (otp = intotcpcb(oinp)) != NULL &&
		    otp->t_state == TCPS_TIME_WAIT &&
		    (ticks - otp->t_starttime) < tcp_msl &&
		    (otp->t_flags & TF_RCVD_CC))
			otp = tcp_close(otp);
		else
			return (EADDRINUSE);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->in6p_laddr))
		inp->in6p_laddr = *addr6;
	inp->in6p_faddr = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	if ((sin6->sin6_flowinfo & IPV6_FLOWINFO_MASK) != NULL)
		inp->in6p_flowinfo = sin6->sin6_flowinfo;
	in_pcbinsconnhash(inp);

	/* Compute window scaling to request.  */
	while (tp->request_r_scale < TCP_MAX_WINSHIFT &&
	    (TCP_MAXWIN << tp->request_r_scale) < so->so_rcv.sb_hiwat)
		tp->request_r_scale++;

	soisconnecting(so);
	tcpstat.tcps_connattempt++;
	tp->t_state = TCPS_SYN_SENT;
	callout_reset(tp->tt_keep, tcp_keepinit, tcp_timer_keep, tp);
	tp->iss = tcp_new_isn(tp);
	tcp_sendseqinit(tp);

	/*
	 * Generate a CC value for this connection and
	 * check whether CC or CCnew should be used.
	 */
	if ((taop = tcp_gettaocache(&tp->t_inpcb->inp_inc)) == NULL) {
		taop = &tao_noncached;
		bzero(taop, sizeof *taop);
	}

	tp->cc_send = CC_INC(tcp_ccgen);
	if (taop->tao_ccsent != 0 &&
	    CC_GEQ(tp->cc_send, taop->tao_ccsent)) {
		taop->tao_ccsent = tp->cc_send;
	} else {
		taop->tao_ccsent = 0;
		tp->t_flags |= TF_SENDCCNEW;
	}

	return (0);
}
#endif /* INET6 */

/*
 * The new sockopt interface makes it possible for us to block in the
 * copyin/out step (if we take a page fault).  Taking a page fault at
 * splnet() is probably a Bad Thing.  (Since sockets and pcbs both now
 * use TSM, there probably isn't any need for this function to run at
 * splnet() any more.  This needs more examination.)
 */
int
tcp_ctloutput(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
{
	int	error, opt, optval, s;
	struct	inpcb *inp;
	struct	tcpcb *tp;

	error = 0;
	s = splnet();		/* XXX */
	inp = so->so_pcb;
	if (inp == NULL) {
		splx(s);
		return (ECONNRESET);
	}
	if (sopt->sopt_level != IPPROTO_TCP) {
#ifdef INET6
		if (INP_CHECK_SOCKAF(so, AF_INET6))
			error = ip6_ctloutput(so, sopt);
		else
#endif /* INET6 */
		error = ip_ctloutput(so, sopt);
		splx(s);
		return (error);
	}
	tp = intotcpcb(inp);

	switch (sopt->sopt_dir) {
	case SOPT_SET:
		switch (sopt->sopt_name) {
		case TCP_NODELAY:
		case TCP_NOOPT:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			switch (sopt->sopt_name) {
			case TCP_NODELAY:
				opt = TF_NODELAY;
				break;
			case TCP_NOOPT:
				opt = TF_NOOPT;
				break;
			default:
				opt = 0; /* dead code to fool gcc */
				break;
			}

			if (optval)
				tp->t_flags |= opt;
			else
				tp->t_flags &= ~opt;
			break;

		case TCP_NOPUSH:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			if (optval)
				tp->t_flags |= TF_NOPUSH;
			else {
				tp->t_flags &= ~TF_NOPUSH;
				error = tcp_output(tp);
			}
			break;

		case TCP_MAXSEG:
			error = sooptcopyin(sopt, &optval, sizeof optval,
					    sizeof optval);
			if (error)
				break;

			if (optval > 0 && optval <= tp->t_maxseg)
				tp->t_maxseg = optval;
			else
				error = EINVAL;
			break;

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case SOPT_GET:
		switch (sopt->sopt_name) {
		case TCP_NODELAY:
			optval = tp->t_flags & TF_NODELAY;
			break;
		case TCP_MAXSEG:
			optval = tp->t_maxseg;
			break;
		case TCP_NOOPT:
			optval = tp->t_flags & TF_NOOPT;
			break;
		case TCP_NOPUSH:
			optval = tp->t_flags & TF_NOPUSH;
			break;
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (error == 0)
			error = sooptcopyout(sopt, &optval, sizeof optval);
		break;
	}
	splx(s);
	return (error);
}

/*
 * tcp_sendspace and tcp_recvspace are the default send and receive window
 * sizes, respectively.  These are obsolescent (this information should
 * be set by the route).
 */
u_long	tcp_sendspace = 1024*32;
SYSCTL_INT(_net_inet_tcp, TCPCTL_SENDSPACE, sendspace, CTLFLAG_RW,
    &tcp_sendspace , 0, "Maximum outgoing TCP datagram size");
u_long	tcp_recvspace = 57344;	/* largest multiple of PAGE_SIZE < 64k */
SYSCTL_INT(_net_inet_tcp, TCPCTL_RECVSPACE, recvspace, CTLFLAG_RW,
    &tcp_recvspace , 0, "Maximum incoming TCP datagram size");

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
static int
tcp_attach(struct socket *so, struct pru_attach_info *ai)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int error;
	int cpu;
#ifdef INET6
	int isipv6 = INP_CHECK_SOCKAF(so, AF_INET6) != NULL;
#endif

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace,
				  ai->sb_rlimit);
		if (error)
			return (error);
	}
	cpu = mycpu->gd_cpuid;
	error = in_pcballoc(so, &tcbinfo[cpu]);
	if (error)
		return (error);
	inp = so->so_pcb;
#ifdef INET6
	if (isipv6) {
		inp->inp_vflag |= INP_IPV6;
		inp->in6p_hops = -1;	/* use kernel default */
	}
	else
#endif
	inp->inp_vflag |= INP_IPV4;
	tp = tcp_newtcpcb(inp);
	if (tp == 0) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
#ifdef INET6
		if (isipv6)
			in6_pcbdetach(inp);
		else
#endif
		in_pcbdetach(inp);
		so->so_state |= nofd;
		return (ENOBUFS);
	}
	tp->t_state = TCPS_CLOSED;
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
static struct tcpcb *
tcp_disconnect(tp)
	struct tcpcb *tp;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (tp->t_state < TCPS_ESTABLISHED)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
static struct tcpcb *
tcp_usrclosed(tp)
	struct tcpcb *tp;
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_SENT:
	case TCPS_SYN_RECEIVED:
		tp->t_flags |= TF_NEEDFIN;
		break;

	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2) {
		soisdisconnected(tp->t_inpcb->inp_socket);
		/* To prevent the connection hanging in FIN_WAIT_2 forever. */
		if (tp->t_state == TCPS_FIN_WAIT_2)
			callout_reset(tp->tt_2msl, tcp_maxidle,
				      tcp_timer_2msl, tp);
	}
	return (tp);
}
