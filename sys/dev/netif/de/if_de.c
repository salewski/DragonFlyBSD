/*	$NetBSD: if_de.c,v 1.86 1999/06/01 19:17:59 thorpej Exp $	*/

/* $FreeBSD: src/sys/pci/if_de.c,v 1.123.2.4 2000/08/04 23:25:09 peter Exp $ */
/* $DragonFly: src/sys/dev/netif/de/if_de.c,v 1.29 2005/02/21 20:48:13 joerg Exp $ */

/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 * Id: if_de.c,v 1.94 1997/07/03 16:55:07 thomas Exp
 *
 */

/*
 * DEC 21040 PCI Ethernet Controller
 *
 * Written by Matt Thomas
 * BPF support code stolen directly from if_ec.c
 *
 *   This driver supports the DEC DE435 or any other PCI
 *   board which support 21040, 21041, or 21140 (mostly).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <machine/clock.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include "opt_inet.h"
#include "opt_ipx.h"

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef IPX
#include <netproto/ipx/ipx.h>
#include <netproto/ipx/ipx_if.h>
#endif

#ifdef NS
#include <netproto/ns/ns.h>
#include <netproto/ns/ns_if.h>
#endif

#include <vm/vm.h>

#include <net/if_var.h>
#include <vm/pmap.h>
#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>
#include <bus/pci/dc21040reg.h>

/*
 * Intel CPUs should use I/O mapped access.
 */
#if defined(__i386__)
#define	TULIP_IOMAPPED
#endif

#define	TULIP_HZ	10

#include "if_devar.h"

static tulip_softc_t *tulips[TULIP_MAX_DEVICES];

/*
 * This module supports
 *	the DEC 21040 PCI Ethernet Controller.
 *	the DEC 21041 PCI Ethernet Controller.
 *	the DEC 21140 PCI Fast Ethernet Controller.
 */
static void	tulip_mii_autonegotiate(tulip_softc_t *, u_int);
static void	tulip_intr_shared(void *);
static void	tulip_intr_normal(void *);
static void	tulip_init(tulip_softc_t *);
static void	tulip_reset(tulip_softc_t *);
static void	tulip_ifstart(struct ifnet *);
static struct mbuf *tulip_txput(tulip_softc_t *, struct mbuf *);
static void	tulip_txput_setup(tulip_softc_t *);
static void	tulip_rx_intr(tulip_softc_t *);
static void	tulip_addr_filter(tulip_softc_t *);
static u_int	tulip_mii_readreg(tulip_softc_t *, u_int, u_int);
static void	tulip_mii_writereg(tulip_softc_t *, u_int, u_int, u_int);
static int	tulip_mii_map_abilities(tulip_softc_t * const sc, unsigned abilities);
static tulip_media_t tulip_mii_phy_readspecific(tulip_softc_t *);
static int	tulip_srom_decode(tulip_softc_t *);
static int	tulip_ifmedia_change(struct ifnet *);
static void	tulip_ifmedia_status(struct ifnet *, struct ifmediareq *);
/* static void tulip_21140_map_media(tulip_softc_t *sc); */

static void
tulip_timeout_callback(void *arg)
{
    tulip_softc_t *sc = arg;
    int s = splimp();

    sc->tulip_flags &= ~TULIP_TIMEOUTPENDING;
    sc->tulip_probe_timeout -= 1000 / TULIP_HZ;
    (sc->tulip_boardsw->bd_media_poll)(sc, TULIP_MEDIAPOLL_TIMER);
    splx(s);
}

static void
tulip_timeout(tulip_softc_t *sc)
{
    if (sc->tulip_flags & TULIP_TIMEOUTPENDING)
	return;
    sc->tulip_flags |= TULIP_TIMEOUTPENDING;
    callout_reset(&sc->tulip_timer, (hz + TULIP_HZ / 2) / TULIP_HZ,
    	    tulip_timeout_callback, sc);
}

static int
tulip_txprobe(tulip_softc_t *sc)
{
    struct mbuf *m;
    /*
     * Before we are sure this is the right media we need
     * to send a small packet to make sure there's carrier.
     * Strangely, BNC and AUI will "see" receive data if
     * either is connected so the transmit is the only way
     * to verify the connectivity.
     */
    MGETHDR(m, MB_DONTWAIT, MT_DATA);
    if (m == NULL)
	return 0;
    /*
     * Construct a LLC TEST message which will point to ourselves.
     */
    bcopy(sc->tulip_enaddr, mtod(m, struct ether_header *)->ether_dhost, 6);
    bcopy(sc->tulip_enaddr, mtod(m, struct ether_header *)->ether_shost, 6);
    mtod(m, struct ether_header *)->ether_type = htons(3);
    mtod(m, unsigned char *)[14] = 0;
    mtod(m, unsigned char *)[15] = 0;
    mtod(m, unsigned char *)[16] = 0xE3;	/* LLC Class1 TEST (no poll) */
    m->m_len = m->m_pkthdr.len = sizeof(struct ether_header) + 3;
    /*
     * send it!
     */
    sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
    sc->tulip_intrmask |= TULIP_STS_TXINTR;
    sc->tulip_flags |= TULIP_TXPROBE_ACTIVE;
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
    if ((m = tulip_txput(sc, m)) != NULL)
	m_freem(m);
    sc->tulip_probe.probe_txprobes++;
    return 1;
}

#ifdef BIG_PACKET
#define TULIP_SIAGEN_WATCHDOG	(sc->tulip_if.if_mtu > ETHERMTU ? TULIP_WATCHDOG_RXDISABLE|TULIP_WATCHDOG_TXDISABLE : 0)
#else
#define	TULIP_SIAGEN_WATCHDOG	0
#endif

static void
tulip_media_set(tulip_softc_t *sc, tulip_media_t media)
{
    const tulip_media_info_t *mi = sc->tulip_mediums[media];

    if (mi == NULL)
	return;

    /*
     * If we are switching media, make sure we don't think there's
     * any stale RX activity
     */
    sc->tulip_flags &= ~TULIP_RXACT;
    if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	TULIP_CSR_WRITE(sc, csr_sia_tx_rx,        mi->mi_sia_tx_rx);
	if (sc->tulip_features & TULIP_HAVE_SIAGP) {
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_gp_control|mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	    DELAY(50);
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_gp_data|mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	} else {
	    TULIP_CSR_WRITE(sc, csr_sia_general,  mi->mi_sia_general|TULIP_SIAGEN_WATCHDOG);
	}
	TULIP_CSR_WRITE(sc, csr_sia_connectivity, mi->mi_sia_connectivity);
    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR) {
#define	TULIP_GPR_CMDBITS	(TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION|TULIP_CMD_SCRAMBLER|TULIP_CMD_TXTHRSHLDCTL)
	/*
	 * If the cmdmode bits don't match the currently operating mode,
	 * set the cmdmode appropriately and reset the chip.
	 */
	if (((mi->mi_cmdmode ^ TULIP_CSR_READ(sc, csr_command)) & TULIP_GPR_CMDBITS) != 0) {
	    sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
	    sc->tulip_cmdmode |= mi->mi_cmdmode;
	    tulip_reset(sc);
	}
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET|sc->tulip_gpinit);
	DELAY(10);
	TULIP_CSR_WRITE(sc, csr_gp, (u_int8_t) mi->mi_gpdata);
    } else if (mi->mi_type == TULIP_MEDIAINFO_SYM) {
	/*
	 * If the cmdmode bits don't match the currently operating mode,
	 * set the cmdmode appropriately and reset the chip.
	 */
	if (((mi->mi_cmdmode ^ TULIP_CSR_READ(sc, csr_command)) & TULIP_GPR_CMDBITS) != 0) {
	    sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
	    sc->tulip_cmdmode |= mi->mi_cmdmode;
	    tulip_reset(sc);
	}
	TULIP_CSR_WRITE(sc, csr_sia_general, mi->mi_gpcontrol);
	TULIP_CSR_WRITE(sc, csr_sia_general, mi->mi_gpdata);
    } else if (mi->mi_type == TULIP_MEDIAINFO_MII
	       && sc->tulip_probe_state != TULIP_PROBE_INACTIVE) {
	int idx;
	if (sc->tulip_features & TULIP_HAVE_SIAGP) {
	    const u_int8_t *dp;
	    dp = &sc->tulip_rombuf[mi->mi_reset_offset];
	    for (idx = 0; idx < mi->mi_reset_length; idx++, dp += 2) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_sia_general, (dp[0] + 256 * dp[1]) << 16);
	    }
	    sc->tulip_phyaddr = mi->mi_phyaddr;
	    dp = &sc->tulip_rombuf[mi->mi_gpr_offset];
	    for (idx = 0; idx < mi->mi_gpr_length; idx++, dp += 2) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_sia_general, (dp[0] + 256 * dp[1]) << 16);
	    }
	} else {
	    for (idx = 0; idx < mi->mi_reset_length; idx++) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_reset_offset + idx]);
	    }
	    sc->tulip_phyaddr = mi->mi_phyaddr;
	    for (idx = 0; idx < mi->mi_gpr_length; idx++) {
		DELAY(10);
		TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_gpr_offset + idx]);
	    }
	}
	if (sc->tulip_flags & TULIP_TRYNWAY) {
	    tulip_mii_autonegotiate(sc, sc->tulip_phyaddr);
	} else if ((sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	    u_int32_t data = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_CONTROL);
	    data &= ~(PHYCTL_SELECT_100MB|PHYCTL_FULL_DUPLEX|PHYCTL_AUTONEG_ENABLE);
	    sc->tulip_flags &= ~TULIP_DIDNWAY;
	    if (TULIP_IS_MEDIA_FD(media))
		data |= PHYCTL_FULL_DUPLEX;
	    if (TULIP_IS_MEDIA_100MB(media))
		data |= PHYCTL_SELECT_100MB;
	    tulip_mii_writereg(sc, sc->tulip_phyaddr, PHYREG_CONTROL, data);
	}
    }
}

static void
tulip_linkup(tulip_softc_t *sc, tulip_media_t media)
{
    if ((sc->tulip_flags & TULIP_LINKUP) == 0)
	sc->tulip_flags |= TULIP_PRINTLINKUP;
    sc->tulip_flags |= TULIP_LINKUP;
    sc->tulip_if.if_flags &= ~IFF_OACTIVE;
#if 0 /* XXX how does with work with ifmedia? */
    if ((sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	if (sc->tulip_if.if_flags & IFF_FULLDUPLEX) {
	    if (TULIP_CAN_MEDIA_FD(media)
		    && sc->tulip_mediums[TULIP_FD_MEDIA_OF(media)] != NULL)
		media = TULIP_FD_MEDIA_OF(media);
	} else {
	    if (TULIP_IS_MEDIA_FD(media)
		    && sc->tulip_mediums[TULIP_HD_MEDIA_OF(media)] != NULL)
		media = TULIP_HD_MEDIA_OF(media);
	}
    }
#endif
    if (sc->tulip_media != media) {
	sc->tulip_media = media;
	sc->tulip_flags |= TULIP_PRINTMEDIA;
	if (TULIP_IS_MEDIA_FD(sc->tulip_media)) {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	} else if (sc->tulip_chipid != TULIP_21041 || (sc->tulip_flags & TULIP_DIDNWAY) == 0) {
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	}
    }
    /*
     * We could set probe_timeout to 0 but setting to 3000 puts this
     * in one central place and the only matters is tulip_link is
     * followed by a tulip_timeout.  Therefore setting it should not
     * result in aberrant behavour.
     */
    sc->tulip_probe_timeout = 3000;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_TRYNWAY);
    if (sc->tulip_flags & TULIP_INRESET) {
	tulip_media_set(sc, sc->tulip_media);
    } else if (sc->tulip_probe_media != sc->tulip_media) {
	/*
	 * No reason to change media if we have the right media.
	 */
	tulip_reset(sc);
    }
    tulip_init(sc);
}

static void
tulip_media_print(tulip_softc_t *sc)
{
    if ((sc->tulip_flags & TULIP_LINKUP) == 0)
	return;
    if (sc->tulip_flags & TULIP_PRINTMEDIA) {
	printf("%s%d: enabling %s port\n",
	       sc->tulip_name, sc->tulip_unit,
	       tulip_mediums[sc->tulip_media]);
	sc->tulip_flags &= ~(TULIP_PRINTMEDIA|TULIP_PRINTLINKUP);
    } else if (sc->tulip_flags & TULIP_PRINTLINKUP) {
	printf("%s%d: link up\n", sc->tulip_name, sc->tulip_unit);
	sc->tulip_flags &= ~TULIP_PRINTLINKUP;
    }
}

#if defined(TULIP_DO_GPR_SENSE)
static tulip_media_t
tulip_21140_gpr_media_sense(tulip_softc_t *sc)
{
    tulip_media_t maybe_media = TULIP_MEDIA_UNKNOWN;
    tulip_media_t last_media = TULIP_MEDIA_UNKNOWN;
    tulip_media_t media;

    /*
     * If one of the media blocks contained a default media flag,
     * use that.
     */
    for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	const tulip_media_info_t *mi;
	/*
	 * Media is not supported (or is full-duplex).
	 */
	if ((mi = sc->tulip_mediums[media]) == NULL || TULIP_IS_MEDIA_FD(media))
	    continue;
	if (mi->mi_type != TULIP_MEDIAINFO_GPR)
	    continue;

	/*
	 * Remember the media is this is the "default" media.
	 */
	if (mi->mi_default && maybe_media == TULIP_MEDIA_UNKNOWN)
	    maybe_media = media;

	/*
	 * No activity mask?  Can't see if it is active if there's no mask.
	 */
	if (mi->mi_actmask == 0)
	    continue;

	/*
	 * Does the activity data match?
	 */
	if ((TULIP_CSR_READ(sc, csr_gp) & mi->mi_actmask) != mi->mi_actdata)
	    continue;

	/*
	 * It does!  If this is the first media we detected, then 
	 * remember this media.  If isn't the first, then there were
	 * multiple matches which we equate to no match (since we don't
	 * which to select (if any).
	 */
	if (last_media == TULIP_MEDIA_UNKNOWN) {
	    last_media = media;
	} else if (last_media != media) {
	    last_media = TULIP_MEDIA_UNKNOWN;
	}
    }
    return (last_media != TULIP_MEDIA_UNKNOWN) ? last_media : maybe_media;
}
#endif /* TULIP_DO_GPR_SENSE */

static tulip_link_status_t
tulip_media_link_monitor(tulip_softc_t *sc)
{
    const tulip_media_info_t *mi = sc->tulip_mediums[sc->tulip_media];
    tulip_link_status_t linkup = TULIP_LINK_DOWN;

    if (mi == NULL) {
#if defined(DIAGNOSTIC)
	panic("tulip_media_link_monitor: %s: botch at line %d\n",
	      tulip_mediums[sc->tulip_media],__LINE__);
#endif
	return TULIP_LINK_UNKNOWN;
    }


    /*
     * Have we seen some packets?  If so, the link must be good.
     */
    if ((sc->tulip_flags & (TULIP_RXACT|TULIP_LINKUP)) == (TULIP_RXACT|TULIP_LINKUP)) {
	sc->tulip_flags &= ~TULIP_RXACT;
	sc->tulip_probe_timeout = 3000;
	return TULIP_LINK_UP;
    }

    sc->tulip_flags &= ~TULIP_RXACT;
    if (mi->mi_type == TULIP_MEDIAINFO_MII) {
	u_int32_t status;
	/*
	 * Read the PHY status register.
	 */
	status = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_STATUS);
	if (status & PHYSTS_AUTONEG_DONE) {
	    /*
	     * If the PHY has completed autonegotiation, see the if the
	     * remote systems abilities have changed.  If so, upgrade or
	     * downgrade as appropriate.
	     */
	    u_int32_t abilities = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_AUTONEG_ABILITIES);
	    abilities = (abilities << 6) & status;
	    if (abilities != sc->tulip_abilities) {
		if (tulip_mii_map_abilities(sc, abilities)) {
		    tulip_linkup(sc, sc->tulip_probe_media);
		    return TULIP_LINK_UP;
		}
		/*
		 * if we had selected media because of autonegotiation,
		 * we need to probe for the new media.
		 */
		sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		if (sc->tulip_flags & TULIP_DIDNWAY)
		    return TULIP_LINK_DOWN;
	    }
	}
	/*
	 * The link is now up.  If was down, say its back up.
	 */
	if ((status & (PHYSTS_LINK_UP|PHYSTS_REMOTE_FAULT)) == PHYSTS_LINK_UP)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR) {
	/*
	 * No activity sensor?  Assume all's well.
	 */
	if (mi->mi_actmask == 0)
	    return TULIP_LINK_UNKNOWN;
	/*
	 * Does the activity data match?
	 */
	if ((TULIP_CSR_READ(sc, csr_gp) & mi->mi_actmask) == mi->mi_actdata)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
	/*
	 * Assume non TP ok for now.
	 */
	if (!TULIP_IS_MEDIA_TP(sc->tulip_media))
	    return TULIP_LINK_UNKNOWN;
	if ((TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL) == 0)
	    linkup = TULIP_LINK_UP;
    } else if (mi->mi_type == TULIP_MEDIAINFO_SYM) {
	return TULIP_LINK_UNKNOWN;
    }
    /*
     * We will wait for 3 seconds until the link goes into suspect mode.
     */
    if (sc->tulip_flags & TULIP_LINKUP) {
	if (linkup == TULIP_LINK_UP)
	    sc->tulip_probe_timeout = 3000;
	if (sc->tulip_probe_timeout > 0)
	    return TULIP_LINK_UP;

	sc->tulip_flags &= ~TULIP_LINKUP;
	printf("%s%d: link down: cable problem?\n", sc->tulip_name, sc->tulip_unit);
    }
    return TULIP_LINK_DOWN;
}

