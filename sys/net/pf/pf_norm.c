/*	$FreeBSD: src/sys/contrib/pf/net/pf_norm.c,v 1.10 2004/08/14 15:32:40 dwmalone Exp $	*/
/*	$OpenBSD: pf_norm.c,v 1.80.2.1 2004/04/30 21:46:33 brad Exp $ */
/* add	$OpenBSD: pf_norm.c,v 1.87 2004/05/11 07:34:11 dhartmei Exp $ */
/*	$DragonFly: src/sys/net/pf/pf_norm.c,v 1.3 2005/02/17 13:59:59 joerg Exp $ */

/*
 * Copyright (c) 2004 The DragonFly Project.  All rights reserved.
 *
 * Copyright 2001 Niels Provos <provos@citi.umich.edu>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <vm/vm_zone.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <net/pf/if_pflog.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#include <net/pf/pfvar.h>

#ifdef INET6
/*
 * XXX: This should go to netinet/ip6.h (KAME)
 */
/* IPv6 options: common part */
struct ip6_opt {
	u_int8_t ip6o_type;
	u_int8_t ip6o_len;
} __packed;

/* Jumbo Payload Option */
struct ip6_opt_jumbo {
	u_int8_t ip6oj_type;
	u_int8_t ip6oj_len;
	u_int8_t ip6oj_jumbo_len[4];
} __packed;

/* NSAP Address Option */
struct ip6_opt_nsap {
	u_int8_t ip6on_type;
	u_int8_t ip6on_len;
	u_int8_t ip6on_src_nsap_len;
	u_int8_t ip6on_dst_nsap_len;
	/* followed by source NSAP */
	/* followed by destination NSAP */
} __packed;

/* Tunnel Limit Option */
struct ip6_opt_tunnel {
	u_int8_t ip6ot_type;
	u_int8_t ip6ot_len;
	u_int8_t ip6ot_encap_limit;
} __packed;

/* Router Alert Option */
struct ip6_opt_router {
	u_int8_t ip6or_type;
	u_int8_t ip6or_len;
	u_int8_t ip6or_value[2];
} __packed;
#endif /* INET6 */

#define PFFRAG_SEENLAST	0x0001		/* Seen the last fragment for this */
#define PFFRAG_NOBUFFER	0x0002		/* Non-buffering fragment cache */
#define PFFRAG_DROP	0x0004		/* Drop all fragments */
#define BUFFER_FRAGMENTS(fr)	(!((fr)->fr_flags & PFFRAG_NOBUFFER))


TAILQ_HEAD(pf_fragqueue, pf_fragment)	pf_fragqueue;
TAILQ_HEAD(pf_cachequeue, pf_fragment)	pf_cachequeue;

static int	 	pf_frag_compare(struct pf_fragment *,
					struct pf_fragment *);
RB_HEAD(pf_frag_tree, pf_fragment)	pf_frag_tree, pf_cache_tree;
RB_PROTOTYPE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);
RB_GENERATE(pf_frag_tree, pf_fragment, fr_entry, pf_frag_compare);

/* Private prototypes */
void			 pf_ip2key(struct pf_fragment *, struct ip *);
void			 pf_remove_fragment(struct pf_fragment *);
void			 pf_flush_fragments(void);
void			 pf_free_fragment(struct pf_fragment *);
struct pf_fragment	*pf_find_fragment(struct ip *, struct pf_frag_tree *);
struct mbuf		*pf_reassemble(struct mbuf **, struct pf_fragment **,
			    struct pf_frent *, int);
struct mbuf		*pf_fragcache(struct mbuf **, struct ip*,
			    struct pf_fragment **, int, int, int *);
u_int16_t		 pf_cksum_fixup(u_int16_t, u_int16_t, u_int16_t);
int			 pf_normalize_tcpopt(struct pf_rule *, struct mbuf *,
			    struct tcphdr *, int);

#define	DPFPRINTF(x)	if (pf_status.debug >= PF_DEBUG_MISC) \
			    { printf("%s: ", __func__); printf x ;}

/* Globals */
vm_zone_t		 pf_frent_pl, pf_frag_pl, pf_cache_pl, pf_cent_pl;
vm_zone_t		 pf_state_scrub_pl;
int			 pf_nfrents, pf_ncache;

void
pf_normalize_init(void)
{
	/* XXX
	pool_sethiwat(&pf_frag_pl, PFFRAG_FRAG_HIWAT);
	pool_sethardlimit(&pf_frent_pl, PFFRAG_FRENT_HIWAT, NULL, 0);
	pool_sethardlimit(&pf_cache_pl, PFFRAG_FRCACHE_HIWAT, NULL, 0);
	pool_sethardlimit(&pf_cent_pl, PFFRAG_FRCENT_HIWAT, NULL, 0);
	*/

	TAILQ_INIT(&pf_fragqueue);
	TAILQ_INIT(&pf_cachequeue);
}

static int
pf_frag_compare(struct pf_fragment *a, struct pf_fragment *b)
{
	int	diff;

	if ((diff = a->fr_id - b->fr_id))
		return (diff);
	else if ((diff = a->fr_p - b->fr_p))
		return (diff);
	else if (a->fr_src.s_addr < b->fr_src.s_addr)
		return (-1);
	else if (a->fr_src.s_addr > b->fr_src.s_addr)
		return (1);
	else if (a->fr_dst.s_addr < b->fr_dst.s_addr)
		return (-1);
	else if (a->fr_dst.s_addr > b->fr_dst.s_addr)
		return (1);
	return (0);
}

void
pf_purge_expired_fragments(void)
{
	struct pf_fragment	*frag;
	u_int32_t		 expire = time_second -
				    pf_default_rule.timeout[PFTM_FRAG];

	while ((frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue)) != NULL) {
		KASSERT((BUFFER_FRAGMENTS(frag)),
			("BUFFER_FRAGMENTS(frag) == 0: %s", __func__));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
	}

	while ((frag = TAILQ_LAST(&pf_cachequeue, pf_cachequeue)) != NULL) {
		KASSERT((!BUFFER_FRAGMENTS(frag)),
			("BUFFER_FRAGMENTS(frag) != 0: %s", __func__));
		if (frag->fr_timeout > expire)
			break;

		DPFPRINTF(("expiring %d(%p)\n", frag->fr_id, frag));
		pf_free_fragment(frag);
		KASSERT((TAILQ_EMPTY(&pf_cachequeue) ||
		    TAILQ_LAST(&pf_cachequeue, pf_cachequeue) != frag),
		    ("!(TAILQ_EMPTY() || TAILQ_LAST() == farg): %s",
		    __func__));
	}
}

