/*
 * Copyright (c) 2003 Hidetoshi Shimokawa
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/dev/firewire/fwohci.c,v 1.72 2004/01/22 14:41:17 simokawa Exp $
 * $FreeBSD: src/sys/dev/firewire/fwohci.c,v 1.1.2.19 2003/05/01 06:24:37 simokawa Exp $
 * $DragonFly: src/sys/bus/firewire/fwohci.c,v 1.8 2004/06/02 14:42:48 eirikn Exp $
 */

#define ATRQ_CH 0
#define ATRS_CH 1
#define ARRQ_CH 2
#define ARRS_CH 3
#define ITX_CH 4
#define IRX_CH 0x24

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/endian.h>

#include <machine/bus.h>

#if defined(__DragonFly__) || __FreeBSD_version < 500000
#include <machine/clock.h>		/* for DELAY() */
#endif

#ifdef __DragonFly__
#include "firewire.h"
#include "firewirereg.h"
#include "fwdma.h"
#include "fwohcireg.h"
#include "fwohcivar.h"
#include "firewire_phy.h"
#else
#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwdma.h>
#include <dev/firewire/fwohcireg.h>
#include <dev/firewire/fwohcivar.h>
#include <dev/firewire/firewire_phy.h>
#endif

#undef OHCI_DEBUG

static char dbcode[16][0x10]={"OUTM", "OUTL","INPM","INPL",
		"STOR","LOAD","NOP ","STOP",};

static char dbkey[8][0x10]={"ST0", "ST1","ST2","ST3",
		"UNDEF","REG","SYS","DEV"};
static char dbcond[4][0x10]={"NEV","C=1", "C=0", "ALL"};
char fwohcicode[32][0x20]={
	"No stat","Undef","long","miss Ack err",
	"underrun","overrun","desc err", "data read err",
	"data write err","bus reset","timeout","tcode err",
	"Undef","Undef","unknown event","flushed",
	"Undef","ack complete","ack pend","Undef",
	"ack busy_X","ack busy_A","ack busy_B","Undef",
	"Undef","Undef","Undef","ack tardy",
	"Undef","ack data_err","ack type_err",""};

#define MAX_SPEED 3
extern char *linkspeed[];
u_int32_t tagbit[4] = { 1 << 28, 1 << 29, 1 << 30, 1 << 31};

static struct tcode_info tinfo[] = {
/*		hdr_len block 	flag*/
/* 0 WREQQ  */ {16,	FWTI_REQ | FWTI_TLABEL},
/* 1 WREQB  */ {16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY},
/* 2 WRES   */ {12,	FWTI_RES},
/* 3 XXX    */ { 0,	0},
/* 4 RREQQ  */ {12,	FWTI_REQ | FWTI_TLABEL},
/* 5 RREQB  */ {16,	FWTI_REQ | FWTI_TLABEL},
/* 6 RRESQ  */ {16,	FWTI_RES},
/* 7 RRESB  */ {16,	FWTI_RES | FWTI_BLOCK_ASY},
/* 8 CYCS   */ { 0,	0},
/* 9 LREQ   */ {16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY},
/* a STREAM */ { 4,	FWTI_REQ | FWTI_BLOCK_STR},
/* b LRES   */ {16,	FWTI_RES | FWTI_BLOCK_ASY},
/* c XXX    */ { 0,	0},
/* d XXX    */ { 0, 	0},
/* e PHY    */ {12,	FWTI_REQ},
/* f XXX    */ { 0,	0}
};

#define OHCI_WRITE_SIGMASK 0xffff0000
#define OHCI_READ_SIGMASK 0xffff0000

#define OWRITE(sc, r, x) bus_space_write_4((sc)->bst, (sc)->bsh, (r), (x))
#define OREAD(sc, r) bus_space_read_4((sc)->bst, (sc)->bsh, (r))

static void fwohci_ibr (struct firewire_comm *);
static void fwohci_db_init (struct fwohci_softc *, struct fwohci_dbch *);
static void fwohci_db_free (struct fwohci_dbch *);
static void fwohci_arcv (struct fwohci_softc *, struct fwohci_dbch *, int);
static void fwohci_txd (struct fwohci_softc *, struct fwohci_dbch *);
static void fwohci_start_atq (struct firewire_comm *);
static void fwohci_start_ats (struct firewire_comm *);
static void fwohci_start (struct fwohci_softc *, struct fwohci_dbch *);
static u_int32_t fwphy_wrdata ( struct fwohci_softc *, u_int32_t, u_int32_t);
static u_int32_t fwphy_rddata ( struct fwohci_softc *, u_int32_t);
static int fwohci_rx_enable (struct fwohci_softc *, struct fwohci_dbch *);
static int fwohci_tx_enable (struct fwohci_softc *, struct fwohci_dbch *);
static int fwohci_irx_enable (struct firewire_comm *, int);
static int fwohci_irx_disable (struct firewire_comm *, int);
#if BYTE_ORDER == BIG_ENDIAN
static void fwohci_irx_post (struct firewire_comm *, u_int32_t *);
#endif
static int fwohci_itxbuf_enable (struct firewire_comm *, int);
static int fwohci_itx_disable (struct firewire_comm *, int);
static void fwohci_timeout (void *);
static void fwohci_set_intr (struct firewire_comm *, int);

static int fwohci_add_rx_buf (struct fwohci_dbch *, struct fwohcidb_tr *, int, struct fwdma_alloc *);
static int fwohci_add_tx_buf (struct fwohci_dbch *, struct fwohcidb_tr *, int);
static void	dump_db (struct fwohci_softc *, u_int32_t);
static void 	print_db (struct fwohcidb_tr *, struct fwohcidb *, u_int32_t , u_int32_t);
static void	dump_dma (struct fwohci_softc *, u_int32_t);
static u_int32_t fwohci_cyctimer (struct firewire_comm *);
static void fwohci_rbuf_update (struct fwohci_softc *, int);
static void fwohci_tbuf_update (struct fwohci_softc *, int);
void fwohci_txbufdb (struct fwohci_softc *, int , struct fw_bulkxfer *);
#if FWOHCI_TASKQUEUE
static void fwohci_complete(void *, int);
#endif

/*
 * memory allocated for DMA programs
 */
#define DMA_PROG_ALLOC		(8 * PAGE_SIZE)

#define NDB FWMAXQUEUE

#define	OHCI_VERSION		0x00
#define	OHCI_ATRETRY		0x08
#define	OHCI_CROMHDR		0x18
#define	OHCI_BUS_OPT		0x20
#define	OHCI_BUSIRMC		(1 << 31)
#define	OHCI_BUSCMC		(1 << 30)
#define	OHCI_BUSISC		(1 << 29)
#define	OHCI_BUSBMC		(1 << 28)
#define	OHCI_BUSPMC		(1 << 27)
#define OHCI_BUSFNC		OHCI_BUSIRMC | OHCI_BUSCMC | OHCI_BUSISC |\
				OHCI_BUSBMC | OHCI_BUSPMC

#define	OHCI_EUID_HI		0x24
#define	OHCI_EUID_LO		0x28

#define	OHCI_CROMPTR		0x34
#define	OHCI_HCCCTL		0x50
#define	OHCI_HCCCTLCLR		0x54
#define	OHCI_AREQHI		0x100
#define	OHCI_AREQHICLR		0x104
#define	OHCI_AREQLO		0x108
#define	OHCI_AREQLOCLR		0x10c
#define	OHCI_PREQHI		0x110
#define	OHCI_PREQHICLR		0x114
#define	OHCI_PREQLO		0x118
#define	OHCI_PREQLOCLR		0x11c
#define	OHCI_PREQUPPER		0x120

#define	OHCI_SID_BUF		0x64
#define	OHCI_SID_CNT		0x68
#define OHCI_SID_ERR		(1 << 31)
#define OHCI_SID_CNT_MASK	0xffc

#define	OHCI_IT_STAT		0x90
#define	OHCI_IT_STATCLR		0x94
#define	OHCI_IT_MASK		0x98
#define	OHCI_IT_MASKCLR		0x9c

#define	OHCI_IR_STAT		0xa0
#define	OHCI_IR_STATCLR		0xa4
#define	OHCI_IR_MASK		0xa8
#define	OHCI_IR_MASKCLR		0xac

#define	OHCI_LNKCTL		0xe0
#define	OHCI_LNKCTLCLR		0xe4

#define	OHCI_PHYACCESS		0xec
#define	OHCI_CYCLETIMER		0xf0

#define	OHCI_DMACTL(off)	(off)
#define	OHCI_DMACTLCLR(off)	(off + 4)
#define	OHCI_DMACMD(off)	(off + 0xc)
#define	OHCI_DMAMATCH(off)	(off + 0x10)

#define OHCI_ATQOFF		0x180
#define OHCI_ATQCTL		OHCI_ATQOFF
#define OHCI_ATQCTLCLR		(OHCI_ATQOFF + 4)
#define OHCI_ATQCMD		(OHCI_ATQOFF + 0xc)
#define OHCI_ATQMATCH		(OHCI_ATQOFF + 0x10)

#define OHCI_ATSOFF		0x1a0
#define OHCI_ATSCTL		OHCI_ATSOFF
#define OHCI_ATSCTLCLR		(OHCI_ATSOFF + 4)
#define OHCI_ATSCMD		(OHCI_ATSOFF + 0xc)
#define OHCI_ATSMATCH		(OHCI_ATSOFF + 0x10)

#define OHCI_ARQOFF		0x1c0
#define OHCI_ARQCTL		OHCI_ARQOFF
#define OHCI_ARQCTLCLR		(OHCI_ARQOFF + 4)
#define OHCI_ARQCMD		(OHCI_ARQOFF + 0xc)
#define OHCI_ARQMATCH		(OHCI_ARQOFF + 0x10)

#define OHCI_ARSOFF		0x1e0
#define OHCI_ARSCTL		OHCI_ARSOFF
#define OHCI_ARSCTLCLR		(OHCI_ARSOFF + 4)
#define OHCI_ARSCMD		(OHCI_ARSOFF + 0xc)
#define OHCI_ARSMATCH		(OHCI_ARSOFF + 0x10)

#define OHCI_ITOFF(CH)		(0x200 + 0x10 * (CH))
#define OHCI_ITCTL(CH)		(OHCI_ITOFF(CH))
#define OHCI_ITCTLCLR(CH)	(OHCI_ITOFF(CH) + 4)
#define OHCI_ITCMD(CH)		(OHCI_ITOFF(CH) + 0xc)

#define OHCI_IROFF(CH)		(0x400 + 0x20 * (CH))
#define OHCI_IRCTL(CH)		(OHCI_IROFF(CH))
#define OHCI_IRCTLCLR(CH)	(OHCI_IROFF(CH) + 4)
#define OHCI_IRCMD(CH)		(OHCI_IROFF(CH) + 0xc)
#define OHCI_IRMATCH(CH)	(OHCI_IROFF(CH) + 0x10)

d_ioctl_t fwohci_ioctl;

/*
 * Communication with PHY device
 */
static u_int32_t
fwphy_wrdata( struct fwohci_softc *sc, u_int32_t addr, u_int32_t data)
{
	u_int32_t fun;

	addr &= 0xf;
	data &= 0xff;

	fun = (PHYDEV_WRCMD | (addr << PHYDEV_REGADDR) | (data << PHYDEV_WRDATA));
	OWRITE(sc, OHCI_PHYACCESS, fun);
	DELAY(100);

	return(fwphy_rddata( sc, addr));
}

static u_int32_t
fwohci_set_bus_manager(struct firewire_comm *fc, u_int node)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int i;
	u_int32_t bm;

#define OHCI_CSR_DATA	0x0c
#define OHCI_CSR_COMP	0x10
#define OHCI_CSR_CONT	0x14
#define OHCI_BUS_MANAGER_ID	0

	OWRITE(sc, OHCI_CSR_DATA, node);
	OWRITE(sc, OHCI_CSR_COMP, 0x3f);
	OWRITE(sc, OHCI_CSR_CONT, OHCI_BUS_MANAGER_ID);
 	for (i = 0; !(OREAD(sc, OHCI_CSR_CONT) & (1<<31)) && (i < 1000); i++)
		DELAY(10);
	bm = OREAD(sc, OHCI_CSR_DATA);
	if((bm & 0x3f) == 0x3f)
		bm = node;
	if (bootverbose)
		device_printf(sc->fc.dev,
			"fw_set_bus_manager: %d->%d (loop=%d)\n", bm, node, i);

	return(bm);
}

