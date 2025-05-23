/*	$FreeBSD: src/sys/netinet6/ah_output.c,v 1.1.2.5 2003/05/06 06:46:58 suz Exp $	*/
/*	$DragonFly: src/sys/netinet6/ah_output.c,v 1.6 2004/06/02 14:43:01 eirikn Exp $	*/
/*	$KAME: ah_output.c,v 1.31 2001/07/26 06:53:15 jinmei Exp $	*/

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
 * RFC1826/2402 authentication header.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#endif

#include <netinet6/ipsec.h>
#ifdef INET6
#include <netinet6/ipsec6.h>
#endif
#include <netinet6/ah.h>
#ifdef INET6
#include <netinet6/ah6.h>
#endif
#include <netproto/key/key.h>
#include <netproto/key/keydb.h>

#include <net/net_osdep.h>

#ifdef INET
static struct in_addr *ah4_finaldst (struct mbuf *);
#endif

/*
 * compute AH header size.
 * transport mode only.  for tunnel mode, we should implement
 * virtual interface, and control MTU/MSS by the interface MTU.
 */
size_t
ah_hdrsiz(struct ipsecrequest *isr)
{
	const struct ah_algorithm *algo;
	size_t hdrsiz;

	/* sanity check */
	if (isr == NULL)
		panic("ah_hdrsiz: NULL was passed.");

	if (isr->saidx.proto != IPPROTO_AH)
		panic("unsupported mode passed to ah_hdrsiz");

	if (isr->sav == NULL)
		goto estimate;
	if (isr->sav->state != SADB_SASTATE_MATURE
	 && isr->sav->state != SADB_SASTATE_DYING)
		goto estimate;

	/* we need transport mode AH. */
	algo = ah_algorithm_lookup(isr->sav->alg_auth);
	if (!algo)
		goto estimate;

	/*
	 * XXX
	 * right now we don't calcurate the padding size.  simply
	 * treat the padding size as constant, for simplicity.
	 *
	 * XXX variable size padding support
	 */
	hdrsiz = (((*algo->sumsiz)(isr->sav) + 3) & ~(4 - 1));
	if (isr->sav->flags & SADB_X_EXT_OLD)
		hdrsiz += sizeof(struct ah);
	else
		hdrsiz += sizeof(struct newah);

	return hdrsiz;

    estimate:
	/* ASSUMING:
	 *	sizeof(struct newah) > sizeof(struct ah).
	 *	16 = (16 + 3) & ~(4 - 1).
	 */
	return sizeof(struct newah) + 16;
}

#ifdef INET
/*
 * Modify the packet so that it includes the authentication data.
 * The mbuf passed must start with IPv4 header.
 *
 * assumes that the first mbuf contains IPv4 header + option only.
 * the function does not modify m.
 */
int
ah4_output(struct mbuf *m, struct ipsecrequest *isr)
{
	struct secasvar *sav = isr->sav;
	const struct ah_algorithm *algo;
	u_int32_t spi;
	u_char *ahdrpos;
	u_char *ahsumpos = NULL;
	size_t hlen = 0;	/* IP header+option in bytes */
	size_t plen = 0;	/* AH payload size in bytes */
	size_t ahlen = 0;	/* plen + sizeof(ah) */
	struct ip *ip;
	struct in_addr dst;
	struct in_addr *finaldst;
	int error;

	/* sanity checks */
	if ((sav->flags & SADB_X_EXT_OLD) == 0 && !sav->replay) {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		ipseclog((LOG_DEBUG, "ah4_output: internal error: "
			"sav->replay is null: %x->%x, SPI=%u\n",
			(u_int32_t)ntohl(ip->ip_src.s_addr),
			(u_int32_t)ntohl(ip->ip_dst.s_addr),
			(u_int32_t)ntohl(sav->spi)));
		ipsecstat.out_inval++;
		m_freem(m);
		return EINVAL;
	}

	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo) {
		ipseclog((LOG_ERR, "ah4_output: unsupported algorithm: "
		    "SPI=%u\n", (u_int32_t)ntohl(sav->spi)));
		ipsecstat.out_inval++;
		m_freem(m);
		return EINVAL;
	}
	spi = sav->spi;

	/*
	 * determine the size to grow.
	 */
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1826 */
		plen = ((*algo->sumsiz)(sav) + 3) & ~(4 - 1); /* XXX pad to 8byte? */
		ahlen = plen + sizeof(struct ah);
	} else {
		/* RFC 2402 */
		plen = ((*algo->sumsiz)(sav) + 3) & ~(4 - 1); /* XXX pad to 8byte? */
		ahlen = plen + sizeof(struct newah);
	}

	/*
	 * grow the mbuf to accomodate AH.
	 */
	ip = mtod(m, struct ip *);
