/*
 *   Copyright (c) 1999, 2000 Gary Jennejohn. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	i4b_ifpi_pci.c: AVM Fritz!Card PCI hardware driver
 *	--------------------------------------------------
 *
 *	$Id: i4b_ifpi_pci.c,v 1.4 2000/06/02 11:58:56 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/layer1/ifpi/i4b_ifpi_pci.c,v 1.6.2.1 2001/08/10 14:08:37 obrien Exp $
 * $DragonFly: src/sys/net/i4b/layer1/ifpi/i4b_ifpi_pci.c,v 1.8 2004/09/16 04:36:32 dillon Exp $
 *
 *      last edit-date: [Fri Jan 12 17:01:26 2001]
 *
 *---------------------------------------------------------------------------*/

#include "use_ifpi.h"
#include "use_pci.h"
#include "opt_i4b.h"

#if (NIFPI > 0) && (NPCI > 0)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/bus.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

#include <sys/socket.h>
#include <net/if.h>

#include <net/i4b/include/machine/i4b_debug.h>
#include <net/i4b/include/machine/i4b_ioctl.h>
#include <net/i4b/include/machine/i4b_trace.h>

#include "../../include/i4b_global.h"
#include "../../include/i4b_mbuf.h"

#include "../i4b_l1.h"
#include "../isic/i4b_isic.h"
#include "../isic/i4b_isac.h"
#include "../isic/i4b_hscx.h"

#include "i4b_ifpi_ext.h"

#define PCI_AVMA1_VID 0x1244
#define PCI_AVMA1_DID 0x0a00

/* prototypes */
static void avma1pp_disable(device_t);

static void avma1pp_intr(void *);
static void hscx_write_reg(int, u_int, u_int, struct l1_softc *);
static u_char hscx_read_reg(int, u_int, struct l1_softc *);
static u_int hscx_read_reg_int(int, u_int, struct l1_softc *);
static void hscx_read_fifo(int, void *, size_t, struct l1_softc *);
static void hscx_write_fifo(int, void *, size_t, struct l1_softc *);
static void avma1pp_hscx_int_handler(struct l1_softc *);
static void avma1pp_hscx_intr(int, u_int, struct l1_softc *);
static void avma1pp_init_linktab(struct l1_softc *);
static void avma1pp_bchannel_setup(int, int, int, int);
static void avma1pp_bchannel_start(int, int);
static void avma1pp_hscx_init(struct l1_softc *, int, int);
static void avma1pp_bchannel_stat(int, int, bchan_statistics_t *);
static void avma1pp_set_linktab(int, int, drvr_link_t *);
static isdn_link_t * avma1pp_ret_linktab(int, int);
static int avma1pp_pci_probe(device_t);
static int avma1pp_hscx_fifo(l1_bchan_state_t *, struct l1_softc *);
int avma1pp_attach_avma1pp(device_t);
static void ifpi_isac_intr(struct l1_softc *sc);

static device_method_t avma1pp_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		avma1pp_pci_probe),
	DEVMETHOD(device_attach,	avma1pp_attach_avma1pp),
	DEVMETHOD(device_shutdown,	avma1pp_disable),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};

#if 0 /* use what's in l1_softc */
/* a minimal softc for the Fritz!Card PCI */
struct avma1pp_softc 
{
	bus_space_handle_t	avma1pp_bhandle;
	bus_space_tag_t		avma1pp_btag;
	void			*avma1pp_intrhand;
	struct resource		*avma1pp_irq;
	struct resource		*avma1pp_res;
	/* pointer to ifpi_sc */
	struct l1_softc	*avma1pp_isc;
};
#endif

static driver_t avma1pp_pci_driver = {
	"ifpi",
	avma1pp_pci_methods,
	sizeof(struct l1_softc)
};

static devclass_t avma1pp_pci_devclass;

DRIVER_MODULE(avma1pp, pci, avma1pp_pci_driver, avma1pp_pci_devclass, 0, 0);

/* jump table for multiplex routines */

struct i4b_l1mux_func avma1pp_l1mux_func = {
	avma1pp_ret_linktab,
	avma1pp_set_linktab,
	ifpi_mph_command_req,
	ifpi_ph_data_req,
	ifpi_ph_activate_req,
};

struct l1_softc *ifpi_scp[IFPI_MAXUNIT];

/*---------------------------------------------------------------------------*
 *	AVM PCI Fritz!Card special registers
 *---------------------------------------------------------------------------*/

/*
 *	register offsets from i/o base
 */
#define STAT0_OFFSET            0x02
#define STAT1_OFFSET            0x03
#define ADDR_REG_OFFSET         0x04
/*#define MODREG_OFFSET		0x06
#define VERREG_OFFSET           0x07*/

/* these 2 are used to select an ISAC register set */
#define ISAC_LO_REG_OFFSET	0x04
#define ISAC_HI_REG_OFFSET	0x06

/* offset higher than this goes to the HI register set */
#define MAX_LO_REG_OFFSET	0x2f

/* mask for the offset */
#define ISAC_REGSET_MASK	0x0f

/* the offset from the base to the ISAC registers */
#define ISAC_REG_OFFSET		0x10

/* the offset from the base to the ISAC FIFO */
#define ISAC_FIFO		0x02

/* not really the HSCX, but sort of */
#define HSCX_FIFO		0x00
#define HSCX_STAT		0x04

/*
 *	AVM PCI Status Latch 0 read only bits
 */
