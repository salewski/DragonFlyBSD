/*	$FreeBSD: src/sys/netipsec/ipsec.c,v 1.2.2.1 2003/01/24 05:11:35 sam Exp $	*/
/*	$DragonFly: src/sys/netproto/ipsec/ipsec.c,v 1.9 2004/12/29 03:26:42 hsu Exp $	*/
/*	$KAME: ipsec.c,v 1.103 2001/05/24 07:14:18 sakane Exp $	*/

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

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/in_cksum.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif

#include <netproto/ipsec/ipsec.h>
#ifdef INET6
#include <netproto/ipsec/ipsec6.h>
#endif
#include <netproto/ipsec/ah_var.h>
#include <netproto/ipsec/esp_var.h>
#include <netproto/ipsec/ipcomp.h>		/*XXX*/
#include <netproto/ipsec/ipcomp_var.h>

#include <netproto/ipsec/key.h>
#include <netproto/ipsec/keydb.h>
#include <netproto/ipsec/key_debug.h>

#include <netproto/ipsec/xform.h>

#include <net/net_osdep.h>

#ifdef IPSEC_DEBUG
int ipsec_debug = 1;
#else
int ipsec_debug = 0;
#endif

/* NB: name changed so netstat doesn't use it */
struct newipsecstat newipsecstat;
int ip4_ah_offsetmask = 0;	/* maybe IP_DF? */
int ip4_ipsec_dfbit = 0;	/* DF bit on encap. 0: clear 1: set 2: copy */
int ip4_esp_trans_deflev = IPSEC_LEVEL_USE;
int ip4_esp_net_deflev = IPSEC_LEVEL_USE;
int ip4_ah_trans_deflev = IPSEC_LEVEL_USE;
int ip4_ah_net_deflev = IPSEC_LEVEL_USE;
struct secpolicy ip4_def_policy;
int ip4_ipsec_ecn = 0;		/* ECN ignore(-1)/forbidden(0)/allowed(1) */
int ip4_esp_randpad = -1;
/*
 * Crypto support requirements:
 *
 *  1	require hardware support
 * -1	require software support
 *  0	take anything
 */
int	crypto_support = 0;

SYSCTL_DECL(_net_inet_ipsec);

/* net.inet.ipsec */
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_POLICY,
	def_policy, CTLFLAG_RW,	&ip4_def_policy.policy,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_RW, &ip4_esp_trans_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_RW, &ip4_esp_net_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_RW, &ip4_ah_trans_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_RW, &ip4_ah_net_deflev,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_CLEARTOS,
	ah_cleartos, CTLFLAG_RW,	&ah_cleartos,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_AH_OFFSETMASK,
	ah_offsetmask, CTLFLAG_RW,	&ip4_ah_offsetmask,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DFBIT,
	dfbit, CTLFLAG_RW,	&ip4_ipsec_dfbit,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_ECN,
	ecn, CTLFLAG_RW,	&ip4_ipsec_ecn,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_DEBUG,
	debug, CTLFLAG_RW,	&ipsec_debug,	0, "");
SYSCTL_INT(_net_inet_ipsec, IPSECCTL_ESP_RANDPAD,
	esp_randpad, CTLFLAG_RW,	&ip4_esp_randpad,	0, "");
SYSCTL_INT(_net_inet_ipsec, OID_AUTO,
	crypto_support,	CTLFLAG_RW,	&crypto_support,0, "");
SYSCTL_STRUCT(_net_inet_ipsec, OID_AUTO,
	ipsecstats,	CTLFLAG_RD,	&newipsecstat,	newipsecstat, "");

#ifdef INET6
int ip6_esp_trans_deflev = IPSEC_LEVEL_USE;
int ip6_esp_net_deflev = IPSEC_LEVEL_USE;
int ip6_ah_trans_deflev = IPSEC_LEVEL_USE;
int ip6_ah_net_deflev = IPSEC_LEVEL_USE;
int ip6_ipsec_ecn = 0;		/* ECN ignore(-1)/forbidden(0)/allowed(1) */
int ip6_esp_randpad = -1;

SYSCTL_DECL(_net_inet6_ipsec6);

/* net.inet6.ipsec6 */
#ifdef COMPAT_KAME
SYSCTL_OID(_net_inet6_ipsec6, IPSECCTL_STATS, stats, CTLFLAG_RD,
	0,0, compat_ipsecstats_sysctl, "S", "");
#endif /* COMPAT_KAME */
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_POLICY,
	def_policy, CTLFLAG_RW,	&ip4_def_policy.policy,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_TRANSLEV, esp_trans_deflev,
	CTLFLAG_RW, &ip6_esp_trans_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_ESP_NETLEV, esp_net_deflev,
	CTLFLAG_RW, &ip6_esp_net_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_TRANSLEV, ah_trans_deflev,
	CTLFLAG_RW, &ip6_ah_trans_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEF_AH_NETLEV, ah_net_deflev,
	CTLFLAG_RW, &ip6_ah_net_deflev,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_ECN,
	ecn, CTLFLAG_RW,	&ip6_ipsec_ecn,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_DEBUG,
	debug, CTLFLAG_RW,	&ipsec_debug,	0, "");
SYSCTL_INT(_net_inet6_ipsec6, IPSECCTL_ESP_RANDPAD,
	esp_randpad, CTLFLAG_RW,	&ip6_esp_randpad,	0, "");
#endif /* INET6 */

static int ipsec4_setspidx_inpcb (struct mbuf *, struct inpcb *pcb);
#ifdef INET6
static int ipsec6_setspidx_in6pcb (struct mbuf *, struct in6pcb *pcb);
#endif
static int ipsec_setspidx (struct mbuf *, struct secpolicyindex *, int);
static void ipsec4_get_ulp (struct mbuf *m, struct secpolicyindex *, int);
static int ipsec4_setspidx_ipaddr (struct mbuf *, struct secpolicyindex *);
#ifdef INET6
static void ipsec6_get_ulp (struct mbuf *m, struct secpolicyindex *, int);
static int ipsec6_setspidx_ipaddr (struct mbuf *, struct secpolicyindex *);
#endif
static void ipsec_delpcbpolicy (struct inpcbpolicy *);
static struct secpolicy *ipsec_deepcopy_policy (struct secpolicy *src);
static int ipsec_set_policy (struct secpolicy **pcb_sp,
	int optname, caddr_t request, size_t len, int priv);
static int ipsec_get_policy (struct secpolicy *pcb_sp, struct mbuf **mp);
static void vshiftl (unsigned char *, int, int);
static size_t ipsec_hdrsiz (struct secpolicy *);

/*
 * Return a held reference to the default SP.
 */
