/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pci/if_wb.c,v 1.26.2.6 2003/03/05 18:42:34 njl Exp $
 * $DragonFly: src/sys/dev/netif/wb/if_wb.c,v 1.17 2005/02/21 18:40:37 joerg Exp $
 */

/*
 * Winbond fast ethernet PCI NIC driver
 *
 * Supports various cheap network adapters based on the Winbond W89C840F
 * fast ethernet controller chip. This includes adapters manufactured by
 * Winbond itself and some made by Linksys.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Winbond W89C840F chip is a bus master; in some ways it resembles
 * a DEC 'tulip' chip, only not as complicated. Unfortunately, it has
 * one major difference which is that while the registers do many of
 * the same things as a tulip adapter, the offsets are different: where
 * tulip registers are typically spaced 8 bytes apart, the Winbond
 * registers are spaced 4 bytes apart. The receiver filter is also
 * programmed differently.
 * 
 * Like the tulip, the Winbond chip uses small descriptors containing
 * a status word, a control word and 32-bit areas that can either be used
 * to point to two external data blocks, or to point to a single block
 * and another descriptor in a linked list. Descriptors can be grouped
 * together in blocks to form fixed length rings or can be chained
 * together in linked lists. A single packet may be spread out over
 * several descriptors if necessary.
 *
 * For the receive ring, this driver uses a linked list of descriptors,
 * each pointing to a single mbuf cluster buffer, which us large enough
 * to hold an entire packet. The link list is looped back to created a
 * closed ring.
 *
 * For transmission, the driver creates a linked list of 'super descriptors'
 * which each contain several individual descriptors linked toghether.
 * Each 'super descriptor' contains WB_MAXFRAGS descriptors, which we
 * abuse as fragment pointers. This allows us to use a buffer managment
 * scheme very similar to that used in the ThunderLAN and Etherlink XL
 * drivers.
 *
 * Autonegotiation is performed using the external PHY via the MII bus.
 * The sample boards I have all use a Davicom PHY.
 *
 * Note: the author of the Linux driver for the Winbond chip alludes
 * to some sort of flaw in the chip's design that seems to mandate some
 * drastic workaround which signigicantly impairs transmit performance.
 * I have no idea what he's on about: transmit performance with all
 * three of my test boards seems fine.
 */

#include "opt_bdg.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/ifq_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

#include "../mii_layer/mii.h"
#include "../mii_layer/miivar.h"

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#define WB_USEIOSPACE

#include "if_wbreg.h"

/*
 * Various supported device vendors/types and their names.
 */
static struct wb_type wb_devs[] = {
	{ WB_VENDORID, WB_DEVICEID_840F,
		"Winbond W89C840F 10/100BaseTX" },
	{ CP_VENDORID, CP_DEVICEID_RL100,
		"Compex RL100-ATX 10/100baseTX" },
	{ 0, 0, NULL }
};

static int wb_probe		(device_t);
static int wb_attach		(device_t);
static int wb_detach		(device_t);

static void wb_bfree		(caddr_t, u_int);
static int wb_newbuf		(struct wb_softc *,
					struct wb_chain_onefrag *,
					struct mbuf *);
static int wb_encap		(struct wb_softc *, struct wb_chain *,
					struct mbuf *);

static void wb_rxeof		(struct wb_softc *);
static void wb_rxeoc		(struct wb_softc *);
static void wb_txeof		(struct wb_softc *);
static void wb_txeoc		(struct wb_softc *);
static void wb_intr		(void *);
static void wb_tick		(void *);
static void wb_start		(struct ifnet *);
static int wb_ioctl		(struct ifnet *, u_long, caddr_t,
					struct ucred *);
static void wb_init		(void *);
static void wb_stop		(struct wb_softc *);
static void wb_watchdog		(struct ifnet *);
static void wb_shutdown		(device_t);
static int wb_ifmedia_upd	(struct ifnet *);
static void wb_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void wb_eeprom_putbyte	(struct wb_softc *, int);
static void wb_eeprom_getword	(struct wb_softc *, int, u_int16_t *);
static void wb_read_eeprom	(struct wb_softc *, caddr_t, int,
							int, int);
static void wb_mii_sync		(struct wb_softc *);
static void wb_mii_send		(struct wb_softc *, u_int32_t, int);
static int wb_mii_readreg	(struct wb_softc *, struct wb_mii_frame *);
static int wb_mii_writereg	(struct wb_softc *, struct wb_mii_frame *);