#ifdef _IP_VHL
	hlen = IP_VHL_HL(ip->ip_vhl) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif

	if (m->m_len != hlen)
		panic("ah4_output: assumption failed (first mbuf length)");
	if (M_LEADINGSPACE(m->m_next) < ahlen) {
		struct mbuf *n;
		MGET(n, MB_DONTWAIT, MT_DATA);
		if (!n) {
			ipseclog((LOG_DEBUG, "ENOBUFS in ah4_output %d\n",
			    __LINE__));
			m_freem(m);
			return ENOBUFS;
		}
		n->m_len = ahlen;
		n->m_next = m->m_next;
		m->m_next = n;
		m->m_pkthdr.len += ahlen;
		ahdrpos = mtod(n, u_char *);
	} else {
		m->m_next->m_len += ahlen;
		m->m_next->m_data -= ahlen;
		m->m_pkthdr.len += ahlen;
		ahdrpos = mtod(m->m_next, u_char *);
	}

	ip = mtod(m, struct ip *);	/* just to be sure */

	/*
	 * initialize AH.
	 */
	if (sav->flags & SADB_X_EXT_OLD) {
		struct ah *ahdr;

		ahdr = (struct ah *)ahdrpos;
		ahsumpos = (u_char *)(ahdr + 1);
		ahdr->ah_len = plen >> 2;
		ahdr->ah_nxt = ip->ip_p;
		ahdr->ah_reserve = htons(0);
		ahdr->ah_spi = spi;
		bzero(ahdr + 1, plen);
	} else {
		struct newah *ahdr;

		ahdr = (struct newah *)ahdrpos;
		ahsumpos = (u_char *)(ahdr + 1);
		ahdr->ah_len = (plen >> 2) + 1;	/* plus one for seq# */
		ahdr->ah_nxt = ip->ip_p;
		ahdr->ah_reserve = htons(0);
		ahdr->ah_spi = spi;
		if (sav->replay->count == ~0) {
			if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0) {
				/* XXX Is it noisy ? */
				ipseclog((LOG_WARNING,
				    "replay counter overflowed. %s\n",
				    ipsec_logsastr(sav)));
				ipsecstat.out_inval++;
				m_freem(m);
				return EINVAL;
			}
		}
		sav->replay->count++;
		/*
		 * XXX sequence number must not be cycled, if the SA is
		 * installed by IKE daemon.
		 */
		ahdr->ah_seq = htonl(sav->replay->count);
		bzero(ahdr + 1, plen);
	}

	/*
	 * modify IPv4 header.
	 */
	ip->ip_p = IPPROTO_AH;
	if (ahlen < (IP_MAXPACKET - ntohs(ip->ip_len)))
		ip->ip_len = htons(ntohs(ip->ip_len) + ahlen);
	else {
		ipseclog((LOG_ERR, "IPv4 AH output: size exceeds limit\n"));
		ipsecstat.out_inval++;
		m_freem(m);
		return EMSGSIZE;
	}

	/*
	 * If there is source routing option, update destination field in
	 * the IPv4 header to the final destination.
	 * Note that we do not need to update source routing option itself
	 * (as done in IPv4 AH processing -- see ip6_output()), since
	 * source routing option is not part of the ICV computation.
	 */
	finaldst = ah4_finaldst(m);
	if (finaldst) {
		dst.s_addr = ip->ip_dst.s_addr;
		ip->ip_dst.s_addr = finaldst->s_addr;
	}

	/*
	 * calcurate the checksum, based on security association
	 * and the algorithm specified.
	 */
	error = ah4_calccksum(m, (caddr_t)ahsumpos, plen, algo, sav);
	if (error) {
		ipseclog((LOG_ERR,
		    "error after ah4_calccksum, called from ah4_output"));
		m_freem(m);
		m = NULL;
		ipsecstat.out_inval++;
		return error;
	}

	if (finaldst) {
		ip = mtod(m, struct ip *);	/* just to make sure */
		ip->ip_dst.s_addr = dst.s_addr;
	}
	ipsecstat.out_success++;
	ipsecstat.out_ahhist[sav->alg_auth]++;
	key_sa_recordxfer(sav, m);

	return 0;
}
#endif

