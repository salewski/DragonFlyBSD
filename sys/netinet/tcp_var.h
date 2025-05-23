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
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * $FreeBSD: src/sys/netinet/tcp_var.h,v 1.56.2.13 2003/02/03 02:34:07 hsu Exp $
 * $DragonFly: src/sys/netinet/tcp_var.h,v 1.31 2005/03/10 08:19:27 hsu Exp $
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_

#include <netinet/in_pcb.h>		/* needed for in_conninfo, inp_gen_t */

/*
 * Kernel variables for tcp.
 */
extern int	tcp_do_rfc1323;
extern int	tcp_do_rfc1644;
extern int	tcp_do_sack;
extern int	tcp_do_smartsack;

/* TCP segment queue entry */
struct tseg_qent {
	LIST_ENTRY(tseg_qent) tqe_q;
	int	tqe_len;		/* TCP segment data length */
	struct	tcphdr *tqe_th;		/* a pointer to tcp header */
	struct	mbuf	*tqe_m;		/* mbuf contains packet */
};
LIST_HEAD(tsegqe_head, tseg_qent);
extern int	tcp_reass_maxseg;
extern int	tcp_reass_qsize;
#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_TSEGQ);
#endif

struct tcptemp {
	u_char	tt_ipgen[40]; /* the size must be of max ip header, now IPv6 */
	struct	tcphdr tt_t;
};

#define tcp6cb		tcpcb  /* for KAME src sync over BSD*'s */

struct raw_sackblock {				/* covers [start, end) */
	tcp_seq rblk_start;
	tcp_seq rblk_end;
};

/* maximum number of SACK blocks that will fit in the TCP option space */
#define	MAX_SACK_REPORT_BLOCKS	4

TAILQ_HEAD(sackblock_list, sackblock);

struct scoreboard {
	int nblocks;
	struct sackblock_list sackblocks;
	tcp_seq lostseq;			/* passed SACK lost test */
	struct sackblock *lastfound;		/* search hint */
};

/*
 * Tcp control block, one per tcp; fields:
 * Organized for 16 byte cacheline efficiency.
 */
struct tcpcb {
	struct	tsegqe_head t_segq;
	int	t_dupacks;		/* consecutive dup acks recd */
	int	tt_cpu;			/* sanity check the cpu */
	struct	callout *tt_rexmt;	/* retransmit timer */

	struct	callout *tt_persist;	/* retransmit persistence */
	struct	callout *tt_keep;	/* keepalive */
	struct	callout *tt_2msl;	/* 2*msl TIME_WAIT timer */
	struct	callout *tt_delack;	/* delayed ACK timer */

	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
	int	t_state;		/* state of this connection */
	u_int	t_flags;
#define	TF_ACKNOW	0x00000001	/* ack peer immediately */
#define	TF_DELACK	0x00000002	/* ack, but try to delay it */
#define	TF_NODELAY	0x00000004	/* don't delay packets to coalesce */
#define	TF_NOOPT	0x00000008	/* don't use tcp options */
#define	TF_SENTFIN	0x00000010	/* have sent FIN */
#define	TF_REQ_SCALE	0x00000020	/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x00000040	/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x00000080	/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x00000100	/* a timestamp was received in SYN */
#define	TF_SACK_PERMITTED 0x00000200	/* other side said I could SACK */
#define	TF_NEEDSYN	0x00000400	/* send SYN (implicit state) */
#define	TF_NEEDFIN	0x00000800	/* send FIN (implicit state) */
#define	TF_NOPUSH	0x00001000	/* don't push */
#define	TF_REQ_CC	0x00002000	/* have/will request CC */
#define	TF_RCVD_CC	0x00004000	/* a CC was received in SYN */
#define	TF_SENDCCNEW	0x00008000	/* send CCnew instead of CC in SYN */
#define	TF_MORETOCOME	0x00010000	/* More data to be appended to sock */
#define	TF_LQ_OVERFLOW	0x00020000	/* listen queue overflow */
#define	TF_LASTIDLE	0x00040000	/* connection was previously idle */
#define	TF_RXWIN0SENT	0x00080000	/* sent a receiver win 0 in response */
#define	TF_FASTRECOVERY	0x00100000	/* in NewReno Fast Recovery */
#define	TF_WASFRECOVERY	0x00200000	/* was in NewReno Fast Recovery */
#define	TF_FIRSTACCACK	0x00400000	/* Look for 1st acceptable ACK. */
#define	TF_FASTREXMT	0x00800000	/* Did Fast Retransmit. */
#define	TF_EARLYREXMT	0x01000000	/* Did Early (Fast) Retransmit. */
#define	TF_FORCE	0x02000000	/* Set if forcing out a byte */
#define TF_ONOUTPUTQ	0x04000000	/* on t_outputq list */
#define TF_DUPSEG	0x08000000	/* last seg a duplicate */
#define TF_ENCLOSESEG	0x10000000	/* enclosing SACK block */
#define TF_SACKLEFT	0x20000000	/* send SACK blocks from left side */
	tcp_seq	snd_up;			/* send urgent pointer */

	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_recover;		/* for use with NewReno Fast Recovery */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
	tcp_seq	snd_nxt;		/* send next */

	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	tcp_seq	irs;			/* initial receive sequence number */

	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_adv;		/* advertised window */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_up;			/* receive urgent pointer */

