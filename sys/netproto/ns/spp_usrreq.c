/*
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)spp_usrreq.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/netns/spp_usrreq.c,v 1.11 1999/08/28 00:49:53 peter Exp $
 * $DragonFly: src/sys/netproto/ns/spp_usrreq.c,v 1.15 2005/01/23 13:21:44 joerg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/tcp_fsm.h>

#include "ns.h"
#include "ns_pcb.h"
#include "idp.h"
#include "idp_var.h"
#include "ns_error.h"
#include "sp.h"
#include "spidp.h"
#include "spp_timer.h"
#include "spp_var.h"
#include "spp_debug.h"

extern u_char nsctlerrmap[];		/* from ns_input.c */
extern int idpcksum;			/* from ns_input.c */

static MALLOC_DEFINE(M_IDP, "ns_idp", "NS Packet Management");
static MALLOC_DEFINE(M_SPIDP_Q, "ns_spidp_q", "NS Packet Management");
static MALLOC_DEFINE(M_SPPCB, "ns_sppcb", "NS PCB Management");

struct spp_istat spp_istat;
u_short spp_iss;
int	spp_backoff[SPP_MAXRXTSHIFT+1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

/*
 * SP protocol implementation.
 */
void
spp_init(void)
{
	spp_iss = 1; /* WRONG !! should fish it out of TODR */
}
struct spidp spp_savesi;
int traceallspps = 0;
extern int sppconsdebug;
int spp_hardnosed;
int spp_use_delack = 0;
u_short spp_newchecks[50];

/*ARGSUSED*/
void
spp_input(struct mbuf *m, ...)
{
	struct sppcb *cb;
	struct spidp *si;
	struct socket *so;
	short ostate = 0;
	int dropsocket = 0;
	struct nspcb *nsp;
	__va_list ap;

	__va_start(ap, m);
	nsp = __va_arg(ap, struct nspcb *);
	__va_end(ap);

	sppstat.spps_rcvtotal++;
	if (nsp == 0) {
		panic("No nspcb in spp_input");
		return;
	}

	cb = nstosppcb(nsp);
	if (cb == 0) goto bad;

	if (m->m_len < sizeof(struct spidp)) {
		if ((m = m_pullup(m, sizeof(*si))) == 0) {
			sppstat.spps_rcvshort++;
			return;
		}
	}
	si = mtod(m, struct spidp *);
	si->si_seq = ntohs(si->si_seq);
	si->si_ack = ntohs(si->si_ack);
	si->si_alo = ntohs(si->si_alo);

	so = nsp->nsp_socket;
	if (so->so_options & SO_DEBUG || traceallspps) {
		ostate = cb->s_state;
		spp_savesi = *si;
	}
	if (so->so_options & SO_ACCEPTCONN) {
		struct sppcb *ocb = cb;

		so = sonewconn(so, 0);
		if (so == 0) {
			goto drop;
		}
		/*
		 * This is ugly, but ....
		 *
		 * Mark socket as temporary until we're
		 * committed to keeping it.  The code at
		 * ``drop'' and ``dropwithreset'' check the
		 * flag dropsocket to see if the temporary
		 * socket created here should be discarded.
		 * We mark the socket as discardable until
		 * we're committed to it below in TCPS_LISTEN.
		 */
		dropsocket++;
		nsp = (struct nspcb *)so->so_pcb;
		nsp->nsp_laddr = si->si_dna;
		cb = nstosppcb(nsp);
		cb->s_mtu = ocb->s_mtu;		/* preserve sockopts */
		cb->s_flags = ocb->s_flags;	/* preserve sockopts */
		cb->s_flags2 = ocb->s_flags2;	/* preserve sockopts */
		cb->s_state = TCPS_LISTEN;
	}

	/*
	 * Packet received on connection.
	 * reset idle time and keep-alive timer;
	 */
	cb->s_idle = 0;
	cb->s_timer[SPPT_KEEP] = SPPTV_KEEP;

	switch (cb->s_state) {

	case TCPS_LISTEN:{
		struct mbuf *am;
		struct sockaddr_ns *sns;
		struct ns_addr laddr;

		/*
		 * If somebody here was carying on a conversation
		 * and went away, and his pen pal thinks he can
		 * still talk, we get the misdirected packet.
		 */
		if (spp_hardnosed && (si->si_did != 0 || si->si_seq != 0)) {
			spp_istat.gonawy++;
			goto dropwithreset;
		}
		am = m_get(MB_DONTWAIT, MT_SONAME);
		if (am == NULL)
			goto drop;
		am->m_len = sizeof (struct sockaddr_ns);
		sns = mtod(am, struct sockaddr_ns *);
		sns->sns_len = sizeof(*sns);
		sns->sns_family = AF_NS;
		sns->sns_addr = si->si_sna;
		laddr = nsp->nsp_laddr;
		if (ns_nullhost(laddr))
			nsp->nsp_laddr = si->si_dna;
		if (ns_pcbconnect(nsp, mtod(am, struct sockaddr *))) {
			nsp->nsp_laddr = laddr;
			(void) m_free(am);
			spp_istat.noconn++;
			goto drop;
		}
		(void) m_free(am);
		spp_template(cb);
		dropsocket = 0;		/* committed to socket */
		cb->s_did = si->si_sid;
		cb->s_rack = si->si_ack;
		cb->s_ralo = si->si_alo;
#define THREEWAYSHAKE
#ifdef THREEWAYSHAKE
		cb->s_state = TCPS_SYN_RECEIVED;
		cb->s_force = 1 + SPPT_KEEP;
		sppstat.spps_accepts++;
		cb->s_timer[SPPT_KEEP] = SPPTV_KEEP;
		}
		break;
	/*
	 * This state means that we have heard a response
	 * to our acceptance of their connection
	 * It is probably logically unnecessary in this
	 * implementation.
	 */
	 case TCPS_SYN_RECEIVED: {
		if (si->si_did!=cb->s_sid) {
			spp_istat.wrncon++;
			goto drop;
		}
#endif
		nsp->nsp_fport =  si->si_sport;
		cb->s_timer[SPPT_REXMT] = 0;
		cb->s_timer[SPPT_KEEP] = SPPTV_KEEP;
		soisconnected(so);
		cb->s_state = TCPS_ESTABLISHED;
		sppstat.spps_accepts++;
		}
		break;

	/*
	 * This state means that we have gotten a response
	 * to our attempt to establish a connection.
	 * We fill in the data from the other side,
	 * telling us which port to respond to, instead of the well-
	 * known one we might have sent to in the first place.
	 * We also require that this is a response to our
	 * connection id.
	 */
	case TCPS_SYN_SENT:
		if (si->si_did!=cb->s_sid) {
			spp_istat.notme++;
			goto drop;
		}
		sppstat.spps_connects++;
		cb->s_did = si->si_sid;
		cb->s_rack = si->si_ack;
		cb->s_ralo = si->si_alo;
		cb->s_dport = nsp->nsp_fport =  si->si_sport;
		cb->s_timer[SPPT_REXMT] = 0;
		cb->s_flags |= SF_ACKNOW;
		soisconnected(so);
		cb->s_state = TCPS_ESTABLISHED;
		/* Use roundtrip time of connection request for initial rtt */
		if (cb->s_rtt) {
			cb->s_srtt = cb->s_rtt << 3;
			cb->s_rttvar = cb->s_rtt << 1;
			SPPT_RANGESET(cb->s_rxtcur,
			    ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
			    SPPTV_MIN, SPPTV_REXMTMAX);
			    cb->s_rtt = 0;
		}
	}
	if (so->so_options & SO_DEBUG || traceallspps)
		spp_trace(SA_INPUT, (u_char)ostate, cb, &spp_savesi, 0);

	m->m_len -= sizeof (struct idp);
	m->m_pkthdr.len -= sizeof (struct idp);
	m->m_data += sizeof (struct idp);

	if (spp_reass(cb, si, m)) {
		(void) m_freem(m);
	}
	if (cb->s_force || (cb->s_flags & (SF_ACKNOW|SF_WIN|SF_RXT)))
		(void) spp_output(cb, (struct mbuf *)0);
	cb->s_flags &= ~(SF_WIN|SF_RXT);
	return;

dropwithreset:
	if (dropsocket)
		(void) soabort(so);
	si->si_seq = ntohs(si->si_seq);
	si->si_ack = ntohs(si->si_ack);
	si->si_alo = ntohs(si->si_alo);
	ns_error(m, NS_ERR_NOSOCK, 0);
	if (cb->s_nspcb->nsp_socket->so_options & SO_DEBUG || traceallspps)
		spp_trace(SA_DROP, (u_char)ostate, cb, &spp_savesi, 0);
	return;

drop:
bad:
	if (cb == 0 || cb->s_nspcb->nsp_socket->so_options & SO_DEBUG ||
            traceallspps)
		spp_trace(SA_DROP, (u_char)ostate, cb, &spp_savesi, 0);
	m_freem(m);
}

int spprexmtthresh = 3;

/*
 * This is structurally similar to the tcp reassembly routine
 * but its function is somewhat different:  It merely queues
 * packets up, and suppresses duplicates.
 */
int
spp_reass(struct sppcb *cb, struct spidp *si, struct mbuf *si_m)
{
	struct spidp_q *q;
	struct spidp_q *nq;
	struct mbuf *m;
	struct socket *so = cb->s_nspcb->nsp_socket;
	char packetp = cb->s_flags & SF_HI;
	int incr;
	char wakeup = 0;

	if (si == NULL)
		goto present;
	/*
	 * Update our news from them.
	 */
	if (si->si_cc & SP_SA)
		cb->s_flags |= (spp_use_delack ? SF_DELACK : SF_ACKNOW);
	if (SSEQ_GT(si->si_alo, cb->s_ralo))
		cb->s_flags |= SF_WIN;
	if (SSEQ_LEQ(si->si_ack, cb->s_rack)) {
		if ((si->si_cc & SP_SP) && cb->s_rack != (cb->s_smax + 1)) {
			sppstat.spps_rcvdupack++;
			/*
			 * If this is a completely duplicate ack
			 * and other conditions hold, we assume
			 * a packet has been dropped and retransmit
			 * it exactly as in tcp_input().
			 */
			if (si->si_ack != cb->s_rack ||
			    si->si_alo != cb->s_ralo)
				cb->s_dupacks = 0;
			else if (++cb->s_dupacks == spprexmtthresh) {
				u_short onxt = cb->s_snxt;
				int cwnd = cb->s_cwnd;

				cb->s_snxt = si->si_ack;
				cb->s_cwnd = CUNIT;
				cb->s_force = 1 + SPPT_REXMT;
				(void) spp_output(cb, (struct mbuf *)0);
				cb->s_timer[SPPT_REXMT] = cb->s_rxtcur;
				cb->s_rtt = 0;
				if (cwnd >= 4 * CUNIT)
					cb->s_cwnd = cwnd / 2;
				if (SSEQ_GT(onxt, cb->s_snxt))
					cb->s_snxt = onxt;
				return (1);
			}
		} else
			cb->s_dupacks = 0;
		goto update_window;
	}
	cb->s_dupacks = 0;
	/*
	 * If our correspondent acknowledges data we haven't sent
	 * TCP would drop the packet after acking.  We'll be a little
	 * more permissive
	 */
	if (SSEQ_GT(si->si_ack, (cb->s_smax + 1))) {
		sppstat.spps_rcvacktoomuch++;
		si->si_ack = cb->s_smax + 1;
	}
	sppstat.spps_rcvackpack++;
	/*
	 * If transmit timer is running and timed sequence
	 * number was acked, update smoothed round trip time.
	 * See discussion of algorithm in tcp_input.c
	 */
	if (cb->s_rtt && SSEQ_GT(si->si_ack, cb->s_rtseq)) {
		sppstat.spps_rttupdated++;
		if (cb->s_srtt != 0) {
			short delta;
			delta = cb->s_rtt - (cb->s_srtt >> 3);
			if ((cb->s_srtt += delta) <= 0)
				cb->s_srtt = 1;
			if (delta < 0)
				delta = -delta;
			delta -= (cb->s_rttvar >> 2);
			if ((cb->s_rttvar += delta) <= 0)
				cb->s_rttvar = 1;
		} else {
			/*
			 * No rtt measurement yet
			 */
			cb->s_srtt = cb->s_rtt << 3;
			cb->s_rttvar = cb->s_rtt << 1;
		}
		cb->s_rtt = 0;
		cb->s_rxtshift = 0;
		SPPT_RANGESET(cb->s_rxtcur,
			((cb->s_srtt >> 2) + cb->s_rttvar) >> 1,
			SPPTV_MIN, SPPTV_REXMTMAX);
	}
	/*
	 * If all outstanding data is acked, stop retransmit
	 * timer and remember to restart (more output or persist).
	 * If there is more data to be acked, restart retransmit
	 * timer, using current (possibly backed-off) value;
	 */
	if (si->si_ack == cb->s_smax + 1) {
		cb->s_timer[SPPT_REXMT] = 0;
		cb->s_flags |= SF_RXT;
	} else if (cb->s_timer[SPPT_PERSIST] == 0)
		cb->s_timer[SPPT_REXMT] = cb->s_rxtcur;
	/*
	 * When new data is acked, open the congestion window.
	 * If the window gives us less than ssthresh packets
	 * in flight, open exponentially (maxseg at a time).
	 * Otherwise open linearly (maxseg^2 / cwnd at a time).
	 */
	incr = CUNIT;
	if (cb->s_cwnd > cb->s_ssthresh)
		incr = max(incr * incr / cb->s_cwnd, 1);
	cb->s_cwnd = min(cb->s_cwnd + incr, cb->s_cwmx);
	/*
	 * Trim Acked data from output queue.
	 */
	while ((m = so->so_snd.sb_mb) != NULL) {
		if (SSEQ_LT((mtod(m, struct spidp *))->si_seq, si->si_ack))
			sbdroprecord(&so->so_snd);
		else
			break;
	}
	sowwakeup(so);
	cb->s_rack = si->si_ack;
update_window:
	if (SSEQ_LT(cb->s_snxt, cb->s_rack))
		cb->s_snxt = cb->s_rack;
	if (SSEQ_LT(cb->s_swl1, si->si_seq) || (cb->s_swl1 == si->si_seq &&
	    (SSEQ_LT(cb->s_swl2, si->si_ack) ||
		(cb->s_swl2 == si->si_ack && SSEQ_LT(cb->s_ralo, si->si_alo))))) {
		/* keep track of pure window updates */
		if ((si->si_cc & SP_SP) && cb->s_swl2 == si->si_ack
		    && SSEQ_LT(cb->s_ralo, si->si_alo)) {
			sppstat.spps_rcvwinupd++;
			sppstat.spps_rcvdupack--;
		}
		cb->s_ralo = si->si_alo;
		cb->s_swl1 = si->si_seq;
		cb->s_swl2 = si->si_ack;
		cb->s_swnd = (1 + si->si_alo - si->si_ack);
		if (cb->s_swnd > cb->s_smxw)
			cb->s_smxw = cb->s_swnd;
		cb->s_flags |= SF_WIN;
	}
	/*
	 * If this packet number is higher than that which
	 * we have allocated refuse it, unless urgent
	 */
	if (SSEQ_GT(si->si_seq, cb->s_alo)) {
		if (si->si_cc & SP_SP) {
			sppstat.spps_rcvwinprobe++;
			return (1);
		} else
			sppstat.spps_rcvpackafterwin++;
		if (si->si_cc & SP_OB) {
			if (SSEQ_GT(si->si_seq, cb->s_alo + 60)) {
				ns_error(si_m, NS_ERR_FULLUP, 0);
				return (0);
			} /* else queue this packet; */
		} else {
			/*register struct socket *so = cb->s_nspcb->nsp_socket;
			if (so->so_state && SS_NOFDREF) {
				ns_error(si_m, NS_ERR_NOSOCK, 0);
				(void)spp_close(cb);
			} else
				       would crash system*/
			spp_istat.notyet++;
			ns_error(si_m, NS_ERR_FULLUP, 0);
			return (0);
		}
	}
	/*
	 * If this is a system packet, we don't need to
	 * queue it up, and won't update acknowledge #
	 */
	if (si->si_cc & SP_SP) {
		return (1);
	}
	/*
	 * We have already seen this packet, so drop.
	 */
	if (SSEQ_LT(si->si_seq, cb->s_ack)) {
		spp_istat.bdreas++;
		sppstat.spps_rcvduppack++;
		if (si->si_seq == cb->s_ack - 1)
			spp_istat.lstdup++;
		return (1);
	}
	/*
	 * Loop through all packets queued up to insert in
	 * appropriate sequence.
	 */
	for (q = cb->s_q.si_next; q!=&cb->s_q; q = q->si_next) {
		if (si->si_seq == SI(q)->si_seq) {
			sppstat.spps_rcvduppack++;
			return (1);
		}
		if (SSEQ_LT(si->si_seq, SI(q)->si_seq)) {
			sppstat.spps_rcvoopack++;
			break;
		}
	}
	nq = malloc(sizeof(struct spidp_q), M_SPIDP_Q, M_INTNOWAIT);
	if (nq == NULL) {
		m_freem(si_m);
		return(0);
	}
	insque(nq, q->si_prev);
	nq->si_mbuf = si_m;
	/*
	 * If this packet is urgent, inform process
	 */
	if (si->si_cc & SP_OB) {
		cb->s_iobc = ((char *)si)[1 + sizeof(*si)];
		sohasoutofband(so);
		cb->s_oobflags |= SF_IOOB;
	}
present:
#define SPINC sizeof(struct sphdr)
	/*
	 * Loop through all packets queued up to update acknowledge
	 * number, and present all acknowledged data to user;
	 * If in packet interface mode, show packet headers.
	 */
	for (q = cb->s_q.si_next; q!=&cb->s_q; q = q->si_next) {
		  if (SI(q)->si_seq == cb->s_ack) {
			cb->s_ack++;
			m = q->si_mbuf;
			if (SI(q)->si_cc & SP_OB) {
				cb->s_oobflags &= ~SF_IOOB;
				if (so->so_rcv.sb_cc)
					so->so_oobmark = so->so_rcv.sb_cc;
				else
					so->so_state |= SS_RCVATMARK;
			}
			nq = q;
			q = q->si_prev;
			remque(nq);
			free(nq, M_SPIDP_Q);
			wakeup = 1;
			sppstat.spps_rcvpack++;
#ifdef SF_NEWCALL
			if (cb->s_flags2 & SF_NEWCALL) {
				struct sphdr *sp = mtod(m, struct sphdr *);
				u_char dt = sp->sp_dt;
				spp_newchecks[4]++;
				if (dt != cb->s_rhdr.sp_dt) {
					struct mbuf *mm =
					   m_getclr(MB_DONTWAIT, MT_CONTROL);
					spp_newchecks[0]++;
					if (mm != NULL) {
						u_short *s =
							mtod(mm, u_short *);
						cb->s_rhdr.sp_dt = dt;
						mm->m_len = 5; /*XXX*/
						s[0] = 5;
						s[1] = 1;
						*(u_char *)(&s[2]) = dt;
						sbappend(&so->so_rcv, mm);
					}
				}
				if (sp->sp_cc & SP_OB) {
					m_chtype(m, MT_OOBDATA);
					spp_newchecks[1]++;
					so->so_oobmark = 0;
					so->so_state &= ~SS_RCVATMARK;
				}
				if (packetp == 0) {
					m->m_data += SPINC;
					m->m_len -= SPINC;
					m->m_pkthdr.len -= SPINC;
				}
				if ((sp->sp_cc & SP_EM) || packetp) {
					sbappendrecord(&so->so_rcv, m);
					spp_newchecks[9]++;
				} else
					sbappend(&so->so_rcv, m);
			} else
#endif
			if (packetp) {
				sbappendrecord(&so->so_rcv, m);
			} else {
				cb->s_rhdr = *mtod(m, struct sphdr *);
				m->m_data += SPINC;
				m->m_len -= SPINC;
				m->m_pkthdr.len -= SPINC;
				sbappend(&so->so_rcv, m);
			}
		  } else
			break;
	}
	if (wakeup) sorwakeup(so);
	return (0);
}

void
spp_ctlinput(int cmd, caddr_t arg)
{
	struct ns_addr *na;
	struct ns_errp *errp = 0;
	struct nspcb *nsp;
	struct sockaddr_ns *sns;
	int type;

	if (cmd < 0 || cmd > PRC_NCMDS)
		return;
	type = NS_ERR_UNREACH_HOST;

	switch (cmd) {

	case PRC_ROUTEDEAD:
		return;

	case PRC_IFDOWN:
	case PRC_HOSTDEAD:
	case PRC_HOSTUNREACH:
		sns = (struct sockaddr_ns *)arg;
		if (sns->sns_family != AF_NS)
			return;
		na = &sns->sns_addr;
		break;

	default:
		errp = (struct ns_errp *)arg;
		na = &errp->ns_err_idp.idp_dna;
		type = errp->ns_err_num;
		type = ntohs((u_short)type);
	}
	switch (type) {

	case NS_ERR_UNREACH_HOST:
		ns_pcbnotify(na, (int)nsctlerrmap[cmd], spp_abort, (long) 0);
		break;

	case NS_ERR_TOO_BIG:
	case NS_ERR_NOSOCK:
		nsp = ns_pcblookup(na, errp->ns_err_idp.idp_sna.x_port,
			NS_WILDCARD);
		if (nsp) {
			if(nsp->nsp_pcb)
				(void) spp_drop((struct sppcb *)nsp->nsp_pcb,
						(int)nsctlerrmap[cmd]);
			else
				(void) idp_drop(nsp, (int)nsctlerrmap[cmd]);
		}
		break;

	case NS_ERR_FULLUP:
		ns_pcbnotify(na, 0, spp_quench, (long) 0);
	}
}
/*
 * When a source quench is received, close congestion window
 * to one packet.  We will gradually open it again as we proceed.
 */
void
spp_quench(struct nspcb *nsp)
{
	struct sppcb *cb = nstosppcb(nsp);

	if (cb)
		cb->s_cwnd = CUNIT;
}

#ifdef notdef
int
spp_fixmtu(struct nspcb *nsp)
{
	struct sppcb *cb = (struct sppcb *)(nsp->nsp_pcb);
	struct mbuf *m;
	struct spidp *si;
	struct ns_errp *ep;
	struct sockbuf *sb;
	int badseq, len;
	struct mbuf *firstbad, *m0;

	if (cb) {
		/*
		 * The notification that we have sent
		 * too much is bad news -- we will
		 * have to go through queued up so far
		 * splitting ones which are too big and
		 * reassigning sequence numbers and checksums.
		 * we should then retransmit all packets from
		 * one above the offending packet to the last one
		 * we had sent (or our allocation)
		 * then the offending one so that the any queued
		 * data at our destination will be discarded.
		 */
		 ep = (struct ns_errp *)nsp->nsp_notify_param;
		 sb = &nsp->nsp_socket->so_snd;
		 cb->s_mtu = ep->ns_err_param;
		 badseq = ep->ns_err_idp.si_seq;
		 for (m = sb->sb_mb; m; m = m->m_nextpkt) {
			si = mtod(m, struct spidp *);
			if (si->si_seq == badseq)
				break;
		 }
		 if (m == 0) return;
		 firstbad = m;
		 /*for (;;) {*/
			/* calculate length */
			for (m0 = m, len = 0; m ; m = m->m_next)
				len += m->m_len;
			if (len > cb->s_mtu) {
			}
		/* FINISH THIS
		} */
	}
}
#endif

int
spp_output(struct sppcb *cb, struct mbuf *m0)
{
	struct socket *so = cb->s_nspcb->nsp_socket;
	struct mbuf *m = NULL;
	struct spidp *si = NULL;
	struct sockbuf *sb = &so->so_snd;
	int len = 0, win, rcv_win;
	short span, off, recordp = 0;
	u_short alo;
	int error = 0, sendalot;
#ifdef notdef
	int idle;
#endif
	struct mbuf *mprev;

	if (m0) {
		int mtu = cb->s_mtu;
		int datalen;
		/*
		 * Make sure that packet isn't too big.
		 */
		for (m = m0; m ; m = m->m_next) {
			mprev = m;
			len += m->m_len;
			if (m->m_flags & M_EOR)
				recordp = 1;
		}
		datalen = (cb->s_flags & SF_HO) ?
				len - sizeof (struct sphdr) : len;
		if (datalen > mtu) {
			if (cb->s_flags & SF_PI) {
				m_freem(m0);
				return (EMSGSIZE);
			} else {
				int oldEM = cb->s_cc & SP_EM;

				cb->s_cc &= ~SP_EM;
				while (len > mtu) {
					/*
					 * Here we are only being called
					 * from usrreq(), so it is OK to
					 * block.
					 */
					m = m_copym(m0, 0, mtu, MB_WAIT);
					if (cb->s_flags & SF_NEWCALL) {
					    struct mbuf *mm = m;
					    spp_newchecks[7]++;
					    while (mm) {
						mm->m_flags &= ~M_EOR;
						mm = mm->m_next;
					    }
					}
					error = spp_output(cb, m);
					if (error) {
						cb->s_cc |= oldEM;
						m_freem(m0);
						return(error);
					}
					m_adj(m0, mtu);
					len -= mtu;
				}
				cb->s_cc |= oldEM;
			}
		}
		/*
		 * Force length even, by adding a "garbage byte" if
		 * necessary.
		 */
		if (len & 1) {
			m = mprev;
			if (M_TRAILINGSPACE(m) >= 1)
				m->m_len++;
			else {
				struct mbuf *m1 = m_get(MB_DONTWAIT, MT_DATA);

				if (m1 == 0) {
					m_freem(m0);
					return (ENOBUFS);
				}
				m1->m_len = 1;
				*(mtod(m1, u_char *)) = 0;
				m->m_next = m1;
			}
		}
		m = m_gethdr(MB_DONTWAIT, MT_HEADER);
		if (m == 0) {
			m_freem(m0);
			return (ENOBUFS);
		}
		/*
		 * Fill in mbuf with extended SP header
		 * and addresses and length put into network format.
		 */
		MH_ALIGN(m, sizeof (struct spidp));
		m->m_len = sizeof (struct spidp);
		m->m_next = m0;
		si = mtod(m, struct spidp *);
		si->si_i = *cb->s_idp;
		si->si_s = cb->s_shdr;
		if ((cb->s_flags & SF_PI) && (cb->s_flags & SF_HO)) {
			struct sphdr *sh;
			if (m0->m_len < sizeof (*sh)) {
				if((m0 = m_pullup(m0, sizeof(*sh))) == NULL) {
					(void) m_free(m);
					m_freem(m0);
					return (EINVAL);
				}
				m->m_next = m0;
			}
			sh = mtod(m0, struct sphdr *);
			si->si_dt = sh->sp_dt;
			si->si_cc |= sh->sp_cc & SP_EM;
			m0->m_len -= sizeof (*sh);
			m0->m_data += sizeof (*sh);
			len -= sizeof (*sh);
		}
		len += sizeof(*si);
		if ((cb->s_flags2 & SF_NEWCALL) && recordp) {
			si->si_cc  |= SP_EM;
			spp_newchecks[8]++;
		}
		if (cb->s_oobflags & SF_SOOB) {
			/*
			 * Per jqj@cornell:
			 * make sure OB packets convey exactly 1 byte.
			 * If the packet is 1 byte or larger, we
			 * have already guaranted there to be at least
			 * one garbage byte for the checksum, and
			 * extra bytes shouldn't hurt!
			 */
			if (len > sizeof(*si)) {
				si->si_cc |= SP_OB;
				len = (1 + sizeof(*si));
			}
		}
		si->si_len = htons((u_short)len);
		m->m_pkthdr.len = ((len - 1) | 1) + 1;
		/*
		 * queue stuff up for output
		 */
		sbappendrecord(sb, m);
		cb->s_seq++;
	}
#ifdef notdef
	idle = (cb->s_smax == (cb->s_rack - 1));
#endif
again:
	sendalot = 0;
	off = cb->s_snxt - cb->s_rack;
	win = min(cb->s_swnd, (cb->s_cwnd/CUNIT));

	/*
	 * If in persist timeout with window of 0, send a probe.
	 * Otherwise, if window is small but nonzero
	 * and timer expired, send what we can and go into
	 * transmit state.
	 */
	if (cb->s_force == 1 + SPPT_PERSIST) {
		if (win != 0) {
			cb->s_timer[SPPT_PERSIST] = 0;
			cb->s_rxtshift = 0;
		}
	}
	span = cb->s_seq - cb->s_rack;
	len = min(span, win) - off;

	if (len < 0) {
		/*
		 * Window shrank after we went into it.
		 * If window shrank to 0, cancel pending
		 * restransmission and pull s_snxt back
		 * to (closed) window.  We will enter persist
		 * state below.  If the widndow didn't close completely,
		 * just wait for an ACK.
		 */
		len = 0;
		if (win == 0) {
			cb->s_timer[SPPT_REXMT] = 0;
			cb->s_snxt = cb->s_rack;
		}
	}
	if (len > 1)
		sendalot = 1;
	rcv_win = sbspace(&so->so_rcv);

	/*
	 * Send if we owe peer an ACK.
	 */
	if (cb->s_oobflags & SF_SOOB) {
		/*
		 * must transmit this out of band packet
		 */
		cb->s_oobflags &= ~ SF_SOOB;
		sendalot = 1;
		sppstat.spps_sndurg++;
		goto found;
	}
	if (cb->s_flags & SF_ACKNOW)
		goto send;
	if (cb->s_state < TCPS_ESTABLISHED)
		goto send;
	/*
	 * Silly window can't happen in spp.
	 * Code from tcp deleted.
	 */
	if (len)
		goto send;
	/*
	 * Compare available window to amount of window
	 * known to peer (as advertised window less
	 * next expected input.)  If the difference is at least two
	 * packets or at least 35% of the mximum possible window,
	 * then want to send a window update to peer.
	 */
	if (rcv_win > 0) {
		u_short delta =  1 + cb->s_alo - cb->s_ack;
		int adv = rcv_win - (delta * cb->s_mtu);

		if ((so->so_rcv.sb_cc == 0 && adv >= (2 * cb->s_mtu)) ||
		    (100 * adv / so->so_rcv.sb_hiwat >= 35)) {
			sppstat.spps_sndwinup++;
			cb->s_flags |= SF_ACKNOW;
			goto send;
		}

	}
	/*
	 * Many comments from tcp_output.c are appropriate here
	 * including . . .
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.
	 * If nothing happens soon, send when timer expires:
	 * if window is nonzero, transmit what we can,
	 * otherwise send a probe.
	 */
	if (so->so_snd.sb_cc && cb->s_timer[SPPT_REXMT] == 0 &&
		cb->s_timer[SPPT_PERSIST] == 0) {
			cb->s_rxtshift = 0;
			spp_setpersist(cb);
	}
	/*
	 * No reason to send a packet, just return.
	 */
	cb->s_outx = 1;
	return (0);

send:
	/*
	 * Find requested packet.
	 */
	si = NULL;
	if (len > 0) {
		cb->s_want = cb->s_snxt;
		for (m = sb->sb_mb; m; m = m->m_nextpkt) {
			si = mtod(m, struct spidp *);
			if (SSEQ_LEQ(cb->s_snxt, si->si_seq))
				break;
		}
	found:
		if (si) {
			if (si->si_seq == cb->s_snxt)
					cb->s_snxt++;
				else
					sppstat.spps_sndvoid++, si = 0;
		}
	}
	/*
	 * update window
	 */
	if (rcv_win < 0)
		rcv_win = 0;
	alo = cb->s_ack - 1 + (rcv_win / ((short)cb->s_mtu));
	if (SSEQ_LT(alo, cb->s_alo))
		alo = cb->s_alo;

	if (si) {
		/*
		 * must make a copy of this packet for
		 * idp_output to monkey with
		 */
		m = m_copy(m, 0, (int)M_COPYALL);
		if (m == NULL) {
			return (ENOBUFS);
		}
		si = mtod(m, struct spidp *);
		if (SSEQ_LT(si->si_seq, cb->s_smax))
			sppstat.spps_sndrexmitpack++;
		else
			sppstat.spps_sndpack++;
	} else if (cb->s_force || cb->s_flags & SF_ACKNOW) {
		/*
		 * Must send an acknowledgement or a probe
		 */
		if (cb->s_force)
			sppstat.spps_sndprobe++;
		if (cb->s_flags & SF_ACKNOW)
			sppstat.spps_sndacks++;
		m = m_gethdr(MB_DONTWAIT, MT_HEADER);
		if (m == 0)
			return (ENOBUFS);
		/*
		 * Fill in mbuf with extended SP header
		 * and addresses and length put into network format.
		 */
		MH_ALIGN(m, sizeof (struct spidp));
		m->m_len = sizeof (*si);
		m->m_pkthdr.len = sizeof (*si);
		si = mtod(m, struct spidp *);
		si->si_i = *cb->s_idp;
		si->si_s = cb->s_shdr;
		si->si_seq = cb->s_smax + 1;
		si->si_len = htons(sizeof (*si));
		si->si_cc |= SP_SP;
	} else {
		cb->s_outx = 3;
		if (so->so_options & SO_DEBUG || traceallspps)
			spp_trace(SA_OUTPUT, cb->s_state, cb, si, 0);
		return (0);
	}
	/*
	 * Stuff checksum and output datagram.
	 */
	if ((si->si_cc & SP_SP) == 0) {
		if (cb->s_force != (1 + SPPT_PERSIST) ||
		    cb->s_timer[SPPT_PERSIST] == 0) {
			/*
			 * If this is a new packet and we are not currently
			 * timing anything, time this one.
			 */
			if (SSEQ_LT(cb->s_smax, si->si_seq)) {
				cb->s_smax = si->si_seq;
				if (cb->s_rtt == 0) {
					sppstat.spps_segstimed++;
					cb->s_rtseq = si->si_seq;
					cb->s_rtt = 1;
				}
			}
			/*
			 * Set rexmt timer if not currently set,
			 * Initial value for retransmit timer is smoothed
			 * round-trip time + 2 * round-trip time variance.
			 * Initialize shift counter which is used for backoff
			 * of retransmit time.
			 */
			if (cb->s_timer[SPPT_REXMT] == 0 &&
			    cb->s_snxt != cb->s_rack) {
				cb->s_timer[SPPT_REXMT] = cb->s_rxtcur;
				if (cb->s_timer[SPPT_PERSIST]) {
					cb->s_timer[SPPT_PERSIST] = 0;
					cb->s_rxtshift = 0;
				}
			}
		} else if (SSEQ_LT(cb->s_smax, si->si_seq)) {
			cb->s_smax = si->si_seq;
		}
	} else if (cb->s_state < TCPS_ESTABLISHED) {
		if (cb->s_rtt == 0)
			cb->s_rtt = 1; /* Time initial handshake */
		if (cb->s_timer[SPPT_REXMT] == 0)
			cb->s_timer[SPPT_REXMT] = cb->s_rxtcur;
	}
	{
		/*
		 * Do not request acks when we ack their data packets or
		 * when we do a gratuitous window update.
		 */
		if (((si->si_cc & SP_SP) == 0) || cb->s_force)
				si->si_cc |= SP_SA;
		si->si_seq = htons(si->si_seq);
		si->si_alo = htons(alo);
		si->si_ack = htons(cb->s_ack);

		if (idpcksum) {
			si->si_sum = 0;
			len = ntohs(si->si_len);
			if (len & 1)
				len++;
			si->si_sum = ns_cksum(m, len);
		} else
			si->si_sum = 0xffff;

		cb->s_outx = 4;
		if (so->so_options & SO_DEBUG || traceallspps)
			spp_trace(SA_OUTPUT, cb->s_state, cb, si, 0);

		if (so->so_options & SO_DONTROUTE)
			error = ns_output(m, (struct route *)0, NS_ROUTETOIF);
		else
			error = ns_output(m, &cb->s_nspcb->nsp_route, 0);
	}
	if (error) {
		return (error);
	}
	sppstat.spps_sndtotal++;
	/*
	 * Data sent (as far as we can tell).
	 * If this advertises a larger window than any other segment,
	 * then remember the size of the advertized window.
	 * Any pending ACK has now been sent.
	 */
	cb->s_force = 0;
	cb->s_flags &= ~(SF_ACKNOW|SF_DELACK);
	if (SSEQ_GT(alo, cb->s_alo))
		cb->s_alo = alo;
	if (sendalot)
		goto again;
	cb->s_outx = 5;
	return (0);
}

int spp_do_persist_panics = 0;

void
spp_setpersist(struct sppcb *cb)
{
	int t = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;

	if (cb->s_timer[SPPT_REXMT] && spp_do_persist_panics)
		panic("spp_output REXMT");
	/*
	 * Start/restart persistance timer.
	 */
	SPPT_RANGESET(cb->s_timer[SPPT_PERSIST],
	    t*spp_backoff[cb->s_rxtshift],
	    SPPTV_PERSMIN, SPPTV_PERSMAX);
	if (cb->s_rxtshift < SPP_MAXRXTSHIFT)
		cb->s_rxtshift++;
}
/*ARGSUSED*/
int
spp_ctloutput(int req, struct socket *so, int level,
		int name, struct mbuf **value)
{
	struct mbuf *m;
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int mask, error = 0;

	if (level != NSPROTO_SPP) {
		/* This will have to be changed when we do more general
		   stacking of protocols */
		return (idp_ctloutput(req, so, level, name, value));
	}
	if (nsp == NULL) {
		error = EINVAL;
		goto release;
	} else
		cb = nstosppcb(nsp);

	switch (req) {

	case PRCO_GETOPT:
		if (value == NULL)
			return (EINVAL);
		m = m_get(MB_DONTWAIT, MT_DATA);
		if (m == NULL)
			return (ENOBUFS);
		switch (name) {

		case SO_HEADERS_ON_INPUT:
			mask = SF_HI;
			goto get_flags;

		case SO_HEADERS_ON_OUTPUT:
			mask = SF_HO;
		get_flags:
			m->m_len = sizeof(short);
			*mtod(m, short *) = cb->s_flags & mask;
			break;

		case SO_MTU:
			m->m_len = sizeof(u_short);
			*mtod(m, short *) = cb->s_mtu;
			break;

		case SO_LAST_HEADER:
			m->m_len = sizeof(struct sphdr);
			*mtod(m, struct sphdr *) = cb->s_rhdr;
			break;

		case SO_DEFAULT_HEADERS:
			m->m_len = sizeof(struct spidp);
			*mtod(m, struct sphdr *) = cb->s_shdr;
			break;

		default:
			error = EINVAL;
		}
		*value = m;
		break;

	case PRCO_SETOPT:
		if (value == 0 || *value == 0) {
			error = EINVAL;
			break;
		}
		switch (name) {
			int *ok;

		case SO_HEADERS_ON_INPUT:
			mask = SF_HI;
			goto set_head;

		case SO_HEADERS_ON_OUTPUT:
			mask = SF_HO;
		set_head:
			if (cb->s_flags & SF_PI) {
				ok = mtod(*value, int *);
				if (*ok)
					cb->s_flags |= mask;
				else
					cb->s_flags &= ~mask;
			} else error = EINVAL;
			break;

		case SO_MTU:
			cb->s_mtu = *(mtod(*value, u_short *));
			break;

#ifdef SF_NEWCALL
		case SO_NEWCALL:
			ok = mtod(*value, int *);
			if (*ok) {
				cb->s_flags2 |= SF_NEWCALL;
				spp_newchecks[5]++;
			} else {
				cb->s_flags2 &= ~SF_NEWCALL;
				spp_newchecks[6]++;
			}
			break;
#endif

		case SO_DEFAULT_HEADERS:
			{
				struct sphdr *sp
						= mtod(*value, struct sphdr *);
				cb->s_dt = sp->sp_dt;
				cb->s_cc = sp->sp_cc & SP_EM;
			}
			break;

		default:
			error = EINVAL;
		}
		m_freem(*value);
		break;
	}
	release:
		return (error);
}

/*
 *  SPP_USRREQ PROCEDURES
 */

static int
spp_usr_abort(struct socket *so)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		spp_drop(cb, ECONNABORTED);
		error = 0;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_accept(struct socket *so, struct sockaddr **nam)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	struct sockaddr_ns sns;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		bzero(&sns, sizeof(sns));
		sns.sns_family = AF_NS;
		sns.sns_addr = nsp->nsp_faddr;
		*nam = dup_sockaddr((struct sockaddr *)&sns);
		error = 0;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_attach(struct socket *so, int proto, struct pru_attach_info *ai)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	struct sockbuf *sb;
	int error;

	if (nsp != NULL)
		return(EISCONN);
	if ((error = ns_pcballoc(so, &nspcb)) != 0)
		return(error);
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		if ((error = soreserve(so, 3072, 3072, ai->sb_rlimit)) != 0)
			return(error);
	}
	nsp = sotonspcb(so);

	sb = &so->so_snd;
	cb = malloc(sizeof(struct sppcb), M_SPPCB, M_WAITOK|M_ZERO);
	cb->s_idp = malloc(sizeof(struct idp), M_IDP, M_WAITOK|M_ZERO);
	cb->s_state = TCPS_LISTEN;
	cb->s_smax = -1;
	cb->s_swl1 = -1;
	cb->s_q.si_next = cb->s_q.si_prev = &cb->s_q;
	cb->s_nspcb = nsp;
	cb->s_mtu = 576 - sizeof (struct spidp);
	cb->s_cwnd = sbspace(sb) * CUNIT / cb->s_mtu;
	cb->s_ssthresh = cb->s_cwnd;
	cb->s_cwmx = sbspace(sb) * CUNIT / (2 * sizeof (struct spidp));

	/*
	 * Above is recomputed when connecting to account
	 * for changed buffering or mtu's
	 */
	cb->s_rtt = SPPTV_SRTTBASE;
	cb->s_rttvar = SPPTV_SRTTDFLT << 2;
	SPPT_RANGESET(cb->s_rxtcur, 
		    ((SPPTV_SRTTBASE >> 2) + (SPPTV_SRTTDFLT << 2)) >> 1,
		    SPPTV_MIN, SPPTV_REXMTMAX);
	nsp->nsp_pcb = (caddr_t)cb;
	return(0);
}

