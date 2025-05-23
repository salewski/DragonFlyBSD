/*	$FreeBSD: src/sys/netipsec/xform_ah.c,v 1.1.4.2 2003/02/26 00:14:05 sam Exp $	*/
/*	$DragonFly: src/sys/netproto/ipsec/xform_ah.c,v 1.5 2004/10/15 22:59:10 hsu Exp $	*/
/*	$OpenBSD: ip_ah.c,v 1.63 2001/06/26 06:18:58 angelos Exp $ */
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis and Niklas Hallqvist.
 *
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 1999 Niklas Hallqvist.
 * Copyright (c) 2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_ecn.h>
#include <netinet/ip6.h>

#include <net/route.h>
#include <netproto/ipsec/ipsec.h>
#include <netproto/ipsec/ah.h>
#include <netproto/ipsec/ah_var.h>
#include <netproto/ipsec/xform.h>

#ifdef INET6
#include <netinet6/ip6_var.h>
#include <netproto/ipsec/ipsec6.h>
#include <netinet6/ip6_ecn.h>
#endif

#include <netproto/ipsec/key.h>
#include <netproto/ipsec/key_debug.h>

#include <opencrypto/cryptodev.h>

/*
 * Return header size in bytes.  The old protocol did not support
 * the replay counter; the new protocol always includes the counter.
 */
#define HDRSIZE(sav) \
	(((sav)->flags & SADB_X_EXT_OLD) ? \
		sizeof (struct ah) : sizeof (struct ah) + sizeof (u_int32_t))
/* 
 * Return authenticator size in bytes.  The old protocol is known
 * to use a fixed 16-byte authenticator.  The new algorithm gets
 * this size from the xform but is (currently) always 12.
 */
#define	AUTHSIZE(sav) \
	((sav->flags & SADB_X_EXT_OLD) ? 16 : (sav)->tdb_authalgxform->authsize)

int	ah_enable = 1;			/* control flow of packets with AH */
int	ah_cleartos = 1;		/* clear ip_tos when doing AH calc */
struct	ahstat ahstat;

SYSCTL_DECL(_net_inet_ah);
SYSCTL_INT(_net_inet_ah, OID_AUTO,
	ah_enable,	CTLFLAG_RW,	&ah_enable,	0, "");
SYSCTL_INT(_net_inet_ah, OID_AUTO,
	ah_cleartos,	CTLFLAG_RW,	&ah_cleartos,	0, "");
SYSCTL_STRUCT(_net_inet_ah, IPSECCTL_STATS,
	stats,		CTLFLAG_RD,	&ahstat,	ahstat, "");

static unsigned char ipseczeroes[256];	/* larger than an ip6 extension hdr */

static int ah_input_cb(struct cryptop*);
static int ah_output_cb(struct cryptop*);

/*
 * NB: this is public for use by the PF_KEY support.
 */
struct auth_hash *
ah_algorithm_lookup(int alg)
{
	if (alg >= AH_ALG_MAX)
		return NULL;
	switch (alg) {
	case SADB_X_AALG_NULL:
		return &auth_hash_null;
	case SADB_AALG_MD5HMAC:
		return &auth_hash_hmac_md5_96;
	case SADB_AALG_SHA1HMAC:
		return &auth_hash_hmac_sha1_96;
	case SADB_X_AALG_RIPEMD160HMAC:
		return &auth_hash_hmac_ripemd_160_96;
	case SADB_X_AALG_MD5:
		return &auth_hash_key_md5;
	case SADB_X_AALG_SHA:
		return &auth_hash_key_sha1;
	case SADB_X_AALG_SHA2_256:
		return &auth_hash_hmac_sha2_256;
	case SADB_X_AALG_SHA2_384:
		return &auth_hash_hmac_sha2_384;
	case SADB_X_AALG_SHA2_512:
		return &auth_hash_hmac_sha2_512;
	}
	return NULL;
}

size_t
ah_hdrsiz(struct secasvar *sav)
{
	size_t size;

	if (sav != NULL) {
		int authsize;
		KASSERT(sav->tdb_authalgxform != NULL,
			("ah_hdrsiz: null xform"));
		/*XXX not right for null algorithm--does it matter??*/
		authsize = AUTHSIZE(sav);
		size = roundup(authsize, sizeof (u_int32_t)) + HDRSIZE(sav);
	} else {
		/* default guess */
		size = sizeof (struct ah) + sizeof (u_int32_t) + 16;
	}
	return size;
}

/*
 * NB: public for use by esp_init.
 */