static void wb_setcfg		(struct wb_softc *, u_int32_t);
static u_int8_t wb_calchash	(caddr_t);
static void wb_setmulti		(struct wb_softc *);
static void wb_reset		(struct wb_softc *);
static void wb_fixmedia		(struct wb_softc *);
static int wb_list_rx_init	(struct wb_softc *);
static int wb_list_tx_init	(struct wb_softc *);

static int wb_miibus_readreg	(device_t, int, int);
static int wb_miibus_writereg	(device_t, int, int, int);
static void wb_miibus_statchg	(device_t);

#ifdef WB_USEIOSPACE
#define WB_RES			SYS_RES_IOPORT
#define WB_RID			WB_PCI_LOIO
#else
#define WB_RES			SYS_RES_MEMORY
#define WB_RID			WB_PCI_LOMEM
#endif

static device_method_t wb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wb_probe),
	DEVMETHOD(device_attach,	wb_attach),
	DEVMETHOD(device_detach,	wb_detach),
	DEVMETHOD(device_shutdown,	wb_shutdown),

	/* bus interface, for miibus */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	wb_miibus_readreg),
	DEVMETHOD(miibus_writereg,	wb_miibus_writereg),
	DEVMETHOD(miibus_statchg,	wb_miibus_statchg),
	{ 0, 0 }
};

static driver_t wb_driver = {
	"wb",
	wb_methods,
	sizeof(struct wb_softc)
};

static devclass_t wb_devclass;

DECLARE_DUMMY_MODULE(if_wb);
DRIVER_MODULE(if_wb, pci, wb_driver, wb_devclass, 0, 0);
DRIVER_MODULE(miibus, wb, miibus_driver, miibus_devclass, 0, 0);

#define WB_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define WB_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, WB_SIO,				\
		CSR_READ_4(sc, WB_SIO) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, WB_SIO,				\
		CSR_READ_4(sc, WB_SIO) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void wb_eeprom_putbyte(sc, addr)
	struct wb_softc		*sc;
	int			addr;
{
	int		d, i;

	d = addr | WB_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(WB_SIO_EE_DATAIN);
		} else {
			SIO_CLR(WB_SIO_EE_DATAIN);
		}
		DELAY(100);
		SIO_SET(WB_SIO_EE_CLK);
		DELAY(150);
		SIO_CLR(WB_SIO_EE_CLK);
		DELAY(100);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void wb_eeprom_getword(sc, addr, dest)
	struct wb_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, WB_SIO, WB_SIO_EESEL|WB_SIO_EE_CS);

	/*
	 * Send address of word we want to read.
	 */
	wb_eeprom_putbyte(sc, addr);

	CSR_WRITE_4(sc, WB_SIO, WB_SIO_EESEL|WB_SIO_EE_CS);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(WB_SIO_EE_CLK);
		DELAY(100);
		if (CSR_READ_4(sc, WB_SIO) & WB_SIO_EE_DATAOUT)
			word |= i;
		SIO_CLR(WB_SIO_EE_CLK);
		DELAY(100);
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_4(sc, WB_SIO, 0);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void wb_read_eeprom(sc, dest, off, cnt, swap)
	struct wb_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		wb_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void wb_mii_sync(sc)
	struct wb_softc		*sc;
{
	int		i;

	SIO_SET(WB_SIO_MII_DIR|WB_SIO_MII_DATAIN);

	for (i = 0; i < 32; i++) {
		SIO_SET(WB_SIO_MII_CLK);
		DELAY(1);
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void wb_mii_send(sc, bits, cnt)
	struct wb_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	SIO_CLR(WB_SIO_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			SIO_SET(WB_SIO_MII_DATAIN);
                } else {
			SIO_CLR(WB_SIO_MII_DATAIN);
                }
		DELAY(1);
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
		SIO_SET(WB_SIO_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int wb_mii_readreg(sc, frame)
	struct wb_softc		*sc;
	struct wb_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = WB_MII_STARTDELIM;
	frame->mii_opcode = WB_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_4(sc, WB_SIO, 0);

	/*
 	 * Turn on data xmit.
	 */
	SIO_SET(WB_SIO_MII_DIR);

	wb_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	wb_mii_send(sc, frame->mii_stdelim, 2);
	wb_mii_send(sc, frame->mii_opcode, 2);
	wb_mii_send(sc, frame->mii_phyaddr, 5);
	wb_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	SIO_CLR((WB_SIO_MII_CLK|WB_SIO_MII_DATAIN));
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(WB_SIO_MII_DIR);
	/* Check for ack */
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, WB_SIO) & WB_SIO_MII_DATAOUT;
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(WB_SIO_MII_CLK);
			DELAY(1);
			SIO_SET(WB_SIO_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, WB_SIO) & WB_SIO_MII_DATAOUT)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(WB_SIO_MII_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int wb_mii_writereg(sc, frame)
	struct wb_softc		*sc;
	struct wb_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = WB_MII_STARTDELIM;
	frame->mii_opcode = WB_MII_WRITEOP;
	frame->mii_turnaround = WB_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	SIO_SET(WB_SIO_MII_DIR);

	wb_mii_sync(sc);

	wb_mii_send(sc, frame->mii_stdelim, 2);
	wb_mii_send(sc, frame->mii_opcode, 2);
	wb_mii_send(sc, frame->mii_phyaddr, 5);
	wb_mii_send(sc, frame->mii_regaddr, 5);
	wb_mii_send(sc, frame->mii_turnaround, 2);
	wb_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	SIO_CLR(WB_SIO_MII_DIR);

	splx(s);

	return(0);
}

static int wb_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct wb_softc		*sc;
	struct wb_mii_frame	frame;