static int
spp_attach_sp(struct socket *so, int proto, struct pru_attach_info *ai)
{
	int error;
	struct nspcb *nsp;

	if ((error = spp_attach(so, proto, ai)) == 0) {
		nsp = sotonspcb(so);
		((struct sppcb *)nsp->nsp_pcb)->s_flags |=
					(SF_HI | SF_HO | SF_PI);
	}
	return (error);
}

static int
spp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct nspcb *nsp = sotonspcb(so);
	int error;

	if (nsp)
		error = ns_pcbbind(nsp, nam);
	else
		error = EINVAL;
	return(error);
}

/*
 * Initiate connection to peer.
 * Enter SYN_SENT state, and mark socket as connecting.
 * Start keep-alive timer, setup prototype header,
 * Send initial system packet requesting connection.
 */
static int
spp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
                if (nsp->nsp_lport == 0) {
			if ((error = ns_pcbbind(nsp, NULL)) != 0)
				return(error);
                }
		if ((error = ns_pcbconnect(nsp, nam)) != 0)
			return(error);
		soisconnecting(so);
		sppstat.spps_connattempt++;
		cb->s_state = TCPS_SYN_SENT;
		cb->s_did = 0;
		spp_template(cb);
		cb->s_timer[SPPT_KEEP] = SPPTV_KEEP;
		cb->s_force = 1 + SPPTV_KEEP;
		/*
		 * Other party is required to respond to
		 * the port I send from, but he is not
		 * required to answer from where I am sending to,
		 * so allow wildcarding.
		 * original port I am sending to is still saved in
		 * cb->s_dport.
		 */
		nsp->nsp_fport = 0;
		error = spp_output(cb, NULL);
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_detach(struct socket *so)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;

	if (nsp == NULL)
		return(ENOTCONN);
	cb = nstosppcb(nsp);
	if (cb->s_state > TCPS_LISTEN)
		spp_disconnect(cb);
	else
		spp_close(cb);
	return(0);
}