#define ASL_IRQ_ISAC            0x01    /* ISAC  interrupt, active low */
#define ASL_IRQ_HSCX            0x02    /* HSX   interrupt, active low */
#define ASL_IRQ_TIMER           0x04    /* Timer interrupt, active low */
#define ASL_IRQ_BCHAN           ASL_IRQ_HSCX
/* actually active LOW */
#define ASL_IRQ_Pending         (ASL_IRQ_ISAC | ASL_IRQ_HSCX | ASL_IRQ_TIMER)

/*
 *	AVM Status Latch 0 write only bits
 */
#define ASL_RESET_ALL           0x01  /* reset siemens IC's, active 1 */
#define ASL_TIMERDISABLE        0x02  /* active high */
#define ASL_TIMERRESET          0x04  /* active high */
#define ASL_ENABLE_INT          0x08  /* active high */
#define ASL_TESTBIT	        0x10  /* active high */

/*
 *	AVM Status Latch 1 write only bits
 */
#define ASL1_INTSEL              0x0f  /* active high */
#define ASL1_ENABLE_IOM          0x80  /* active high */

/*
 * "HSCX" mode bits
 */
#define  HSCX_MODE_ITF_FLG	0x01
#define  HSCX_MODE_TRANS	0x02
#define  HSCX_MODE_CCR_7	0x04
#define  HSCX_MODE_CCR_16	0x08
#define  HSCX_MODE_TESTLOOP	0x80

/*
 * "HSCX" status bits
 */
#define  HSCX_STAT_RME		0x01
#define  HSCX_STAT_RDO		0x10
#define  HSCX_STAT_CRCVFRRAB	0x0E
#define  HSCX_STAT_CRCVFR	0x06
#define  HSCX_STAT_RML_MASK	0x3f00

/*
 * "HSCX" interrupt bits
 */
#define  HSCX_INT_XPR		0x80
#define  HSCX_INT_XDU		0x40
#define  HSCX_INT_RPR		0x20
#define  HSCX_INT_MASK		0xE0

/*
 * "HSCX" command bits
 */
#define  HSCX_CMD_XRS		0x80
#define  HSCX_CMD_XME		0x01
#define  HSCX_CMD_RRS		0x20
#define  HSCX_CMD_XML_MASK	0x3f00

/*
 * Commands and parameters are sent to the "HSCX" as a long, but the
 * fields are handled as bytes.
 *
 * The long contains:
 *	(prot << 16)|(txl << 8)|cmd
 *
 * where:
 *	prot = protocol to use
 *	txl = transmit length
 *	cmd = the command to be executed
 *
 * The fields are defined as u_char in struct l1_softc.
 *
 * Macro to coalesce the byte fields into a u_int
 */
#define AVMA1PPSETCMDLONG(f) (f) = ((sc->avma1pp_cmd) | (sc->avma1pp_txl << 8) \
 					| (sc->avma1pp_prot << 16))

/*
 * to prevent deactivating the "HSCX" when both channels are active we
 * define an HSCX_ACTIVE flag which is or'd into the channel's state
 * flag in avma1pp_bchannel_setup upon active and cleared upon deactivation.
 * It is set high to allow room for new flags.
 */
#define HSCX_AVMA1PP_ACTIVE	0x1000 

/*---------------------------------------------------------------------------*
 *	AVM read fifo routines
 *---------------------------------------------------------------------------*/

