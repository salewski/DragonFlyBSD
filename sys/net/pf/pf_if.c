/*	$FreeBSD: src/sys/contrib/pf/net/pf_if.c,v 1.6 2004/09/14 15:20:24 mlaier Exp $ */
/*	$OpenBSD: pf_if.c,v 1.11 2004/03/15 11:38:23 cedric Exp $ */
/* add	$OpenBSD: pf_if.c,v 1.19 2004/08/11 12:06:44 henning Exp $ */
/*	$DragonFly: src/sys/net/pf/pf_if.c,v 1.3.2.1 2005/06/15 16:09:34 joerg Exp $ */

/*
 * Copyright (c) 2004 The DragonFly Project.  All rights reserved.
 *
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2003 Cedric Berger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/eventhandler.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <vm/vm_zone.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/pf/pfvar.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

#define ACCEPT_FLAGS(oklist)			\
	do {					\
		if ((flags & ~(oklist)) &	\
		    PFI_FLAG_ALLMASK)		\
			return (EINVAL);	\
	} while (0)

#define senderr(e)      do { rv = (e); goto _bad; } while (0)

struct pfi_kif		**pfi_index2kif;
struct pfi_kif		 *pfi_self, *pfi_dummy;
int			  pfi_indexlim;
struct pfi_ifhead	  pfi_ifs;
struct pfi_statehead	  pfi_statehead;
int			  pfi_ifcnt;
vm_zone_t		  pfi_addr_pl;
long			  pfi_update = 1;
struct pfr_addr		 *pfi_buffer;
int			  pfi_buffer_cnt;
int			  pfi_buffer_max;
char			  pfi_reserved_anchor[PF_ANCHOR_NAME_SIZE] =
				PF_RESERVED_ANCHOR;
char			  pfi_interface_ruleset[PF_RULESET_NAME_SIZE] =
				PF_INTERFACE_RULESET;

eventhandler_tag	 pfi_clone_cookie = NULL;
eventhandler_tag	 pfi_attach_cookie = NULL;
eventhandler_tag	 pfi_detach_cookie = NULL;

void		 pfi_dynaddr_update(void *);
void		 pfi_kifaddr_update(void *);
void		 pfi_table_update(struct pfr_ktable *, struct pfi_kif *,
		    int, int);
void		 pfi_instance_add(struct ifnet *, int, int);
void		 pfi_address_add(struct sockaddr *, int, int);
int		 pfi_if_compare(struct pfi_kif *, struct pfi_kif *);
struct pfi_kif	*pfi_if_create(const char *, struct pfi_kif *, int);
void		 pfi_copy_group(char *, const char *, int);
void		 pfi_dynamic_drivers(void);
void		 pfi_newgroup(const char *, int);
int		 pfi_skip_if(const char *, struct pfi_kif *, int);
int		 pfi_unmask(void *);
void		 pfi_dohooks(struct pfi_kif *);
void		 pfi_kifaddr_update_event(void *, struct ifnet *);
void		 pfi_attach_clone_event(void *, struct if_clone *);
void		 pfi_attach_ifnet_event(void *, struct ifnet *);
void		 pfi_detach_ifnet_event(void *, struct ifnet *);

RB_PROTOTYPE(pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);
RB_GENERATE(pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);

#define PFI_DYNAMIC_BUSES	{ "pcmcia", "cardbus", "uhub" }
#define PFI_BUFFER_MAX		0x10000
MALLOC_DEFINE(PFI_MTYPE, "pf_if", "pf interface table");

void
pfi_initialize(void)
{
	struct ifnet	*ifp;

	if (pfi_self != NULL)	/* already initialized */
		return;

	TAILQ_INIT(&pfi_statehead);
	pfi_buffer_max = 64;
	pfi_buffer = malloc(pfi_buffer_max * sizeof(*pfi_buffer),
	    PFI_MTYPE, M_WAITOK);
	pfi_self = pfi_if_create("self", NULL, PFI_IFLAG_GROUP);
	pfi_dynamic_drivers();

	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_dunit != IF_DUNIT_NONE)
			pfi_attach_ifnet(ifp);
	}
	pfi_dummy = pfi_if_create("notyet", pfi_self,
	    PFI_IFLAG_GROUP | PFI_IFLAG_DYNAMIC);
	pfi_attach_cookie = EVENTHANDLER_REGISTER(ifnet_attach_event,
	    pfi_attach_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_detach_cookie = EVENTHANDLER_REGISTER(ifnet_detach_event,
	    pfi_detach_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_clone_cookie = EVENTHANDLER_REGISTER(if_clone_event,
	    pfi_attach_clone_event, NULL, EVENTHANDLER_PRI_ANY);
}

void
pfi_cleanup(void)
{
	struct pfi_kif *p, key;
	struct ifnet *ifp;

	EVENTHANDLER_DEREGISTER(ifnet_attach_event, pfi_attach_cookie);
	EVENTHANDLER_DEREGISTER(ifnet_detach_event, pfi_detach_cookie);
	EVENTHANDLER_DEREGISTER(if_clone_event, pfi_clone_cookie);

	/* release PFI_IFLAG_INSTANCE */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		strlcpy(key.pfik_name, ifp->if_xname, sizeof(key.pfik_name));
		p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
		if (p != NULL)
			pfi_detach_ifnet(ifp);
	}

	/* XXX clear all other interface group */
	while ((p = RB_MIN(pfi_ifhead, &pfi_ifs))) {
		RB_REMOVE(pfi_ifhead, &pfi_ifs, p);

		free(p->pfik_ah_head, PFI_MTYPE);
		free(p, PFI_MTYPE);
	}
	free(pfi_index2kif, PFI_MTYPE);
	free(pfi_buffer, PFI_MTYPE);
	pfi_index2kif = NULL;
	pfi_buffer = NULL;
	pfi_self = NULL;
}