static struct secpolicy *
key_allocsp_default(const char* where, int tag)
{
	struct secpolicy *sp;

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP key_allocsp_default from %s:%u\n", where, tag));

	sp = &ip4_def_policy;
	if (sp->policy != IPSEC_POLICY_DISCARD &&
	    sp->policy != IPSEC_POLICY_NONE) {
		ipseclog((LOG_INFO, "fixed system default policy: %d->%d\n",
		    sp->policy, IPSEC_POLICY_NONE));
		sp->policy = IPSEC_POLICY_NONE;
	}
	sp->refcnt++;

	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP key_allocsp_default returns SP:%p (%u)\n",
			sp, sp->refcnt));
	return sp;
}
#define	KEY_ALLOCSP_DEFAULT() \
	key_allocsp_default(__FILE__, __LINE__)

/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 *
 * NOTE: IPv6 mapped adddress concern is implemented here.
 */
struct secpolicy *
ipsec_getpolicy(struct tdb_ident *tdbi, u_int dir)
{
	struct secpolicy *sp;

	KASSERT(tdbi != NULL, ("ipsec_getpolicy: null tdbi"));
	KASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("ipsec_getpolicy: invalid direction %u", dir));

	sp = KEY_ALLOCSP2(tdbi->spi, &tdbi->dst, tdbi->proto, dir);
	if (sp == NULL)			/*XXX????*/
		sp = KEY_ALLOCSP_DEFAULT();
	KASSERT(sp != NULL, ("ipsec_getpolicy: null SP"));
	return sp;
}

/*
 * For OUTBOUND packet having a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 *	others:	a pointer to SP
 *
 * NOTE: IPv6 mapped adddress concern is implemented here.
 */
struct secpolicy *
ipsec_getpolicybysock(m, dir, inp, error)
	struct mbuf *m;
	u_int dir;
	struct inpcb *inp;
	int *error;
{
	struct inpcbpolicy *pcbsp = NULL;
	struct secpolicy *currsp = NULL;	/* policy on socket */
	struct secpolicy *sp;
	int af;

	KASSERT(m != NULL, ("ipsec_getpolicybysock: null mbuf"));
	KASSERT(inp != NULL, ("ipsec_getpolicybysock: null inpcb"));
	KASSERT(error != NULL, ("ipsec_getpolicybysock: null error"));
	KASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("ipsec_getpolicybysock: invalid direction %u", dir));

	af = inp->inp_socket->so_proto->pr_domain->dom_family;
	KASSERT(af == AF_INET || af == AF_INET6,
		("ipsec_getpolicybysock: unexpected protocol family %u", af));

	switch (af) {
	case AF_INET:
		/* set spidx in pcb */
		*error = ipsec4_setspidx_inpcb(m, inp);
		pcbsp = inp->inp_sp;
		break;
#ifdef INET6
	case AF_INET6:
		/* set spidx in pcb */
		*error = ipsec6_setspidx_in6pcb(m, inp);
		pcbsp = inp->in6p_sp;
		break;
#endif
	default:
		*error = EPFNOSUPPORT;
		break;
	}
	if (*error)
		return NULL;

	KASSERT(pcbsp != NULL, ("ipsec_getpolicybysock: null pcbsp"));
	switch (dir) {
	case IPSEC_DIR_INBOUND:
		currsp = pcbsp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		currsp = pcbsp->sp_out;
		break;
	}
	KASSERT(currsp != NULL, ("ipsec_getpolicybysock: null currsp"));

	if (pcbsp->priv) {			/* when privilieged socket */
		switch (currsp->policy) {
		case IPSEC_POLICY_BYPASS:
		case IPSEC_POLICY_IPSEC:
			currsp->refcnt++;
			sp = currsp;
			break;

		case IPSEC_POLICY_ENTRUST:
			/* look for a policy in SPD */
			sp = KEY_ALLOCSP(&currsp->spidx, dir);
			if (sp == NULL)		/* no SP found */
				sp = KEY_ALLOCSP_DEFAULT();
			break;

		default:
			ipseclog((LOG_ERR, "ipsec_getpolicybysock: "
			      "Invalid policy for PCB %d\n", currsp->policy));
			*error = EINVAL;
			return NULL;
		}
	} else {				/* unpriv, SPD has policy */
		sp = KEY_ALLOCSP(&currsp->spidx, dir);
		if (sp == NULL) {		/* no SP found */
			switch (currsp->policy) {
			case IPSEC_POLICY_BYPASS:
				ipseclog((LOG_ERR, "ipsec_getpolicybysock: "
				       "Illegal policy for non-priviliged defined %d\n",
					currsp->policy));
				*error = EINVAL;
				return NULL;

			case IPSEC_POLICY_ENTRUST:
				sp = KEY_ALLOCSP_DEFAULT();
				break;

			case IPSEC_POLICY_IPSEC:
				currsp->refcnt++;
				sp = currsp;
				break;

			default:
				ipseclog((LOG_ERR, "ipsec_getpolicybysock: "
				   "Invalid policy for PCB %d\n", currsp->policy));
				*error = EINVAL;
				return NULL;
			}
		}
	}
	KASSERT(sp != NULL,
		("ipsec_getpolicybysock: null SP (priv %u policy %u",
		 pcbsp->priv, currsp->policy));
	KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
		printf("DP ipsec_getpolicybysock (priv %u policy %u) allocates "
		       "SP:%p (refcnt %u)\n", pcbsp->priv, currsp->policy,
		       sp, sp->refcnt));
	return sp;
}

/*
 * For FORWADING packet or OUTBOUND without a socket. Searching SPD for packet,
 * and return a pointer to SP.
 * OUT:	positive: a pointer to the entry for security policy leaf matched.
 *	NULL:	no apropreate SP found, the following value is set to error.
 *		0	: bypass
 *		EACCES	: discard packet.
 *		ENOENT	: ipsec_acquire() in progress, maybe.
 *		others	: error occured.
 */
struct secpolicy *
ipsec_getpolicybyaddr(m, dir, flag, error)
	struct mbuf *m;
	u_int dir;
	int flag;
	int *error;
{
	struct secpolicyindex spidx;
	struct secpolicy *sp;

	KASSERT(m != NULL, ("ipsec_getpolicybyaddr: null mbuf"));
	KASSERT(error != NULL, ("ipsec_getpolicybyaddr: null error"));
	KASSERT(dir == IPSEC_DIR_INBOUND || dir == IPSEC_DIR_OUTBOUND,
		("ipsec4_getpolicybaddr: invalid direction %u", dir));

	sp = NULL;
	if (key_havesp(dir)) {
		/* Make an index to look for a policy. */
		*error = ipsec_setspidx(m, &spidx,
					(flag & IP_FORWARDING) ? 0 : 1);
		if (*error != 0) {
			DPRINTF(("ipsec_getpolicybyaddr: setpidx failed,"
				" dir %u flag %u\n", dir, flag));
			bzero(&spidx, sizeof (spidx));
			return NULL;
		}
		spidx.dir = dir;

		sp = KEY_ALLOCSP(&spidx, dir);
	}
	if (sp == NULL)			/* no SP found, use system default */
		sp = KEY_ALLOCSP_DEFAULT();
	KASSERT(sp != NULL, ("ipsec_getpolicybyaddr: null SP"));
	return sp;
}

