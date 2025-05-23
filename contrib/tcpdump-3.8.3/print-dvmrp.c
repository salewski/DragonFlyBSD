/*
 * Copyright (c) 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-dvmrp.c,v 1.24.2.3 2003/11/19 09:41:28 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"

/*
 * DVMRP message types and flag values shamelessly stolen from
 * mrouted/dvmrp.h.
 */
#define DVMRP_PROBE		1	/* for finding neighbors */
#define DVMRP_REPORT		2	/* for reporting some or all routes */
#define DVMRP_ASK_NEIGHBORS	3	/* sent by mapper, asking for a list */
					/* of this router's neighbors */
#define DVMRP_NEIGHBORS		4	/* response to such a request */
#define DVMRP_ASK_NEIGHBORS2	5	/* as above, want new format reply */
#define DVMRP_NEIGHBORS2	6
#define DVMRP_PRUNE		7	/* prune message */
#define DVMRP_GRAFT		8	/* graft message */
#define DVMRP_GRAFT_ACK		9	/* graft acknowledgement */

/*
 * 'flags' byte values in DVMRP_NEIGHBORS2 reply.
 */
#define DVMRP_NF_TUNNEL		0x01	/* neighbors reached via tunnel */
#define DVMRP_NF_SRCRT		0x02	/* tunnel uses IP source routing */
#define DVMRP_NF_DOWN		0x10	/* kernel state of interface */
#define DVMRP_NF_DISABLED	0x20	/* administratively disabled */
#define DVMRP_NF_QUERIER	0x40	/* I am the subnet's querier */

static int print_probe(const u_char *, const u_char *, u_int);
static int print_report(const u_char *, const u_char *, u_int);
static int print_neighbors(const u_char *, const u_char *, u_int);
static int print_neighbors2(const u_char *, const u_char *, u_int);
static int print_prune(const u_char *);
static int print_graft(const u_char *);
static int print_graft_ack(const u_char *);

static u_int32_t target_level;

void
dvmrp_print(register const u_char *bp, register u_int len)
{
	register const u_char *ep;
	register u_char type;

	ep = (const u_char *)snapend;
	if (bp >= ep)
		return;

	TCHECK(bp[1]);
	type = bp[1];

	/* Skip IGMP header */
	bp += 8;
	len -= 8;

	switch (type) {

	case DVMRP_PROBE:
		printf(" Probe");
		if (vflag) {
			if (print_probe(bp, ep, len) < 0)
				goto trunc;
		}
		break;

	case DVMRP_REPORT:
		printf(" Report");
		if (vflag > 1) {
			if (print_report(bp, ep, len) < 0)
				goto trunc;
		}
		break;

	case DVMRP_ASK_NEIGHBORS:
		printf(" Ask-neighbors(old)");
		break;

	case DVMRP_NEIGHBORS:
		printf(" Neighbors(old)");
		if (print_neighbors(bp, ep, len) < 0)
			goto trunc;
		break;

	case DVMRP_ASK_NEIGHBORS2:
		printf(" Ask-neighbors2");
		break;

	case DVMRP_NEIGHBORS2:
		printf(" Neighbors2");
		/*
		 * extract version and capabilities from IGMP group
		 * address field
		 */
		bp -= 4;
		TCHECK2(bp[0], 4);
		target_level = (bp[0] << 24) | (bp[1] << 16) |
		    (bp[2] << 8) | bp[3];
		bp += 4;
		if (print_neighbors2(bp, ep, len) < 0)
			goto trunc;
		break;

	case DVMRP_PRUNE:
		printf(" Prune");
		if (print_prune(bp) < 0)
			goto trunc;
		break;

	case DVMRP_GRAFT:
		printf(" Graft");
		if (print_graft(bp) < 0)
			goto trunc;
		break;

	case DVMRP_GRAFT_ACK:
		printf(" Graft-ACK");
		if (print_graft_ack(bp) < 0)
			goto trunc;
		break;

	default:
		printf(" [type %d]", type);
		break;
	}
	return;

trunc:
	printf("[|dvmrp]");
	return;
}