/*
 * We may decide later to implement connection closing
 * handshaking at the spp level optionally.
 * here is the hook to do it:
 */
static int
spp_usr_disconnect(struct socket *so)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		spp_disconnect(cb);
		error = 0;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_listen(struct socket *so, struct thread *td)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		error = 0;
		if (nsp->nsp_lport == 0)
			error = ns_pcbbind(nsp, NULL);
		if (error == 0)
			cb->s_state = TCPS_LISTEN;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_peeraddr(struct socket *so, struct sockaddr **nam)
{
	struct nspcb *nsp = sotonspcb(so);
	int error;

	if (nsp) {
		ns_setpeeraddr(nsp, nam);
		error = 0;
	} else {
		error = EINVAL;
	}	
	return(error);
}

static int
spp_rcvd(struct socket *so, int flags)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		cb->s_flags |= SF_RVD;
		spp_output(cb, (struct mbuf *) 0);
		cb->s_flags &= ~SF_RVD;
		error = 0;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_rcvoob(struct socket *so, struct mbuf *m, int flags)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		if ((cb->s_oobflags & SF_IOOB) || so->so_oobmark ||
		    (so->so_state & SS_RCVATMARK)) {
			m->m_len = 1;
			*mtod(m, caddr_t) = cb->s_iobc;
			error = 0;
		} else {
			error = EINVAL;
		}
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_send(struct socket *so, int flags, struct mbuf *m,
	struct sockaddr *addr, struct mbuf *control,
	struct thread *td)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		error = 0;
		if (flags & PRUS_OOB) {
			if (sbspace(&so->so_snd) < -512) {
				error = ENOBUFS;
			} else {
				cb->s_oobflags |= SF_SOOB;
			}
		}
		if (error == 0) {
			if (control) {
				u_short *p = mtod(control, u_short *);
				spp_newchecks[2]++;
				/* XXXX, for testing */
				if ((p[0] == 5) && p[1] == 1) { 
					cb->s_shdr.sp_dt = *(u_char *)(&p[2]);
					spp_newchecks[3]++;
				}
				m_freem(control);
				control = NULL;
			}
			error = spp_output(cb, m);
			m = NULL;
		}
	} else {
		error = EINVAL;
	}
	if (m)
		m_freem(m);
	if (control)
		m_freem(control);
	return(error);
}