struct secpolicy *
ipsec4_checkpolicy(m, dir, flag, error, inp)
	struct mbuf *m;
	u_int dir, flag;
	int *error;
	struct inpcb *inp;
{
	struct secpolicy *sp;

	*error = 0;
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, dir, flag, error);
	else
		sp = ipsec_getpolicybysock(m, dir, inp, error);
	if (sp == NULL) {
		KASSERT(*error != 0,
			("ipsec4_checkpolicy: getpolicy failed w/o error"));
		newipsecstat.ips_out_inval++;
		return NULL;
	}
	KASSERT(*error == 0,
		("ipsec4_checkpolicy: sp w/ error set to %u", *error));
	switch (sp->policy) {
	case IPSEC_POLICY_ENTRUST:
	default:
		printf("ipsec4_checkpolicy: invalid policy %u\n", sp->policy);
		/* fall thru... */
	case IPSEC_POLICY_DISCARD:
		newipsecstat.ips_out_polvio++;
		*error = -EINVAL;	/* packet is discarded by caller */
		break;
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		KEY_FREESP(&sp);
		sp = NULL;		/* NB: force NULL result */
		break;
	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL)	/* acquire an SA */
			*error = key_spdacquire(sp);
		break;
	}
	if (*error != 0) {
		KEY_FREESP(&sp);
		sp = NULL;
	}
	return sp;
}

static int
ipsec4_setspidx_inpcb(m, pcb)
	struct mbuf *m;
	struct inpcb *pcb;
{
	int error;

	KASSERT(pcb != NULL, ("ipsec4_setspidx_inpcb: null pcb"));
	KASSERT(pcb->inp_sp != NULL, ("ipsec4_setspidx_inpcb: null inp_sp"));
	KASSERT(pcb->inp_sp->sp_out != NULL && pcb->inp_sp->sp_in != NULL,
		("ipsec4_setspidx_inpcb: null sp_in || sp_out"));

	error = ipsec_setspidx(m, &pcb->inp_sp->sp_in->spidx, 1);
	if (error == 0) {
		pcb->inp_sp->sp_in->spidx.dir = IPSEC_DIR_INBOUND;
		pcb->inp_sp->sp_out->spidx = pcb->inp_sp->sp_in->spidx;
		pcb->inp_sp->sp_out->spidx.dir = IPSEC_DIR_OUTBOUND;
	} else {
		bzero(&pcb->inp_sp->sp_in->spidx,
			sizeof (pcb->inp_sp->sp_in->spidx));
		bzero(&pcb->inp_sp->sp_out->spidx,
			sizeof (pcb->inp_sp->sp_in->spidx));
	}
	return error;
}

#ifdef INET6
static int
ipsec6_setspidx_in6pcb(m, pcb)
	struct mbuf *m;
	struct in6pcb *pcb;
{
	struct secpolicyindex *spidx;
	int error;

	KASSERT(pcb != NULL, ("ipsec6_setspidx_in6pcb: null pcb"));
	KASSERT(pcb->in6p_sp != NULL, ("ipsec6_setspidx_in6pcb: null inp_sp"));
	KASSERT(pcb->in6p_sp->sp_out != NULL && pcb->in6p_sp->sp_in != NULL,
		("ipsec6_setspidx_in6pcb: null sp_in || sp_out"));

	bzero(&pcb->in6p_sp->sp_in->spidx, sizeof(*spidx));
	bzero(&pcb->in6p_sp->sp_out->spidx, sizeof(*spidx));

	spidx = &pcb->in6p_sp->sp_in->spidx;
	error = ipsec_setspidx(m, spidx, 1);
	if (error)
		goto bad;
	spidx->dir = IPSEC_DIR_INBOUND;

	spidx = &pcb->in6p_sp->sp_out->spidx;
	error = ipsec_setspidx(m, spidx, 1);
	if (error)
		goto bad;
	spidx->dir = IPSEC_DIR_OUTBOUND;

	return 0;

bad:
	bzero(&pcb->in6p_sp->sp_in->spidx, sizeof(*spidx));
	bzero(&pcb->in6p_sp->sp_out->spidx, sizeof(*spidx));
	return error;
}
#endif

/*
 * configure security policy index (src/dst/proto/sport/dport)
 * by looking at the content of mbuf.
 * the caller is responsible for error recovery (like clearing up spidx).
 */
static int
ipsec_setspidx(m, spidx, needport)
	struct mbuf *m;
	struct secpolicyindex *spidx;
	int needport;
{
	struct ip *ip = NULL;
	struct ip ipbuf;
	u_int v;
	struct mbuf *n;
	int len;
	int error;

	KASSERT(m != NULL, ("ipsec_setspidx: null mbuf"));

	/*
	 * validate m->m_pkthdr.len.  we see incorrect length if we
	 * mistakenly call this function with inconsistent mbuf chain
	 * (like 4.4BSD tcp/udp processing).  XXX should we panic here?
	 */
	len = 0;
	for (n = m; n; n = n->m_next)
		len += n->m_len;
	if (m->m_pkthdr.len != len) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			       "total of m_len(%d) != pkthdr.len(%d), "
			       "ignored.\n",
				len, m->m_pkthdr.len));
		return EINVAL;
	}

	if (m->m_pkthdr.len < sizeof(struct ip)) {
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			    "pkthdr.len(%d) < sizeof(struct ip), ignored.\n",
			    m->m_pkthdr.len));
		return EINVAL;
	}

	if (m->m_len >= sizeof(*ip))
		ip = mtod(m, struct ip *);
	else {
		m_copydata(m, 0, sizeof(ipbuf), (caddr_t)&ipbuf);
		ip = &ipbuf;
	}
#ifdef _IP_VHL
	v = _IP_VHL_V(ip->ip_vhl);
#else
	v = ip->ip_v;
#endif
	switch (v) {
	case 4:
		error = ipsec4_setspidx_ipaddr(m, spidx);
		if (error)
			return error;
		ipsec4_get_ulp(m, spidx, needport);
		return 0;
#ifdef INET6
	case 6:
		if (m->m_pkthdr.len < sizeof(struct ip6_hdr)) {
			KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				printf("ipsec_setspidx: "
				    "pkthdr.len(%d) < sizeof(struct ip6_hdr), "
				    "ignored.\n", m->m_pkthdr.len));
			return EINVAL;
		}
		error = ipsec6_setspidx_ipaddr(m, spidx);
		if (error)
			return error;
		ipsec6_get_ulp(m, spidx, needport);
		return 0;
#endif
	default:
		KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
			printf("ipsec_setspidx: "
			    "unknown IP version %u, ignored.\n", v));
		return EINVAL;
	}
}

