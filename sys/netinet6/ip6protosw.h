/*	$FreeBSD: src/sys/netinet6/ip6protosw.h,v 1.2.2.4 2002/04/28 05:40:27 suz Exp $	*/
/*	$DragonFly: src/sys/netinet6/ip6protosw.h,v 1.6 2004/06/03 16:21:22 joerg Exp $	*/
/*	$KAME: ip6protosw.h,v 1.25 2001/09/26 06:13:03 keiichi Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *
 */

/*	BSDI protosw.h,v 2.3 1996/10/11 16:02:40 pjd Exp	*/

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
 */

#ifndef _NETINET6_IP6PROTOSW_H_
#define _NETINET6_IP6PROTOSW_H_

/*
 * Protocol switch table for IPv6.
 * All other definitions should refer to sys/protosw.h
 */

struct mbuf;
struct sockaddr;
struct socket;
struct domain;
struct proc;
struct ip6_hdr;
struct icmp6_hdr;
struct in6_addr;
struct pr_usrreqs;

/*
 * argument type for the last arg of pr_ctlinput().
 * should be consulted only with AF_INET6 family.
 *
 * IPv6 ICMP IPv6 [exthdrs] finalhdr paylaod
 * ^    ^    ^              ^
 * |    |    ip6c_ip6       ip6c_off
 * |    ip6c_icmp6
 * ip6c_m
 *
 * ip6c_finaldst usually points to ip6c_ip6->ip6_dst.  if the original
 * (internal) packet carries a routing header, it may point the final
 * dstination address in the routing header.
 *
 * ip6c_src: ip6c_ip6->ip6_src + scope info + flowlabel in ip6c_ip6
 *	(beware of flowlabel, if you try to compare it against others)
 * ip6c_dst: ip6c_finaldst + scope info
 */
struct ip6ctlparam {
	struct mbuf *ip6c_m;		/* start of mbuf chain */
	struct icmp6_hdr *ip6c_icmp6;	/* icmp6 header of target packet */
	struct ip6_hdr *ip6c_ip6;	/* ip6 header of target packet */
	int ip6c_off;			/* offset of the target proto header */
	struct sockaddr_in6 *ip6c_src;	/* srcaddr w/ additional info */
	struct sockaddr_in6 *ip6c_dst;	/* (final) dstaddr w/ additional info */
	struct in6_addr *ip6c_finaldst;	/* final destination address */
	void *ip6c_cmdarg;		/* control command dependent data */
	u_int8_t ip6c_nxt;		/* final next header field */
};

struct lwkt_port;

struct ip6protosw {
	short	pr_type;		/* socket type used for */
	const struct	domain *pr_domain; /* domain protocol a member of */
	short	pr_protocol;		/* protocol number */
	short	pr_flags;		/* see below */

/* protocol-protocol hooks */
	int	(*pr_input)		/* input to protocol (from below) */
			(struct mbuf **, int *, int);
	int	(*pr_output)		/* output to protocol (from above) */
			(struct mbuf *, struct socket *, ...);
	void	(*pr_ctlinput)		/* control input (from below) */
			(int, struct sockaddr *, void *);
	int	(*pr_ctloutput)		/* control output (from above) */
			(struct socket *, struct sockopt *);

/* user-protocol hook */
	struct lwkt_port *(*pr_soport)
			(struct socket *, struct sockaddr *, int);

/* utility hooks */
	void	(*pr_init)		/* initialization hook */
			(void);

	void	(*pr_fasttimo)		/* fast timeout (200ms) */
			(void);
	void	(*pr_slowtimo)		/* slow timeout (500ms) */
			(void);
	void	(*pr_drain)		/* flush any excess space possible */
			(void);
	const struct	pr_usrreqs *pr_usrreqs;	/* supersedes pr_usrreq() */
};

#endif /* !_NETINET6_IP6PROTOSW_H_ */