/*
 * Try to flush old fragments to make space for new ones
 */

void
pf_flush_fragments(void)
{
	struct pf_fragment	*frag;
	int			 goal;

	goal = pf_nfrents * 9 / 10;
	DPFPRINTF(("trying to free > %d frents\n",
	    pf_nfrents - goal));
	while (goal < pf_nfrents) {
		frag = TAILQ_LAST(&pf_fragqueue, pf_fragqueue);
		if (frag == NULL)
			break;
		pf_free_fragment(frag);
	}


	goal = pf_ncache * 9 / 10;
	DPFPRINTF(("trying to free > %d cache entries\n",
	    pf_ncache - goal));
	while (goal < pf_ncache) {
		frag = TAILQ_LAST(&pf_cachequeue, pf_cachequeue);
		if (frag == NULL)
			break;
		pf_free_fragment(frag);
	}
}

/* Frees the fragments and all associated entries */

void
pf_free_fragment(struct pf_fragment *frag)
{
	struct pf_frent		*frent;
	struct pf_frcache	*frcache;

	/* Free all fragments */
	if (BUFFER_FRAGMENTS(frag)) {
		for (frent = LIST_FIRST(&frag->fr_queue); frent;
		    frent = LIST_FIRST(&frag->fr_queue)) {
			LIST_REMOVE(frent, fr_next);

			m_freem(frent->fr_m);
			pool_put(&pf_frent_pl, frent);
			pf_nfrents--;
		}
	} else {
		for (frcache = LIST_FIRST(&frag->fr_cache); frcache;
		    frcache = LIST_FIRST(&frag->fr_cache)) {
			LIST_REMOVE(frcache, fr_next);

			KASSERT((LIST_EMPTY(&frag->fr_cache) ||
			    LIST_FIRST(&frag->fr_cache)->fr_off >
			    frcache->fr_end),
			    ("! (LIST_EMPTY() || LIST_FIRST()->fr_off >"
                             " frcache->fr_end): %s", __func__));

			pool_put(&pf_cent_pl, frcache);
			pf_ncache--;
		}
	}

	pf_remove_fragment(frag);
}

void
pf_ip2key(struct pf_fragment *key, struct ip *ip)
{
	key->fr_p = ip->ip_p;
	key->fr_id = ip->ip_id;
	key->fr_src.s_addr = ip->ip_src.s_addr;
	key->fr_dst.s_addr = ip->ip_dst.s_addr;
}

struct pf_fragment *
pf_find_fragment(struct ip *ip, struct pf_frag_tree *tree)
{
	struct pf_fragment	 key;
	struct pf_fragment	*frag;

	pf_ip2key(&key, ip);

	frag = RB_FIND(pf_frag_tree, tree, &key);
	if (frag != NULL) {
		/* XXX Are we sure we want to update the timeout? */
		frag->fr_timeout = time_second;
		if (BUFFER_FRAGMENTS(frag)) {
			TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
			TAILQ_INSERT_HEAD(&pf_fragqueue, frag, frag_next);
		} else {
			TAILQ_REMOVE(&pf_cachequeue, frag, frag_next);
			TAILQ_INSERT_HEAD(&pf_cachequeue, frag, frag_next);
		}
	}

	return (frag);
}

/* Removes a fragment from the fragment queue and frees the fragment */

void
pf_remove_fragment(struct pf_fragment *frag)
{
	if (BUFFER_FRAGMENTS(frag)) {
		RB_REMOVE(pf_frag_tree, &pf_frag_tree, frag);
		TAILQ_REMOVE(&pf_fragqueue, frag, frag_next);
		pool_put(&pf_frag_pl, frag);
	} else {
		RB_REMOVE(pf_frag_tree, &pf_cache_tree, frag);
		TAILQ_REMOVE(&pf_cachequeue, frag, frag_next);
		pool_put(&pf_cache_pl, frag);
	}
}

#define FR_IP_OFF(fr)	(((fr)->fr_ip->ip_off & IP_OFFMASK) << 3)
struct mbuf *
pf_reassemble(struct mbuf **m0, struct pf_fragment **frag,
    struct pf_frent *frent, int mff)
{
	struct mbuf	*m = *m0, *m2;
	struct pf_frent	*frea, *next;
	struct pf_frent	*frep = NULL;
	struct ip	*ip = frent->fr_ip;
	int		 hlen = ip->ip_hl << 2;
	u_int16_t	 off = (ip->ip_off & IP_OFFMASK) << 3;
	u_int16_t	 ip_len = ip->ip_len - ip->ip_hl * 4;
	u_int16_t	 max = ip_len + off;

	KASSERT((*frag == NULL || BUFFER_FRAGMENTS(*frag)),
	    ("! (*frag == NULL || BUFFER_FRAGMENTS(*frag)): %s", __func__));

	/* Strip off ip header */
	m->m_data += hlen;
	m->m_len -= hlen;