static u_int32_t
fwphy_rddata(struct fwohci_softc *sc,  u_int addr)
{
	u_int32_t fun, stat;
	u_int i, retry = 0;

	addr &= 0xf;
#define MAX_RETRY 100
again:
	OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_REG_FAIL);
	fun = PHYDEV_RDCMD | (addr << PHYDEV_REGADDR);
	OWRITE(sc, OHCI_PHYACCESS, fun);
	for ( i = 0 ; i < MAX_RETRY ; i ++ ){
		fun = OREAD(sc, OHCI_PHYACCESS);
		if ((fun & PHYDEV_RDCMD) == 0 && (fun & PHYDEV_RDDONE) != 0)
			break;
		DELAY(100);
	}
	if(i >= MAX_RETRY) {
		if (bootverbose)
			device_printf(sc->fc.dev, "phy read failed(1).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	/* Make sure that SCLK is started */
	stat = OREAD(sc, FWOHCI_INTSTAT);
	if ((stat & OHCI_INT_REG_FAIL) != 0 ||
			((fun >> PHYDEV_REGADDR) & 0xf) != addr) {
		if (bootverbose)
			device_printf(sc->fc.dev, "phy read failed(2).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	if (bootverbose || retry >= MAX_RETRY)
		device_printf(sc->fc.dev, 
		    "fwphy_rddata: 0x%x loop=%d, retry=%d\n", addr, i, retry);
#undef MAX_RETRY
	return((fun >> PHYDEV_RDDATA )& 0xff);
}
/* Device specific ioctl. */
int
fwohci_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	struct firewire_softc *sc;
	struct fwohci_softc *fc;
	int unit = DEV2UNIT(dev);
	int err = 0;
	struct fw_reg_req_t *reg  = (struct fw_reg_req_t *) data;
	u_int32_t *dmach = (u_int32_t *) data;

	sc = devclass_get_softc(firewire_devclass, unit);
	if(sc == NULL){
		return(EINVAL);
	}
	fc = (struct fwohci_softc *)sc->fc;

	if (!data)
		return(EINVAL);

	switch (cmd) {
	case FWOHCI_WRREG:
#define OHCI_MAX_REG 0x800
		if(reg->addr <= OHCI_MAX_REG){
			OWRITE(fc, reg->addr, reg->data);
			reg->data = OREAD(fc, reg->addr);
		}else{
			err = EINVAL;
		}
		break;
	case FWOHCI_RDREG:
		if(reg->addr <= OHCI_MAX_REG){
			reg->data = OREAD(fc, reg->addr);
		}else{
			err = EINVAL;
		}
		break;
/* Read DMA descriptors for debug  */
	case DUMPDMA:
		if(*dmach <= OHCI_MAX_DMA_CH ){
			dump_dma(fc, *dmach);
			dump_db(fc, *dmach);
		}else{
			err = EINVAL;
		}
		break;
/* Read/Write Phy registers */
#define OHCI_MAX_PHY_REG 0xf
	case FWOHCI_RDPHYREG:
		if (reg->addr <= OHCI_MAX_PHY_REG)
			reg->data = fwphy_rddata(fc, reg->addr);
		else
			err = EINVAL;
		break;
	case FWOHCI_WRPHYREG:
		if (reg->addr <= OHCI_MAX_PHY_REG)
			reg->data = fwphy_wrdata(fc, reg->addr, reg->data);
		else
			err = EINVAL;
		break;
	default:
		err = EINVAL;
		break;
	}
	return err;
}

static int
fwohci_probe_phy(struct fwohci_softc *sc, device_t dev)
{
	u_int32_t reg, reg2;
	int e1394a = 1;
/*
 * probe PHY parameters
 * 0. to prove PHY version, whether compliance of 1394a.
 * 1. to probe maximum speed supported by the PHY and 
 *    number of port supported by core-logic.
 *    It is not actually available port on your PC .
 */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS);
	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);

	if((reg >> 5) != 7 ){
		sc->fc.mode &= ~FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = reg & FW_PHY_SPD >> 6;
		if (sc->fc.speed > MAX_SPEED) {
			device_printf(dev, "invalid speed %d (fixed to %d).\n",
				sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		device_printf(dev,
			"Phy 1394 only %s, %d ports.\n",
			linkspeed[sc->fc.speed], sc->fc.nport);
	}else{
		reg2 = fwphy_rddata(sc, FW_PHY_ESPD_REG);
		sc->fc.mode |= FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = (reg2 & FW_PHY_ESPD) >> 5;
		if (sc->fc.speed > MAX_SPEED) {
			device_printf(dev, "invalid speed %d (fixed to %d).\n",
				sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		device_printf(dev,
			"Phy 1394a available %s, %d ports.\n",
			linkspeed[sc->fc.speed], sc->fc.nport);

		/* check programPhyEnable */
		reg2 = fwphy_rddata(sc, 5);
#if 0
		if (e1394a && (OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_PRPHY)) {
#else	/* XXX force to enable 1394a */
		if (e1394a) {
#endif
			if (bootverbose)
				device_printf(dev,
					"Enable 1394a Enhancements\n");
			/* enable EAA EMC */
			reg2 |= 0x03;
			/* set aPhyEnhanceEnable */
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_PHYEN);
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_PRPHY);
		} else {
			/* for safe */
			reg2 &= ~0x83;
		}
		reg2 = fwphy_wrdata(sc, 5, reg2);
	}

	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);
	if((reg >> 5) == 7 ){
		reg = fwphy_rddata(sc, 4);
		reg |= 1 << 6;
		fwphy_wrdata(sc, 4, reg);
		reg = fwphy_rddata(sc, 4);
	}
	return 0;
}


void
fwohci_reset(struct fwohci_softc *sc, device_t dev)
{
	int i, max_rec, speed;
	u_int32_t reg, reg2;
	struct fwohcidb_tr *db_tr;

	/* Disable interrupt */ 
	OWRITE(sc, FWOHCI_INTMASKCLR, ~0);

	/* Now stopping all DMA channel */
	OWRITE(sc,  OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	OWRITE(sc,  OHCI_IR_MASKCLR, ~0);
	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		OWRITE(sc,  OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc,  OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

	/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_RESET);
	if (bootverbose)
		device_printf(dev, "resetting OHCI...");
	i = 0;
	while(OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_RESET) {
		if (i++ > 100) break;
		DELAY(1000);
	}
	if (bootverbose)
		printf("done (loop=%d)\n", i);

	/* Probe phy */
	fwohci_probe_phy(sc, dev);

	/* Probe link */
	reg = OREAD(sc,  OHCI_BUS_OPT);
	reg2 = reg | OHCI_BUSFNC;
	max_rec = (reg & 0x0000f000) >> 12;
	speed = (reg & 0x00000007);
	device_printf(dev, "Link %s, max_rec %d bytes.\n",
			linkspeed[speed], MAXREC(max_rec));
	/* XXX fix max_rec */
	sc->fc.maxrec = sc->fc.speed + 8;
	if (max_rec != sc->fc.maxrec) {
		reg2 = (reg2 & 0xffff0fff) | (sc->fc.maxrec << 12);
		device_printf(dev, "max_rec %d -> %d\n",
				MAXREC(max_rec), MAXREC(sc->fc.maxrec));
	}
	if (bootverbose)
		device_printf(dev, "BUS_OPT 0x%x -> 0x%x\n", reg, reg2);
	OWRITE(sc,  OHCI_BUS_OPT, reg2);

	/* Initialize registers */
	OWRITE(sc, OHCI_CROMHDR, sc->fc.config_rom[0]);
	OWRITE(sc, OHCI_CROMPTR, sc->crom_dma.bus_addr);
	OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_BIGEND);
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_POSTWR);
	OWRITE(sc, OHCI_SID_BUF, sc->sid_dma.bus_addr);
	OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_SID);

	/* Enable link */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LINKEN);

	/* Force to start async RX DMA */
	sc->arrq.xferq.flag &= ~FWXFERQ_RUNNING;
	sc->arrs.xferq.flag &= ~FWXFERQ_RUNNING;
	fwohci_rx_enable(sc, &sc->arrq);
	fwohci_rx_enable(sc, &sc->arrs);

	/* Initialize async TX */
	OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);
	OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);

	/* AT Retries */
	OWRITE(sc, FWOHCI_RETRY,
		/* CycleLimit   PhyRespRetries ATRespRetries ATReqRetries */
		(0xffff << 16 ) | (0x0f << 8) | (0x0f << 4) | 0x0f) ;

	sc->atrq.top = STAILQ_FIRST(&sc->atrq.db_trq);
	sc->atrs.top = STAILQ_FIRST(&sc->atrs.db_trq);
	sc->atrq.bottom = sc->atrq.top;
	sc->atrs.bottom = sc->atrs.top;

	for( i = 0, db_tr = sc->atrq.top; i < sc->atrq.ndb ;
				i ++, db_tr = STAILQ_NEXT(db_tr, link)){
		db_tr->xfer = NULL;
	}
	for( i = 0, db_tr = sc->atrs.top; i < sc->atrs.ndb ;
				i ++, db_tr = STAILQ_NEXT(db_tr, link)){
		db_tr->xfer = NULL;
	}


	/* Enable interrupt */
	OWRITE(sc, FWOHCI_INTMASK,
			OHCI_INT_ERR  | OHCI_INT_PHY_SID 
			| OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS 
			| OHCI_INT_DMA_PRRQ | OHCI_INT_DMA_PRRS
			| OHCI_INT_PHY_BUS_R | OHCI_INT_PW_ERR);
	fwohci_set_intr(&sc->fc, 1);

}

int
fwohci_init(struct fwohci_softc *sc, device_t dev)
{
	int i, mver;
	u_int32_t reg;
	u_int8_t ui[8];

#if FWOHCI_TASKQUEUE
	TASK_INIT(&sc->fwohci_task_complete, 0, fwohci_complete, sc);
#endif

/* OHCI version */
	reg = OREAD(sc, OHCI_VERSION);
	mver = (reg >> 16) & 0xff;
	device_printf(dev, "OHCI version %x.%x (ROM=%d)\n",
			mver, reg & 0xff, (reg>>24) & 1);
	if (mver < 1 || mver > 9) {
		device_printf(dev, "invalid OHCI version\n");
		return (ENXIO);
	}

/* Available Isochrounous DMA channel probe */
	OWRITE(sc, OHCI_IT_MASK, 0xffffffff);
	OWRITE(sc, OHCI_IR_MASK, 0xffffffff);
	reg = OREAD(sc, OHCI_IT_MASK) & OREAD(sc, OHCI_IR_MASK);
	OWRITE(sc, OHCI_IT_MASKCLR, 0xffffffff);
	OWRITE(sc, OHCI_IR_MASKCLR, 0xffffffff);
	for (i = 0; i < 0x20; i++)
		if ((reg & (1 << i)) == 0)
			break;
	sc->fc.nisodma = i;
	device_printf(dev, "No. of Isochronous channel is %d.\n", i);
	if (i == 0)
		return (ENXIO);

	sc->fc.arq = &sc->arrq.xferq;
	sc->fc.ars = &sc->arrs.xferq;
	sc->fc.atq = &sc->atrq.xferq;
	sc->fc.ats = &sc->atrs.xferq;

	sc->arrq.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->arrs.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->atrq.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);
	sc->atrs.xferq.psize = roundup2(FWPMAX_S400, PAGE_SIZE);

	sc->arrq.xferq.start = NULL;
	sc->arrs.xferq.start = NULL;
	sc->atrq.xferq.start = fwohci_start_atq;
	sc->atrs.xferq.start = fwohci_start_ats;

	sc->arrq.xferq.buf = NULL;
	sc->arrs.xferq.buf = NULL;
	sc->atrq.xferq.buf = NULL;
	sc->atrs.xferq.buf = NULL;

	sc->arrq.xferq.dmach = -1;
	sc->arrs.xferq.dmach = -1;
	sc->atrq.xferq.dmach = -1;
	sc->atrs.xferq.dmach = -1;

	sc->arrq.ndesc = 1;
	sc->arrs.ndesc = 1;
	sc->atrq.ndesc = 8;	/* equal to maximum of mbuf chains */
	sc->atrs.ndesc = 2;

	sc->arrq.ndb = NDB;
	sc->arrs.ndb = NDB / 2;
	sc->atrq.ndb = NDB;
	sc->atrs.ndb = NDB / 2;

	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		sc->fc.it[i] = &sc->it[i].xferq;
		sc->fc.ir[i] = &sc->ir[i].xferq;
		sc->it[i].xferq.dmach = i;
		sc->ir[i].xferq.dmach = i;
		sc->it[i].ndb = 0;
		sc->ir[i].ndb = 0;
	}

	sc->fc.tcode = tinfo;
	sc->fc.dev = dev;

	sc->fc.config_rom = fwdma_malloc(&sc->fc, CROMSIZE, CROMSIZE,
						&sc->crom_dma, BUS_DMA_WAITOK);
	if(sc->fc.config_rom == NULL){
		device_printf(dev, "config_rom alloc failed.");
		return ENOMEM;
	}

#if 0
	bzero(&sc->fc.config_rom[0], CROMSIZE);
	sc->fc.config_rom[1] = 0x31333934;
	sc->fc.config_rom[2] = 0xf000a002;
	sc->fc.config_rom[3] = OREAD(sc, OHCI_EUID_HI);
	sc->fc.config_rom[4] = OREAD(sc, OHCI_EUID_LO);
	sc->fc.config_rom[5] = 0;
	sc->fc.config_rom[0] = (4 << 24) | (5 << 16);

	sc->fc.config_rom[0] |= fw_crc16(&sc->fc.config_rom[1], 5*4);