static int
spp_shutdown(struct socket *so)
{
	struct nspcb *nsp = sotonspcb(so);
	struct sppcb *cb;
	int error;

	if (nsp) {
		cb = nstosppcb(nsp);
		socantsendmore(so);
		if ((cb = spp_usrclosed(cb)) != NULL)
			error = spp_output(cb, NULL);
		else
			error = 0;
	} else {
		error = EINVAL;
	}
	return(error);
}

static int
spp_sockaddr(struct socket *so, struct sockaddr **nam)
{
	struct nspcb *nsp = sotonspcb(so);
	int error;

	if (nsp) {
		ns_setsockaddr(nsp, nam);
		error = 0;
	} else {
		error = EINVAL;
	}	
	return(error);
}

struct pr_usrreqs spp_usrreqs = {
	spp_usr_abort, spp_accept, spp_attach, spp_bind,
	spp_connect, pru_connect2_notsupp, ns_control, spp_detach,
	spp_usr_disconnect, spp_listen, spp_peeraddr, spp_rcvd,
	spp_rcvoob, spp_send, pru_sense_null, spp_shutdown,
	spp_sockaddr, sosend, soreceive, sopoll
};

struct pr_usrreqs spp_usrreqs_sp = {
	spp_usr_abort, spp_accept, spp_attach_sp, spp_bind,
	spp_connect, pru_connect2_notsupp, ns_control, spp_detach,
	spp_usr_disconnect, spp_listen, spp_peeraddr, spp_rcvd,
	spp_rcvoob, spp_send, pru_sense_null, spp_shutdown,
	spp_sockaddr, sosend, soreceive, sopoll
};

