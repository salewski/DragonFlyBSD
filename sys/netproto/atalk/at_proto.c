/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 *
 * $DragonFly: src/sys/netproto/atalk/at_proto.c,v 1.4 2005/03/04 02:21:49 hsu Exp $
 */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>

#include <sys/kernel.h>

#include <net/route.h>

#include "at.h"
#include "ddp_var.h"
#include "at_extern.h"

extern struct domain atalkdomain;

static struct protosw atalksw[] = {
    {
	/* Identifiers */
	SOCK_DGRAM,	&atalkdomain,	ATPROTO_DDP,	PR_ATOMIC|PR_ADDR,
	/*
	 * protocol-protocol interface.
	 * fields are pr_input, pr_output, pr_ctlinput, and pr_ctloutput.
	 * pr_input can be called from the udp protocol stack for iptalk
	 * packets bound for a local socket.
	 * pr_output can be used by higher level appletalk protocols, should
	 * they be included in the kernel.
	 */
	0,		ddp_output,	0,		0,
	cpu0_soport,
	/* utility routines. */
	ddp_init,	0,		0,		0,
	&ddp_usrreqs
    },
};

static struct domain atalkdomain = {
	AF_APPLETALK, "appletalk", NULL, NULL, NULL,
	atalksw, &atalksw[sizeof(atalksw)/sizeof(atalksw[0])],
	SLIST_ENTRY_INITIALIZER,
	rn_inithead, 8 * offsetof(struct sockaddr_at, sat_addr),
	sizeof(struct sockaddr_at),
};

DOMAIN_SET(atalk);