/*
 * Wrapper functions for FreeBSD eventhandler
 */
void
pfi_kifaddr_update_event(void *arg, struct ifnet *ifp)
{
	struct pfi_kif *p = arg;
	
	/* 
	 * Check to see if it is 'our' interface as we do not have per
	 * interface hooks and thus get an update for every interface.
	 */
	if (p && p->pfik_ifp == ifp)
		pfi_kifaddr_update(p);
}

void
pfi_attach_clone_event(void *arg __unused, struct if_clone *ifc)
{
	pfi_attach_clone(ifc);
}

void
pfi_attach_ifnet_event(void *arg __unused, struct ifnet *ifp)
{
	if (ifp->if_dunit != IF_DUNIT_NONE)
		pfi_attach_ifnet(ifp);
}

void
pfi_detach_ifnet_event(void *arg __unused, struct ifnet *ifp)
{
	pfi_detach_ifnet(ifp);
}

void
pfi_attach_clone(struct if_clone *ifc)
{
	pfi_initialize();
	pfi_newgroup(ifc->ifc_name, PFI_IFLAG_CLONABLE);
}

void
pfi_attach_ifnet(struct ifnet *ifp)
{
	struct pfi_kif	*p, *q, key;
	int		 s;
	int		 realname;

	pfi_initialize();
	s = splsoftnet();
	pfi_update++;
	if (ifp->if_index >= pfi_indexlim) {
		/*
		 * grow pfi_index2kif,  similar to ifindex2ifnet code in if.c
		 */
		size_t m, n, oldlim;
		struct pfi_kif **mp, **np;

		oldlim = pfi_indexlim;
		if (pfi_indexlim == 0)
			pfi_indexlim = 64;
		while (ifp->if_index >= pfi_indexlim)
			pfi_indexlim <<= 1;

		m = oldlim * sizeof(struct pfi_kif *);
		mp = pfi_index2kif;
		n = pfi_indexlim * sizeof(struct pfi_kif *);
		np = malloc(n, PFI_MTYPE, M_NOWAIT);
		if (np == NULL)
			panic("pfi_attach_ifnet: "
			    "cannot allocate translation table");
		bzero(np, n);
		if (mp != NULL)
			bcopy(mp, np, m);
		pfi_index2kif = np;
		if (mp != NULL)
			free(mp, PFI_MTYPE);
	}

	strlcpy(key.pfik_name, ifp->if_xname, sizeof(key.pfik_name));
	p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
	/* some additional trickery for placeholders */
	if ((p == NULL) || (p->pfik_parent == pfi_dummy)) {
		/* are we looking at a renamed instance or not? */
		pfi_copy_group(key.pfik_name, ifp->if_xname,
		    sizeof(key.pfik_name));
		realname = (strncmp(key.pfik_name, ifp->if_dname,
		    sizeof(key.pfik_name)) == 0);
		/* add group */
		/* we can change if_xname, hence use if_dname as group id */
		pfi_copy_group(key.pfik_name, ifp->if_dname,
		    sizeof(key.pfik_name));
		q = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
		if (q == NULL)
		    q = pfi_if_create(key.pfik_name, pfi_self,
		        PFI_IFLAG_GROUP|PFI_IFLAG_DYNAMIC);
		else if (q->pfik_parent == pfi_dummy) {
			q->pfik_parent = pfi_self;
			q->pfik_flags = (PFI_IFLAG_GROUP | PFI_IFLAG_DYNAMIC);
		}
		if (q == NULL)
			panic("pfi_attach_ifnet: "
			    "cannot allocate '%s' group", key.pfik_name);

		/* add/modify interface */
		if (p == NULL)
			p = pfi_if_create(ifp->if_xname, q,
			    realname?PFI_IFLAG_INSTANCE:PFI_IFLAG_PLACEHOLDER);
		else {
			/* remove from the dummy group */
			/* XXX: copy stats? We should not have any!!! */
			pfi_dummy->pfik_delcnt++;
			TAILQ_REMOVE(&pfi_dummy->pfik_grouphead, p,
			    pfik_instances);
			/* move to the right group */
			p->pfik_parent = q;
			q->pfik_addcnt++;
			TAILQ_INSERT_TAIL(&q->pfik_grouphead, p,
			    pfik_instances);
			if (realname) {
				p->pfik_flags &= ~PFI_IFLAG_PLACEHOLDER;
				p->pfik_flags |= PFI_IFLAG_INSTANCE;
			}
		}
		if (p == NULL)
			panic("pfi_attach_ifnet: "
			    "cannot allocate '%s' interface", ifp->if_xname);
	} else
		q = p->pfik_parent;
	p->pfik_ifp = ifp;
	p->pfik_flags |= PFI_IFLAG_ATTACHED;
	p->pfik_ah_cookie = EVENTHANDLER_REGISTER(ifaddr_event,
	    pfi_kifaddr_update_event, p, EVENTHANDLER_PRI_ANY);
	pfi_index2kif[ifp->if_index] = p;
	pfi_dohooks(p);
	splx(s);
}