static void
tulip_media_poll(tulip_softc_t *sc, tulip_mediapoll_event_t event)
{
    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE
	    && event == TULIP_MEDIAPOLL_TIMER) {
	switch (tulip_media_link_monitor(sc)) {
	    case TULIP_LINK_DOWN: {
		/*
		 * Link Monitor failed.  Probe for new media.
		 */
		event = TULIP_MEDIAPOLL_LINKFAIL;
		break;
	    }
	    case TULIP_LINK_UP: {
		/*
		 * Check again soon.
		 */
		tulip_timeout(sc);
		return;
	    }
	    case TULIP_LINK_UNKNOWN: {
		/*
		 * We can't tell so don't bother.
		 */
		return;
	    }
	}
    }

    if (event == TULIP_MEDIAPOLL_LINKFAIL) {
	if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE) {
	    if (TULIP_DO_AUTOSENSE(sc)) {
		sc->tulip_media = TULIP_MEDIA_UNKNOWN;
		if (sc->tulip_if.if_flags & IFF_UP)
		    tulip_reset(sc);	/* restart probe */
	    }
	    return;
	}
    }

    if (event == TULIP_MEDIAPOLL_START) {
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	if (sc->tulip_probe_state != TULIP_PROBE_INACTIVE)
	    return;
	sc->tulip_probe_mediamask = 0;
	sc->tulip_probe_passes = 0;
	/*
	 * If the SROM contained an explicit media to use, use it.
	 */
	sc->tulip_cmdmode &= ~(TULIP_CMD_RXRUN|TULIP_CMD_FULLDUPLEX);
	sc->tulip_flags |= TULIP_TRYNWAY|TULIP_PROBE1STPASS;
	sc->tulip_flags &= ~(TULIP_DIDNWAY|TULIP_PRINTMEDIA|TULIP_PRINTLINKUP);
	/*
	 * connidx is defaulted to a media_unknown type.
	 */
	sc->tulip_probe_media = tulip_srom_conninfo[sc->tulip_connidx].sc_media;
	if (sc->tulip_probe_media != TULIP_MEDIA_UNKNOWN) {
	    tulip_linkup(sc, sc->tulip_probe_media);
	    tulip_timeout(sc);
	    return;
	}

	if (sc->tulip_features & TULIP_HAVE_GPR) {
	    sc->tulip_probe_state = TULIP_PROBE_GPRTEST;
	    sc->tulip_probe_timeout = 2000;
	} else {
	    sc->tulip_probe_media = TULIP_MEDIA_MAX;
	    sc->tulip_probe_timeout = 0;
	    sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	}
    }

    /*
     * Ignore txprobe failures or spurious callbacks.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED
	    && sc->tulip_probe_state != TULIP_PROBE_MEDIATEST) {
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
	return;
    }

    /*
     * If we really transmitted a packet, then that's the media we'll use.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_OK || event == TULIP_MEDIAPOLL_LINKPASS) {
	if (event == TULIP_MEDIAPOLL_LINKPASS) {
	    /* XXX Check media status just to be sure */
	    sc->tulip_probe_media = TULIP_MEDIA_10BASET;
	}
	tulip_linkup(sc, sc->tulip_probe_media);
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state == TULIP_PROBE_GPRTEST) {
#if defined(TULIP_DO_GPR_SENSE)
	/*
	 * Check for media via the general purpose register.
	 *
	 * Try to sense the media via the GPR.  If the same value
	 * occurs 3 times in a row then just use that.
	 */
	if (sc->tulip_probe_timeout > 0) {
	    tulip_media_t new_probe_media = tulip_21140_gpr_media_sense(sc);
	    if (new_probe_media != TULIP_MEDIA_UNKNOWN) {
		if (new_probe_media == sc->tulip_probe_media) {
		    if (--sc->tulip_probe_count == 0)
			tulip_linkup(sc, sc->tulip_probe_media);
		} else {
		    sc->tulip_probe_count = 10;
		}
	    }
	    sc->tulip_probe_media = new_probe_media;
	    tulip_timeout(sc);
	    return;
	}
#endif /* TULIP_DO_GPR_SENSE */
	/*
	 * Brute force.  We cycle through each of the media types
	 * and try to transmit a packet.
	 */
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe_media = TULIP_MEDIA_MAX;
	sc->tulip_probe_timeout = 0;
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state != TULIP_PROBE_MEDIATEST
	   && (sc->tulip_features & TULIP_HAVE_MII)) {
	tulip_media_t old_media = sc->tulip_probe_media;
	tulip_mii_autonegotiate(sc, sc->tulip_phyaddr);
	switch (sc->tulip_probe_state) {
	    case TULIP_PROBE_FAILED:
	    case TULIP_PROBE_MEDIATEST: {
		/*
		 * Try the next media.
		 */
		sc->tulip_probe_mediamask |= sc->tulip_mediums[sc->tulip_probe_media]->mi_mediamask;
		sc->tulip_probe_timeout = 0;
#ifdef notyet
		if (sc->tulip_probe_state == TULIP_PROBE_FAILED)
		    break;
		if (sc->tulip_probe_media != tulip_mii_phy_readspecific(sc))
		    break;
		sc->tulip_probe_timeout = TULIP_IS_MEDIA_TP(sc->tulip_probe_media) ? 2500 : 300;
#endif
		break;
	    }
	    case TULIP_PROBE_PHYAUTONEG: {
		return;
	    }
	    case TULIP_PROBE_INACTIVE: {
		/*
		 * Only probe if we autonegotiated a media that hasn't failed.
		 */
		sc->tulip_probe_timeout = 0;
		if (sc->tulip_probe_mediamask & TULIP_BIT(sc->tulip_probe_media)) {
		    sc->tulip_probe_media = old_media;
		    break;
		}
		tulip_linkup(sc, sc->tulip_probe_media);
		tulip_timeout(sc);
		return;
	    }
	    default: {
#if defined(DIAGNOSTIC)
		panic("tulip_media_poll: botch at line %d\n", __LINE__);
#endif
		break;
	    }
	}
    }

    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED) {
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
	return;
    }

    /*
     * switch to another media if we tried this one enough.
     */
    if (/* event == TULIP_MEDIAPOLL_TXPROBE_FAILED || */ sc->tulip_probe_timeout <= 0) {
	/*
	 * Find the next media type to check for.  Full Duplex
	 * types are not allowed.
	 */
	do {
	    sc->tulip_probe_media -= 1;
	    if (sc->tulip_probe_media == TULIP_MEDIA_UNKNOWN) {
		if (++sc->tulip_probe_passes == 3) {
		    printf("%s%d: autosense failed: cable problem?\n",
			   sc->tulip_name, sc->tulip_unit);
		    if ((sc->tulip_if.if_flags & IFF_UP) == 0) {
			sc->tulip_if.if_flags &= ~IFF_RUNNING;
			sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
			return;
		    }
		}
		sc->tulip_flags ^= TULIP_TRYNWAY;	/* XXX */
		sc->tulip_probe_mediamask = 0;
		sc->tulip_probe_media = TULIP_MEDIA_MAX - 1;
	    }
	} while (sc->tulip_mediums[sc->tulip_probe_media] == NULL
		 || (sc->tulip_probe_mediamask & TULIP_BIT(sc->tulip_probe_media))
		 || TULIP_IS_MEDIA_FD(sc->tulip_probe_media));

	sc->tulip_probe_timeout = TULIP_IS_MEDIA_TP(sc->tulip_probe_media) ? 2500 : 1000;
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe.probe_txprobes = 0;
	tulip_reset(sc);
	tulip_media_set(sc, sc->tulip_probe_media);
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
    }
    tulip_timeout(sc);

    /*
     * If this is hanging off a phy, we know are doing NWAY and we have
     * forced the phy to a specific speed.  Wait for link up before
     * before sending a packet.
     */
    switch (sc->tulip_mediums[sc->tulip_probe_media]->mi_type) {
	case TULIP_MEDIAINFO_MII: {
	    if (sc->tulip_probe_media != tulip_mii_phy_readspecific(sc))
		return;
	    break;
	}
	case TULIP_MEDIAINFO_SIA: {
	    if (TULIP_IS_MEDIA_TP(sc->tulip_probe_media)) {
		if (TULIP_CSR_READ(sc, csr_sia_status) & TULIP_SIASTS_LINKFAIL)
		    return;
		tulip_linkup(sc, sc->tulip_probe_media);
#ifdef notyet
		if (sc->tulip_features & TULIP_HAVE_MII)
		    tulip_timeout(sc);
#endif
		return;
	    }
	    break;
	}
	case TULIP_MEDIAINFO_RESET:
	case TULIP_MEDIAINFO_SYM:
	case TULIP_MEDIAINFO_NONE:
	case TULIP_MEDIAINFO_GPR: {
	    break;
	}
    }
    /*
     * Try to send a packet.
     */
    tulip_txprobe(sc);
}

static void
tulip_media_select(tulip_softc_t *sc)
{
    if (sc->tulip_features & TULIP_HAVE_GPR) {
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET|sc->tulip_gpinit);
	DELAY(10);
	TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_gpdata);
    }
    /*
     * If this board has no media, just return
     */
    if (sc->tulip_features & TULIP_HAVE_NOMEDIA)
	return;

    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	(*sc->tulip_boardsw->bd_media_poll)(sc, TULIP_MEDIAPOLL_START);
    } else {
	tulip_media_set(sc, sc->tulip_media);
    }
}

static void
tulip_21040_mediainfo_init(tulip_softc_t *sc, tulip_media_t media)
{
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_THRSHLD160
	|TULIP_CMD_BACKOFFCTR;
    sc->tulip_if.if_baudrate = 10000000;

    if (media == TULIP_MEDIA_10BASET || media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[0], 21040, 10BASET);
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[1], 21040, 10BASET_FD);
	sc->tulip_intrmask |= TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
    }

    if (media == TULIP_MEDIA_AUIBNC || media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[2], 21040, AUIBNC);
    }

    if (media == TULIP_MEDIA_UNKNOWN) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &sc->tulip_mediainfo[3], 21040, EXTSIA);
    }
}

static void
tulip_21040_media_probe(tulip_softc_t *sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_UNKNOWN);
}

static void
tulip_21040_10baset_only_media_probe(tulip_softc_t *sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_10BASET);
    tulip_media_set(sc, TULIP_MEDIA_10BASET);
    sc->tulip_media = TULIP_MEDIA_10BASET;
}

static void
tulip_21040_10baset_only_media_select(tulip_softc_t *sc)
{
    sc->tulip_flags |= TULIP_LINKUP;
    if (sc->tulip_media == TULIP_MEDIA_10BASET_FD) {
	sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	sc->tulip_flags &= ~TULIP_SQETEST;
    } else {
	sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	sc->tulip_flags |= TULIP_SQETEST;
    }
    tulip_media_set(sc, sc->tulip_media);
}

static void
tulip_21040_auibnc_only_media_probe(tulip_softc_t *sc)
{
    tulip_21040_mediainfo_init(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_flags |= TULIP_SQETEST|TULIP_LINKUP;
    tulip_media_set(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_media = TULIP_MEDIA_AUIBNC;
}

static void
tulip_21040_auibnc_only_media_select(tulip_softc_t *sc)
{
    tulip_media_set(sc, TULIP_MEDIA_AUIBNC);
    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
}

static const tulip_boardsw_t tulip_21040_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_media_probe,
    tulip_media_select,
    tulip_media_poll,
};

static const tulip_boardsw_t tulip_21040_10baset_only_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_10baset_only_media_probe,
    tulip_21040_10baset_only_media_select,
    NULL,
};

static const tulip_boardsw_t tulip_21040_auibnc_only_boardsw = {
    TULIP_21040_GENERIC,
    tulip_21040_auibnc_only_media_probe,
    tulip_21040_auibnc_only_media_select,
    NULL,
};

static void
tulip_21041_mediainfo_init(tulip_softc_t *sc)
{
    tulip_media_info_t *mi = sc->tulip_mediainfo;

#ifdef notyet
    if (sc->tulip_revinfo >= 0x20) {
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041P2, 10BASET);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041P2, 10BASET_FD);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041P2, AUI);
	TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041P2, BNC);
	return;
    }
#endif
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[0], 21041, 10BASET);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[1], 21041, 10BASET_FD);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[2], 21041, AUI);
    TULIP_MEDIAINFO_SIA_INIT(sc, &mi[3], 21041, BNC);
}

static void
tulip_21041_media_probe(tulip_softc_t *sc)
{
    sc->tulip_if.if_baudrate = 10000000;
    sc->tulip_cmdmode |= TULIP_CMD_CAPTREFFCT|TULIP_CMD_ENHCAPTEFFCT
	|TULIP_CMD_THRSHLD160|TULIP_CMD_BACKOFFCTR;
    sc->tulip_intrmask |= TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
    tulip_21041_mediainfo_init(sc);
}

static void
tulip_21041_media_poll(tulip_softc_t *sc, tulip_mediapoll_event_t event)
{
    uint32_t sia_status;

    if (event == TULIP_MEDIAPOLL_LINKFAIL) {
	if (sc->tulip_probe_state != TULIP_PROBE_INACTIVE
		|| !TULIP_DO_AUTOSENSE(sc))
	    return;
	sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	tulip_reset(sc);	/* start probe */
	return;
    }

    /*
     * If we've been been asked to start a poll or link change interrupt
     * restart the probe (and reset the tulip to a known state).
     */
    if (event == TULIP_MEDIAPOLL_START) {
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	sc->tulip_cmdmode &= ~(TULIP_CMD_FULLDUPLEX|TULIP_CMD_RXRUN);
#ifdef notyet
	if (sc->tulip_revinfo >= 0x20) {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX;
	    sc->tulip_flags |= TULIP_DIDNWAY;
	}
#endif
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	sc->tulip_probe_media = TULIP_MEDIA_10BASET;
	sc->tulip_probe_timeout = TULIP_21041_PROBE_10BASET_TIMEOUT;
	tulip_media_set(sc, TULIP_MEDIA_10BASET);
	tulip_timeout(sc);
	return;
    }

    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE)
	return;

    if (event == TULIP_MEDIAPOLL_TXPROBE_OK) {
	tulip_linkup(sc, sc->tulip_probe_media);
	return;
    }

    sia_status = TULIP_CSR_READ(sc, csr_sia_status);
    TULIP_CSR_WRITE(sc, csr_sia_status, sia_status);
    if ((sia_status & TULIP_SIASTS_LINKFAIL) == 0) {
	if (sc->tulip_revinfo >= 0x20) {
	    if (sia_status & (PHYSTS_10BASET_FD << (16 - 6)))
		sc->tulip_probe_media = TULIP_MEDIA_10BASET_FD;
	}
	/*
	 * If the link has passed LinkPass, 10baseT is the
	 * proper media to use.
	 */
	tulip_linkup(sc, sc->tulip_probe_media);
	return;
    }

    /*
     * wait for up to 2.4 seconds for the link to reach pass state.
     * Only then start scanning the other media for activity.
     * choose media with receive activity over those without.
     */
    if (sc->tulip_probe_media == TULIP_MEDIA_10BASET) {
	if (event != TULIP_MEDIAPOLL_TIMER)
	    return;
	if (sc->tulip_probe_timeout > 0
		&& (sia_status & TULIP_SIASTS_OTHERRXACTIVITY) == 0) {
	    tulip_timeout(sc);
	    return;
	}
	sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	sc->tulip_flags |= TULIP_WANTRXACT;
	if (sia_status & TULIP_SIASTS_OTHERRXACTIVITY) {
	    sc->tulip_probe_media = TULIP_MEDIA_BNC;
	} else {
	    sc->tulip_probe_media = TULIP_MEDIA_AUI;
	}
	tulip_media_set(sc, sc->tulip_probe_media);
	tulip_timeout(sc);
	return;
    }

    /*
     * If we failed, clear the txprobe active flag.
     */
    if (event == TULIP_MEDIAPOLL_TXPROBE_FAILED)
	sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;


    if (event == TULIP_MEDIAPOLL_TIMER) {
	/*
	 * If we've received something, then that's our link!
	 */
	if (sc->tulip_flags & TULIP_RXACT) {
	    tulip_linkup(sc, sc->tulip_probe_media);
	    return;
	}
	/*
	 * if no txprobe active  
	 */
	if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0
		&& ((sc->tulip_flags & TULIP_WANTRXACT) == 0
		    || (sia_status & TULIP_SIASTS_RXACTIVITY))) {
	    sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	    tulip_txprobe(sc);
	    tulip_timeout(sc);
	    return;
	}
	/*
	 * Take 2 passes through before deciding to not
	 * wait for receive activity.  Then take another
	 * two passes before spitting out a warning.
	 */
	if (sc->tulip_probe_timeout <= 0) {
	    if (sc->tulip_flags & TULIP_WANTRXACT) {
		sc->tulip_flags &= ~TULIP_WANTRXACT;
		sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
	    } else {
		printf("%s%d: autosense failed: cable problem?\n",
		       sc->tulip_name, sc->tulip_unit);
		if ((sc->tulip_if.if_flags & IFF_UP) == 0) {
		    sc->tulip_if.if_flags &= ~IFF_RUNNING;
		    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
		    return;
		}
	    }
	}
    }
    
    /*
     * Since this media failed to probe, try the other one.
     */
    sc->tulip_probe_timeout = TULIP_21041_PROBE_AUIBNC_TIMEOUT;
    if (sc->tulip_probe_media == TULIP_MEDIA_AUI) {
	sc->tulip_probe_media = TULIP_MEDIA_BNC;
    } else {
	sc->tulip_probe_media = TULIP_MEDIA_AUI;
    }
    tulip_media_set(sc, sc->tulip_probe_media);
    sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
    tulip_timeout(sc);
}

static const tulip_boardsw_t tulip_21041_boardsw = {
    TULIP_21041_GENERIC,
    tulip_21041_media_probe,
    tulip_media_select,
    tulip_21041_media_poll
};

static const tulip_phy_attr_t tulip_mii_phy_attrlist[] = {
    { 0x20005c00, 0,		/* 08-00-17 */
      {
	{ 0x19, 0x0040, 0x0040 },	/* 10TX */
	{ 0x19, 0x0040, 0x0000 },	/* 100TX */
      },
    },
    { 0x0281F400, 0,		/* 00-A0-7D */
      {
	{ 0x12, 0x0010, 0x0000 },	/* 10T */
	{ },				/* 100TX */
	{ 0x12, 0x0010, 0x0010 },	/* 100T4 */
	{ 0x12, 0x0008, 0x0008 },	/* FULL_DUPLEX */
      },
    },
#if 0
    { 0x0015F420, 0,	/* 00-A0-7D */
      {
	{ 0x12, 0x0010, 0x0000 },	/* 10T */
	{ },				/* 100TX */
	{ 0x12, 0x0010, 0x0010 },	/* 100T4 */
	{ 0x12, 0x0008, 0x0008 },	/* FULL_DUPLEX */
      },
    },
#endif
    { 0x0281F400, 0,		/* 00-A0-BE */
      {
	{ 0x11, 0x8000, 0x0000 },	/* 10T */
	{ 0x11, 0x8000, 0x8000 },	/* 100TX */
	{ },				/* 100T4 */
	{ 0x11, 0x4000, 0x4000 },	/* FULL_DUPLEX */
      },
    },
    { 0 }
};

static tulip_media_t
tulip_mii_phy_readspecific(tulip_softc_t *sc)
{
    const tulip_phy_attr_t *attr;
    u_int16_t data;
    u_int32_t id;
    unsigned idx = 0;
    static const tulip_media_t table[] = {
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET,
	TULIP_MEDIA_100BASETX,
	TULIP_MEDIA_100BASET4,
	TULIP_MEDIA_UNKNOWN,
	TULIP_MEDIA_10BASET_FD,
	TULIP_MEDIA_100BASETX_FD,
	TULIP_MEDIA_UNKNOWN
    };

    /*
     * Don't read phy specific registers if link is not up.
     */
    data = tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_STATUS);
    if ((data & (PHYSTS_LINK_UP|PHYSTS_EXTENDED_REGS)) != (PHYSTS_LINK_UP|PHYSTS_EXTENDED_REGS))
	return TULIP_MEDIA_UNKNOWN;

    id = (tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_IDLOW) << 16) |
	tulip_mii_readreg(sc, sc->tulip_phyaddr, PHYREG_IDHIGH);
    for (attr = tulip_mii_phy_attrlist;; attr++) {
	if (attr->attr_id == 0)
	    return TULIP_MEDIA_UNKNOWN;
	if ((id & ~0x0F) == attr->attr_id)
	    break;
    }

    if (attr->attr_modes[PHY_MODE_100TX].pm_regno) {
	const tulip_phy_modedata_t *pm = &attr->attr_modes[PHY_MODE_100TX];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 2;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_100T4].pm_regno) {
	const tulip_phy_modedata_t *pm = &attr->attr_modes[PHY_MODE_100T4];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 3;
    }
    if (idx == 0 && attr->attr_modes[PHY_MODE_10T].pm_regno) {
	const tulip_phy_modedata_t *pm = &attr->attr_modes[PHY_MODE_10T];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	if ((data & pm->pm_mask) == pm->pm_value)
	    idx = 1;
    } 
    if (idx != 0 && attr->attr_modes[PHY_MODE_FULLDUPLEX].pm_regno) {
	const tulip_phy_modedata_t *pm = &attr->attr_modes[PHY_MODE_FULLDUPLEX];
	data = tulip_mii_readreg(sc, sc->tulip_phyaddr, pm->pm_regno);
	idx += ((data & pm->pm_mask) == pm->pm_value ? 4 : 0);
    }
    return table[idx];
}