static void
avma1pp_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(btag, bhandle,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_read_multi_1(btag, bhandle,  ISAC_REG_OFFSET, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_read_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_read_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_read_fifo(int chan, void *buf, size_t len, struct l1_softc *sc)
{
	u_int32_t *ip;
	size_t cnt;
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	bus_space_write_4(btag, bhandle, ADDR_REG_OFFSET, chan);
	ip = (u_int32_t *)buf;
	cnt = 0;
	/* what if len isn't a multiple of sizeof(int) and buf is */
	/* too small ???? */
	while (cnt < len)
	{
		*ip++ = bus_space_read_4(btag, bhandle, ISAC_REG_OFFSET);
		cnt += 4;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write fifo routines
 *---------------------------------------------------------------------------*/
static void
avma1pp_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(btag, bhandle,  ADDR_REG_OFFSET, ISAC_FIFO);
			bus_space_write_multi_1(btag, bhandle,  ISAC_REG_OFFSET, (u_int8_t*)buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_fifo(0, buf, size, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_fifo(1, buf, size, sc);
			break;
	}
}

static void
hscx_write_fifo(int chan, void *buf, size_t len, struct l1_softc *sc)
{
	u_int32_t *ip;
	size_t cnt;
	l1_bchan_state_t *Bchan = &sc->sc_chan[chan];
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 


	sc->avma1pp_cmd &= ~HSCX_CMD_XME;
	sc->avma1pp_txl = 0;
	if (Bchan->out_mbuf_cur == NULL)
	{
	  if (Bchan->bprot != BPROT_NONE)
		 sc->avma1pp_cmd |= HSCX_CMD_XME;
	}
	if (len != sc->sc_bfifolen)
		sc->avma1pp_txl = len;
	
	cnt = 0; /* borrow cnt */
	AVMA1PPSETCMDLONG(cnt);
	hscx_write_reg(chan, HSCX_STAT, cnt, sc);

	ip = (u_int32_t *)buf;
	cnt = 0;
	while (cnt < len)
	{
		bus_space_write_4(btag, bhandle, ISAC_REG_OFFSET, *ip);
		ip++;
		cnt += 4;
	}
}

/*---------------------------------------------------------------------------*
 *	AVM write register routines
 *---------------------------------------------------------------------------*/

static void
avma1pp_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	u_char reg_bank;
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
			printf("write_reg bank %d  off %ld.. ", (int)reg_bank, (long)offs);
#endif
			/* set the register bank */
			bus_space_write_1(btag, bhandle, ADDR_REG_OFFSET, reg_bank);
			bus_space_write_1(btag, bhandle, ISAC_REG_OFFSET + (offs & ISAC_REGSET_MASK), data);
			break;
		case ISIC_WHAT_HSCXA:
			hscx_write_reg(0, offs, data, sc);
			break;
		case ISIC_WHAT_HSCXB:
			hscx_write_reg(1, offs, data, sc);
			break;
	}
}

static void
hscx_write_reg(int chan, u_int off, u_int val, struct l1_softc *sc)
{
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	/* point at the correct channel */
	bus_space_write_4(btag, bhandle, ADDR_REG_OFFSET, chan);
	bus_space_write_4(btag, bhandle, ISAC_REG_OFFSET + off, val);
}

/*---------------------------------------------------------------------------*
 *	AVM read register routines
 *---------------------------------------------------------------------------*/
static u_int8_t
avma1pp_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	u_char reg_bank;
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	switch (what) {
		case ISIC_WHAT_ISAC:
			reg_bank = (offs > MAX_LO_REG_OFFSET) ? ISAC_HI_REG_OFFSET:ISAC_LO_REG_OFFSET;
#ifdef AVMA1PCI_DEBUG
			printf("read_reg bank %d  off %ld.. ", (int)reg_bank, (long)offs);
#endif
			/* set the register bank */
			bus_space_write_1(btag, bhandle, ADDR_REG_OFFSET, reg_bank);
			return(bus_space_read_1(btag, bhandle, ISAC_REG_OFFSET +
				(offs & ISAC_REGSET_MASK)));
		case ISIC_WHAT_HSCXA:
			return hscx_read_reg(0, offs, sc);
		case ISIC_WHAT_HSCXB:
			return hscx_read_reg(1, offs, sc);
	}
	return 0;
}

static u_char
hscx_read_reg(int chan, u_int off, struct l1_softc *sc)
{
	return(hscx_read_reg_int(chan, off, sc) & 0xff);
}

/*
 * need to be able to return an int because the RBCH is in the 2nd
 * byte.
 */
static u_int
hscx_read_reg_int(int chan, u_int off, struct l1_softc *sc)
{
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	/* point at the correct channel */
	bus_space_write_4(btag, bhandle, ADDR_REG_OFFSET, chan);
	return(bus_space_read_4(btag, bhandle, ISAC_REG_OFFSET + off));
}

/*---------------------------------------------------------------------------*
 *	avma1pp_probe - probe for a card
 *---------------------------------------------------------------------------*/
static int
avma1pp_pci_probe(dev)
	device_t		dev;
{
	u_int16_t		did, vid;

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);

	if ((vid == PCI_AVMA1_VID) && (did == PCI_AVMA1_DID)) {
		device_set_desc(dev, "AVM Fritz!Card PCI");
		return(0);
	}

	return(ENXIO);
}

/*---------------------------------------------------------------------------*
 *	avma1pp_attach_avma1pp - attach Fritz!Card PCI
 *---------------------------------------------------------------------------*/