/* Calculate AH length */
int
ah_hdrlen(struct secasvar *sav)
{
	const struct ah_algorithm *algo;
	int plen, ahlen;
	
	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo)
		return 0;
	if (sav->flags & SADB_X_EXT_OLD) {
		/* RFC 1826 */
		plen = ((*algo->sumsiz)(sav) + 3) & ~(4 - 1);	/* XXX pad to 8byte? */
		ahlen = plen + sizeof(struct ah);
	} else {
		/* RFC 2402 */
		plen = ((*algo->sumsiz)(sav) + 3) & ~(4 - 1);	/* XXX pad to 8byte? */
		ahlen = plen + sizeof(struct newah);
	}

	return(ahlen);
}

#ifdef INET6
/*
 * Fill in the Authentication Header and calculate checksum.
 */
int
ah6_output(struct mbuf *m, u_char *nexthdrp, struct mbuf *md,
	   struct ipsecrequest *isr)
{
	struct mbuf *mprev;
	struct mbuf *mah;
	struct secasvar *sav = isr->sav;
	const struct ah_algorithm *algo;
	u_int32_t spi;
	u_char *ahsumpos = NULL;
	size_t plen;	/* AH payload size in bytes */
	int error = 0;
	int ahlen;
	struct ip6_hdr *ip6;

	if (m->m_len < sizeof(struct ip6_hdr)) {
		ipseclog((LOG_DEBUG, "ah6_output: first mbuf too short\n"));
		m_freem(m);
		return EINVAL;
	}

	ahlen = ah_hdrlen(sav);
	if (ahlen == 0)
		return 0;

	for (mprev = m; mprev && mprev->m_next != md; mprev = mprev->m_next)
		;
	if (!mprev || mprev->m_next != md) {
		ipseclog((LOG_DEBUG, "ah6_output: md is not in chain\n"));
		m_freem(m);
		return EINVAL;
	}

	MGET(mah, MB_DONTWAIT, MT_DATA);
	if (!mah) {
		m_freem(m);
		return ENOBUFS;
	}
	if (ahlen > MLEN) {
		MCLGET(mah, MB_DONTWAIT);
		if ((mah->m_flags & M_EXT) == 0) {
			m_free(mah);
			m_freem(m);
			return ENOBUFS;
		}
	}
	mah->m_len = ahlen;
	mah->m_next = md;
	mprev->m_next = mah;
	m->m_pkthdr.len += ahlen;

	/* fix plen */
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) > IPV6_MAXPACKET) {
		ipseclog((LOG_ERR,
		    "ip6_output: AH with IPv6 jumbogram is not supported\n"));
		m_freem(m);
		return EINVAL;
	}
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));

	if ((sav->flags & SADB_X_EXT_OLD) == 0 && !sav->replay) {
		ipseclog((LOG_DEBUG, "ah6_output: internal error: "
			"sav->replay is null: SPI=%u\n",
			(u_int32_t)ntohl(sav->spi)));
		ipsec6stat.out_inval++;
		m_freem(m);
		return EINVAL;
	}

	algo = ah_algorithm_lookup(sav->alg_auth);
	if (!algo) {
		ipseclog((LOG_ERR, "ah6_output: unsupported algorithm: "
		    "SPI=%u\n", (u_int32_t)ntohl(sav->spi)));
		ipsec6stat.out_inval++;
		m_freem(m);
		return EINVAL;
	}
	spi = sav->spi;

	/*
	 * initialize AH.
	 */
	if (sav->flags & SADB_X_EXT_OLD) {
		struct ah *ahdr = mtod(mah, struct ah *);

		plen = mah->m_len - sizeof(struct ah);
		ahsumpos = (u_char *)(ahdr + 1);
		ahdr->ah_nxt = *nexthdrp;
		*nexthdrp = IPPROTO_AH;
		ahdr->ah_len = plen >> 2;
		ahdr->ah_reserve = htons(0);
		ahdr->ah_spi = spi;
		bzero(ahdr + 1, plen);
	} else {
		struct newah *ahdr = mtod(mah, struct newah *);

		plen = mah->m_len - sizeof(struct newah);
		ahsumpos = (u_char *)(ahdr + 1);
		ahdr->ah_nxt = *nexthdrp;
		*nexthdrp = IPPROTO_AH;
		ahdr->ah_len = (plen >> 2) + 1;	/* plus one for seq# */
		ahdr->ah_reserve = htons(0);
		ahdr->ah_spi = spi;
		if (sav->replay->count == ~0) {
			if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0) {
				/* XXX Is it noisy ? */
				ipseclog((LOG_WARNING,
				    "replay counter overflowed. %s\n",
				    ipsec_logsastr(sav)));
				ipsec6stat.out_inval++;
				m_freem(m);
				return EINVAL;
			}
		}
		sav->replay->count++;
		/*
		 * XXX sequence number must not be cycled, if the SA is
		 * installed by IKE daemon.
		 */
		ahdr->ah_seq = htonl(sav->replay->count);
		bzero(ahdr + 1, plen);
	}

	/*
	 * calcurate the checksum, based on security association
	 * and the algorithm specified.
	 */
	error = ah6_calccksum(m, (caddr_t)ahsumpos, plen, algo, sav);
	if (error) {
		ipsec6stat.out_inval++;
		m_freem(m);
	} else {
		ipsec6stat.out_success++;
		key_sa_recordxfer(sav, m);
	}
	ipsec6stat.out_ahhist[sav->alg_auth]++;

	return(error);
}
#endif