	/* Create a new reassembly queue for this packet */
	if (*frag == NULL) {
		*frag = pool_get(&pf_frag_pl, PR_NOWAIT);
		if (*frag == NULL) {
			pf_flush_fragments();
			*frag = pool_get(&pf_frag_pl, PR_NOWAIT);
			if (*frag == NULL)
				goto drop_fragment;
		}

		(*frag)->fr_flags = 0;
		(*frag)->fr_max = 0;
		(*frag)->fr_src = frent->fr_ip->ip_src;
		(*frag)->fr_dst = frent->fr_ip->ip_dst;
		(*frag)->fr_p = frent->fr_ip->ip_p;
		(*frag)->fr_id = frent->fr_ip->ip_id;
		(*frag)->fr_timeout = time_second;
		LIST_INIT(&(*frag)->fr_queue);

		RB_INSERT(pf_frag_tree, &pf_frag_tree, *frag);
		TAILQ_INSERT_HEAD(&pf_fragqueue, *frag, frag_next);

		/* We do not have a previous fragment */
		frep = NULL;
		goto insert;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	LIST_FOREACH(frea, &(*frag)->fr_queue, fr_next) {
		if (FR_IP_OFF(frea) > off)
			break;
		frep = frea;
	}

	KASSERT((frep != NULL || frea != NULL),
	    ("!(frep != NULL || frea != NULL): %s", __func__));;

	if (frep != NULL &&
	    FR_IP_OFF(frep) + frep->fr_ip->ip_len - frep->fr_ip->ip_hl *
	    4 > off)
	{
		u_int16_t	precut;

		precut = FR_IP_OFF(frep) + frep->fr_ip->ip_len -
		    frep->fr_ip->ip_hl * 4 - off;
		if (precut >= ip_len)
			goto drop_fragment;
		m_adj(frent->fr_m, precut);
		DPFPRINTF(("overlap -%d\n", precut));
		/* Enforce 8 byte boundaries */
		ip->ip_off = ip->ip_off + (precut >> 3);
		off = (ip->ip_off & IP_OFFMASK) << 3;
		ip_len -= precut;
		ip->ip_len = ip_len;
	}

	for (; frea != NULL && ip_len + off > FR_IP_OFF(frea);
	    frea = next)
	{
		u_int16_t	aftercut;

		aftercut = ip_len + off - FR_IP_OFF(frea);
		DPFPRINTF(("adjust overlap %d\n", aftercut));
		if (aftercut < frea->fr_ip->ip_len - frea->fr_ip->ip_hl
		    * 4)
		{
			frea->fr_ip->ip_len =
			    frea->fr_ip->ip_len - aftercut;
			frea->fr_ip->ip_off = frea->fr_ip->ip_off +
			    (aftercut >> 3);
			m_adj(frea->fr_m, aftercut);
			break;
		}

		/* This fragment is completely overlapped, loose it */
		next = LIST_NEXT(frea, fr_next);
		m_freem(frea->fr_m);
		LIST_REMOVE(frea, fr_next);
		pool_put(&pf_frent_pl, frea);
		pf_nfrents--;
	}

 insert:
	/* Update maximum data size */
	if ((*frag)->fr_max < max)
		(*frag)->fr_max = max;
	/* This is the last segment */
	if (!mff)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	if (frep == NULL)
		LIST_INSERT_HEAD(&(*frag)->fr_queue, frent, fr_next);
	else
		LIST_INSERT_AFTER(frep, frent, fr_next);

	/* Check if we are completely reassembled */
	if (!((*frag)->fr_flags & PFFRAG_SEENLAST))
		return (NULL);

	/* Check if we have all the data */
	off = 0;
	for (frep = LIST_FIRST(&(*frag)->fr_queue); frep; frep = next) {
		next = LIST_NEXT(frep, fr_next);

		off += frep->fr_ip->ip_len - frep->fr_ip->ip_hl * 4;
		if (off < (*frag)->fr_max &&
		    (next == NULL || FR_IP_OFF(next) != off))
		{
			DPFPRINTF(("missing fragment at %d, next %d, max %d\n",
			    off, next == NULL ? -1 : FR_IP_OFF(next),
			    (*frag)->fr_max));
			return (NULL);
		}
	}
	DPFPRINTF(("%d < %d?\n", off, (*frag)->fr_max));
	if (off < (*frag)->fr_max)
		return (NULL);

	/* We have all the data */
	frent = LIST_FIRST(&(*frag)->fr_queue);
	KASSERT((frent != NULL), ("frent == NULL: %s", __func__));
	if ((frent->fr_ip->ip_hl << 2) + off > IP_MAXPACKET) {
		DPFPRINTF(("drop: too big: %d\n", off));
		pf_free_fragment(*frag);
		*frag = NULL;
		return (NULL);
	}
	next = LIST_NEXT(frent, fr_next);

	/* Magic from ip_input */
	ip = frent->fr_ip;
	m = frent->fr_m;
	m2 = m->m_next;
	m->m_next = NULL;
	m_cat(m, m2);
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	for (frent = next; frent != NULL; frent = next) {
		next = LIST_NEXT(frent, fr_next);

		m2 = frent->fr_m;
		pool_put(&pf_frent_pl, frent);
		pf_nfrents--;
		m_cat(m, m2);
	}

	ip->ip_src = (*frag)->fr_src;
	ip->ip_dst = (*frag)->fr_dst;

	/* Remove from fragment queue */
	pf_remove_fragment(*frag);
	*frag = NULL;

	hlen = ip->ip_hl << 2;
	ip->ip_len = off + hlen;
	m->m_len += hlen;
	m->m_data -= hlen;

	/* some debugging cruft by sklower, below, will go away soon */
	/* XXX this should be done elsewhere */
	if (m->m_flags & M_PKTHDR) {
		int plen = 0;
		for (m2 = m; m2; m2 = m2->m_next)
			plen += m2->m_len;
		m->m_pkthdr.len = plen;
	}

	DPFPRINTF(("complete: %p(%d)\n", m, ip->ip_len));
	return (m);

 drop_fragment:
	/* Oops - fail safe - drop packet */
	pool_put(&pf_frent_pl, frent);
	pf_nfrents--;
	m_freem(m);
	return (NULL);
}

struct mbuf *
pf_fragcache(struct mbuf **m0, struct ip *h, struct pf_fragment **frag, int mff,
    int drop, int *nomem)
{
	struct mbuf		*m = *m0;
	struct pf_frcache	*frp, *fra, *cur = NULL;
	int			 ip_len = h->ip_len - (h->ip_hl << 2);
	u_int16_t		 off = h->ip_off << 3;
	u_int16_t		 max = ip_len + off;
	int			 hosed = 0;

	KASSERT((*frag == NULL || !BUFFER_FRAGMENTS(*frag)),
	    ("!(*frag == NULL || !BUFFER_FRAGMENTS(*frag)): %s", __func__));

	/* Create a new range queue for this packet */
	if (*frag == NULL) {
		*frag = pool_get(&pf_cache_pl, PR_NOWAIT);
		if (*frag == NULL) {
			pf_flush_fragments();
			*frag = pool_get(&pf_cache_pl, PR_NOWAIT);
			if (*frag == NULL)
				goto no_mem;
		}

		/* Get an entry for the queue */
		cur = pool_get(&pf_cent_pl, PR_NOWAIT);
		if (cur == NULL) {
			pool_put(&pf_cache_pl, *frag);
			*frag = NULL;
			goto no_mem;
		}
		pf_ncache++;

		(*frag)->fr_flags = PFFRAG_NOBUFFER;
		(*frag)->fr_max = 0;
		(*frag)->fr_src = h->ip_src;
		(*frag)->fr_dst = h->ip_dst;
		(*frag)->fr_p = h->ip_p;
		(*frag)->fr_id = h->ip_id;
		(*frag)->fr_timeout = time_second;

		cur->fr_off = off;
		cur->fr_end = max;
		LIST_INIT(&(*frag)->fr_cache);
		LIST_INSERT_HEAD(&(*frag)->fr_cache, cur, fr_next);

		RB_INSERT(pf_frag_tree, &pf_cache_tree, *frag);
		TAILQ_INSERT_HEAD(&pf_cachequeue, *frag, frag_next);

		DPFPRINTF(("fragcache[%d]: new %d-%d\n", h->ip_id, off, max));

		goto pass;
	}

	/*
	 * Find a fragment after the current one:
	 *  - off contains the real shifted offset.
	 */
	frp = NULL;
	LIST_FOREACH(fra, &(*frag)->fr_cache, fr_next) {
		if (fra->fr_off > off)
			break;
		frp = fra;
	}

	KASSERT((frp != NULL || fra != NULL),
	    ("!(frp != NULL || fra != NULL): %s", __func__));

	if (frp != NULL) {
		int	precut;

		precut = frp->fr_end - off;
		if (precut >= ip_len) {
			/* Fragment is entirely a duplicate */
			DPFPRINTF(("fragcache[%d]: dead (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			goto drop_fragment;
		}
		if (precut == 0) {
			/* They are adjacent.  Fixup cache entry */
			DPFPRINTF(("fragcache[%d]: adjacent (%d-%d) %d-%d\n",
			    h->ip_id, frp->fr_off, frp->fr_end, off, max));
			frp->fr_end = max;
		} else if (precut > 0) {
			/* The first part of this payload overlaps with a
			 * fragment that has already been passed.
			 * Need to trim off the first part of the payload.
			 * But to do so easily, we need to create another
			 * mbuf to throw the original header into.
			 */

			DPFPRINTF(("fragcache[%d]: chop %d (%d-%d) %d-%d\n",
			    h->ip_id, precut, frp->fr_off, frp->fr_end, off,
			    max));

			off += precut;
			max -= precut;
			/* Update the previous frag to encompass this one */
			frp->fr_end = max;

			if (!drop) {
				/* XXX Optimization opportunity
				 * This is a very heavy way to trim the payload.
				 * we could do it much faster by diddling mbuf
				 * internals but that would be even less legible
				 * than this mbuf magic.  For my next trick,
				 * I'll pull a rabbit out of my laptop.
				 */
				*m0 = m_dup(m, MB_DONTWAIT);
				/* From KAME Project : We have missed this! */
				m_adj(*m0, (h->ip_hl << 2) -
				    (*m0)->m_pkthdr.len);
				if (*m0 == NULL)
					goto no_mem;
				KASSERT(((*m0)->m_next == NULL), 
				    ("(*m0)->m_next != NULL: %s", 
				    __func__));
				m_adj(m, precut + (h->ip_hl << 2));
				m_cat(*m0, m);
				m = *m0;
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}


				h = mtod(m, struct ip *);

				KASSERT(((int)m->m_len ==
				    h->ip_len - precut),
				    ("m->m_len != h->ip_len - precut: %s",
				    __func__));
				h->ip_off = h->ip_off +
				    (precut >> 3);
				h->ip_len = h->ip_len - precut;
			} else {
				hosed++;
			}
		} else {
			/* There is a gap between fragments */

			DPFPRINTF(("fragcache[%d]: gap %d (%d-%d) %d-%d\n",
			    h->ip_id, -precut, frp->fr_off, frp->fr_end, off,
			    max));

			cur = pool_get(&pf_cent_pl, PR_NOWAIT);
			if (cur == NULL)
				goto no_mem;
			pf_ncache++;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_AFTER(frp, cur, fr_next);
		}
	}

	if (fra != NULL) {
		int	aftercut;
		int	merge = 0;

		aftercut = max - fra->fr_off;
		if (aftercut == 0) {
			/* Adjacent fragments */
			DPFPRINTF(("fragcache[%d]: adjacent %d-%d (%d-%d)\n",
			    h->ip_id, off, max, fra->fr_off, fra->fr_end));
			fra->fr_off = off;
			merge = 1;
		} else if (aftercut > 0) {
			/* Need to chop off the tail of this fragment */
			DPFPRINTF(("fragcache[%d]: chop %d %d-%d (%d-%d)\n",
			    h->ip_id, aftercut, off, max, fra->fr_off,
			    fra->fr_end));
			fra->fr_off = off;
			max -= aftercut;

			merge = 1;

			if (!drop) {
				m_adj(m, -aftercut);
				if (m->m_flags & M_PKTHDR) {
					int plen = 0;
					struct mbuf *t;
					for (t = m; t; t = t->m_next)
						plen += t->m_len;
					m->m_pkthdr.len = plen;
				}
				h = mtod(m, struct ip *);
				KASSERT(((int)m->m_len == h->ip_len - aftercut),
				    ("m->m_len != h->ip_len - aftercut: %s",
				    __func__));
				h->ip_len = h->ip_len - aftercut;
			} else {
				hosed++;
			}
		} else {
			/* There is a gap between fragments */
			DPFPRINTF(("fragcache[%d]: gap %d %d-%d (%d-%d)\n",
			    h->ip_id, -aftercut, off, max, fra->fr_off,
			    fra->fr_end));

			cur = pool_get(&pf_cent_pl, PR_NOWAIT);
			if (cur == NULL)
				goto no_mem;
			pf_ncache++;

			cur->fr_off = off;
			cur->fr_end = max;
			LIST_INSERT_BEFORE(fra, cur, fr_next);
		}


		/* Need to glue together two separate fragment descriptors */
		if (merge) {
			if (cur && fra->fr_off <= cur->fr_end) {
				/* Need to merge in a previous 'cur' */
				DPFPRINTF(("fragcache[%d]: adjacent(merge "
				    "%d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, cur->fr_off, cur->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = cur->fr_off;
				LIST_REMOVE(cur, fr_next);
				pool_put(&pf_cent_pl, cur);
				pf_ncache--;
				cur = NULL;

			} else if (frp && fra->fr_off <= frp->fr_end) {
				/* Need to merge in a modified 'frp' */
				KASSERT((cur == NULL), ("cur != NULL: %s",
				    __func__));
				DPFPRINTF(("fragcache[%d]: adjacent(merge "
				    "%d-%d) %d-%d (%d-%d)\n",
				    h->ip_id, frp->fr_off, frp->fr_end, off,
				    max, fra->fr_off, fra->fr_end));
				fra->fr_off = frp->fr_off;
				LIST_REMOVE(frp, fr_next);
				pool_put(&pf_cent_pl, frp);
				pf_ncache--;
				frp = NULL;

			}
		}
	}

	if (hosed) {
		/*
		 * We must keep tracking the overall fragment even when
		 * we're going to drop it anyway so that we know when to
		 * free the overall descriptor.  Thus we drop the frag late.
		 */
		goto drop_fragment;
	}


 pass:
	/* Update maximum data size */
	if ((*frag)->fr_max < max)
		(*frag)->fr_max = max;

	/* This is the last segment */
	if (!mff)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	/* Check if we are completely reassembled */
	if (((*frag)->fr_flags & PFFRAG_SEENLAST) &&
	    LIST_FIRST(&(*frag)->fr_cache)->fr_off == 0 &&
	    LIST_FIRST(&(*frag)->fr_cache)->fr_end == (*frag)->fr_max) {
		/* Remove from fragment queue */
		DPFPRINTF(("fragcache[%d]: done 0-%d\n", h->ip_id,
		    (*frag)->fr_max));
		pf_free_fragment(*frag);
		*frag = NULL;
	}

	return (m);

 no_mem:
	*nomem = 1;

	/* Still need to pay attention to !IP_MF */
	if (!mff && *frag != NULL)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	m_freem(m);
	return (NULL);

 drop_fragment:

	/* Still need to pay attention to !IP_MF */
	if (!mff && *frag != NULL)
		(*frag)->fr_flags |= PFFRAG_SEENLAST;

	if (drop) {
		/* This fragment has been deemed bad.  Don't reass */
		if (((*frag)->fr_flags & PFFRAG_DROP) == 0)
			DPFPRINTF(("fragcache[%d]: dropping overall fragment\n",
			    h->ip_id));
		(*frag)->fr_flags |= PFFRAG_DROP;
	}

	m_freem(m);
	return (NULL);
}

int
pf_normalize_ip(struct mbuf **m0, int dir, struct pfi_kif *kif, u_short *reason)
{
	struct mbuf		*m = *m0;
	struct pf_rule		*r;
	struct pf_frent		*frent;
	struct pf_fragment	*frag = NULL;
	struct ip		*h = mtod(m, struct ip *);
	int			 mff = (h->ip_off & IP_MF);
	int			 hlen = h->ip_hl << 2;
	u_int16_t		 fragoff = (h->ip_off & IP_OFFMASK) << 3;
	u_int16_t		 max;
	int			 ip_len;
	int			 ip_off;

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (r->kif != NULL &&
		    (r->kif != kif && r->kif != kif->pfik_parent) == !r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != AF_INET)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != h->ip_p)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr,
		    (struct pf_addr *)&h->ip_src.s_addr, AF_INET, r->src.not))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr,
		    (struct pf_addr *)&h->ip_dst.s_addr, AF_INET, r->dst.not))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else
			break;
	}

	if (r == NULL)
		return (PF_PASS);
	else
		r->packets++;

	/* Check for illegal packets */
	if (hlen < (int)sizeof(struct ip))
		goto drop;

	if (hlen > h->ip_len)
		goto drop;

	/* Clear IP_DF if the rule uses the no-df option */
	if (r->rule_flag & PFRULE_NODF)
		h->ip_off &= ~IP_DF;

	/* We will need other tests here */
	if (!fragoff && !mff)
		goto no_fragment;

	/* We're dealing with a fragment now. Don't allow fragments
	 * with IP_DF to enter the cache. If the flag was cleared by
	 * no-df above, fine. Otherwise drop it.
	 */
	if (h->ip_off & IP_DF) {
		DPFPRINTF(("IP_DF\n"));
		goto bad;
	}

	ip_len = h->ip_len - hlen;
	ip_off = (h->ip_off & IP_OFFMASK) << 3;

	/* All fragments are 8 byte aligned */
	if (mff && (ip_len & 0x7)) {
		DPFPRINTF(("mff and %d\n", ip_len));
		goto bad;
	}

	/* Respect maximum length */
	if (fragoff + ip_len > IP_MAXPACKET) {
		DPFPRINTF(("max packet %d\n", fragoff + ip_len));
		goto bad;
	}
	max = fragoff + ip_len;

	if ((r->rule_flag & (PFRULE_FRAGCROP|PFRULE_FRAGDROP)) == 0) {
		/* Fully buffer all of the fragments */

		frag = pf_find_fragment(h, &pf_frag_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max)
			goto bad;

		/* Get an entry for the fragment queue */
		frent = pool_get(&pf_frent_pl, PR_NOWAIT);
		if (frent == NULL) {
			REASON_SET(reason, PFRES_MEMORY);
			return (PF_DROP);
		}
		pf_nfrents++;
		frent->fr_ip = h;
		frent->fr_m = m;

		/* Might return a completely reassembled mbuf, or NULL */
		DPFPRINTF(("reass frag %d @ %d-%d\n", h->ip_id, fragoff, max));
		*m0 = m = pf_reassemble(m0, &frag, frent, mff);

		if (m == NULL)
			return (PF_DROP);

		if (frag != NULL && (frag->fr_flags & PFFRAG_DROP))
			goto drop;

		h = mtod(m, struct ip *);
	} else {
		/* non-buffering fragment cache (drops or masks overlaps) */
		int	nomem = 0;

		if (dir == PF_OUT) {
			if (m->m_pkthdr.fw_flags & PF_MBUF_FRAGCACHE) {
				/* Already passed the fragment cache in the
				 * input direction.  If we continued, it would
				 * appear to be a dup and would be dropped.
				 */
				goto fragment_pass;
			}
		}

		frag = pf_find_fragment(h, &pf_cache_tree);

		/* Check if we saw the last fragment already */
		if (frag != NULL && (frag->fr_flags & PFFRAG_SEENLAST) &&
		    max > frag->fr_max) {
			if (r->rule_flag & PFRULE_FRAGDROP)
				frag->fr_flags |= PFFRAG_DROP;
			goto bad;
		}

		*m0 = m = pf_fragcache(m0, h, &frag, mff,
		    (r->rule_flag & PFRULE_FRAGDROP) ? 1 : 0, &nomem);
		if (m == NULL) {
			if (nomem)
				goto no_mem;
			goto drop;
		}

		if (dir == PF_IN)
			m->m_pkthdr.fw_flags |= PF_MBUF_FRAGCACHE;

		if (frag != NULL && (frag->fr_flags & PFFRAG_DROP))
			goto drop;
		goto fragment_pass;
	}

 no_fragment:
	/* At this point, only IP_DF is allowed in ip_off */
	h->ip_off &= IP_DF;

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (r->min_ttl && h->ip_ttl < r->min_ttl)
		h->ip_ttl = r->min_ttl;

	if (r->rule_flag & PFRULE_RANDOMID) {
#ifdef RANDOM_IP_ID
		h->ip_id = ip_randomid();
#else
		h->ip_id = htons(ip_id++);
#endif
	}

	return (PF_PASS);

 fragment_pass:
	/* Enforce a minimum ttl, may cause endless packet loops */
	if (r->min_ttl && h->ip_ttl < r->min_ttl)
		h->ip_ttl = r->min_ttl;

	return (PF_PASS);

 no_mem:
	REASON_SET(reason, PFRES_MEMORY);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET, dir, *reason, r, NULL, NULL);
	return (PF_DROP);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET, dir, *reason, r, NULL, NULL);
	return (PF_DROP);

 bad:
	DPFPRINTF(("dropping bad fragment\n"));

	/* Free associated fragments */
	if (frag != NULL)
		pf_free_fragment(frag);

	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET, dir, *reason, r, NULL, NULL);

	return (PF_DROP);
}