static void
ipsec4_get_ulp(struct mbuf *m, struct secpolicyindex *spidx, int needport)
{
	u_int8_t nxt;
	int off;

	/* sanity check */
	KASSERT(m != NULL, ("ipsec4_get_ulp: null mbuf"));
	KASSERT(m->m_pkthdr.len >= sizeof(struct ip),
		("ipsec4_get_ulp: packet too short"));

	/* NB: ip_input() flips it into host endian XXX need more checking */
	if (m->m_len < sizeof (struct ip)) {
		struct ip *ip = mtod(m, struct ip *);
		if (ip->ip_off & (IP_MF | IP_OFFMASK))
			goto done;
#ifdef _IP_VHL
		off = _IP_VHL_HL(ip->ip_vhl) << 2;
#else
		off = ip->ip_hl << 2;
#endif
		nxt = ip->ip_p;
	} else {
		struct ip ih;

		m_copydata(m, 0, sizeof (struct ip), (caddr_t) &ih);
		if (ih.ip_off & (IP_MF | IP_OFFMASK))
			goto done;
#ifdef _IP_VHL
		off = _IP_VHL_HL(ih.ip_vhl) << 2;
#else
		off = ih.ip_hl << 2;
#endif
		nxt = ih.ip_p;
	}

	while (off < m->m_pkthdr.len) {
		struct ip6_ext ip6e;
		struct tcphdr th;
		struct udphdr uh;

		switch (nxt) {
		case IPPROTO_TCP:
			spidx->ul_proto = nxt;
			if (!needport)
				goto done_proto;
			if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
				goto done;
			m_copydata(m, off, sizeof (th), (caddr_t) &th);
			spidx->src.sin.sin_port = th.th_sport;
			spidx->dst.sin.sin_port = th.th_dport;
			return;
		case IPPROTO_UDP:
			spidx->ul_proto = nxt;
			if (!needport)
				goto done_proto;
			if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
				goto done;
			m_copydata(m, off, sizeof (uh), (caddr_t) &uh);
			spidx->src.sin.sin_port = uh.uh_sport;
			spidx->dst.sin.sin_port = uh.uh_dport;
			return;
		case IPPROTO_AH:
			if (m->m_pkthdr.len > off + sizeof(ip6e))
				goto done;
			/* XXX sigh, this works but is totally bogus */
			m_copydata(m, off, sizeof(ip6e), (caddr_t) &ip6e);
			off += (ip6e.ip6e_len + 2) << 2;
			nxt = ip6e.ip6e_nxt;
			break;
		case IPPROTO_ICMP:
		default:
			/* XXX intermediate headers??? */
			spidx->ul_proto = nxt;
			goto done_proto;
		}
	}
done:
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
done_proto:
	spidx->src.sin.sin_port = IPSEC_PORT_ANY;
	spidx->dst.sin.sin_port = IPSEC_PORT_ANY;
}

/* assumes that m is sane */
static int
ipsec4_setspidx_ipaddr(struct mbuf *m, struct secpolicyindex *spidx)
{
	static const struct sockaddr_in template = {
		sizeof (struct sockaddr_in),
		AF_INET,
		0, { 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }
	};

	spidx->src.sin = template;
	spidx->dst.sin = template;

	if (m->m_len < sizeof (struct ip)) {
		m_copydata(m, offsetof(struct ip, ip_src),
			   sizeof (struct  in_addr),
			   (caddr_t) &spidx->src.sin.sin_addr);
		m_copydata(m, offsetof(struct ip, ip_dst),
			   sizeof (struct  in_addr),
			   (caddr_t) &spidx->dst.sin.sin_addr);
	} else {
		struct ip *ip = mtod(m, struct ip *);
		spidx->src.sin.sin_addr = ip->ip_src;
		spidx->dst.sin.sin_addr = ip->ip_dst;
	}

	spidx->prefs = sizeof(struct in_addr) << 3;
	spidx->prefd = sizeof(struct in_addr) << 3;

	return 0;
}

#ifdef INET6
static void
ipsec6_get_ulp(m, spidx, needport)
	struct mbuf *m;
	struct secpolicyindex *spidx;
	int needport;
{
	int off, nxt;
	struct tcphdr th;
	struct udphdr uh;

	/* sanity check */
	if (m == NULL)
		panic("ipsec6_get_ulp: NULL pointer was passed.\n");

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec6_get_ulp:\n"); kdebug_mbuf(m));

	/* set default */
	spidx->ul_proto = IPSEC_ULPROTO_ANY;
	((struct sockaddr_in6 *)&spidx->src)->sin6_port = IPSEC_PORT_ANY;
	((struct sockaddr_in6 *)&spidx->dst)->sin6_port = IPSEC_PORT_ANY;

	nxt = -1;
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off < 0 || m->m_pkthdr.len < off)
		return;

	switch (nxt) {
	case IPPROTO_TCP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct tcphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(th), (caddr_t)&th);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = th.th_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = th.th_dport;
		break;
	case IPPROTO_UDP:
		spidx->ul_proto = nxt;
		if (!needport)
			break;
		if (off + sizeof(struct udphdr) > m->m_pkthdr.len)
			break;
		m_copydata(m, off, sizeof(uh), (caddr_t)&uh);
		((struct sockaddr_in6 *)&spidx->src)->sin6_port = uh.uh_sport;
		((struct sockaddr_in6 *)&spidx->dst)->sin6_port = uh.uh_dport;
		break;
	case IPPROTO_ICMPV6:
	default:
		/* XXX intermediate headers??? */
		spidx->ul_proto = nxt;
		break;
	}
}

/* assumes that m is sane */
static int
ipsec6_setspidx_ipaddr(m, spidx)
	struct mbuf *m;
	struct secpolicyindex *spidx;
{
	struct ip6_hdr *ip6 = NULL;
	struct ip6_hdr ip6buf;
	struct sockaddr_in6 *sin6;

	if (m->m_len >= sizeof(*ip6))
		ip6 = mtod(m, struct ip6_hdr *);
	else {
		m_copydata(m, 0, sizeof(ip6buf), (caddr_t)&ip6buf);
		ip6 = &ip6buf;
	}

	sin6 = (struct sockaddr_in6 *)&spidx->src;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_src, &sin6->sin6_addr, sizeof(ip6->ip6_src));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
		sin6->sin6_scope_id = ntohs(ip6->ip6_src.s6_addr16[1]);
	}
	spidx->prefs = sizeof(struct in6_addr) << 3;

	sin6 = (struct sockaddr_in6 *)&spidx->dst;
	bzero(sin6, sizeof(*sin6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_dst, &sin6->sin6_addr, sizeof(ip6->ip6_dst));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
		sin6->sin6_addr.s6_addr16[1] = 0;
		sin6->sin6_scope_id = ntohs(ip6->ip6_dst.s6_addr16[1]);
	}
	spidx->prefd = sizeof(struct in6_addr) << 3;

	return 0;
}
#endif

static void
ipsec_delpcbpolicy(p)
	struct inpcbpolicy *p;
{
	free(p, M_SECA);
}

/* initialize policy in PCB */
int
ipsec_init_policy(so, pcb_sp)
	struct socket *so;
	struct inpcbpolicy **pcb_sp;
{
	struct inpcbpolicy *new;