/*
 * Create template to be used to send spp packets on a connection.
 * Called after host entry created, fills
 * in a skeletal spp header (choosing connection id),
 * minimizing the amount of work necessary when the connection is used.
 */
void
spp_template(struct sppcb *cb)
{
	struct nspcb *nsp = cb->s_nspcb;
	struct idp *idp = cb->s_idp;
	struct sockbuf *sb = &(nsp->nsp_socket->so_snd);

	idp->idp_pt = NSPROTO_SPP;
	idp->idp_sna = nsp->nsp_laddr;
	idp->idp_dna = nsp->nsp_faddr;
	cb->s_sid = htons(spp_iss);
	spp_iss += SPP_ISSINCR/2;
	cb->s_alo = 1;
	cb->s_cwnd = (sbspace(sb) * CUNIT) / cb->s_mtu;
	cb->s_ssthresh = cb->s_cwnd; /* Try to expand fast to full complement
					of large packets */
	cb->s_cwmx = (sbspace(sb) * CUNIT) / (2 * sizeof(struct spidp));
	cb->s_cwmx = max(cb->s_cwmx, cb->s_cwnd);
		/* But allow for lots of little packets as well */
}

/*
 * Close a SPIP control block:
 *	discard spp control block itself
 *	discard ns protocol control block
 *	wake up any sleepers
 */
