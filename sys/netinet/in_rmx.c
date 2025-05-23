/*
 * Copyright 1994, 1995 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netinet/in_rmx.c,v 1.37.2.3 2002/08/09 14:49:23 ru Exp $
 * $DragonFly: src/sys/netinet/in_rmx.c,v 1.11 2005/03/09 23:31:44 hsu Exp $
 */

/*
 * This code does two things necessary for the enhanced TCP metrics to
 * function in a useful manner:
 *  1) It marks all non-host routes as `cloning', thus ensuring that
 *     every actual reference to such a route actually gets turned
 *     into a reference to a host route to the specific destination
 *     requested.
 *  2) When such routes lose all their references, it arranges for them
 *     to be deleted in some random collection of circumstances, so that
 *     a large quantity of stale routing data is not kept in kernel memory
 *     indefinitely.  See in_rtqtimo() below for the exact mechanism.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#define RTPRF_EXPIRING	RTF_PROTO3	/* set on routes we manage */

static struct callout in_rtqtimo_ch;

/*
 * Do what we need to do when inserting a route.
 */
static struct radix_node *
in_addroute(char *key, char *mask, struct radix_node_head *head,
	    struct radix_node *treenodes)
{
	struct rtentry *rt = (struct rtentry *)treenodes;
	struct sockaddr_in *sin = (struct sockaddr_in *)rt_key(rt);
	struct radix_node *ret;

	/*
	 * For IP, all unicast non-host routes are automatically cloning.
	 */
	if (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
		rt->rt_flags |= RTF_MULTICAST;

	if (!(rt->rt_flags & (RTF_HOST | RTF_CLONING | RTF_MULTICAST)))
		rt->rt_flags |= RTF_PRCLONING;

	/*
	 * A little bit of help for both IP output and input:
	 *   For host routes, we make sure that RTF_BROADCAST
	 *   is set for anything that looks like a broadcast address.
	 *   This way, we can avoid an expensive call to in_broadcast()
	 *   in ip_output() most of the time (because the route passed
	 *   to ip_output() is almost always a host route).
	 *
	 *   We also do the same for local addresses, with the thought
	 *   that this might one day be used to speed up ip_input().
	 *
	 * We also mark routes to multicast addresses as such, because
	 * it's easy to do and might be useful (but this is much more
	 * dubious since it's so easy to inspect the address).  (This
	 * is done above.)
	 */
	if (rt->rt_flags & RTF_HOST) {
		if (in_broadcast(sin->sin_addr, rt->rt_ifp)) {
			rt->rt_flags |= RTF_BROADCAST;
		} else {
			if (satosin(rt->rt_ifa->ifa_addr)->sin_addr.s_addr
			    == sin->sin_addr.s_addr)
				rt->rt_flags |= RTF_LOCAL;
		}
	}

	if (rt->rt_rmx.rmx_mtu != 0 && !(rt->rt_rmx.rmx_locks & RTV_MTU) &&
	    rt->rt_ifp != NULL)
		rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;

	ret = rn_addroute(key, mask, head, treenodes);
	if (ret == NULL && rt->rt_flags & RTF_HOST) {
		struct rtentry *oldrt;

		/*
		 * We are trying to add a host route, but can't.
		 * Find out if it is because of an ARP entry and
		 * delete it if so.
		 */
		oldrt = rtpurelookup((struct sockaddr *)sin);
		if (oldrt != NULL) {
			--oldrt->rt_refcnt;
			if (oldrt->rt_flags & RTF_LLINFO &&
			    oldrt->rt_flags & RTF_HOST &&
			    oldrt->rt_gateway &&
			    oldrt->rt_gateway->sa_family == AF_LINK) {
				rtrequest(RTM_DELETE, rt_key(oldrt),
					  oldrt->rt_gateway, rt_mask(oldrt),
					  oldrt->rt_flags, NULL);
				ret = rn_addroute(key, mask, head, treenodes);
			}
		}
	}

	/*
	 * If the new route created successfully, and we are forwarding,
	 * and there is a cached route, free it.  Otherwise, we may end
	 * up using the wrong route.
	 */
	if (ret != NULL && ipforwarding && ipforward_rt.ro_rt != NULL) {
		RTFREE(ipforward_rt.ro_rt);
		ipforward_rt.ro_rt = NULL;
	}

	return ret;
}

/*
 * This code is the inverse of in_closeroute: on first reference, if we
 * were managing the route, stop doing so and set the expiration timer
 * back off again.
 */
static struct radix_node *
in_matchroute(char *key, struct radix_node_head *head)
{
	struct radix_node *rn = rn_match(key, head);
	struct rtentry *rt = (struct rtentry *)rn;

	if (rt != NULL && rt->rt_refcnt == 0) { /* this is first reference */
		if (rt->rt_flags & RTPRF_EXPIRING) {
			rt->rt_flags &= ~RTPRF_EXPIRING;
			rt->rt_rmx.rmx_expire = 0;
		}
	}
	return rn;
}

static int rtq_reallyold = 60*60;  /* one hour is ``really old'' */
SYSCTL_INT(_net_inet_ip, IPCTL_RTEXPIRE, rtexpire, CTLFLAG_RW,
    &rtq_reallyold , 0,
    "Default expiration time on cloned routes");

static int rtq_minreallyold = 10;  /* never automatically crank down to less */
SYSCTL_INT(_net_inet_ip, IPCTL_RTMINEXPIRE, rtminexpire, CTLFLAG_RW,
    &rtq_minreallyold , 0,
    "Minimum time to attempt to hold onto cloned routes");

static int rtq_toomany = 128;	   /* 128 cached routes is ``too many'' */
SYSCTL_INT(_net_inet_ip, IPCTL_RTMAXCACHE, rtmaxcache, CTLFLAG_RW,
    &rtq_toomany , 0, "Upper limit on cloned routes");

/*
 * On last reference drop, mark the route as belong to us so that it can be
 * timed out.
 */
static void
in_closeroute(struct radix_node *rn, struct radix_node_head *head)
{
	struct rtentry *rt = (struct rtentry *)rn;

	if (!(rt->rt_flags & RTF_UP))
		return;		/* prophylactic measures */

	if ((rt->rt_flags & (RTF_LLINFO | RTF_HOST)) != RTF_HOST)
		return;

	if ((rt->rt_flags & (RTF_WASCLONED | RTPRF_EXPIRING)) != RTF_WASCLONED)
		return;

	/*
	 * As requested by David Greenman:
	 * If rtq_reallyold is 0, just delete the route without
	 * waiting for a timeout cycle to kill it.
	 */
	if (rtq_reallyold != 0) {
		rt->rt_flags |= RTPRF_EXPIRING;
		rt->rt_rmx.rmx_expire = time_second + rtq_reallyold;
	} else {
		/*
		 * Remove route from the radix tree, but defer deallocation
		 * until we return to rtfree().
		 */
		rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway, rt_mask(rt),
			  rt->rt_flags, &rt);
	}
}

