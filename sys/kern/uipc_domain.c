/*
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
 *	@(#)uipc_domain.c	8.2 (Berkeley) 10/18/93
 * $FreeBSD: src/sys/kern/uipc_domain.c,v 1.22.2.1 2001/07/03 11:01:37 ume Exp $
 * $DragonFly: src/sys/kern/uipc_domain.c,v 1.8 2005/03/04 02:21:48 hsu Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <vm/vm_zone.h>

/*
 * System initialization
 *
 * Note: domain initialization wants to take place on a per domain basis
 * as a result of traversing a linker set.  Most likely, each domain
 * want to call a registration function rather than being handled here
 * in domaininit().  Probably this will look like:
 *
 * SYSINIT(unique, SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY, domain_add, xxx)
 *
 * Where 'xxx' is replaced by the address of a parameter struct to be
 * passed to the doamin_add() function.
 */

static void domaininit (void *);
SYSINIT(domain, SI_SUB_PROTO_DOMAIN, SI_ORDER_FIRST, domaininit, NULL)

static void	pffasttimo (void *);
static void	pfslowtimo (void *);

struct domainlist domains;

static struct callout pffasttimo_ch;
static struct callout pfslowtimo_ch;

/*
 * Add a new protocol domain to the list of supported domains
 * Note: you cant unload it again because a socket may be using it.
 * XXX can't fail at this time.
 */
static void
net_init_domain(struct domain *dp)
{
	struct protosw *pr;
	int s;

	s = splnet();
	if (dp->dom_init)
		(*dp->dom_init)();
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if (pr->pr_usrreqs == NULL)
			panic("domaininit: %ssw[%d] has no usrreqs!",
			      dp->dom_name, pr - dp->dom_protosw);
		if (pr->pr_init)
			(*pr->pr_init)();
	}
	/*
	 * update global information about maximums
	 */
	max_hdr = max_linkhdr + max_protohdr;
	max_datalen = MHLEN - max_hdr;
	splx(s);
}

/*
 * Add a new protocol domain to the list of supported domains
 * Note: you cant unload it again because a socket may be using it.
 * XXX can't fail at this time.
 */
void
net_add_domain(void *data)
{
	struct domain *dp = data;
	int s;

	s = splnet();
	SLIST_INSERT_HEAD(&domains, dp, dom_next);
	splx(s);
	net_init_domain(dp);
}

/* ARGSUSED*/
static void
domaininit(void *dummy)
{
	/*
	 * Before we do any setup, make sure to initialize the
	 * zone allocator we get struct sockets from.  The obvious
	 * maximum number of sockets is `maxfiles', but it is possible
	 * to have a socket without an open file (e.g., a connection waiting
	 * to be accept(2)ed).  Rather than think up and define a
	 * better value, we just use nmbclusters, since that's what people
	 * are told to increase first when the network runs out of memory.
	 * Perhaps we should have two pools, one of unlimited size
	 * for use during socreate(), and one ZONE_INTERRUPT pool for
	 * use in sonewconn().
	 */
	socket_zone = zinit("socket", sizeof(struct socket), maxsockets,
			    ZONE_INTERRUPT, 0);

	if (max_linkhdr < 16)		/* XXX */
		max_linkhdr = 16;

	callout_init(&pffasttimo_ch);
	callout_init(&pfslowtimo_ch);
	callout_reset(&pffasttimo_ch, 1, pffasttimo, NULL);
	callout_reset(&pfslowtimo_ch, 1, pfslowtimo, NULL);
}


struct protosw *
pffindtype(int family, int type)
{
	struct domain *dp;
	struct protosw *pr;

	SLIST_FOREACH(dp, &domains, dom_next)
		if (dp->dom_family == family)
			goto found;
	return (NULL);

found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
		if (pr->pr_type && pr->pr_type == type)
			return (pr);
	return (NULL);
}

struct protosw *
pffindproto(int family, int protocol, int type)
{
	struct domain *dp;
	struct protosw *pr;
	struct protosw *maybe = NULL;

	if (family == 0)
		return (NULL);
	SLIST_FOREACH(dp, &domains, dom_next)
		if (dp->dom_family == family)
			goto found;
	return (NULL);

found:
	for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++) {
		if ((pr->pr_protocol == protocol) && (pr->pr_type == type))
			return (pr);

		if (type == SOCK_RAW && pr->pr_type == SOCK_RAW &&
		    pr->pr_protocol == 0 && maybe == (struct protosw *)NULL)
			maybe = pr;
	}
	return (maybe);
}

void
pfctlinput(int cmd, struct sockaddr *sa)
{
	struct domain *dp;
	struct protosw *pr;

	SLIST_FOREACH(dp, &domains, dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, (void *)NULL);
}

void
pfctlinput2(int cmd, struct sockaddr *sa, void *ctlparam)
{
	struct domain *dp;
	struct protosw *pr;

	if (!sa)
		return;
	SLIST_FOREACH(dp, &domains, dom_next) {
		/*
		 * the check must be made by xx_ctlinput() anyways, to
		 * make sure we use data item pointed to by ctlparam in
		 * correct way.  the following check is made just for safety.
		 */
		if (dp->dom_family != sa->sa_family)
			continue;

		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_ctlinput)
				(*pr->pr_ctlinput)(cmd, sa, ctlparam);
	}
}

static void
pfslowtimo(void *arg)
{
	struct domain *dp;
	struct protosw *pr;

	SLIST_FOREACH(dp, &domains, dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_slowtimo)
				(*pr->pr_slowtimo)();
	callout_reset(&pfslowtimo_ch, hz / 2, pfslowtimo, NULL);
}

static void
pffasttimo(void *arg)
{
	struct domain *dp;
	struct protosw *pr;

	SLIST_FOREACH(dp, &domains, dom_next)
		for (pr = dp->dom_protosw; pr < dp->dom_protoswNPROTOSW; pr++)
			if (pr->pr_fasttimo)
				(*pr->pr_fasttimo)();
	callout_reset(&pffasttimo_ch, hz / 5, pffasttimo, NULL);
}