void
pfi_detach_ifnet(struct ifnet *ifp)
{
	struct pfi_kif	*p, *q, key;
	int		 s;

	strlcpy(key.pfik_name, ifp->if_xname, sizeof(key.pfik_name));

	s = splsoftnet();
	pfi_update++;
	p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
	if (p == NULL) {
		printf("pfi_detach_ifnet: cannot find %s", ifp->if_xname);
		splx(s);
		return;
	}
	EVENTHANDLER_DEREGISTER(ifaddr_event, p->pfik_ah_cookie);
	q = p->pfik_parent;
	p->pfik_ifp = NULL;
	p->pfik_flags &= ~PFI_IFLAG_ATTACHED;
	pfi_index2kif[ifp->if_index] = NULL;
	pfi_dohooks(p);
	pfi_maybe_destroy(p);
	splx(s);
}

struct pfi_kif *
pfi_lookup_create(const char *name)
{
	struct pfi_kif	*p, *q, key;
	int		 s;

	s = splsoftnet();
	p = pfi_lookup_if(name);
	if (p == NULL) {
		pfi_copy_group(key.pfik_name, name, sizeof(key.pfik_name));
		q = pfi_lookup_if(key.pfik_name);
		if ((q != NULL) && (q->pfik_parent != pfi_dummy))
			p = pfi_if_create(name, q, PFI_IFLAG_INSTANCE);
		else {
			if (pfi_dummy == NULL)
				panic("no 'notyet' dummy group");
			p = pfi_if_create(name, pfi_dummy,
			    PFI_IFLAG_PLACEHOLDER);
		}
	}
	splx(s);
	return (p);
}