int
ah_init0(struct secasvar *sav, struct xformsw *xsp, struct cryptoini *cria)
{
	struct auth_hash *thash;
	int keylen;

	thash = ah_algorithm_lookup(sav->alg_auth);
	if (thash == NULL) {
		DPRINTF(("ah_init: unsupported authentication algorithm %u\n",
			sav->alg_auth));
		return EINVAL;
	}
	/*
	 * Verify the replay state block allocation is consistent with
	 * the protocol type.  We check here so we can make assumptions
	 * later during protocol processing.
	 */
	/* NB: replay state is setup elsewhere (sigh) */
	if (((sav->flags&SADB_X_EXT_OLD) == 0) ^ (sav->replay != NULL)) {
		DPRINTF(("ah_init: replay state block inconsistency, "
			"%s algorithm %s replay state\n",
			(sav->flags & SADB_X_EXT_OLD) ? "old" : "new",
			sav->replay == NULL ? "without" : "with"));
		return EINVAL;
	}
	if (sav->key_auth == NULL) {
		DPRINTF(("ah_init: no authentication key for %s "
			"algorithm\n", thash->name));
		return EINVAL;
	}
	keylen = _KEYLEN(sav->key_auth);
	if (keylen != thash->keysize && thash->keysize != 0) {
		DPRINTF(("ah_init: invalid keylength %d, algorithm "
			 "%s requires keysize %d\n",
			 keylen, thash->name, thash->keysize));
		return EINVAL;
	}

	sav->tdb_xform = xsp;
	sav->tdb_authalgxform = thash;

	/* Initialize crypto session. */
	bzero(cria, sizeof (*cria));
	cria->cri_alg = sav->tdb_authalgxform->type;
	cria->cri_klen = _KEYBITS(sav->key_auth);
	cria->cri_key = _KEYBUF(sav->key_auth);

	return 0;
}

/*
 * ah_init() is called when an SPI is being set up.
 */
static int
ah_init(struct secasvar *sav, struct xformsw *xsp)
{
	struct cryptoini cria;
	int error;

	error = ah_init0(sav, xsp, &cria);
	return error ? error :
		 crypto_newsession(&sav->tdb_cryptoid, &cria, crypto_support);
}

/*
 * Paranoia.
 *
 * NB: public for use by esp_zeroize (XXX).
 */
int
ah_zeroize(struct secasvar *sav)
{
	int err;

	if (sav->key_auth)
		bzero(_KEYBUF(sav->key_auth), _KEYLEN(sav->key_auth));

	err = crypto_freesession(sav->tdb_cryptoid);
	sav->tdb_cryptoid = 0;
	sav->tdb_authalgxform = NULL;
	sav->tdb_xform = NULL;
	return err;
}

/*
 * Massage IPv4/IPv6 headers for AH processing.
 */