#ifdef INET6
int
pf_normalize_ip6(struct mbuf **m0, int dir, struct pfi_kif *kif,
    u_short *reason)
{
	struct mbuf		*m = *m0;
	struct pf_rule		*r;
	struct ip6_hdr		*h = mtod(m, struct ip6_hdr *);
	int			 off;
	struct ip6_ext		 ext;
	struct ip6_opt		 opt;
	struct ip6_opt_jumbo	 jumbo;
	struct ip6_frag		 frag;
	u_int32_t		 jumbolen = 0, plen;
	u_int16_t		 fragoff = 0;
	int			 optend;
	int			 ooff;
	u_int8_t		 proto;
	int			 terminal;

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (r->kif != NULL &&
		    (r->kif != kif && r->kif != kif->pfik_parent) == !r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != AF_INET6)
			r = r->skip[PF_SKIP_AF].ptr;
#if 0 /* header chain! */
		else if (r->proto && r->proto != h->ip6_nxt)
			r = r->skip[PF_SKIP_PROTO].ptr;
#endif
		else if (PF_MISMATCHAW(&r->src.addr,
		    (struct pf_addr *)&h->ip6_src, AF_INET6, r->src.not))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr,
		    (struct pf_addr *)&h->ip6_dst, AF_INET6, r->dst.not))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else
			break;
	}

	if (r == NULL)
		return (PF_PASS);
	else
		r->packets++;

	/* Check for illegal packets */
	if (sizeof(struct ip6_hdr) + IPV6_MAXPACKET < m->m_pkthdr.len)
		goto drop;

	off = sizeof(struct ip6_hdr);
	proto = h->ip6_nxt;
	terminal = 0;
	do {
		switch (proto) {
		case IPPROTO_FRAGMENT:
			goto fragment;
			break;
		case IPPROTO_AH:
		case IPPROTO_ROUTING:
		case IPPROTO_DSTOPTS:
			if (!pf_pull_hdr(m, off, &ext, sizeof(ext), NULL,
			    NULL, AF_INET6))
				goto shortpkt;
			if (proto == IPPROTO_AH)
				off += (ext.ip6e_len + 2) * 4;
			else
				off += (ext.ip6e_len + 1) * 8;
			proto = ext.ip6e_nxt;
			break;
		case IPPROTO_HOPOPTS:
			if (!pf_pull_hdr(m, off, &ext, sizeof(ext), NULL,
			    NULL, AF_INET6))
				goto shortpkt;
			optend = off + (ext.ip6e_len + 1) * 8;
			ooff = off + sizeof(ext);
			do {
				if (!pf_pull_hdr(m, ooff, &opt.ip6o_type,
				    sizeof(opt.ip6o_type), NULL, NULL,
				    AF_INET6))
					goto shortpkt;
				if (opt.ip6o_type == IP6OPT_PAD1) {
					ooff++;
					continue;
				}
				if (!pf_pull_hdr(m, ooff, &opt, sizeof(opt),
				    NULL, NULL, AF_INET6))
					goto shortpkt;
				if (ooff + sizeof(opt) + opt.ip6o_len > optend)
					goto drop;
				switch (opt.ip6o_type) {
				case IP6OPT_JUMBO:
					if (h->ip6_plen != 0)
						goto drop;
					if (!pf_pull_hdr(m, ooff, &jumbo,
					    sizeof(jumbo), NULL, NULL,
					    AF_INET6))
						goto shortpkt;
					memcpy(&jumbolen, jumbo.ip6oj_jumbo_len,
					    sizeof(jumbolen));
					jumbolen = ntohl(jumbolen);
					if (jumbolen <= IPV6_MAXPACKET)
						goto drop;
					if (sizeof(struct ip6_hdr) + jumbolen !=
					    m->m_pkthdr.len)
						goto drop;
					break;
				default:
					break;
				}
				ooff += sizeof(opt) + opt.ip6o_len;
			} while (ooff < optend);

			off = optend;
			proto = ext.ip6e_nxt;
			break;
		default:
			terminal = 1;
			break;
		}
	} while (!terminal);

	/* jumbo payload option must be present, or plen > 0 */
	if (ntohs(h->ip6_plen) == 0)
		plen = jumbolen;
	else
		plen = ntohs(h->ip6_plen);
	if (plen == 0)
		goto drop;
	if (sizeof(struct ip6_hdr) + plen > m->m_pkthdr.len)
		goto shortpkt;

	/* Enforce a minimum ttl, may cause endless packet loops */
	if (r->min_ttl && h->ip6_hlim < r->min_ttl)
		h->ip6_hlim = r->min_ttl;

	return (PF_PASS);

 fragment:
	if (ntohs(h->ip6_plen) == 0 || jumbolen)
		goto drop;
	plen = ntohs(h->ip6_plen);

	if (!pf_pull_hdr(m, off, &frag, sizeof(frag), NULL, NULL, AF_INET6))
		goto shortpkt;
	fragoff = ntohs(frag.ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff + (plen - off - sizeof(frag)) > IPV6_MAXPACKET)
		goto badfrag;

	/* do something about it */
	return (PF_PASS);

 shortpkt:
	REASON_SET(reason, PFRES_SHORT);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET6, dir, *reason, r, NULL, NULL);
	return (PF_DROP);

 drop:
	REASON_SET(reason, PFRES_NORM);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET6, dir, *reason, r, NULL, NULL);
	return (PF_DROP);

 badfrag:
	REASON_SET(reason, PFRES_FRAG);
	if (r != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET6, dir, *reason, r, NULL, NULL);
	return (PF_DROP);
}
#endif

