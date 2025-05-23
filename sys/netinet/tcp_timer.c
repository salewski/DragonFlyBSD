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
 *	@(#)tcp_timer.c	8.2 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_timer.c,v 1.34.2.14 2003/02/03 02:33:41 hsu Exp $
 * $DragonFly: src/sys/netinet/tcp_timer.c,v 1.13 2004/12/21 02:54:15 hsu Exp $
 */

#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_tcpdebug.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/thread.h>
#include <sys/globaldata.h>

#include <machine/cpu.h>	/* before tcp_seq.h, for tcp_random18() */

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet6/in6_pcb.h>
#endif
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#ifdef TCPDEBUG
#include <netinet/tcp_debug.h>
#endif

static int
sysctl_msec_to_ticks(SYSCTL_HANDLER_ARGS)
{
	int error, s, tt;

	tt = *(int *)oidp->oid_arg1;
	s = (int)((int64_t)tt * 1000 / hz);

	error = sysctl_handle_int(oidp, &s, 0, req);
	if (error || !req->newptr)
		return (error);

	tt = (int)((int64_t)s * hz / 1000);
	if (tt < 1)
		return (EINVAL);

	*(int *)oidp->oid_arg1 = tt;
	return (0);
}

int	tcp_keepinit;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINIT, keepinit, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepinit, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_keepidle;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPIDLE, keepidle, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepidle, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_keepintvl;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_KEEPINTVL, keepintvl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_keepintvl, 0, sysctl_msec_to_ticks, "I", "");

int	tcp_delacktime;
SYSCTL_PROC(_net_inet_tcp, TCPCTL_DELACKTIME, delacktime,
    CTLTYPE_INT|CTLFLAG_RW, &tcp_delacktime, 0, sysctl_msec_to_ticks, "I",
    "Time before a delayed ACK is sent");

int	tcp_msl;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, msl, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_msl, 0, sysctl_msec_to_ticks, "I", "Maximum segment lifetime");

int	tcp_rexmit_min;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_min, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_min, 0, sysctl_msec_to_ticks, "I", "Minimum Retransmission Timeout");

int	tcp_rexmit_slop;
SYSCTL_PROC(_net_inet_tcp, OID_AUTO, rexmit_slop, CTLTYPE_INT|CTLFLAG_RW,
    &tcp_rexmit_slop, 0, sysctl_msec_to_ticks, "I", "Retransmission Timer Slop");

static int	always_keepalive = 0;
SYSCTL_INT(_net_inet_tcp, OID_AUTO, always_keepalive, CTLFLAG_RW,
    &always_keepalive , 0, "Assume SO_KEEPALIVE on all TCP connections");

static int	tcp_keepcnt = TCPTV_KEEPCNT;
	/* max idle probes */
int	tcp_maxpersistidle;
	/* max idle time in persist */
int	tcp_maxidle;

/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates timestamps used for TCP
 * causes finite state machine actions if timers expire.
 */
void
tcp_slowtimo(void)
{
	int s;

	s = splnet();

	tcp_maxidle = tcp_keepcnt * tcp_keepintvl;

	splx(s);
}

/*
 * Cancel all timers for TCP tp.
 */
void
tcp_canceltimers(struct tcpcb *tp)
{
	callout_stop(tp->tt_2msl);
	callout_stop(tp->tt_persist);
	callout_stop(tp->tt_keep);
	callout_stop(tp->tt_rexmt);
}

int	tcp_syn_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 1, 1, 1, 1, 2, 4, 8, 16, 32, 64, 64, 64 };

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

static int tcp_totbackoff = 511;	/* sum of tcp_backoff[] */

/*
 * TCP timer processing.
 */
void
tcp_timer_delack(void *xtp)
{
	struct tcpcb *tp = xtp;
	int s;

	s = splnet();
	if (callout_pending(tp->tt_delack) || !callout_active(tp->tt_delack)) {
		splx(s);
		return;
	}
	callout_deactivate(tp->tt_delack);

	tp->t_flags |= TF_ACKNOW;
	tcpstat.tcps_delack++;
	tcp_output(tp);
	splx(s);
}