static int
ah_massage_headers(struct mbuf **m0, int proto, int skip, int alg, int out)
{
	struct mbuf *m = *m0;
	unsigned char *ptr;
	int off, count;

#ifdef INET
	struct ip *ip;
#endif /* INET */

#ifdef INET6
	struct ip6_ext *ip6e;
	struct ip6_hdr ip6;
	int alloc, len, ad;
#endif /* INET6 */

	switch (proto) {
#ifdef INET
	case AF_INET:
		/*
		 * This is the least painful way of dealing with IPv4 header
		 * and option processing -- just make sure they're in
		 * contiguous memory.
		 */
		*m0 = m = m_pullup(m, skip);
		if (m == NULL) {
			DPRINTF(("ah_massage_headers: m_pullup failed\n"));
			return ENOBUFS;
		}

		/* Fix the IP header */
		ip = mtod(m, struct ip *);
		if (ah_cleartos)
			ip->ip_tos = 0;
		ip->ip_ttl = 0;
		ip->ip_sum = 0;

		/*
		 * On input, fix ip_len which has been byte-swapped
		 * at ip_input().
		 */
		if (!out) {
			ip->ip_len = htons(ip->ip_len + skip);

			if (alg == CRYPTO_MD5_KPDK || alg == CRYPTO_SHA1_KPDK)
				ip->ip_off = htons(ip->ip_off & IP_DF);
			else
				ip->ip_off = 0;
		} else {
			if (alg == CRYPTO_MD5_KPDK || alg == CRYPTO_SHA1_KPDK)
				ip->ip_off = htons(ntohs(ip->ip_off) & IP_DF);
			else
				ip->ip_off = 0;
		}

		ptr = mtod(m, unsigned char *) + sizeof(struct ip);

		/* IPv4 option processing */
		for (off = sizeof(struct ip); off < skip;) {
			if (ptr[off] == IPOPT_EOL || ptr[off] == IPOPT_NOP ||
			    off + 1 < skip)
				;
			else {
				DPRINTF(("ah_massage_headers: illegal IPv4 "
				    "option length for option %d\n",
				    ptr[off]));

				m_freem(m);
				return EINVAL;
			}

			switch (ptr[off]) {
			case IPOPT_EOL:
				off = skip;  /* End the loop. */
				break;

			case IPOPT_NOP:
				off++;
				break;

			case IPOPT_SECURITY:	/* 0x82 */
			case 0x85:	/* Extended security. */
			case 0x86:	/* Commercial security. */
			case 0x94:	/* Router alert */
			case 0x95:	/* RFC1770 */
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("ah_massage_headers: "
					    "illegal IPv4 option length for "
					    "option %d\n", ptr[off]));

					m_freem(m);
					return EINVAL;
				}

				off += ptr[off + 1];
				break;

			case IPOPT_LSRR:
			case IPOPT_SSRR:
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("ah_massage_headers: "
					    "illegal IPv4 option length for "
					    "option %d\n", ptr[off]));

					m_freem(m);
					return EINVAL;
				}

				/*
				 * On output, if we have either of the
				 * source routing options, we should
				 * swap the destination address of the
				 * IP header with the last address
				 * specified in the option, as that is
				 * what the destination's IP header
				 * will look like.
				 */
				if (out)
					bcopy(ptr + off + ptr[off + 1] -
					    sizeof(struct in_addr),
					    &(ip->ip_dst), sizeof(struct in_addr));

				/* Fall through */
			default:
				/* Sanity check for option length. */
				if (ptr[off + 1] < 2) {
					DPRINTF(("ah_massage_headers: "
					    "illegal IPv4 option length for "
					    "option %d\n", ptr[off]));
					m_freem(m);
					return EINVAL;
				}

				/* Zeroize all other options. */
				count = ptr[off + 1];
				bcopy(ipseczeroes, ptr, count);
				off += count;
				break;
			}

			/* Sanity check. */
			if (off > skip)	{
				DPRINTF(("ah_massage_headers(): malformed "
				    "IPv4 options header\n"));

				m_freem(m);
				return EINVAL;
			}
		}

		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:  /* Ugly... */
		/* Copy and "cook" the IPv6 header. */
		m_copydata(m, 0, sizeof(ip6), (caddr_t) &ip6);

		/* We don't do IPv6 Jumbograms. */
		if (ip6.ip6_plen == 0) {
			DPRINTF(("ah_massage_headers: unsupported IPv6 jumbogram\n"));
			m_freem(m);
			return EMSGSIZE;
		}

		ip6.ip6_flow = 0;
		ip6.ip6_hlim = 0;
		ip6.ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6.ip6_vfc |= IPV6_VERSION;

		/* Scoped address handling. */
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6.ip6_src))
			ip6.ip6_src.s6_addr16[1] = 0;
		if (IN6_IS_SCOPE_LINKLOCAL(&ip6.ip6_dst))
			ip6.ip6_dst.s6_addr16[1] = 0;

		/* Done with IPv6 header. */
		m_copyback(m, 0, sizeof(struct ip6_hdr), (caddr_t) &ip6);

		/* Let's deal with the remaining headers (if any). */
		if (skip - sizeof(struct ip6_hdr) > 0) {
			if (m->m_len <= skip) {
				ptr = malloc(skip - sizeof(struct ip6_hdr),
					    M_XDATA, M_INTWAIT | M_NULLOK);
				if (ptr == NULL) {
					DPRINTF(("ah_massage_headers: failed "
					    "to allocate memory for IPv6 "
					    "headers\n"));
					m_freem(m);
					return ENOBUFS;
				}

				/*
				 * Copy all the protocol headers after
				 * the IPv6 header.
				 */
				m_copydata(m, sizeof(struct ip6_hdr),
				    skip - sizeof(struct ip6_hdr), ptr);
				alloc = 1;
			} else {
				/* No need to allocate memory. */
				ptr = mtod(m, unsigned char *) +
				    sizeof(struct ip6_hdr);
				alloc = 0;
			}
		} else
			break;

		off = ip6.ip6_nxt & 0xff; /* Next header type. */

		for (len = 0; len < skip - sizeof(struct ip6_hdr);)
			switch (off) {
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS:
				ip6e = (struct ip6_ext *) (ptr + len);

				/*
				 * Process the mutable/immutable
				 * options -- borrows heavily from the
				 * KAME code.
				 */
				for (count = len + sizeof(struct ip6_ext);
				     count < len + ((ip6e->ip6e_len + 1) << 3);) {
					if (ptr[count] == IP6OPT_PAD1) {
						count++;
						continue; /* Skip padding. */
					}

					/* Sanity check. */
					if (count > len +
					    ((ip6e->ip6e_len + 1) << 3)) {
						m_freem(m);

						/* Free, if we allocated. */
						if (alloc)
							FREE(ptr, M_XDATA);
						return EINVAL;
					}

					ad = ptr[count + 1];

					/* If mutable option, zeroize. */
					if (ptr[count] & IP6OPT_MUTABLE)
						bcopy(ipseczeroes, ptr + count,
						    ptr[count + 1]);

					count += ad;

					/* Sanity check. */
					if (count >
					    skip - sizeof(struct ip6_hdr)) {
						m_freem(m);

						/* Free, if we allocated. */
						if (alloc)
							FREE(ptr, M_XDATA);
						return EINVAL;
					}
				}

				/* Advance. */
				len += ((ip6e->ip6e_len + 1) << 3);
				off = ip6e->ip6e_nxt;
				break;

			case IPPROTO_ROUTING:
				/*
				 * Always include routing headers in
				 * computation.
				 */
				ip6e = (struct ip6_ext *) (ptr + len);
				len += ((ip6e->ip6e_len + 1) << 3);
				off = ip6e->ip6e_nxt;
				break;

			default:
				DPRINTF(("ah_massage_headers: unexpected "
				    "IPv6 header type %d", off));
				if (alloc)
					FREE(ptr, M_XDATA);
				m_freem(m);
				return EINVAL;
			}

		/* Copyback and free, if we allocated. */
		if (alloc) {
			m_copyback(m, sizeof(struct ip6_hdr),
			    skip - sizeof(struct ip6_hdr), ptr);
			free(ptr, M_XDATA);
		}

		break;