#ifdef INET
/*
 * Find the final destination if there is loose/strict source routing option.
 * Returns NULL if there's no source routing options.
 * Returns NULL on errors too.
 * Note that this function will return a pointer INTO the given parameter,
 * struct mbuf *m.
 * The mbuf must be pulled up toward, at least, ip option part.
 */
static struct in_addr *
ah4_finaldst(struct mbuf *m)
{
	struct ip *ip;
	int optlen;
	u_char *q;
	int i;
	int hlen;

	if (!m)
		panic("ah4_finaldst: m == NULL");
	ip = mtod(m, struct ip *);
	hlen = (ip->ip_hl << 2);

	if (m->m_len < hlen) {
		ipseclog((LOG_DEBUG,
		    "ah4_finaldst: parameter mbuf wrong (not pulled up)\n"));
		return NULL;
	}

	if (hlen == sizeof(struct ip))
		return NULL;

	optlen = hlen - sizeof(struct ip);
	if (optlen < 0) {
		ipseclog((LOG_DEBUG, "ah4_finaldst: wrong optlen %d\n",
		    optlen));
		return NULL;
	}

	q = (u_char *)(ip + 1);
	i = 0;
	while (i < optlen) {
		if (i + IPOPT_OPTVAL >= optlen)
			return NULL;
		if (q[i + IPOPT_OPTVAL] == IPOPT_EOL ||
		    q[i + IPOPT_OPTVAL] == IPOPT_NOP ||
		    i + IPOPT_OLEN < optlen)
			;
		else
			return NULL;

		switch (q[i + IPOPT_OPTVAL]) {
		case IPOPT_EOL:
			i = optlen;	/* bye */
			break;
		case IPOPT_NOP:
			i++;
			break;
		case IPOPT_LSRR:
		case IPOPT_SSRR:
			if (q[i + IPOPT_OLEN] < 2 + sizeof(struct in_addr) ||
			    optlen - i < q[i + IPOPT_OLEN]) {
				ipseclog((LOG_ERR,
				    "ip_finaldst: invalid IP option "
				    "(code=%02x len=%02x)\n",
				    q[i + IPOPT_OPTVAL], q[i + IPOPT_OLEN]));
				return NULL;
			}
			i += q[i + IPOPT_OLEN] - sizeof(struct in_addr);
			return (struct in_addr *)(q + i);
		default:
			if (q[i + IPOPT_OLEN] < 2 ||
			    optlen - i < q[i + IPOPT_OLEN]) {
				ipseclog((LOG_ERR,
				    "ip_finaldst: invalid IP option "
				    "(code=%02x len=%02x)\n",
				    q[i + IPOPT_OPTVAL], q[i + IPOPT_OLEN]));
				return NULL;
			}
			i += q[i + IPOPT_OLEN];
			break;
		}
	}
	return NULL;
}
#endif