static u_int
tulip_mii_get_phyaddr(tulip_softc_t *sc, u_int offset)
{
    u_int phyaddr;

    for (phyaddr = 1; phyaddr < 32; phyaddr++) {
	u_int status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	if (status == 0 || status == 0xFFFF || status < PHYSTS_10BASET)
	    continue;
	if (offset == 0)
	    return phyaddr;
	offset--;
    }
    if (offset == 0) {
	u_int status = tulip_mii_readreg(sc, 0, PHYREG_STATUS);
	if (status == 0 || status == 0xFFFF || status < PHYSTS_10BASET)
	    return TULIP_MII_NOPHY;
	return 0;
    }
    return TULIP_MII_NOPHY;
}

static int
tulip_mii_map_abilities(tulip_softc_t *sc, u_int abilities)
{
    sc->tulip_abilities = abilities;
    if (abilities & PHYSTS_100BASETX_FD) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASETX_FD;
    } else if (abilities & PHYSTS_100BASET4) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASET4;
    } else if (abilities & PHYSTS_100BASETX) {
	sc->tulip_probe_media = TULIP_MEDIA_100BASETX;
    } else if (abilities & PHYSTS_10BASET_FD) {
	sc->tulip_probe_media = TULIP_MEDIA_10BASET_FD;
    } else if (abilities & PHYSTS_10BASET) {
	sc->tulip_probe_media = TULIP_MEDIA_10BASET;
    } else {
	sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
	return 0;
    }
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    return 1;
}

static void
tulip_mii_autonegotiate(tulip_softc_t *sc, u_int phyaddr)
{
    switch (sc->tulip_probe_state) {
        case TULIP_PROBE_MEDIATEST:
        case TULIP_PROBE_INACTIVE: {
	    sc->tulip_flags |= TULIP_DIDNWAY;
	    tulip_mii_writereg(sc, phyaddr, PHYREG_CONTROL, PHYCTL_RESET);
	    sc->tulip_probe_timeout = 3000;
	    sc->tulip_intrmask |= TULIP_STS_ABNRMLINTR|TULIP_STS_NORMALINTR;
	    sc->tulip_probe_state = TULIP_PROBE_PHYRESET;
	    /* FALL THROUGH */
	}
        case TULIP_PROBE_PHYRESET: {
	    uint32_t status;
	    uint32_t data = tulip_mii_readreg(sc, phyaddr, PHYREG_CONTROL);
	    if (data & PHYCTL_RESET) {
		if (sc->tulip_probe_timeout > 0) {
		    tulip_timeout(sc);
		    return;
		}
		printf("%s%d(phy%d): error: reset of PHY never completed!\n",
			   sc->tulip_name, sc->tulip_unit, phyaddr);
		sc->tulip_flags &= ~TULIP_TXPROBE_ACTIVE;
		sc->tulip_probe_state = TULIP_PROBE_FAILED;
		sc->tulip_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
		return;
	    }
	    status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	    if ((status & PHYSTS_CAN_AUTONEG) == 0) {
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    if (tulip_mii_readreg(sc, phyaddr, PHYREG_AUTONEG_ADVERTISEMENT) != ((status >> 6) | 0x01))
		tulip_mii_writereg(sc, phyaddr, PHYREG_AUTONEG_ADVERTISEMENT, (status >> 6) | 0x01);
	    tulip_mii_writereg(sc, phyaddr, PHYREG_CONTROL, data|PHYCTL_AUTONEG_RESTART|PHYCTL_AUTONEG_ENABLE);
	    data = tulip_mii_readreg(sc, phyaddr, PHYREG_CONTROL);
	    sc->tulip_probe_state = TULIP_PROBE_PHYAUTONEG;
	    sc->tulip_probe_timeout = 3000;
	    /* FALL THROUGH */
	}
        case TULIP_PROBE_PHYAUTONEG: {
	    u_int32_t status = tulip_mii_readreg(sc, phyaddr, PHYREG_STATUS);
	    u_int32_t data;
	    if ((status & PHYSTS_AUTONEG_DONE) == 0) {
		if (sc->tulip_probe_timeout > 0) {
		    tulip_timeout(sc);
		    return;
		}
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		sc->tulip_probe_state = TULIP_PROBE_MEDIATEST;
		return;
	    }
	    data = tulip_mii_readreg(sc, phyaddr, PHYREG_AUTONEG_ABILITIES);
	    data = (data << 6) & status;
	    if (!tulip_mii_map_abilities(sc, data))
		sc->tulip_flags &= ~TULIP_DIDNWAY;
	    return;
	}
	default: {
#if defined(DIAGNOSTIC)
	    panic("tulip_media_poll: botch at line %d\n", __LINE__);
#endif
	    break;
	}
    }
}

static void
tulip_2114x_media_preset(tulip_softc_t *sc)
{
    const tulip_media_info_t *mi = NULL;
    tulip_media_t media = sc->tulip_media;

    if (sc->tulip_probe_state == TULIP_PROBE_INACTIVE)
	media = sc->tulip_media;
    else
	media = sc->tulip_probe_media;
    
    sc->tulip_cmdmode &= ~TULIP_CMD_PORTSELECT;
    sc->tulip_flags &= ~TULIP_SQETEST;
    if (media != TULIP_MEDIA_UNKNOWN && media != TULIP_MEDIA_MAX) {
	    mi = sc->tulip_mediums[media];
	    if (mi->mi_type == TULIP_MEDIAINFO_MII) {
		sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT;
	    } else if (mi->mi_type == TULIP_MEDIAINFO_GPR
		       || mi->mi_type == TULIP_MEDIAINFO_SYM) {
		sc->tulip_cmdmode &= ~TULIP_GPR_CMDBITS;
		sc->tulip_cmdmode |= mi->mi_cmdmode;
	    } else if (mi->mi_type == TULIP_MEDIAINFO_SIA) {
		TULIP_CSR_WRITE(sc, csr_sia_connectivity, TULIP_SIACONN_RESET);
	    }
    }
    switch (media) {
	case TULIP_MEDIA_BNC:
	case TULIP_MEDIA_AUI:
	case TULIP_MEDIA_10BASET: {
	    sc->tulip_cmdmode &= ~TULIP_CMD_FULLDUPLEX;
	    sc->tulip_cmdmode |= TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 10000000;
	    sc->tulip_flags |= TULIP_SQETEST;
	    break;
	}
	case TULIP_MEDIA_10BASET_FD: {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 10000000;
	    break;
	}
	case TULIP_MEDIA_100BASEFX:
	case TULIP_MEDIA_100BASET4:
	case TULIP_MEDIA_100BASETX: {
	    sc->tulip_cmdmode &= ~(TULIP_CMD_FULLDUPLEX|TULIP_CMD_TXTHRSHLDCTL);
	    sc->tulip_cmdmode |= TULIP_CMD_PORTSELECT;
	    sc->tulip_if.if_baudrate = 100000000;
	    break;
	}
	case TULIP_MEDIA_100BASEFX_FD:
	case TULIP_MEDIA_100BASETX_FD: {
	    sc->tulip_cmdmode |= TULIP_CMD_FULLDUPLEX|TULIP_CMD_PORTSELECT;
	    sc->tulip_cmdmode &= ~TULIP_CMD_TXTHRSHLDCTL;
	    sc->tulip_if.if_baudrate = 100000000;
	    break;
	}
	default: {
	    break;
	}
    }
    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
}

/*
 ********************************************************************
 *  Start of 21140/21140A support which does not use the MII interface 
 */

static void
tulip_null_media_poll(tulip_softc_t *sc, tulip_mediapoll_event_t event)
{
#if defined(DIAGNOSTIC)
    printf("%s%d: botch(media_poll) at line %d\n",
	   sc->tulip_name, sc->tulip_unit, __LINE__);
#endif
}

static void
tulip_21140_mediainit(tulip_softc_t *sc, tulip_media_info_t *mip,
		      tulip_media_t media, u_int gpdata, u_int cmdmode)
{
    sc->tulip_mediums[media] = mip;
    mip->mi_type = TULIP_MEDIAINFO_GPR;
    mip->mi_cmdmode = cmdmode;
    mip->mi_gpdata = gpdata;
}

static void
tulip_21140_evalboard_media_probe(tulip_softc_t *sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;

    sc->tulip_gpinit = TULIP_GP_EB_PINS;
    sc->tulip_gpdata = TULIP_GP_EB_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    if ((TULIP_CSR_READ(sc, csr_gp) & TULIP_GP_EB_OK100) != 0) {
	sc->tulip_media = TULIP_MEDIA_10BASET;
    } else {
	sc->tulip_media = TULIP_MEDIA_100BASETX;
    }
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_EB_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_eb_boardsw = {
    TULIP_21140_DEC_EB,
    tulip_21140_evalboard_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_21140_accton_media_probe(tulip_softc_t *sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    u_int gpdata;

    sc->tulip_gpinit = TULIP_GP_EB_PINS;
    sc->tulip_gpdata = TULIP_GP_EB_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EB_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);
    DELAY(1000000);
    gpdata = TULIP_CSR_READ(sc, csr_gp);
    if ((gpdata & TULIP_GP_EN1207_UTP_INIT) == 0) {
	sc->tulip_media = TULIP_MEDIA_10BASET;
    } else {
	if ((gpdata & TULIP_GP_EN1207_BNC_INIT) == 0) {
		sc->tulip_media = TULIP_MEDIA_BNC;
        } else {
		sc->tulip_media = TULIP_MEDIA_100BASETX;
        }
    }
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_BNC,
			  TULIP_GP_EN1207_BNC_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_EN1207_UTP_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_EN1207_UTP_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_EN1207_100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_EN1207_100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_accton_boardsw = {
    TULIP_21140_EN1207,
    tulip_21140_accton_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_21140_smc9332_media_probe(tulip_softc_t *sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    int idx, cnt = 0;

    TULIP_CSR_WRITE(sc, csr_command, TULIP_CMD_PORTSELECT|TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */
    TULIP_CSR_WRITE(sc, csr_command, TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    sc->tulip_gpinit = TULIP_GP_SMC_9332_PINS;
    sc->tulip_gpdata = TULIP_GP_SMC_9332_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_PINS|TULIP_GP_PINSET);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_SMC_9332_INIT);
    DELAY(200000);
    for (idx = 1000; idx > 0; idx--) {
	u_int32_t csr = TULIP_CSR_READ(sc, csr_gp);
	if ((csr & (TULIP_GP_SMC_9332_OK10|TULIP_GP_SMC_9332_OK100)) == (TULIP_GP_SMC_9332_OK10|TULIP_GP_SMC_9332_OK100)) {
	    if (++cnt > 100)
		break;
	} else if ((csr & TULIP_GP_SMC_9332_OK10) == 0) {
	    break;
	} else {
	    cnt = 0;
	}
	DELAY(1000);
    }
    sc->tulip_media = cnt > 100 ? TULIP_MEDIA_100BASETX : TULIP_MEDIA_10BASET;
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_SMC_9332_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
}
 
static const tulip_boardsw_t tulip_21140_smc9332_boardsw = {
    TULIP_21140_SMC_9332,
    tulip_21140_smc9332_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_21140_cogent_em100_media_probe(tulip_softc_t *sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    uint32_t cmdmode = TULIP_CSR_READ(sc, csr_command);

    sc->tulip_gpinit = TULIP_GP_EM100_PINS;
    sc->tulip_gpdata = TULIP_GP_EM100_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_EM100_INIT);

    cmdmode = TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION|TULIP_CMD_MUSTBEONE;
    cmdmode &= ~(TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_SCRAMBLER);
    if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100FX_ID) {
	TULIP_CSR_WRITE(sc, csr_command, cmdmode);
	sc->tulip_media = TULIP_MEDIA_100BASEFX;

	tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASEFX,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION);
	tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASEFX_FD,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_FULLDUPLEX);
    } else {
	TULIP_CSR_WRITE(sc, csr_command, cmdmode|TULIP_CMD_SCRAMBLER);
	sc->tulip_media = TULIP_MEDIA_100BASETX;
	tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
	tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_EM100_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
    }
}

static const tulip_boardsw_t tulip_21140_cogent_em100_boardsw = {
    TULIP_21140_COGENT_EM100,
    tulip_21140_cogent_em100_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset
};

static void
tulip_21140_znyx_zx34x_media_probe(tulip_softc_t *sc)
{
    tulip_media_info_t *mip = sc->tulip_mediainfo;
    int cnt10 = 0, cnt100 = 0, idx;

    sc->tulip_gpinit = TULIP_GP_ZX34X_PINS;
    sc->tulip_gpdata = TULIP_GP_ZX34X_INIT;
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_PINS);
    TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ZX34X_INIT);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) | TULIP_CMD_PORTSELECT |
	TULIP_CMD_PCSFUNCTION | TULIP_CMD_SCRAMBLER | TULIP_CMD_MUSTBEONE);
    TULIP_CSR_WRITE(sc, csr_command,
	TULIP_CSR_READ(sc, csr_command) & ~TULIP_CMD_TXTHRSHLDCTL);

    DELAY(200000);
    for (idx = 1000; idx > 0; idx--) {
	uint32_t csr = TULIP_CSR_READ(sc, csr_gp);
	if ((csr & (TULIP_GP_ZX34X_LNKFAIL|TULIP_GP_ZX34X_SYMDET|TULIP_GP_ZX34X_SIGDET)) == (TULIP_GP_ZX34X_LNKFAIL|TULIP_GP_ZX34X_SYMDET|TULIP_GP_ZX34X_SIGDET)) {
	    if (++cnt100 > 100)
		break;
	} else if ((csr & TULIP_GP_ZX34X_LNKFAIL) == 0) {
	    if (++cnt10 > 100)
		break;
	} else {
	    cnt10 = 0;
	    cnt100 = 0;
	}
	DELAY(1000);
    }
    sc->tulip_media = cnt100 > 100 ? TULIP_MEDIA_100BASETX : TULIP_MEDIA_10BASET;
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_TXTHRSHLDCTL);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_10BASET_FD,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_TXTHRSHLDCTL|TULIP_CMD_FULLDUPLEX);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER);
    tulip_21140_mediainit(sc, mip++, TULIP_MEDIA_100BASETX_FD,
			  TULIP_GP_ZX34X_INIT,
			  TULIP_CMD_PORTSELECT|TULIP_CMD_PCSFUNCTION
			      |TULIP_CMD_SCRAMBLER|TULIP_CMD_FULLDUPLEX);
}

static const tulip_boardsw_t tulip_21140_znyx_zx34x_boardsw = {
    TULIP_21140_ZNYX_ZX34X,
    tulip_21140_znyx_zx34x_media_probe,
    tulip_media_select,
    tulip_null_media_poll,
    tulip_2114x_media_preset,
};

static void
tulip_2114x_media_probe(tulip_softc_t *sc)
{
    sc->tulip_cmdmode |= TULIP_CMD_MUSTBEONE | TULIP_CMD_BACKOFFCTR | 
	TULIP_CMD_THRSHLD72;
}

static const tulip_boardsw_t tulip_2114x_isv_boardsw = {
    TULIP_21140_ISV,
    tulip_2114x_media_probe,
    tulip_media_select,
    tulip_media_poll,
    tulip_2114x_media_preset,
};

/*
 * ******** END of chip-specific handlers. ***********
 */

/*
 * Code the read the SROM and MII bit streams (I2C)
 */
#define	EMIT    do {					\
	TULIP_CSR_WRITE(sc, csr_srom_mii, csr);	\
	DELAY(1);					\
} while (0)

static void
tulip_srom_idle(tulip_softc_t *sc)
{
    u_int bit, csr;
    
    csr  = SROMSEL ; EMIT;
    csr  = SROMSEL | SROMRD; EMIT;  
    csr ^= SROMCS; EMIT;
    csr ^= SROMCLKON; EMIT;

    /*
     * Write 25 cycles of 0 which will force the SROM to be idle.
     */
    for (bit = 3 + SROM_BITWIDTH + 16; bit > 0; bit--) {
        csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
    }
    csr ^= SROMCLKOFF; EMIT;
    csr ^= SROMCS; EMIT;
    csr  = 0; EMIT;
}

     
static void
tulip_srom_read(tulip_softc_t *sc)
{   
    const u_int bitwidth = SROM_BITWIDTH;
    const u_int cmdmask = (SROMCMD_RD << bitwidth);
    const u_int msb = 1 << (bitwidth + 3 - 1);
    u_int idx, lastidx = (1 << bitwidth) - 1;

    tulip_srom_idle(sc);

    for (idx = 0; idx <= lastidx; idx++) {
        u_int lastbit, data, bits, bit, csr;
	csr  = SROMSEL ;	        EMIT;
        csr  = SROMSEL | SROMRD;        EMIT;
        csr ^= SROMCSON;                EMIT;
        csr ^=            SROMCLKON;    EMIT;
    
        lastbit = 0;
        for (bits = idx|cmdmask, bit = bitwidth + 3; bit > 0; bit--, bits <<= 1) {
            u_int thisbit = bits & msb;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
            if (thisbit != lastbit) {
                csr ^= SROMDOUT; EMIT;  /* clock low; invert data */
            } else {
		EMIT;
	    }
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
            lastbit = thisbit;
        }
        csr ^= SROMCLKOFF; EMIT;

        for (data = 0, bits = 0; bits < 16; bits++) {
            data <<= 1;
            csr ^= SROMCLKON; EMIT;     /* clock high; data valid */ 
            data |= TULIP_CSR_READ(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
            csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
        }
	sc->tulip_rombuf[idx*2] = data & 0xFF;
	sc->tulip_rombuf[idx*2+1] = data >> 8;
	csr  = SROMSEL | SROMRD; EMIT;
	csr  = 0; EMIT;
    }
    tulip_srom_idle(sc);
}

#define	MII_EMIT    do { 				\
	TULIP_CSR_WRITE(sc, csr_srom_mii, csr);		\
	DELAY(1);					\
} while (0)

static void
tulip_mii_writebits(tulip_softc_t *sc, u_int data, u_int bits)
{
    u_int csr, msb, lastbit;

    msb = 1 << (bits - 1);
    csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    lastbit = (csr & MII_DOUT) ? msb : 0;
    csr |= MII_WR; MII_EMIT;  		/* clock low; assert write */

    for (; bits > 0; bits--, data <<= 1) {
	const unsigned thisbit = data & msb;
	if (thisbit != lastbit) {
	    csr ^= MII_DOUT; MII_EMIT;  /* clock low; invert data */
	}
	csr ^= MII_CLKON; MII_EMIT;     /* clock high; data valid */
	lastbit = thisbit;
	csr ^= MII_CLKOFF; MII_EMIT;    /* clock low; data not valid */
    }
}

static void
tulip_mii_turnaround(tulip_softc_t *sc, u_int cmd)
{
    u_int csr;

    csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);

    if (cmd == MII_WRCMD) {
	csr |= MII_DOUT; MII_EMIT;	/* clock low; change data */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
	csr ^= MII_DOUT; MII_EMIT;	/* clock low; change data */
    } else {
	csr |= MII_RD; MII_EMIT;	/* clock low; switch to read */
    }
    csr ^= MII_CLKON; MII_EMIT;		/* clock high; data valid */
    csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
}

static u_int
tulip_mii_readbits(tulip_softc_t *sc)
{
    u_int csr, data, idx;

    csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);

    for (idx = 0, data = 0; idx < 16; idx++) {
	data <<= 1;	/* this is NOOP on the first pass through */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	if (TULIP_CSR_READ(sc, csr_srom_mii) & MII_DIN)
	    data |= 1;
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
    }
    csr ^= MII_RD; MII_EMIT;		/* clock low; turn off read */

    return data;
}

static u_int
tulip_mii_readreg(tulip_softc_t *sc, u_int devaddr, u_int regno)
{
    u_int csr;
    u_int data;

    csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_writebits(sc, MII_PREAMBLE, 32);
    tulip_mii_writebits(sc, MII_RDCMD, 8);
    tulip_mii_writebits(sc, devaddr, 5);
    tulip_mii_writebits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_RDCMD);

    data = tulip_mii_readbits(sc);
    return data;
}