int
pf_normalize_tcp(int dir, struct pfi_kif *kif, struct mbuf *m, int ipoff,
    int off, void *h, struct pf_pdesc *pd)
{
	struct pf_rule	*r, *rm = NULL;
	struct tcphdr	*th = pd->hdr.tcp;
	int		 rewrite = 0;
	u_short		 reason;
	u_int8_t	 flags;
	sa_family_t	 af = pd->af;

	r = TAILQ_FIRST(pf_main_ruleset.rules[PF_RULESET_SCRUB].active.ptr);
	while (r != NULL) {
		r->evaluations++;
		if (r->kif != NULL &&
		    (r->kif != kif && r->kif != kif->pfik_parent) == !r->ifnot)
			r = r->skip[PF_SKIP_IFP].ptr;
		else if (r->direction && r->direction != dir)
			r = r->skip[PF_SKIP_DIR].ptr;
		else if (r->af && r->af != af)
			r = r->skip[PF_SKIP_AF].ptr;
		else if (r->proto && r->proto != pd->proto)
			r = r->skip[PF_SKIP_PROTO].ptr;
		else if (PF_MISMATCHAW(&r->src.addr, pd->src, af, r->src.not))
			r = r->skip[PF_SKIP_SRC_ADDR].ptr;
		else if (r->src.port_op && !pf_match_port(r->src.port_op,
			    r->src.port[0], r->src.port[1], th->th_sport))
			r = r->skip[PF_SKIP_SRC_PORT].ptr;
		else if (PF_MISMATCHAW(&r->dst.addr, pd->dst, af, r->dst.not))
			r = r->skip[PF_SKIP_DST_ADDR].ptr;
		else if (r->dst.port_op && !pf_match_port(r->dst.port_op,
			    r->dst.port[0], r->dst.port[1], th->th_dport))
			r = r->skip[PF_SKIP_DST_PORT].ptr;
		else if (r->os_fingerprint != PF_OSFP_ANY && !pf_osfp_match(
			    pf_osfp_fingerprint(pd, m, off, th),
			    r->os_fingerprint))
			r = TAILQ_NEXT(r, entries);
		else {
			rm = r;
			break;
		}
	}

	if (rm == NULL)
		return (PF_PASS);
	else
		r->packets++;

	if (rm->rule_flag & PFRULE_REASSEMBLE_TCP)
		pd->flags |= PFDESC_TCP_NORM;

	flags = th->th_flags;
	if (flags & TH_SYN) {
		/* Illegal packet */
		if (flags & TH_RST)
			goto tcp_drop;

		if (flags & TH_FIN)
			flags &= ~TH_FIN;
	} else {
		/* Illegal packet */
		if (!(flags & (TH_ACK|TH_RST)))
			goto tcp_drop;
	}

	if (!(flags & TH_ACK)) {
		/* These flags are only valid if ACK is set */
		if ((flags & TH_FIN) || (flags & TH_PUSH) || (flags & TH_URG))
			goto tcp_drop;
	}

	/* Check for illegal header length */
	if (th->th_off < (sizeof(struct tcphdr) >> 2))
		goto tcp_drop;

	/* If flags changed, or reserved data set, then adjust */
	if (flags != th->th_flags || th->th_x2 != 0) {
		u_int16_t	ov, nv;

		ov = *(u_int16_t *)(&th->th_ack + 1);
		th->th_flags = flags;
		th->th_x2 = 0;
		nv = *(u_int16_t *)(&th->th_ack + 1);

		th->th_sum = pf_cksum_fixup(th->th_sum, ov, nv);
		rewrite = 1;
	}

	/* Remove urgent pointer, if TH_URG is not set */
	if (!(flags & TH_URG) && th->th_urp) {
		th->th_sum = pf_cksum_fixup(th->th_sum, th->th_urp, 0);
		th->th_urp = 0;
		rewrite = 1;
	}

	/* Process options */
	if (r->max_mss && pf_normalize_tcpopt(r, m, th, off))
		rewrite = 1;

	/* copy back packet headers if we sanitized */
	if (rewrite)
		m_copyback(m, off, sizeof(*th), (caddr_t)th);

	return (PF_PASS);

 tcp_drop:
	REASON_SET(&reason, PFRES_NORM);
	if (rm != NULL && r->log)
		PFLOG_PACKET(kif, h, m, AF_INET, dir, reason, r, NULL, NULL);
	return (PF_DROP);
}