struct pfi_kif *
pfi_attach_rule(const char *name)
{
	struct pfi_kif	*p;

	p = pfi_lookup_create(name);
	if (p != NULL)
		p->pfik_rules++;
	return (p);
}

void
pfi_detach_rule(struct pfi_kif *p)
{
	if (p == NULL)
		return;
	if (p->pfik_rules > 0)
		p->pfik_rules--;
	else
		printf("pfi_detach_rule: reference count at 0\n");
	pfi_maybe_destroy(p);
}

void
pfi_attach_state(struct pfi_kif *p)
{
	if (!p->pfik_states++)
		TAILQ_INSERT_TAIL(&pfi_statehead, p, pfik_w_states);
}

void
pfi_detach_state(struct pfi_kif *p)
{
	if (p == NULL)
		return;
	if (p->pfik_states <= 0) {
		printf("pfi_detach_state: reference count <= 0\n");
		return;
	}
	if (!--p->pfik_states)
		TAILQ_REMOVE(&pfi_statehead, p, pfik_w_states);
	pfi_maybe_destroy(p);
}

int
pfi_dynaddr_setup(struct pf_addr_wrap *aw, sa_family_t af)
{
	struct pfi_dynaddr	*dyn;
	char			 tblname[PF_TABLE_NAME_SIZE];
	struct pf_ruleset	*ruleset = NULL;
	int			 s, rv = 0;

	if (aw->type != PF_ADDR_DYNIFTL)
		return (0);
	dyn = pool_get(&pfi_addr_pl, PR_NOWAIT);
	if (dyn == NULL)
		return (1);
	bzero(dyn, sizeof(*dyn));

	s = splsoftnet();
	dyn->pfid_kif = pfi_attach_rule(aw->v.ifname);
	if (dyn->pfid_kif == NULL)
		senderr(1);

	dyn->pfid_net = pfi_unmask(&aw->v.a.mask);
	if (af == AF_INET && dyn->pfid_net == 32)
		dyn->pfid_net = 128;
	strlcpy(tblname, aw->v.ifname, sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_NETWORK)
		strlcat(tblname, ":network", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_BROADCAST)
		strlcat(tblname, ":broadcast", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_PEER)
		strlcat(tblname, ":peer", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_NOALIAS)
		strlcat(tblname, ":0", sizeof(tblname));
	if (dyn->pfid_net != 128)
		snprintf(tblname + strlen(tblname),
		    sizeof(tblname) - strlen(tblname), "/%d", dyn->pfid_net);
	ruleset = pf_find_or_create_ruleset(pfi_reserved_anchor,
	    pfi_interface_ruleset);
	if (ruleset == NULL)
		senderr(1);

	dyn->pfid_kt = pfr_attach_table(ruleset, tblname);
	if (dyn->pfid_kt == NULL)
		senderr(1);

	dyn->pfid_kt->pfrkt_flags |= PFR_TFLAG_ACTIVE;
	dyn->pfid_iflags = aw->iflags;
	dyn->pfid_af = af;
	dyn->pfid_hook_cookie = hook_establish(dyn->pfid_kif->pfik_ah_head, 1,
	    pfi_dynaddr_update, dyn);
	if (dyn->pfid_hook_cookie == NULL)
		senderr(1);

	aw->p.dyn = dyn;
	pfi_dynaddr_update(aw->p.dyn);
	splx(s);
	return (0);

_bad:
	if (dyn->pfid_kt != NULL)
		pfr_detach_table(dyn->pfid_kt);
	if (ruleset != NULL)
		pf_remove_if_empty_ruleset(ruleset);
	if (dyn->pfid_kif != NULL)
		pfi_detach_rule(dyn->pfid_kif);
	pool_put(&pfi_addr_pl, dyn);
	splx(s);
	return (rv);
}