static int 
print_report(register const u_char *bp, register const u_char *ep,
    register u_int len)
{
	register u_int32_t mask, origin;
	register int metric, done;
	register u_int i, width;

	while (len > 0) {
		if (len < 3) {
			printf(" [|]");
			return (0);
		}
		TCHECK2(bp[0], 3);
		mask = (u_int32_t)0xff << 24 | bp[0] << 16 | bp[1] << 8 | bp[2];
		width = 1;
		if (bp[0])
			width = 2;
		if (bp[1])
			width = 3;
		if (bp[2])
			width = 4;

		printf("\n\tMask %s", intoa(htonl(mask)));
		bp += 3;
		len -= 3;
		do {
			if (bp + width + 1 > ep) {
				printf(" [|]");
				return (0);
			}
			if (len < width + 1) {
				printf("\n\t  [Truncated Report]");
				return (0);
			}
			origin = 0;
			for (i = 0; i < width; ++i) {
				TCHECK(*bp);
				origin = origin << 8 | *bp++;
			}
			for ( ; i < 4; ++i)
				origin <<= 8;

			TCHECK(*bp);
			metric = *bp++;
			done = metric & 0x80;
			metric &= 0x7f;
			printf("\n\t  %s metric %d", intoa(htonl(origin)),
				metric);
			len -= width + 1;
		} while (!done);
	}
	return (0);
trunc:
	return (-1);
}

static int
print_probe(register const u_char *bp, register const u_char *ep,
    register u_int len)
{
	register u_int32_t genid;

	TCHECK2(bp[0], 4);
	if ((len < 4) || ((bp + 4) > ep)) {
		/* { (ctags) */
		printf(" [|}");
		return (0);
	}
	genid = (bp[0] << 24) | (bp[1] << 16) | (bp[2] << 8) | bp[3];
	bp += 4;
	len -= 4;
	if (vflag > 1)
		printf("\n\t");
	else
		printf(" ");
	printf("genid %u", genid);
	if (vflag < 2)
		return (0);

	while ((len > 0) && (bp < ep)) {
		TCHECK2(bp[0], 4);
		printf("\n\tneighbor %s", ipaddr_string(bp));
		bp += 4; len -= 4;
	}
	return (0);
trunc:
	return (-1);
}

static int
print_neighbors(register const u_char *bp, register const u_char *ep,
    register u_int len)
{
	const u_char *laddr;
	register u_char metric;
	register u_char thresh;
	register int ncount;

	while (len > 0 && bp < ep) {
		TCHECK2(bp[0], 7);
		laddr = bp;
		bp += 4;
		metric = *bp++;
		thresh = *bp++;
		ncount = *bp++;
		len -= 7;
		while (--ncount >= 0) {
			TCHECK2(bp[0], 4);
			printf(" [%s ->", ipaddr_string(laddr));
			printf(" %s, (%d/%d)]",
				   ipaddr_string(bp), metric, thresh);
			bp += 4;
			len -= 4;
		}
	}
	return (0);
trunc:
	return (-1);
}

static int
print_neighbors2(register const u_char *bp, register const u_char *ep,
    register u_int len)
{
	const u_char *laddr;
	register u_char metric, thresh, flags;
	register int ncount;

	printf(" (v %d.%d):",
	       (int)target_level & 0xff,
	       (int)(target_level >> 8) & 0xff);

	while (len > 0 && bp < ep) {
		TCHECK2(bp[0], 8);
		laddr = bp;
		bp += 4;
		metric = *bp++;
		thresh = *bp++;
		flags = *bp++;
		ncount = *bp++;
		len -= 8;
		while (--ncount >= 0 && (len >= 4) && (bp + 4) <= ep) {
			printf(" [%s -> ", ipaddr_string(laddr));
			printf("%s (%d/%d", ipaddr_string(bp),
				     metric, thresh);
			if (flags & DVMRP_NF_TUNNEL)
				printf("/tunnel");
			if (flags & DVMRP_NF_SRCRT)
				printf("/srcrt");
			if (flags & DVMRP_NF_QUERIER)
				printf("/querier");
			if (flags & DVMRP_NF_DISABLED)
				printf("/disabled");
			if (flags & DVMRP_NF_DOWN)
				printf("/down");
			printf(")]");
			bp += 4;
			len -= 4;
		}
		if (ncount != -1) {
			printf(" [|]");
			return (0);
		}
	}
	return (0);
trunc:
	return (-1);
}

static int
print_prune(register const u_char *bp)
{
	TCHECK2(bp[0], 12);
	printf(" src %s grp %s", ipaddr_string(bp), ipaddr_string(bp + 4));
	bp += 8;
	(void)printf(" timer ");
	relts_print(EXTRACT_32BITS(bp));
	return (0);
trunc:
	return (-1);
}

static int
print_graft(register const u_char *bp)
{
	TCHECK2(bp[0], 8);
	printf(" src %s grp %s", ipaddr_string(bp), ipaddr_string(bp + 4));
	return (0);
trunc:
	return (-1);
}

static int
print_graft_ack(register const u_char *bp)
{
	TCHECK2(bp[0], 8);
	printf(" src %s grp %s", ipaddr_string(bp), ipaddr_string(bp + 4));
	return (0);
trunc:
	return (-1);
}