#endif


/* SID recieve buffer must allign 2^11 */
#define	OHCI_SIDSIZE	(1 << 11)
	sc->sid_buf = fwdma_malloc(&sc->fc, OHCI_SIDSIZE, OHCI_SIDSIZE,
						&sc->sid_dma, BUS_DMA_WAITOK);
	if (sc->sid_buf == NULL) {
		device_printf(dev, "sid_buf alloc failed.");
		return ENOMEM;
	}

	fwdma_malloc(&sc->fc, sizeof(u_int32_t), sizeof(u_int32_t),
					&sc->dummy_dma, BUS_DMA_WAITOK);

	if (sc->dummy_dma.v_addr == NULL) {
		device_printf(dev, "dummy_dma alloc failed.");
		return ENOMEM;
	}

	fwohci_db_init(sc, &sc->arrq);
	if ((sc->arrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->arrs);
	if ((sc->arrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->atrq);
	if ((sc->atrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(sc, &sc->atrs);
	if ((sc->atrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	sc->fc.eui.hi = OREAD(sc, FWOHCIGUID_H);
	sc->fc.eui.lo = OREAD(sc, FWOHCIGUID_L);
	for( i = 0 ; i < 8 ; i ++)
		ui[i] = FW_EUI64_BYTE(&sc->fc.eui,i);
	device_printf(dev, "EUI64 %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		ui[0], ui[1], ui[2], ui[3], ui[4], ui[5], ui[6], ui[7]);

	sc->fc.ioctl = fwohci_ioctl;
	sc->fc.cyctimer = fwohci_cyctimer;
	sc->fc.set_bmr = fwohci_set_bus_manager;
	sc->fc.ibr = fwohci_ibr;
	sc->fc.irx_enable = fwohci_irx_enable;
	sc->fc.irx_disable = fwohci_irx_disable;

	sc->fc.itx_enable = fwohci_itxbuf_enable;
	sc->fc.itx_disable = fwohci_itx_disable;
#if BYTE_ORDER == BIG_ENDIAN
	sc->fc.irx_post = fwohci_irx_post;
#else
	sc->fc.irx_post = NULL;
#endif
	sc->fc.itx_post = NULL;
	sc->fc.timeout = fwohci_timeout;
	sc->fc.poll = fwohci_poll;
	sc->fc.set_intr = fwohci_set_intr;

	sc->intmask = sc->irstat = sc->itstat = 0;

	fw_init(&sc->fc);
	fwohci_reset(sc, dev);

	return 0;
}

void
fwohci_timeout(void *arg)
{
	struct fwohci_softc *sc;

	sc = (struct fwohci_softc *)arg;
}

u_int32_t
fwohci_cyctimer(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	return(OREAD(sc, OHCI_CYCLETIMER));
}

int
fwohci_detach(struct fwohci_softc *sc, device_t dev)
{
	int i;

	if (sc->sid_buf != NULL)
		fwdma_free(&sc->fc, &sc->sid_dma);
	if (sc->fc.config_rom != NULL)
		fwdma_free(&sc->fc, &sc->crom_dma);

	fwohci_db_free(&sc->arrq);
	fwohci_db_free(&sc->arrs);

	fwohci_db_free(&sc->atrq);
	fwohci_db_free(&sc->atrs);

	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		fwohci_db_free(&sc->it[i]);
		fwohci_db_free(&sc->ir[i]);
	}

	return 0;
}

#define LAST_DB(dbtr, db) do {						\
	struct fwohcidb_tr *_dbtr = (dbtr);				\
	int _cnt = _dbtr->dbcnt;					\
	db = &_dbtr->db[ (_cnt > 2) ? (_cnt -1) : 0];			\
} while (0)
	
static void
fwohci_execute_db(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;
	bus_dma_segment_t *s;
	int i;

	db_tr = (struct fwohcidb_tr *)arg;
	db = &db_tr->db[db_tr->dbcnt];
	if (error) {
		if (firewire_debug || error != EFBIG)
			printf("fwohci_execute_db: error=%d\n", error);
		return;
	}
	for (i = 0; i < nseg; i++) {
		s = &segs[i];
		FWOHCI_DMA_WRITE(db->db.desc.addr, s->ds_addr);
		FWOHCI_DMA_WRITE(db->db.desc.cmd, s->ds_len);
 		FWOHCI_DMA_WRITE(db->db.desc.res, 0);
		db++;
		db_tr->dbcnt++;
	}
}

static void
fwohci_execute_db2(void *arg, bus_dma_segment_t *segs, int nseg,
						bus_size_t size, int error)
{
	fwohci_execute_db(arg, segs, nseg, error);
}

static void
fwohci_start(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int i, s;
	int tcode, hdr_len, pl_off;
	int fsegment = -1;
	u_int32_t off;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	struct fwohci_txpkthdr *ohcifp;
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;
	u_int32_t *ld;
	struct tcode_info *info;
	static int maxdesc=0;

	if(&sc->atrq == dbch){
		off = OHCI_ATQOFF;
	}else if(&sc->atrs == dbch){
		off = OHCI_ATSOFF;
	}else{
		return;
	}

	if (dbch->flags & FWOHCI_DBCH_FULL)
		return;

	s = splfw();
	db_tr = dbch->top;
txloop:
	xfer = STAILQ_FIRST(&dbch->xferq.q);
	if(xfer == NULL){
		goto kick;
	}
	if(dbch->xferq.queued == 0 ){
		device_printf(sc->fc.dev, "TX queue empty\n");
	}
	STAILQ_REMOVE_HEAD(&dbch->xferq.q, link);
	db_tr->xfer = xfer;
	xfer->state = FWXF_START;

	fp = &xfer->send.hdr;
	tcode = fp->mode.common.tcode;

	ohcifp = (struct fwohci_txpkthdr *) db_tr->db[1].db.immed;
	info = &tinfo[tcode];
	hdr_len = pl_off = info->hdr_len;

	ld = &ohcifp->mode.ld[0];
	ld[0] = ld[1] = ld[2] = ld[3] = 0;
	for( i = 0 ; i < pl_off ; i+= 4)
		ld[i/4] = fp->mode.ld[i/4];

	ohcifp->mode.common.spd = xfer->send.spd & 0x7;
	if (tcode == FWTCODE_STREAM ){
		hdr_len = 8;
		ohcifp->mode.stream.len = fp->mode.stream.len;
	} else if (tcode == FWTCODE_PHY) {
		hdr_len = 12;
		ld[1] = fp->mode.ld[1];
		ld[2] = fp->mode.ld[2];
		ohcifp->mode.common.spd = 0;
		ohcifp->mode.common.tcode = FWOHCITCODE_PHY;
	} else {
		ohcifp->mode.asycomm.dst = fp->mode.hdr.dst;
		ohcifp->mode.asycomm.srcbus = OHCI_ASYSRCBUS;
		ohcifp->mode.asycomm.tlrt |= FWRETRY_X;
	}
	db = &db_tr->db[0];
 	FWOHCI_DMA_WRITE(db->db.desc.cmd,
			OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | hdr_len);
 	FWOHCI_DMA_WRITE(db->db.desc.addr, 0);
 	FWOHCI_DMA_WRITE(db->db.desc.res, 0);
/* Specify bound timer of asy. responce */
	if(&sc->atrs == dbch){
 		FWOHCI_DMA_WRITE(db->db.desc.res,
			 (OREAD(sc, OHCI_CYCLETIMER) >> 12) + (1 << 13));
	}
#if BYTE_ORDER == BIG_ENDIAN
	if (tcode == FWTCODE_WREQQ || tcode == FWTCODE_RRESQ)
		hdr_len = 12;
	for (i = 0; i < hdr_len/4; i ++)
		FWOHCI_DMA_WRITE(ld[i], ld[i]);
#endif

again:
	db_tr->dbcnt = 2;
	db = &db_tr->db[db_tr->dbcnt];
	if (xfer->send.pay_len > 0) {
		int err;
		/* handle payload */
		if (xfer->mbuf == NULL) {
			err = bus_dmamap_load(dbch->dmat, db_tr->dma_map,
				&xfer->send.payload[0], xfer->send.pay_len,
				fwohci_execute_db, db_tr,
				/*flags*/0);
		} else {
			/* XXX we can handle only 6 (=8-2) mbuf chains */
			err = bus_dmamap_load_mbuf(dbch->dmat, db_tr->dma_map,
				xfer->mbuf,
				fwohci_execute_db2, db_tr,
				/* flags */0);
			if (err == EFBIG) {
				struct mbuf *m0;

				if (firewire_debug)
					device_printf(sc->fc.dev, "EFBIG.\n");
				m0 = m_getcl(MB_DONTWAIT, MT_DATA, M_PKTHDR);
				if (m0 != NULL) {
					m_copydata(xfer->mbuf, 0,
						xfer->mbuf->m_pkthdr.len,
						mtod(m0, caddr_t));
					m0->m_len = m0->m_pkthdr.len = 
						xfer->mbuf->m_pkthdr.len;
					m_freem(xfer->mbuf);
					xfer->mbuf = m0;
					goto again;
				}
				device_printf(sc->fc.dev, "m_getcl failed.\n");
			}
		}
		if (err)
			printf("dmamap_load: err=%d\n", err);
		bus_dmamap_sync(dbch->dmat, db_tr->dma_map,
						BUS_DMASYNC_PREWRITE);
#if 0 /* OHCI_OUTPUT_MODE == 0 */
		for (i = 2; i < db_tr->dbcnt; i++)
			FWOHCI_DMA_SET(db_tr->db[i].db.desc.cmd,
						OHCI_OUTPUT_MORE);
#endif
	}
	if (maxdesc < db_tr->dbcnt) {
		maxdesc = db_tr->dbcnt;
		if (bootverbose)
			device_printf(sc->fc.dev, "maxdesc: %d\n", maxdesc);
	}
	/* last db */
	LAST_DB(db_tr, db);
 	FWOHCI_DMA_SET(db->db.desc.cmd,
		OHCI_OUTPUT_LAST | OHCI_INTERRUPT_ALWAYS | OHCI_BRANCH_ALWAYS);
 	FWOHCI_DMA_WRITE(db->db.desc.depend,
			STAILQ_NEXT(db_tr, link)->bus_addr);

	if(fsegment == -1 )
		fsegment = db_tr->dbcnt;
	if (dbch->pdb_tr != NULL) {
		LAST_DB(dbch->pdb_tr, db);
 		FWOHCI_DMA_SET(db->db.desc.depend, db_tr->dbcnt);
	}
	dbch->pdb_tr = db_tr;
	db_tr = STAILQ_NEXT(db_tr, link);
	if(db_tr != dbch->bottom){
		goto txloop;
	} else {
		device_printf(sc->fc.dev, "fwohci_start: lack of db_trq\n");
		dbch->flags |= FWOHCI_DBCH_FULL;
	}
kick:
	/* kick asy q */
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREREAD);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREWRITE);

	if(dbch->xferq.flag & FWXFERQ_RUNNING) {
		OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_WAKE);
	} else {
		if (bootverbose)
			device_printf(sc->fc.dev, "start AT DMA status=%x\n",
					OREAD(sc, OHCI_DMACTL(off)));
		OWRITE(sc, OHCI_DMACMD(off), dbch->top->bus_addr | fsegment);
		OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_RUN);
		dbch->xferq.flag |= FWXFERQ_RUNNING;
	}

	dbch->top = db_tr;
	splx(s);
	return;
}

static void
fwohci_start_atq(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_start( sc, &(sc->atrq));
	return;
}

static void
fwohci_start_ats(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_start( sc, &(sc->atrs));
	return;
}