struct sppcb *
spp_close(struct sppcb *cb)
{
	struct spidp_q *q;
	struct spidp_q *oq;
	struct nspcb *nsp = cb->s_nspcb;
	struct socket *so = nsp->nsp_socket;
	struct mbuf *m;

	q = cb->s_q.si_next;
	while (q != &(cb->s_q)) {
		oq = q;
		q = q->si_next;
		m = oq->si_mbuf;
		remque(oq);
		m_freem(m);
		free(oq, M_SPIDP_Q);
	}
	free(cb->s_idp, M_IDP);
	free(cb, M_SPPCB);
	nsp->nsp_pcb = 0;
	soisdisconnected(so);
	ns_pcbdetach(nsp);
	sppstat.spps_closed++;
	return ((struct sppcb *)0);
}
/*
 *	Someday we may do level 3 handshaking
 *	to close a connection or send a xerox style error.
 *	For now, just close.
 */
struct sppcb *
spp_usrclosed(struct sppcb *cb)
{
	return (spp_close(cb));
}

struct sppcb *
spp_disconnect(struct sppcb *cb)
{
	return (spp_close(cb));
}

/*
 * Drop connection, reporting
 * the specified error.
 */
struct sppcb *
spp_drop(struct sppcb *cb, int errno)
{
	struct socket *so = cb->s_nspcb->nsp_socket;

	/*
	 * someday, in the xerox world
	 * we will generate error protocol packets
	 * announcing that the socket has gone away.
	 */
	if (TCPS_HAVERCVDSYN(cb->s_state)) {
		sppstat.spps_drops++;
		cb->s_state = TCPS_CLOSED;
		/*(void) tcp_output(cb);*/
	} else
		sppstat.spps_conndrops++;
	so->so_error = errno;
	return (spp_close(cb));
}