	sc = device_get_softc(dev);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	wb_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int wb_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct wb_softc		*sc;
	struct wb_mii_frame	frame;

	sc = device_get_softc(dev);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	wb_mii_writereg(sc, &frame);

	return(0);
}

static void wb_miibus_statchg(dev)
	device_t		dev;
{
	struct wb_softc		*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->wb_miibus);
	wb_setcfg(sc, mii->mii_media_active);

	return;
}

static u_int8_t wb_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		crc, carry;
	int			i, j;
	u_int8_t		c;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (i = 0; i < 6; i++) {
		c = *(addr + i);
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/*
	 * return the filter bit position
	 * Note: I arrived at the following nonsense
	 * through experimentation. It's not the usual way to
	 * generate the bit position but it's the only thing
	 * I could come up with that works.
	 */
	return(~(crc >> 26) & 0x0000003F);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void wb_setmulti(sc)
	struct wb_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, WB_NETCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= WB_NETCFG_RX_MULTI;
		CSR_WRITE_4(sc, WB_NETCFG, rxfilt);
		CSR_WRITE_4(sc, WB_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, WB_MAR1, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, WB_MAR0, 0);
	CSR_WRITE_4(sc, WB_MAR1, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = wb_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= WB_NETCFG_RX_MULTI;
	else
		rxfilt &= ~WB_NETCFG_RX_MULTI;

	CSR_WRITE_4(sc, WB_MAR0, hashes[0]);
	CSR_WRITE_4(sc, WB_MAR1, hashes[1]);
	CSR_WRITE_4(sc, WB_NETCFG, rxfilt);

	return;
}

/*
 * The Winbond manual states that in order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void wb_setcfg(sc, media)
	struct wb_softc		*sc;
	u_int32_t		media;
{
	int			i, restart = 0;

	if (CSR_READ_4(sc, WB_NETCFG) & (WB_NETCFG_TX_ON|WB_NETCFG_RX_ON)) {
		restart = 1;
		WB_CLRBIT(sc, WB_NETCFG, (WB_NETCFG_TX_ON|WB_NETCFG_RX_ON));

		for (i = 0; i < WB_TIMEOUT; i++) {
			DELAY(10);
			if ((CSR_READ_4(sc, WB_ISR) & WB_ISR_TX_IDLE) &&
				(CSR_READ_4(sc, WB_ISR) & WB_ISR_RX_IDLE))
				break;
		}

		if (i == WB_TIMEOUT)
			printf("wb%d: failed to force tx and "
				"rx to idle state\n", sc->wb_unit);
	}

	if (IFM_SUBTYPE(media) == IFM_10_T)
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_100MBPS);
	else
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_100MBPS);

	if ((media & IFM_GMASK) == IFM_FDX)
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_FULLDUPLEX);
	else
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_FULLDUPLEX);

	if (restart)
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON|WB_NETCFG_RX_ON);

	return;
}

static void wb_reset(sc)
	struct wb_softc		*sc;
{
	int		i;
	struct mii_data		*mii;

	CSR_WRITE_4(sc, WB_NETCFG, 0);
	CSR_WRITE_4(sc, WB_BUSCTL, 0);
	CSR_WRITE_4(sc, WB_TXADDR, 0);
	CSR_WRITE_4(sc, WB_RXADDR, 0);

	WB_SETBIT(sc, WB_BUSCTL, WB_BUSCTL_RESET);
	WB_SETBIT(sc, WB_BUSCTL, WB_BUSCTL_RESET);

	for (i = 0; i < WB_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, WB_BUSCTL) & WB_BUSCTL_RESET))
			break;
	}
	if (i == WB_TIMEOUT)
		printf("wb%d: reset never completed!\n", sc->wb_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	if (sc->wb_miibus == NULL)
		return;

	mii = device_get_softc(sc->wb_miibus);
	if (mii == NULL)
		return;

        if (mii->mii_instance) {
                struct mii_softc        *miisc;
                for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
                                miisc = LIST_NEXT(miisc, mii_list))
                        mii_phy_reset(miisc);
        }

        return;
}

static void wb_fixmedia(sc)
	struct wb_softc		*sc;
{
	struct mii_data		*mii = NULL;
	struct ifnet		*ifp;
	u_int32_t		media;

	if (sc->wb_miibus == NULL)
		return;

	mii = device_get_softc(sc->wb_miibus);
	ifp = &sc->arpcom.ac_if;

	mii_pollstat(mii);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T) {
		media = mii->mii_media_active & ~IFM_10_T;
		media |= IFM_100_TX;
	} else if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		media = mii->mii_media_active & ~IFM_100_TX;
		media |= IFM_10_T;
	} else
		return;

	ifmedia_set(&mii->mii_media, media);

	return;
}

/*
 * Probe for a Winbond chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int wb_probe(dev)
	device_t		dev;
{
	struct wb_type		*t;

	t = wb_devs;

	while(t->wb_name != NULL) {
		if ((pci_get_vendor(dev) == t->wb_vid) &&
		    (pci_get_device(dev) == t->wb_did)) {
			device_set_desc(dev, t->wb_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int wb_attach(dev)
	device_t		dev;
{
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct wb_softc		*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	callout_init(&sc->wb_stat_timer);

	/*
	 * Handle power management nonsense.
	 */

	command = pci_read_config(dev, WB_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, WB_PCI_PWRMGMTCTRL, 4);
		if (command & WB_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, WB_PCI_LOIO, 4);
			membase = pci_read_config(dev, WB_PCI_LOMEM, 4);
			irq = pci_read_config(dev, WB_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("wb%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & WB_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, WB_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, WB_PCI_LOIO, iobase, 4);
			pci_write_config(dev, WB_PCI_LOMEM, membase, 4);
			pci_write_config(dev, WB_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef WB_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("wb%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("wb%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}
#endif

	rid = WB_RID;
	sc->wb_res = bus_alloc_resource(dev, WB_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->wb_res == NULL) {
		printf("wb%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->wb_btag = rman_get_bustag(sc->wb_res);
	sc->wb_bhandle = rman_get_bushandle(sc->wb_res);

	/* Allocate interrupt */
	rid = 0;
	sc->wb_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->wb_irq == NULL) {
		printf("wb%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, WB_RES, WB_RID, sc->wb_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->wb_irq, INTR_TYPE_NET,
	    wb_intr, sc, &sc->wb_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->wb_irq);
		bus_release_resource(dev, WB_RES, WB_RID, sc->wb_res);
		printf("wb%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* Save the cache line size. */
	sc->wb_cachesize = pci_read_config(dev, WB_PCI_CACHELEN, 4) & 0xFF;

	/* Reset the adapter. */
	wb_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	wb_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 0);

	sc->wb_unit = unit;

	sc->wb_ldata = contigmalloc(sizeof(struct wb_list_data) + 8, M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->wb_ldata == NULL) {
		printf("wb%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->wb_irq, sc->wb_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->wb_irq);
		bus_release_resource(dev, WB_RES, WB_RID, sc->wb_res);
		error = ENXIO;
		goto fail;
	}

	bzero(sc->wb_ldata, sizeof(struct wb_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, "wb", unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wb_ioctl;
	ifp->if_start = wb_start;
	ifp->if_watchdog = wb_watchdog;
	ifp->if_init = wb_init;
	ifp->if_baudrate = 10000000;
	ifq_set_maxlen(&ifp->if_snd, WB_TX_LIST_CNT - 1);
	ifq_set_ready(&ifp->if_snd);

	/*
	 * Do MII setup.
	 */
	if (mii_phy_probe(dev, &sc->wb_miibus,
	    wb_ifmedia_upd, wb_ifmedia_sts)) {
		contigfree(sc->wb_ldata_ptr, sizeof(struct wb_list_data) + 8,
		    M_DEVBUF);
		bus_teardown_intr(dev, sc->wb_irq, sc->wb_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->wb_irq);
		bus_release_resource(dev, WB_RES, WB_RID, sc->wb_res);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

fail:
	if (error)
		device_delete_child(dev, sc->wb_miibus);
	splx(s);

	return(error);
}

static int wb_detach(dev)
	device_t		dev;
{
	struct wb_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	wb_stop(sc);
	ether_ifdetach(ifp);

	/* Delete any miibus and phy devices attached to this interface */
	bus_generic_detach(dev);
	device_delete_child(dev, sc->wb_miibus);

	bus_teardown_intr(dev, sc->wb_irq, sc->wb_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->wb_irq);
	bus_release_resource(dev, WB_RES, WB_RID, sc->wb_res);

	contigfree(sc->wb_ldata_ptr, sizeof(struct wb_list_data) + 8,
	    M_DEVBUF);

	splx(s);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int wb_list_tx_init(sc)
	struct wb_softc		*sc;
{
	struct wb_chain_data	*cd;
	struct wb_list_data	*ld;
	int			i;

	cd = &sc->wb_cdata;
	ld = sc->wb_ldata;

	for (i = 0; i < WB_TX_LIST_CNT; i++) {
		cd->wb_tx_chain[i].wb_ptr = &ld->wb_tx_list[i];
		if (i == (WB_TX_LIST_CNT - 1)) {
			cd->wb_tx_chain[i].wb_nextdesc =
				&cd->wb_tx_chain[0];
		} else {
			cd->wb_tx_chain[i].wb_nextdesc =
				&cd->wb_tx_chain[i + 1];
		}
	}

	cd->wb_tx_free = &cd->wb_tx_chain[0];
	cd->wb_tx_tail = cd->wb_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int wb_list_rx_init(sc)
	struct wb_softc		*sc;
{
	struct wb_chain_data	*cd;
	struct wb_list_data	*ld;
	int			i;

	cd = &sc->wb_cdata;
	ld = sc->wb_ldata;

	for (i = 0; i < WB_RX_LIST_CNT; i++) {
		cd->wb_rx_chain[i].wb_ptr =
			(struct wb_desc *)&ld->wb_rx_list[i];
		cd->wb_rx_chain[i].wb_buf = (void *)&ld->wb_rxbufs[i];
		if (wb_newbuf(sc, &cd->wb_rx_chain[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (WB_RX_LIST_CNT - 1)) {
			cd->wb_rx_chain[i].wb_nextdesc = &cd->wb_rx_chain[0];
			ld->wb_rx_list[i].wb_next = 
					vtophys(&ld->wb_rx_list[0]);
		} else {
			cd->wb_rx_chain[i].wb_nextdesc =
					&cd->wb_rx_chain[i + 1];
			ld->wb_rx_list[i].wb_next =
					vtophys(&ld->wb_rx_list[i + 1]);
		}
	}

	cd->wb_rx_head = &cd->wb_rx_chain[0];

	return(0);
}

static void wb_bfree(buf, size)
	caddr_t			buf;
	u_int			size;
{
	return;
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int wb_newbuf(sc, c, m)
	struct wb_softc		*sc;
	struct wb_chain_onefrag	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, MB_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		m_new->m_data = m_new->m_ext.ext_buf = c->wb_buf;
		m_new->m_flags |= M_EXT | M_EXT_OLD;
		m_new->m_ext.ext_size = m_new->m_pkthdr.len =
		    m_new->m_len = WB_BUFBYTES;
		m_new->m_ext.ext_nfree.old = wb_bfree;
		m_new->m_ext.ext_nref.old = wb_bfree;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = WB_BUFBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->wb_mbuf = m_new;
	c->wb_ptr->wb_data = vtophys(mtod(m_new, caddr_t));
	c->wb_ptr->wb_ctl = WB_RXCTL_RLINK | 1536;
	c->wb_ptr->wb_status = WB_RXSTAT;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void wb_rxeof(sc)
	struct wb_softc		*sc;
{
        struct mbuf		*m = NULL;
        struct ifnet		*ifp;
	struct wb_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->wb_cdata.wb_rx_head->wb_ptr->wb_status) &
							WB_RXSTAT_OWN)) {
		struct mbuf		*m0 = NULL;

		cur_rx = sc->wb_cdata.wb_rx_head;
		sc->wb_cdata.wb_rx_head = cur_rx->wb_nextdesc;

		m = cur_rx->wb_mbuf;

		if ((rxstat & WB_RXSTAT_MIIERR) ||
		    (WB_RXBYTES(cur_rx->wb_ptr->wb_status) < WB_MIN_FRAMELEN) ||
		    (WB_RXBYTES(cur_rx->wb_ptr->wb_status) > 1536) ||
		    !(rxstat & WB_RXSTAT_LASTFRAG) ||
		    !(rxstat & WB_RXSTAT_RXCMP)) {
			ifp->if_ierrors++;
			wb_newbuf(sc, cur_rx, m);
			printf("wb%x: receiver babbling: possible chip "
				"bug, forcing reset\n", sc->wb_unit);
			wb_fixmedia(sc);
			wb_reset(sc);
			wb_init(sc);
			return;
		}

		if (rxstat & WB_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			wb_newbuf(sc, cur_rx, m);
			break;
		}

		/* No errors; receive the packet. */	
		total_len = WB_RXBYTES(cur_rx->wb_ptr->wb_status);

		/*
		 * XXX The Winbond chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
	 	 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		     total_len + ETHER_ALIGN, 0, ifp, NULL);
		wb_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			break;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, m);
	}
}

void wb_rxeoc(sc)
	struct wb_softc		*sc;
{
	wb_rxeof(sc);

	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXADDR, vtophys(&sc->wb_ldata->wb_rx_list[0]));
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	if (CSR_READ_4(sc, WB_ISR) & WB_RXSTATE_SUSPEND)
		CSR_WRITE_4(sc, WB_RXSTART, 0xFFFFFFFF);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void wb_txeof(sc)
	struct wb_softc		*sc;
{
	struct wb_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->wb_cdata.wb_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->wb_cdata.wb_tx_head->wb_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->wb_cdata.wb_tx_head;
		txstat = WB_TXSTATUS(cur_tx);

		if ((txstat & WB_TXSTAT_OWN) || txstat == WB_UNSENT)
			break;

		if (txstat & WB_TXSTAT_TXERR) {
			ifp->if_oerrors++;
			if (txstat & WB_TXSTAT_ABORT)
				ifp->if_collisions++;
			if (txstat & WB_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & WB_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		m_freem(cur_tx->wb_mbuf);
		cur_tx->wb_mbuf = NULL;

		if (sc->wb_cdata.wb_tx_head == sc->wb_cdata.wb_tx_tail) {
			sc->wb_cdata.wb_tx_head = NULL;
			sc->wb_cdata.wb_tx_tail = NULL;
			break;
		}

		sc->wb_cdata.wb_tx_head = cur_tx->wb_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void wb_txeoc(sc)
	struct wb_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->wb_cdata.wb_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->wb_cdata.wb_tx_tail = NULL;
	} else {
		if (WB_TXOWN(sc->wb_cdata.wb_tx_head) == WB_UNSENT) {
			WB_TXOWN(sc->wb_cdata.wb_tx_head) = WB_TXSTAT_OWN;
			ifp->if_timer = 5;
			CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
		}
	}

	return;
}

static void wb_intr(arg)
	void			*arg;
{
	struct wb_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return;

	/* Disable interrupts. */
	CSR_WRITE_4(sc, WB_IMR, 0x00000000);

	for (;;) {

		status = CSR_READ_4(sc, WB_ISR);
		if (status)
			CSR_WRITE_4(sc, WB_ISR, status);

		if ((status & WB_INTRS) == 0)
			break;

		if ((status & WB_ISR_RX_NOBUF) || (status & WB_ISR_RX_ERR)) {
			ifp->if_ierrors++;
			wb_reset(sc);
			if (status & WB_ISR_RX_ERR)
				wb_fixmedia(sc);
			wb_init(sc);
			continue;
		}

		if (status & WB_ISR_RX_OK)
			wb_rxeof(sc);
	
		if (status & WB_ISR_RX_IDLE)
			wb_rxeoc(sc);

		if (status & WB_ISR_TX_OK)
			wb_txeof(sc);

		if (status & WB_ISR_TX_NOBUF)
			wb_txeoc(sc);

		if (status & WB_ISR_TX_IDLE) {
			wb_txeof(sc);
			if (sc->wb_cdata.wb_tx_head != NULL) {
				WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
				CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & WB_ISR_TX_UNDERRUN) {
			ifp->if_oerrors++;
			wb_txeof(sc);
			WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
			/* Jack up TX threshold */
			sc->wb_txthresh += WB_TXTHRESH_CHUNK;
			WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_THRESH);
			WB_SETBIT(sc, WB_NETCFG, WB_TXTHRESH(sc->wb_txthresh));
			WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
		}

		if (status & WB_ISR_BUS_ERR) {
			wb_reset(sc);
			wb_init(sc);
		}

	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, WB_IMR, WB_INTRS);

	if (!ifq_is_empty(&ifp->if_snd)) {
		wb_start(ifp);
	}

	return;
}

static void wb_tick(xsc)
	void			*xsc;
{
	struct wb_softc		*sc;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	sc = xsc;
	mii = device_get_softc(sc->wb_miibus);

	mii_tick(mii);

	callout_reset(&sc->wb_stat_timer, hz, wb_tick, sc);

	splx(s);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int wb_encap(sc, c, m_head)
	struct wb_softc		*sc;
	struct wb_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct wb_desc		*f = NULL;
	int			total_len;
	struct mbuf		*m;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	total_len = 0;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == WB_MAXFRAGS)
				break;
			total_len += m->m_len;
			f = &c->wb_ptr->wb_frag[frag];
			f->wb_ctl = WB_TXCTL_TLINK | m->m_len;
			if (frag == 0) {
				f->wb_ctl |= WB_TXCTL_FIRSTFRAG;
				f->wb_status = 0;
			} else
				f->wb_status = WB_TXSTAT_OWN;
			f->wb_next = vtophys(&c->wb_ptr->wb_frag[frag + 1]);
			f->wb_data = vtophys(mtod(m, vm_offset_t));
			frag++;
		}
	}

	/*
	 * Handle special case: we used up all 16 fragments,
	 * but we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, MB_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(1);
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, MB_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->wb_ptr->wb_frag[0];
		f->wb_status = 0;
		f->wb_data = vtophys(mtod(m_new, caddr_t));
		f->wb_ctl = total_len = m_new->m_len;
		f->wb_ctl |= WB_TXCTL_TLINK|WB_TXCTL_FIRSTFRAG;
		frag = 1;
	}

	if (total_len < WB_MIN_FRAMELEN) {
		f = &c->wb_ptr->wb_frag[frag];
		f->wb_ctl = WB_MIN_FRAMELEN - total_len;
		f->wb_data = vtophys(&sc->wb_cdata.wb_pad);
		f->wb_ctl |= WB_TXCTL_TLINK;
		f->wb_status = WB_TXSTAT_OWN;
		frag++;
	}

	c->wb_mbuf = m_head;
	c->wb_lastdesc = frag - 1;
	WB_TXCTL(c) |= WB_TXCTL_LASTFRAG;
	WB_TXNEXT(c) = vtophys(&c->wb_nextdesc->wb_ptr->wb_frag[0]);

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void wb_start(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct wb_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->wb_cdata.wb_tx_free->wb_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->wb_cdata.wb_tx_free;

	while(sc->wb_cdata.wb_tx_free->wb_mbuf == NULL) {
		m_head = ifq_dequeue(&ifp->if_snd);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->wb_cdata.wb_tx_free;
		sc->wb_cdata.wb_tx_free = cur_tx->wb_nextdesc;

		/* Pack the data into the descriptor. */
		wb_encap(sc, cur_tx, m_head);

		if (cur_tx != start_tx)
			WB_TXOWN(cur_tx) = WB_TXSTAT_OWN;

		BPF_MTAP(ifp, cur_tx->wb_mbuf);
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interupt once for the whole chain rather than
	 * once for each packet.
	 */
	WB_TXCTL(cur_tx) |= WB_TXCTL_FINT;
	cur_tx->wb_ptr->wb_frag[0].wb_ctl |= WB_TXCTL_FINT;
	sc->wb_cdata.wb_tx_tail = cur_tx;

	if (sc->wb_cdata.wb_tx_head == NULL) {
		sc->wb_cdata.wb_tx_head = start_tx;
		WB_TXOWN(start_tx) = WB_TXSTAT_OWN;
		CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
	} else {
		/*
		 * We need to distinguish between the case where
		 * the own bit is clear because the chip cleared it
		 * and where the own bit is clear because we haven't
		 * set it yet. The magic value WB_UNSET is just some
		 * ramdomly chosen number which doesn't have the own
	 	 * bit set. When we actually transmit the frame, the
		 * status word will have _only_ the own bit set, so
		 * the txeoc handler will be able to tell if it needs
		 * to initiate another transmission to flush out pending
		 * frames.
		 */
		WB_TXOWN(start_tx) = WB_UNSENT;
	}

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void wb_init(xsc)
	void			*xsc;
{
	struct wb_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s, i;
	struct mii_data		*mii;

	s = splimp();

	mii = device_get_softc(sc->wb_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	wb_stop(sc);
	wb_reset(sc);

	sc->wb_txthresh = WB_TXTHRESH_INIT;

	/*
	 * Set cache alignment and burst length.
	 */
#ifdef foo
	CSR_WRITE_4(sc, WB_BUSCTL, WB_BUSCTL_CONFIG);
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_THRESH);
	WB_SETBIT(sc, WB_NETCFG, WB_TXTHRESH(sc->wb_txthresh));
#endif

	CSR_WRITE_4(sc, WB_BUSCTL, WB_BUSCTL_MUSTBEONE|WB_BUSCTL_ARBITRATION);
	WB_SETBIT(sc, WB_BUSCTL, WB_BURSTLEN_16LONG);
	switch(sc->wb_cachesize) {
	case 32:
		WB_SETBIT(sc, WB_BUSCTL, WB_CACHEALIGN_32LONG);
		break;
	case 16:
		WB_SETBIT(sc, WB_BUSCTL, WB_CACHEALIGN_16LONG);
		break;
	case 8:
		WB_SETBIT(sc, WB_BUSCTL, WB_CACHEALIGN_8LONG);
		break;
	case 0:
	default:
		WB_SETBIT(sc, WB_BUSCTL, WB_CACHEALIGN_NONE);
		break;
	}

	/* This doesn't tend to work too well at 100Mbps. */
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_EARLY_ON);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, WB_NODE0 + i, sc->arpcom.ac_enaddr[i]);
	}

	/* Init circular RX list. */
	if (wb_list_rx_init(sc) == ENOBUFS) {
		printf("wb%d: initialization failed: no "
			"memory for rx buffers\n", sc->wb_unit);
		wb_stop(sc);
		(void)splx(s);
		return;
	}

	/* Init TX descriptors. */
	wb_list_tx_init(sc);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ALLPHYS);
	} else {
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ALLPHYS);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_BROAD);
	} else {
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_BROAD);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	wb_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXADDR, vtophys(&sc->wb_ldata->wb_rx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, WB_IMR, WB_INTRS);
	CSR_WRITE_4(sc, WB_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXSTART, 0xFFFFFFFF);

	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
	CSR_WRITE_4(sc, WB_TXADDR, vtophys(&sc->wb_ldata->wb_tx_list[0]));
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);

	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	callout_reset(&sc->wb_stat_timer, hz, wb_tick, sc);
}

/*
 * Set media options.
 */
static int wb_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		wb_init(sc);

	return(0);
}

/*
 * Report current media status.
 */
static void wb_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct wb_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->wb_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int wb_ioctl(ifp, command, data, cr)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
	struct ucred		*cr;
{
	struct wb_softc		*sc = ifp->if_softc;
	struct mii_data		*mii;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;

	s = splimp();

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			wb_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				wb_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		wb_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->wb_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void wb_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("wb%d: watchdog timeout\n", sc->wb_unit);
#ifdef foo
	if (!(wb_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("wb%d: no carrier - transceiver cable problem?\n",
								sc->wb_unit);
#endif
	wb_stop(sc);
	wb_reset(sc);
	wb_init(sc);

	if (!ifq_is_empty(&ifp->if_snd))
		wb_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void wb_stop(sc)
	struct wb_softc		*sc;
{
	int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	callout_stop(&sc->wb_stat_timer);

	WB_CLRBIT(sc, WB_NETCFG, (WB_NETCFG_RX_ON|WB_NETCFG_TX_ON));
	CSR_WRITE_4(sc, WB_IMR, 0x00000000);
	CSR_WRITE_4(sc, WB_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, WB_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < WB_RX_LIST_CNT; i++) {
		if (sc->wb_cdata.wb_rx_chain[i].wb_mbuf != NULL) {
			m_freem(sc->wb_cdata.wb_rx_chain[i].wb_mbuf);
			sc->wb_cdata.wb_rx_chain[i].wb_mbuf = NULL;
		}
	}
	bzero((char *)&sc->wb_ldata->wb_rx_list,
		sizeof(sc->wb_ldata->wb_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < WB_TX_LIST_CNT; i++) {
		if (sc->wb_cdata.wb_tx_chain[i].wb_mbuf != NULL) {
			m_freem(sc->wb_cdata.wb_tx_chain[i].wb_mbuf);
			sc->wb_cdata.wb_tx_chain[i].wb_mbuf = NULL;
		}
	}

	bzero((char *)&sc->wb_ldata->wb_tx_list,
		sizeof(sc->wb_ldata->wb_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void wb_shutdown(dev)
	device_t		dev;
{
	struct wb_softc		*sc;

	sc = device_get_softc(dev);
	wb_stop(sc);

	return;
}