void
tcp_timer_2msl(void *xtp)
{
	struct tcpcb *tp = xtp;
	int s;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	s = splnet();
	if (callout_pending(tp->tt_2msl) || !callout_active(tp->tt_2msl)) {
		splx(s);
		return;
	}
	callout_deactivate(tp->tt_2msl);
	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long, or if 2MSL time is up from TIME_WAIT, delete connection
	 * control block.  Otherwise, check again in a bit.
	 */
	if (tp->t_state != TCPS_TIME_WAIT &&
	    (ticks - tp->t_rcvtime) <= tcp_maxidle)
		callout_reset(tp->tt_2msl, tcp_keepintvl,
			      tcp_timer_2msl, tp);
	else
		tp = tcp_close(tp);

#ifdef TCPDEBUG
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	splx(s);
}

void
tcp_timer_keep(void *xtp)
{
	struct tcpcb *tp = xtp;
	struct tcptemp *t_template;
	int s;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	s = splnet();
	if (callout_pending(tp->tt_keep) || !callout_active(tp->tt_keep)) {
		splx(s);
		return;
	}
	callout_deactivate(tp->tt_keep);
	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
	tcpstat.tcps_keeptimeo++;
	if (tp->t_state < TCPS_ESTABLISHED)
		goto dropit;
	if ((always_keepalive ||
	     tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if ((ticks - tp->t_rcvtime) >= tcp_keepidle + tcp_maxidle)
			goto dropit;
		/*
		 * Send a packet designed to force a response
		 * if the peer is up and reachable:
		 * either an ACK if the connection is still alive,
		 * or an RST if the peer has closed the connection
		 * due to timeout or reboot.
		 * Using sequence number tp->snd_una-1
		 * causes the transmitted zero-length segment
		 * to lie outside the receive window;
		 * by the protocol spec, this requires the
		 * correspondent TCP to respond.
		 */
		tcpstat.tcps_keepprobe++;
		t_template = tcp_maketemplate(tp);
		if (t_template) {
			tcp_respond(tp, t_template->tt_ipgen,
				    &t_template->tt_t, (struct mbuf *)NULL,
				    tp->rcv_nxt, tp->snd_una - 1, 0);
			tcp_freetemplate(t_template);
		}
		callout_reset(tp->tt_keep, tcp_keepintvl, tcp_timer_keep, tp);
	} else
		callout_reset(tp->tt_keep, tcp_keepidle, tcp_timer_keep, tp);

#ifdef TCPDEBUG
	if (tp->t_inpcb->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	splx(s);
	return;

dropit:
	tcpstat.tcps_keepdrops++;
	tp = tcp_drop(tp, ETIMEDOUT);

#ifdef TCPDEBUG
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	splx(s);
}

void
tcp_timer_persist(void *xtp)
{
	struct tcpcb *tp = xtp;
	int s;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	s = splnet();
	if (callout_pending(tp->tt_persist) || !callout_active(tp->tt_persist)){
		splx(s);
		return;
	}
	callout_deactivate(tp->tt_persist);
	/*
	 * Persistance timer into zero window.
	 * Force a byte to be output, if possible.
	 */
	tcpstat.tcps_persisttimeo++;
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
	    ((ticks - tp->t_rcvtime) >= tcp_maxpersistidle ||
	     (ticks - tp->t_rcvtime) >= TCP_REXMTVAL(tp) * tcp_totbackoff)) {
		tcpstat.tcps_persistdrop++;
		tp = tcp_drop(tp, ETIMEDOUT);
		goto out;
	}
	tcp_setpersist(tp);
	tp->t_flags |= TF_FORCE;
	tcp_output(tp);
	tp->t_flags &= ~TF_FORCE;

out:
#ifdef TCPDEBUG
	if (tp && tp->t_inpcb->inp_socket->so_options & SO_DEBUG)
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	splx(s);
}

void
tcp_save_congestion_state(struct tcpcb *tp)
{
	tp->snd_cwnd_prev = tp->snd_cwnd;
	tp->snd_ssthresh_prev = tp->snd_ssthresh;
	tp->snd_recover_prev = tp->snd_recover;
	if (IN_FASTRECOVERY(tp))
	  tp->t_flags |= TF_WASFRECOVERY;
	else
	  tp->t_flags &= ~TF_WASFRECOVERY;
	if (tp->t_flags & TF_RCVD_TSTMP) {
		tp->t_rexmtTS = ticks;
		tp->t_flags |= TF_FIRSTACCACK;
	}
#ifdef later
	tcp_sack_save_scoreboard(&tp->scb);
#endif
}

void
tcp_revert_congestion_state(struct tcpcb *tp)
{
	tp->snd_cwnd = tp->snd_cwnd_prev;
	tp->snd_ssthresh = tp->snd_ssthresh_prev;
	tp->snd_recover = tp->snd_recover_prev;
	if (tp->t_flags & TF_WASFRECOVERY)
	    ENTER_FASTRECOVERY(tp);
	if (tp->t_flags & TF_FASTREXMT) {
		++tcpstat.tcps_sndfastrexmitbad;
		if (tp->t_flags & TF_EARLYREXMT)
			++tcpstat.tcps_sndearlyrexmitbad;
	} else
		++tcpstat.tcps_sndrtobad;
	tp->t_badrxtwin = 0;
	tp->t_rxtshift = 0;
	tp->snd_nxt = tp->snd_max;
#ifdef later
	tcp_sack_revert_scoreboard(&tp->scb, tp->snd_una);
#endif
}

void
tcp_timer_rexmt(void *xtp)
{
	struct tcpcb *tp = xtp;
	int s;
	int rexmt;
#ifdef TCPDEBUG
	int ostate;

	ostate = tp->t_state;
#endif
	s = splnet();
	if (callout_pending(tp->tt_rexmt) || !callout_active(tp->tt_rexmt)) {
		splx(s);
		return;
	}
	callout_deactivate(tp->tt_rexmt);
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		tcpstat.tcps_timeoutdrop++;
		tp = tcp_drop(tp, tp->t_softerror ?
			      tp->t_softerror : ETIMEDOUT);
		goto out;
	}
	if (tp->t_rxtshift == 1) {
		/*
		 * first retransmit; record ssthresh and cwnd so they can
		 * be recovered if this turns out to be a "bad" retransmit.
		 * A retransmit is considered "bad" if an ACK for this
		 * segment is received within RTT/2 interval; the assumption
		 * here is that the ACK was already in flight.  See
		 * "On Estimating End-to-End Network Path Properties" by
		 * Allman and Paxson for more details.
		 */
		tp->t_badrxtwin = ticks + (tp->t_srtt >> (TCP_RTT_SHIFT + 1));
		tcp_save_congestion_state(tp);
		tp->t_flags &= ~(TF_FASTREXMT | TF_EARLYREXMT);
	}
	/* Throw away SACK blocks on a RTO, as specified by RFC2018. */
	tcp_sack_cleanup(&tp->scb);
	tcpstat.tcps_rexmttimeo++;
	if (tp->t_state == TCPS_SYN_SENT)
		rexmt = TCP_REXMTVAL(tp) * tcp_syn_backoff[tp->t_rxtshift];
	else
		rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);
	/*
	 * Disable rfc1323 and rfc1644 if we havn't got any response to
	 * our third SYN to work-around some broken terminal servers
	 * (most of which have hopefully been retired) that have bad VJ
	 * header compression code which trashes TCP segments containing
	 * unknown-to-them TCP options.
	 */
	if ((tp->t_state == TCPS_SYN_SENT) && (tp->t_rxtshift == 3))
		tp->t_flags &= ~(TF_REQ_SCALE|TF_REQ_TSTMP|TF_REQ_CC);
	/*
	 * If losing, let the lower level know and try for
	 * a better route.  Also, if we backed off this far,
	 * our srtt estimate is probably bogus.  Clobber it
	 * so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current
	 * retransmit times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
#ifdef INET6
		if ((tp->t_inpcb->inp_vflag & INP_IPV6) != 0)
			in6_losing(tp->t_inpcb);
		else
#endif
		in_losing(tp->t_inpcb);
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
	}
	tp->snd_nxt = tp->snd_una;
	tp->rexmt_high = tp->snd_una;
	tp->snd_recover = tp->snd_max;
	/*
	 * Force a segment to be sent.
	 */
	tp->t_flags |= TF_ACKNOW;
	/*
	 * If timing a segment in this window, stop the timer.
	 */
	tp->t_rtttime = 0;
	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshhold size.
	 * For a threshhold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshhold
	 * to go below this.)
	 */
	{
		u_int win = min(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
		if (win < 2)
			win = 2;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_ssthresh = win * tp->t_maxseg;
		tp->t_dupacks = 0;
	}
	EXIT_FASTRECOVERY(tp);
	tcp_output(tp);

out:
#ifdef TCPDEBUG
	if (tp && (tp->t_inpcb->inp_socket->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, NULL, NULL, PRU_SLOWTIMO);
#endif
	splx(s);
}