void
pfi_dynaddr_update(void *p)
{
	struct pfi_dynaddr	*dyn = (struct pfi_dynaddr *)p;
	struct pfi_kif		*kif = dyn->pfid_kif;
	struct pfr_ktable	*kt = dyn->pfid_kt;

	if (dyn == NULL || kif == NULL || kt == NULL)
		panic("pfi_dynaddr_update");
	if (kt->pfrkt_larg != pfi_update) {
		/* this table needs to be brought up-to-date */
		pfi_table_update(kt, kif, dyn->pfid_net, dyn->pfid_iflags);
		kt->pfrkt_larg = pfi_update;
	}
	pfr_dynaddr_update(kt, dyn);
}

void
pfi_table_update(struct pfr_ktable *kt, struct pfi_kif *kif, int net, int flags)
{
	int			 e, size2 = 0;
	struct pfi_kif		*p;
	struct pfr_table	 t;

	if ((kif->pfik_flags & PFI_IFLAG_INSTANCE) && kif->pfik_ifp == NULL) {
		pfr_clr_addrs(&kt->pfrkt_t, NULL, 0);
		return;
	}
	pfi_buffer_cnt = 0;
	if ((kif->pfik_flags & PFI_IFLAG_INSTANCE))
		pfi_instance_add(kif->pfik_ifp, net, flags);
	else if (strcmp(kif->pfik_name, "self")) {
		TAILQ_FOREACH(p, &kif->pfik_grouphead, pfik_instances)
			pfi_instance_add(p->pfik_ifp, net, flags);
	} else {
		RB_FOREACH(p, pfi_ifhead, &pfi_ifs)
			if (p->pfik_flags & PFI_IFLAG_INSTANCE)
				pfi_instance_add(p->pfik_ifp, net, flags);
	}
	t = kt->pfrkt_t;
	t.pfrt_flags = 0;
	if ((e = pfr_set_addrs(&t, pfi_buffer, pfi_buffer_cnt, &size2,
	    NULL, NULL, NULL, 0)))
		printf("pfi_table_update: cannot set %d new addresses "
		    "into table %s: %d\n", pfi_buffer_cnt, kt->pfrkt_name, e);
}

void
pfi_instance_add(struct ifnet *ifp, int net, int flags)
{
	struct ifaddr	*ia;
	int		 got4 = 0, got6 = 0;
	int		 net2, af;

	if (ifp == NULL)
		return;
	TAILQ_FOREACH(ia, &ifp->if_addrlist, ifa_list) {
		if (ia->ifa_addr == NULL)
			continue;
		af = ia->ifa_addr->sa_family;
		if (af != AF_INET && af != AF_INET6)
			continue;
		/*
		 * XXX: For point-to-point interfaces, (ifname:0) and IPv4,
		 *	jump over address without a proper route to work
		 *	around a problem with ppp not fully removing the
		 *	address used during IPCP.
		 */
		if ((ifp->if_flags & IFF_POINTOPOINT) &&
		    !(ia->ifa_flags & IFA_ROUTE) &&
		    (flags & PFI_AFLAG_NOALIAS) && (af == AF_INET))
			continue;
		if ((flags & PFI_AFLAG_BROADCAST) && af == AF_INET6)
			continue;
		if ((flags & PFI_AFLAG_BROADCAST) &&
		    !(ifp->if_flags & IFF_BROADCAST))
			continue;
		if ((flags & PFI_AFLAG_PEER) &&
		    !(ifp->if_flags & IFF_POINTOPOINT))
			continue;
		if ((flags & PFI_AFLAG_NETWORK) && af == AF_INET6 &&
		    IN6_IS_ADDR_LINKLOCAL(
		    &((struct sockaddr_in6 *)ia->ifa_addr)->sin6_addr))
			continue;
		if (flags & PFI_AFLAG_NOALIAS) {
			if (af == AF_INET && got4)
				continue;
			if (af == AF_INET6 && got6)
				continue;
		}
		if (af == AF_INET)
			got4 = 1;
		else
			got6 = 1;
		net2 = net;
		if (net2 == 128 && (flags & PFI_AFLAG_NETWORK)) {
			if (af == AF_INET) {
				net2 = pfi_unmask(&((struct sockaddr_in *)
				    ia->ifa_netmask)->sin_addr);
			} else {
				net2 = pfi_unmask(&((struct sockaddr_in6 *)
				    ia->ifa_netmask)->sin6_addr);
			}
		}
		if (af == AF_INET && net2 > 32)
			net2 = 32;
		if (flags & PFI_AFLAG_BROADCAST)
			pfi_address_add(ia->ifa_broadaddr, af, net2);
		else if (flags & PFI_AFLAG_PEER)
			pfi_address_add(ia->ifa_dstaddr, af, net2);
		else
			pfi_address_add(ia->ifa_addr, af, net2);
	}
}