int
pf_normalize_tcp_init(struct mbuf *m, int off, struct pf_pdesc *pd,
    struct tcphdr *th, struct pf_state_peer *src, struct pf_state_peer *dst)
{
	u_int8_t hdr[60];
	u_int8_t *opt;

	KASSERT((src->scrub == NULL), 
	    ("pf_normalize_tcp_init: src->scrub != NULL"));

	src->scrub = pool_get(&pf_state_scrub_pl, PR_NOWAIT);
	if (src->scrub == NULL)
		return (1);
	bzero(src->scrub, sizeof(*src->scrub));

	switch (pd->af) {
#ifdef INET
	case AF_INET: {
		struct ip *h = mtod(m, struct ip *);
		src->scrub->pfss_ttl = h->ip_ttl;
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *h = mtod(m, struct ip6_hdr *);
		src->scrub->pfss_ttl = h->ip6_hlim;
		break;
	}
#endif /* INET6 */
	}


	/*
	 * All normalizations below are only begun if we see the start of
	 * the connections.  They must all set an enabled bit in pfss_flags
	 */
	if ((th->th_flags & TH_SYN) == 0)
		return (0);


	if (th->th_off > (sizeof(struct tcphdr) >> 2) && src->scrub &&
	    pf_pull_hdr(m, off, hdr, th->th_off << 2, NULL, NULL, pd->af)) {
		/* Diddle with TCP options */
		int hlen;
		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					src->scrub->pfss_flags |=
					    PFSS_TIMESTAMP;
					src->scrub->pfss_ts_mod = arc4random();
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
	}