struct rtqk_arg {
	struct radix_node_head *rnh;
	int draining;
	int killed;
	int found;
	int updating;
	time_t nextstop;
};

/*
 * Get rid of old routes.  When draining, this deletes everything, even when
 * the timeout is not expired yet.  When updating, this makes sure that
 * nothing has a timeout longer than the current value of rtq_reallyold.
 */
static int
in_rtqkill(struct radix_node *rn, void *rock)
{
	struct rtqk_arg *ap = rock;
	struct rtentry *rt = (struct rtentry *)rn;
	int err;

	if (rt->rt_flags & RTPRF_EXPIRING) {
		ap->found++;
		if (ap->draining || rt->rt_rmx.rmx_expire <= time_second) {
			if (rt->rt_refcnt > 0)
				panic("rtqkill route really not free");

			err = rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
					rt_mask(rt), rt->rt_flags, NULL);
			if (err)
				log(LOG_WARNING, "in_rtqkill: error %d\n", err);
			else
				ap->killed++;
		} else {
			if (ap->updating &&
			    (rt->rt_rmx.rmx_expire - time_second >
			     rtq_reallyold)) {
				rt->rt_rmx.rmx_expire = time_second +
				    rtq_reallyold;
			}
			ap->nextstop = lmin(ap->nextstop,
					    rt->rt_rmx.rmx_expire);
		}
	}

	return 0;
}

#define RTQ_TIMEOUT	60*10	/* run no less than once every ten minutes */
static int rtq_timeout = RTQ_TIMEOUT;