void
fwohci_txd(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int s, ch, err = 0;
	struct fwohcidb_tr *tr;
	struct fwohcidb *db;
	struct fw_xfer *xfer;
	u_int32_t off;
	u_int stat, status;
	int	packets;
	struct firewire_comm *fc = (struct firewire_comm *)sc;

	if(&sc->atrq == dbch){
		off = OHCI_ATQOFF;
		ch = ATRQ_CH;
	}else if(&sc->atrs == dbch){
		off = OHCI_ATSOFF;
		ch = ATRS_CH;
	}else{
		return;
	}
	s = splfw();
	tr = dbch->bottom;
	packets = 0;
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_POSTREAD);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_POSTWRITE);
	while(dbch->xferq.queued > 0){
		LAST_DB(tr, db);
		status = FWOHCI_DMA_READ(db->db.desc.res) >> OHCI_STATUS_SHIFT;
		if(!(status & OHCI_CNTL_DMA_ACTIVE)){
			if (fc->status != FWBUSRESET) 
				/* maybe out of order?? */
				goto out;
		}
		bus_dmamap_sync(dbch->dmat, tr->dma_map,
			BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dbch->dmat, tr->dma_map);
#if 1
		if (firewire_debug)
			dump_db(sc, ch);
#endif
		if(status & OHCI_CNTL_DMA_DEAD) {
			/* Stop DMA */
			OWRITE(sc, OHCI_DMACTLCLR(off), OHCI_CNTL_DMA_RUN);
			device_printf(sc->fc.dev, "force reset AT FIFO\n");
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS | OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_DMACTLCLR(off), OHCI_CNTL_DMA_RUN);
		}
		stat = status & FWOHCIEV_MASK;
		switch(stat){
		case FWOHCIEV_ACKPEND:
		case FWOHCIEV_ACKCOMPL:
			err = 0;
			break;
		case FWOHCIEV_ACKBSA:
		case FWOHCIEV_ACKBSB:
		case FWOHCIEV_ACKBSX:
			device_printf(sc->fc.dev, "txd err=%2x %s\n", stat, fwohcicode[stat]);
			err = EBUSY;
			break;
		case FWOHCIEV_FLUSHED:
		case FWOHCIEV_ACKTARD:
			device_printf(sc->fc.dev, "txd err=%2x %s\n", stat, fwohcicode[stat]);
			err = EAGAIN;
			break;
		case FWOHCIEV_MISSACK:
		case FWOHCIEV_UNDRRUN:
		case FWOHCIEV_OVRRUN:
		case FWOHCIEV_DESCERR:
		case FWOHCIEV_DTRDERR:
		case FWOHCIEV_TIMEOUT:
		case FWOHCIEV_TCODERR:
		case FWOHCIEV_UNKNOWN:
		case FWOHCIEV_ACKDERR:
		case FWOHCIEV_ACKTERR:
		default:
			device_printf(sc->fc.dev, "txd err=%2x %s\n",
							stat, fwohcicode[stat]);
			err = EINVAL;
			break;
		}
		if (tr->xfer != NULL) {
			xfer = tr->xfer;
			if (xfer->state == FWXF_RCVD) {
#if 0
				if (firewire_debug)
					printf("already rcvd\n");
#endif
				fw_xfer_done(xfer);
			} else {
				xfer->state = FWXF_SENT;
				if (err == EBUSY && fc->status != FWBUSRESET) {
					xfer->state = FWXF_BUSY;
					xfer->resp = err;
					if (xfer->retry_req != NULL)
						xfer->retry_req(xfer);
					else {
						xfer->recv.pay_len = 0;
						fw_xfer_done(xfer);
					}
				} else if (stat != FWOHCIEV_ACKPEND) {
					if (stat != FWOHCIEV_ACKCOMPL)
						xfer->state = FWXF_SENTERR;
					xfer->resp = err;
					xfer->recv.pay_len = 0;
					fw_xfer_done(xfer);
				}
			}
			/*
			 * The watchdog timer takes care of split
			 * transcation timeout for ACKPEND case.
			 */
		} else {
			printf("this shouldn't happen\n");
		}
		dbch->xferq.queued --;
		tr->xfer = NULL;

		packets ++;
		tr = STAILQ_NEXT(tr, link);
		dbch->bottom = tr;
		if (dbch->bottom == dbch->top) {
			/* we reaches the end of context program */
			if (firewire_debug && dbch->xferq.queued > 0)
				printf("queued > 0\n");
			break;
		}
	}
out:
	if ((dbch->flags & FWOHCI_DBCH_FULL) && packets > 0) {
		printf("make free slot\n");
		dbch->flags &= ~FWOHCI_DBCH_FULL;
		fwohci_start(sc, dbch);
	}
	splx(s);
}

static void
fwohci_db_free(struct fwohci_dbch *dbch)
{
	struct fwohcidb_tr *db_tr;
	int idb;

	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
		return;

	for(db_tr = STAILQ_FIRST(&dbch->db_trq), idb = 0; idb < dbch->ndb;
			db_tr = STAILQ_NEXT(db_tr, link), idb++){
		if ((dbch->xferq.flag & FWXFERQ_EXTBUF) == 0 &&
					db_tr->buf != NULL) {
			fwdma_free_size(dbch->dmat, db_tr->dma_map,
					db_tr->buf, dbch->xferq.psize);
			db_tr->buf = NULL;
		} else if (db_tr->dma_map != NULL)
			bus_dmamap_destroy(dbch->dmat, db_tr->dma_map);
	}
	dbch->ndb = 0;
	db_tr = STAILQ_FIRST(&dbch->db_trq);
	fwdma_free_multiseg(dbch->am);
	free(db_tr, M_FW);
	STAILQ_INIT(&dbch->db_trq);
	dbch->flags &= ~FWOHCI_DBCH_INIT;
}

static void
fwohci_db_init(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int	idb;
	struct fwohcidb_tr *db_tr;

	if ((dbch->flags & FWOHCI_DBCH_INIT) != 0)
		goto out;

	/* create dma_tag for buffers */
#define MAX_REQCOUNT	0xffff
	if (bus_dma_tag_create(/*parent*/ sc->fc.dmat,
			/*alignment*/ 1, /*boundary*/ 0,
			/*lowaddr*/ BUS_SPACE_MAXADDR_32BIT,
			/*highaddr*/ BUS_SPACE_MAXADDR,
			/*filter*/NULL, /*filterarg*/NULL,
			/*maxsize*/ dbch->xferq.psize,
			/*nsegments*/ dbch->ndesc > 3 ? dbch->ndesc - 2 : 1,
			/*maxsegsz*/ MAX_REQCOUNT,
			/*flags*/ 0,
#if defined(__FreeBSD__) && __FreeBSD_version >= 501102
			/*lockfunc*/busdma_lock_mutex,
			/*lockarg*/&Giant,
#endif
			&dbch->dmat))
		return;

	/* allocate DB entries and attach one to each DMA channels */
	/* DB entry must start at 16 bytes bounary. */
	STAILQ_INIT(&dbch->db_trq);
	db_tr = (struct fwohcidb_tr *)
		malloc(sizeof(struct fwohcidb_tr) * dbch->ndb,
		M_FW, M_WAITOK | M_ZERO);
	if(db_tr == NULL){
		printf("fwohci_db_init: malloc(1) failed\n");
		return;
	}

#define DB_SIZE(x) (sizeof(struct fwohcidb) * (x)->ndesc)
	dbch->am = fwdma_malloc_multiseg(&sc->fc, DB_SIZE(dbch),
		DB_SIZE(dbch), dbch->ndb, BUS_DMA_WAITOK);
	if (dbch->am == NULL) {
		printf("fwohci_db_init: fwdma_malloc_multiseg failed\n");
		free(db_tr, M_FW);
		return;
	}
	/* Attach DB to DMA ch. */
	for(idb = 0 ; idb < dbch->ndb ; idb++){
		db_tr->dbcnt = 0;
		db_tr->db = (struct fwohcidb *)fwdma_v_addr(dbch->am, idb);
		db_tr->bus_addr = fwdma_bus_addr(dbch->am, idb);
		/* create dmamap for buffers */
		/* XXX do we need 4bytes alignment tag? */
		/* XXX don't alloc dma_map for AR */
		if (bus_dmamap_create(dbch->dmat, 0, &db_tr->dma_map) != 0) {
			printf("bus_dmamap_create failed\n");
			dbch->flags = FWOHCI_DBCH_INIT; /* XXX fake */
			fwohci_db_free(dbch);
			return;
		}
		STAILQ_INSERT_TAIL(&dbch->db_trq, db_tr, link);
		if (dbch->xferq.flag & FWXFERQ_EXTBUF) {
			if (idb % dbch->xferq.bnpacket == 0)
				dbch->xferq.bulkxfer[idb / dbch->xferq.bnpacket
						].start = (caddr_t)db_tr;
			if ((idb + 1) % dbch->xferq.bnpacket == 0)
				dbch->xferq.bulkxfer[idb / dbch->xferq.bnpacket
						].end = (caddr_t)db_tr;
		}
		db_tr++;
	}
	STAILQ_LAST(&dbch->db_trq, fwohcidb_tr,link)->link.stqe_next
			= STAILQ_FIRST(&dbch->db_trq);
out:
	dbch->xferq.queued = 0;
	dbch->pdb_tr = NULL;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	dbch->bottom = dbch->top;
	dbch->flags = FWOHCI_DBCH_INIT;
}

static int
fwohci_itx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int sleepch;

	OWRITE(sc, OHCI_ITCTLCLR(dmach), 
			OHCI_CNTL_DMA_RUN | OHCI_CNTL_CYCMATCH_S);
	OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
	/* XXX we cannot free buffers until the DMA really stops */
	tsleep((void *)&sleepch, FWPRI, "fwitxd", hz);
	fwohci_db_free(&sc->it[dmach]);
	sc->it[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}

static int
fwohci_irx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int sleepch;

	OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
	/* XXX we cannot free buffers until the DMA really stops */
	tsleep((void *)&sleepch, FWPRI, "fwirxd", hz);
	fwohci_db_free(&sc->ir[dmach]);
	sc->ir[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}

#if BYTE_ORDER == BIG_ENDIAN
static void
fwohci_irx_post (struct firewire_comm *fc , u_int32_t *qld)
{
	qld[0] = FWOHCI_DMA_READ(qld[0]);
	return;
}
#endif

