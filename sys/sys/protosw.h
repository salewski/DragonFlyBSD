/*-
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
 *	@(#)protosw.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/sys/sys/protosw.h,v 1.28.2.2 2001/07/03 11:02:01 ume Exp $
 * $DragonFly: src/sys/sys/protosw.h,v 1.14 2005/03/04 02:21:49 hsu Exp $
 */

#ifndef _SYS_PROTOSW_H_
#define _SYS_PROTOSW_H_

/* Forward declare these structures referenced from prototypes below. */
struct mbuf;
struct thread;
struct sockaddr;
struct socket;
struct sockopt;

struct pr_output_info {
	pid_t	p_pid;
};

/*#ifdef _KERNEL*/
/*
 * Protocol switch table.
 *
 * Each protocol has a handle initializing one of these structures,
 * which is used for protocol-protocol and system-protocol communication.
 *
 * A protocol is called through the pr_init entry before any other.
 * Thereafter it is called every 200ms through the pr_fasttimo entry and
 * every 500ms through the pr_slowtimo for timer based actions.
 * The system will call the pr_drain entry if it is low on space and
 * this should throw away any non-critical data.
 *
 * Protocols pass data between themselves as chains of mbufs using
 * the pr_input and pr_output hooks.  Pr_input passes data up (towards
 * the users) and pr_output passes it down (towards the interfaces); control
 * information passes up and down on pr_ctlinput and pr_ctloutput.
 * The protocol is responsible for the space occupied by any the
 * arguments to these entries and must dispose it.
 *
 * In retrospect, it would be a lot nicer to use an interface
 * similar to the vnode VOP interface.
 */
struct protosw {
	short	pr_type;		/* socket type used for */
	const struct domain *pr_domain; /* domain protocol a member of */
	short	pr_protocol;		/* protocol number */
	short	pr_flags;		/* see below */
/* protocol-protocol hooks */
	void	(*pr_input) (struct mbuf *, ...);
					/* input to protocol (from below) */
	int	(*pr_output)	(struct mbuf *, struct socket *, ...);
					/* output to protocol (from above) */
	void	(*pr_ctlinput)(int, struct sockaddr *, void *);
					/* control input (from below) */
	int	(*pr_ctloutput)(struct socket *, struct sockopt *);
					/* control output (from above) */
/* user-protocol hook */
	struct lwkt_port *(*pr_mport)(struct socket *, struct sockaddr *, int);
/* utility hooks */
	void	(*pr_init) (void);	/* initialization hook */
	void	(*pr_fasttimo) (void);
					/* fast timeout (200ms) */
	void	(*pr_slowtimo) (void);
					/* slow timeout (500ms) */
	void	(*pr_drain) (void);
					/* flush any excess space possible */
	const struct	pr_usrreqs *pr_usrreqs;	/* supersedes pr_usrreq() */
};
/*#endif*/

#define	PR_SLOWHZ	2		/* 2 slow timeouts per second */
#define	PR_FASTHZ	5		/* 5 fast timeouts per second */

/*
 * Values for pr_flags.
 * PR_ADDR requires PR_ATOMIC;
 * PR_ADDR and PR_CONNREQUIRED are mutually exclusive.
 * PR_IMPLOPCL means that the protocol allows sendto without prior connect,
 *	and the protocol understands the MSG_EOF flag.  The first property is
 *	is only relevant if PR_CONNREQUIRED is set (otherwise sendto is allowed
 *	anyhow).
 */
#define	PR_ATOMIC	0x01		/* exchange atomic messages only */
#define	PR_ADDR		0x02		/* addresses given with messages */
#define	PR_CONNREQUIRED	0x04		/* connection required by protocol */
#define	PR_WANTRCVD	0x08		/* want PRU_RCVD calls */
#define	PR_RIGHTS	0x10		/* passes capabilities */
#define PR_IMPLOPCL	0x20		/* implied open/close */
#define	PR_LASTHDR	0x40		/* enforce ipsec policy; last header */

/*
 * The arguments to usrreq are:
 *	(*protosw[].pr_usrreq)(up, req, m, nam, opt);
 * where up is a (struct socket *), req is one of these requests,
 * m is a optional mbuf chain containing a message,
 * nam is an optional mbuf chain containing an address,
 * and opt is a pointer to a socketopt structure or nil.
 * The protocol is responsible for disposal of the mbuf chain m,
 * the caller is responsible for any space held by nam and opt.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define	PRU_ATTACH		0	/* attach protocol to up */