	u_long	snd_wnd;		/* send window */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_bwnd;		/* bandwidth-controlled window */
	u_long	snd_ssthresh;		/* snd_cwnd size threshold for
					 * for slow start exponential to
					 * linear switch
					 */
	u_long	snd_bandwidth;		/* calculated bandwidth or 0 */

	u_int	t_maxopd;		/* mss plus options */

	u_long	t_rcvtime;		/* inactivity time */
	u_long	t_starttime;		/* time connection was established */
	int	t_rtttime;		/* round trip time */
	tcp_seq	t_rtseq;		/* sequence number being timed */

	int	t_bw_rtttime;		/* used for bandwidth calculation */
	tcp_seq	t_bw_rtseq;		/* used for bandwidth calculation */

	int	t_rxtcur;		/* current retransmit value (ticks) */
	u_int	t_maxseg;		/* maximum segment size */
	int	t_srtt;			/* smoothed round-trip time */
	int	t_rttvar;		/* variance in round-trip time */

	int	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	u_int	t_rttmin;		/* minimum rtt allowed */
	u_int	t_rttbest;		/* best rtt we've seen */
	u_long	t_rttupdated;		/* number of times rtt sampled */
	u_long	max_sndwnd;		/* largest window peer has offered */

	int	t_softerror;		/* possible error not yet reported */
/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02
/* RFC 1323 variables */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_char	requested_s_scale;
	u_long	ts_recent;		/* timestamp echo data */

	u_long	ts_recent_age;		/* when last updated */
	tcp_seq	last_ack_sent;
/* RFC 1644 variables */
	tcp_cc	cc_send;		/* send connection count */
	tcp_cc	cc_recv;		/* receive connection count */
/* experimental */
	u_long	snd_cwnd_prev;		/* cwnd prior to retransmit */
	u_long	snd_ssthresh_prev;	/* ssthresh prior to retransmit */
	tcp_seq snd_recover_prev;	/* snd_recover prior to retransmit */
	u_long	t_badrxtwin;		/* window for retransmit recovery */
	u_long	t_rexmtTS;		/* timestamp of last retransmit */
	u_char	snd_limited;		/* segments limited transmitted */

	tcp_seq	rexmt_high;		/* highest seq # retransmitted + 1 */
	tcp_seq	snd_max_rexmt;		/* snd_max when rexmting snd_una */
	struct scoreboard scb;		/* sack scoreboard */
	struct raw_sackblock reportblk; /* incoming segment or D-SACK block */
	struct raw_sackblock encloseblk;
	int	nsackhistory;
	struct raw_sackblock sackhistory[MAX_SACK_REPORT_BLOCKS]; /* reported */
	TAILQ_ENTRY(tcpcb) t_outputq;	/* tcp_output needed list */
};

#define	IN_FASTRECOVERY(tp)	(tp->t_flags & TF_FASTRECOVERY)
#define	ENTER_FASTRECOVERY(tp)	tp->t_flags |= TF_FASTRECOVERY
#define	EXIT_FASTRECOVERY(tp)	tp->t_flags &= ~TF_FASTRECOVERY

#ifdef _KERNEL