static int
fwohci_tx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int err = 0;
	int idb, z, i, dmach = 0, ldesc;
	u_int32_t off = 0;
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;

	if(!(dbch->xferq.flag & FWXFERQ_EXTBUF)){
		err = EINVAL;
		return err;
	}
	z = dbch->ndesc;
	for(dmach = 0 ; dmach < sc->fc.nisodma ; dmach++){
		if( &sc->it[dmach] == dbch){
			off = OHCI_ITOFF(dmach);
			break;
		}
	}
	if(off == 0){
		err = EINVAL;
		return err;
	}
	if(dbch->xferq.flag & FWXFERQ_RUNNING)
		return err;
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	for( i = 0, dbch->bottom = dbch->top; i < (dbch->ndb - 1); i++){
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	}
	db_tr = dbch->top;
	for (idb = 0; idb < dbch->ndb; idb ++) {
		fwohci_add_tx_buf(dbch, db_tr, idb);
		if(STAILQ_NEXT(db_tr, link) == NULL){
			break;
		}
		db = db_tr->db;
		ldesc = db_tr->dbcnt - 1;
		FWOHCI_DMA_WRITE(db[0].db.desc.depend,
				STAILQ_NEXT(db_tr, link)->bus_addr | z);
		db[ldesc].db.desc.depend = db[0].db.desc.depend;
		if(dbch->xferq.flag & FWXFERQ_EXTBUF){
			if(((idb + 1 ) % dbch->xferq.bnpacket) == 0){
				FWOHCI_DMA_SET(
					db[ldesc].db.desc.cmd,
					OHCI_INTERRUPT_ALWAYS);
				/* OHCI 1.1 and above */
				FWOHCI_DMA_SET(
					db[0].db.desc.cmd,
					OHCI_INTERRUPT_ALWAYS);
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	FWOHCI_DMA_CLEAR(
		dbch->bottom->db[dbch->bottom->dbcnt - 1].db.desc.depend, 0xf);
	return err;
}

static int
fwohci_rx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int err = 0;
	int idb, z, i, dmach = 0, ldesc;
	u_int32_t off = 0;
	struct fwohcidb_tr *db_tr;
	struct fwohcidb *db;

	z = dbch->ndesc;
	if(&sc->arrq == dbch){
		off = OHCI_ARQOFF;
	}else if(&sc->arrs == dbch){
		off = OHCI_ARSOFF;
	}else{
		for(dmach = 0 ; dmach < sc->fc.nisodma ; dmach++){
			if( &sc->ir[dmach] == dbch){
				off = OHCI_IROFF(dmach);
				break;
			}
		}
	}
	if(off == 0){
		err = EINVAL;
		return err;
	}
	if(dbch->xferq.flag & FWXFERQ_STREAM){
		if(dbch->xferq.flag & FWXFERQ_RUNNING)
			return err;
	}else{
		if(dbch->xferq.flag & FWXFERQ_RUNNING){
			err = EBUSY;
			return err;
		}
	}
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	for( i = 0, dbch->bottom = dbch->top; i < (dbch->ndb - 1); i++){
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	}
	db_tr = dbch->top;
	for (idb = 0; idb < dbch->ndb; idb ++) {
		fwohci_add_rx_buf(dbch, db_tr, idb, &sc->dummy_dma);
		if (STAILQ_NEXT(db_tr, link) == NULL)
			break;
		db = db_tr->db;
		ldesc = db_tr->dbcnt - 1;
		FWOHCI_DMA_WRITE(db[ldesc].db.desc.depend,
			STAILQ_NEXT(db_tr, link)->bus_addr | z);
		if(dbch->xferq.flag & FWXFERQ_EXTBUF){
			if(((idb + 1 ) % dbch->xferq.bnpacket) == 0){
				FWOHCI_DMA_SET(
					db[ldesc].db.desc.cmd,
					OHCI_INTERRUPT_ALWAYS);
				FWOHCI_DMA_CLEAR(
					db[ldesc].db.desc.depend,
					0xf);
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	FWOHCI_DMA_CLEAR(
		dbch->bottom->db[db_tr->dbcnt - 1].db.desc.depend, 0xf);
	dbch->buf_offset = 0;
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREREAD);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREWRITE);
	if(dbch->xferq.flag & FWXFERQ_STREAM){
		return err;
	}else{
		OWRITE(sc, OHCI_DMACMD(off), dbch->top->bus_addr | z);
	}
	OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_RUN);
	return err;
}

static int
fwohci_next_cycle(struct firewire_comm *fc, int cycle_now)
{
	int sec, cycle, cycle_match;

	cycle = cycle_now & 0x1fff;
	sec = cycle_now >> 13;
#define CYCLE_MOD	0x10
#if 1
#define CYCLE_DELAY	8	/* min delay to start DMA */
#else
#define CYCLE_DELAY	7000	/* min delay to start DMA */
#endif
	cycle = cycle + CYCLE_DELAY;
	if (cycle >= 8000) {
		sec ++;
		cycle -= 8000;
	}
	cycle = roundup2(cycle, CYCLE_MOD);
	if (cycle >= 8000) {
		sec ++;
		if (cycle == 8000)
			cycle = 0;
		else
			cycle = CYCLE_MOD;
	}
	cycle_match = ((sec << 13) | cycle) & 0x7ffff;

	return(cycle_match);
}

static int
fwohci_itxbuf_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0;
	unsigned short tag, ich;
	struct fwohci_dbch *dbch;
	int cycle_match, cycle_now, s, ldesc;
	u_int32_t stat;
	struct fw_bulkxfer *first, *chunk, *prev;
	struct fw_xferq *it;

	dbch = &sc->it[dmach];
	it = &dbch->xferq;

	tag = (it->flag >> 6) & 3;
	ich = it->flag & 0x3f;
	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0) {
		dbch->ndb = it->bnpacket * it->bnchunk;
		dbch->ndesc = 3;
		fwohci_db_init(sc, dbch);
		if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_tx_enable(sc, dbch);
	}
	if(err)
		return err;

	ldesc = dbch->ndesc - 1;
	s = splfw();
	prev = STAILQ_LAST(&it->stdma, fw_bulkxfer, link);
	while  ((chunk = STAILQ_FIRST(&it->stvalid)) != NULL) {
		struct fwohcidb *db;

		fwdma_sync_multiseg(it->buf, chunk->poffset, it->bnpacket,
					BUS_DMASYNC_PREWRITE);
		fwohci_txbufdb(sc, dmach, chunk);
		if (prev != NULL) {
			db = ((struct fwohcidb_tr *)(prev->end))->db;
#if 0 /* XXX necessary? */
			FWOHCI_DMA_SET(db[ldesc].db.desc.cmd,
						OHCI_BRANCH_ALWAYS);
#endif
#if 0 /* if bulkxfer->npacket changes */
			db[ldesc].db.desc.depend = db[0].db.desc.depend = 
				((struct fwohcidb_tr *)
				(chunk->start))->bus_addr | dbch->ndesc;
#else
			FWOHCI_DMA_SET(db[0].db.desc.depend, dbch->ndesc);
			FWOHCI_DMA_SET(db[ldesc].db.desc.depend, dbch->ndesc);
#endif
		}
		STAILQ_REMOVE_HEAD(&it->stvalid, link);
		STAILQ_INSERT_TAIL(&it->stdma, chunk, link);
		prev = chunk;
	}
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREWRITE);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREREAD);
	splx(s);
	stat = OREAD(sc, OHCI_ITCTL(dmach));
	if (firewire_debug && (stat & OHCI_CNTL_CYCMATCH_S))
		printf("stat 0x%x\n", stat);

	if (stat & (OHCI_CNTL_DMA_ACTIVE | OHCI_CNTL_CYCMATCH_S))
		return 0;

#if 0
	OWRITE(sc, OHCI_ITCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
#endif
	OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_MASK, 1 << dmach);
	OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IT);

	first = STAILQ_FIRST(&it->stdma);
	OWRITE(sc, OHCI_ITCMD(dmach),
		((struct fwohcidb_tr *)(first->start))->bus_addr | dbch->ndesc);
	if (firewire_debug) {
		printf("fwohci_itxbuf_enable: kick 0x%08x\n", stat);
#if 1
		dump_dma(sc, ITX_CH + dmach);
#endif
	}
	if ((stat & OHCI_CNTL_DMA_RUN) == 0) {
#if 1
		/* Don't start until all chunks are buffered */
		if (STAILQ_FIRST(&it->stfree) != NULL)
			goto out;
#endif
#if 1
		/* Clear cycle match counter bits */
		OWRITE(sc, OHCI_ITCTLCLR(dmach), 0xffff0000);

		/* 2bit second + 13bit cycle */
		cycle_now = (fc->cyctimer(fc) >> 12) & 0x7fff;
		cycle_match = fwohci_next_cycle(fc, cycle_now);

		OWRITE(sc, OHCI_ITCTL(dmach),
				OHCI_CNTL_CYCMATCH_S | (cycle_match << 16)
				| OHCI_CNTL_DMA_RUN);
#else
		OWRITE(sc, OHCI_ITCTL(dmach), OHCI_CNTL_DMA_RUN);
#endif
		if (firewire_debug) {
			printf("cycle_match: 0x%04x->0x%04x\n",
						cycle_now, cycle_match);
			dump_dma(sc, ITX_CH + dmach);
			dump_db(sc, ITX_CH + dmach);
		}
	} else if ((stat & OHCI_CNTL_CYCMATCH_S) == 0) {
		device_printf(sc->fc.dev,
			"IT DMA underrun (0x%08x)\n", stat);
		OWRITE(sc, OHCI_ITCTL(dmach), OHCI_CNTL_DMA_WAKE);
	}
out:
	return err;
}

static int
fwohci_irx_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0, s, ldesc;
	unsigned short tag, ich;
	u_int32_t stat;
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *db_tr;
	struct fw_bulkxfer *first, *prev, *chunk;
	struct fw_xferq *ir;

	dbch = &sc->ir[dmach];
	ir = &dbch->xferq;

	if ((ir->flag & FWXFERQ_RUNNING) == 0) {
		tag = (ir->flag >> 6) & 3;
		ich = ir->flag & 0x3f;
		OWRITE(sc, OHCI_IRMATCH(dmach), tagbit[tag] | ich);

		ir->queued = 0;
		dbch->ndb = ir->bnpacket * ir->bnchunk;
		dbch->ndesc = 2;
		fwohci_db_init(sc, dbch);
		if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_rx_enable(sc, dbch);
	}
	if(err)
		return err;

	first = STAILQ_FIRST(&ir->stfree);
	if (first == NULL) {
		device_printf(fc->dev, "IR DMA no free chunk\n");
		return 0;
	}

	ldesc = dbch->ndesc - 1;
	s = splfw();
	prev = STAILQ_LAST(&ir->stdma, fw_bulkxfer, link);
	while  ((chunk = STAILQ_FIRST(&ir->stfree)) != NULL) {
		struct fwohcidb *db;

#if 1 /* XXX for if_fwe */
		if (chunk->mbuf != NULL) {
			db_tr = (struct fwohcidb_tr *)(chunk->start);
			db_tr->dbcnt = 1;
			err = bus_dmamap_load_mbuf(dbch->dmat, db_tr->dma_map,
					chunk->mbuf, fwohci_execute_db2, db_tr,
					/* flags */0);
 			FWOHCI_DMA_SET(db_tr->db[1].db.desc.cmd,
				OHCI_UPDATE | OHCI_INPUT_LAST |
				OHCI_INTERRUPT_ALWAYS | OHCI_BRANCH_ALWAYS);
		}
#endif
		db = ((struct fwohcidb_tr *)(chunk->end))->db;
		FWOHCI_DMA_WRITE(db[ldesc].db.desc.res, 0);
		FWOHCI_DMA_CLEAR(db[ldesc].db.desc.depend, 0xf);
		if (prev != NULL) {
			db = ((struct fwohcidb_tr *)(prev->end))->db;
			FWOHCI_DMA_SET(db[ldesc].db.desc.depend, dbch->ndesc);
		}
		STAILQ_REMOVE_HEAD(&ir->stfree, link);
		STAILQ_INSERT_TAIL(&ir->stdma, chunk, link);
		prev = chunk;
	}
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREWRITE);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREREAD);
	splx(s);
	stat = OREAD(sc, OHCI_IRCTL(dmach));
	if (stat & OHCI_CNTL_DMA_ACTIVE)
		return 0;
	if (stat & OHCI_CNTL_DMA_RUN) {
		OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
		device_printf(sc->fc.dev, "IR DMA overrun (0x%08x)\n", stat);
	}

	if (firewire_debug)
		printf("start IR DMA 0x%x\n", stat);
	OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_MASK, 1 << dmach);
	OWRITE(sc, OHCI_IRCTLCLR(dmach), 0xf0000000);
	OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_ISOHDR);
	OWRITE(sc, OHCI_IRCMD(dmach),
		((struct fwohcidb_tr *)(first->start))->bus_addr
							| dbch->ndesc);
	OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IR);
#if 0
	dump_db(sc, IRX_CH + dmach);
#endif
	return err;
}

int
fwohci_stop(struct fwohci_softc *sc, device_t dev)
{
	u_int i;

/* Now stopping all DMA channel */
	OWRITE(sc,  OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		OWRITE(sc,  OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc,  OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc,  OHCI_HCCCTL, OHCI_HCC_RESET);

/* Stop interrupt */
	OWRITE(sc, FWOHCI_INTMASKCLR,
			OHCI_INT_EN | OHCI_INT_ERR | OHCI_INT_PHY_SID
			| OHCI_INT_PHY_INT
			| OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS 
			| OHCI_INT_DMA_PRRQ | OHCI_INT_DMA_PRRS
			| OHCI_INT_DMA_ARRQ | OHCI_INT_DMA_ARRS 
			| OHCI_INT_PHY_BUS_R);

	if (sc->fc.arq !=0 && sc->fc.arq->maxq > 0)
		fw_drain_txq(&sc->fc);

/* XXX Link down?  Bus reset? */
	return 0;
}

int
fwohci_resume(struct fwohci_softc *sc, device_t dev)
{
	int i;
	struct fw_xferq *ir;
	struct fw_bulkxfer *chunk;

	fwohci_reset(sc, dev);
	/* XXX resume isochronus receive automatically. (how about TX?) */
	for(i = 0; i < sc->fc.nisodma; i ++) {
		ir = &sc->ir[i].xferq;
		if((ir->flag & FWXFERQ_RUNNING) != 0) {
			device_printf(sc->fc.dev,
				"resume iso receive ch: %d\n", i);
			ir->flag &= ~FWXFERQ_RUNNING;
			/* requeue stdma to stfree */
			while((chunk = STAILQ_FIRST(&ir->stdma)) != NULL) {
				STAILQ_REMOVE_HEAD(&ir->stdma, link);
				STAILQ_INSERT_TAIL(&ir->stfree, chunk, link);
			}
			sc->fc.irx_enable(&sc->fc, i);
		}
	}

	bus_generic_resume(dev);
	sc->fc.ibr(&sc->fc);
	return 0;
}

#define ACK_ALL
static void
fwohci_intr_body(struct fwohci_softc *sc, u_int32_t stat, int count)
{
	u_int32_t irstat, itstat;
	u_int i;
	struct firewire_comm *fc = (struct firewire_comm *)sc;

#ifdef OHCI_DEBUG
	if(stat & OREAD(sc, FWOHCI_INTMASK))
		device_printf(fc->dev, "INTERRUPT < %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s> 0x%08x, 0x%08x\n",
			stat & OHCI_INT_EN ? "DMA_EN ":"",
			stat & OHCI_INT_PHY_REG ? "PHY_REG ":"",
			stat & OHCI_INT_CYC_LONG ? "CYC_LONG ":"",
			stat & OHCI_INT_ERR ? "INT_ERR ":"",
			stat & OHCI_INT_CYC_ERR ? "CYC_ERR ":"",
			stat & OHCI_INT_CYC_LOST ? "CYC_LOST ":"",
			stat & OHCI_INT_CYC_64SECOND ? "CYC_64SECOND ":"",
			stat & OHCI_INT_CYC_START ? "CYC_START ":"",
			stat & OHCI_INT_PHY_INT ? "PHY_INT ":"",
			stat & OHCI_INT_PHY_BUS_R ? "BUS_RESET ":"",
			stat & OHCI_INT_PHY_SID ? "SID ":"",
			stat & OHCI_INT_LR_ERR ? "DMA_LR_ERR ":"",
			stat & OHCI_INT_PW_ERR ? "DMA_PW_ERR ":"",
			stat & OHCI_INT_DMA_IR ? "DMA_IR ":"",
			stat & OHCI_INT_DMA_IT  ? "DMA_IT " :"",
			stat & OHCI_INT_DMA_PRRS  ? "DMA_PRRS " :"",
			stat & OHCI_INT_DMA_PRRQ  ? "DMA_PRRQ " :"",
			stat & OHCI_INT_DMA_ARRS  ? "DMA_ARRS " :"",
			stat & OHCI_INT_DMA_ARRQ  ? "DMA_ARRQ " :"",
			stat & OHCI_INT_DMA_ATRS  ? "DMA_ATRS " :"",
			stat & OHCI_INT_DMA_ATRQ  ? "DMA_ATRQ " :"",
			stat, OREAD(sc, FWOHCI_INTMASK) 
		);
#endif
/* Bus reset */
	if(stat & OHCI_INT_PHY_BUS_R ){
		if (fc->status == FWBUSRESET)
			goto busresetout;
		/* Disable bus reset interrupt until sid recv. */
		OWRITE(sc, FWOHCI_INTMASKCLR,  OHCI_INT_PHY_BUS_R);
	
		device_printf(fc->dev, "BUS reset\n");
		OWRITE(sc, FWOHCI_INTMASKCLR,  OHCI_INT_CYC_LOST);
		OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCSRC);

		OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrq.xferq.flag &= ~FWXFERQ_RUNNING;
		OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrs.xferq.flag &= ~FWXFERQ_RUNNING;

#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_BUS_R);
#endif
		fw_busreset(fc);
		OWRITE(sc, OHCI_CROMHDR, ntohl(sc->fc.config_rom[0]));
		OWRITE(sc, OHCI_BUS_OPT, ntohl(sc->fc.config_rom[2]));
	}
busresetout:
	if((stat & OHCI_INT_DMA_IR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_IR);
#endif
#if defined(__DragonFly__) || __FreeBSD_version < 500000
		irstat = sc->irstat;
		sc->irstat = 0;
#else
		irstat = atomic_readandclear_int(&sc->irstat);
#endif
		for(i = 0; i < fc->nisodma ; i++){
			struct fwohci_dbch *dbch;

			if((irstat & (1 << i)) != 0){
				dbch = &sc->ir[i];
				if ((dbch->xferq.flag & FWXFERQ_OPEN) == 0) {
					device_printf(sc->fc.dev,
						"dma(%d) not active\n", i);
					continue;
				}
				fwohci_rbuf_update(sc, i);
			}
		}
	}
	if((stat & OHCI_INT_DMA_IT )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_IT);
#endif
#if defined(__DragonFly__) || __FreeBSD_version < 500000
		itstat = sc->itstat;
		sc->itstat = 0;
#else
		itstat = atomic_readandclear_int(&sc->itstat);
#endif
		for(i = 0; i < fc->nisodma ; i++){
			if((itstat & (1 << i)) != 0){
				fwohci_tbuf_update(sc, i);
			}
		}
	}
	if((stat & OHCI_INT_DMA_PRRS )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_PRRS);