#define	PRU_DETACH		1	/* detach protocol from up */
#define	PRU_BIND		2	/* bind socket to address */
#define	PRU_LISTEN		3	/* listen for connection */
#define	PRU_CONNECT		4	/* establish connection to peer */
#define	PRU_ACCEPT		5	/* accept connection from peer */
#define	PRU_DISCONNECT		6	/* disconnect from peer */
#define	PRU_SHUTDOWN		7	/* won't send any more data */
#define	PRU_RCVD		8	/* have taken data; more room now */
#define	PRU_SEND		9	/* send this data */
#define	PRU_ABORT		10	/* abort (fast DISCONNECT, DETATCH) */
#define	PRU_CONTROL		11	/* control operations on protocol */
#define	PRU_SENSE		12	/* return status into m */
#define	PRU_RCVOOB		13	/* retrieve out of band data */
#define	PRU_SENDOOB		14	/* send out of band data */
#define	PRU_SOCKADDR		15	/* fetch socket's address */
#define	PRU_PEERADDR		16	/* fetch peer's address */
#define	PRU_CONNECT2		17	/* connect two sockets */
#define PRU_SOPOLL		18
/* begin for protocols internal use */
#define	PRU_FASTTIMO		19	/* 200ms timeout */
#define	PRU_SLOWTIMO		20	/* 500ms timeout */
#define	PRU_PROTORCV		21	/* receive from below */
#define	PRU_PROTOSEND		22	/* send to below */
/* end for protocol's internal use */
#define PRU_SEND_EOF		23	/* send and close */
#define	PRU_PRED		24
#define PRU_NREQ		25

#ifdef PRUREQUESTS
char *prurequests[] = {
	"ATTACH",	"DETACH",	"BIND",		"LISTEN",
	"CONNECT",	"ACCEPT",	"DISCONNECT",	"SHUTDOWN",
	"RCVD",		"SEND",		"ABORT",	"CONTROL",
	"SENSE",	"RCVOOB",	"SENDOOB",	"SOCKADDR",
	"PEERADDR",	"CONNECT2",	"SOPOLL",
	"FASTTIMO",	"SLOWTIMO",	"PROTORCV",	"PROTOSEND",
	"SEND_EOF",	"PREDICATE"
};
#endif

#ifdef	_KERNEL			/* users shouldn't see this decl */

struct ifnet;
struct stat;
struct ucred;
struct uio;

struct pru_attach_info {
	struct rlimit *sb_rlimit;
	struct ucred *p_ucred;
	struct vnode *fd_rdir;
};

/*
 * If the ordering here looks odd, that's because it's alphabetical.
 * Having this structure separated out from the main protoswitch is allegedly
 * a big (12 cycles per call) lose on high-end CPUs.  We will eventually
 * migrate this stuff back into the main structure.
 */
struct pr_usrreqs {
	int	(*pru_abort) (struct socket *so);
	int	(*pru_accept) (struct socket *so, struct sockaddr **nam);
	int	(*pru_attach) (struct socket *so, int proto,
			       struct pru_attach_info *ai);
	int	(*pru_bind) (struct socket *so, struct sockaddr *nam,
				 struct thread *td);
	int	(*pru_connect) (struct socket *so, struct sockaddr *nam,
				    struct thread *td);
	int	(*pru_connect2) (struct socket *so1, struct socket *so2);
	int	(*pru_control) (struct socket *so, u_long cmd, caddr_t data,
				    struct ifnet *ifp, struct thread *td);
	int	(*pru_detach) (struct socket *so);
	int	(*pru_disconnect) (struct socket *so);
	int	(*pru_listen) (struct socket *so, struct thread *td);
	int	(*pru_peeraddr) (struct socket *so, 
				     struct sockaddr **nam);
	int	(*pru_rcvd) (struct socket *so, int flags);
	int	(*pru_rcvoob) (struct socket *so, struct mbuf *m,
				   int flags);
	int	(*pru_send) (struct socket *so, int flags, struct mbuf *m, 
				 struct sockaddr *addr, struct mbuf *control,
				 struct thread *td);
#define	PRUS_OOB	0x1
#define	PRUS_EOF	0x2
#define	PRUS_MORETOCOME	0x4
	int	(*pru_sense) (struct socket *so, struct stat *sb);
	int	(*pru_shutdown) (struct socket *so);
	int	(*pru_sockaddr) (struct socket *so, 
				     struct sockaddr **nam);
	 