static void
tulip_mii_writereg(tulip_softc_t *sc, u_int devaddr, u_int regno, u_int data)
{
    u_int csr;

    csr = TULIP_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    tulip_mii_writebits(sc, MII_PREAMBLE, 32);
    tulip_mii_writebits(sc, MII_WRCMD, 8);
    tulip_mii_writebits(sc, devaddr, 5);
    tulip_mii_writebits(sc, regno, 5);
    tulip_mii_turnaround(sc, MII_WRCMD);
    tulip_mii_writebits(sc, data, 16);
}

#define	tulip_mchash(mca)	(ether_crc32_le(mca, 6) & 0x1FF)
#define	tulip_srom_crcok(databuf)	( \
    ((ether_crc32_le(databuf, 126) & 0xFFFFU) ^ 0xFFFFU) == \
     ((databuf)[126] | ((databuf)[127] << 8)))

static void
tulip_identify_dec_nic(tulip_softc_t *sc)
{
    strcpy(sc->tulip_boardid, "DEC ");
#define D0	4
    if (sc->tulip_chipid <= TULIP_21040)
	return;
    if (bcmp(sc->tulip_rombuf + 29, "DE500", 5) == 0
	|| bcmp(sc->tulip_rombuf + 29, "DE450", 5) == 0) {
	bcopy(sc->tulip_rombuf + 29, &sc->tulip_boardid[D0], 8);
	sc->tulip_boardid[D0+8] = ' ';
    }
#undef D0
}

static void
tulip_identify_znyx_nic(tulip_softc_t * const sc)
{
    u_int id = 0;
    strcpy(sc->tulip_boardid, "ZNYX ZX3XX ");
    if (sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A) {
	u_int znyx_ptr;
	sc->tulip_boardid[8] = '4';
	znyx_ptr = sc->tulip_rombuf[124] + 256 * sc->tulip_rombuf[125];
	if (znyx_ptr < 26 || znyx_ptr > 116) {
	    sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    return;
	}
	/* ZX344 = 0010 .. 0013FF
	 */
	if (sc->tulip_rombuf[znyx_ptr] == 0x4A
		&& sc->tulip_rombuf[znyx_ptr + 1] == 0x52
		&& sc->tulip_rombuf[znyx_ptr + 2] == 0x01) {
	    id = sc->tulip_rombuf[znyx_ptr + 5] + 256 * sc->tulip_rombuf[znyx_ptr + 4];
	    if ((id >> 8) == (TULIP_ZNYX_ID_ZX342 >> 8)) {
		sc->tulip_boardid[9] = '2';
		if (id == TULIP_ZNYX_ID_ZX342B) {
		    sc->tulip_boardid[10] = 'B';
		    sc->tulip_boardid[11] = ' ';
		}
		sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    } else if (id == TULIP_ZNYX_ID_ZX344) {
		sc->tulip_boardid[10] = '4';
		sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	    } else if (id == TULIP_ZNYX_ID_ZX345) {
		sc->tulip_boardid[9] = (sc->tulip_rombuf[19] > 1) ? '8' : '5';
	    } else if (id == TULIP_ZNYX_ID_ZX346) {
		sc->tulip_boardid[9] = '6';
	    } else if (id == TULIP_ZNYX_ID_ZX351) {
		sc->tulip_boardid[8] = '5';
		sc->tulip_boardid[9] = '1';
	    }
	}
	if (id == 0) {
	    /*
	     * Assume it's a ZX342...
	     */
	    sc->tulip_boardsw = &tulip_21140_znyx_zx34x_boardsw;
	}
	return;
    }
    sc->tulip_boardid[8] = '1';
    if (sc->tulip_chipid == TULIP_21041) {
	sc->tulip_boardid[10] = '1';
	return;
    }
    if (sc->tulip_rombuf[32] == 0x4A && sc->tulip_rombuf[33] == 0x52) {
	id = sc->tulip_rombuf[37] + 256 * sc->tulip_rombuf[36];
	if (id == TULIP_ZNYX_ID_ZX312T) {
	    sc->tulip_boardid[9] = '2';
	    sc->tulip_boardid[10] = 'T';
	    sc->tulip_boardid[11] = ' ';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	} else if (id == TULIP_ZNYX_ID_ZX314_INTA) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_features |= TULIP_HAVE_SHAREDINTR|TULIP_HAVE_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX314) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_features |= TULIP_HAVE_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX315_INTA) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_features |= TULIP_HAVE_SHAREDINTR|TULIP_HAVE_BASEROM;
	} else if (id == TULIP_ZNYX_ID_ZX315) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_features |= TULIP_HAVE_BASEROM;
	} else {
	    id = 0;
	}
    }		    
    if (id == 0) {
	if ((sc->tulip_enaddr[3] & ~3) == 0xF0 && (sc->tulip_enaddr[5] & 2) == 0) {
	    sc->tulip_boardid[9] = '4';
	    sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
	    sc->tulip_features |= TULIP_HAVE_SHAREDINTR|TULIP_HAVE_BASEROM;
	} else if ((sc->tulip_enaddr[3] & ~3) == 0xF4 && (sc->tulip_enaddr[5] & 1) == 0) {
	    sc->tulip_boardid[9] = '5';
	    sc->tulip_boardsw = &tulip_21040_boardsw;
	    sc->tulip_features |= TULIP_HAVE_SHAREDINTR|TULIP_HAVE_BASEROM;
	} else if ((sc->tulip_enaddr[3] & ~3) == 0xEC) {
	    sc->tulip_boardid[9] = '2';
	    sc->tulip_boardsw = &tulip_21040_boardsw;
	}
    }
}

static void
tulip_identify_smc_nic(tulip_softc_t * const sc)
{
    uint32_t id1, id2, ei;
    int auibnc = 0, utp = 0;
    char *cp;

    strcpy(sc->tulip_boardid, "SMC ");
    if (sc->tulip_chipid == TULIP_21041)
	return;
    if (sc->tulip_chipid != TULIP_21040) {
	if (sc->tulip_boardsw != &tulip_2114x_isv_boardsw) {
	    strcpy(&sc->tulip_boardid[4], "9332DST ");
	    sc->tulip_boardsw = &tulip_21140_smc9332_boardsw;
	} else if (sc->tulip_features & (TULIP_HAVE_BASEROM|TULIP_HAVE_SLAVEDROM)) {
	    strcpy(&sc->tulip_boardid[4], "9334BDT ");
	} else {
	    strcpy(&sc->tulip_boardid[4], "9332BDT ");
	}
	return;
    }
    id1 = sc->tulip_rombuf[0x60] | (sc->tulip_rombuf[0x61] << 8);
    id2 = sc->tulip_rombuf[0x62] | (sc->tulip_rombuf[0x63] << 8);
    ei  = sc->tulip_rombuf[0x66] | (sc->tulip_rombuf[0x67] << 8);

    strcpy(&sc->tulip_boardid[4], "8432");
    cp = &sc->tulip_boardid[8];
    if ((id1 & 1) == 0)
	*cp++ = 'B', auibnc = 1;
    if ((id1 & 0xFF) > 0x32)
	*cp++ = 'T', utp = 1;
    if ((id1 & 0x4000) == 0)
	*cp++ = 'A', auibnc = 1;
    if (id2 == 0x15) {
	sc->tulip_boardid[7] = '4';
	*cp++ = '-';
	*cp++ = 'C';
	*cp++ = 'H';
	*cp++ = (ei ? '2' : '1');
    }
    *cp++ = ' ';
    *cp = '\0';
    if (utp && !auibnc)
	sc->tulip_boardsw = &tulip_21040_10baset_only_boardsw;
    else if (!utp && auibnc)
	sc->tulip_boardsw = &tulip_21040_auibnc_only_boardsw;
}

static void
tulip_identify_cogent_nic(tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "Cogent ");
    if (sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A) {
	if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100TX_ID) {
	    strcat(sc->tulip_boardid, "EM100TX ");
	    sc->tulip_boardsw = &tulip_21140_cogent_em100_boardsw;
#if defined(TULIP_COGENT_EM110TX_ID)
	} else if (sc->tulip_rombuf[32] == TULIP_COGENT_EM110TX_ID) {
	    strcat(sc->tulip_boardid, "EM110TX ");
	    sc->tulip_boardsw = &tulip_21140_cogent_em100_boardsw;
#endif
	} else if (sc->tulip_rombuf[32] == TULIP_COGENT_EM100FX_ID) {
	    strcat(sc->tulip_boardid, "EM100FX ");
	    sc->tulip_boardsw = &tulip_21140_cogent_em100_boardsw;
	}
	/*
	 * Magic number (0x24001109U) is the SubVendor (0x2400) and
	 * SubDevId (0x1109) for the ANA6944TX (EM440TX).
	 */
	if (*(uint32_t *) sc->tulip_rombuf == 0x24001109U
		&& (sc->tulip_features & TULIP_HAVE_BASEROM)) {
	    /*
	     * Cogent (Adaptec) is still mapping all INTs to INTA of
	     * first 21140.  Dumb!  Dumb!
	     */
	    strcat(sc->tulip_boardid, "EM440TX ");
	    sc->tulip_features |= TULIP_HAVE_SHAREDINTR;
	}
    } else if (sc->tulip_chipid == TULIP_21040) {
	sc->tulip_features |= TULIP_HAVE_SHAREDINTR|TULIP_HAVE_BASEROM;
    }
}

static void
tulip_identify_accton_nic(tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "ACCTON ");
    switch (sc->tulip_chipid) {
	case TULIP_21140A:
	    strcat(sc->tulip_boardid, "EN1207 ");
	    if (sc->tulip_boardsw != &tulip_2114x_isv_boardsw)
		sc->tulip_boardsw = &tulip_21140_accton_boardsw;
	    break;
	case TULIP_21140:
	    strcat(sc->tulip_boardid, "EN1207TX ");
	    if (sc->tulip_boardsw != &tulip_2114x_isv_boardsw)
		sc->tulip_boardsw = &tulip_21140_eb_boardsw;
            break;
        case TULIP_21040:
	    strcat(sc->tulip_boardid, "EN1203 ");
            sc->tulip_boardsw = &tulip_21040_boardsw;
            break;
        case TULIP_21041:
	    strcat(sc->tulip_boardid, "EN1203 ");
            sc->tulip_boardsw = &tulip_21041_boardsw;
            break;
	default:
            sc->tulip_boardsw = &tulip_2114x_isv_boardsw;
            break;
    }
}

static void
tulip_identify_asante_nic(tulip_softc_t * const sc)
{
    strcpy(sc->tulip_boardid, "Asante ");
    if ((sc->tulip_chipid == TULIP_21140 || sc->tulip_chipid == TULIP_21140A)
	    && sc->tulip_boardsw != &tulip_2114x_isv_boardsw) {
	tulip_media_info_t *mi = sc->tulip_mediainfo;
	int idx;
	/*
	 * The Asante Fast Ethernet doesn't always ship with a valid
	 * new format SROM.  So if isn't in the new format, we cheat
	 * set it up as if we had.
	 */

	sc->tulip_gpinit = TULIP_GP_ASANTE_PINS;
	sc->tulip_gpdata = 0;

	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ASANTE_PINS|TULIP_GP_PINSET);
	TULIP_CSR_WRITE(sc, csr_gp, TULIP_GP_ASANTE_PHYRESET);
	DELAY(100);
	TULIP_CSR_WRITE(sc, csr_gp, 0);

	mi->mi_type = TULIP_MEDIAINFO_MII;
	mi->mi_gpr_length = 0;
	mi->mi_gpr_offset = 0;
	mi->mi_reset_length = 0;
	mi->mi_reset_offset = 0;;

	mi->mi_phyaddr = TULIP_MII_NOPHY;
	for (idx = 20; idx > 0 && mi->mi_phyaddr == TULIP_MII_NOPHY; idx--) {
	    DELAY(10000);
	    mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, 0);
	}
	if (mi->mi_phyaddr == TULIP_MII_NOPHY) {
	    printf("%s%d: can't find phy 0\n", sc->tulip_name, sc->tulip_unit);
	    return;
	}

	sc->tulip_features |= TULIP_HAVE_MII;
	mi->mi_capabilities  = PHYSTS_10BASET|PHYSTS_10BASET_FD|PHYSTS_100BASETX|PHYSTS_100BASETX_FD;
	mi->mi_advertisement = PHYSTS_10BASET|PHYSTS_10BASET_FD|PHYSTS_100BASETX|PHYSTS_100BASETX_FD;
	mi->mi_full_duplex   = PHYSTS_10BASET_FD|PHYSTS_100BASETX_FD;
	mi->mi_tx_threshold  = PHYSTS_10BASET|PHYSTS_10BASET_FD;
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
	TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);
	mi->mi_phyid = (tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDLOW) << 16) |
	    tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDHIGH);

	sc->tulip_boardsw = &tulip_2114x_isv_boardsw;
    }
}

static void
tulip_identify_compex_nic(tulip_softc_t *sc)
{
    strcpy(sc->tulip_boardid, "COMPEX ");
    if (sc->tulip_chipid == TULIP_21140A) {
	int root_unit;
	tulip_softc_t *root_sc = NULL;

	strcat(sc->tulip_boardid, "400TX/PCI ");
	/*
	 * All 4 chips on these boards share an interrupt.  This code
	 * copied from tulip_read_macaddr.
	 */
	sc->tulip_features |= TULIP_HAVE_SHAREDINTR;
	for (root_unit = sc->tulip_unit - 1; root_unit >= 0; root_unit--) {
	    root_sc = tulips[root_unit];
	    if (root_sc == NULL
		|| !(root_sc->tulip_features & TULIP_HAVE_SLAVEDINTR))
		break;
	    root_sc = NULL;
	}
	if (root_sc != NULL
	    && root_sc->tulip_chipid == sc->tulip_chipid
	    && root_sc->tulip_pci_busno == sc->tulip_pci_busno) {
	    sc->tulip_features |= TULIP_HAVE_SLAVEDINTR;
	    sc->tulip_slaves = root_sc->tulip_slaves;
	    root_sc->tulip_slaves = sc;
	} else if(sc->tulip_features & TULIP_HAVE_SLAVEDINTR) {
	    printf("\nCannot find master device for de%d interrupts",
		   sc->tulip_unit);
	}
    } else {
	strcat(sc->tulip_boardid, "unknown ");
    }
    /*      sc->tulip_boardsw = &tulip_21140_eb_boardsw; */
    return;
}

static int
tulip_srom_decode(tulip_softc_t *sc)
{
    u_int idx1, idx2, idx3;

    const tulip_srom_header_t *shp = (const tulip_srom_header_t *) &sc->tulip_rombuf[0];
    const tulip_srom_adapter_info_t *saip = (const tulip_srom_adapter_info_t *) (shp + 1);
    tulip_srom_media_t srom_media;
    tulip_media_info_t *mi = sc->tulip_mediainfo;
    const uint8_t *dp;
    uint32_t leaf_offset, blocks, data;

    for (idx1 = 0; idx1 < shp->sh_adapter_count; idx1++, saip++) {
	if (shp->sh_adapter_count == 1)
	    break;
	if (saip->sai_device == sc->tulip_pci_devno)
	    break;
    }
    /*
     * Didn't find the right media block for this card.
     */
    if (idx1 == shp->sh_adapter_count)
	return 0;

    /*
     * Save the hardware address.
     */
    bcopy(shp->sh_ieee802_address, sc->tulip_enaddr, 6);
    /*
     * If this is a multiple port card, add the adapter index to the last
     * byte of the hardware address.  (if it isn't multiport, adding 0
     * won't hurt.
     */
    sc->tulip_enaddr[5] += idx1;

    leaf_offset = saip->sai_leaf_offset_lowbyte
	+ saip->sai_leaf_offset_highbyte * 256;
    dp = sc->tulip_rombuf + leaf_offset;
	
    sc->tulip_conntype = (tulip_srom_connection_t) (dp[0] + dp[1] * 256); dp += 2;

    for (idx2 = 0;; idx2++) {
	if (tulip_srom_conninfo[idx2].sc_type == sc->tulip_conntype
	        || tulip_srom_conninfo[idx2].sc_type == TULIP_SROM_CONNTYPE_NOT_USED)
	    break;
    }
    sc->tulip_connidx = idx2;

    if (sc->tulip_chipid == TULIP_21041) {
	blocks = *dp++;
	for (idx2 = 0; idx2 < blocks; idx2++) {
	    tulip_media_t media;
	    data = *dp++;
	    srom_media = (tulip_srom_media_t) (data & 0x3F);
	    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
		if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
		    break;
	    }
	    media = tulip_srom_mediums[idx3].sm_type;
	    if (media != TULIP_MEDIA_UNKNOWN) {
		if (data & TULIP_SROM_21041_EXTENDED) {
		    mi->mi_type = TULIP_MEDIAINFO_SIA;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_sia_connectivity = dp[0] + dp[1] * 256;
		    mi->mi_sia_tx_rx        = dp[2] + dp[3] * 256;
		    mi->mi_sia_general      = dp[4] + dp[5] * 256;
		    mi++;
		} else {
		    switch (media) {
			case TULIP_MEDIA_BNC: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, BNC);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_AUI: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, AUI);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_10BASET: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET);
			    mi++;
			    break;
			}
			case TULIP_MEDIA_10BASET_FD: {
			    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET_FD);
			    mi++;
			    break;
			}
			default: {
			    break;
			}
		    }
		}
	    }
	    if (data & TULIP_SROM_21041_EXTENDED)	
		dp += 6;
	}
#ifdef notdef
	if (blocks == 0) {
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, BNC); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, AUI); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET); mi++;
	    TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21041, 10BASET_FD); mi++;
	}