#endif
#if 0
		dump_dma(sc, ARRS_CH);
		dump_db(sc, ARRS_CH);
#endif
		fwohci_arcv(sc, &sc->arrs, count);
	}
	if((stat & OHCI_INT_DMA_PRRQ )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_PRRQ);
#endif
#if 0
		dump_dma(sc, ARRQ_CH);
		dump_db(sc, ARRQ_CH);
#endif
		fwohci_arcv(sc, &sc->arrq, count);
	}
	if(stat & OHCI_INT_PHY_SID){
		u_int32_t *buf, node_id;
		int plen;

#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_SID);
#endif
		/* Enable bus reset interrupt */
		OWRITE(sc, FWOHCI_INTMASK,  OHCI_INT_PHY_BUS_R);
		/* Allow async. request to us */
		OWRITE(sc, OHCI_AREQHI, 1 << 31);
		/* XXX insecure ?? */
		OWRITE(sc, OHCI_PREQHI, 0x7fffffff);
		OWRITE(sc, OHCI_PREQLO, 0xffffffff);
		OWRITE(sc, OHCI_PREQUPPER, 0x10000);
		/* Set ATRetries register */
		OWRITE(sc, OHCI_ATRETRY, 1<<(13+16) | 0xfff);
/*
** Checking whether the node is root or not. If root, turn on 
** cycle master.
*/
		node_id = OREAD(sc, FWOHCI_NODEID);
		plen = OREAD(sc, OHCI_SID_CNT);

		device_printf(fc->dev, "node_id=0x%08x, gen=%d, ",
			node_id, (plen >> 16) & 0xff);
		if (!(node_id & OHCI_NODE_VALID)) {
			printf("Bus reset failure\n");
			goto sidout;
		}
		if (node_id & OHCI_NODE_ROOT) {
			printf("CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTL,
				OHCI_CNTL_CYCMTR | OHCI_CNTL_CYCTIMER);
		} else {
			printf("non CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCMTR);
			OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_CYCTIMER);
		}
		fc->nodeid = node_id & 0x3f;

		if (plen & OHCI_SID_ERR) {
			device_printf(fc->dev, "SID Error\n");
			goto sidout;
		}
		plen &= OHCI_SID_CNT_MASK;
		if (plen < 4 || plen > OHCI_SIDSIZE) {
			device_printf(fc->dev, "invalid SID len = %d\n", plen);
			goto sidout;
		}
		plen -= 4; /* chop control info */
		buf = (u_int32_t *)malloc(OHCI_SIDSIZE, M_FW, M_INTWAIT);
		if (buf == NULL) {
			device_printf(fc->dev, "malloc failed\n");
			goto sidout;
		}
		for (i = 0; i < plen / 4; i ++)
			buf[i] = FWOHCI_DMA_READ(sc->sid_buf[i+1]);
#if 1
		/* pending all pre-bus_reset packets */
		fwohci_txd(sc, &sc->atrq);
		fwohci_txd(sc, &sc->atrs);
		fwohci_arcv(sc, &sc->arrs, -1);
		fwohci_arcv(sc, &sc->arrq, -1);
		fw_drain_txq(fc);
#endif
		fw_sidrcv(fc, buf, plen);
		free(buf, M_FW);
	}
sidout:
	if((stat & OHCI_INT_DMA_ATRQ )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_ATRQ);
#endif
		fwohci_txd(sc, &(sc->atrq));
	}
	if((stat & OHCI_INT_DMA_ATRS )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_ATRS);
#endif
		fwohci_txd(sc, &(sc->atrs));
	}
	if((stat & OHCI_INT_PW_ERR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PW_ERR);
#endif
		device_printf(fc->dev, "posted write error\n");
	}
	if((stat & OHCI_INT_ERR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_ERR);
#endif
		device_printf(fc->dev, "unrecoverable error\n");
	}
	if((stat & OHCI_INT_PHY_INT)) {
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_INT);
#endif
		device_printf(fc->dev, "phy int\n");
	}

	return;
}

#if FWOHCI_TASKQUEUE
static void
fwohci_complete(void *arg, int pending)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)arg;
	u_int32_t stat;

again:
	stat = atomic_readandclear_int(&sc->intstat);
	if (stat)
		fwohci_intr_body(sc, stat, -1);
	else
		return;
	goto again;
}
#endif

static u_int32_t
fwochi_check_stat(struct fwohci_softc *sc)
{
	u_int32_t stat, irstat, itstat;

	stat = OREAD(sc, FWOHCI_INTSTAT);
	if (stat == 0xffffffff) {
		device_printf(sc->fc.dev, 
			"device physically ejected?\n");
		return(stat);
	}
#ifdef ACK_ALL
	if (stat)
		OWRITE(sc, FWOHCI_INTSTATCLR, stat);
#endif
	if (stat & OHCI_INT_DMA_IR) {
		irstat = OREAD(sc, OHCI_IR_STAT);
		OWRITE(sc, OHCI_IR_STATCLR, irstat);
		atomic_set_int(&sc->irstat, irstat);
	}
	if (stat & OHCI_INT_DMA_IT) {
		itstat = OREAD(sc, OHCI_IT_STAT);
		OWRITE(sc, OHCI_IT_STATCLR, itstat);
		atomic_set_int(&sc->itstat, itstat);
	}
	return(stat);
}

void
fwohci_intr(void *arg)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)arg;
	u_int32_t stat;
#if !FWOHCI_TASKQUEUE
	u_int32_t bus_reset = 0;
#endif

	if (!(sc->intmask & OHCI_INT_EN)) {
		/* polling mode */
		return;
	}

#if !FWOHCI_TASKQUEUE
again:
#endif
	stat = fwochi_check_stat(sc);
	if (stat == 0 || stat == 0xffffffff)
		return;
#if FWOHCI_TASKQUEUE
	atomic_set_int(&sc->intstat, stat);
	/* XXX mask bus reset intr. during bus reset phase */
	if (stat)
		taskqueue_enqueue(taskqueue_swi_giant, &sc->fwohci_task_complete);
#else
	/* We cannot clear bus reset event during bus reset phase */
	if ((stat & ~bus_reset) == 0)
		return;
	bus_reset = stat & OHCI_INT_PHY_BUS_R;
	fwohci_intr_body(sc, stat, -1);
	goto again;
#endif
}

void
fwohci_poll(struct firewire_comm *fc, int quick, int count)
{
	int s;
	u_int32_t stat;
	struct fwohci_softc *sc;


	sc = (struct fwohci_softc *)fc;
	stat = OHCI_INT_DMA_IR | OHCI_INT_DMA_IT |
		OHCI_INT_DMA_PRRS | OHCI_INT_DMA_PRRQ |
		OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS;
#if 0
	if (!quick) {
#else
	if (1) {
#endif
		stat = fwochi_check_stat(sc);
		if (stat == 0 || stat == 0xffffffff)
			return;
	}
	s = splfw();
	fwohci_intr_body(sc, stat, count);
	splx(s);
}

static void
fwohci_set_intr(struct firewire_comm *fc, int enable)
{
	struct fwohci_softc *sc;

	sc = (struct fwohci_softc *)fc;
	if (bootverbose)
		device_printf(sc->fc.dev, "fwohci_set_intr: %d\n", enable);
	if (enable) {
		sc->intmask |= OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_EN);
	} else {
		sc->intmask &= ~OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);
	}
}

static void
fwohci_tbuf_update(struct fwohci_softc *sc, int dmach)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb *db;
	struct fw_bulkxfer *chunk;
	struct fw_xferq *it;
	u_int32_t stat, count;
	int s, w=0, ldesc;

	it = fc->it[dmach];
	ldesc = sc->it[dmach].ndesc - 1;
	s = splfw(); /* unnecessary ? */
	fwdma_sync_multiseg_all(sc->it[dmach].am, BUS_DMASYNC_POSTREAD);
	if (firewire_debug)
		dump_db(sc, ITX_CH + dmach);
	while ((chunk = STAILQ_FIRST(&it->stdma)) != NULL) {
		db = ((struct fwohcidb_tr *)(chunk->end))->db;
		stat = FWOHCI_DMA_READ(db[ldesc].db.desc.res) 
				>> OHCI_STATUS_SHIFT;
		db = ((struct fwohcidb_tr *)(chunk->start))->db;
		/* timestamp */
		count = FWOHCI_DMA_READ(db[ldesc].db.desc.res)
				& OHCI_COUNT_MASK;
		if (stat == 0)
			break;
		STAILQ_REMOVE_HEAD(&it->stdma, link);
		switch (stat & FWOHCIEV_MASK){
		case FWOHCIEV_ACKCOMPL:
#if 0
			device_printf(fc->dev, "0x%08x\n", count);
#endif
			break;
		default:
			device_printf(fc->dev,
				"Isochronous transmit err %02x(%s)\n",
					stat, fwohcicode[stat & 0x1f]);
		}
		STAILQ_INSERT_TAIL(&it->stfree, chunk, link);
		w++;
	}
	splx(s);
	if (w)
		wakeup(it);
}

