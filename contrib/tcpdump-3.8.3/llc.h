/*
 * Copyright (c) 1993, 1994, 1997
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
 *
 * @(#) $Header: /tcpdump/master/tcpdump/llc.h,v 1.16 2002/12/11 07:13:54 guy Exp $ (LBL)
 */

/*
 * This stuff should come from a system header file, but there's no
 * obviously portable way to do that and it's not really going
 * to change from system to system.
 */

/*
 * A somewhat abstracted view of the LLC header
 */

struct llc {
	u_int8_t dsap;
	u_int8_t ssap;
	union {
		u_int8_t u_ctl;
		u_int16_t is_ctl;
		struct {
			u_int8_t snap_ui;
			u_int8_t snap_pi[5];
		} snap;
		struct {
			u_int8_t snap_ui;
			u_int8_t snap_orgcode[3];
			u_int8_t snap_ethertype[2];
		} snap_ether;
	} ctl;
};

#define	llcui		ctl.snap.snap_ui
#define	llcpi		ctl.snap.snap_pi
#define	llc_orgcode	ctl.snap_ether.snap_orgcode
#define	llc_ethertype	ctl.snap_ether.snap_ethertype
#define	llcis		ctl.is_ctl
#define	llcu		ctl.u_ctl

#define	LLC_U_FMT	3
#define	LLC_GSAP	1
#define LLC_S_FMT	1

#define	LLC_U_POLL	0x10
#define	LLC_IS_POLL	0x0100
#define	LLC_XID_FI	0x81

#define	LLC_U_CMD(u)	((u) & 0xef)
#define	LLC_UI		0x03
#define	LLC_UA		0x63
#define	LLC_DISC	0x43
#define	LLC_DM		0x0f
#define	LLC_SABME	0x6f
#define	LLC_TEST	0xe3
#define	LLC_XID		0xaf
#define	LLC_FRMR	0x87

#define	LLC_S_CMD(is)	(((is) >> 1) & 0x03)
#define	LLC_RR		0x0001
#define	LLC_RNR		0x0005
#define	LLC_REJ		0x0009

#define LLC_IS_NR(is)	(((is) >> 9) & 0x7f)
#define LLC_I_NS(is)	(((is) >> 1) & 0x7f)

#ifndef LLCSAP_NULL
#define	LLCSAP_NULL		0x00
#endif
#ifndef LLCSAP_GLOBAL
#define	LLCSAP_GLOBAL		0xff
#endif
#ifndef LLCSAP_8021B_I
#define	LLCSAP_8021B_I		0x02
#endif
#ifndef LLCSAP_8021B_G
#define	LLCSAP_8021B_G		0x03
#endif
#ifndef LLCSAP_IP
#define	LLCSAP_IP		0x06
#endif
#ifndef LLCSAP_PROWAYNM
#define	LLCSAP_PROWAYNM		0x0e
#endif
#ifndef LLCSAP_8021D
#define	LLCSAP_8021D		0x42
#endif
#ifndef LLCSAP_RS511
#define	LLCSAP_RS511		0x4e
#endif
#ifndef LLCSAP_ISO8208
#define	LLCSAP_ISO8208		0x7e
#endif
#ifndef LLCSAP_PROWAY
#define	LLCSAP_PROWAY		0x8e
#endif
#ifndef LLCSAP_SNAP
#define	LLCSAP_SNAP		0xaa
#endif
#ifndef LLCSAP_IPX
#define LLCSAP_IPX		0xe0
#endif
#ifndef LLCSAP_NETBEUI
#define LLCSAP_NETBEUI		0xf0
#endif
#ifndef LLCSAP_ISONS
#define	LLCSAP_ISONS		0xfe
#endif

#define	OUI_ENCAP_ETHER	0x000000	/* encapsulated Ethernet */
#define	OUI_CISCO	0x00000c	/* Cisco protocols */
#define	OUI_CISCO_90	0x0000f8	/* Cisco bridging */
#define OUI_RFC2684	0x0080c2	/* RFC 2684 bridged Ethernet */
#define	OUI_APPLETALK	0x080007	/* Appletalk */

/*
 * PIDs for use with OUI_CISCO.
 */
#define	PID_CISCO_CDP		0x2000	/* Cisco Discovery Protocol */

/*
 * PIDs for use with OUI_RFC2684.
 */
#define PID_RFC2684_ETH_FCS	0x0001	/* Ethernet, with FCS */
#define PID_RFC2684_ETH_NOFCS	0x0007	/* Ethernet, without FCS */
#define PID_RFC2684_802_4_FCS	0x0002	/* 802.4, with FCS */
#define PID_RFC2684_802_4_NOFCS	0x0008	/* 802.4, without FCS */
#define PID_RFC2684_802_5_FCS	0x0003	/* 802.5, with FCS */
#define PID_RFC2684_802_5_NOFCS	0x0009	/* 802.5, without FCS */
#define PID_RFC2684_FDDI_FCS	0x0004	/* FDDI, with FCS */
#define PID_RFC2684_FDDI_NOFCS	0x000a	/* FDDI, without FCS */
#define PID_RFC2684_802_6_FCS	0x0005	/* 802.6, with FCS */
#define PID_RFC2684_802_6_NOFCS	0x000b	/* 802.6, without FCS */
#define PID_RFC2684_BPDU	0x000e	/* BPDUs */