	/* sanity check. */
	if (so == NULL || pcb_sp == NULL)
		panic("ipsec_init_policy: NULL pointer was passed.\n");

	new = malloc(sizeof(struct inpcbpolicy),
			M_SECA, M_INTWAIT | M_ZERO | M_NULLOK);
	if (new == NULL) {
		ipseclog((LOG_DEBUG, "ipsec_init_policy: No more memory.\n"));
		return ENOBUFS;
	}

	if (so->so_cred != 0 && so->so_cred->cr_uid == 0)
		new->priv = 1;
	else
		new->priv = 0;

	if ((new->sp_in = KEY_NEWSP()) == NULL) {
		ipsec_delpcbpolicy(new);
		return ENOBUFS;
	}
	new->sp_in->state = IPSEC_SPSTATE_ALIVE;
	new->sp_in->policy = IPSEC_POLICY_ENTRUST;

	if ((new->sp_out = KEY_NEWSP()) == NULL) {
		KEY_FREESP(&new->sp_in);
		ipsec_delpcbpolicy(new);
		return ENOBUFS;
	}
	new->sp_out->state = IPSEC_SPSTATE_ALIVE;
	new->sp_out->policy = IPSEC_POLICY_ENTRUST;

	*pcb_sp = new;

	return 0;
}

/* copy old ipsec policy into new */
int
ipsec_copy_policy(old, new)
	struct inpcbpolicy *old, *new;
{
	struct secpolicy *sp;

	sp = ipsec_deepcopy_policy(old->sp_in);
	if (sp) {
		KEY_FREESP(&new->sp_in);
		new->sp_in = sp;
	} else
		return ENOBUFS;

	sp = ipsec_deepcopy_policy(old->sp_out);
	if (sp) {
		KEY_FREESP(&new->sp_out);
		new->sp_out = sp;
	} else
		return ENOBUFS;

	new->priv = old->priv;

	return 0;
}

/* deep-copy a policy in PCB */
static struct secpolicy *
ipsec_deepcopy_policy(src)
	struct secpolicy *src;
{
	struct ipsecrequest *newchain = NULL;
	struct ipsecrequest *p;
	struct ipsecrequest **q;
	struct ipsecrequest *r;
	struct secpolicy *dst;

	if (src == NULL)
		return NULL;
	dst = KEY_NEWSP();
	if (dst == NULL)
		return NULL;

	/*
	 * deep-copy IPsec request chain.  This is required since struct
	 * ipsecrequest is not reference counted.
	 */
	q = &newchain;
	for (p = src->req; p; p = p->next) {
		*q = malloc(sizeof(struct ipsecrequest),
			M_SECA, M_INTWAIT | M_ZERO | M_NULLOK);
		if (*q == NULL)
			goto fail;
		(*q)->next = NULL;

		(*q)->saidx.proto = p->saidx.proto;
		(*q)->saidx.mode = p->saidx.mode;
		(*q)->level = p->level;
		(*q)->saidx.reqid = p->saidx.reqid;

		bcopy(&p->saidx.src, &(*q)->saidx.src, sizeof((*q)->saidx.src));
		bcopy(&p->saidx.dst, &(*q)->saidx.dst, sizeof((*q)->saidx.dst));

		(*q)->sav = NULL;
		(*q)->sp = dst;

		q = &((*q)->next);
	}

	dst->req = newchain;
	dst->state = src->state;
	dst->policy = src->policy;
	/* do not touch the refcnt fields */

	return dst;

fail:
	for (p = newchain; p; p = r) {
		r = p->next;
		free(p, M_SECA);
		p = NULL;
	}
	return NULL;
}

/* set policy and ipsec request if present. */
static int
ipsec_set_policy(pcb_sp, optname, request, len, priv)
	struct secpolicy **pcb_sp;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *newsp = NULL;
	int error;

	/* sanity check. */
	if (pcb_sp == NULL || *pcb_sp == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_set_policy: passed policy\n");
		kdebug_sadb_x_policy((struct sadb_ext *)xpl));

	/* check policy type */
	/* ipsec_set_policy() accepts IPSEC, ENTRUST and BYPASS. */
	if (xpl->sadb_x_policy_type == IPSEC_POLICY_DISCARD
	 || xpl->sadb_x_policy_type == IPSEC_POLICY_NONE)
		return EINVAL;

	/* check privileged socket */
	if (priv == 0 && xpl->sadb_x_policy_type == IPSEC_POLICY_BYPASS)
		return EACCES;

	/* allocation new SP entry */
	if ((newsp = key_msg2sp(xpl, len, &error)) == NULL)
		return error;

	newsp->state = IPSEC_SPSTATE_ALIVE;

	/* clear old SP and set new SP */
	KEY_FREESP(pcb_sp);
	*pcb_sp = newsp;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_set_policy: new policy\n");
		kdebug_secpolicy(newsp));

	return 0;
}

static int
ipsec_get_policy(pcb_sp, mp)
	struct secpolicy *pcb_sp;
	struct mbuf **mp;
{

	/* sanity check. */
	if (pcb_sp == NULL || mp == NULL)
		return EINVAL;

	*mp = key_sp2msg(pcb_sp);
	if (!*mp) {
		ipseclog((LOG_DEBUG, "ipsec_get_policy: No more memory.\n"));
		return ENOBUFS;
	}

	(*mp)->m_type = MT_DATA;
	KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
		printf("ipsec_get_policy:\n");
		kdebug_mbuf(*mp));

	return 0;
}

int
ipsec4_set_policy(inp, optname, request, len, priv)
	struct inpcb *inp;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy **pcb_sp;

	/* sanity check. */
	if (inp == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = &inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = &inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec4_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_set_policy(pcb_sp, optname, request, len, priv);
}

int
ipsec4_get_policy(inp, request, len, mp)
	struct inpcb *inp;
	caddr_t request;
	size_t len;
	struct mbuf **mp;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *pcb_sp;

	/* sanity check. */
	if (inp == NULL || request == NULL || mp == NULL)
		return EINVAL;
	KASSERT(inp->inp_sp != NULL, ("ipsec4_get_policy: null inp_sp"));
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = inp->inp_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = inp->inp_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec4_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_get_policy(pcb_sp, mp);
}

/* delete policy in PCB */
int
ipsec4_delete_pcbpolicy(inp)
	struct inpcb *inp;
{
	KASSERT(inp != NULL, ("ipsec4_delete_pcbpolicy: null inp"));

	if (inp->inp_sp == NULL)
		return 0;

	if (inp->inp_sp->sp_in != NULL)
		KEY_FREESP(&inp->inp_sp->sp_in);

	if (inp->inp_sp->sp_out != NULL)
		KEY_FREESP(&inp->inp_sp->sp_out);

	ipsec_delpcbpolicy(inp->inp_sp);
	inp->inp_sp = NULL;

	return 0;
}