void
pfi_address_add(struct sockaddr *sa, int af, int net)
{
	struct pfr_addr	*p;
	int		 i;

	if (pfi_buffer_cnt >= pfi_buffer_max) {
		int		 new_max = pfi_buffer_max * 2;

		if (new_max > PFI_BUFFER_MAX) {
			printf("pfi_address_add: address buffer full (%d/%d)\n",
			    pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		p = malloc(new_max * sizeof(*pfi_buffer), PFI_MTYPE,
		    M_NOWAIT);
		if (p == NULL) {
			printf("pfi_address_add: no memory to grow buffer "
			    "(%d/%d)\n", pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		memcpy(pfi_buffer, p, pfi_buffer_cnt * sizeof(*pfi_buffer));
		/* no need to zero buffer */
		free(pfi_buffer, PFI_MTYPE);
		pfi_buffer = p;
		pfi_buffer_max = new_max;
	}
	if (af == AF_INET && net > 32)
		net = 128;
	p = pfi_buffer + pfi_buffer_cnt++;
	bzero(p, sizeof(*p));
	p->pfra_af = af;
	p->pfra_net = net;
	if (af == AF_INET)
		p->pfra_ip4addr = ((struct sockaddr_in *)sa)->sin_addr;
	if (af == AF_INET6) {
		p->pfra_ip6addr = ((struct sockaddr_in6 *)sa)->sin6_addr;
		if (IN6_IS_ADDR_LINKLOCAL(&p->pfra_ip6addr))
			p->pfra_ip6addr.s6_addr16[1] = 0;
	}
	/* mask network address bits */
	if (net < 128)
		((caddr_t)p)[p->pfra_net/8] &= ~(0xFF >> (p->pfra_net%8));
	for (i = (p->pfra_net+7)/8; i < sizeof(p->pfra_u); i++)
		((caddr_t)p)[i] = 0;
}

void
pfi_dynaddr_remove(struct pf_addr_wrap *aw)
{
	int	s;

	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL || aw->p.dyn->pfid_kt == NULL)
		return;

	s = splsoftnet();
	hook_disestablish(aw->p.dyn->pfid_kif->pfik_ah_head,
	    aw->p.dyn->pfid_hook_cookie);
	pfi_detach_rule(aw->p.dyn->pfid_kif);
	aw->p.dyn->pfid_kif = NULL;
	pfr_detach_table(aw->p.dyn->pfid_kt);
	aw->p.dyn->pfid_kt = NULL;
	pool_put(&pfi_addr_pl, aw->p.dyn);
	aw->p.dyn = NULL;
	splx(s);
}

void
pfi_dynaddr_copyout(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL)
		return;
	aw->p.dyncnt = aw->p.dyn->pfid_acnt4 + aw->p.dyn->pfid_acnt6;
}

void
pfi_kifaddr_update(void *v)
{
	int		 s;

	s = splsoftnet();
	pfi_update++;
	pfi_dohooks(v);
	splx(s);
}

int
pfi_if_compare(struct pfi_kif *p, struct pfi_kif *q)
{
	return (strncmp(p->pfik_name, q->pfik_name, IFNAMSIZ));
}

struct pfi_kif *
pfi_if_create(const char *name, struct pfi_kif *q, int flags)
{
	struct pfi_kif *p;

	p = malloc(sizeof(*p), PFI_MTYPE, M_NOWAIT);
	if (p == NULL)
		return (NULL);
	bzero(p, sizeof(*p));
	p->pfik_ah_head = malloc(sizeof(*p->pfik_ah_head), PFI_MTYPE,
	    M_NOWAIT);
	if (p->pfik_ah_head == NULL) {
		free(p, PFI_MTYPE);
		return (NULL);
	}
	bzero(p->pfik_ah_head, sizeof(*p->pfik_ah_head));
	TAILQ_INIT(p->pfik_ah_head);
	TAILQ_INIT(&p->pfik_grouphead);
	strlcpy(p->pfik_name, name, sizeof(p->pfik_name));
	RB_INIT(&p->pfik_lan_ext);
	RB_INIT(&p->pfik_ext_gwy);
	p->pfik_flags = flags;
	p->pfik_parent = q;
	p->pfik_tzero = time_second;

	RB_INSERT(pfi_ifhead, &pfi_ifs, p);
	if (q != NULL) {
		q->pfik_addcnt++;
		TAILQ_INSERT_TAIL(&q->pfik_grouphead, p, pfik_instances);
	}
	pfi_ifcnt++;
	return (p);
}

int
pfi_maybe_destroy(struct pfi_kif *p)
{
	int		 i, j, k, s;
	struct pfi_kif	*q = p->pfik_parent;

	if ((p->pfik_flags & (PFI_IFLAG_ATTACHED | PFI_IFLAG_GROUP)) ||
	    p->pfik_rules > 0 || p->pfik_states > 0)
		if (!(p->pfik_flags & PFI_IFLAG_PLACEHOLDER))
			return (0);

	s = splsoftnet();
	if (q != NULL) {
		for (i = 0; i < 2; i++)
			for (j = 0; j < 2; j++)
				for (k = 0; k < 2; k++) {
					q->pfik_bytes[i][j][k] +=
					    p->pfik_bytes[i][j][k];
					q->pfik_packets[i][j][k] +=
					    p->pfik_packets[i][j][k];
			/* clear stats in case we return to the dummy group */
					p->pfik_bytes[i][j][k] = 0;
					p->pfik_packets[i][j][k] = 0;
				}
		q->pfik_delcnt++;
		TAILQ_REMOVE(&q->pfik_grouphead, p, pfik_instances);
	}
	if (p->pfik_rules > 0 || p->pfik_states > 0) {
		/* move back to the dummy group */
		p->pfik_parent = pfi_dummy;
		pfi_dummy->pfik_addcnt++;
		TAILQ_INSERT_TAIL(&pfi_dummy->pfik_grouphead, p,
		    pfik_instances);
		splx(s);
		return (0);
	}
	pfi_ifcnt--;
	RB_REMOVE(pfi_ifhead, &pfi_ifs, p);
	splx(s);

	free(p->pfik_ah_head, PFI_MTYPE);
	free(p, PFI_MTYPE);
	return (1);
}

void
pfi_copy_group(char *p, const char *q, int m)
{
	while (m > 1 && *q && !(*q >= '0' && *q <= '9')) {
		*p++ = *q++;
		m--;
	}
	if (m > 0)
		*p++ = '\0';
}

void
pfi_dynamic_drivers(void)
{
	struct ifnet	*ifp;

/*
 * For FreeBSD basically every interface is "dynamic" as we can unload
 * modules e.g.
 */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (ifp->if_dunit == IF_DUNIT_NONE)
			continue;
		pfi_newgroup(ifp->if_dname, PFI_IFLAG_DYNAMIC);
	}
}

void
pfi_newgroup(const char *name, int flags)
{
	struct pfi_kif	*p;

	p = pfi_lookup_if(name);
	if (p == NULL)
		p = pfi_if_create(name, pfi_self, PFI_IFLAG_GROUP);
	if (p == NULL) {
		printf("pfi_newgroup: cannot allocate '%s' group", name);
		return;
	}
	p->pfik_flags |= flags;
}

void
pfi_fill_oldstatus(struct pf_status *pfs)
{
	struct pfi_kif	*p, key;
	int		 i, j, k, s;

	strlcpy(key.pfik_name, pfs->ifname, sizeof(key.pfik_name));
	s = splsoftnet();
	p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
	if (p == NULL) {
		splx(s);
		return;
	}
	bzero(pfs->pcounters, sizeof(pfs->pcounters));
	bzero(pfs->bcounters, sizeof(pfs->bcounters));
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			for (k = 0; k < 2; k++) {
				pfs->pcounters[i][j][k] =
					p->pfik_packets[i][j][k];
				pfs->bcounters[i][j] +=
					p->pfik_bytes[i][j][k];
			}
	splx(s);
}

int
pfi_clr_istats(const char *name, int *nzero, int flags)
{
	struct pfi_kif	*p;
	int		 n = 0, s;
	long		 tzero = time_second;

	s = splsoftnet();
	ACCEPT_FLAGS(PFI_FLAG_GROUP|PFI_FLAG_INSTANCE);
	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p, flags))
			continue;
		bzero(p->pfik_packets, sizeof(p->pfik_packets));
		bzero(p->pfik_bytes, sizeof(p->pfik_bytes));
		p->pfik_tzero = tzero;
		n++;
	}
	splx(s);
	if (nzero != NULL)
		*nzero = n;
	return (0);
}

