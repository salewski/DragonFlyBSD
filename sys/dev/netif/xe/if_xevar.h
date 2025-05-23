/*-
 * Copyright (c) 1998, 1999 Scott Mitchell
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: if_xe.c,v 1.20 1999/06/13 19:17:40 scott Exp $
 * $FreeBSD: src/sys/dev/xe/if_xevar.h,v 1.1.2.1 2000/06/01 01:23:53 imp Exp $
 * $DragonFly: src/sys/dev/netif/xe/if_xevar.h,v 1.3 2004/09/15 01:23:00 joerg Exp $
 */
#ifndef DEV_XE_IF_XEDEV_H
#define DEV_XE_IF_XEDEV_H

/*
 * One of these structures per allocated device
 */
struct xe_softc {
  struct arpcom arpcom;
  struct ifmedia ifmedia;
  struct ifmib_iso_8802_3 mibdata;
  struct callout xe_timer;
  struct ifnet *ifp;
  struct ifmedia *ifm;
  char *card_type;	/* Card model name */
  char *vendor;		/* Card manufacturer */
  device_t dev;		/* Device */
  bus_space_tag_t bst;	/* Bus space tag for card */
  bus_space_handle_t bsh; /* Bus space handle for card */
  void *intrhand;
  struct resource *irq_res;
  int irq_rid;
  struct resource *port_res;
  int port_rid;
  int srev;     	/* Silicon revision */
  int tx_queued;	/* Packets currently waiting to transmit */
  int tx_tpr;		/* Last value of TPR reg on card */
  int tx_collisions;	/* Collisions since last successful send */
  int tx_timeouts;	/* Count of transmit timeouts */
  int autoneg_status;	/* Autonegotiation progress state */
  int media;		/* Private media word */
  u_char version;	/* Bonding Version register from card */
  u_char modem;		/* 1 = Card has a modem */
  u_char ce2;		/* 1 = Card has CE2 silicon */
  u_char mohawk;      	/* 1 = Card has Mohawk (CE3) silicon */
  u_char dingo;    	/* 1 = Card has Dingo (CEM56) silicon */
  u_char phy_ok;	/* 1 = MII-compliant PHY found and initialised */
  u_char gone;		/* 1 = Card bailed out */
};

/*
 * For accessing card registers
 */
#define XE_INB(r)         bus_space_read_1(scp->bst, scp->bsh, (r))
#define XE_INW(r)         bus_space_read_2(scp->bst, scp->bsh, (r))
#define XE_OUTB(r, b)     bus_space_write_1(scp->bst, scp->bsh, (r), (b))
#define XE_OUTW(r, w)     bus_space_write_2(scp->bst, scp->bsh, (r), (w))
#define XE_SELECT_PAGE(p) XE_OUTB(XE_PR, (p))

/*
 * Horrid stuff for accessing CIS tuples
 */
#define CISTPL_BUFSIZE		512
#define CISTPL_TYPE(tpl)	bus_space_read_1(bst, bsh, tpl + 0)
#define CISTPL_LEN(tpl)		bus_space_read_1(bst, bsh, tpl + 2)
#define CISTPL_DATA(tpl,pos)	bus_space_read_1(bst, bsh, tpl+ 4 + ((pos)<<1))

#endif /* DEV_XE_IF_XEVAR_H */