#ifdef INET6
int
ipsec6_set_policy(in6p, optname, request, len, priv)
	struct in6pcb *in6p;
	int optname;
	caddr_t request;
	size_t len;
	int priv;
{
	struct sadb_x_policy *xpl;
	struct secpolicy **pcb_sp;

	/* sanity check. */
	if (in6p == NULL || request == NULL)
		return EINVAL;
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = &in6p->in6p_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = &in6p->in6p_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec6_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_set_policy(pcb_sp, optname, request, len, priv);
}

int
ipsec6_get_policy(in6p, request, len, mp)
	struct in6pcb *in6p;
	caddr_t request;
	size_t len;
	struct mbuf **mp;
{
	struct sadb_x_policy *xpl;
	struct secpolicy *pcb_sp;

	/* sanity check. */
	if (in6p == NULL || request == NULL || mp == NULL)
		return EINVAL;
	KASSERT(in6p->in6p_sp != NULL, ("ipsec6_get_policy: null in6p_sp"));
	if (len < sizeof(*xpl))
		return EINVAL;
	xpl = (struct sadb_x_policy *)request;

	/* select direction */
	switch (xpl->sadb_x_policy_dir) {
	case IPSEC_DIR_INBOUND:
		pcb_sp = in6p->in6p_sp->sp_in;
		break;
	case IPSEC_DIR_OUTBOUND:
		pcb_sp = in6p->in6p_sp->sp_out;
		break;
	default:
		ipseclog((LOG_ERR, "ipsec6_set_policy: invalid direction=%u\n",
			xpl->sadb_x_policy_dir));
		return EINVAL;
	}

	return ipsec_get_policy(pcb_sp, mp);
}

int
ipsec6_delete_pcbpolicy(in6p)
	struct in6pcb *in6p;
{
	KASSERT(in6p != NULL, ("ipsec6_delete_pcbpolicy: null in6p"));

	if (in6p->in6p_sp == NULL)
		return 0;

	if (in6p->in6p_sp->sp_in != NULL)
		KEY_FREESP(&in6p->in6p_sp->sp_in);

	if (in6p->in6p_sp->sp_out != NULL)
		KEY_FREESP(&in6p->in6p_sp->sp_out);

	ipsec_delpcbpolicy(in6p->in6p_sp);
	in6p->in6p_sp = NULL;

	return 0;
}
#endif

/*
 * return current level.
 * Either IPSEC_LEVEL_USE or IPSEC_LEVEL_REQUIRE are always returned.
 */
u_int
ipsec_get_reqlevel(isr)
	struct ipsecrequest *isr;
{
	u_int level = 0;
	u_int esp_trans_deflev, esp_net_deflev;
	u_int ah_trans_deflev, ah_net_deflev;

	KASSERT(isr != NULL && isr->sp != NULL,
		("ipsec_get_reqlevel: null argument"));
	KASSERT(isr->sp->spidx.src.sa.sa_family == isr->sp->spidx.dst.sa.sa_family,
		("ipsec_get_reqlevel: af family mismatch, src %u, dst %u",
		 isr->sp->spidx.src.sa.sa_family,
		 isr->sp->spidx.dst.sa.sa_family));

/* XXX note that we have ipseclog() expanded here - code sync issue */
#define IPSEC_CHECK_DEFAULT(lev) \
	(((lev) != IPSEC_LEVEL_USE && (lev) != IPSEC_LEVEL_REQUIRE	      \
			&& (lev) != IPSEC_LEVEL_UNIQUE)			      \
		? (ipsec_debug						      \
			? log(LOG_INFO, "fixed system default level " #lev ":%d->%d\n",\
				(lev), IPSEC_LEVEL_REQUIRE)		      \
			: 0),						      \
			(lev) = IPSEC_LEVEL_REQUIRE,			      \
			(lev)						      \
		: (lev))

	/* set default level */
	switch (((struct sockaddr *)&isr->sp->spidx.src)->sa_family) {
#ifdef INET
	case AF_INET:
		esp_trans_deflev = IPSEC_CHECK_DEFAULT(ip4_esp_trans_deflev);
		esp_net_deflev = IPSEC_CHECK_DEFAULT(ip4_esp_net_deflev);
		ah_trans_deflev = IPSEC_CHECK_DEFAULT(ip4_ah_trans_deflev);
		ah_net_deflev = IPSEC_CHECK_DEFAULT(ip4_ah_net_deflev);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		esp_trans_deflev = IPSEC_CHECK_DEFAULT(ip6_esp_trans_deflev);
		esp_net_deflev = IPSEC_CHECK_DEFAULT(ip6_esp_net_deflev);
		ah_trans_deflev = IPSEC_CHECK_DEFAULT(ip6_ah_trans_deflev);
		ah_net_deflev = IPSEC_CHECK_DEFAULT(ip6_ah_net_deflev);
		break;
#endif /* INET6 */
	default:
		panic("key_get_reqlevel: unknown af %u",
			isr->sp->spidx.src.sa.sa_family);
	}

#undef IPSEC_CHECK_DEFAULT

	/* set level */
	switch (isr->level) {
	case IPSEC_LEVEL_DEFAULT:
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = esp_net_deflev;
			else
				level = esp_trans_deflev;
			break;
		case IPPROTO_AH:
			if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
				level = ah_net_deflev;
			else
				level = ah_trans_deflev;
		case IPPROTO_IPCOMP:
			/*
			 * we don't really care, as IPcomp document says that
			 * we shouldn't compress small packets
			 */
			level = IPSEC_LEVEL_USE;
			break;
		default:
			panic("ipsec_get_reqlevel: "
				"Illegal protocol defined %u\n",
				isr->saidx.proto);
		}
		break;

	case IPSEC_LEVEL_USE:
	case IPSEC_LEVEL_REQUIRE:
		level = isr->level;
		break;
	case IPSEC_LEVEL_UNIQUE:
		level = IPSEC_LEVEL_REQUIRE;
		break;

	default:
		panic("ipsec_get_reqlevel: Illegal IPsec level %u\n",
			isr->level);
	}

	return level;
}

/*
 * Check security policy requirements against the actual
 * packet contents.  Return one if the packet should be
 * reject as "invalid"; otherwiser return zero to have the
 * packet treated as "valid".
 *
 * OUT:
 *	0: valid
 *	1: invalid
 */