int
pfi_get_ifaces(const char *name, struct pfi_if *buf, int *size, int flags)
{
	struct pfi_kif	*p;
	int		 s, n = 0;

	ACCEPT_FLAGS(PFI_FLAG_GROUP|PFI_FLAG_INSTANCE);
	s = splsoftnet();
	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p, flags))
			continue;
		if (*size > n++) {
			if (!p->pfik_tzero)
				p->pfik_tzero = boottime.tv_sec;
			if (copyout(p, buf++, sizeof(*buf))) {
				splx(s);
				return (EFAULT);
			}
		}
	}
	splx(s);
	*size = n;
	return (0);
}

struct pfi_kif *
pfi_lookup_if(const char *name)
{
	struct pfi_kif	*p, key;

	strlcpy(key.pfik_name, name, sizeof(key.pfik_name));
	p = RB_FIND(pfi_ifhead, &pfi_ifs, &key);
	return (p);
}

int
pfi_skip_if(const char *filter, struct pfi_kif *p, int f)
{
	int	n;

	if ((p->pfik_flags & PFI_IFLAG_GROUP) && !(f & PFI_FLAG_GROUP))
		return (1);
	if ((p->pfik_flags & PFI_IFLAG_INSTANCE) && !(f & PFI_FLAG_INSTANCE))
		return (1);
	if (filter == NULL || !*filter)
		return (0);
	if (!strcmp(p->pfik_name, filter))
		return (0);	/* exact match */
	n = strlen(filter);
	if (n < 1 || n >= IFNAMSIZ)
		return (1);	/* sanity check */
	if (filter[n-1] >= '0' && filter[n-1] <= '9')
		return (1);	/* only do exact match in that case */
	if (strncmp(p->pfik_name, filter, n))
		return (1);	/* prefix doesn't match */
	return (p->pfik_name[n] < '0' || p->pfik_name[n] > '9');
}