int
avma1pp_attach_avma1pp(device_t dev)
{
	struct l1_softc *sc;
	u_int v;
	int unit, error = 0;
	int s;
	u_int16_t did, vid;
	void *ih = 0;
	bus_space_handle_t bhandle;
	bus_space_tag_t btag; 
#if defined (__FreeBSD__) && __FreeBSD__ > 4
	l1_bchan_state_t *chan;
#endif

	s = splimp();

	vid = pci_get_vendor(dev);
	did = pci_get_device(dev);
	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct l1_softc));

	/* probably not really required */
	if(unit > IFPI_MAXUNIT) {
		printf("avma1pp%d: Error, unit > IFPI_MAXUNIT!\n", unit);
		splx(s);
		return(ENXIO);
	}

	if ((vid != PCI_AVMA1_VID) && (did != PCI_AVMA1_DID)) {
		printf("avma1pp%d: unknown device!?\n", unit);
		goto fail;
	}

	ifpi_scp[unit] = sc;

	sc->sc_resources.io_rid[0] = PCIR_MAPS+4;
	sc->sc_resources.io_base[0] = bus_alloc_resource(dev, SYS_RES_IOPORT,
		&sc->sc_resources.io_rid[0],
		0, ~0, 1, RF_ACTIVE);

	if (sc->sc_resources.io_base[0] == NULL) {
		printf("avma1pp%d: couldn't map IO port\n", unit);
		error = ENXIO;
		goto fail;
	}

	bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	/* Allocate interrupt */
	sc->sc_resources.irq_rid = 0;
	sc->sc_resources.irq = bus_alloc_resource(dev, SYS_RES_IRQ,
		&sc->sc_resources.irq_rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->sc_resources.irq == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_MAPS+4, sc->sc_resources.io_base[0]);
		printf("avma1pp%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET, avma1pp_intr, sc, &ih);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_resources.irq);
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_MAPS+4, sc->sc_resources.io_base[0]);
		printf("avma1pp%d: couldn't set up irq\n", unit);
		goto fail;
	}

	sc->sc_unit = unit;

	/* end of new-bus stuff */

	ISAC_BASE = (caddr_t)ISIC_WHAT_ISAC;

	HSCX_A_BASE = (caddr_t)ISIC_WHAT_HSCXA;
	HSCX_B_BASE = (caddr_t)ISIC_WHAT_HSCXB;

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = avma1pp_read_reg;
	sc->writereg = avma1pp_write_reg;

	sc->readfifo = avma1pp_read_fifo;
	sc->writefifo = avma1pp_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_AVMA1PCI;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* set up some other miscellaneous things */
	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* reset the card */
	/* the Linux driver does this to clear any pending ISAC interrupts */
	v = 0;
	v = ISAC_READ(I_STAR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_STAR %x...", v);
#endif
	v = ISAC_READ(I_MODE);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_MODE %x...", v);
#endif
	v = ISAC_READ(I_ADF2);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ADF2 %x...", v);
#endif
	v = ISAC_READ(I_ISTA);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_ISTA %x...", v);
#endif
	if (v & ISAC_ISTA_EXI)
	{
		 v = ISAC_READ(I_EXIR);
#ifdef AVMA1PCI_DEBUG
		 printf("avma1pp_attach: I_EXIR %x...", v);
#endif
	}
	v = ISAC_READ(I_CIRR);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: I_CIRR %x...", v);
#endif
	ISAC_WRITE(I_MASK, 0xff);
	/* the Linux driver does this to clear any pending HSCX interrupts */
	v = hscx_read_reg_int(0, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 0 HSCX_STAT %x...", v);
#endif
	v = hscx_read_reg_int(1, HSCX_STAT, sc);
#ifdef AVMA1PCI_DEBUG
	printf("avma1pp_attach: 1 HSCX_STAT %x\n", v);
#endif

	bus_space_write_1(btag, bhandle, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
	bus_space_write_1(btag, bhandle, STAT0_OFFSET, ASL_TIMERRESET|ASL_ENABLE_INT|ASL_TIMERDISABLE);
	DELAY(SEC_DELAY/100); /* 10 ms */
#ifdef AVMA1PCI_DEBUG
	bus_space_write_1(btag, bhandle, STAT1_OFFSET, ASL1_ENABLE_IOM|sc->sc_irq);
	DELAY(SEC_DELAY/100); /* 10 ms */
	v = bus_space_read_1(btag, bhandle, STAT1_OFFSET);
	printf("after reset: S1 %#x\n", v);

	v = bus_space_read_4(btag, bhandle, 0);
	printf("avma1pp_attach_avma1pp: v %#x\n", v);
#endif

   /* from here to the end would normally be done in isic_pciattach */

	 printf("ifpi%d: ISAC %s (IOM-%c)\n", unit,
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		 sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');

	/* init the ISAC */
	ifpi_isac_init(sc);

#if defined (__FreeBSD__) && __FreeBSD__ > 4
	/* Init the channel mutexes */
	chan = &sc->sc_chan[HSCX_CH_A];
	mtx_init(&chan->rx_queue.ifq_mtx, "i4b_avma1pp_rx", MTX_DEF);
	mtx_init(&chan->tx_queue.ifq_mtx, "i4b_avma1pp_tx", MTX_DEF);
	chan = &sc->sc_chan[HSCX_CH_B];
	mtx_init(&chan->rx_queue.ifq_mtx, "i4b_avma1pp_rx", MTX_DEF);
	mtx_init(&chan->tx_queue.ifq_mtx, "i4b_avma1pp_tx", MTX_DEF);
#endif

	/* init the "HSCX" */
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	avma1pp_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	avma1pp_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

	callout_init(&sc->sc_T3_timeout);
	callout_init(&sc->sc_T4_timeout);	
	
	/* init higher protocol layers */
	
	i4b_l1_mph_status_ind(L0IFPIUNIT(sc->sc_unit), STI_ATTACH, sc->sc_cardtyp, &avma1pp_l1mux_func);

  fail:
	splx(s);
	return(error);
}

/*
 * this is the real interrupt routine
 */
static void
avma1pp_hscx_intr(int h_chan, u_int stat, struct l1_softc *sc)
{
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int activity = -1;
	u_int param = 0;
	
	NDBGL1(L1_H_IRQ, "%#x", stat);

	if((stat & HSCX_INT_XDU) && (chan->bprot != BPROT_NONE))/* xmit data underrun */
	{
		chan->stat_XDU++;			
		NDBGL1(L1_H_XFRERR, "xmit data underrun");
		/* abort the transmission */
		sc->avma1pp_txl = 0;
		sc->avma1pp_cmd |= HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd &= ~HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);

		if (chan->out_mbuf_head != NULL)  /* don't continue to transmit this buffer */
		{
			i4b_Bfreembuf(chan->out_mbuf_head);
			chan->out_mbuf_cur = chan->out_mbuf_head = NULL;
		}
	}

	/*
	 * The following is based on examination of the Linux driver.
	 *
	 * The logic here is different than with a "real" HSCX; all kinds
	 * of information (interrupt/status bits) are in stat.
	 *		HSCX_INT_RPR indicates a receive interrupt
	 *			HSCX_STAT_RDO indicates an overrun condition, abort -
	 *			otherwise read the bytes ((stat & HSCX_STZT_RML_MASK) >> 8)
	 *			HSCX_STAT_RME indicates end-of-frame and apparently any
	 *			CRC/framing errors are only reported in this state.
	 *				if ((stat & HSCX_STAT_CRCVFRRAB) != HSCX_STAT_CRCVFR)
	 *					CRC/framing error
	 */
	
	if(stat & HSCX_INT_RPR)
	{
		int fifo_data_len;
		int error = 0;
		/* always have to read the FIFO, so use a scratch buffer */
		u_char scrbuf[HSCX_FIFO_LEN];

		if(stat & HSCX_STAT_RDO)
		{
			chan->stat_RDO++;
			NDBGL1(L1_H_XFRERR, "receive data overflow");
			error++;				
		}

		/*
		 * check whether we're receiving data for an inactive B-channel
		 * and discard it. This appears to happen for telephony when
		 * both B-channels are active and one is deactivated. Since
		 * it is not really possible to deactivate the channel in that
		 * case (the ASIC seems to deactivate _both_ channels), the
		 * "deactivated" channel keeps receiving data which can lead
		 * to exhaustion of mbufs and a kernel panic.
		 *
		 * This is a hack, but it's the only solution I can think of
		 * without having the documentation for the ASIC.
		 * GJ - 28 Nov 1999
		 */
		 if (chan->state == HSCX_IDLE)
		 {
			NDBGL1(L1_H_XFRERR, "toss data from %d", h_chan);
			error++;
		 }

		fifo_data_len = ((stat & HSCX_STAT_RML_MASK) >> 8);
		
		if(fifo_data_len == 0)
			fifo_data_len = sc->sc_bfifolen;

		/* ALWAYS read data from HSCX fifo */
	
		HSCX_RDFIFO(h_chan, scrbuf, fifo_data_len);
		chan->rxcount += fifo_data_len;

		/* all error conditions checked, now decide and take action */
		
		if(error == 0)
		{
			if(chan->in_mbuf == NULL)
			{
				if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
					panic("L1 avma1pp_hscx_intr: RME, cannot allocate mbuf!\n");
				chan->in_cbptr = chan->in_mbuf->m_data;
				chan->in_len = 0;
			}

			if((chan->in_len + fifo_data_len) <= BCH_MAX_DATALEN)
			{
			   	/* OK to copy the data */
				bcopy(scrbuf, chan->in_cbptr, fifo_data_len);
				chan->in_cbptr += fifo_data_len;
				chan->in_len += fifo_data_len;

				/* setup mbuf data length */
					
				chan->in_mbuf->m_len = chan->in_len;
				chan->in_mbuf->m_pkthdr.len = chan->in_len;

				if(sc->sc_trace & TRACE_B_RX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0IFPIUNIT(sc->sc_unit);
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_NT;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
				}

				if (stat & HSCX_STAT_RME)
				{
				  if((stat & HSCX_STAT_CRCVFRRAB) == HSCX_STAT_CRCVFR)
				  {
					 (*chan->isic_drvr_linktab->bch_rx_data_ready)(chan->isic_drvr_linktab->unit);
					 activity = ACT_RX;
				
					 /* mark buffer ptr as unused */
					
					 chan->in_mbuf = NULL;
					 chan->in_cbptr = NULL;
					 chan->in_len = 0;
				  }
				  else
				  {
						chan->stat_CRC++;
						NDBGL1(L1_H_XFRERR, "CRC/RAB");
					  if (chan->in_mbuf != NULL)
					  {
						  i4b_Bfreembuf(chan->in_mbuf);
						  chan->in_mbuf = NULL;
						  chan->in_cbptr = NULL;
						  chan->in_len = 0;
					  }
				  }
				}
			} /* END enough space in mbuf */
			else
			{
				 if(chan->bprot == BPROT_NONE)
				 {
					  /* setup mbuf data length */
				
					  chan->in_mbuf->m_len = chan->in_len;
					  chan->in_mbuf->m_pkthdr.len = chan->in_len;

					  if(sc->sc_trace & TRACE_B_RX)
					  {
							i4b_trace_hdr_t hdr;
							hdr.unit = L0IFPIUNIT(sc->sc_unit);
							hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
							hdr.dir = FROM_NT;
							hdr.count = ++sc->sc_trace_bcount;
							MICROTIME(hdr.time);
							i4b_l1_trace_ind(&hdr, chan->in_mbuf->m_len, chan->in_mbuf->m_data);
						}

					  if(!(i4b_l1_bchan_tel_silence(chan->in_mbuf->m_data, chan->in_mbuf->m_len)))
						 activity = ACT_RX;
				
					  /* move rx'd data to rx queue */

#if defined (__FreeBSD__) && __FreeBSD__ > 4
					  (void) IF_HANDOFF(&chan->rx_queue, chan->in_mbuf, NULL);
#else
					  if(!(IF_QFULL(&chan->rx_queue)))
					  {
						IF_ENQUEUE(&chan->rx_queue, chan->in_mbuf);
					  }
					  else
					  {
						i4b_Bfreembuf(chan->in_mbuf);
					  }
#endif					
					  /* signal upper layer that data are available */
					  (*chan->isic_drvr_linktab->bch_rx_data_ready)(chan->isic_drvr_linktab->unit);

					  /* alloc new buffer */
				
					  if((chan->in_mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
						 panic("L1 avma1pp_hscx_intr: RPF, cannot allocate new mbuf!\n");
	
					  /* setup new data ptr */
				
					  chan->in_cbptr = chan->in_mbuf->m_data;
	
					  /* OK to copy the data */
					  bcopy(scrbuf, chan->in_cbptr, fifo_data_len);

					  chan->in_cbptr += fifo_data_len;
					  chan->in_len = fifo_data_len;

					  chan->rxcount += fifo_data_len;
					}
				 else
					{
					  NDBGL1(L1_H_XFRERR, "RAWHDLC rx buffer overflow in RPF, in_len=%d", chan->in_len);
					  chan->in_cbptr = chan->in_mbuf->m_data;
					  chan->in_len = 0;
					}
			  }
		} /* if(error == 0) */
		else
		{
		  	/* land here for RDO */
			if (chan->in_mbuf != NULL)
			{
				i4b_Bfreembuf(chan->in_mbuf);
				chan->in_mbuf = NULL;
				chan->in_cbptr = NULL;
				chan->in_len = 0;
			}
			sc->avma1pp_txl = 0;
			sc->avma1pp_cmd |= HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
			sc->avma1pp_cmd &= ~HSCX_CMD_RRS;
			AVMA1PPSETCMDLONG(param);
			hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		}
	}


	/* transmit fifo empty, new data can be written to fifo */
	
	if(stat & HSCX_INT_XPR)
	{
		/*
		 * for a description what is going on here, please have
		 * a look at isic_bchannel_start() in i4b_bchan.c !
		 */

		NDBGL1(L1_H_IRQ, "unit %d, chan %d - XPR, Tx Fifo Empty!", sc->sc_unit, h_chan);

		if(chan->out_mbuf_cur == NULL) 	/* last frame is transmitted */
		{
			IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);

			if(chan->out_mbuf_head == NULL)
			{
				chan->state &= ~HSCX_TX_ACTIVE;
				(*chan->isic_drvr_linktab->bch_tx_queue_empty)(chan->isic_drvr_linktab->unit);
			}
			else
			{
				chan->state |= HSCX_TX_ACTIVE;
				chan->out_mbuf_cur = chan->out_mbuf_head;
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;

				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0IFPIUNIT(sc->sc_unit);
					hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
				
				if(chan->bprot == BPROT_NONE)
				{
					if(!(i4b_l1_bchan_tel_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
						activity = ACT_TX;
				}
				else
				{
					activity = ACT_TX;
				}
			}
		}
			
		avma1pp_hscx_fifo(chan, sc);
	}

	/* call timeout handling routine */
	
	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->isic_drvr_linktab->bch_activity)(chan->isic_drvr_linktab->unit, activity);
}

/*
 * this is the main routine which checks each channel and then calls
 * the real interrupt routine as appropriate
 */
static void
avma1pp_hscx_int_handler(struct l1_softc *sc)
{
	u_int stat;

	/* has to be a u_int because the byte count is in the 2nd byte */
	stat = hscx_read_reg_int(0, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(0, stat, sc);
	stat = hscx_read_reg_int(1, HSCX_STAT, sc);
	if (stat & HSCX_INT_MASK)
	  avma1pp_hscx_intr(1, stat, sc);
}

static void
avma1pp_disable(device_t dev)
{
	struct l1_softc *sc = device_get_softc(dev);
	bus_space_handle_t bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	bus_space_tag_t btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	bus_space_write_1(btag, bhandle, STAT0_OFFSET, ASL_RESET_ALL|ASL_TIMERDISABLE);
}

static void
avma1pp_intr(void *xsc)
{
	u_char stat;
	struct l1_softc *sc;
	bus_space_handle_t bhandle;
	bus_space_tag_t btag; 

	sc = xsc;
	bhandle = rman_get_bushandle(sc->sc_resources.io_base[0]);
	btag = rman_get_bustag(sc->sc_resources.io_base[0]); 

	stat = bus_space_read_1(btag, bhandle, STAT0_OFFSET);
	NDBGL1(L1_H_IRQ, "stat %x", stat);
	/* was there an interrupt from this card ? */
	if ((stat & ASL_IRQ_Pending) == ASL_IRQ_Pending)
		return; /* no */
	/* interrupts are low active */
	if (!(stat & ASL_IRQ_TIMER))
	  NDBGL1(L1_H_IRQ, "timer interrupt ???");
	if (!(stat & ASL_IRQ_HSCX))
	{
	  NDBGL1(L1_H_IRQ, "HSCX");
		avma1pp_hscx_int_handler(sc);
	}
	if (!(stat & ASL_IRQ_ISAC))
	{
	  NDBGL1(L1_H_IRQ, "ISAC");
		ifpi_isac_intr(sc);
	}
}

static void
avma1pp_hscx_init(struct l1_softc *sc, int h_chan, int activate)
{
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	u_int param = 0;

	NDBGL1(L1_BCHAN, "unit=%d, channel=%d, %s",
		sc->sc_unit, h_chan, activate ? "activate" : "deactivate");

	if (activate == 0)
	{
		/* only deactivate if both channels are idle */
		if (sc->sc_chan[HSCX_CH_A].state != HSCX_IDLE ||
			sc->sc_chan[HSCX_CH_B].state != HSCX_IDLE)
		{
			return;
		}
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		return;
	}
	if(chan->bprot == BPROT_RHDLC)
	{
		  NDBGL1(L1_BCHAN, "BPROT_RHDLC");

		/* HDLC Frames, transparent mode 0 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_ITF_FLG;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
	else
	{
		  NDBGL1(L1_BCHAN, "BPROT_NONE??");

		/* Raw Telephony, extended transparent mode 1 */
		sc->avma1pp_cmd = HSCX_CMD_XRS|HSCX_CMD_RRS;
		sc->avma1pp_prot = HSCX_MODE_TRANS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = HSCX_CMD_XRS;
		AVMA1PPSETCMDLONG(param);
		hscx_write_reg(h_chan, HSCX_STAT, param, sc);
		sc->avma1pp_cmd = 0;
	}
}

static void
avma1pp_bchannel_setup(int unit, int h_chan, int bprot, int activate)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
	struct l1_softc *sc = ifpi_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];

	int s = SPLI4B();
	
	if(activate == 0)
	{
		/* deactivation */
		chan->state = HSCX_IDLE;
		avma1pp_hscx_init(sc, h_chan, activate);
	}
		
	NDBGL1(L1_BCHAN, "unit=%d, channel=%d, %s",
		sc->sc_unit, h_chan, activate ? "activate" : "deactivate");

	/* general part */

	chan->unit = sc->sc_unit;	/* unit number */
	chan->channel = h_chan;		/* B channel */
	chan->bprot = bprot;		/* B channel protocol */
	chan->state = HSCX_IDLE;	/* B channel state */

	/* receiver part */

	chan->rx_queue.ifq_maxlen = IFQ_MAXLEN;

	i4b_Bcleanifq(&chan->rx_queue);	/* clean rx queue */

	chan->rxcount = 0;		/* reset rx counter */
	
	i4b_Bfreembuf(chan->in_mbuf);	/* clean rx mbuf */

	chan->in_mbuf = NULL;		/* reset mbuf ptr */
	chan->in_cbptr = NULL;		/* reset mbuf curr ptr */
	chan->in_len = 0;		/* reset mbuf data len */
	
	/* transmitter part */

	chan->tx_queue.ifq_maxlen = IFQ_MAXLEN;
	
	i4b_Bcleanifq(&chan->tx_queue);	/* clean tx queue */

	chan->txcount = 0;		/* reset tx counter */
	
	i4b_Bfreembuf(chan->out_mbuf_head);	/* clean tx mbuf */

	chan->out_mbuf_head = NULL;	/* reset head mbuf ptr */
	chan->out_mbuf_cur = NULL;	/* reset current mbuf ptr */	
	chan->out_mbuf_cur_ptr = NULL;	/* reset current mbuf data ptr */
	chan->out_mbuf_cur_len = 0;	/* reset current mbuf data cnt */
	
	if(activate != 0)
	{
		/* activation */
		avma1pp_hscx_init(sc, h_chan, activate);
		chan->state |= HSCX_AVMA1PP_ACTIVE;
	}

	splx(s);
}

static void
avma1pp_bchannel_start(int unit, int h_chan)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
	struct l1_softc *sc = ifpi_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int s;
	int activity = -1;

	s = SPLI4B();				/* enter critical section */
	if(chan->state & HSCX_TX_ACTIVE)	/* already running ? */
	{
		splx(s);
		return;				/* yes, leave */
	}

	/* get next mbuf from queue */
	
	IF_DEQUEUE(&chan->tx_queue, chan->out_mbuf_head);
	
	if(chan->out_mbuf_head == NULL)		/* queue empty ? */
	{
		splx(s);			/* leave critical section */
		return;				/* yes, exit */
	}

	/* init current mbuf values */
	
	chan->out_mbuf_cur = chan->out_mbuf_head;
	chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;	
	
	/* activity indicator for timeout handling */

	if(chan->bprot == BPROT_NONE)
	{
		if(!(i4b_l1_bchan_tel_silence(chan->out_mbuf_cur->m_data, chan->out_mbuf_cur->m_len)))
			activity = ACT_TX;
	}
	else
	{
		activity = ACT_TX;
	}

	chan->state |= HSCX_TX_ACTIVE;		/* we start transmitting */
	
	if(sc->sc_trace & TRACE_B_TX)	/* if trace, send mbuf to trace dev */
	{
		i4b_trace_hdr_t hdr;
		hdr.unit = L0IFPIUNIT(sc->sc_unit);
		hdr.type = (h_chan == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_bcount;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
	}			

	avma1pp_hscx_fifo(chan, sc);

	/* call timeout handling routine */
	
	if(activity == ACT_RX || activity == ACT_TX)
		(*chan->isic_drvr_linktab->bch_activity)(chan->isic_drvr_linktab->unit, activity);

	splx(s);	
}

/*---------------------------------------------------------------------------*
 *	return the address of isic drivers linktab	
 *---------------------------------------------------------------------------*/
static isdn_link_t *
avma1pp_ret_linktab(int unit, int channel)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
	struct l1_softc *sc = ifpi_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[channel];

	return(&chan->isic_isdn_linktab);
}
 
/*---------------------------------------------------------------------------*
 *	set the driver linktab in the b channel softc
 *---------------------------------------------------------------------------*/
static void
avma1pp_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
	struct l1_softc *sc = ifpi_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[channel];

	chan->isic_drvr_linktab = dlt;
}


/*---------------------------------------------------------------------------*
 *	initialize our local linktab
 *---------------------------------------------------------------------------*/
static void
avma1pp_init_linktab(struct l1_softc *sc)
{
	l1_bchan_state_t *chan = &sc->sc_chan[HSCX_CH_A];
	isdn_link_t *lt = &chan->isic_isdn_linktab;

	/* make sure the hardware driver is known to layer 4 */
	/* avoid overwriting if already set */
	if (ctrl_types[CTRL_PASSIVE].set_linktab == NULL)
	{
		ctrl_types[CTRL_PASSIVE].set_linktab = avma1pp_set_linktab;
		ctrl_types[CTRL_PASSIVE].get_linktab = avma1pp_ret_linktab;
	}

	/* local setup */
	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_A;
	lt->bch_config = avma1pp_bchannel_setup;
	lt->bch_tx_start = avma1pp_bchannel_start;
	lt->bch_stat = avma1pp_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
                                                
	chan = &sc->sc_chan[HSCX_CH_B];
	lt = &chan->isic_isdn_linktab;

	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_B;
	lt->bch_config = avma1pp_bchannel_setup;
	lt->bch_tx_start = avma1pp_bchannel_start;
	lt->bch_stat = avma1pp_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
}

/*
 * use this instead of isic_bchannel_stat in i4b_bchan.c because it's static
 */
static void
avma1pp_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp)
{
#if defined(__DragonFly__) || defined(__FreeBSD__)
	struct l1_softc *sc = ifpi_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int s;

	s = SPLI4B();
	
	bsp->outbytes = chan->txcount;
	bsp->inbytes = chan->rxcount;

	chan->txcount = 0;
	chan->rxcount = 0;

	splx(s);
}