#endif
    } else {
	unsigned length, type;
	tulip_media_t gp_media = TULIP_MEDIA_UNKNOWN;
	if (sc->tulip_features & TULIP_HAVE_GPR)
	    sc->tulip_gpinit = *dp++;
	blocks = *dp++;
	for (idx2 = 0; idx2 < blocks; idx2++) {
	    const uint8_t *ep;
	    if ((*dp & 0x80) == 0) {
		length = 4;
		type = 0;
	    } else {
		length = (*dp++ & 0x7f) - 1;
		type = *dp++ & 0x3f;
	    }
	    ep = dp + length;
	    switch (type & 0x3f) {
		case 0: {	/* 21140[A] GPR block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t)(dp[0] & 0x3f);
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_GPR;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_gpdata = dp[1];
		    if (media > gp_media && !TULIP_IS_MEDIA_FD(media)) {
			sc->tulip_gpdata = mi->mi_gpdata;
			gp_media = media;
		    }
		    data = dp[2] + dp[3] * 256;
		    mi->mi_cmdmode = TULIP_SROM_2114X_CMDBITS(data);
		    if (data & TULIP_SROM_2114X_NOINDICATOR) {
			mi->mi_actmask = 0;
		    } else {
#if 0
			mi->mi_default = (data & TULIP_SROM_2114X_DEFAULT) != 0;
#endif
			mi->mi_actmask = TULIP_SROM_2114X_BITPOS(data);
			mi->mi_actdata = (data & TULIP_SROM_2114X_POLARITY) ? 0 : mi->mi_actmask;
		    }
		    mi++;
		    break;
		}
		case 1: {	/* 21140[A] MII block */
		    const u_int phyno = *dp++;
		    mi->mi_type = TULIP_MEDIAINFO_MII;
		    mi->mi_gpr_length = *dp++;
		    mi->mi_gpr_offset = dp - sc->tulip_rombuf;
		    dp += mi->mi_gpr_length;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += mi->mi_reset_length;

		    /*
		     * Before we probe for a PHY, use the GPR information
		     * to select it.  If we don't, it may be inaccessible.
		     */
		    TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_gpinit|TULIP_GP_PINSET);
		    for (idx3 = 0; idx3 < mi->mi_reset_length; idx3++) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_reset_offset + idx3]);
		    }
		    sc->tulip_phyaddr = mi->mi_phyaddr;
		    for (idx3 = 0; idx3 < mi->mi_gpr_length; idx3++) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_gp, sc->tulip_rombuf[mi->mi_gpr_offset + idx3]);
		    }

		    /*
		     * At least write something!
		     */
		    if (mi->mi_reset_length == 0 && mi->mi_gpr_length == 0)
			TULIP_CSR_WRITE(sc, csr_gp, 0);

		    mi->mi_phyaddr = TULIP_MII_NOPHY;
		    for (idx3 = 20; idx3 > 0 && mi->mi_phyaddr == TULIP_MII_NOPHY; idx3--) {
			DELAY(10000);
			mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, phyno);
		    }
		    if (mi->mi_phyaddr == TULIP_MII_NOPHY)
			break;
		    sc->tulip_features |= TULIP_HAVE_MII;
		    mi->mi_capabilities  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_advertisement = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_full_duplex   = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_tx_threshold  = dp[0] + dp[1] * 256; dp += 2;
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);
		    mi->mi_phyid = (tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDLOW) << 16) |
			tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDHIGH);
		    mi++;
		    break;
		}
		case 2: {	/* 2114[23] SIA block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t)(dp[0] & 0x3f);
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_SIA;
		    sc->tulip_mediums[media] = mi;
		    if (dp[0] & 0x40) {
			mi->mi_sia_connectivity = dp[1] + dp[2] * 256;
			mi->mi_sia_tx_rx        = dp[3] + dp[4] * 256;
			mi->mi_sia_general      = dp[5] + dp[6] * 256;
			dp += 6;
		    } else {
			switch (media) {
			    case TULIP_MEDIA_BNC: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, BNC);
				break;
			    }
			    case TULIP_MEDIA_AUI: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, AUI);
				break;
			    }
			    case TULIP_MEDIA_10BASET: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, 10BASET);
				sc->tulip_intrmask |= TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
				break;
			    }
			    case TULIP_MEDIA_10BASET_FD: {
				TULIP_MEDIAINFO_SIA_INIT(sc, mi, 21142, 10BASET_FD);
				sc->tulip_intrmask |= TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
				break;
			    }
			    default: {
				goto bad_media;
			    }
			}
		    }
		    mi->mi_sia_gp_control = (dp[1] + dp[2] * 256) << 16;
		    mi->mi_sia_gp_data    = (dp[3] + dp[4] * 256) << 16;
		    mi++;
		  bad_media:
		    break;
		}
		case 3: {	/* 2114[23] MII PHY block */
		    const u_int phyno = *dp++;
		    const uint8_t *dp0;
		    mi->mi_type = TULIP_MEDIAINFO_MII;
		    mi->mi_gpr_length = *dp++;
		    mi->mi_gpr_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_gpr_length;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_reset_length;

		    dp0 = &sc->tulip_rombuf[mi->mi_reset_offset];
		    for (idx3 = 0; idx3 < mi->mi_reset_length; idx3++, dp0 += 2) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_sia_general, (dp0[0] + 256 * dp0[1]) << 16);
		    }
		    sc->tulip_phyaddr = mi->mi_phyaddr;
		    dp0 = &sc->tulip_rombuf[mi->mi_gpr_offset];
		    for (idx3 = 0; idx3 < mi->mi_gpr_length; idx3++, dp0 += 2) {
			DELAY(10);
			TULIP_CSR_WRITE(sc, csr_sia_general, (dp0[0] + 256 * dp0[1]) << 16);
		    }

		    if (mi->mi_reset_length == 0 && mi->mi_gpr_length == 0)
			TULIP_CSR_WRITE(sc, csr_sia_general, 0);

		    mi->mi_phyaddr = TULIP_MII_NOPHY;
		    for (idx3 = 20; idx3 > 0 && mi->mi_phyaddr == TULIP_MII_NOPHY; idx3--) {
			DELAY(10000);
			mi->mi_phyaddr = tulip_mii_get_phyaddr(sc, phyno);
		    }
		    if (mi->mi_phyaddr == TULIP_MII_NOPHY)
			break;
		    sc->tulip_features |= TULIP_HAVE_MII;
		    mi->mi_capabilities  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_advertisement = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_full_duplex   = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_tx_threshold  = dp[0] + dp[1] * 256; dp += 2;
		    mi->mi_mii_interrupt = dp[0] + dp[1] * 256; dp += 2;
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASETX);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 100BASET4);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET_FD);
		    TULIP_MEDIAINFO_ADD_CAPABILITY(sc, mi, 10BASET);
		    mi->mi_phyid = (tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDLOW) << 16) |
			tulip_mii_readreg(sc, mi->mi_phyaddr, PHYREG_IDHIGH);
		    mi++;
		    break;
		}
		case 4: {	/* 21143 SYM block */
		    tulip_media_t media;
		    srom_media = (tulip_srom_media_t) dp[0];
		    for (idx3 = 0; tulip_srom_mediums[idx3].sm_type != TULIP_MEDIA_UNKNOWN; idx3++) {
			if (tulip_srom_mediums[idx3].sm_srom_type == srom_media)
			    break;
		    }
		    media = tulip_srom_mediums[idx3].sm_type;
		    if (media == TULIP_MEDIA_UNKNOWN)
			break;
		    mi->mi_type = TULIP_MEDIAINFO_SYM;
		    sc->tulip_mediums[media] = mi;
		    mi->mi_gpcontrol = (dp[1] + dp[2] * 256) << 16;
		    mi->mi_gpdata    = (dp[3] + dp[4] * 256) << 16;
		    data = dp[5] + dp[6] * 256;
		    mi->mi_cmdmode = TULIP_SROM_2114X_CMDBITS(data);
		    if (data & TULIP_SROM_2114X_NOINDICATOR) {
			mi->mi_actmask = 0;
		    } else {
			mi->mi_default = (data & TULIP_SROM_2114X_DEFAULT) != 0;
			mi->mi_actmask = TULIP_SROM_2114X_BITPOS(data);
			mi->mi_actdata = (data & TULIP_SROM_2114X_POLARITY) ? 0 : mi->mi_actmask;
		    }
		    if (TULIP_IS_MEDIA_TP(media))
			sc->tulip_intrmask |= TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL;
		    mi++;
		    break;
		}
#if 0
		case 5: {	/* 21143 Reset block */
		    mi->mi_type = TULIP_MEDIAINFO_RESET;
		    mi->mi_reset_length = *dp++;
		    mi->mi_reset_offset = dp - sc->tulip_rombuf;
		    dp += 2 * mi->mi_reset_length;
		    mi++;
		    break;
		}
#endif
		default: {
		}
	    }
	    dp = ep;
	}
    }
    return mi - sc->tulip_mediainfo;
}

static const struct {
    void (*vendor_identify_nic)(tulip_softc_t * const sc);
    unsigned char vendor_oui[3];
} tulip_vendors[] = {
    { tulip_identify_dec_nic,		{ 0x08, 0x00, 0x2B } },
    { tulip_identify_dec_nic,		{ 0x00, 0x00, 0xF8 } },
    { tulip_identify_smc_nic,		{ 0x00, 0x00, 0xC0 } },
    { tulip_identify_smc_nic,		{ 0x00, 0xE0, 0x29 } },
    { tulip_identify_znyx_nic,		{ 0x00, 0xC0, 0x95 } },
    { tulip_identify_cogent_nic,	{ 0x00, 0x00, 0x92 } },
    { tulip_identify_asante_nic,	{ 0x00, 0x00, 0x94 } },
    { tulip_identify_cogent_nic,	{ 0x00, 0x00, 0xD1 } },
    { tulip_identify_accton_nic,	{ 0x00, 0x00, 0xE8 } },
    { tulip_identify_compex_nic,        { 0x00, 0x80, 0x48 } },
    { NULL }
};

/*
 * This deals with the vagaries of the address roms and the
 * brain-deadness that various vendors commit in using them.
 */
static int
tulip_read_macaddr(tulip_softc_t *sc)
{
    u_int cksum, rom_cksum, idx;
    uint32_t csr;
    unsigned char tmpbuf[8];
    static const u_char testpat[] = { 0xFF, 0, 0x55, 0xAA, 0xFF, 0, 0x55, 0xAA };

    sc->tulip_connidx = TULIP_SROM_LASTCONNIDX;

    if (sc->tulip_chipid == TULIP_21040) {
	TULIP_CSR_WRITE(sc, csr_enetrom, 1);
	for (idx = 0; idx < sizeof(sc->tulip_rombuf); idx++) {
	    int cnt = 0;
	    while (((csr = TULIP_CSR_READ(sc, csr_enetrom)) & 0x80000000L) && cnt < 10000)
		cnt++;
	    sc->tulip_rombuf[idx] = csr & 0xFF;
	}
	sc->tulip_boardsw = &tulip_21040_boardsw;
    } else {
	if (sc->tulip_chipid == TULIP_21041) {
	    /*
	     * Thankfully all 21041's act the same.
	     */
	    sc->tulip_boardsw = &tulip_21041_boardsw;
	} else {
	    /*
	     * Assume all 21140 board are compatible with the
	     * DEC 10/100 evaluation board.  Not really valid but
	     * it's the best we can do until every one switches to
	     * the new SROM format.
	     */

	    sc->tulip_boardsw = &tulip_21140_eb_boardsw;
	}
	tulip_srom_read(sc);
	if (tulip_srom_crcok(sc->tulip_rombuf)) {
	    /*
	     * SROM CRC is valid therefore it must be in the
	     * new format.
	     */
	    sc->tulip_features |= TULIP_HAVE_ISVSROM|TULIP_HAVE_OKSROM;
	} else if (sc->tulip_rombuf[126] == 0xff && sc->tulip_rombuf[127] == 0xFF) {
	    /*
	     * No checksum is present.  See if the SROM id checks out;
	     * the first 18 bytes should be 0 followed by a 1 followed
	     * by the number of adapters (which we don't deal with yet).
	     */
	    for (idx = 0; idx < 18; idx++) {
		if (sc->tulip_rombuf[idx] != 0)
		    break;
	    }
	    if (idx == 18 && sc->tulip_rombuf[18] == 1 && sc->tulip_rombuf[19] != 0)
		sc->tulip_features |= TULIP_HAVE_ISVSROM;
	} else if (sc->tulip_chipid >= TULIP_21142) {
	    sc->tulip_features |= TULIP_HAVE_ISVSROM;
	    sc->tulip_boardsw = &tulip_2114x_isv_boardsw;
	}
	if ((sc->tulip_features & TULIP_HAVE_ISVSROM) && tulip_srom_decode(sc)) {
	    if (sc->tulip_chipid != TULIP_21041)
		sc->tulip_boardsw = &tulip_2114x_isv_boardsw;

	    /*
	     * If the SROM specifies more than one adapter, tag this as a
	     * BASE rom.
	     */
	    if (sc->tulip_rombuf[19] > 1)
		sc->tulip_features |= TULIP_HAVE_BASEROM;
	    if (sc->tulip_boardsw == NULL)
		return -6;
	    goto check_oui;
	}
    }


    if (bcmp(&sc->tulip_rombuf[0], &sc->tulip_rombuf[16], 8) != 0) {
	/*
	 * Some folks don't use the standard ethernet rom format
	 * but instead just put the address in the first 6 bytes
	 * of the rom and let the rest be all 0xffs.  (Can we say
	 * ZNYX?) (well sometimes they put in a checksum so we'll
	 * start at 8).
	 */
	for (idx = 8; idx < 32; idx++) {
	    if (sc->tulip_rombuf[idx] != 0xFF)
		return -4;
	}
	/*
	 * Make sure the address is not multicast or locally assigned
	 * that the OUI is not 00-00-00.
	 */
	if ((sc->tulip_rombuf[0] & 3) != 0)
	    return -4;
	if (sc->tulip_rombuf[0] == 0 && sc->tulip_rombuf[1] == 0
		&& sc->tulip_rombuf[2] == 0)
	    return -4;
	bcopy(sc->tulip_rombuf, sc->tulip_enaddr, 6);
	sc->tulip_features |= TULIP_HAVE_OKROM;
	goto check_oui;
    } else {
	/*
	 * A number of makers of multiport boards (ZNYX and Cogent)
	 * only put on one address ROM on their 21040 boards.  So
	 * if the ROM is all zeros (or all 0xFFs), look at the
	 * previous configured boards (as long as they are on the same
	 * PCI bus and the bus number is non-zero) until we find the
	 * master board with address ROM.  We then use its address ROM
	 * as the base for this board.  (we add our relative board
	 * to the last byte of its address).
	 */
	for (idx = 0; idx < sizeof(sc->tulip_rombuf); idx++) {
	    if (sc->tulip_rombuf[idx] != 0 && sc->tulip_rombuf[idx] != 0xFF)
		break;
	}
	if (idx == sizeof(sc->tulip_rombuf)) {
	    int root_unit;
	    tulip_softc_t *root_sc = NULL;
	    for (root_unit = sc->tulip_unit - 1; root_unit >= 0; root_unit--) {
		root_sc = tulips[root_unit];
		if (root_sc == NULL || (root_sc->tulip_features & (TULIP_HAVE_OKROM|TULIP_HAVE_SLAVEDROM)) == TULIP_HAVE_OKROM)
		    break;
		root_sc = NULL;
	    }
	    if (root_sc != NULL && (root_sc->tulip_features & TULIP_HAVE_BASEROM)
		    && root_sc->tulip_chipid == sc->tulip_chipid
		    && root_sc->tulip_pci_busno == sc->tulip_pci_busno) {
		sc->tulip_features |= TULIP_HAVE_SLAVEDROM;
		sc->tulip_boardsw = root_sc->tulip_boardsw;
		strcpy(sc->tulip_boardid, root_sc->tulip_boardid);
		if (sc->tulip_boardsw->bd_type == TULIP_21140_ISV) {
		    bcopy(root_sc->tulip_rombuf, sc->tulip_rombuf,
			  sizeof(sc->tulip_rombuf));
		    if (!tulip_srom_decode(sc))
			return -5;
		} else {
		    bcopy(root_sc->tulip_enaddr, sc->tulip_enaddr, 6);
		    sc->tulip_enaddr[5] += sc->tulip_unit - root_sc->tulip_unit;
		}
		/*
		 * Now for a truly disgusting kludge: all 4 21040s on
		 * the ZX314 share the same INTA line so the mapping
		 * setup by the BIOS on the PCI bridge is worthless.
		 * Rather than reprogramming the value in the config
		 * register, we will handle this internally.
		 */
		if (root_sc->tulip_features & TULIP_HAVE_SHAREDINTR) {
		    sc->tulip_slaves = root_sc->tulip_slaves;
		    root_sc->tulip_slaves = sc;
		    sc->tulip_features |= TULIP_HAVE_SLAVEDINTR;
		}
		return 0;
	    }
	}
    }

    /*
     * This is the standard DEC address ROM test.
     */

    if (bcmp(&sc->tulip_rombuf[24], testpat, 8) != 0)
	return -3;

    tmpbuf[0] = sc->tulip_rombuf[15]; tmpbuf[1] = sc->tulip_rombuf[14];
    tmpbuf[2] = sc->tulip_rombuf[13]; tmpbuf[3] = sc->tulip_rombuf[12];
    tmpbuf[4] = sc->tulip_rombuf[11]; tmpbuf[5] = sc->tulip_rombuf[10];
    tmpbuf[6] = sc->tulip_rombuf[9];  tmpbuf[7] = sc->tulip_rombuf[8];
    if (bcmp(&sc->tulip_rombuf[0], tmpbuf, 8) != 0)
	return -2;

    bcopy(sc->tulip_rombuf, sc->tulip_enaddr, 6);

    cksum = *(u_int16_t *) &sc->tulip_enaddr[0];
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_enaddr[2];
    if (cksum > 65535) cksum -= 65535;
    cksum *= 2;
    if (cksum > 65535) cksum -= 65535;
    cksum += *(u_int16_t *) &sc->tulip_enaddr[4];
    if (cksum >= 65535) cksum -= 65535;

    rom_cksum = *(u_int16_t *) &sc->tulip_rombuf[6];
	
    if (cksum != rom_cksum)
	return -1;

  check_oui:
    /*
     * Check for various boards based on OUI.  Did I say braindead?
     */
    for (idx = 0; tulip_vendors[idx].vendor_identify_nic != NULL; idx++) {
	if (bcmp(sc->tulip_enaddr, tulip_vendors[idx].vendor_oui, 3) == 0) {
	    (*tulip_vendors[idx].vendor_identify_nic)(sc);
	    break;
	}
    }

    sc->tulip_features |= TULIP_HAVE_OKROM;
    return 0;
}

static void
tulip_ifmedia_add(tulip_softc_t * const sc)
{
    tulip_media_t media;
    int medias = 0;

    for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	if (sc->tulip_mediums[media] != NULL) {
	    ifmedia_add(&sc->tulip_ifmedia, tulip_media_to_ifmedia[media],
			0, 0);
	    medias++;
	}
    }
    if (medias == 0) {
	sc->tulip_features |= TULIP_HAVE_NOMEDIA;
	ifmedia_add(&sc->tulip_ifmedia, IFM_ETHER | IFM_NONE, 0, 0);
	ifmedia_set(&sc->tulip_ifmedia, IFM_ETHER | IFM_NONE);
    } else if (sc->tulip_media == TULIP_MEDIA_UNKNOWN) {
	ifmedia_add(&sc->tulip_ifmedia, IFM_ETHER | IFM_AUTO, 0, 0);
	ifmedia_set(&sc->tulip_ifmedia, IFM_ETHER | IFM_AUTO);
    } else {
	ifmedia_set(&sc->tulip_ifmedia, tulip_media_to_ifmedia[sc->tulip_media]);
	sc->tulip_flags |= TULIP_PRINTMEDIA;
	tulip_linkup(sc, sc->tulip_media);
    }
}

static int
tulip_ifmedia_change(struct ifnet *ifp)
{
    tulip_softc_t *sc = (tulip_softc_t *)ifp->if_softc;

    sc->tulip_flags |= TULIP_NEEDRESET;
    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
    if (IFM_SUBTYPE(sc->tulip_ifmedia.ifm_media) != IFM_AUTO) {
	tulip_media_t media;
	for (media = TULIP_MEDIA_UNKNOWN; media < TULIP_MEDIA_MAX; media++) {
	    if (sc->tulip_mediums[media] != NULL
		&& sc->tulip_ifmedia.ifm_media == tulip_media_to_ifmedia[media]) {
		sc->tulip_flags |= TULIP_PRINTMEDIA;
		sc->tulip_flags &= ~TULIP_DIDNWAY;
		tulip_linkup(sc, media);
		return 0;
	    }
	}
    }
    sc->tulip_flags &= ~(TULIP_TXPROBE_ACTIVE|TULIP_WANTRXACT);
    tulip_reset(sc);
    tulip_init(sc);
    return 0;
}

/*
 * Media status callback
 */
static void
tulip_ifmedia_status(struct ifnet *ifp, struct ifmediareq *req)
{
    tulip_softc_t *sc = (tulip_softc_t *)ifp->if_softc;

    if (sc->tulip_media == TULIP_MEDIA_UNKNOWN)
	return;

    req->ifm_status = IFM_AVALID;
    if (sc->tulip_flags & TULIP_LINKUP)
	req->ifm_status |= IFM_ACTIVE;

    req->ifm_active = tulip_media_to_ifmedia[sc->tulip_media];
}