/* from pf_print_state.c */
int
pfi_unmask(void *addr)
{
	struct pf_addr *m = addr;
	int i = 31, j = 0, b = 0;
	u_int32_t tmp;

	while (j < 4 && m->addr32[j] == 0xffffffff) {
		b += 32;
		j++;
	}
	if (j < 4) {
		tmp = ntohl(m->addr32[j]);
		for (i = 31; tmp & (1 << i); --i)
			b++;
	}
	return (b);
}

void
pfi_dohooks(struct pfi_kif *p)
{
	for (; p != NULL; p = p->pfik_parent)
		dohooks(p->pfik_ah_head, 0);
}

int
pfi_match_addr(struct pfi_dynaddr *dyn, struct pf_addr *a, sa_family_t af)
{
	if (af == AF_INET) {
		switch (dyn->pfid_acnt4) {
		case 0:
			return (0);
		case 1:
			return (PF_MATCHA(0, &dyn->pfid_addr4,
			    &dyn->pfid_mask4, a, AF_INET));
		default:
			return (pfr_match_addr(dyn->pfid_kt, a, AF_INET));
		}
	} else {
		switch (dyn->pfid_acnt6) {
		case 0:
			return (0);
		case 1:
			return (PF_MATCHA(0, &dyn->pfid_addr6,
			    &dyn->pfid_mask6, a, AF_INET6));
		default:
			return (pfr_match_addr(dyn->pfid_kt, a, AF_INET6));
		}
	}
}
