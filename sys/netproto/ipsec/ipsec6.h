/*	$FreeBSD: src/sys/netipsec/ipsec6.h,v 1.1.4.1 2003/01/24 05:11:35 sam Exp $	*/
/*	$DragonFly: src/sys/netproto/ipsec/ipsec6.h,v 1.4 2003/08/23 10:06:23 rob Exp $	*/
/*	$KAME: ipsec.h,v 1.44 2001/03/23 08:08:47 itojun Exp $	*/

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
 */

/*
 * IPsec controller part.
 */

#ifndef _NETIPSEC_IPSEC6_H_
#define _NETIPSEC_IPSEC6_H_

#include <net/pfkeyv2.h>
#include "keydb.h"

#ifdef _KERNEL
extern int ip6_esp_trans_deflev;
extern int ip6_esp_net_deflev;
extern int ip6_ah_trans_deflev;
extern int ip6_ah_net_deflev;
extern int ip6_ipsec_ecn;
extern int ip6_esp_randpad;

struct inpcb;

/* KAME compatibility shims */
#define	ipsec6_getpolicybyaddr	ipsec_getpolicybyaddr
#define	ipsec6_getpolicybysock	ipsec_getpolicybysock
#define	ipsec6stat		newipsecstat
#define	out_inval		ips_out_inval
#define	in_polvio		ips_in_polvio
#define	out_polvio		ips_out_polvio
#define	key_freesp(_x)		KEY_FREESP(&_x)

extern int ipsec6_delete_pcbpolicy (struct inpcb *);
extern int ipsec6_set_policy (struct inpcb *inp, int optname,
	caddr_t request, size_t len, int priv);
extern int ipsec6_get_policy
	(struct inpcb *inp, caddr_t request, size_t len, struct mbuf **mp);
extern int ipsec6_in_reject (struct mbuf *, struct inpcb *);

struct tcp6cb;

extern size_t ipsec6_hdrsiz (struct mbuf *, u_int, struct inpcb *);

struct ip6_hdr;
extern const char *ipsec6_logpacketstr (struct ip6_hdr *, u_int32_t);

struct m_tag;
extern int ipsec6_common_input(struct mbuf **mp, int *offp, int proto);
extern int ipsec6_common_input_cb(struct mbuf *m, struct secasvar *sav,
			int skip, int protoff, struct m_tag *mt);
extern void esp6_ctlinput(int, struct sockaddr *, void *);

struct ipsec_output_state;
extern int ipsec6_output_trans (struct ipsec_output_state *, u_char *,
	struct mbuf *, struct secpolicy *, int, int *);
extern int ipsec6_output_tunnel (struct ipsec_output_state *,
	struct secpolicy *, int);
#endif /*_KERNEL*/

#endif /*_NETIPSEC_IPSEC6_H_*/