#endif /* INET6 */
	}

	return 0;
}

/*
 * ah_input() gets called to verify that an input packet
 * passes authentication.
 */
static int
ah_input(struct mbuf *m, struct secasvar *sav, int skip, int protoff)
{
	struct auth_hash *ahx;
	struct tdb_ident *tdbi;
	struct tdb_crypto *tc;
	struct m_tag *mtag;
	struct newah *ah;
	int hl, rplen, authsize;

	struct cryptodesc *crda;
	struct cryptop *crp;

	SPLASSERT(net, "ah_input");

	KASSERT(sav != NULL, ("ah_input: null SA"));
	KASSERT(sav->key_auth != NULL,
		("ah_input: null authentication key"));
	KASSERT(sav->tdb_authalgxform != NULL,
		("ah_input: null authentication xform"));

	/* Figure out header size. */
	rplen = HDRSIZE(sav);

	/* XXX don't pullup, just copy header */
	IP6_EXTHDR_GET(ah, struct newah *, m, skip, rplen);
	if (ah == NULL) {
		DPRINTF(("ah_input: cannot pullup header\n"));
		ahstat.ahs_hdrops++;		/*XXX*/
		m_freem(m);
		return ENOBUFS;
	}

	/* Check replay window, if applicable. */
	if (sav->replay && !ipsec_chkreplay(ntohl(ah->ah_seq), sav)) {
		ahstat.ahs_replay++;
		DPRINTF(("ah_input: packet replay failure: %s\n",
			  ipsec_logsastr(sav)));
		m_freem(m);
		return ENOBUFS;
	}

	/* Verify AH header length. */
	hl = ah->ah_len * sizeof (u_int32_t);
	ahx = sav->tdb_authalgxform;
	authsize = AUTHSIZE(sav);
	if (hl != authsize + rplen - sizeof (struct ah)) {
		DPRINTF(("ah_input: bad authenticator length %u (expecting %lu)"
			" for packet in SA %s/%08lx\n",
			hl, (u_long) (authsize + rplen - sizeof (struct ah)),
			ipsec_address(&sav->sah->saidx.dst),
			(u_long) ntohl(sav->spi)));
		ahstat.ahs_badauthl++;
		m_freem(m);
		return EACCES;
	}
	ahstat.ahs_ibytes += m->m_pkthdr.len - skip - hl;

	/* Get crypto descriptors. */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		DPRINTF(("ah_input: failed to acquire crypto descriptor\n"));
		ahstat.ahs_crypto++;
		m_freem(m);
		return ENOBUFS;
	}

	crda = crp->crp_desc;
	KASSERT(crda != NULL, ("ah_input: null crypto descriptor"));

	crda->crd_skip = 0;
	crda->crd_len = m->m_pkthdr.len;
	crda->crd_inject = skip + rplen;

	/* Authentication operation. */
	crda->crd_alg = ahx->type;
	crda->crd_key = _KEYBUF(sav->key_auth);
	crda->crd_klen = _KEYBITS(sav->key_auth);

	/* Find out if we've already done crypto. */
	for (mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, NULL);
	     mtag != NULL;
	     mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_CRYPTO_DONE, mtag)) {
		tdbi = (struct tdb_ident *) (mtag + 1);
		if (tdbi->proto == sav->sah->saidx.proto &&
		    tdbi->spi == sav->spi &&
		    !bcmp(&tdbi->dst, &sav->sah->saidx.dst,
			  sizeof (union sockaddr_union)))
			break;
	}

	/* Allocate IPsec-specific opaque crypto info. */
	if (mtag == NULL) {
		tc = malloc(sizeof (struct tdb_crypto) +
			skip + rplen + authsize, M_XDATA, 
			M_INTWAIT | M_ZERO | M_NULLOK);
	} else {
		/* Hash verification has already been done successfully. */
		tc = malloc(sizeof (struct tdb_crypto),
			M_XDATA, M_INTWAIT | M_ZERO | M_NULLOK);
	}
	if (tc == NULL) {
		DPRINTF(("ah_input: failed to allocate tdb_crypto\n"));
		ahstat.ahs_crypto++;
		crypto_freereq(crp);
		m_freem(m);
		return ENOBUFS;
	}

	/* Only save information if crypto processing is needed. */
	if (mtag == NULL) {
		int error;

		/*
		 * Save the authenticator, the skipped portion of the packet,
		 * and the AH header.
		 */
		m_copydata(m, 0, skip + rplen + authsize, (caddr_t)(tc+1));

		/* Zeroize the authenticator on the packet. */
		m_copyback(m, skip + rplen, authsize, ipseczeroes);

		/* "Massage" the packet headers for crypto processing. */
		error = ah_massage_headers(&m, sav->sah->saidx.dst.sa.sa_family,
		    skip, ahx->type, 0);
		if (error != 0) {
			/* NB: mbuf is free'd by ah_massage_headers */
			ahstat.ahs_hdrops++;
			free(tc, M_XDATA);
			crypto_freereq(crp);
			return error;
		}
	}

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = ah_input_cb;
	crp->crp_sid = sav->tdb_cryptoid;
	crp->crp_opaque = (caddr_t) tc;

	/* These are passed as-is to the callback. */
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_nxt = ah->ah_nxt;
	tc->tc_protoff = protoff;
	tc->tc_skip = skip;
	tc->tc_ptr = (caddr_t) mtag; /* Save the mtag we've identified. */

	if (mtag == NULL)
		return crypto_dispatch(crp);
	else
		return ah_input_cb(crp);
}