#if defined(SMP)
#define _GD	mycpu
#define tcpstat	tcpstats_ary[_GD->gd_cpuid]
#else
#define tcpstat	tcpstats_ary[0]
#endif

struct tcp_stats;
extern struct tcp_stats		tcpstats_ary[MAXCPU];

static const int tcprexmtthresh = 3;
#endif

/*
 * TCP statistics.
 */
struct tcp_stats {
	u_long	tcps_connattempt;	/* connections initiated */
	u_long	tcps_accepts;		/* connections accepted */
	u_long	tcps_connects;		/* connections established */
	u_long	tcps_drops;		/* connections dropped */
	u_long	tcps_conndrops;		/* embryonic connections dropped */
	u_long	tcps_closed;		/* conn. closed (includes drops) */
	u_long	tcps_segstimed;		/* segs where we tried to get rtt */
	u_long	tcps_rttupdated;	/* times we succeeded */
	u_long	tcps_delack;		/* delayed acks sent */
	u_long	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_long	tcps_rexmttimeo;	/* retransmit timeouts */
	u_long	tcps_persisttimeo;	/* persist timeouts */
	u_long	tcps_keeptimeo;		/* keepalive timeouts */
	u_long	tcps_keepprobe;		/* keepalive probes sent */
	u_long	tcps_keepdrops;		/* connections dropped in keepalive */

	u_long	tcps_sndtotal;		/* total packets sent */
	u_long	tcps_sndpack;		/* data packets sent */
	u_long	tcps_sndbyte;		/* data bytes sent */
	u_long	tcps_sndrexmitpack;	/* data packets retransmitted */
	u_long	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_long	tcps_sndfastrexmit;	/* Fast Retransmissions */
	u_long	tcps_sndearlyrexmit;	/* early Fast Retransmissions */
	u_long	tcps_sndlimited;	/* Limited Transmit packets */
	u_long	tcps_sndrtobad;		/* spurious RTO retransmissions */
	u_long	tcps_sndfastrexmitbad;	/* spurious Fast Retransmissions */
	u_long	tcps_sndearlyrexmitbad;	/* spurious early Fast Retransmissions,
					   a subset of tcps_sndfastrexmitbad */
	u_long	tcps_eifeldetected;	/* Eifel-detected spurious rexmits */
	u_long	tcps_rttcantdetect;	/* Eifel but not 1/2 RTT-detectable */
	u_long	tcps_rttdetected;	/* RTT-detected spurious RTO rexmits */
	u_long	tcps_sndacks;		/* ack-only packets sent */
	u_long	tcps_sndprobe;		/* window probes sent */
	u_long	tcps_sndurg;		/* packets sent with URG only */
	u_long	tcps_sndwinup;		/* window update-only packets sent */
	u_long	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */
	u_long	tcps_sndsackpack;	/* packets sent by SACK recovery alg */
	u_long	tcps_sndsackbyte;	/* bytes sent by SACK recovery */
	u_long	tcps_snduna3;		/* re-retransmit snd_una on 3 new seg */
	u_long	tcps_snduna1;		/* re-retransmit snd_una on 1 new seg */

	u_long	tcps_rcvtotal;		/* total packets received */
	u_long	tcps_rcvpack;		/* packets received in sequence */
	u_long	tcps_rcvbyte;		/* bytes received in sequence */
	u_long	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u_long	tcps_rcvbadoff;		/* packets received with bad offset */
	u_long	tcps_rcvmemdrop;	/* packets dropped for lack of memory */
	u_long	tcps_rcvshort;		/* packets received too short */
	u_long	tcps_rcvduppack;	/* duplicate-only packets received */
	u_long	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_long	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_long	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_long	tcps_rcvoopack;		/* out-of-order packets received */
	u_long	tcps_rcvoobyte;		/* out-of-order bytes received */
	u_long	tcps_rcvpackafterwin;	/* packets with data after window */
	u_long	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_long	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_long	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_long	tcps_rcvdupack;		/* rcvd duplicate acks */
	u_long	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_long	tcps_rcvackpack;	/* rcvd ack packets */
	u_long	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_long	tcps_rcvwinupd;		/* rcvd window update packets */
	u_long	tcps_pawsdrop;		/* segments dropped due to PAWS */
	u_long	tcps_predack;		/* times hdr predict ok for acks */
	u_long	tcps_preddat;		/* times hdr predict ok for data pkts */
	u_long	tcps_pcbcachemiss;
	u_long	tcps_cachedrtt;		/* times cached RTT in route updated */
	u_long	tcps_cachedrttvar;	/* times cached rttvar updated */
	u_long	tcps_cachedssthresh;	/* times cached ssthresh updated */
	u_long	tcps_usedrtt;		/* times RTT initialized from route */
	u_long	tcps_usedrttvar;	/* times RTTVAR initialized from rt */
	u_long	tcps_usedssthresh;	/* times ssthresh initialized from rt*/
	u_long	tcps_persistdrop;	/* timeout in persist state */
	u_long	tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
	u_long	tcps_mturesent;		/* resends due to MTU discovery */
	u_long	tcps_listendrop;	/* listen queue overflows */