static void
fwohci_rbuf_update(struct fwohci_softc *sc, int dmach)
{
	struct firewire_comm *fc = &sc->fc;
	struct fwohcidb_tr *db_tr;
	struct fw_bulkxfer *chunk;
	struct fw_xferq *ir;
	u_int32_t stat;
	int s, w=0, ldesc;

	ir = fc->ir[dmach];
	ldesc = sc->ir[dmach].ndesc - 1;
#if 0
	dump_db(sc, dmach);
#endif
	s = splfw();
	fwdma_sync_multiseg_all(sc->ir[dmach].am, BUS_DMASYNC_POSTREAD);
	while ((chunk = STAILQ_FIRST(&ir->stdma)) != NULL) {
		db_tr = (struct fwohcidb_tr *)chunk->end;
		stat = FWOHCI_DMA_READ(db_tr->db[ldesc].db.desc.res)
				>> OHCI_STATUS_SHIFT;
		if (stat == 0)
			break;

		if (chunk->mbuf != NULL) {
			bus_dmamap_sync(sc->ir[dmach].dmat, db_tr->dma_map,
						BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->ir[dmach].dmat, db_tr->dma_map);
		} else if (ir->buf != NULL) {
			fwdma_sync_multiseg(ir->buf, chunk->poffset,
				ir->bnpacket, BUS_DMASYNC_POSTREAD);
		} else {
			/* XXX */
			printf("fwohci_rbuf_update: this shouldn't happend\n");
		}

		STAILQ_REMOVE_HEAD(&ir->stdma, link);
		STAILQ_INSERT_TAIL(&ir->stvalid, chunk, link);
		switch (stat & FWOHCIEV_MASK) {
		case FWOHCIEV_ACKCOMPL:
			chunk->resp = 0;
			break;
		default:
			chunk->resp = EINVAL;
			device_printf(fc->dev,
				"Isochronous receive err %02x(%s)\n",
					stat, fwohcicode[stat & 0x1f]);
		}
		w++;
	}
	splx(s);
	if (w) {
		if (ir->flag & FWXFERQ_HANDLER) 
			ir->hand(ir);
		else
			wakeup(ir);
	}
}

void
dump_dma(struct fwohci_softc *sc, u_int32_t ch)
{
	u_int32_t off, cntl, stat, cmd, match;

	if(ch == 0){
		off = OHCI_ATQOFF;
	}else if(ch == 1){
		off = OHCI_ATSOFF;
	}else if(ch == 2){
		off = OHCI_ARQOFF;
	}else if(ch == 3){
		off = OHCI_ARSOFF;
	}else if(ch < IRX_CH){
		off = OHCI_ITCTL(ch - ITX_CH);
	}else{
		off = OHCI_IRCTL(ch - IRX_CH);
	}
	cntl = stat = OREAD(sc, off);
	cmd = OREAD(sc, off + 0xc);
	match = OREAD(sc, off + 0x10);

	device_printf(sc->fc.dev, "ch %1x cntl:0x%08x cmd:0x%08x match:0x%08x\n",
		ch,
		cntl, 
		cmd, 
		match);
	stat &= 0xffff ;
	if (stat) {
		device_printf(sc->fc.dev, "dma %d ch:%s%s%s%s%s%s %s(%x)\n",
			ch,
			stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
			stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
			stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
			stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
			stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
			stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
			fwohcicode[stat & 0x1f],
			stat & 0x1f
		);
	}else{
		device_printf(sc->fc.dev, "dma %d ch: Nostat\n", ch);
	}
}

void
dump_db(struct fwohci_softc *sc, u_int32_t ch)
{
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *cp = NULL, *pp, *np = NULL;
	struct fwohcidb *curr = NULL, *prev, *next = NULL;
	int idb, jdb;
	u_int32_t cmd, off;
	if(ch == 0){
		off = OHCI_ATQOFF;
		dbch = &sc->atrq;
	}else if(ch == 1){
		off = OHCI_ATSOFF;
		dbch = &sc->atrs;
	}else if(ch == 2){
		off = OHCI_ARQOFF;
		dbch = &sc->arrq;
	}else if(ch == 3){
		off = OHCI_ARSOFF;
		dbch = &sc->arrs;
	}else if(ch < IRX_CH){
		off = OHCI_ITCTL(ch - ITX_CH);
		dbch = &sc->it[ch - ITX_CH];
	}else {
		off = OHCI_IRCTL(ch - IRX_CH);
		dbch = &sc->ir[ch - IRX_CH];
	}
	cmd = OREAD(sc, off + 0xc);

	if( dbch->ndb == 0 ){
		device_printf(sc->fc.dev, "No DB is attached ch=%d\n", ch);
		return;
	}
	pp = dbch->top;
	prev = pp->db;
	for(idb = 0 ; idb < dbch->ndb ; idb ++ ){
		if(pp == NULL){
			curr = NULL;
			goto outdb;
		}
		cp = STAILQ_NEXT(pp, link);
		if(cp == NULL){
			curr = NULL;
			goto outdb;
		}
		np = STAILQ_NEXT(cp, link);
		for(jdb = 0 ; jdb < dbch->ndesc ; jdb ++ ){
			if ((cmd  & 0xfffffff0) == cp->bus_addr) {
				curr = cp->db;
				if(np != NULL){
					next = np->db;
				}else{
					next = NULL;
				}
				goto outdb;
			}
		}
		pp = STAILQ_NEXT(pp, link);
		prev = pp->db;
	}
outdb:
	if( curr != NULL){
#if 0
		printf("Prev DB %d\n", ch);
		print_db(pp, prev, ch, dbch->ndesc);
#endif
		printf("Current DB %d\n", ch);
		print_db(cp, curr, ch, dbch->ndesc);
#if 0
		printf("Next DB %d\n", ch);
		print_db(np, next, ch, dbch->ndesc);
#endif
	}else{
		printf("dbdump err ch = %d cmd = 0x%08x\n", ch, cmd);
	}
	return;
}

void
print_db(struct fwohcidb_tr *db_tr, struct fwohcidb *db,
		u_int32_t ch, u_int32_t max)
{
	fwohcireg_t stat;
	int i, key;
	u_int32_t cmd, res;

	if(db == NULL){
		printf("No Descriptor is found\n");
		return;
	}

	printf("ch = %d\n%8s %s %s %s %s %4s %8s %8s %4s:%4s\n",
		ch,
		"Current",
		"OP  ",
		"KEY",
		"INT",
		"BR ",
		"len",
		"Addr",
		"Depend",
		"Stat",
		"Cnt");
	for( i = 0 ; i <= max ; i ++){
		cmd = FWOHCI_DMA_READ(db[i].db.desc.cmd);
		res = FWOHCI_DMA_READ(db[i].db.desc.res);
		key = cmd & OHCI_KEY_MASK;
		stat = res >> OHCI_STATUS_SHIFT;
#if defined(__DragonFly__) || __FreeBSD_version < 500000
		printf("%08x %s %s %s %s %5d %08x %08x %04x:%04x",
				db_tr->bus_addr,
#else
		printf("%08jx %s %s %s %s %5d %08x %08x %04x:%04x",
				(uintmax_t)db_tr->bus_addr,
#endif
				dbcode[(cmd >> 28) & 0xf],
				dbkey[(cmd >> 24) & 0x7],
				dbcond[(cmd >> 20) & 0x3],
				dbcond[(cmd >> 18) & 0x3],
				cmd & OHCI_COUNT_MASK,
				FWOHCI_DMA_READ(db[i].db.desc.addr),
				FWOHCI_DMA_READ(db[i].db.desc.depend),
				stat,
				res & OHCI_COUNT_MASK);
		if(stat & 0xff00){
			printf(" %s%s%s%s%s%s %s(%x)\n",
				stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
				stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
				stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
				stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
				stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
				stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
				fwohcicode[stat & 0x1f],
				stat & 0x1f
			);
		}else{
			printf(" Nostat\n");
		}
		if(key == OHCI_KEY_ST2 ){
			printf("0x%08x 0x%08x 0x%08x 0x%08x\n", 
				FWOHCI_DMA_READ(db[i+1].db.immed[0]),
				FWOHCI_DMA_READ(db[i+1].db.immed[1]),
				FWOHCI_DMA_READ(db[i+1].db.immed[2]),
				FWOHCI_DMA_READ(db[i+1].db.immed[3]));
		}
		if(key == OHCI_KEY_DEVICE){
			return;
		}
		if((cmd & OHCI_BRANCH_MASK) 
				== OHCI_BRANCH_ALWAYS){
			return;
		}
		if((cmd & OHCI_CMD_MASK) 
				== OHCI_OUTPUT_LAST){
			return;
		}
		if((cmd & OHCI_CMD_MASK) 
				== OHCI_INPUT_LAST){
			return;
		}
		if(key == OHCI_KEY_ST2 ){
			i++;
		}
	}
	return;
}

void
fwohci_ibr(struct firewire_comm *fc)
{
	struct fwohci_softc *sc;
	u_int32_t fun;

	device_printf(fc->dev, "Initiate bus reset\n");
	sc = (struct fwohci_softc *)fc;

	/*
	 * Set root hold-off bit so that non cyclemaster capable node
	 * shouldn't became the root node.
	 */
#if 1
	fun = fwphy_rddata(sc, FW_PHY_IBR_REG);
	fun |= FW_PHY_IBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_IBR_REG, fun);
#else	/* Short bus reset */
	fun = fwphy_rddata(sc, FW_PHY_ISBR_REG);
	fun |= FW_PHY_ISBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_ISBR_REG, fun);
#endif
}