void
spp_abort(struct nspcb *nsp)
{
	spp_close((struct sppcb *)nsp->nsp_pcb);
}

/*
 * Fast timeout routine for processing delayed acks
 */
void
spp_fasttimo(void)
{
	struct nspcb *nsp;
	struct sppcb *cb;
	int s = splnet();

	nsp = nspcb.nsp_next;
	if (nsp)
	for (; nsp != &nspcb; nsp = nsp->nsp_next)
		if ((cb = (struct sppcb *)nsp->nsp_pcb) &&
		    (cb->s_flags & SF_DELACK)) {
			cb->s_flags &= ~SF_DELACK;
			cb->s_flags |= SF_ACKNOW;
			sppstat.spps_delack++;
			(void) spp_output(cb, (struct mbuf *) 0);
		}
	splx(s);
}

/*
 * spp protocol timeout routine called every 500 ms.
 * Updates the timers in all active pcb's and
 * causes finite state machine actions if timers expire.
 */
void
spp_slowtimo(void)
{
	struct nspcb *ip, *ipnxt;
	struct sppcb *cb;
	int s = splnet();
	int i;

	/*
	 * Search through tcb's and update active timers.
	 */
	ip = nspcb.nsp_next;
	if (ip == 0) {
		splx(s);
		return;
	}
	while (ip != &nspcb) {
		cb = nstosppcb(ip);
		ipnxt = ip->nsp_next;
		if (cb == 0)
			goto tpgone;
		for (i = 0; i < SPPT_NTIMERS; i++) {
			if (cb->s_timer[i] && --cb->s_timer[i] == 0) {
				spp_timers(cb, i);
				if (ipnxt->nsp_prev != ip)
					goto tpgone;
			}
		}
		cb->s_idle++;
		if (cb->s_rtt)
			cb->s_rtt++;
tpgone:
		ip = ipnxt;
	}
	spp_iss += SPP_ISSINCR/PR_SLOWHZ;		/* increment iss */
	splx(s);
}
/*
 * SPP timer processing.
 */