	u_long	tcps_sc_added;		/* entry added to syncache */
	u_long	tcps_sc_retransmitted;	/* syncache entry was retransmitted */
	u_long	tcps_sc_dupsyn;		/* duplicate SYN packet */
	u_long	tcps_sc_dropped;	/* could not reply to packet */
	u_long	tcps_sc_completed;	/* successful extraction of entry */
	u_long	tcps_sc_bucketoverflow;	/* syncache per-bucket limit hit */
	u_long	tcps_sc_cacheoverflow;	/* syncache cache limit hit */
	u_long	tcps_sc_reset;		/* RST removed entry from syncache */
	u_long	tcps_sc_stale;		/* timed out or listen socket gone */
	u_long	tcps_sc_aborted;	/* syncache entry aborted */
	u_long	tcps_sc_badack;		/* removed due to bad ACK */
	u_long	tcps_sc_unreach;	/* ICMP unreachable received */
	u_long	tcps_sc_zonefail;	/* zalloc() failed */
	u_long	tcps_sc_sendcookie;	/* SYN cookie sent */
	u_long	tcps_sc_recvcookie;	/* SYN cookie received */
};

/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions.
 */
struct tcpopt {
	u_long		to_flags;	/* which options are present */
#define	TOF_TS			0x0001		/* timestamp */
#define	TOF_CC			0x0002		/* CC and CCnew are exclusive */
#define	TOF_CCNEW		0x0004
#define	TOF_CCECHO		0x0008
#define	TOF_MSS			0x0010
#define	TOF_SCALE		0x0020
#define	TOF_SACK_PERMITTED	0x0040
#define	TOF_SACK		0x0080
	u_int32_t	to_tsval;
	u_int32_t	to_tsecr;
	tcp_cc		to_cc;		/* holds CC or CCnew */
	tcp_cc		to_ccecho;
	u_int16_t	to_mss;
	u_int8_t	to_requested_s_scale;
	u_int8_t	to_nsackblocks;
	struct raw_sackblock *to_sackblocks;
};

struct syncache {
	inp_gen_t	sc_inp_gencnt;		/* pointer check */
	struct		tcpcb *sc_tp;		/* tcb for listening socket */
	struct		mbuf *sc_ipopts;	/* source route */
	struct		in_conninfo sc_inc;	/* addresses */
#define sc_route	sc_inc.inc_route
#define sc_route6	sc_inc.inc6_route
	u_int32_t	sc_tsrecent;
	tcp_cc		sc_cc_send;		/* holds CC or CCnew */
	tcp_cc		sc_cc_recv;
	tcp_seq		sc_irs;			/* seq from peer */
	tcp_seq		sc_iss;			/* our ISS */
	u_long		sc_rxttime;		/* retransmit time */
	u_int16_t	sc_rxtslot;		/* retransmit counter */
	u_int16_t	sc_peer_mss;		/* peer's MSS */
	u_int16_t	sc_wnd;			/* advertised window */
	u_int8_t	sc_requested_s_scale:4,
			sc_request_r_scale:4;
	u_int8_t	sc_flags;
#define SCF_NOOPT		0x01		/* no TCP options */
#define SCF_WINSCALE		0x02		/* negotiated window scaling */
#define SCF_TIMESTAMP		0x04		/* negotiated timestamps */
#define SCF_CC			0x08		/* negotiated CC */
#define SCF_UNREACH		0x10		/* icmp unreachable received */
#define	SCF_SACK_PERMITTED	0x20		/* saw SACK permitted option */
	TAILQ_ENTRY(syncache) sc_hash;
	TAILQ_ENTRY(syncache) sc_timerq;
};