void
fwohci_txbufdb(struct fwohci_softc *sc, int dmach, struct fw_bulkxfer *bulkxfer)
{
	struct fwohcidb_tr *db_tr, *fdb_tr;
	struct fwohci_dbch *dbch;
	struct fwohcidb *db;
	struct fw_pkt *fp;
	struct fwohci_txpkthdr *ohcifp;
	unsigned short chtag;
	int idb;

	dbch = &sc->it[dmach];
	chtag = sc->it[dmach].xferq.flag & 0xff;

	db_tr = (struct fwohcidb_tr *)(bulkxfer->start);
	fdb_tr = (struct fwohcidb_tr *)(bulkxfer->end);
/*
device_printf(sc->fc.dev, "DB %08x %08x %08x\n", bulkxfer, db_tr->bus_addr, fdb_tr->bus_addr);
*/
	for (idb = 0; idb < dbch->xferq.bnpacket; idb ++) {
		db = db_tr->db;
		fp = (struct fw_pkt *)db_tr->buf;
		ohcifp = (struct fwohci_txpkthdr *) db[1].db.immed;
		ohcifp->mode.ld[0] = fp->mode.ld[0];
		ohcifp->mode.common.spd = 0 & 0x7;
		ohcifp->mode.stream.len = fp->mode.stream.len;
		ohcifp->mode.stream.chtag = chtag;
		ohcifp->mode.stream.tcode = 0xa;
#if BYTE_ORDER == BIG_ENDIAN
		FWOHCI_DMA_WRITE(db[1].db.immed[0], db[1].db.immed[0]); 
		FWOHCI_DMA_WRITE(db[1].db.immed[1], db[1].db.immed[1]); 
#endif

		FWOHCI_DMA_CLEAR(db[2].db.desc.cmd, OHCI_COUNT_MASK);
		FWOHCI_DMA_SET(db[2].db.desc.cmd, fp->mode.stream.len);
		FWOHCI_DMA_WRITE(db[2].db.desc.res, 0);
#if 0 /* if bulkxfer->npackets changes */
		db[2].db.desc.cmd = OHCI_OUTPUT_LAST
			| OHCI_UPDATE
			| OHCI_BRANCH_ALWAYS;
		db[0].db.desc.depend =
			= db[dbch->ndesc - 1].db.desc.depend
			= STAILQ_NEXT(db_tr, link)->bus_addr | dbch->ndesc;
#else
		FWOHCI_DMA_SET(db[0].db.desc.depend, dbch->ndesc);
		FWOHCI_DMA_SET(db[dbch->ndesc - 1].db.desc.depend, dbch->ndesc);
#endif
		bulkxfer->end = (caddr_t)db_tr;
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	db = ((struct fwohcidb_tr *)bulkxfer->end)->db;
	FWOHCI_DMA_CLEAR(db[0].db.desc.depend, 0xf);
	FWOHCI_DMA_CLEAR(db[dbch->ndesc - 1].db.desc.depend, 0xf);
#if 0 /* if bulkxfer->npackets changes */
	db[dbch->ndesc - 1].db.desc.control |= OHCI_INTERRUPT_ALWAYS;
	/* OHCI 1.1 and above */
	db[0].db.desc.control |= OHCI_INTERRUPT_ALWAYS;
#endif
/*
	db_tr = (struct fwohcidb_tr *)bulkxfer->start;
	fdb_tr = (struct fwohcidb_tr *)bulkxfer->end;
device_printf(sc->fc.dev, "DB %08x %3d %08x %08x\n", bulkxfer, bulkxfer->npacket, db_tr->bus_addr, fdb_tr->bus_addr);
*/
	return;
}

static int
fwohci_add_tx_buf(struct fwohci_dbch *dbch, struct fwohcidb_tr *db_tr,
								int poffset)
{
	struct fwohcidb *db = db_tr->db;
	struct fw_xferq *it;
	int err = 0;

	it = &dbch->xferq;
	if(it->buf == 0){
		err = EINVAL;
		return err;
	}
	db_tr->buf = fwdma_v_addr(it->buf, poffset);
	db_tr->dbcnt = 3;

	FWOHCI_DMA_WRITE(db[0].db.desc.cmd,
		OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | 8);
	FWOHCI_DMA_WRITE(db[0].db.desc.addr, 0);
	bzero((void *)&db[1].db.immed[0], sizeof(db[1].db.immed));
	FWOHCI_DMA_WRITE(db[2].db.desc.addr,
	fwdma_bus_addr(it->buf, poffset) + sizeof(u_int32_t));

	FWOHCI_DMA_WRITE(db[2].db.desc.cmd,
		OHCI_OUTPUT_LAST | OHCI_UPDATE | OHCI_BRANCH_ALWAYS);
#if 1
	FWOHCI_DMA_WRITE(db[0].db.desc.res, 0);
	FWOHCI_DMA_WRITE(db[2].db.desc.res, 0);
#endif
	return 0;
}

int
fwohci_add_rx_buf(struct fwohci_dbch *dbch, struct fwohcidb_tr *db_tr,
		int poffset, struct fwdma_alloc *dummy_dma)
{
	struct fwohcidb *db = db_tr->db;
	struct fw_xferq *ir;
	int i, ldesc;
	bus_addr_t dbuf[2];
	int dsiz[2];

	ir = &dbch->xferq;
	if (ir->buf == NULL && (dbch->xferq.flag & FWXFERQ_EXTBUF) == 0) {
		db_tr->buf = fwdma_malloc_size(dbch->dmat, &db_tr->dma_map,
			ir->psize, &dbuf[0], BUS_DMA_NOWAIT);
		if (db_tr->buf == NULL)
			return(ENOMEM);
		db_tr->dbcnt = 1;
		dsiz[0] = ir->psize;
		bus_dmamap_sync(dbch->dmat, db_tr->dma_map,
			BUS_DMASYNC_PREREAD);
	} else {
		db_tr->dbcnt = 0;
		if (dummy_dma != NULL) {
			dsiz[db_tr->dbcnt] = sizeof(u_int32_t);
			dbuf[db_tr->dbcnt++] = dummy_dma->bus_addr;
		}
		dsiz[db_tr->dbcnt] = ir->psize;
		if (ir->buf != NULL) {
			db_tr->buf = fwdma_v_addr(ir->buf, poffset);
			dbuf[db_tr->dbcnt] = fwdma_bus_addr( ir->buf, poffset);
		}
		db_tr->dbcnt++;
	}
	for(i = 0 ; i < db_tr->dbcnt ; i++){
		FWOHCI_DMA_WRITE(db[i].db.desc.addr, dbuf[i]);
		FWOHCI_DMA_WRITE(db[i].db.desc.cmd, OHCI_INPUT_MORE | dsiz[i]);
		if (ir->flag & FWXFERQ_STREAM) {
			FWOHCI_DMA_SET(db[i].db.desc.cmd, OHCI_UPDATE);
		}
		FWOHCI_DMA_WRITE(db[i].db.desc.res, dsiz[i]);
	}
	ldesc = db_tr->dbcnt - 1;
	if (ir->flag & FWXFERQ_STREAM) {
		FWOHCI_DMA_SET(db[ldesc].db.desc.cmd, OHCI_INPUT_LAST);
	}
	FWOHCI_DMA_SET(db[ldesc].db.desc.cmd, OHCI_BRANCH_ALWAYS);
	return 0;
}


static int
fwohci_arcv_swap(struct fw_pkt *fp, int len)
{
	struct fw_pkt *fp0;
	u_int32_t ld0;
	int slen, hlen;
#if BYTE_ORDER == BIG_ENDIAN
	int i;
#endif

	ld0 = FWOHCI_DMA_READ(fp->mode.ld[0]);
#if 0
	printf("ld0: x%08x\n", ld0);
#endif
	fp0 = (struct fw_pkt *)&ld0;
	/* determine length to swap */
	switch (fp0->mode.common.tcode) {
	case FWTCODE_RREQQ:
	case FWTCODE_WRES:
	case FWTCODE_WREQQ:
	case FWTCODE_RRESQ:
	case FWOHCITCODE_PHY:
		slen = 12;
		break;
	case FWTCODE_RREQB:
	case FWTCODE_WREQB:
	case FWTCODE_LREQ:
	case FWTCODE_RRESB:
	case FWTCODE_LRES:
		slen = 16;
		break;
	default:
		printf("Unknown tcode %d\n", fp0->mode.common.tcode);
		return(0);
	}
	hlen = tinfo[fp0->mode.common.tcode].hdr_len;
	if (hlen > len) {
		if (firewire_debug)
			printf("splitted header\n");
		return(-hlen);
	}
#if BYTE_ORDER == BIG_ENDIAN
	for(i = 0; i < slen/4; i ++)
		fp->mode.ld[i] = FWOHCI_DMA_READ(fp->mode.ld[i]);
#endif
	return(hlen);
}

static int
fwohci_get_plen(struct fwohci_softc *sc, struct fwohci_dbch *dbch, struct fw_pkt *fp)
{
	struct tcode_info *info;
	int r;

	info = &tinfo[fp->mode.common.tcode];
	r = info->hdr_len + sizeof(u_int32_t);
	if ((info->flag & FWTI_BLOCK_ASY) != 0)
		r += roundup2(fp->mode.wreqb.len, sizeof(u_int32_t));

	if (r == sizeof(u_int32_t))
		/* XXX */
		device_printf(sc->fc.dev, "Unknown tcode %d\n",
						fp->mode.common.tcode);

	if (r > dbch->xferq.psize) {
		device_printf(sc->fc.dev, "Invalid packet length %d\n", r);
		/* panic ? */
	}

	return r;
}

static void
fwohci_arcv_free_buf(struct fwohci_dbch *dbch, struct fwohcidb_tr *db_tr)
{
	struct fwohcidb *db = &db_tr->db[0];

	FWOHCI_DMA_CLEAR(db->db.desc.depend, 0xf);
	FWOHCI_DMA_WRITE(db->db.desc.res, dbch->xferq.psize);
	FWOHCI_DMA_SET(dbch->bottom->db[0].db.desc.depend, 1);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_PREWRITE);
	dbch->bottom = db_tr;
}

static void
fwohci_arcv(struct fwohci_softc *sc, struct fwohci_dbch *dbch, int count)
{
	struct fwohcidb_tr *db_tr;
	struct iovec vec[2];
	struct fw_pkt pktbuf;
	int nvec;
	struct fw_pkt *fp;
	u_int8_t *ld;
	u_int32_t stat, off, status;
	u_int spd;
	int len, plen, hlen, pcnt, offset;
	int s;
	caddr_t buf;
	int resCount;

	if(&sc->arrq == dbch){
		off = OHCI_ARQOFF;
	}else if(&sc->arrs == dbch){
		off = OHCI_ARSOFF;
	}else{
		return;
	}

	s = splfw();
	db_tr = dbch->top;
	pcnt = 0;
	/* XXX we cannot handle a packet which lies in more than two buf */
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_POSTREAD);
	fwdma_sync_multiseg_all(dbch->am, BUS_DMASYNC_POSTWRITE);
	status = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res) >> OHCI_STATUS_SHIFT;
	resCount = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res) & OHCI_COUNT_MASK;
#if 0
	printf("status 0x%04x, resCount 0x%04x\n", status, resCount);
#endif
	while (status & OHCI_CNTL_DMA_ACTIVE) {
		len = dbch->xferq.psize - resCount;
		ld = (u_int8_t *)db_tr->buf;
		if (dbch->pdb_tr == NULL) {
			len -= dbch->buf_offset;
			ld += dbch->buf_offset;
		}
		if (len > 0)
			bus_dmamap_sync(dbch->dmat, db_tr->dma_map,
					BUS_DMASYNC_POSTREAD);
		while (len > 0 ) {
			if (count >= 0 && count-- == 0)
				goto out;
			if(dbch->pdb_tr != NULL){
				/* we have a fragment in previous buffer */
				int rlen;

				offset = dbch->buf_offset;
				if (offset < 0)
					offset = - offset;
				buf = dbch->pdb_tr->buf + offset;
				rlen = dbch->xferq.psize - offset;
				if (firewire_debug)
					printf("rlen=%d, offset=%d\n",
						rlen, dbch->buf_offset);
				if (dbch->buf_offset < 0) {
					/* splitted in header, pull up */
					char *p;

					p = (char *)&pktbuf;
					bcopy(buf, p, rlen);
					p += rlen;
					/* this must be too long but harmless */
					rlen = sizeof(pktbuf) - rlen;
					if (rlen < 0)
						printf("why rlen < 0\n");
					bcopy(db_tr->buf, p, rlen);
					ld += rlen;
					len -= rlen;
					hlen = fwohci_arcv_swap(&pktbuf, sizeof(pktbuf));
					if (hlen < 0) {
						printf("hlen < 0 shouldn't happen");
					}
					offset = sizeof(pktbuf);
					vec[0].iov_base = (char *)&pktbuf;
					vec[0].iov_len = offset;
				} else {
					/* splitted in payload */
					offset = rlen;
					vec[0].iov_base = buf;
					vec[0].iov_len = rlen;
				}
				fp=(struct fw_pkt *)vec[0].iov_base;
				nvec = 1;
			} else {
				/* no fragment in previous buffer */
				fp=(struct fw_pkt *)ld;
				hlen = fwohci_arcv_swap(fp, len);
				if (hlen == 0)
					/* XXX need reset */
					goto out;
				if (hlen < 0) {
					dbch->pdb_tr = db_tr;
					dbch->buf_offset = - dbch->buf_offset;
					/* sanity check */
					if (resCount != 0) 
						printf("resCount = %d !?\n",
						    resCount);
					/* XXX clear pdb_tr */
					goto out;
				}
				offset = 0;
				nvec = 0;
			}
			plen = fwohci_get_plen(sc, dbch, fp) - offset;
			if (plen < 0) {
				/* minimum header size + trailer
				= sizeof(fw_pkt) so this shouldn't happens */
				printf("plen(%d) is negative! offset=%d\n",
				    plen, offset);
				/* XXX clear pdb_tr */
				goto out;
			}
			if (plen > 0) {
				len -= plen;
				if (len < 0) {
					dbch->pdb_tr = db_tr;
					if (firewire_debug)
						printf("splitted payload\n");
					/* sanity check */
					if (resCount != 0) 
						printf("resCount = %d !?\n",
						    resCount);
					/* XXX clear pdb_tr */
					goto out;
				}
				vec[nvec].iov_base = ld;
				vec[nvec].iov_len = plen;
				nvec ++;
				ld += plen;
			}
			dbch->buf_offset = ld - (u_int8_t *)db_tr->buf;
			if (nvec == 0)
				printf("nvec == 0\n");

/* DMA result-code will be written at the tail of packet */
#if BYTE_ORDER == BIG_ENDIAN
			stat = FWOHCI_DMA_READ(((struct fwohci_trailer *)(ld - sizeof(struct fwohci_trailer)))->stat) >> 16;
#else
			stat = ((struct fwohci_trailer *)(ld - sizeof(struct fwohci_trailer)))->stat;
#endif
#if 0
			printf("plen: %d, stat %x\n",
			    plen ,stat);
#endif
			spd = (stat >> 5) & 0x3;
			stat &= 0x1f;
			switch(stat){
			case FWOHCIEV_ACKPEND:
#if 0
				printf("fwohci_arcv: ack pending tcode=0x%x..\n", fp->mode.common.tcode);
#endif
				/* fall through */
			case FWOHCIEV_ACKCOMPL:
			{
				struct fw_rcv_buf rb;

				if ((vec[nvec-1].iov_len -=
					sizeof(struct fwohci_trailer)) == 0)
					nvec--; 
				rb.fc = &sc->fc;
				rb.vec = vec;
				rb.nvec = nvec;
				rb.spd = spd;
				fw_rcv(&rb);
				break;
			}
			case FWOHCIEV_BUSRST:
				if (sc->fc.status != FWBUSRESET) 
					printf("got BUSRST packet!?\n");
				break;
			default:
				device_printf(sc->fc.dev, "Async DMA Receive error err = %02x %s\n", stat, fwohcicode[stat]);
#if 0 /* XXX */
				goto out;
#endif
				break;
			}
			pcnt ++;
			if (dbch->pdb_tr != NULL) {
				fwohci_arcv_free_buf(dbch, dbch->pdb_tr);
				dbch->pdb_tr = NULL;
			}

		}
out:
		if (resCount == 0) {
			/* done on this buffer */
			if (dbch->pdb_tr == NULL) {
				fwohci_arcv_free_buf(dbch, db_tr);
				dbch->buf_offset = 0;
			} else
				if (dbch->pdb_tr != db_tr)
					printf("pdb_tr != db_tr\n");
			db_tr = STAILQ_NEXT(db_tr, link);
			status = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res)
						>> OHCI_STATUS_SHIFT;
			resCount = FWOHCI_DMA_READ(db_tr->db[0].db.desc.res)
						& OHCI_COUNT_MASK;
			/* XXX check buffer overrun */
			dbch->top = db_tr;
		} else {
			dbch->buf_offset = dbch->xferq.psize - resCount;
			break;
		}
		/* XXX make sure DMA is not dead */
	}
#if 0
	if (pcnt < 1)
		printf("fwohci_arcv: no packets\n");
#endif
	splx(s);
}