struct sppcb *
spp_timers(struct sppcb *cb, int timer)
{
	long rexmt;
	int win;

	cb->s_force = 1 + timer;
	switch (timer) {

	/*
	 * 2 MSL timeout in shutdown went off.  TCP deletes connection
	 * control block.
	 */
	case SPPT_2MSL:
		printf("spp: SPPT_2MSL went off for no reason\n");
		cb->s_timer[timer] = 0;
		break;

	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one packet.
	 */
	case SPPT_REXMT:
		if (++cb->s_rxtshift > SPP_MAXRXTSHIFT) {
			cb->s_rxtshift = SPP_MAXRXTSHIFT;
			sppstat.spps_timeoutdrop++;
			cb = spp_drop(cb, ETIMEDOUT);
			break;
		}
		sppstat.spps_rexmttimeo++;
		rexmt = ((cb->s_srtt >> 2) + cb->s_rttvar) >> 1;
		rexmt *= spp_backoff[cb->s_rxtshift];
		SPPT_RANGESET(cb->s_rxtcur, rexmt, SPPTV_MIN, SPPTV_REXMTMAX);
		cb->s_timer[SPPT_REXMT] = cb->s_rxtcur;
		/*
		 * If we have backed off fairly far, our srtt
		 * estimate is probably bogus.  Clobber it
		 * so we'll take the next rtt measurement as our srtt;
		 * move the current srtt into rttvar to keep the current
		 * retransmit times until then.
		 */
		if (cb->s_rxtshift > SPP_MAXRXTSHIFT / 4 ) {
			cb->s_rttvar += (cb->s_srtt >> 2);
			cb->s_srtt = 0;
		}
		cb->s_snxt = cb->s_rack;
		/*
		 * If timing a packet, stop the timer.
		 */
		cb->s_rtt = 0;
		/*
		 * See very long discussion in tcp_timer.c about congestion
		 * window and sstrhesh
		 */
		win = min(cb->s_swnd, (cb->s_cwnd/CUNIT)) / 2;
		if (win < 2)
			win = 2;
		cb->s_cwnd = CUNIT;
		cb->s_ssthresh = win * CUNIT;
		(void) spp_output(cb, (struct mbuf *) 0);
		break;

	/*
	 * Persistance timer into zero window.
	 * Force a probe to be sent.
	 */
	case SPPT_PERSIST:
		sppstat.spps_persisttimeo++;
		spp_setpersist(cb);
		(void) spp_output(cb, (struct mbuf *) 0);
		break;

	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	case SPPT_KEEP:
		sppstat.spps_keeptimeo++;
		if (cb->s_state < TCPS_ESTABLISHED)
			goto dropit;
		if (cb->s_nspcb->nsp_socket->so_options & SO_KEEPALIVE) {
		    	if (cb->s_idle >= SPPTV_MAXIDLE)
				goto dropit;
			sppstat.spps_keepprobe++;
			(void) spp_output(cb, (struct mbuf *) 0);
		} else
			cb->s_idle = 0;
		cb->s_timer[SPPT_KEEP] = SPPTV_KEEP;
		break;
	dropit:
		sppstat.spps_keepdrops++;
		cb = spp_drop(cb, ETIMEDOUT);
		break;
	}
	return (cb);
}
#ifndef lint
int SppcbSize = sizeof (struct sppcb);
int NspcbSize = sizeof (struct nspcb);
#endif /* lint */