static void
tulip_addr_filter(tulip_softc_t *sc)
{
    struct ifmultiaddr *ifma;
    u_char *addrp;
    int multicnt;

    sc->tulip_flags &= ~(TULIP_WANTHASHPERFECT|TULIP_WANTHASHONLY|TULIP_ALLMULTI);
    sc->tulip_flags |= TULIP_WANTSETUP|TULIP_WANTTXSTART;
    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
#if defined(IFF_ALLMULTI)    
    if (sc->tulip_if.if_flags & IFF_ALLMULTI)
	sc->tulip_flags |= TULIP_ALLMULTI ;
#endif

    multicnt = 0;
    for (ifma = sc->tulip_if.if_multiaddrs.lh_first; ifma != NULL;
	 ifma = ifma->ifma_link.le_next) {

	    if (ifma->ifma_addr->sa_family == AF_LINK)
		multicnt++;
    }

    sc->tulip_if.if_start = tulip_ifstart;	/* so the setup packet gets queued */
    if (multicnt > 14) {
	uint32_t *sp = sc->tulip_setupdata;
	u_int hash;
	/*
	 * Some early passes of the 21140 have broken implementations of
	 * hash-perfect mode.  When we get too many multicasts for perfect
	 * filtering with these chips, we need to switch into hash-only
	 * mode (this is better than all-multicast on network with lots
	 * of multicast traffic).
	 */
	if (sc->tulip_features & TULIP_HAVE_BROKEN_HASH)
	    sc->tulip_flags |= TULIP_WANTHASHONLY;
	else
	    sc->tulip_flags |= TULIP_WANTHASHPERFECT;
	/*
	 * If we have more than 14 multicasts, we have
	 * go into hash perfect mode (512 bit multicast
	 * hash and one perfect hardware).
	 */
	bzero(sc->tulip_setupdata, sizeof(sc->tulip_setupdata));

	for (ifma = sc->tulip_if.if_multiaddrs.lh_first; ifma != NULL;
	     ifma = ifma->ifma_link.le_next) {

		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		hash = tulip_mchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
#if BYTE_ORDER == BIG_ENDIAN
		sp[hash >> 4] |= bswap32(1 << (hash & 0xF));
#else
		sp[hash >> 4] |= 1 << (hash & 0xF);
#endif
	}
	/*
	 * No reason to use a hash if we are going to be
	 * receiving every multicast.
	 */
	if ((sc->tulip_flags & TULIP_ALLMULTI) == 0) {
	    hash = tulip_mchash(sc->tulip_if.if_broadcastaddr);
#if BYTE_ORDER == BIG_ENDIAN
	    sp[hash >> 4] |= bswap32(1 << (hash & 0xF));
#else
	    sp[hash >> 4] |= 1 << (hash & 0xF);
#endif
	    if (sc->tulip_flags & TULIP_WANTHASHONLY) {
		hash = tulip_mchash(sc->tulip_enaddr);
#if BYTE_ORDER == BIG_ENDIAN
		sp[hash >> 4] |= bswap32(1 << (hash & 0xF));
#else
		sp[hash >> 4] |= 1 << (hash & 0xF);
#endif
	    } else {
#if BYTE_ORDER == BIG_ENDIAN
		sp[39] = ((u_int16_t *) sc->tulip_enaddr)[0] << 16;
		sp[40] = ((u_int16_t *) sc->tulip_enaddr)[1] << 16;
		sp[41] = ((u_int16_t *) sc->tulip_enaddr)[2] << 16;
#else
		sp[39] = ((u_int16_t *) sc->tulip_enaddr)[0]; 
		sp[40] = ((u_int16_t *) sc->tulip_enaddr)[1]; 
		sp[41] = ((u_int16_t *) sc->tulip_enaddr)[2];
#endif
	    }
	}
    }
    if ((sc->tulip_flags & (TULIP_WANTHASHPERFECT|TULIP_WANTHASHONLY)) == 0) {
	uint32_t *sp = sc->tulip_setupdata;
	int idx = 0;
	if ((sc->tulip_flags & TULIP_ALLMULTI) == 0) {
	    /*
	     * Else can get perfect filtering for 16 addresses.
	     */
	    for (ifma = sc->tulip_if.if_multiaddrs.lh_first; ifma != NULL;
		 ifma = ifma->ifma_link.le_next) {
		    if (ifma->ifma_addr->sa_family != AF_LINK)
			    continue;
		    addrp = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
#if BYTE_ORDER == BIG_ENDIAN
		    *sp++ = ((u_int16_t *) addrp)[0] << 16;
		    *sp++ = ((u_int16_t *) addrp)[1] << 16;
		    *sp++ = ((u_int16_t *) addrp)[2] << 16;
#else
		    *sp++ = ((u_int16_t *) addrp)[0]; 
		    *sp++ = ((u_int16_t *) addrp)[1]; 
		    *sp++ = ((u_int16_t *) addrp)[2];
#endif
		    idx++;
	    }
	    /*
	     * Add the broadcast address.
	     */
	    idx++;
#if BYTE_ORDER == BIG_ENDIAN
	    *sp++ = 0xFFFF << 16;
	    *sp++ = 0xFFFF << 16;
	    *sp++ = 0xFFFF << 16;
#else
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
	    *sp++ = 0xFFFF;
#endif
	}
	/*
	 * Pad the rest with our hardware address
	 */
	for (; idx < 16; idx++) {
#if BYTE_ORDER == BIG_ENDIAN
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[0] << 16;
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[1] << 16;
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[2] << 16;
#else
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[0]; 
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[1]; 
	    *sp++ = ((u_int16_t *) sc->tulip_enaddr)[2];
#endif
	}
    }
#if defined(IFF_ALLMULTI)
    if (sc->tulip_flags & TULIP_ALLMULTI)
	sc->tulip_if.if_flags |= IFF_ALLMULTI;
#endif
}

static void
tulip_reset(tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri;
    tulip_desc_t *di;
    uint32_t inreset = (sc->tulip_flags & TULIP_INRESET);

    /*
     * Brilliant.  Simply brilliant.  When switching modes/speeds
     * on a 2114*, you need to set the appriopriate MII/PCS/SCL/PS
     * bits in CSR6 and then do a software reset to get the 21140
     * to properly reset its internal pathways to the right places.
     *   Grrrr.
     */
    if ((sc->tulip_flags & TULIP_DEVICEPROBE) == 0
	    && sc->tulip_boardsw->bd_media_preset != NULL)
	(*sc->tulip_boardsw->bd_media_preset)(sc);

    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    if (!inreset) {
	sc->tulip_flags |= TULIP_INRESET;
	sc->tulip_flags &= ~(TULIP_NEEDRESET|TULIP_RXBUFSLOW);
	sc->tulip_if.if_flags &= ~IFF_OACTIVE;
	sc->tulip_if.if_start = tulip_ifstart;
    }

    TULIP_CSR_WRITE(sc, csr_txlist, TULIP_KVATOPHYS(sc, &sc->tulip_txinfo.ri_first[0]));
    TULIP_CSR_WRITE(sc, csr_rxlist, TULIP_KVATOPHYS(sc, &sc->tulip_rxinfo.ri_first[0]));
    TULIP_CSR_WRITE(sc, csr_busmode,
		    (1 << (3 /*pci_max_burst_len*/ + 8))
		    |TULIP_BUSMODE_CACHE_ALIGN8
		    |TULIP_BUSMODE_READMULTIPLE
		    |(BYTE_ORDER != LITTLE_ENDIAN ?
		      TULIP_BUSMODE_DESC_BIGENDIAN : 0));

    sc->tulip_txtimer = 0;
    sc->tulip_txq.ifq_maxlen = TULIP_TXDESCS;
    /*
     * Free all the mbufs that were on the transmit ring.
     */
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_txq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    ri = &sc->tulip_txinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++)
	di->d_status = 0;

    /*
     * We need to collect all the mbufs were on the 
     * receive ring before we reinit it either to put
     * them back on or to know if we have to allocate
     * more.
     */
    ri = &sc->tulip_rxinfo;
    ri->ri_nextin = ri->ri_nextout = ri->ri_first;
    ri->ri_free = ri->ri_max;
    for (di = ri->ri_first; di < ri->ri_last; di++) {
	di->d_status = 0;
	di->d_length1 = 0; di->d_addr1 = 0;
	di->d_length2 = 0; di->d_addr2 = 0;
    }
    for (;;) {
	struct mbuf *m;
	IF_DEQUEUE(&sc->tulip_rxq, m);
	if (m == NULL)
	    break;
	m_freem(m);
    }

    /*
     * If tulip_reset is being called recurisvely, exit quickly knowing
     * that when the outer tulip_reset returns all the right stuff will
     * have happened.
     */
    if (inreset)
	return;

    sc->tulip_intrmask |= TULIP_STS_NORMALINTR|TULIP_STS_RXINTR|TULIP_STS_TXINTR
	|TULIP_STS_ABNRMLINTR|TULIP_STS_SYSERROR|TULIP_STS_TXSTOPPED
	|TULIP_STS_TXUNDERFLOW|TULIP_STS_TXBABBLE
	|TULIP_STS_RXSTOPPED;

    if ((sc->tulip_flags & TULIP_DEVICEPROBE) == 0)
	(*sc->tulip_boardsw->bd_media_select)(sc);
    tulip_media_print(sc);
    if (sc->tulip_features & TULIP_HAVE_DUALSENSE)
	TULIP_CSR_WRITE(sc, csr_sia_status, TULIP_CSR_READ(sc, csr_sia_status));

    sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_WANTSETUP|TULIP_INRESET
			 |TULIP_RXACT);
    tulip_addr_filter(sc);
}