/*---------------------------------------------------------------------------*
 *	fill HSCX fifo with data from the current mbuf
 *	Put this here until it can go into i4b_hscx.c
 *---------------------------------------------------------------------------*/
static int
avma1pp_hscx_fifo(l1_bchan_state_t *chan, struct l1_softc *sc)
{
	int len;
	int nextlen;
	int i;
	int cmd = 0;
	/* using a scratch buffer simplifies writing to the FIFO */
	u_char scrbuf[HSCX_FIFO_LEN];

	len = 0;

	/*
	 * fill the HSCX tx fifo with data from the current mbuf. if
	 * current mbuf holds less data than HSCX fifo length, try to
	 * get the next mbuf from (a possible) mbuf chain. if there is
	 * not enough data in a single mbuf or in a chain, then this
	 * is the last mbuf and we tell the HSCX that it has to send
	 * CRC and closing flag
	 */
	 
	while(chan->out_mbuf_cur && len != sc->sc_bfifolen)
	{
		nextlen = min(chan->out_mbuf_cur_len, sc->sc_bfifolen - len);

#ifdef NOTDEF
		printf("i:mh=%p, mc=%p, mcp=%p, mcl=%d l=%d nl=%d # ",
			chan->out_mbuf_head,
			chan->out_mbuf_cur,			
			chan->out_mbuf_cur_ptr,
			chan->out_mbuf_cur_len,
			len,
			nextlen);
#endif

		cmd |= HSCX_CMDR_XTF;
		/* collect the data in the scratch buffer */
		for (i = 0; i < nextlen; i++)
			scrbuf[i + len] = chan->out_mbuf_cur_ptr[i];

		len += nextlen;
		chan->txcount += nextlen;
	
		chan->out_mbuf_cur_ptr += nextlen;
		chan->out_mbuf_cur_len -= nextlen;
			
		if(chan->out_mbuf_cur_len == 0) 
		{
			if((chan->out_mbuf_cur = chan->out_mbuf_cur->m_next) != NULL)
			{
				chan->out_mbuf_cur_ptr = chan->out_mbuf_cur->m_data;
				chan->out_mbuf_cur_len = chan->out_mbuf_cur->m_len;
	
				if(sc->sc_trace & TRACE_B_TX)
				{
					i4b_trace_hdr_t hdr;
					hdr.unit = L0IFPIUNIT(sc->sc_unit);
					hdr.type = (chan->channel == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
					hdr.dir = FROM_TE;
					hdr.count = ++sc->sc_trace_bcount;
					MICROTIME(hdr.time);
					i4b_l1_trace_ind(&hdr, chan->out_mbuf_cur->m_len, chan->out_mbuf_cur->m_data);
				}
			}
			else
			{
				if (chan->bprot != BPROT_NONE)
					cmd |= HSCX_CMDR_XME;
				i4b_Bfreembuf(chan->out_mbuf_head);
				chan->out_mbuf_head = NULL;
			}
		}
	}
	/* write what we have from the scratch buf to the HSCX fifo */
	if (len != 0)
		HSCX_WRFIFO(chan->channel, scrbuf, len);
	return(cmd);
}

/*---------------------------------------------------------------------------*
 *	ifpi - ISAC interrupt routine
 *---------------------------------------------------------------------------*/
static void
ifpi_isac_intr(struct l1_softc *sc)
{
	u_char isac_irq_stat;

	for(;;)
	{
		/* get isac irq status */
		isac_irq_stat = ISAC_READ(I_ISTA);

		if(isac_irq_stat)
			ifpi_isac_irq(sc, isac_irq_stat); /* isac handler */
		else
			break;
	}

	ISAC_WRITE(I_MASK, 0xff);

	DELAY(100);

	ISAC_WRITE(I_MASK, ISAC_IMASK);
}

/*---------------------------------------------------------------------------*
 *	ifpi_recover - try to recover from irq lockup
 *---------------------------------------------------------------------------*/
void
ifpi_recover(struct l1_softc *sc)
{
	u_char byte;
	
	/* get isac irq status */

	byte = ISAC_READ(I_ISTA);

	NDBGL1(L1_ERROR, "  ISAC: ISTA = 0x%x", byte);
	
	if(byte & ISAC_ISTA_EXI)
		NDBGL1(L1_ERROR, "  ISAC: EXIR = 0x%x", (u_char)ISAC_READ(I_EXIR));

	if(byte & ISAC_ISTA_CISQ)
	{
		byte = ISAC_READ(I_CIRR);
	
		NDBGL1(L1_ERROR, "  ISAC: CISQ = 0x%x", byte);
		
		if(byte & ISAC_CIRR_SQC)
			NDBGL1(L1_ERROR, "  ISAC: SQRR = 0x%x", (u_char)ISAC_READ(I_SQRR));
	}

	NDBGL1(L1_ERROR, "  ISAC: IMASK = 0x%x", ISAC_IMASK);

	ISAC_WRITE(I_MASK, 0xff);	
	DELAY(100);
	ISAC_WRITE(I_MASK, ISAC_IMASK);
}


#endif /* NIFPI > 0 */