int
ipsec_in_reject(struct secpolicy *sp, struct mbuf *m)
{
	struct ipsecrequest *isr;
	int need_auth;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec_in_reject: using SP\n");
		kdebug_secpolicy(sp));

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		return 1;
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return 0;
	}

	KASSERT(sp->policy == IPSEC_POLICY_IPSEC,
		("ipsec_in_reject: invalid policy %u", sp->policy));

	/* XXX should compare policy against ipsec header history */

	need_auth = 0;
	for (isr = sp->req; isr != NULL; isr = isr->next) {
		if (ipsec_get_reqlevel(isr) != IPSEC_LEVEL_REQUIRE)
			continue;
		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			if ((m->m_flags & M_DECRYPTED) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("ipsec_in_reject: ESP m_flags:%x\n",
					    m->m_flags));
				return 1;
			}

			if (!need_auth &&
			    isr->sav != NULL &&
			    isr->sav->tdb_authalgxform != NULL &&
			    (m->m_flags & M_AUTHIPDGM) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("ipsec_in_reject: ESP/AH m_flags:%x\n",
					    m->m_flags));
				return 1;
			}
			break;
		case IPPROTO_AH:
			need_auth = 1;
			if ((m->m_flags & M_AUTHIPHDR) == 0) {
				KEYDEBUG(KEYDEBUG_IPSEC_DUMP,
				    printf("ipsec_in_reject: AH m_flags:%x\n",
					    m->m_flags));
				return 1;
			}
			break;
		case IPPROTO_IPCOMP:
			/*
			 * we don't really care, as IPcomp document
			 * says that we shouldn't compress small
			 * packets, IPComp policy should always be
			 * treated as being in "use" level.
			 */
			break;
		}
	}
	return 0;		/* valid */
}

/*
 * Check AH/ESP integrity.
 * This function is called from tcp_input(), udp_input(),
 * and {ah,esp}4_input for tunnel mode
 */
int
ipsec4_in_reject(m, inp)
	struct mbuf *m;
	struct inpcb *inp;
{
	struct secpolicy *sp;
	int error;
	int result;

	KASSERT(m != NULL, ("ipsec4_in_reject_so: null mbuf"));

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND, IP_FORWARDING, &error);
	else
		sp = ipsec_getpolicybysock(m, IPSEC_DIR_INBOUND, inp, &error);

	if (sp != NULL) {
		result = ipsec_in_reject(sp, m);
		if (result)
			newipsecstat.ips_in_polvio++;
		KEY_FREESP(&sp);
	} else {
		result = 0;	/* XXX should be panic ?
				 * -> No, there may be error. */
	}
	return result;
}

#ifdef INET6
/*
 * Check AH/ESP integrity.
 * This function is called from tcp6_input(), udp6_input(),
 * and {ah,esp}6_input for tunnel mode
 */
int
ipsec6_in_reject(m, inp)
	struct mbuf *m;
	struct inpcb *inp;
{
	struct secpolicy *sp = NULL;
	int error;
	int result;

	/* sanity check */
	if (m == NULL)
		return 0;	/* XXX should be panic ? */

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND, IP_FORWARDING, &error);
	else
		sp = ipsec_getpolicybysock(m, IPSEC_DIR_INBOUND, inp, &error);

	if (sp != NULL) {
		result = ipsec_in_reject(sp, m);
		if (result)
			newipsecstat.ips_in_polvio++;
		KEY_FREESP(&sp);
	} else {
		result = 0;
	}
	return result;
}
#endif

/*
 * compute the byte size to be occupied by IPsec header.
 * in case it is tunneled, it includes the size of outer IP header.
 * NOTE: SP passed is free in this function.
 */
static size_t
ipsec_hdrsiz(struct secpolicy *sp)
{
	struct ipsecrequest *isr;
	size_t siz;

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec_hdrsiz: using SP\n");
		kdebug_secpolicy(sp));

	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
		return 0;
	}

	KASSERT(sp->policy == IPSEC_POLICY_IPSEC,
		("ipsec_hdrsiz: invalid policy %u", sp->policy));

	siz = 0;
	for (isr = sp->req; isr != NULL; isr = isr->next) {
		size_t clen = 0;

		switch (isr->saidx.proto) {
		case IPPROTO_ESP:
			clen = esp_hdrsiz(isr->sav);
			break;
		case IPPROTO_AH:
			clen = ah_hdrsiz(isr->sav);
			break;
		case IPPROTO_IPCOMP:
			clen = sizeof(struct ipcomp);
			break;
		}

		if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
			switch (isr->saidx.dst.sa.sa_family) {
			case AF_INET:
				clen += sizeof(struct ip);
				break;
#ifdef INET6
			case AF_INET6:
				clen += sizeof(struct ip6_hdr);
				break;
#endif
			default:
				ipseclog((LOG_ERR, "ipsec_hdrsiz: "
				    "unknown AF %d in IPsec tunnel SA\n",
				    ((struct sockaddr *)&isr->saidx.dst)->sa_family));
				break;
			}
		}
		siz += clen;
	}

	return siz;
}

/* This function is called from ip_forward() and ipsec4_hdrsize_tcp(). */
size_t
ipsec4_hdrsiz(m, dir, inp)
	struct mbuf *m;
	u_int dir;
	struct inpcb *inp;
{
	struct secpolicy *sp;
	int error;
	size_t size;

	KASSERT(m != NULL, ("ipsec4_hdrsiz: null mbuf"));
	KASSERT(inp == NULL || inp->inp_socket != NULL,
		("ipsec4_hdrsize: socket w/o inpcb"));

	/* get SP for this packet.
	 * When we are called from ip_forward(), we call
	 * ipsec_getpolicybyaddr() with IP_FORWARDING flag.
	 */
	if (inp == NULL)
		sp = ipsec_getpolicybyaddr(m, dir, IP_FORWARDING, &error);
	else
		sp = ipsec_getpolicybysock(m, dir, inp, &error);

	if (sp != NULL) {
		size = ipsec_hdrsiz(sp);
		KEYDEBUG(KEYDEBUG_IPSEC_DATA,
			printf("ipsec4_hdrsiz: size:%lu.\n",
				(unsigned long)size));

		KEY_FREESP(&sp);
	} else {
		size = 0;	/* XXX should be panic ? */
	}
	return size;
}

#ifdef INET6
/* This function is called from ipsec6_hdrsize_tcp(),
 * and maybe from ip6_forward.()
 */
size_t
ipsec6_hdrsiz(m, dir, in6p)
	struct mbuf *m;
	u_int dir;
	struct in6pcb *in6p;
{
	struct secpolicy *sp;
	int error;
	size_t size;

	KASSERT(m != NULL, ("ipsec6_hdrsiz: null mbuf"));
	KASSERT(in6p == NULL || in6p->in6p_socket != NULL,
		("ipsec6_hdrsize: socket w/o inpcb"));

	/* get SP for this packet */
	/* XXX Is it right to call with IP_FORWARDING. */
	if (in6p == NULL)
		sp = ipsec_getpolicybyaddr(m, dir, IP_FORWARDING, &error);
	else
		sp = ipsec_getpolicybysock(m, dir, in6p, &error);

	if (sp == NULL)
		return 0;
	size = ipsec_hdrsiz(sp);
	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_hdrsiz: size:%lu.\n", (unsigned long)size));
	KEY_FREESP(&sp);

	return size;
}
#endif /*INET6*/

/*
 * Check the variable replay window.
 * ipsec_chkreplay() performs replay check before ICV verification.
 * ipsec_updatereplay() updates replay bitmap.  This must be called after
 * ICV verification (it also performs replay check, which is usually done
 * beforehand).
 * 0 (zero) is returned if packet disallowed, 1 if packet permitted.
 *
 * based on RFC 2401.
 */