struct syncache_head {
	TAILQ_HEAD(, syncache)	sch_bucket;
	u_int		sch_length;
};

/*
 * The TAO cache entry which is stored in the protocol family specific
 * portion of the route metrics.
 */
struct rmxp_tao {
	tcp_cc	tao_cc;			/* latest CC in valid SYN */
	tcp_cc	tao_ccsent;		/* latest CC sent to peer */
	u_short	tao_mssopt;		/* peer's cached MSS */

#ifdef notyet
	u_short	tao_flags;		/* cache status flags */
#define	TAOF_DONT	0x0001		/* peer doesn't understand rfc1644 */
#define	TAOF_OK		0x0002		/* peer does understand rfc1644 */
#define	TAOF_UNDEF	0		/* we don't know yet */
#endif
};

#define rmx_taop(rt)	((struct rmxp_tao *)(rt).rmx_filler)

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		32	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		5	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	16	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	4	/* shift for rttvar; 2 bits */
#define	TCP_DELTA_SHIFT		2	/* see tcp_input.c */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define	TCP_REXMTVAL(tp) \
	max((tp)->t_rttmin, (((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/*
 * TCB structure exported to user-land via sysctl(3).
 * Evil hack: declare only if in_pcb.h and sys/socketvar.h have been
 * included.  Not all of our clients do.
 */
#if defined(_NETINET_IN_PCB_H_) && defined(_SYS_SOCKETVAR_H_)
struct	xtcpcb {
	size_t	xt_len;
	struct	inpcb	xt_inp;
	struct	tcpcb	xt_tp;
	struct	xsocket	xt_socket;
	u_quad_t	xt_alignment_hack;
};
#endif

/*
 * Names for TCP sysctl objects
 */
#define	TCPCTL_DO_RFC1323	1	/* use RFC-1323 extensions */
#define	TCPCTL_DO_RFC1644	2	/* use RFC-1644 extensions */
#define	TCPCTL_MSSDFLT		3	/* MSS default */
#define TCPCTL_STATS		4	/* statistics (read-only) */
#define	TCPCTL_RTTDFLT		5	/* default RTT estimate */
#define	TCPCTL_KEEPIDLE		6	/* keepalive idle timer */
#define	TCPCTL_KEEPINTVL	7	/* interval to send keepalives */
#define	TCPCTL_SENDSPACE	8	/* send buffer space */
#define	TCPCTL_RECVSPACE	9	/* receive buffer space */
#define	TCPCTL_KEEPINIT		10	/* timeout for establishing syn */
#define	TCPCTL_PCBLIST		11	/* list of all outstanding PCBs */
#define	TCPCTL_DELACKTIME	12	/* time before sending delayed ACK */
#define	TCPCTL_V6MSSDFLT	13	/* MSS default for IPv6 */
#define	TCPCTL_MAXID		14

#define TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323", CTLTYPE_INT }, \
	{ "rfc1644", CTLTYPE_INT }, \
	{ "mssdflt", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "rttdflt", CTLTYPE_INT }, \
	{ "keepidle", CTLTYPE_INT }, \
	{ "keepintvl", CTLTYPE_INT }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "keepinit", CTLTYPE_INT }, \
	{ "pcblist", CTLTYPE_STRUCT }, \
	{ "delacktime", CTLTYPE_INT }, \
	{ "v6mssdflt", CTLTYPE_INT }, \
}

#ifdef _KERNEL
#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_inet_tcp);
#endif

#define TCP_DO_SACK(tp)		((tp)->t_flags & TF_SACK_PERMITTED)

TAILQ_HEAD(tcpcbackqhead,tcpcb);

extern	struct inpcbinfo tcbinfo[];
extern	struct tcpcbackqhead tcpcbackq[];

extern	int tcp_mssdflt;	/* XXX */
extern	int tcp_delack_enabled;
extern	int path_mtu_discovery;

int	 tcp_addrcpu(in_addr_t faddr, in_port_t fport,
	    in_addr_t laddr, in_port_t lport);
struct lwkt_port *
	tcp_addrport(in_addr_t faddr, in_port_t fport,
	    in_addr_t laddr, in_port_t lport);
void	 tcp_canceltimers (struct tcpcb *);
struct tcpcb *
	 tcp_close (struct tcpcb *);
void	 tcpmsg_service_loop (void *);
void	 tcp_ctlinput (int, struct sockaddr *, void *);
int	 tcp_ctloutput (struct socket *, struct sockopt *);
struct lwkt_port *
	 tcp_cport(int cpu);
struct tcpcb *
	 tcp_drop (struct tcpcb *, int);
void	 tcp_drain (void);
void	 tcp_fasttimo (void);
struct rmxp_tao *
	 tcp_gettaocache (struct in_conninfo *);
void	 tcp_init (void);
void	 tcp_thread_init (void);
void	 tcp_input (struct mbuf *, ...);
void	 tcp_mss (struct tcpcb *, int);
int	 tcp_mssopt (struct tcpcb *);
void	 tcp_drop_syn_sent (struct inpcb *, int);
void	 tcp_mtudisc (struct inpcb *, int);
struct tcpcb *
	 tcp_newtcpcb (struct inpcb *);
int	 tcp_output (struct tcpcb *);
void	 tcp_quench (struct inpcb *, int);
void	 tcp_respond (struct tcpcb *, void *,
	    struct tcphdr *, struct mbuf *, tcp_seq, tcp_seq, int);
struct rtentry *
	 tcp_rtlookup (struct in_conninfo *);
int	 tcp_sack_bytes_below(struct scoreboard *scb, tcp_seq seq);
void	 tcp_sack_cleanup(struct scoreboard *scb);
int	 tcp_sack_ndsack_blocks(struct raw_sackblock *blocks,
	    const int numblocks, tcp_seq snd_una);
void	 tcp_sack_fill_report(struct tcpcb *tp, u_char *opt, u_int *plen);
boolean_t
	 tcp_sack_has_sacked(struct scoreboard *scb, u_int amount);
void	 tcp_sack_init(void);
void	 tcp_sack_tcpcb_init(struct tcpcb *tp);
uint32_t tcp_sack_compute_pipe(struct tcpcb *tp);
boolean_t
	 tcp_sack_nextseg(struct tcpcb *tp, tcp_seq *nextrexmt, uint32_t *len,
			  boolean_t *losdup);
#ifdef later
void	 tcp_sack_revert_scoreboard(struct scoreboard *scb, tcp_seq snd_una,
				    u_int maxseg);
void	 tcp_sack_save_scoreboard(struct scoreboard *scb);
#endif
void	 tcp_sack_skip_sacked(struct scoreboard *scb, tcp_seq *prexmt);
void	 tcp_sack_update_scoreboard(struct tcpcb *tp, struct tcpopt *to);
void	 tcp_save_congestion_state(struct tcpcb *tp);
void	 tcp_revert_congestion_state(struct tcpcb *tp);
void	 tcp_setpersist (struct tcpcb *);
void	 tcp_slowtimo (void);
struct tcptemp *tcp_maketemplate (struct tcpcb *);
void	 tcp_freetemplate (struct tcptemp *);
void	 tcp_fillheaders (struct tcpcb *, void *, void *);
struct lwkt_port *
	 tcp_soport(struct socket *, struct sockaddr *nam, int req);
struct tcpcb *
	 tcp_timers (struct tcpcb *, int);
void	 tcp_trace (int, int, struct tcpcb *, void *, struct tcphdr *,
			int);
void	 tcp_xmit_bandwidth_limit(struct tcpcb *tp, tcp_seq ack_seq);
void	 syncache_init(void);
void	 syncache_unreach(struct in_conninfo *, struct tcphdr *);
int	 syncache_expand(struct in_conninfo *, struct tcphdr *,
	     struct socket **, struct mbuf *);
int	 syncache_add(struct in_conninfo *, struct tcpopt *,
	     struct tcphdr *, struct socket **, struct mbuf *);
void	 syncache_chkrst(struct in_conninfo *, struct tcphdr *);
void	 syncache_badack(struct in_conninfo *);

extern	struct pr_usrreqs tcp_usrreqs;
extern	u_long tcp_sendspace;
extern	u_long tcp_recvspace;
tcp_seq tcp_new_isn (struct tcpcb *);

#endif /* _KERNEL */

#endif /* _NETINET_TCP_VAR_H_ */