#ifdef INET6
#define	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag) do {		     \
	if (saidx->dst.sa.sa_family == AF_INET6) {			     \
		error = ipsec6_common_input_cb(m, sav, skip, protoff, mtag); \
	} else {							     \
		error = ipsec4_common_input_cb(m, sav, skip, protoff, mtag); \
	}								     \
} while (0)
#else
#define	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag)		     \
	(error = ipsec4_common_input_cb(m, sav, skip, protoff, mtag))
#endif

/*
 * AH input callback from the crypto driver.
 */
static int
ah_input_cb(struct cryptop *crp)
{
	int rplen, error, skip, protoff;
	unsigned char calc[AH_ALEN_MAX];
	struct mbuf *m;
	struct cryptodesc *crd;
	struct auth_hash *ahx;
	struct tdb_crypto *tc;
	struct m_tag *mtag;
	struct secasvar *sav;
	struct secasindex *saidx;
	u_int8_t nxt;
	caddr_t ptr;
	int s, authsize;

	crd = crp->crp_desc;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	KASSERT(tc != NULL, ("ah_input_cb: null opaque crypto data area!"));
	skip = tc->tc_skip;
	nxt = tc->tc_nxt;
	protoff = tc->tc_protoff;
	mtag = (struct m_tag *) tc->tc_ptr;
	m = (struct mbuf *) crp->crp_buf;

	s = splnet();

	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		ahstat.ahs_notdb++;
		DPRINTF(("ah_input_cb: SA expired while in crypto\n"));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}

	saidx = &sav->sah->saidx;
	KASSERT(saidx->dst.sa.sa_family == AF_INET ||
		saidx->dst.sa.sa_family == AF_INET6,
		("ah_input_cb: unexpected protocol family %u",
		 saidx->dst.sa.sa_family));

	ahx = (struct auth_hash *) sav->tdb_authalgxform;

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN)
			return crypto_dispatch(crp);

		ahstat.ahs_noxform++;
		DPRINTF(("ah_input_cb: crypto error %d\n", crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	} else {
		ahstat.ahs_hist[sav->alg_auth]++;
		crypto_freereq(crp);		/* No longer needed. */
		crp = NULL;
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		ahstat.ahs_crypto++;
		DPRINTF(("ah_input_cb: bogus returned buffer from crypto\n"));
		error = EINVAL;
		goto bad;
	}

	/* Figure out header size. */
	rplen = HDRSIZE(sav);
	authsize = AUTHSIZE(sav);

	/* Copy authenticator off the packet. */
	m_copydata(m, skip + rplen, authsize, calc);

	/*
	 * If we have an mtag, we don't need to verify the authenticator --
	 * it has been verified by an IPsec-aware NIC.
	 */
	if (mtag == NULL) {
		ptr = (caddr_t) (tc + 1);

		/* Verify authenticator. */
		if (bcmp(ptr + skip + rplen, calc, authsize)) {
			DPRINTF(("ah_input: authentication hash mismatch "
			    "for packet in SA %s/%08lx\n",
			    ipsec_address(&saidx->dst),
			    (u_long) ntohl(sav->spi)));
			ahstat.ahs_badauth++;
			error = EACCES;
			goto bad;
		}

		/* Fix the Next Protocol field. */
		((u_int8_t *) ptr)[protoff] = nxt;

		/* Copyback the saved (uncooked) network headers. */
		m_copyback(m, 0, skip, ptr);
	} else {
		/* Fix the Next Protocol field. */
		m_copyback(m, protoff, sizeof(u_int8_t), &nxt);
	}

	free(tc, M_XDATA), tc = NULL;			/* No longer needed */

	/*
	 * Header is now authenticated.
	 */
	m->m_flags |= M_AUTHIPHDR|M_AUTHIPDGM;

	/*
	 * Update replay sequence number, if appropriate.
	 */
	if (sav->replay) {
		u_int32_t seq;

		m_copydata(m, skip + offsetof(struct newah, ah_seq),
			   sizeof (seq), (caddr_t) &seq);
		if (ipsec_updatereplay(ntohl(seq), sav)) {
			ahstat.ahs_replay++;
			error = ENOBUFS;			/*XXX as above*/
			goto bad;
		}
	}

	/*
	 * Remove the AH header and authenticator from the mbuf.
	 */
	error = m_striphdr(m, skip, rplen + authsize);
	if (error) {
		DPRINTF(("ah_input_cb: mangled mbuf chain for SA %s/%08lx\n",
		    ipsec_address(&saidx->dst), (u_long) ntohl(sav->spi)));

		ahstat.ahs_hdrops++;
		goto bad;
	}

	IPSEC_COMMON_INPUT_CB(m, sav, skip, protoff, mtag);

	KEY_FREESAV(&sav);
	splx(s);
	return error;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	splx(s);
	if (m != NULL)
		m_freem(m);
	if (tc != NULL)
		free(tc, M_XDATA);
	if (crp != NULL)
		crypto_freereq(crp);
	return error;
}