int
ipsec_chkreplay(seq, sav)
	u_int32_t seq;
	struct secasvar *sav;
{
	const struct secreplay *replay;
	u_int32_t diff;
	int fr;
	u_int32_t wsizeb;	/* constant: bits of window size */
	int frlast;		/* constant: last frame */

	SPLASSERT(net, "ipsec_chkreplay");

	KASSERT(sav != NULL, ("ipsec_chkreplay: Null SA"));
	KASSERT(sav->replay != NULL, ("ipsec_chkreplay: Null replay state"));

	replay = sav->replay;

	if (replay->wsize == 0)
		return 1;	/* no need to check replay. */

	/* constant */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* sequence number of 0 is invalid */
	if (seq == 0)
		return 0;

	/* first time is always okay */
	if (replay->count == 0)
		return 1;

	if (seq > replay->lastseq) {
		/* larger sequences are okay */
		return 1;
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* over range to check, i.e. too old or wrapped */
		if (diff >= wsizeb)
			return 0;

		fr = frlast - diff / 8;

		/* this packet already seen ? */
		if ((replay->bitmap)[fr] & (1 << (diff % 8)))
			return 0;

		/* out of order but good */
		return 1;
	}
}

/*
 * check replay counter whether to update or not.
 * OUT:	0:	OK
 *	1:	NG
 */
int
ipsec_updatereplay(seq, sav)
	u_int32_t seq;
	struct secasvar *sav;
{
	struct secreplay *replay;
	u_int32_t diff;
	int fr;
	u_int32_t wsizeb;	/* constant: bits of window size */
	int frlast;		/* constant: last frame */

	SPLASSERT(net, "ipsec_updatereplay");

	KASSERT(sav != NULL, ("ipsec_updatereplay: Null SA"));
	KASSERT(sav->replay != NULL, ("ipsec_updatereplay: Null replay state"));

	replay = sav->replay;

	if (replay->wsize == 0)
		goto ok;	/* no need to check replay. */

	/* constant */
	frlast = replay->wsize - 1;
	wsizeb = replay->wsize << 3;

	/* sequence number of 0 is invalid */
	if (seq == 0)
		return 1;

	/* first time */
	if (replay->count == 0) {
		replay->lastseq = seq;
		bzero(replay->bitmap, replay->wsize);
		(replay->bitmap)[frlast] = 1;
		goto ok;
	}

	if (seq > replay->lastseq) {
		/* seq is larger than lastseq. */
		diff = seq - replay->lastseq;

		/* new larger sequence number */
		if (diff < wsizeb) {
			/* In window */
			/* set bit for this packet */
			vshiftl(replay->bitmap, diff, replay->wsize);
			(replay->bitmap)[frlast] |= 1;
		} else {
			/* this packet has a "way larger" */
			bzero(replay->bitmap, replay->wsize);
			(replay->bitmap)[frlast] = 1;
		}
		replay->lastseq = seq;

		/* larger is good */
	} else {
		/* seq is equal or less than lastseq. */
		diff = replay->lastseq - seq;

		/* over range to check, i.e. too old or wrapped */
		if (diff >= wsizeb)
			return 1;

		fr = frlast - diff / 8;

		/* this packet already seen ? */
		if ((replay->bitmap)[fr] & (1 << (diff % 8)))
			return 1;

		/* mark as seen */
		(replay->bitmap)[fr] |= (1 << (diff % 8));

		/* out of order but good */
	}

ok:
	if (replay->count == ~0) {

		/* set overflow flag */
		replay->overflow++;

		/* don't increment, no more packets accepted */
		if ((sav->flags & SADB_X_EXT_CYCSEQ) == 0)
			return 1;

		ipseclog((LOG_WARNING, "replay counter made %d cycle. %s\n",
		    replay->overflow, ipsec_logsastr(sav)));
	}

	replay->count++;

	return 0;
}

/*
 * shift variable length bunffer to left.
 * IN:	bitmap: pointer to the buffer
 * 	nbit:	the number of to shift.
 *	wsize:	buffer size (bytes).
 */
static void
vshiftl(bitmap, nbit, wsize)
	unsigned char *bitmap;
	int nbit, wsize;
{
	int s, j, i;
	unsigned char over;

	for (j = 0; j < nbit; j += 8) {
		s = (nbit - j < 8) ? (nbit - j): 8;
		bitmap[0] <<= s;
		for (i = 1; i < wsize; i++) {
			over = (bitmap[i] >> (8 - s));
			bitmap[i] <<= s;
			bitmap[i-1] |= over;
		}
	}

	return;
}

/* Return a printable string for the address. */
char *
ipsec_address(union sockaddr_union* sa)
{
	switch (sa->sa.sa_family) {
#if INET
	case AF_INET:
		return inet_ntoa(sa->sin.sin_addr);
#endif /* INET */

#if INET6
	case AF_INET6:
		return ip6_sprintf(&sa->sin6.sin6_addr);
#endif /* INET6 */

	default:
		return "(unknown address family)";
	}
}

const char *
ipsec_logsastr(sav)
	struct secasvar *sav;
{
	static char buf[256];
	char *p;
	struct secasindex *saidx = &sav->sah->saidx;

	KASSERT(saidx->src.sa.sa_family == saidx->dst.sa.sa_family,
		("ipsec_logsastr: address family mismatch"));

	p = buf;
	snprintf(buf, sizeof(buf), "SA(SPI=%u ", (u_int32_t)ntohl(sav->spi));
	while (p && *p)
		p++;
	/* NB: only use ipsec_address on one address at a time */
	snprintf(p, sizeof (buf) - (p - buf), "src=%s ",
		ipsec_address(&saidx->src));
	while (p && *p)
		p++;
	snprintf(p, sizeof (buf) - (p - buf), "dst=%s)",
		ipsec_address(&saidx->dst));

	return buf;
}

void
ipsec_dumpmbuf(m)
	struct mbuf *m;
{
	int totlen;
	int i;
	u_char *p;

	totlen = 0;
	printf("---\n");
	while (m) {
		p = mtod(m, u_char *);
		for (i = 0; i < m->m_len; i++) {
			printf("%02x ", p[i]);
			totlen++;
			if (totlen % 16 == 0)
				printf("\n");
		}
		m = m->m_next;
	}
	if (totlen % 16 != 0)
		printf("\n");
	printf("---\n");
}

/* XXX this stuff doesn't belong here... */

static	struct xformsw* xforms = NULL;

/*
 * Register a transform; typically at system startup.
 */
void
xform_register(struct xformsw* xsp)
{
	xsp->xf_next = xforms;
	xforms = xsp;
}

/*
 * Initialize transform support in an sav.
 */
int
xform_init(struct secasvar *sav, int xftype)
{
	struct xformsw *xsp;

	for (xsp = xforms; xsp; xsp = xsp->xf_next)
		if (xsp->xf_type == xftype)
			return (*xsp->xf_init)(sav, xsp);
	return EINVAL;
}