static void
tulip_init(tulip_softc_t * const sc)
{
    if (sc->tulip_if.if_flags & IFF_UP) {
	if ((sc->tulip_if.if_flags & IFF_RUNNING) == 0) {
	    /* initialize the media */
	    tulip_reset(sc);
	}
	sc->tulip_if.if_flags |= IFF_RUNNING;
	if (sc->tulip_if.if_flags & IFF_PROMISC) {
	    sc->tulip_flags |= TULIP_PROMISC;
	    sc->tulip_cmdmode |= TULIP_CMD_PROMISCUOUS;
	    sc->tulip_intrmask |= TULIP_STS_TXINTR;
	} else {
	    sc->tulip_flags &= ~TULIP_PROMISC;
	    sc->tulip_cmdmode &= ~TULIP_CMD_PROMISCUOUS;
	    if (sc->tulip_flags & TULIP_ALLMULTI) {
		sc->tulip_cmdmode |= TULIP_CMD_ALLMULTI;
	    } else {
		sc->tulip_cmdmode &= ~TULIP_CMD_ALLMULTI;
	    }
	}
	sc->tulip_cmdmode |= TULIP_CMD_TXRUN;
	if ((sc->tulip_flags & (TULIP_TXPROBE_ACTIVE|TULIP_WANTSETUP)) == 0) {
	    tulip_rx_intr(sc);
	    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
	    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
	} else {
	    sc->tulip_if.if_flags |= IFF_OACTIVE;
	    sc->tulip_cmdmode &= ~TULIP_CMD_RXRUN;
	    sc->tulip_intrmask &= ~TULIP_STS_RXSTOPPED;
	}
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	if ((sc->tulip_flags & (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == TULIP_WANTSETUP)
	    tulip_txput_setup(sc);
    } else {
	sc->tulip_if.if_flags &= ~IFF_RUNNING;
	tulip_reset(sc);
    }
}

static void
tulip_rx_intr(tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri = &sc->tulip_rxinfo;
    struct ifnet *ifp = &sc->tulip_if;
    int fillok = 1;

    for (;;) {
	struct ether_header eh;
	tulip_desc_t *eop = ri->ri_nextin;
	int total_len = 0, last_offset = 0;
	struct mbuf *ms = NULL, *me = NULL;
	int accept = 0;

	if (fillok && sc->tulip_rxq.ifq_len < TULIP_RXQ_TARGET)
	    goto queue_mbuf;

	/*
	 * If the TULIP has no descriptors, there can't be any receive
	 * descriptors to process.
 	 */
	if (eop == ri->ri_nextout)
	    break;

	/*
	 * 90% of the packets will fit in one descriptor.  So we optimize
	 * for that case.
	 */
	TULIP_RXDESC_POSTSYNC(sc, eop, sizeof(*eop));
	if ((((volatile tulip_desc_t *) eop)->d_status & (TULIP_DSTS_OWNER|TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) == (TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) {
	    IF_DEQUEUE(&sc->tulip_rxq, ms);
	    me = ms;
	} else {
	    /*
	     * If still owned by the TULIP, don't touch it.
	     */
	    if (((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER)
		break;

	    /*
	     * It is possible (though improbable unless the BIG_PACKET support
	     * is enabled or MCLBYTES < 1518) for a received packet to cross
	     * more than one receive descriptor.  
	     */
	    while ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_RxLASTDESC) == 0) {
		if (++eop == ri->ri_last)
		    eop = ri->ri_first;
		TULIP_RXDESC_POSTSYNC(sc, eop, sizeof(*eop));
		if (eop == ri->ri_nextout || ((((volatile tulip_desc_t *) eop)->d_status & TULIP_DSTS_OWNER))) {
		    return;
		}
		total_len++;
	    }

	    /*
	     * Dequeue the first buffer for the start of the packet.  Hopefully
	     * this will be the only one we need to dequeue.  However, if the
	     * packet consumed multiple descriptors, then we need to dequeue
	     * those buffers and chain to the starting mbuf.  All buffers but
	     * the last buffer have the same length so we can set that now.
	     * (we add to last_offset instead of multiplying since we normally
	     * won't go into the loop and thereby saving a ourselves from
	     * doing a multiplication by 0 in the normal case).
	     */
	    IF_DEQUEUE(&sc->tulip_rxq, ms);
	    for (me = ms; total_len > 0; total_len--) {
		me->m_len = TULIP_RX_BUFLEN;
		last_offset += TULIP_RX_BUFLEN;
		IF_DEQUEUE(&sc->tulip_rxq, me->m_next);
		me = me->m_next;
	    }
	}

	/*
	 *  Now get the size of received packet (minus the CRC).
	 */
	total_len = ((eop->d_status >> 16) & 0x7FFF) - 4;
	if ((sc->tulip_flags & TULIP_RXIGNORE) == 0
		&& ((eop->d_status & TULIP_DSTS_ERRSUM) == 0
#ifdef BIG_PACKET
		     || (total_len <= sc->tulip_if.if_mtu + sizeof(struct ether_header) && 
			 (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxRUNT|
					  TULIP_DSTS_RxCOLLSEEN|TULIP_DSTS_RxBADCRC|
					  TULIP_DSTS_RxOVERFLOW)) == 0)
#endif
		)) {
	    me->m_len = total_len - last_offset;

	    eh = *mtod(ms, struct ether_header *);
	    sc->tulip_flags |= TULIP_RXACT;
	    accept = 1;
	} else {
	    ifp->if_ierrors++;
	    if (eop->d_status & (TULIP_DSTS_RxBADLENGTH|TULIP_DSTS_RxOVERFLOW|TULIP_DSTS_RxWATCHDOG)) {
		sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
	    } else {
		if (eop->d_status & TULIP_DSTS_RxTOOLONG) {
		    sc->tulip_dot3stats.dot3StatsFrameTooLongs++;
		}
		if (eop->d_status & TULIP_DSTS_RxBADCRC) {
		    if (eop->d_status & TULIP_DSTS_RxDRBBLBIT) {
			sc->tulip_dot3stats.dot3StatsAlignmentErrors++;
		    } else {
			sc->tulip_dot3stats.dot3StatsFCSErrors++;
		    }
		}
	    }
	}
	ifp->if_ipackets++;
	if (++eop == ri->ri_last)
	    eop = ri->ri_first;
	ri->ri_nextin = eop;
      queue_mbuf:
	/*
	 * Either we are priming the TULIP with mbufs (m == NULL)
	 * or we are about to accept an mbuf for the upper layers
	 * so we need to allocate an mbuf to replace it.  If we
	 * can't replace it, send up it anyways.  This may cause
	 * us to drop packets in the future but that's better than
	 * being caught in livelock.
	 *
	 * Note that if this packet crossed multiple descriptors
	 * we don't even try to reallocate all the mbufs here.
	 * Instead we rely on the test of the beginning of
	 * the loop to refill for the extra consumed mbufs.
	 */
	if (accept || ms == NULL) {
	    struct mbuf *m0;
	    MGETHDR(m0, MB_DONTWAIT, MT_DATA);
	    if (m0 != NULL) {
#if defined(TULIP_COPY_RXDATA)
		if (!accept || total_len >= (MHLEN - 2)) {
#endif
		    MCLGET(m0, MB_DONTWAIT);
		    if ((m0->m_flags & M_EXT) == 0) {
			m_freem(m0);
			m0 = NULL;
		    }
#if defined(TULIP_COPY_RXDATA)
		}
#endif
	    }
	    if (accept
#if defined(TULIP_COPY_RXDATA)
		&& m0 != NULL
#endif
		) {
#if !defined(TULIP_COPY_RXDATA)
		ms->m_pkthdr.len = total_len;
		ms->m_pkthdr.rcvif = ifp;
		m_adj(ms, sizeof(struct ether_header));
		ether_input(ifp, &eh, ms);
#else
#ifdef BIG_PACKET
#error BIG_PACKET is incompatible with TULIP_COPY_RXDATA
#endif
		m0->m_data += 2;	/* align data after header */
		m_copydata(ms, 0, total_len, mtod(m0, caddr_t));
		m0->m_len = m0->m_pkthdr.len = total_len;
		m0->m_pkthdr.rcvif = ifp;
		m_adj(m0, sizeof(struct ether_header));
		ether_input(ifp, &eh, m0);
		m0 = ms;
#endif /* ! TULIP_COPY_RXDATA */
	    }
	    ms = m0;
	}
	if (ms == NULL) {
	    /*
	     * Couldn't allocate a new buffer.  Don't bother 
	     * trying to replenish the receive queue.
	     */
	    fillok = 0;
	    sc->tulip_flags |= TULIP_RXBUFSLOW;
	    continue;
	}
	/*
	 * Now give the buffer(s) to the TULIP and save in our
	 * receive queue.
	 */
	do {
	    tulip_desc_t * const nextout = ri->ri_nextout;
	    nextout->d_addr1 = TULIP_KVATOPHYS(sc, mtod(ms, caddr_t));
	    nextout->d_length1 = TULIP_RX_BUFLEN;
	    nextout->d_status = TULIP_DSTS_OWNER;
	    TULIP_RXDESC_POSTSYNC(sc, nextout, sizeof(u_int32_t));
	    if (++ri->ri_nextout == ri->ri_last)
		ri->ri_nextout = ri->ri_first;
	    me = ms->m_next;
	    ms->m_next = NULL;
	    IF_ENQUEUE(&sc->tulip_rxq, ms);
	} while ((ms = me) != NULL);

	if (sc->tulip_rxq.ifq_len >= TULIP_RXQ_TARGET)
	    sc->tulip_flags &= ~TULIP_RXBUFSLOW;
    }
}

static int
tulip_tx_intr(tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri = &sc->tulip_txinfo;
    struct mbuf *m;
    int xmits = 0;
    int descs = 0;

    while (ri->ri_free < ri->ri_max) {
	uint32_t d_flag;

	TULIP_TXDESC_POSTSYNC(sc, ri->ri_nextin, sizeof(*ri->ri_nextin));
	if (((volatile tulip_desc_t *) ri->ri_nextin)->d_status & TULIP_DSTS_OWNER)
	    break;

	ri->ri_free++;
	descs++;
	d_flag = ri->ri_nextin->d_flag;
	if (d_flag & TULIP_DFLAG_TxLASTSEG) {
	    if (d_flag & TULIP_DFLAG_TxSETUPPKT) {
		/*
		 * We've just finished processing a setup packet.
		 * Mark that we finished it.  If there's not
		 * another pending, startup the TULIP receiver.
		 * Make sure we ack the RXSTOPPED so we won't get
		 * an abormal interrupt indication.
		 */
		TULIP_TXMAP_POSTSYNC(sc, sc->tulip_setupmap);
		sc->tulip_flags &= ~(TULIP_DOINGSETUP|TULIP_HASHONLY);
		if (ri->ri_nextin->d_flag & TULIP_DFLAG_TxINVRSFILT)
		    sc->tulip_flags |= TULIP_HASHONLY;
		if ((sc->tulip_flags & (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == 0) {
		    tulip_rx_intr(sc);
		    sc->tulip_cmdmode |= TULIP_CMD_RXRUN;
		    sc->tulip_intrmask |= TULIP_STS_RXSTOPPED;
		    TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
		    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
		}
	    } else {
		const uint32_t d_status = ri->ri_nextin->d_status;
		IF_DEQUEUE(&sc->tulip_txq, m);
		if (m != NULL) {
		    m_freem(m);
		}
		if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
		    tulip_mediapoll_event_t event = TULIP_MEDIAPOLL_TXPROBE_OK;
		    if (d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxEXCCOLL)) {
			event = TULIP_MEDIAPOLL_TXPROBE_FAILED;
		    }
		    (*sc->tulip_boardsw->bd_media_poll)(sc, event);
		    /*
		     * Escape from the loop before media poll has reset the TULIP!
		     */
		    break;
		} else {
		    xmits++;
		    if (d_status & TULIP_DSTS_ERRSUM) {
			sc->tulip_if.if_oerrors++;
			if (d_status & TULIP_DSTS_TxEXCCOLL)
			    sc->tulip_dot3stats.dot3StatsExcessiveCollisions++;
			if (d_status & TULIP_DSTS_TxLATECOLL)
			    sc->tulip_dot3stats.dot3StatsLateCollisions++;
			if (d_status & (TULIP_DSTS_TxNOCARR|TULIP_DSTS_TxCARRLOSS))
			    sc->tulip_dot3stats.dot3StatsCarrierSenseErrors++;
			if (d_status & (TULIP_DSTS_TxUNDERFLOW|TULIP_DSTS_TxBABBLE))
			    sc->tulip_dot3stats.dot3StatsInternalMacTransmitErrors++;
			if (d_status & TULIP_DSTS_TxUNDERFLOW)
			    sc->tulip_dot3stats.dot3StatsInternalTransmitUnderflows++;
			if (d_status & TULIP_DSTS_TxBABBLE)
			    sc->tulip_dot3stats.dot3StatsInternalTransmitBabbles++;
		    } else {
			u_int32_t collisions = 
			    (d_status & TULIP_DSTS_TxCOLLMASK)
				>> TULIP_DSTS_V_TxCOLLCNT;
			sc->tulip_if.if_collisions += collisions;
			if (collisions == 1)
			    sc->tulip_dot3stats.dot3StatsSingleCollisionFrames++;
			else if (collisions > 1)
			    sc->tulip_dot3stats.dot3StatsMultipleCollisionFrames++;
			else if (d_status & TULIP_DSTS_TxDEFERRED)
			    sc->tulip_dot3stats.dot3StatsDeferredTransmissions++;
			/*
			 * SQE is only valid for 10baseT/BNC/AUI when not
			 * running in full-duplex.  In order to speed up the
			 * test, the corresponding bit in tulip_flags needs to
			 * set as well to get us to count SQE Test Errors.
			 */
			if (d_status & TULIP_DSTS_TxNOHRTBT & sc->tulip_flags)
			    sc->tulip_dot3stats.dot3StatsSQETestErrors++;
		    }
		}
	    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;

	if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
	    sc->tulip_if.if_flags &= ~IFF_OACTIVE;
    }
    /*
     * If nothing left to transmit, disable the timer.
     * Else if progress, reset the timer back to 2 ticks.
     */
    if (ri->ri_free == ri->ri_max || (sc->tulip_flags & TULIP_TXPROBE_ACTIVE))
	sc->tulip_txtimer = 0;
    else if (xmits > 0)
	sc->tulip_txtimer = TULIP_TXTIMER;
    sc->tulip_if.if_opackets += xmits;
    return descs;
}

static void
tulip_print_abnormal_interrupt(tulip_softc_t *sc, uint32_t csr)
{
    const char * const *msgp = tulip_status_bits;
    const char *sep;
    uint32_t mask;
    const char thrsh[] = "72|128\0\0\0" "96|256\0\0\0" "128|512\0\0" "160|1024";

    csr &= (1 << (sizeof(tulip_status_bits)/sizeof(tulip_status_bits[0]))) - 1;
    printf("%s%d: abnormal interrupt:", sc->tulip_name, sc->tulip_unit);
    for (sep = " ", mask = 1; mask <= csr; mask <<= 1, msgp++) {
	if ((csr & mask) && *msgp != NULL) {
	    printf("%s%s", sep, *msgp);
	    if (mask == TULIP_STS_TXUNDERFLOW && (sc->tulip_flags & TULIP_NEWTXTHRESH)) {
		sc->tulip_flags &= ~TULIP_NEWTXTHRESH;
		if (sc->tulip_cmdmode & TULIP_CMD_STOREFWD) {
		    printf(" (switching to store-and-forward mode)");
		} else {
		    printf(" (raising TX threshold to %s)",
			   &thrsh[9 * ((sc->tulip_cmdmode & TULIP_CMD_THRESHOLDCTL) >> 14)]);
		}
	    }
	    sep = ", ";
	}
    }
    printf("\n");
}

static void
tulip_intr_handler(tulip_softc_t *sc, int *progress_p)
{
    uint32_t csr;

    while ((csr = TULIP_CSR_READ(sc, csr_status)) & sc->tulip_intrmask) {
	*progress_p = 1;
	TULIP_CSR_WRITE(sc, csr_status, csr);

	if (csr & TULIP_STS_SYSERROR) {
	    sc->tulip_last_system_error = (csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
	    if (sc->tulip_flags & TULIP_NOMESSAGES) {
		sc->tulip_flags |= TULIP_SYSTEMERROR;
	    } else {
		printf("%s%d: system error: %s\n",
		       sc->tulip_name, sc->tulip_unit,
		       tulip_system_errors[sc->tulip_last_system_error]);
	    }
	    sc->tulip_flags |= TULIP_NEEDRESET;
	    sc->tulip_system_errors++;
	    break;
	}
	if (csr & (TULIP_STS_LINKPASS|TULIP_STS_LINKFAIL) & sc->tulip_intrmask) {
	    if (sc->tulip_boardsw->bd_media_poll != NULL) {
		(*sc->tulip_boardsw->bd_media_poll)(sc, csr & TULIP_STS_LINKFAIL
						    ? TULIP_MEDIAPOLL_LINKFAIL
						    : TULIP_MEDIAPOLL_LINKPASS);
		csr &= ~TULIP_STS_ABNRMLINTR;
	    }
	    tulip_media_print(sc);
	}
	if (csr & (TULIP_STS_RXINTR|TULIP_STS_RXNOBUF)) {
	    u_int32_t misses = TULIP_CSR_READ(sc, csr_missed_frames);
	    if (csr & TULIP_STS_RXNOBUF)
		sc->tulip_dot3stats.dot3StatsMissedFrames += misses & 0xFFFF;
	    /*
	     * Pass 2.[012] of the 21140A-A[CDE] may hang and/or corrupt data
	     * on receive overflows.
	     */
	   if ((misses & 0x0FFE0000) && (sc->tulip_features & TULIP_HAVE_RXBADOVRFLW)) {
		sc->tulip_dot3stats.dot3StatsInternalMacReceiveErrors++;
		/*
		 * Stop the receiver process and spin until it's stopped.
		 * Tell rx_intr to drop the packets it dequeues.
		 */
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode & ~TULIP_CMD_RXRUN);
		while ((TULIP_CSR_READ(sc, csr_status) & TULIP_STS_RXSTOPPED) == 0)
		    ;
		TULIP_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		sc->tulip_flags |= TULIP_RXIGNORE;
	    }
	    tulip_rx_intr(sc);
	    if (sc->tulip_flags & TULIP_RXIGNORE) {
		/*
		 * Restart the receiver.
		 */
		sc->tulip_flags &= ~TULIP_RXIGNORE;
		TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	    }
	}
	if (csr & TULIP_STS_ABNRMLINTR) {
	    u_int32_t tmp = csr & sc->tulip_intrmask
		& ~(TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR);
	    if (csr & TULIP_STS_TXUNDERFLOW) {
		if ((sc->tulip_cmdmode & TULIP_CMD_THRESHOLDCTL) != TULIP_CMD_THRSHLD160) {
		    sc->tulip_cmdmode += TULIP_CMD_THRSHLD96;
		    sc->tulip_flags |= TULIP_NEWTXTHRESH;
		} else if (sc->tulip_features & TULIP_HAVE_STOREFWD) {
		    sc->tulip_cmdmode |= TULIP_CMD_STOREFWD;
		    sc->tulip_flags |= TULIP_NEWTXTHRESH;
		}
	    }
	    if (sc->tulip_flags & TULIP_NOMESSAGES) {
		sc->tulip_statusbits |= tmp;
	    } else {
		tulip_print_abnormal_interrupt(sc, tmp);
		sc->tulip_flags |= TULIP_NOMESSAGES;
	    }
	    TULIP_CSR_WRITE(sc, csr_command, sc->tulip_cmdmode);
	}
	if (sc->tulip_flags & (TULIP_WANTTXSTART|TULIP_TXPROBE_ACTIVE|TULIP_DOINGSETUP|TULIP_PROMISC)) {
	    tulip_tx_intr(sc);
	    if ((sc->tulip_flags & TULIP_TXPROBE_ACTIVE) == 0)
		tulip_ifstart(&sc->tulip_if);
	}
    }
    if (sc->tulip_flags & TULIP_NEEDRESET) {
	tulip_reset(sc);
	tulip_init(sc);
    }
}

static void
tulip_intr_shared(void *arg)
{
    tulip_softc_t *sc = arg;
    int progress = 0;

    for (; sc != NULL; sc = sc->tulip_slaves) {
	tulip_intr_handler(sc, &progress);
    }
}

static void
tulip_intr_normal(void *arg)
{
    tulip_softc_t *sc = (tulip_softc_t *)arg;
    int progress = 0;

    tulip_intr_handler(sc, &progress);
}

static struct mbuf *
tulip_mbuf_compress(struct mbuf *m)
{
    struct mbuf *m0;
#if MCLBYTES >= ETHERMTU + 18 && !defined(BIG_PACKET)
    MGETHDR(m0, MB_DONTWAIT, MT_DATA);
    if (m0 != NULL) {
	if (m->m_pkthdr.len > MHLEN) {
	    MCLGET(m0, MB_DONTWAIT);
	    if ((m0->m_flags & M_EXT) == 0) {
		m_freem(m);
		m_freem(m0);
		return NULL;
	    }
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
	m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
    }
#else
    int mlen = MHLEN;
    int len = m->m_pkthdr.len;
    struct mbuf **mp = &m0;

    while (len > 0) {
	if (mlen == MHLEN) {
	    MGETHDR(*mp, MB_DONTWAIT, MT_DATA);
	} else {
	    MGET(*mp, MB_DONTWAIT, MT_DATA);
	}
	if (*mp == NULL) {
	    m_freem(m0);
	    m0 = NULL;
	    break;
	}
	if (len > MLEN) {
	    MCLGET(*mp, MB_DONTWAIT);
	    if (((*mp)->m_flags & M_EXT) == 0) {
		m_freem(m0);
		m0 = NULL;
		break;
	    }
	    (*mp)->m_len = len <= MCLBYTES ? len : MCLBYTES;
	} else {
	    (*mp)->m_len = len <= mlen ? len : mlen;
	}
	m_copydata(m, m->m_pkthdr.len - len,
		   (*mp)->m_len, mtod((*mp), caddr_t));
	len -= (*mp)->m_len;
	mp = &(*mp)->m_next;
	mlen = MLEN;
    }
#endif
    m_freem(m);
    return m0;
}

static struct mbuf *
tulip_txput(tulip_softc_t *sc, struct mbuf *m)
{
    tulip_ringinfo_t *ri = &sc->tulip_txinfo;
    tulip_desc_t *eop, *nextout;
    int segcnt, free;
    uint32_t d_status;
    struct mbuf *m0;

    /*
     * Now we try to fill in our transmit descriptors.  This is
     * a bit reminiscent of going on the Ark two by two
     * since each descriptor for the TULIP can describe
     * two buffers.  So we advance through packet filling
     * each of the two entries at a time to to fill each
     * descriptor.  Clear the first and last segment bits
     * in each descriptor (actually just clear everything
     * but the end-of-ring or chain bits) to make sure
     * we don't get messed up by previously sent packets.
     *
     * We may fail to put the entire packet on the ring if
     * there is either not enough ring entries free or if the
     * packet has more than MAX_TXSEG segments.  In the former
     * case we will just wait for the ring to empty.  In the
     * latter case we have to recopy.
     */
  again:
    m0 = m;
    d_status = 0;
    eop = nextout = ri->ri_nextout;
    segcnt = 0;
    free = ri->ri_free;

    do {
	int len = m0->m_len;
	caddr_t addr = mtod(m0, caddr_t);
	unsigned clsize = PAGE_SIZE - (((uintptr_t) addr) & (PAGE_SIZE-1));

	while (len > 0) {
	    unsigned slen = min(len, clsize);
#ifdef BIG_PACKET
	    int partial = 0;
	    if (slen >= 2048)
		slen = 2040, partial = 1;
#endif
	    segcnt++;
	    if (segcnt > TULIP_MAX_TXSEG) {
		/*
		 * The packet exceeds the number of transmit buffer
		 * entries that we can use for one packet, so we have
		 * recopy it into one mbuf and then try again.
		 */
		m = tulip_mbuf_compress(m);
		if (m == NULL)
		    goto finish;
		goto again;
	    }
	    if (segcnt & 1) {
		if (--free == 0) {
		    /*
		     * See if there's any unclaimed space in the
		     * transmit ring.
		     */
		    if ((free += tulip_tx_intr(sc)) == 0) {
			/*
			 * There's no more room but since nothing
			 * has been committed at this point, just
			 * show output is active, put back the
			 * mbuf and return.
			 */
			sc->tulip_flags |= TULIP_WANTTXSTART;
			goto finish;
		    }
		}
		eop = nextout;
		if (++nextout == ri->ri_last)
		    nextout = ri->ri_first;
		eop->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
		eop->d_status = d_status;
		eop->d_addr1 = TULIP_KVATOPHYS(sc, addr);
		eop->d_length1 = slen;
	    } else {
		/*
		 *  Fill in second half of descriptor
		 */
		eop->d_addr2 = TULIP_KVATOPHYS(sc, addr);
		eop->d_length2 = slen;
	    }
	    d_status = TULIP_DSTS_OWNER;
	    len -= slen;
	    addr += slen;
#ifdef BIG_PACKET
	    if (partial)
		continue;
#endif
	    clsize = PAGE_SIZE;
	}
    } while ((m0 = m0->m_next) != NULL);

    BPF_MTAP(&sc->tulip_if, m);

    /*
     * The descriptors have been filled in.  Now get ready
     * to transmit.
     */
    IF_ENQUEUE(&sc->tulip_txq, m);
    m = NULL;

    /*
     * Make sure the next descriptor after this packet is owned
     * by us since it may have been set up above if we ran out
     * of room in the ring.
     */
    nextout->d_status = 0;
    TULIP_TXDESC_PRESYNC(sc, nextout, sizeof(u_int32_t));

    /*
     * If we only used the first segment of the last descriptor,
     * make sure the second segment will not be used.
     */
    if (segcnt & 1) {
	eop->d_addr2 = 0;
	eop->d_length2 = 0;
    }

    /*
     * Mark the last and first segments, indicate we want a transmit
     * complete interrupt, and tell it to transmit!
     */
    eop->d_flag |= TULIP_DFLAG_TxLASTSEG|TULIP_DFLAG_TxWANTINTR;

    /*
     * Note that ri->ri_nextout is still the start of the packet
     * and until we set the OWNER bit, we can still back out of
     * everything we have done.
     */
    ri->ri_nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG;
    ri->ri_nextout->d_status = TULIP_DSTS_OWNER;
    TULIP_TXDESC_PRESYNC(sc, ri->ri_nextout, sizeof(u_int32_t));

    /*
     * This advances the ring for us.
     */
    ri->ri_nextout = nextout;
    ri->ri_free = free;

    if (sc->tulip_flags & TULIP_TXPROBE_ACTIVE) {
	TULIP_CSR_WRITE(sc, csr_txpoll, 1);
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	sc->tulip_if.if_start = tulip_ifstart;
	return NULL;
    }

    /*
     * switch back to the single queueing ifstart.
     */
    sc->tulip_flags &= ~TULIP_WANTTXSTART;
    if (sc->tulip_txtimer == 0)
	sc->tulip_txtimer = TULIP_TXTIMER;

    /*
     * If we want a txstart, there must be not enough space in the
     * transmit ring.  So we want to enable transmit done interrupts
     * so we can immediately reclaim some space.  When the transmit
     * interrupt is posted, the interrupt handler will call tx_intr
     * to reclaim space and then txstart (since WANTTXSTART is set).
     * txstart will move the packet into the transmit ring and clear
     * WANTTXSTART thereby causing TXINTR to be cleared.
     */
  finish:
    if (sc->tulip_flags & (TULIP_WANTTXSTART|TULIP_DOINGSETUP)) {
	sc->tulip_if.if_flags |= IFF_OACTIVE;
	sc->tulip_if.if_start = tulip_ifstart;
	if ((sc->tulip_intrmask & TULIP_STS_TXINTR) == 0) {
	    sc->tulip_intrmask |= TULIP_STS_TXINTR;
	    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	}
    } else if ((sc->tulip_flags & TULIP_PROMISC) == 0) {
	if (sc->tulip_intrmask & TULIP_STS_TXINTR) {
	    sc->tulip_intrmask &= ~TULIP_STS_TXINTR;
	    TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
	}
    }
    TULIP_CSR_WRITE(sc, csr_txpoll, 1);
    return m;
}

static void
tulip_txput_setup(tulip_softc_t *sc)
{
    tulip_ringinfo_t *ri = &sc->tulip_txinfo;
    tulip_desc_t *nextout;
	
    /*
     * We will transmit, at most, one setup packet per call to ifstart.
     */

    /*
     * Try to reclaim some free descriptors..
     */
    if (ri->ri_free < 2)
	tulip_tx_intr(sc);
    if ((sc->tulip_flags & TULIP_DOINGSETUP) || ri->ri_free == 1) {
	sc->tulip_flags |= TULIP_WANTTXSTART;
	sc->tulip_if.if_start = tulip_ifstart;
	return;
    }
    bcopy(sc->tulip_setupdata, sc->tulip_setupbuf,
	  sizeof(sc->tulip_setupbuf));
    /*
     * Clear WANTSETUP and set DOINGSETUP.  Set know that WANTSETUP is
     * set and DOINGSETUP is clear doing an XOR of the two will DTRT.
     */
    sc->tulip_flags ^= TULIP_WANTSETUP|TULIP_DOINGSETUP;
    ri->ri_free--;
    nextout = ri->ri_nextout;
    nextout->d_flag &= TULIP_DFLAG_ENDRING|TULIP_DFLAG_CHAIN;
    nextout->d_flag |= TULIP_DFLAG_TxFIRSTSEG|TULIP_DFLAG_TxLASTSEG
	|TULIP_DFLAG_TxSETUPPKT|TULIP_DFLAG_TxWANTINTR;
    if (sc->tulip_flags & TULIP_WANTHASHPERFECT)
	nextout->d_flag |= TULIP_DFLAG_TxHASHFILT;
    else if (sc->tulip_flags & TULIP_WANTHASHONLY)
	nextout->d_flag |= TULIP_DFLAG_TxHASHFILT|TULIP_DFLAG_TxINVRSFILT;

    nextout->d_length2 = 0;
    nextout->d_addr2 = 0;
    nextout->d_length1 = sizeof(sc->tulip_setupbuf);
    nextout->d_addr1 = TULIP_KVATOPHYS(sc, sc->tulip_setupbuf);

    /*
     * Advance the ring for the next transmit packet.
     */
    if (++ri->ri_nextout == ri->ri_last)
	ri->ri_nextout = ri->ri_first;

    /*
     * Make sure the next descriptor is owned by us since it
     * may have been set up above if we ran out of room in the
     * ring.
     */
    ri->ri_nextout->d_status = 0;
    TULIP_TXDESC_PRESYNC(sc, ri->ri_nextout, sizeof(u_int32_t));
    nextout->d_status = TULIP_DSTS_OWNER;
    /*
     * Flush the ownwership of the current descriptor
     */
    TULIP_TXDESC_PRESYNC(sc, nextout, sizeof(u_int32_t));
    TULIP_CSR_WRITE(sc, csr_txpoll, 1);
    if ((sc->tulip_intrmask & TULIP_STS_TXINTR) == 0) {
	sc->tulip_intrmask |= TULIP_STS_TXINTR;
	TULIP_CSR_WRITE(sc, csr_intr, sc->tulip_intrmask);
    }
}

static int
tulip_ifioctl(struct ifnet *ifp, u_long cmd, caddr_t data, struct ucred * cr)
{
    tulip_softc_t *sc = (tulip_softc_t *)ifp->if_softc;
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *)data;
    int s;
    int error = 0;

    s = splimp();
    switch (cmd) {
	case SIOCSIFADDR: {
	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    tulip_init(sc);
		    arp_ifinit(&(sc)->tulip_ac.ac_if, ifa);
		    break;
		}
#endif /* INET */

#ifdef IPX
		case AF_IPX: {
		    struct ipx_addr *ina = &(IA_SIPX(ifa)->sipx_addr);
		    if (ipx_nullhost(*ina)) {
			ina->x_host = *(union ipx_host *)(sc->tulip_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->tulip_enaddr,
			      sizeof(sc->tulip_enaddr));
		    }
		    tulip_init(sc);
		    break;
		}
#endif /* IPX */

#ifdef NS
		/*
		 * This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->tulip_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->tulip_enaddr,
			      sizeof(sc->tulip_enaddr));
		    }
		    tulip_init(sc);
		    break;
		}
#endif /* NS */

		default: {
		    tulip_init(sc);
		    break;
		}
	    }
	    break;
	}
	case SIOCGIFADDR: {
	    bcopy((caddr_t) sc->tulip_enaddr,
		  (caddr_t) ((struct sockaddr *)&ifr->ifr_data)->sa_data,
		  6);
	    break;
	}

	case SIOCSIFFLAGS: {
	    tulip_addr_filter(sc); /* reinit multicast filter */
	    tulip_init(sc);
	    break;
	}

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA: {
	    error = ifmedia_ioctl(ifp, ifr, &sc->tulip_ifmedia, cmd);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    tulip_addr_filter(sc);		/* reset multicast filtering */
	    tulip_init(sc);
	    error = 0;
	    break;
	}

	case SIOCSIFMTU:
	    /*
	     * Set the interface MTU.
	     */
	    if (ifr->ifr_mtu > ETHERMTU
#ifdef BIG_PACKET
		    && sc->tulip_chipid != TULIP_21140
		    && sc->tulip_chipid != TULIP_21140A
		    && sc->tulip_chipid != TULIP_21041
#endif
		) {
		error = EINVAL;
		break;
	    }
	    ifp->if_mtu = ifr->ifr_mtu;
#ifdef BIG_PACKET
	    tulip_reset(sc);
	    tulip_init(sc);
#endif
	    break;

#ifdef SIOCGADDRROM
	case SIOCGADDRROM: {
	    error = copyout(sc->tulip_rombuf, ifr->ifr_data, sizeof(sc->tulip_rombuf));
	    break;
	}
#endif
#ifdef SIOCGCHIPID
	case SIOCGCHIPID: {
	    ifr->ifr_metric = (int) sc->tulip_chipid;
	    break;
	}
#endif
	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

static void
tulip_ifstart(struct ifnet *ifp)
{
    tulip_softc_t *sc = (tulip_softc_t *)ifp->if_softc;

    if (sc->tulip_if.if_flags & IFF_RUNNING) {

	if ((sc->tulip_flags & (TULIP_WANTSETUP|TULIP_TXPROBE_ACTIVE)) == TULIP_WANTSETUP)
	    tulip_txput_setup(sc);

	while (sc->tulip_if.if_snd.ifq_head != NULL) {
	    struct mbuf *m;
	    IF_DEQUEUE(&sc->tulip_if.if_snd, m);
	    if ((m = tulip_txput(sc, m)) != NULL) {
		IF_PREPEND(&sc->tulip_if.if_snd, m);
		break;
	    }
	}
    }
}

static void
tulip_ifwatchdog(struct ifnet *ifp)
{
    tulip_softc_t * const sc = (tulip_softc_t *)ifp->if_softc;

    sc->tulip_if.if_timer = 1;
    /*
     * These should be rare so do a bulk test up front so we can just skip
     * them if needed.
     */
    if (sc->tulip_flags & (TULIP_SYSTEMERROR|TULIP_RXBUFSLOW|TULIP_NOMESSAGES)) {
	/*
	 * If the number of receive buffer is low, try to refill
	 */
	if (sc->tulip_flags & TULIP_RXBUFSLOW)
	    tulip_rx_intr(sc);

	if (sc->tulip_flags & TULIP_SYSTEMERROR) {
	    printf("%s%d: %d system errors: last was %s\n",
		   sc->tulip_name, sc->tulip_unit, sc->tulip_system_errors,
		   tulip_system_errors[sc->tulip_last_system_error]);
	}
	if (sc->tulip_statusbits) {
	    tulip_print_abnormal_interrupt(sc, sc->tulip_statusbits);
	    sc->tulip_statusbits = 0;
	}

	sc->tulip_flags &= ~(TULIP_NOMESSAGES|TULIP_SYSTEMERROR);
    }

    if (sc->tulip_txtimer)
	tulip_tx_intr(sc);
    if (sc->tulip_txtimer && --sc->tulip_txtimer == 0) {
	printf("%s%d: transmission timeout\n", sc->tulip_name, sc->tulip_unit);
	if (TULIP_DO_AUTOSENSE(sc)) {
	    sc->tulip_media = TULIP_MEDIA_UNKNOWN;
	    sc->tulip_probe_state = TULIP_PROBE_INACTIVE;
	    sc->tulip_flags &= ~(TULIP_WANTRXACT|TULIP_LINKUP);
	}
	tulip_reset(sc);
	tulip_init(sc);
    }
}

static void
tulip_attach(tulip_softc_t *sc)
{
    struct ifnet *ifp = &sc->tulip_if;

    callout_init(&sc->tulip_timer);
    callout_init(&sc->tulip_fast_timer);

    if_initname(ifp, sc->tulip_name, sc->tulip_unit);
    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
    ifp->if_ioctl = tulip_ifioctl;
    ifp->if_start = tulip_ifstart;
    ifp->if_watchdog = tulip_ifwatchdog;
    ifp->if_timer = 1;
  
    printf("%s%d: %s%s pass %d.%d%s\n",
	   sc->tulip_name, sc->tulip_unit,
	   sc->tulip_boardid,
	   tulip_chipdescs[sc->tulip_chipid],
	   (sc->tulip_revinfo & 0xF0) >> 4,
	   sc->tulip_revinfo & 0x0F,
	   (sc->tulip_features & (TULIP_HAVE_ISVSROM|TULIP_HAVE_OKSROM))
		 == TULIP_HAVE_ISVSROM ? " (invalid EESPROM checksum)" : "");

    (*sc->tulip_boardsw->bd_media_probe)(sc);
    ifmedia_init(&sc->tulip_ifmedia, 0,
		 tulip_ifmedia_change,
		 tulip_ifmedia_status);
    sc->tulip_flags &= ~TULIP_DEVICEPROBE;
    tulip_ifmedia_add(sc);

    tulip_reset(sc);

    ether_ifattach(&(sc)->tulip_if, sc->tulip_enaddr);
    ifp->if_snd.ifq_maxlen = ifqmaxlen;
}

static void
tulip_initcsrs(tulip_softc_t *sc, tulip_csrptr_t csr_base, size_t csr_size)
{
    sc->tulip_csrs.csr_busmode		= csr_base +  0 * csr_size;
    sc->tulip_csrs.csr_txpoll		= csr_base +  1 * csr_size;
    sc->tulip_csrs.csr_rxpoll		= csr_base +  2 * csr_size;
    sc->tulip_csrs.csr_rxlist		= csr_base +  3 * csr_size;
    sc->tulip_csrs.csr_txlist		= csr_base +  4 * csr_size;
    sc->tulip_csrs.csr_status		= csr_base +  5 * csr_size;
    sc->tulip_csrs.csr_command		= csr_base +  6 * csr_size;
    sc->tulip_csrs.csr_intr		= csr_base +  7 * csr_size;
    sc->tulip_csrs.csr_missed_frames	= csr_base +  8 * csr_size;
    sc->tulip_csrs.csr_9		= csr_base +  9 * csr_size;
    sc->tulip_csrs.csr_10		= csr_base + 10 * csr_size;
    sc->tulip_csrs.csr_11		= csr_base + 11 * csr_size;
    sc->tulip_csrs.csr_12		= csr_base + 12 * csr_size;
    sc->tulip_csrs.csr_13		= csr_base + 13 * csr_size;
    sc->tulip_csrs.csr_14		= csr_base + 14 * csr_size;
    sc->tulip_csrs.csr_15		= csr_base + 15 * csr_size;
}

static void
tulip_initring(tulip_softc_t *sc, tulip_ringinfo_t *ri, tulip_desc_t *descs,
	       int ndescs)
{
    ri->ri_max = ndescs;
    ri->ri_first = descs;
    ri->ri_last = ri->ri_first + ri->ri_max;
    bzero((caddr_t) ri->ri_first, sizeof(ri->ri_first[0]) * ri->ri_max);
    ri->ri_last[-1].d_flag = TULIP_DFLAG_ENDRING;
}

/*
 * This is the PCI configuration support.
 */

#define	PCI_CBIO	0x10	/* Configuration Base IO Address */
#define	PCI_CBMA	0x14	/* Configuration Base Memory Address */
#define	PCI_CFDA	0x40	/* Configuration Driver Area */

static int
tulip_pci_probe(device_t dev)
{
    const char *name = NULL;

    if (pci_get_vendor(dev) != DEC_VENDORID)
	return ENXIO;

    /*
     * Some LanMedia WAN cards use the Tulip chip, but they have
     * their own driver, and we should not recognize them
     */
    if (pci_get_subvendor(dev) == 0x1376)
	return ENXIO;

    switch (pci_get_device(dev)) {
    case CHIPID_21040:
	name = "Digital 21040 Ethernet";
	break;
    case CHIPID_21041:
	name = "Digital 21041 Ethernet";
	break;
    case CHIPID_21140:
	if (pci_get_revid(dev) >= 0x20)
	    name = "Digital 21140A Fast Ethernet";
	else
	    name = "Digital 21140 Fast Ethernet";
	break;
    case CHIPID_21142:
	if (pci_get_revid(dev) >= 0x20)
	    name = "Digital 21143 Fast Ethernet";
	else
	    name = "Digital 21142 Fast Ethernet";
	break;
    }
    if (name) {
	device_set_desc(dev, name);
	return -200;
    }
    return ENXIO;
}

static int
tulip_shutdown(device_t dev)
{
    tulip_softc_t *sc = device_get_softc(dev);
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(10);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */
    return 0;
}

static int
tulip_pci_attach(device_t dev)
{
    tulip_softc_t *sc;
    int retval, idx;
    uint32_t revinfo, cfdainfo, cfcsinfo;
    u_int csroffset = TULIP_PCI_CSROFFSET;
    u_int csrsize = TULIP_PCI_CSRSIZE;
    tulip_csrptr_t csr_base;
    tulip_chipid_t chipid = TULIP_CHIPID_UNKNOWN;
    struct resource *res;
    int rid, unit;

    unit = device_get_unit(dev);

    if (unit >= TULIP_MAX_DEVICES) {
	printf("de%d", unit);
	printf(": not configured; limit of %d reached or exceeded\n",
	       TULIP_MAX_DEVICES);
	return ENXIO;
    }

    revinfo  = pci_get_revid(dev);
    cfdainfo = pci_read_config(dev, PCI_CFDA, 4);
    cfcsinfo = pci_read_config(dev, PCIR_COMMAND, 4);

    /* turn busmaster on in case BIOS doesn't set it */
    pci_enable_busmaster(dev);

    if (pci_get_vendor(dev) == DEC_VENDORID) {
	if (pci_get_device(dev) == CHIPID_21040)
		chipid = TULIP_21040;
	else if (pci_get_device(dev) == CHIPID_21041)
		chipid = TULIP_21041;
	else if (pci_get_device(dev) == CHIPID_21140)
		chipid = (revinfo >= 0x20) ? TULIP_21140A : TULIP_21140;
	else if (pci_get_device(dev) == CHIPID_21142)
		chipid = (revinfo >= 0x20) ? TULIP_21143 : TULIP_21142;
    }
    if (chipid == TULIP_CHIPID_UNKNOWN)
	return ENXIO;

    if (chipid == TULIP_21040 && revinfo < 0x20) {
	printf("de%d", unit);
	printf(": not configured; 21040 pass 2.0 required (%d.%d found)\n",
	       revinfo >> 4, revinfo & 0x0f);
	return ENXIO;
    } else if (chipid == TULIP_21140 && revinfo < 0x11) {
	printf("de%d: not configured; 21140 pass 1.1 required (%d.%d found)\n",
	       unit, revinfo >> 4, revinfo & 0x0f);
	return ENXIO;
    }

    sc = device_get_softc(dev);
    sc->tulip_pci_busno = pci_get_bus(dev);
    sc->tulip_pci_devno = pci_get_slot(dev);
    sc->tulip_chipid = chipid;
    sc->tulip_flags |= TULIP_DEVICEPROBE;
    if (chipid == TULIP_21140 || chipid == TULIP_21140A)
	sc->tulip_features |= TULIP_HAVE_GPR|TULIP_HAVE_STOREFWD;
    if (chipid == TULIP_21140A && revinfo <= 0x22)
	sc->tulip_features |= TULIP_HAVE_RXBADOVRFLW;
    if (chipid == TULIP_21140)
	sc->tulip_features |= TULIP_HAVE_BROKEN_HASH;
    if (chipid != TULIP_21040 && chipid != TULIP_21140)
	sc->tulip_features |= TULIP_HAVE_POWERMGMT;
    if (chipid == TULIP_21041 || chipid == TULIP_21142 || chipid == TULIP_21143) {
	sc->tulip_features |= TULIP_HAVE_DUALSENSE;
	if (chipid != TULIP_21041 || revinfo >= 0x20)
	    sc->tulip_features |= TULIP_HAVE_SIANWAY;
	if (chipid != TULIP_21041)
	    sc->tulip_features |= TULIP_HAVE_SIAGP|TULIP_HAVE_RXBADOVRFLW|TULIP_HAVE_STOREFWD;
	if (chipid != TULIP_21041 && revinfo >= 0x20)
	    sc->tulip_features |= TULIP_HAVE_SIA100;
    }

    if (sc->tulip_features & TULIP_HAVE_POWERMGMT
	    && (cfdainfo & (TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE))) {
	cfdainfo &= ~(TULIP_CFDA_SLEEP|TULIP_CFDA_SNOOZE);
	pci_write_config(dev, PCI_CFDA, cfdainfo, 4);
	DELAY(11*1000);
    }
    sc->tulip_unit = unit;
    sc->tulip_name = "de";
    sc->tulip_revinfo = revinfo;
    sc->tulip_if.if_softc = sc;
#if defined(TULIP_IOMAPPED)
    rid = PCI_CBIO;
    res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
			     0, ~0, 1, RF_ACTIVE);
#else
    rid = PCI_CBMA;
    res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
			     0, ~0, 1, RF_ACTIVE);
#endif
    if (!res)
	return ENXIO;
    sc->tulip_csrs_bst = rman_get_bustag(res);
    sc->tulip_csrs_bsh = rman_get_bushandle(res);
    csr_base = 0;

    tulips[unit] = sc;

    tulip_initcsrs(sc, csr_base + csroffset, csrsize);

    sc->tulip_rxdescs = malloc(sizeof(tulip_desc_t) * TULIP_RXDESCS, 
				M_DEVBUF, M_INTWAIT);
    sc->tulip_txdescs = malloc(sizeof(tulip_desc_t) * TULIP_TXDESCS,
				M_DEVBUF, M_INTWAIT);

    tulip_initring(sc, &sc->tulip_rxinfo, sc->tulip_rxdescs, TULIP_RXDESCS);
    tulip_initring(sc, &sc->tulip_txinfo, sc->tulip_txdescs, TULIP_TXDESCS);

    /*
     * Make sure there won't be any interrupts or such...
     */
    TULIP_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
    DELAY(100);	/* Wait 10 microseconds (actually 50 PCI cycles but at 
		   33MHz that comes to two microseconds but wait a
		   bit longer anyways) */

    if ((retval = tulip_read_macaddr(sc)) < 0) {
	printf("%s%d", sc->tulip_name, sc->tulip_unit);
	printf(": can't read ENET ROM (why=%d) (", retval);
	for (idx = 0; idx < 32; idx++)
	    printf("%02x", sc->tulip_rombuf[idx]);
	printf("\n");
	printf("%s%d: %s%s pass %d.%d\n",
	       sc->tulip_name, sc->tulip_unit,
	       sc->tulip_boardid, tulip_chipdescs[sc->tulip_chipid],
	       (sc->tulip_revinfo & 0xF0) >> 4, sc->tulip_revinfo & 0x0F);
	printf("%s%d: address unknown\n", sc->tulip_name, sc->tulip_unit);
    } else {
	int s;
	void (*intr_rtn)(void *) = tulip_intr_normal;

	if (sc->tulip_features & TULIP_HAVE_SHAREDINTR)
	    intr_rtn = tulip_intr_shared;

	if ((sc->tulip_features & TULIP_HAVE_SLAVEDINTR) == 0) {
	    void *ih;

	    rid = 0;
	    res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				     0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	    if (res == 0 || bus_setup_intr(dev, res, INTR_TYPE_NET,
					   intr_rtn, sc, &ih)) {
		printf("%s%d: couldn't map interrupt\n",
		       sc->tulip_name, sc->tulip_unit);
		free((caddr_t) sc->tulip_rxdescs, M_DEVBUF);
		free((caddr_t) sc->tulip_txdescs, M_DEVBUF);
		return ENXIO;
	    }
	}

	s = splimp();
	tulip_attach(sc);
	splx(s);
    }
    return 0;
}

static device_method_t tulip_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	tulip_pci_probe),
    DEVMETHOD(device_attach,	tulip_pci_attach),
    DEVMETHOD(device_shutdown,	tulip_shutdown),
    { 0, 0 }
};
static driver_t tulip_pci_driver = {
    "de",
    tulip_pci_methods,
    sizeof(tulip_softc_t),
};
static devclass_t tulip_devclass;

DECLARE_DUMMY_MODULE(if_de);
DRIVER_MODULE(if_de, pci, tulip_pci_driver, tulip_devclass, 0, 0);