	return (0);
}

void
pf_normalize_tcp_cleanup(struct pf_state *state)
{
	if (state->src.scrub)
		pool_put(&pf_state_scrub_pl, state->src.scrub);
	if (state->dst.scrub)
		pool_put(&pf_state_scrub_pl, state->dst.scrub);

	/* Someday... flush the TCP segment reassembly descriptors. */
}

int
pf_normalize_tcp_stateful(struct mbuf *m, int off, struct pf_pdesc *pd,
    u_short *reason, struct tcphdr *th, struct pf_state_peer *src,
    struct pf_state_peer *dst, int *writeback)
{
	u_int8_t hdr[60];
	u_int8_t *opt;
	int copyback = 0;

	KASSERT((src->scrub || dst->scrub), 
	    ("pf_normalize_tcp_statefull: src->scrub && dst->scrub!"));

	/*
	 * Enforce the minimum TTL seen for this connection.  Negate a common
	 * technique to evade an intrusion detection system and confuse
	 * firewall state code.
	 */
	switch (pd->af) {
#ifdef INET
	case AF_INET: {
		if (src->scrub) {
			struct ip *h = mtod(m, struct ip *);
			if (h->ip_ttl > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip_ttl;
			h->ip_ttl = src->scrub->pfss_ttl;
		}
		break;
	}
#endif /* INET */
#ifdef INET6
	case AF_INET6: {
		if (src->scrub) {
			struct ip6_hdr *h = mtod(m, struct ip6_hdr *);
			if (h->ip6_hlim > src->scrub->pfss_ttl)
				src->scrub->pfss_ttl = h->ip6_hlim;
			h->ip6_hlim = src->scrub->pfss_ttl;
		}
		break;
	}
#endif /* INET6 */
	}

	if (th->th_off > (sizeof(struct tcphdr) >> 2) &&
	    ((src->scrub && (src->scrub->pfss_flags & PFSS_TIMESTAMP)) ||
	    (dst->scrub && (dst->scrub->pfss_flags & PFSS_TIMESTAMP))) &&
	    pf_pull_hdr(m, off, hdr, th->th_off << 2, NULL, NULL, pd->af)) {
		/* Diddle with TCP options */
		int hlen;
		opt = hdr + sizeof(struct tcphdr);
		hlen = (th->th_off << 2) - sizeof(struct tcphdr);
		while (hlen >= TCPOLEN_TIMESTAMP) {
			switch (*opt) {
			case TCPOPT_EOL:	/* FALLTHROUGH */
			case TCPOPT_NOP:
				opt++;
				hlen--;
				break;
			case TCPOPT_TIMESTAMP:
				/* Modulate the timestamps.  Can be used for
				 * NAT detection, OS uptime determination or
				 * reboot detection.
				 */
				if (opt[1] >= TCPOLEN_TIMESTAMP) {
					u_int32_t ts_value;
					if (src->scrub &&
					    (src->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						memcpy(&ts_value, &opt[2],
						    sizeof(u_int32_t));
						ts_value = htonl(ntohl(ts_value)
						    + src->scrub->pfss_ts_mod);
						pf_change_a(&opt[2],
						    &th->th_sum, ts_value, 0);
						copyback = 1;
					}

					/* Modulate TS reply iff valid (!0) */
					memcpy(&ts_value, &opt[6],
					    sizeof(u_int32_t));
					if (ts_value && dst->scrub &&
					    (dst->scrub->pfss_flags &
					    PFSS_TIMESTAMP)) {
						ts_value = htonl(ntohl(ts_value)
						    - dst->scrub->pfss_ts_mod);
						pf_change_a(&opt[6],
						    &th->th_sum, ts_value, 0);
						copyback = 1;
					}
				}
				/* FALLTHROUGH */
			default:
				hlen -= MAX(opt[1], 2);
				opt += MAX(opt[1], 2);
				break;
			}
		}
		if (copyback) {
			/* Copyback the options, caller copys back header */
			*writeback = 1;
			m_copyback(m, off + sizeof(struct tcphdr),
			    (th->th_off << 2) - sizeof(struct tcphdr), hdr +
			    sizeof(struct tcphdr));
		}
	}


	/* I have a dream....  TCP segment reassembly.... */
	return (0);
}

int
pf_normalize_tcpopt(struct pf_rule *r, struct mbuf *m, struct tcphdr *th,
    int off)
{
	u_int16_t	*mss;
	int		 thoff;
	int		 opt, cnt, optlen = 0;
	int		 rewrite = 0;
	u_char		*optp;

	thoff = th->th_off << 2;
	cnt = thoff - sizeof(struct tcphdr);
	optp = mtod(m, caddr_t) + off + sizeof(struct tcphdr);

	for (; cnt > 0; cnt -= optlen, optp += optlen) {
		opt = optp[0];
		if (opt == TCPOPT_EOL)
			break;
		if (opt == TCPOPT_NOP)
			optlen = 1;
		else {
			if (cnt < 2)
				break;
			optlen = optp[1];
			if (optlen < 2 || optlen > cnt)
				break;
		}
		switch (opt) {
		case TCPOPT_MAXSEG:
			mss = (u_int16_t *)(optp + 2);
			if ((ntohs(*mss)) > r->max_mss) {
				th->th_sum = pf_cksum_fixup(th->th_sum,
				    *mss, htons(r->max_mss));
				*mss = htons(r->max_mss);
				rewrite = 1;
			}
			break;
		default:
			break;
		}
	}

	return (rewrite);
}