/*
 * AH output routine, called by ipsec[46]_process_packet().
 */
static int
ah_output(
	struct mbuf *m,
	struct ipsecrequest *isr,
	struct mbuf **mp,
	int skip,
	int protoff)
{
	struct secasvar *sav;
	struct auth_hash *ahx;
	struct cryptodesc *crda;
	struct tdb_crypto *tc;
	struct mbuf *mi;
	struct cryptop *crp;
	u_int16_t iplen;
	int error, rplen, authsize, maxpacketsize, roff;
	u_int8_t prot;
	struct newah *ah;

	SPLASSERT(net, "ah_output");

	sav = isr->sav;
	KASSERT(sav != NULL, ("ah_output: null SA"));
	ahx = sav->tdb_authalgxform;
	KASSERT(ahx != NULL, ("ah_output: null authentication xform"));

	ahstat.ahs_output++;

	/* Figure out header size. */
	rplen = HDRSIZE(sav);

	/* Check for maximum packet size violations. */
	switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		maxpacketsize = IP_MAXPACKET;
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		maxpacketsize = IPV6_MAXPACKET;
		break;
#endif /* INET6 */
	default:
		DPRINTF(("ah_output: unknown/unsupported protocol "
		    "family %u, SA %s/%08lx\n",
		    sav->sah->saidx.dst.sa.sa_family,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		ahstat.ahs_nopf++;
		error = EPFNOSUPPORT;
		goto bad;
	}
	authsize = AUTHSIZE(sav);
	if (rplen + authsize + m->m_pkthdr.len > maxpacketsize) {
		DPRINTF(("ah_output: packet in SA %s/%08lx got too big "
		    "(len %u, max len %u)\n",
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi),
		    rplen + authsize + m->m_pkthdr.len, maxpacketsize));
		ahstat.ahs_toobig++;
		error = EMSGSIZE;
		goto bad;
	}

	/* Update the counters. */
	ahstat.ahs_obytes += m->m_pkthdr.len - skip;

	m = m_clone(m);
	if (m == NULL) {
		DPRINTF(("ah_output: cannot clone mbuf chain, SA %s/%08lx\n",
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		ahstat.ahs_hdrops++;
		error = ENOBUFS;
		goto bad;
	}

	/* Inject AH header. */
	mi = m_makespace(m, skip, rplen + authsize, &roff);
	if (mi == NULL) {
		DPRINTF(("ah_output: failed to inject %u byte AH header for SA "
		    "%s/%08lx\n",
		    rplen + authsize,
		    ipsec_address(&sav->sah->saidx.dst),
		    (u_long) ntohl(sav->spi)));
		ahstat.ahs_hdrops++;		/*XXX differs from openbsd */
		error = ENOBUFS;
		goto bad;
	}

	/*
	 * The AH header is guaranteed by m_makespace() to be in
	 * contiguous memory, at roff bytes offset into the returned mbuf.
	 */
	ah = (struct newah *)(mtod(mi, caddr_t) + roff);

	/* Initialize the AH header. */
	m_copydata(m, protoff, sizeof(u_int8_t), (caddr_t) &ah->ah_nxt);
	ah->ah_len = (rplen + authsize - sizeof(struct ah)) / sizeof(u_int32_t);
	ah->ah_reserve = 0;
	ah->ah_spi = sav->spi;

	/* Zeroize authenticator. */
	m_copyback(m, skip + rplen, authsize, ipseczeroes);

	/* Insert packet replay counter, as requested.  */
	if (sav->replay) {
		if (sav->replay->count == ~0 &&
		    (sav->flags & SADB_X_EXT_CYCSEQ) == 0) {
			DPRINTF(("ah_output: replay counter wrapped for SA "
				"%s/%08lx\n",
				ipsec_address(&sav->sah->saidx.dst),
				(u_long) ntohl(sav->spi)));
			ahstat.ahs_wrap++;
			error = EINVAL;
			goto bad;
		}
		sav->replay->count++;
		ah->ah_seq = htonl(sav->replay->count);
	}

	/* Get crypto descriptors. */
	crp = crypto_getreq(1);
	if (crp == NULL) {
		DPRINTF(("ah_output: failed to acquire crypto descriptors\n"));
		ahstat.ahs_crypto++;
		error = ENOBUFS;
		goto bad;
	}

	crda = crp->crp_desc;

	crda->crd_skip = 0;
	crda->crd_inject = skip + rplen;
	crda->crd_len = m->m_pkthdr.len;

	/* Authentication operation. */
	crda->crd_alg = ahx->type;
	crda->crd_key = _KEYBUF(sav->key_auth);
	crda->crd_klen = _KEYBITS(sav->key_auth);

	/* Allocate IPsec-specific opaque crypto info. */
	tc = malloc(sizeof(struct tdb_crypto) + skip, M_XDATA,
		M_INTWAIT | M_ZERO | M_NULLOK);
	if (tc == NULL) {
		crypto_freereq(crp);
		DPRINTF(("ah_output: failed to allocate tdb_crypto\n"));
		ahstat.ahs_crypto++;
		error = ENOBUFS;
		goto bad;
	}

	/* Save the skipped portion of the packet. */
	m_copydata(m, 0, skip, (caddr_t) (tc + 1));

	/*
	 * Fix IP header length on the header used for
	 * authentication. We don't need to fix the original
	 * header length as it will be fixed by our caller.
	 */
	switch (sav->sah->saidx.dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		bcopy(((caddr_t)(tc + 1)) +
		    offsetof(struct ip, ip_len),
		    (caddr_t) &iplen, sizeof(u_int16_t));
		iplen = htons(ntohs(iplen) + rplen + authsize);
		m_copyback(m, offsetof(struct ip, ip_len),
		    sizeof(u_int16_t), (caddr_t) &iplen);
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		bcopy(((caddr_t)(tc + 1)) +
		    offsetof(struct ip6_hdr, ip6_plen),
		    (caddr_t) &iplen, sizeof(u_int16_t));
		iplen = htons(ntohs(iplen) + rplen + authsize);
		m_copyback(m, offsetof(struct ip6_hdr, ip6_plen),
		    sizeof(u_int16_t), (caddr_t) &iplen);
		break;
#endif /* INET6 */
	}

	/* Fix the Next Header field in saved header. */
	((u_int8_t *) (tc + 1))[protoff] = IPPROTO_AH;

	/* Update the Next Protocol field in the IP header. */
	prot = IPPROTO_AH;
	m_copyback(m, protoff, sizeof(u_int8_t), (caddr_t) &prot);

	/* "Massage" the packet headers for crypto processing. */
	error = ah_massage_headers(&m, sav->sah->saidx.dst.sa.sa_family,
			skip, ahx->type, 1);
	if (error != 0) {
		m = NULL;	/* mbuf was free'd by ah_massage_headers. */
		free(tc, M_XDATA);
		crypto_freereq(crp);
		goto bad;
	}

	/* Crypto operation descriptor. */
	crp->crp_ilen = m->m_pkthdr.len; /* Total input length. */
	crp->crp_flags = CRYPTO_F_IMBUF;
	crp->crp_buf = (caddr_t) m;
	crp->crp_callback = ah_output_cb;
	crp->crp_sid = sav->tdb_cryptoid;
	crp->crp_opaque = (caddr_t) tc;

	/* These are passed as-is to the callback. */
	tc->tc_isr = isr;
	tc->tc_spi = sav->spi;
	tc->tc_dst = sav->sah->saidx.dst;
	tc->tc_proto = sav->sah->saidx.proto;
	tc->tc_skip = skip;
	tc->tc_protoff = protoff;

	return crypto_dispatch(crp);
bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * AH output callback from the crypto driver.
 */
static int
ah_output_cb(struct cryptop *crp)
{
	int skip, protoff, error;
	struct tdb_crypto *tc;
	struct ipsecrequest *isr;
	struct secasvar *sav;
	struct mbuf *m;
	caddr_t ptr;
	int s, err;

	tc = (struct tdb_crypto *) crp->crp_opaque;
	KASSERT(tc != NULL, ("ah_output_cb: null opaque data area!"));
	skip = tc->tc_skip;
	protoff = tc->tc_protoff;
	ptr = (caddr_t) (tc + 1);
	m = (struct mbuf *) crp->crp_buf;

	s = splnet();

	isr = tc->tc_isr;
	sav = KEY_ALLOCSA(&tc->tc_dst, tc->tc_proto, tc->tc_spi);
	if (sav == NULL) {
		ahstat.ahs_notdb++;
		DPRINTF(("ah_output_cb: SA expired while in crypto\n"));
		error = ENOBUFS;		/*XXX*/
		goto bad;
	}
	KASSERT(isr->sav == sav, ("ah_output_cb: SA changed\n"));

	/* Check for crypto errors. */
	if (crp->crp_etype) {
		if (sav->tdb_cryptoid != 0)
			sav->tdb_cryptoid = crp->crp_sid;

		if (crp->crp_etype == EAGAIN) {
			KEY_FREESAV(&sav);
			splx(s);
			return crypto_dispatch(crp);
		}

		ahstat.ahs_noxform++;
		DPRINTF(("ah_output_cb: crypto error %d\n", crp->crp_etype));
		error = crp->crp_etype;
		goto bad;
	}

	/* Shouldn't happen... */
	if (m == NULL) {
		ahstat.ahs_crypto++;
		DPRINTF(("ah_output_cb: bogus returned buffer from crypto\n"));
		error = EINVAL;
		goto bad;
	}
	ahstat.ahs_hist[sav->alg_auth]++;

	/*
	 * Copy original headers (with the new protocol number) back
	 * in place.
	 */
	m_copyback(m, 0, skip, ptr);

	/* No longer needed. */
	free(tc, M_XDATA);
	crypto_freereq(crp);

	/* NB: m is reclaimed by ipsec_process_done. */
	err = ipsec_process_done(m, isr);
	KEY_FREESAV(&sav);
	splx(s);
	return err;
bad:
	if (sav)
		KEY_FREESAV(&sav);
	splx(s);
	if (m)
		m_freem(m);
	free(tc, M_XDATA);
	crypto_freereq(crp);
	return error;
}

static struct xformsw ah_xformsw = {
	XF_AH,		XFT_AUTH,	"IPsec AH",
	ah_init,	ah_zeroize,	ah_input,	ah_output,
};

static void
ah_attach(void)
{
	xform_register(&ah_xformsw);
}
SYSINIT(ah_xform_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_MIDDLE, ah_attach, NULL);