	/*
	 * These three added later, so they are out of order.  They are used
	 * for shortcutting (fast path input/output) in some protocols.
	 * XXX - that's a lie, they are not implemented yet
	 * Rather than calling sosend() etc. directly, calls are made
	 * through these entry points.  For protocols which still use
	 * the generic code, these just point to those routines.
	 */
	int	(*pru_sosend) (struct socket *so, struct sockaddr *addr,
				   struct uio *uio, struct mbuf *top,
				   struct mbuf *control, int flags,
				   struct thread *td);
	int	(*pru_soreceive) (struct socket *so, 
				      struct sockaddr **paddr,
				      struct uio *uio, struct mbuf **mp0,
				      struct mbuf **controlp, int *flagsp);
	int	(*pru_sopoll) (struct socket *so, int events,
				     struct ucred *cred, struct thread *td);
};

typedef int (*pru_abort_fn_t) (struct socket *so);
typedef int (*pru_accept_fn_t) (struct socket *so, struct sockaddr **nam);
typedef int (*pru_attach_fn_t) (struct socket *so, int proto,
					struct pru_attach_info *ai);
typedef int (*pru_bind_fn_t) (struct socket *so, struct sockaddr *nam,
					struct thread *td);
typedef int (*pru_connect_fn_t) (struct socket *so, struct sockaddr *nam,
					struct thread *td);
typedef int (*pru_connect2_fn_t) (struct socket *so1, struct socket *so2);
typedef int (*pru_control_fn_t) (struct socket *so, u_long cmd, caddr_t data,
					struct ifnet *ifp,
					struct thread *td);
typedef int (*pru_detach_fn_t) (struct socket *so);
typedef int (*pru_disconnect_fn_t) (struct socket *so);
typedef int (*pru_listen_fn_t) (struct socket *so, struct thread *td);
typedef int (*pru_peeraddr_fn_t) (struct socket *so, struct sockaddr **nam);
typedef int (*pru_rcvd_fn_t) (struct socket *so, int flags);
typedef int (*pru_rcvoob_fn_t) (struct socket *so, struct mbuf *m, int flags);
typedef int (*pru_send_fn_t) (struct socket *so, int flags, struct mbuf *m, 
					struct sockaddr *addr,
					struct mbuf *control,
					struct thread *td);
typedef int (*pru_sense_fn_t) (struct socket *so, struct stat *sb);
typedef int (*pru_shutdown_fn_t) (struct socket *so);
typedef int (*pru_sockaddr_fn_t) (struct socket *so, struct sockaddr **nam);
typedef int (*pru_sosend_fn_t) (struct socket *so, struct sockaddr *addr,
					struct uio *uio, struct mbuf *top,
					struct mbuf *control, int flags,
					struct thread *td);
typedef int (*pru_soreceive_fn_t) (struct socket *so, struct sockaddr **paddr,
					struct uio *uio, struct mbuf **mp0,
					struct mbuf **controlp,
					int *flagsp);
typedef int (*pru_sopoll_fn_t) (struct socket *so, int events,
					struct ucred *cred,
					struct thread *td);

int	pru_accept_notsupp (struct socket *so, struct sockaddr **nam);
int	pru_connect_notsupp (struct socket *so, struct sockaddr *nam,
				struct thread *td);
int	pru_connect2_notsupp (struct socket *so1, struct socket *so2);
int	pru_control_notsupp (struct socket *so, u_long cmd, caddr_t data,
				struct ifnet *ifp, struct thread *td);
int	pru_listen_notsupp (struct socket *so, struct thread *td);
int	pru_rcvd_notsupp (struct socket *so, int flags);
int	pru_rcvoob_notsupp (struct socket *so, struct mbuf *m, int flags);
int	pru_sense_null (struct socket *so, struct stat *sb);

