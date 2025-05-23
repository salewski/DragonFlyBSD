/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * kern.c,v 3.8.4.10 1998/01/06 02:00:51 fenner Exp
 *
 * $FreeBSD: src/usr.sbin/mrouted/kern.c,v 1.12 1999/08/28 01:17:04 peter Exp $
 * $DragonFly: src/usr.sbin/mrouted/kern.c,v 1.4 2004/12/16 03:39:05 dillon Exp $
 */

#include "defs.h"

int curttl = 0;

void
k_set_rcvbuf(int bufsize, int minsize)
{
    int delta = bufsize / 2;
    int iter = 0;

    /*
     * Set the socket buffer.  If we can't set it as large as we
     * want, search around to try to find the highest acceptable
     * value.  The highest acceptable value being smaller than
     * minsize is a fatal error.
     */
    if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF,
		(char *)&bufsize, sizeof(bufsize)) < 0) {
	bufsize -= delta;
	while (1) {
	    iter++;
	    if (delta > 1)
		delta /= 2;

	    if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF,
			(char *)&bufsize, sizeof(bufsize)) < 0) {
		    bufsize -= delta;
	    } else {
		    if (delta < 1024)
			break;
		    bufsize += delta;
	    }
	}
	if (bufsize < minsize) {
	    log(LOG_ERR, 0, "OS-allowed buffer size %u < app min %u",
		bufsize, minsize);
	    /*NOTREACHED*/
	}
    }
    IF_DEBUG(DEBUG_KERN)
    log(LOG_DEBUG, 0, "Got %d byte buffer size in %d iterations",
	    bufsize, iter);
}

void
k_hdr_include(int boolv)
{
#ifdef IP_HDRINCL
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_HDRINCL,
		   (char *)&boolv, sizeof(boolv)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_HDRINCL %u", boolv);
#endif
}

void
k_set_ttl(int t)
{
#ifndef RAW_OUTPUT_IS_RAW
    u_char ttl;

    ttl = t;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_TTL,
		   (char *)&ttl, sizeof(ttl)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_TTL %u", ttl);
#endif
    curttl = t;
}

void
k_set_loop(int l)
{
    u_char loop;

    loop = l;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
		   (char *)&loop, sizeof(loop)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_LOOP %u", loop);
}

void
k_set_if(u_int32 ifa)
{
    struct in_addr adr;

    adr.s_addr = ifa;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_IF,
		   (char *)&adr, sizeof(adr)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_IF %s",
	    		    inet_fmt(ifa, s1));
}

void
k_join(u_int32 grp, u_int32 ifa)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't join group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}

void
k_leave(u_int32 grp, u_int32 ifa)
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't leave group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}

void
k_init_dvmrp(void)
{
#ifdef OLD_KERNEL
    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_INIT,
		   (char *)NULL, 0) < 0)
#else
    int v=1;

    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_INIT,
		   (char *)&v, sizeof(int)) < 0)
#endif
	log(LOG_ERR, errno, "can't enable Multicast routing in kernel");
}

void
k_stop_dvmrp(void)
{
    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_DONE,
		   (char *)NULL, 0) < 0)
	log(LOG_WARNING, errno, "can't disable Multicast routing in kernel");
}

void 
k_add_vif(vifi_t vifi, struct uvif *v)
{
    struct vifctl vc;

    vc.vifc_vifi            = vifi;
    vc.vifc_flags           = v->uv_flags & VIFF_KERNEL_FLAGS;
    vc.vifc_threshold       = v->uv_threshold;
    vc.vifc_rate_limit	    = v->uv_rate_limit;
    vc.vifc_lcl_addr.s_addr = v->uv_lcl_addr;
    vc.vifc_rmt_addr.s_addr = v->uv_rmt_addr;

    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_ADD_VIF,
		   (char *)&vc, sizeof(vc)) < 0)
	log(LOG_ERR, errno, "setsockopt MRT_ADD_VIF on vif %d", vifi);
}

void
k_del_vif(vifi_t vifi)
{

    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_DEL_VIF,
		   (char *)&vifi, sizeof(vifi)) < 0)
	log(LOG_ERR, errno, "setsockopt MRT_DEL_VIF on vif %d", vifi);
}

/*
 * Adds a (source, mcastgrp) entry to the kernel
 */
void
k_add_rg(u_int32 origin, struct gtable *g)
{
    struct mfcctl mc;
    vifi_t i;

#ifdef DEBUG_MFC
    md_log(MD_ADD, origin, g->gt_mcastgrp);
#endif
    /* copy table values so that setsockopt can process it */
    mc.mfcc_origin.s_addr = origin;
#ifdef OLD_KERNEL
    mc.mfcc_originmask.s_addr = 0xffffffff;
#endif
    mc.mfcc_mcastgrp.s_addr = g->gt_mcastgrp;
    mc.mfcc_parent = g->gt_route ? g->gt_route->rt_parent : NO_VIF;
    for (i = 0; i < numvifs; i++)
	mc.mfcc_ttls[i] = g->gt_ttls[i];

    /* write to kernel space */
    if (setsockopt(igmp_socket, IPPROTO_IP, MRT_ADD_MFC,
		   (char *)&mc, sizeof(mc)) < 0) {
#ifdef DEBUG_MFC
	md_log(MD_ADD_FAIL, origin, g->gt_mcastgrp);
#endif
	log(LOG_WARNING, errno, "setsockopt MRT_ADD_MFC",
		inet_fmt(origin, s1), inet_fmt(g->gt_mcastgrp, s2));
    }
}


/*
 * Deletes a (source, mcastgrp) entry from the kernel
 */
int
k_del_rg(u_int32 origin, struct gtable *g)
{
    struct mfcctl mc;
    int retval;

#ifdef DEBUG_MFC
    md_log(MD_DEL, origin, g->gt_mcastgrp);
#endif
    /* copy table values so that setsockopt can process it */
    mc.mfcc_origin.s_addr = origin;
#ifdef OLD_KERNEL
    mc.mfcc_originmask.s_addr = 0xffffffff;
#endif
    mc.mfcc_mcastgrp.s_addr = g->gt_mcastgrp;

    /* write to kernel space */
    if ((retval = setsockopt(igmp_socket, IPPROTO_IP, MRT_DEL_MFC,
		   (char *)&mc, sizeof(mc))) < 0) {
#ifdef DEBUG_MFC
	md_log(MD_DEL_FAIL, origin, g->gt_mcastgrp);
#endif
	log(LOG_WARNING, errno, "setsockopt MRT_DEL_MFC of (%s %s)",
		inet_fmt(origin, s1), inet_fmt(g->gt_mcastgrp, s2));
    }

    return retval;
}	

/*
 * Get the kernel's idea of what version of mrouted needs to run with it.
 */
int
k_get_version(void)
{
#ifdef OLD_KERNEL
    return -1;
#else
    int vers;
    int len = sizeof(vers);

    if (getsockopt(igmp_socket, IPPROTO_IP, MRT_VERSION,
			(char *)&vers, &len) < 0)
	log(LOG_ERR, errno,
		"getsockopt MRT_VERSION: perhaps your kernel is too old");

    return vers;
#endif
}