static void
in_rtqtimo(void *rock)
{
	struct radix_node_head *rnh = rock;
	struct rtqk_arg arg;
	struct timeval atv;
	static time_t last_adjusted_timeout = 0;
	int s;

	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = time_second + rtq_timeout;
	arg.draining = arg.updating = 0;
	s = splnet();
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	splx(s);

	/*
	 * Attempt to be somewhat dynamic about this:
	 * If there are ``too many'' routes sitting around taking up space,
	 * then crank down the timeout, and see if we can't make some more
	 * go away.  However, we make sure that we will never adjust more
	 * than once in rtq_timeout seconds, to keep from cranking down too
	 * hard.
	 */
	if ((arg.found - arg.killed > rtq_toomany) &&
	    (time_second - last_adjusted_timeout >= rtq_timeout) &&
	    rtq_reallyold > rtq_minreallyold) {
		rtq_reallyold = 2*rtq_reallyold / 3;
		if (rtq_reallyold < rtq_minreallyold) {
			rtq_reallyold = rtq_minreallyold;
		}

		last_adjusted_timeout = time_second;
#ifdef DIAGNOSTIC
		log(LOG_DEBUG, "in_rtqtimo: adjusted rtq_reallyold to %d\n",
		    rtq_reallyold);
#endif
		arg.found = arg.killed = 0;
		arg.updating = 1;
		s = splnet();
		rnh->rnh_walktree(rnh, in_rtqkill, &arg);
		splx(s);
	}

	atv.tv_usec = 0;
	atv.tv_sec = arg.nextstop - time_second;
	callout_reset(&in_rtqtimo_ch, tvtohz_high(&atv), in_rtqtimo, rock);
}

void
in_rtqdrain(void)
{
	struct radix_node_head *rnh = rt_tables[AF_INET];
	struct rtqk_arg arg;
	int s;

	arg.found = arg.killed = 0;
	arg.rnh = rnh;
	arg.nextstop = 0;
	arg.draining = 1;
	arg.updating = 0;
	s = splnet();
	rnh->rnh_walktree(rnh, in_rtqkill, &arg);
	splx(s);
}

/*
 * Initialize our routing tree.
 */
int
in_inithead(void **head, int off)
{
	struct radix_node_head *rnh;

	if (!rn_inithead(head, off))
		return 0;

	if (head != (void **)&rt_tables[AF_INET]) /* BOGUS! */
		return 1;	/* only do this for the real routing table */

	rnh = *head;
	rnh->rnh_addaddr = in_addroute;
	rnh->rnh_matchaddr = in_matchroute;
	rnh->rnh_close = in_closeroute;
	callout_init(&in_rtqtimo_ch);
	in_rtqtimo(rnh);	/* kick off timeout first time */
	return 1;
}

/*
 * This zaps old routes when the interface goes down or interface
 * address is deleted.  In the latter case, it deletes static routes
 * that point to this address.  If we don't do this, we may end up
 * using the old address in the future.  The ones we always want to
 * get rid of are things like ARP entries, since the user might down
 * the interface, walk over to a completely different network, and
 * plug back in.
 */
struct in_ifadown_arg {
	struct radix_node_head *rnh;
	struct ifaddr *ifa;
	int del;
};

static int
in_ifadownkill(struct radix_node *rn, void *xap)
{
	struct in_ifadown_arg *ap = xap;
	struct rtentry *rt = (struct rtentry *)rn;
	int err;

	if (rt->rt_ifa == ap->ifa &&
	    (ap->del || !(rt->rt_flags & RTF_STATIC))) {
		/*
		 * We need to disable the automatic prune that happens
		 * in this case in rtrequest() because it will blow
		 * away the pointers that rn_walktree() needs in order
		 * continue our descent.  We will end up deleting all
		 * the routes that rtrequest() would have in any case,
		 * so that behavior is not needed there.
		 */
		rt->rt_flags &= ~(RTF_CLONING | RTF_PRCLONING);
		err = rtrequest(RTM_DELETE, rt_key(rt), rt->rt_gateway,
				rt_mask(rt), rt->rt_flags, NULL);
		if (err)
			log(LOG_WARNING, "in_ifadownkill: error %d\n", err);
	}
	return 0;
}

int
in_ifadown(struct ifaddr *ifa, int delete)
{
	struct in_ifadown_arg arg;
	struct radix_node_head *rnh;

	if (ifa->ifa_addr->sa_family != AF_INET)
		return 1;

	arg.rnh = rnh = rt_tables[AF_INET];
	arg.ifa = ifa;
	arg.del = delete;
	rnh->rnh_walktree(rnh, in_ifadownkill, &arg);
	ifa->ifa_flags &= ~IFA_ROUTE;
	return 0;
}