struct lwkt_port *cpu0_soport(struct socket *, struct sockaddr *, int);
struct lwkt_port *sync_soport(struct socket *, struct sockaddr *, int);

#endif /* _KERNEL */

/*
 * The arguments to the ctlinput routine are
 *	(*protosw[].pr_ctlinput)(cmd, sa, arg);
 * where cmd is one of the commands below, sa is a pointer to a sockaddr,
 * and arg is a `void *' argument used within a protocol family.
 */
#define	PRC_IFDOWN		0	/* interface transition */
#define	PRC_ROUTEDEAD		1	/* select new route if possible ??? */
#define	PRC_IFUP		2	/* interface has come back up */
#define	PRC_QUENCH2		3	/* DEC congestion bit says slow down */
#define	PRC_QUENCH		4	/* some one said to slow down */
#define	PRC_MSGSIZE		5	/* message size forced drop */
#define	PRC_HOSTDEAD		6	/* host appears to be down */
#define	PRC_HOSTUNREACH		7	/* deprecated (use PRC_UNREACH_HOST) */
#define	PRC_UNREACH_NET		8	/* no route to network */
#define	PRC_UNREACH_HOST	9	/* no route to host */
#define	PRC_UNREACH_PROTOCOL	10	/* dst says bad protocol */
#define	PRC_UNREACH_PORT	11	/* bad port # */
/* was	PRC_UNREACH_NEEDFRAG	12	   (use PRC_MSGSIZE) */
#define	PRC_UNREACH_SRCFAIL	13	/* source route failed */
#define	PRC_REDIRECT_NET	14	/* net routing redirect */
#define	PRC_REDIRECT_HOST	15	/* host routing redirect */
#define	PRC_REDIRECT_TOSNET	16	/* redirect for type of service & net */
#define	PRC_REDIRECT_TOSHOST	17	/* redirect for tos & host */
#define	PRC_TIMXCEED_INTRANS	18	/* packet lifetime expired in transit */
#define	PRC_TIMXCEED_REASS	19	/* lifetime expired on reass q */
#define	PRC_PARAMPROB		20	/* header incorrect */
#define	PRC_UNREACH_ADMIN_PROHIB	21	/* packet administrativly prohibited */

#define	PRC_NCMDS		22

#define	PRC_IS_REDIRECT(cmd)	\
	((cmd) >= PRC_REDIRECT_NET && (cmd) <= PRC_REDIRECT_TOSHOST)

#ifdef PRCREQUESTS
char	*prcrequests[] = {
	"IFDOWN", "ROUTEDEAD", "IFUP", "DEC-BIT-QUENCH2",
	"QUENCH", "MSGSIZE", "HOSTDEAD", "#7",
	"NET-UNREACH", "HOST-UNREACH", "PROTO-UNREACH", "PORT-UNREACH",
	"#12", "SRCFAIL-UNREACH", "NET-REDIRECT", "HOST-REDIRECT",
	"TOSNET-REDIRECT", "TOSHOST-REDIRECT", "TX-INTRANS", "TX-REASS",
	"PARAMPROB", "ADMIN-UNREACH"
};
#endif

/*
 * The arguments to ctloutput are:
 *	(*protosw[].pr_ctloutput)(req, so, level, optname, optval, p);
 * req is one of the actions listed below, so is a (struct socket *),
 * level is an indication of which protocol layer the option is intended.
 * optname is a protocol dependent socket option request,
 * optval is a pointer to a mbuf-chain pointer, for value-return results.
 * The protocol is responsible for disposal of the mbuf chain *optval
 * if supplied,
 * the caller is responsible for any space held by *optval, when returned.
 * A non-zero return from usrreq gives an
 * UNIX error number which should be passed to higher level software.
 */
#define	PRCO_GETOPT	0
#define	PRCO_SETOPT	1

#define	PRCO_NCMDS	2

#ifdef PRCOREQUESTS
char	*prcorequests[] = {
	"GETOPT", "SETOPT",
};
#endif

#ifdef _KERNEL
void	pfctlinput (int, struct sockaddr *);
void	pfctlinput2 (int, struct sockaddr *, void *);
struct protosw *pffindproto (int family, int protocol, int type);
struct protosw *pffindtype (int family, int type);
#endif	/* _KERNEL */

#endif	/* _SYS_PROTOSW_H_ */
