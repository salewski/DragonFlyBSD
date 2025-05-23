/*	$FreeBSD: src/sys/dev/stg/tmc18c30.c,v 1.1.2.5 2001/12/17 13:30:19 non Exp $	*/
/*	$DragonFly: src/sys/dev/disk/stg/tmc18c30.c,v 1.7 2004/02/12 00:00:18 dillon Exp $	*/
/*	$NecBSD: tmc18c30.c,v 1.28.12.3 2001/06/19 04:35:48 honda Exp $	*/
/*	$NetBSD$	*/

#define	STG_DEBUG
#define	STG_STATICS
#define	STG_IO_CONTROL_FLAGS	(STG_FIFO_INTERRUPTS | STG_WAIT_FOR_SELECT)

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998, 1999, 2000, 2001
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998, 1999
 *	Kouichi Matsuda. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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
 */
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#ifdef __NetBSD__
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <i386/Cbus/dev/scsi_low.h>
#include <i386/Cbus/dev/tmc18c30reg.h>
#include <i386/Cbus/dev/tmc18c30var.h>
#endif /* __NetBSD__ */

#ifdef __DragonFly__
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <bus/cam/scsi/scsi_low.h>
#include "tmc18c30reg.h"
#include "tmc18c30var.h"
#endif /* __DragonFly__ */

/***************************************************
 * USER SETTINGS
 ***************************************************/
/* DEVICE CONFIGURATION FLAGS (MINOR)
 *
 * 0x01   DISCONECT OFF
 * 0x02   PARITY LINE OFF
 * 0x04   IDENTIFY MSG OFF ( = single lun)
 * 0x08   SYNC TRANSFER OFF
 */
/* #define	STG_SYNC_SUPPORT */	/* NOT YET but easy */

/* For the 512 fifo type: change below */
#define	TMC18C30_FIFOSZ	0x800
#define	TMC18C30_FCBSZ	0x200
#define	TMC18C50_FIFOSZ	0x2000
#define	TMC18C50_FCBSZ	0x400

#define	STG_MAX_DATA_SIZE	(64 * 1024)
#define	STG_DELAY_MAX			(2 * 1000 * 1000)
#define	STG_DELAY_INTERVAL		(1)
#define	STG_DELAY_SELECT_POLLING_MAX	(5 * 1000 * 1000)

/***************************************************
 * PARAMS
 ***************************************************/
#define	STG_NTARGETS	8
#define	STG_NLUNS	8

/***************************************************
 * DEBUG
 ***************************************************/
#ifdef	STG_DEBUG
int stg_debug;
#endif	/* STG_DEBUG */

#ifdef	STG_STATICS
struct stg_statics {
	int arbit_fail_0;
	int arbit_fail_1;
	int disconnect;
	int reselect;
} stg_statics;
#endif	/* STG_STATICS */

/***************************************************
 * IO control flags
 ***************************************************/
#define	STG_FIFO_INTERRUPTS	0x0001
#define	STG_WAIT_FOR_SELECT	0x0100

int stg_io_control = STG_IO_CONTROL_FLAGS;

/***************************************************
 * DEVICE STRUCTURE
 ***************************************************/
extern struct cfdriver stg_cd;

/**************************************************************
 * DECLARE
 **************************************************************/
/* static */
static void stg_pio_read (struct stg_softc *, struct targ_info *, u_int);
static void stg_pio_write (struct stg_softc *, struct targ_info *, u_int);
static int stg_xfer (struct stg_softc *, u_int8_t *, int, int, int);
static int stg_msg (struct stg_softc *, struct targ_info *, u_int);
static int stg_reselected (struct stg_softc *);
static int stg_disconnected (struct stg_softc *, struct targ_info *);
static __inline void stg_pdma_end (struct stg_softc *, struct targ_info *);
static int stghw_select_targ_wait (struct stg_softc *, int);
static int stghw_check (struct stg_softc *);
static void stghw_init (struct stg_softc *);
static int stg_negate_signal (struct stg_softc *, u_int8_t, u_char *);
static int stg_expect_signal (struct stg_softc *, u_int8_t, u_int8_t);
static int stg_world_start (struct stg_softc *, int);
static int stghw_start_selection (struct stg_softc *sc, struct slccb *);
static void stghw_bus_reset (struct stg_softc *);
static void stghw_attention (struct stg_softc *);
static int stg_target_nexus_establish (struct stg_softc *);
static int stg_lun_nexus_establish (struct stg_softc *);
static int stg_ccb_nexus_establish (struct stg_softc *);
static int stg_targ_init (struct stg_softc *, struct targ_info *, int);
static __inline void stghw_bcr_write_1 (struct stg_softc *, u_int8_t);
static int stg_timeout (struct stg_softc *);
static void stg_selection_done_and_expect_msgout (struct stg_softc *);

struct scsi_low_funcs stgfuncs = {
	SC_LOW_INIT_T stg_world_start,
	SC_LOW_BUSRST_T stghw_bus_reset,
	SC_LOW_TARG_INIT_T stg_targ_init,
	SC_LOW_LUN_INIT_T NULL,

	SC_LOW_SELECT_T stghw_start_selection,
	SC_LOW_NEXUS_T stg_lun_nexus_establish,
	SC_LOW_NEXUS_T stg_ccb_nexus_establish,

	SC_LOW_ATTEN_T stghw_attention,
	SC_LOW_MSG_T stg_msg,

	SC_LOW_TIMEOUT_T stg_timeout,
	SC_LOW_POLL_T stgintr,

	NULL,
};

/****************************************************
 * hwfuncs
 ****************************************************/
static __inline void 
stghw_bcr_write_1(sc, bcv)
	struct stg_softc *sc;
	u_int8_t bcv;
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, tmc_bctl, bcv);
	sc->sc_busimg = bcv;
}

static int
stghw_check(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int fcbsize, fcb;
	u_int16_t lsb, msb;

	lsb = bus_space_read_1(iot, ioh, tmc_idlsb);
	msb = bus_space_read_1(iot, ioh, tmc_idmsb);
	switch (msb << 8 | lsb)
	{
		case 0x6127:
			/* TMCCHIP_1800 not supported. (it's my policy) */
			sc->sc_chip = TMCCHIP_1800;
			return EINVAL;

		case 0x60e9:
			if (bus_space_read_1(iot, ioh, tmc_cfg2) & 0x02)
			{
				sc->sc_chip = TMCCHIP_18C30;
				sc->sc_fsz = TMC18C30_FIFOSZ;
				fcbsize = TMC18C30_FCBSZ;
			}
			else
			{
				sc->sc_chip = TMCCHIP_18C50;
				sc->sc_fsz = TMC18C50_FIFOSZ;
				fcbsize = TMC18C50_FCBSZ;
			}
			break;

		default:
			sc->sc_chip = TMCCHIP_UNK;
			return ENODEV;
	}

	sc->sc_fcRinit = FCTL_INTEN;
	sc->sc_fcWinit = FCTL_PARENB | FCTL_INTEN;

	if (slp->sl_cfgflags & CFG_NOATTEN)
		sc->sc_imsg = 0;
	else
		sc->sc_imsg = BCTL_ATN;
	sc->sc_busc = BCTL_BUSEN;

	sc->sc_wthold = fcbsize + 256;
	sc->sc_rthold = fcbsize - 256;
	sc->sc_maxwsize = sc->sc_fsz;

	fcb = fcbsize / (sc->sc_fsz / 16);
	sc->sc_icinit = ICTL_CD | ICTL_SEL | ICTL_ARBIT | fcb;
	return 0;
}

static void
stghw_init(sc)
	struct stg_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, tmc_ictl, 0);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
	bus_space_write_1(iot, ioh, tmc_fctl,
			  sc->sc_fcRinit | FCTL_CLRFIFO | FCTL_CLRINT);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	bus_space_write_1(iot, ioh, tmc_ictl, sc->sc_icinit);

	bus_space_write_1(iot, ioh, tmc_ssctl, 0);
}

static int
stg_targ_init(sc, ti, action)
	struct stg_softc *sc;
	struct targ_info *ti;
	int action;
{
	struct stg_targ_info *sti = (void *) ti;

	if (action == SCSI_LOW_INFO_ALLOC || action == SCSI_LOW_INFO_REVOKE)
	{
		ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
		ti->ti_maxsynch.period = 0;
		ti->ti_maxsynch.offset = 0;
		sti->sti_reg_synch = 0;
	}
	return 0;
}	

/****************************************************
 * scsi low interface
 ****************************************************/
static void
stghw_attention(sc)
	struct stg_softc *sc;
{

	sc->sc_busc |= BCTL_ATN;
	sc->sc_busimg |= BCTL_ATN;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, tmc_bctl, sc->sc_busimg);
	SCSI_LOW_DELAY(10);
}

static void
stghw_bus_reset(sc)
	struct stg_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, tmc_ictl, 0);
	bus_space_write_1(iot, ioh, tmc_fctl, 0);
	stghw_bcr_write_1(sc, BCTL_RST);
	SCSI_LOW_DELAY(100000);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
}

static int
stghw_start_selection(sc, cb)
	struct stg_softc *sc;
	struct slccb *cb;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti = cb->ti;
	u_int8_t stat;
	int s;

	sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	sc->sc_dataout_timeout = 0;
	sc->sc_ubf_timeout = 0;
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
	bus_space_write_1(iot, ioh, tmc_ictl, sc->sc_icinit);

	s = splhigh();
	stat = bus_space_read_1(iot, ioh, tmc_astat);
	if ((stat & ASTAT_INT) != 0)
	{
		splx(s);
		return SCSI_LOW_START_FAIL;
	}

	bus_space_write_1(iot, ioh, tmc_scsiid, sc->sc_idbit);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_ARBIT);
	splx(s);

	SCSI_LOW_SETUP_PHASE(ti, PH_ARBSTART);
	return SCSI_LOW_START_OK;
}

static int
stg_world_start(sc, fdone)
	struct stg_softc *sc;
	int fdone;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	int error;

	if ((slp->sl_cfgflags & CFG_NOPARITY) == 0)
		sc->sc_fcRinit |= FCTL_PARENB;
	else
		sc->sc_fcRinit &= ~FCTL_PARENB;

	if ((error = stghw_check(sc)) != 0)
		return error;

	stghw_init(sc);
	scsi_low_bus_reset(slp);
	stghw_init(sc);

	SOFT_INTR_REQUIRED(slp);
	return 0;
}

static int
stg_msg(sc, ti, msg)
	struct stg_softc *sc;
	struct targ_info *ti;
	u_int msg;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct stg_targ_info *sti = (void *) ti;
	u_int period, offset;

	if ((msg & SCSI_LOW_MSG_WIDE) != 0)
	{
		if (ti->ti_width != SCSI_LOW_BUS_WIDTH_8)
		{
			ti->ti_width = SCSI_LOW_BUS_WIDTH_8;
			return EINVAL;
		}
		return 0;
	}

	if ((msg & SCSI_LOW_MSG_SYNCH) == 0)
		return 0;

 	period = ti->ti_maxsynch.period;
	offset = ti->ti_maxsynch.offset;
	period = period << 2;
	if (period >= 200)
	{
		sti->sti_reg_synch = (period - 200) / 50;
		if (period % 50)
			sti->sti_reg_synch ++;
		sti->sti_reg_synch |= SSCTL_SYNCHEN;
	}
	else if (period >= 100)
	{
		sti->sti_reg_synch = (period - 100) / 50;
		if (period % 50)
			sti->sti_reg_synch ++;
		sti->sti_reg_synch |= SSCTL_SYNCHEN | SSCTL_FSYNCHEN;
	}
	bus_space_write_1(iot, ioh, tmc_ssctl, sti->sti_reg_synch);
	return 0;
}

/**************************************************************
 * General probe attach
 **************************************************************/
int
stgprobesubr(iot, ioh, dvcfg)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int dvcfg;
{
	u_int16_t lsb, msb;

	lsb = bus_space_read_1(iot, ioh, tmc_idlsb);
	msb = bus_space_read_1(iot, ioh, tmc_idmsb);
	switch (msb << 8 | lsb)
	{
		default:
			return 0;
		case 0x6127:
			/* not support! */
			return 0;
		case 0x60e9:
			return 1;
	}
	return 0;
}

int
stgprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

void
stgattachsubr(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;

	printf("\n");

	sc->sc_idbit = (1 << slp->sl_hostid); 
	slp->sl_funcs = &stgfuncs;
	sc->sc_tmaxcnt = SCSI_LOW_MIN_TOUT * 1000 * 1000; /* default */

	slp->sl_flags |= HW_READ_PADDING;
	slp->sl_cfgflags |= CFG_ASYNC;	/* XXX */

	(void) scsi_low_attach(slp, 0, STG_NTARGETS, STG_NLUNS,
				sizeof(struct stg_targ_info), 0);
}

/**************************************************************
 * PDMA functions
 **************************************************************/
static __inline void
stg_pdma_end(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct slccb *cb = slp->sl_Qnexus;
	u_int len, tres;

	slp->sl_flags &= ~HW_PDMASTART;
	sc->sc_icinit &= ~ICTL_FIFO;
	sc->sc_dataout_timeout = 0;

	if (cb == NULL)
	{
		slp->sl_error |= PDMAERR;
		goto out;
	}

	if (ti->ti_phase == PH_DATA)
	{
		len = bus_space_read_2(iot, ioh, tmc_fdcnt);
		if (slp->sl_scp.scp_direction == SCSI_LOW_WRITE)
		{
			if (len != 0)
			{
				tres = len + slp->sl_scp.scp_datalen;
				if (tres <= (u_int) cb->ccb_scp.scp_datalen)
				{
					slp->sl_scp.scp_data -= len;
					slp->sl_scp.scp_datalen = tres;
				}
				else
				{
					slp->sl_error |= PDMAERR;
					printf("%s len %x >= datalen %x\n",
						slp->sl_xname,
						len, slp->sl_scp.scp_datalen);
				}
			}
		}
		else if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
		{
			if (len != 0)
			{
				slp->sl_error |= PDMAERR;
				printf("%s: len %x left in fifo\n",
					slp->sl_xname, len);
			}
		}
		scsi_low_data_finish(slp);
	}
	else
	{

		printf("%s data phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}

out:
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
}

static void
stg_pio_read(sc, ti, thold)
	struct stg_softc *sc;
	struct targ_info *ti;
	u_int thold;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct sc_p *sp = &slp->sl_scp;
	int s, tout;
	u_int res;
	u_int8_t stat;

	if ((slp->sl_flags & HW_PDMASTART) == 0)
	{
		bus_space_write_1(iot, ioh, tmc_fctl,
				  sc->sc_fcRinit | FCTL_FIFOEN);
		slp->sl_flags |= HW_PDMASTART;
	}

	tout = sc->sc_tmaxcnt;
	while (tout -- > 0)
	{
		if (thold > 0)
		{
			s = splhigh();
			res = bus_space_read_2(iot, ioh, tmc_fdcnt);
			if (res < thold)
			{
				bus_space_write_1(iot, ioh, tmc_ictl,
						  sc->sc_icinit);
				splx(s);
				break;
			}
			splx(s);
		}
		else
		{
			stat = bus_space_read_1(iot, ioh, tmc_bstat);
			res = bus_space_read_2(iot, ioh, tmc_fdcnt);
			if (res == 0)
			{
				if ((stat & PHASE_MASK) != DATA_IN_PHASE)
					break;
				if (sp->scp_datalen <= 0)
					break;
				SCSI_LOW_DELAY(1);
				continue;
			}
		}

		/* The assumtion res != 0 is valid here */
		if (res > sp->scp_datalen)
		{
			if (res == (u_int) -1)
				break;

			slp->sl_error |= PDMAERR;
			if ((slp->sl_flags & HW_READ_PADDING) == 0)
			{
				printf("%s: read padding required\n",
					slp->sl_xname);
				break;
			}

			sp->scp_datalen = 0;
			if (res > STG_MAX_DATA_SIZE)
				res = STG_MAX_DATA_SIZE;
			while (res -- > 0)
			{
				(void) bus_space_read_1(iot, ioh, tmc_rfifo);
			}
			continue;
		}

		sp->scp_datalen -= res;
		if (res & 1)
		{
			*sp->scp_data = bus_space_read_1(iot, ioh, tmc_rfifo);
			sp->scp_data ++;
			res --;
		}

		bus_space_read_multi_2(iot, ioh, tmc_rfifo,
				       (u_int16_t *) sp->scp_data, res >> 1);
		sp->scp_data += res;
	}

	if (tout <= 0)
		printf("%s: pio read timeout\n", slp->sl_xname);
}

static void
stg_pio_write(sc, ti, thold)
	struct stg_softc *sc;
	struct targ_info *ti;
	u_int thold;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct sc_p *sp = &slp->sl_scp;
	u_int res;
	int s, tout;
	u_int8_t stat;

	if ((slp->sl_flags & HW_PDMASTART) == 0)
	{
		stat = sc->sc_fcWinit | FCTL_FIFOEN | FCTL_FIFOW;
		bus_space_write_1(iot, ioh, tmc_fctl, stat | FCTL_CLRFIFO);
		bus_space_write_1(iot, ioh, tmc_fctl, stat);
		slp->sl_flags |= HW_PDMASTART;
	}

	tout = sc->sc_tmaxcnt;
	while (tout -- > 0)
	{
		stat = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((stat & PHASE_MASK) != DATA_OUT_PHASE)
			break;

		if (sp->scp_datalen <= 0)
		{
			if (sc->sc_dataout_timeout == 0)
				sc->sc_dataout_timeout = SCSI_LOW_TIMEOUT_HZ;
			break;
		}

		if (thold > 0)
		{
			s = splhigh();
			res = bus_space_read_2(iot, ioh, tmc_fdcnt);
			if (res > thold)
			{
				bus_space_write_1(iot, ioh, tmc_ictl,
						  sc->sc_icinit);
				splx(s);
				break;
			}
			splx(s);
		}
		else
		{
			res = bus_space_read_2(iot, ioh, tmc_fdcnt);
			if (res > sc->sc_maxwsize / 2)
			{
				SCSI_LOW_DELAY(1);
				continue;
			}
		}
			
		if (res == (u_int) -1)
			break;
		res = sc->sc_maxwsize - res;
		if (res > sp->scp_datalen)
			res = sp->scp_datalen;

		sp->scp_datalen -= res;
		if ((res & 0x1) != 0)
		{
			bus_space_write_1(iot, ioh, tmc_wfifo, *sp->scp_data);
			sp->scp_data ++;
			res --;
		}

		bus_space_write_multi_2(iot, ioh, tmc_wfifo, 
					(u_int16_t *) sp->scp_data, res >> 1);
		sp->scp_data += res;
	}

	if (tout <= 0)
		printf("%s: pio write timeout\n", slp->sl_xname);
}

static int
stg_negate_signal(sc, mask, s)
	struct stg_softc *sc;
	u_int8_t mask;
	u_char *s;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int wc;
	u_int8_t regv;

	for (wc = 0; wc < STG_DELAY_MAX / STG_DELAY_INTERVAL; wc ++)
	{
		regv = bus_space_read_1(bst, bsh, tmc_bstat);
		if (regv == (u_int8_t) -1)
			return -1;
		if ((regv & mask) == 0)
			return 1;

		SCSI_LOW_DELAY(STG_DELAY_INTERVAL);
	}

	printf("%s: %s stg_negate_signal timeout\n", slp->sl_xname, s);
	return -1;
}

static int
stg_expect_signal(sc, phase, mask)
	struct stg_softc *sc;
	u_int8_t phase, mask;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t bst = sc->sc_iot;
	bus_space_handle_t bsh = sc->sc_ioh;
	int wc;
	u_int8_t ph;

	phase &= PHASE_MASK;
	for (wc = 0; wc < STG_DELAY_MAX / STG_DELAY_INTERVAL; wc ++)
	{
		ph = bus_space_read_1(bst, bsh, tmc_bstat);
		if (ph == (u_int8_t) -1)
			return -1;
		if ((ph & PHASE_MASK) != phase)
			return 0;
		if ((ph & mask) != 0)
			return 1;

		SCSI_LOW_DELAY(STG_DELAY_INTERVAL);
	}

	printf("%s: stg_expect_signal timeout\n", slp->sl_xname);
	return -1;
}

static int
stg_xfer(sc, buf, len, phase, clear_atn)
	struct stg_softc *sc;
	u_int8_t *buf;
	int len;
	int phase;
	int clear_atn;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int rv, ptr;

	if (phase & BSTAT_IO)
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	else
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit);

	for (ptr = 0; len > 0; len --)
	{
		rv = stg_expect_signal(sc, phase, BSTAT_REQ);
		if (rv <= 0)
			goto bad;

		if (len == 1 && clear_atn != 0)
		{
			sc->sc_busc &= ~BCTL_ATN;
			stghw_bcr_write_1(sc, sc->sc_busc);
			SCSI_LOW_DEASSERT_ATN(&sc->sc_sclow);
		}

		if (phase & BSTAT_IO)
		{
			buf[ptr ++] = bus_space_read_1(iot, ioh, tmc_rdata);
		}
		else
		{
			bus_space_write_1(iot, ioh, tmc_wdata, buf[ptr ++]);
		}

		stg_negate_signal(sc, BSTAT_ACK, "xfer<ACK>");
	}

bad:
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	return len;
}

/**************************************************************
 * disconnect & reselect (HW low)
 **************************************************************/
static int
stg_reselected(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int tout;
	u_int sid;
	u_int8_t regv;

	if (slp->sl_selid != NULL)
	{
		/* XXX:
		 * Selection vs Reselection conflicts.
		 */
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		stghw_bcr_write_1(sc, BCTL_BUSFREE);
	}
	else if (slp->sl_Tnexus != NULL)
	{
		printf("%s: unexpected termination\n", slp->sl_xname);
		stg_disconnected(sc, slp->sl_Tnexus);
	}

	/* XXX:
	 * We should ack the reselection as soon as possible,
	 * because the target would abort the current reselection seq 
  	 * due to reselection timeout.
	 */
	tout = STG_DELAY_SELECT_POLLING_MAX;
	while (tout -- > 0)
	{
		regv = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((regv & (BSTAT_IO | BSTAT_SEL | BSTAT_BSY)) == 
			    (BSTAT_IO | BSTAT_SEL))
		{
			SCSI_LOW_DELAY(1);
			regv = bus_space_read_1(iot, ioh, tmc_bstat);
			if ((regv & (BSTAT_IO | BSTAT_SEL | BSTAT_BSY)) == 
				    (BSTAT_IO | BSTAT_SEL))
				goto reselect_start;
		}
		SCSI_LOW_DELAY(1);
	}
	printf("%s: reselction timeout I\n", slp->sl_xname);
	return EJUSTRETURN;
	
reselect_start:
	sid = (u_int) bus_space_read_1(iot, ioh, tmc_scsiid);
	if ((sid & sc->sc_idbit) == 0)
	{
		/* not us */
		return EJUSTRETURN;
	}

	bus_space_write_1(iot, ioh, tmc_fctl, 
			    sc->sc_fcRinit | FCTL_CLRFIFO | FCTL_CLRINT);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	stghw_bcr_write_1(sc, sc->sc_busc | BCTL_BSY);

	while (tout -- > 0)
	{
		regv = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((regv & (BSTAT_SEL | BSTAT_BSY)) == BSTAT_BSY)
			goto reselected;
		SCSI_LOW_DELAY(1);
	}
	printf("%s: reselction timeout II\n", slp->sl_xname);
	return EJUSTRETURN;

reselected:
	sid &= ~sc->sc_idbit;
	sid = ffs(sid) - 1;
	if (scsi_low_reselected(slp, sid) == NULL)
		return EJUSTRETURN;

#ifdef	STG_STATICS
	stg_statics.reselect ++;
#endif	/* STG_STATICS */
	return EJUSTRETURN;
}

static int
stg_disconnected(sc, ti)
	struct stg_softc *sc;
	struct targ_info *ti;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* clear bus status & fifo */
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	stghw_bcr_write_1(sc, BCTL_BUSFREE);
	sc->sc_icinit &= ~ICTL_FIFO;
	sc->sc_busc &= ~BCTL_ATN;
	sc->sc_dataout_timeout = 0;
	sc->sc_ubf_timeout = 0;

#ifdef	STG_STATICS
	stg_statics.disconnect ++;
#endif	/* STG_STATICS */
	scsi_low_disconnected(slp, ti);
	return 1;
}

/**************************************************************
 * SEQUENCER
 **************************************************************/
static int
stg_target_nexus_establish(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti = slp->sl_Tnexus;
	struct stg_targ_info *sti = (void *) ti;

	bus_space_write_1(iot, ioh, tmc_ssctl, sti->sti_reg_synch);
	if ((stg_io_control & STG_FIFO_INTERRUPTS) != 0)
	{
		sc->sc_icinit |= ICTL_FIFO;
	}
	return 0;
}

static int
stg_lun_nexus_establish(sc)
	struct stg_softc *sc;
{

	return 0;
}

static int
stg_ccb_nexus_establish(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	struct slccb *cb = slp->sl_Qnexus;

	sc->sc_tmaxcnt = cb->ccb_tcmax * 1000 * 1000;
	return 0;
}

#define	STGHW_SELECT_INTERVAL	10

static int
stghw_select_targ_wait(sc, mu)
	struct stg_softc *sc;
	int mu;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	mu = mu / STGHW_SELECT_INTERVAL;
	while (mu -- > 0)
	{
		if ((bus_space_read_1(iot, ioh, tmc_bstat) & BSTAT_BSY) == 0)
		{
			SCSI_LOW_DELAY(STGHW_SELECT_INTERVAL);
			continue;
		}
		SCSI_LOW_DELAY(1);
		if ((bus_space_read_1(iot, ioh, tmc_bstat) & BSTAT_BSY) != 0)
		{
			return 0;
		}
	}
	return ENXIO;
}

static void
stg_selection_done_and_expect_msgout(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit | FCTL_CLRFIFO);
	bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
	stghw_bcr_write_1(sc, sc->sc_imsg | sc->sc_busc);
	SCSI_LOW_ASSERT_ATN(slp);
}

int
stgintr(arg)
	void *arg;
{
	struct stg_softc *sc = arg;
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct targ_info *ti;
	struct physio_proc *pp;
	struct buf *bp;
	u_int derror, flags;
	int len, s;
	u_int8_t status, astatus, regv;

	/*******************************************
	 * interrupt check
	 *******************************************/
	if (slp->sl_flags & HW_INACTIVE)
		return 0;

	astatus = bus_space_read_1(iot, ioh, tmc_astat);
	status = bus_space_read_1(iot, ioh, tmc_bstat);

	if ((astatus & ASTAT_STATMASK) == 0 || astatus == (u_int8_t) -1)
		return 0;

	bus_space_write_1(iot, ioh, tmc_ictl, 0);
	if (astatus & ASTAT_SCSIRST)
	{
		bus_space_write_1(iot, ioh, tmc_fctl,
				  sc->sc_fcRinit | FCTL_CLRFIFO);
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		bus_space_write_1(iot, ioh, tmc_ictl, 0);

		scsi_low_restart(slp, SCSI_LOW_RESTART_SOFT, 
				 "bus reset (power off?)");
		return 1;
	}

	/*******************************************
	 * debug section
	 *******************************************/
#ifdef	STG_DEBUG
	if (stg_debug)
	{
		scsi_low_print(slp, NULL);
		printf("%s: st %x ist %x\n\n", slp->sl_xname,
		       status, astatus);
#ifdef	DDB
		if (stg_debug > 1)
			SCSI_LOW_DEBUGGER("stg");
#endif	/* DDB */
	}
#endif	/* STG_DEBUG */

	/*******************************************
	 * reselection & nexus
	 *******************************************/
	if ((status & RESEL_PHASE_MASK)== PHASE_RESELECTED)
	{
		if (stg_reselected(sc) == EJUSTRETURN)
			goto out;
	}

	if ((ti = slp->sl_Tnexus) == NULL)
		return 0;

	derror = 0;
	if ((astatus & ASTAT_PARERR) != 0 && ti->ti_phase != PH_ARBSTART &&
	    (sc->sc_fcRinit & FCTL_PARENB) != 0)
	{
		slp->sl_error |= PARITYERR;
		derror = SCSI_LOW_DATA_PE;
		if ((status & PHASE_MASK) == MESSAGE_IN_PHASE)
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_PARITY, 0);
		else
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ERROR, 1);
	}

	/*******************************************
	 * aribitration & selection
	 *******************************************/
	switch (ti->ti_phase)
	{
	case PH_ARBSTART:
		if ((astatus & ASTAT_ARBIT) == 0)
		{
#ifdef	STG_STATICS
			stg_statics.arbit_fail_0 ++;
#endif	/* STG_STATICS */
			goto arb_fail;
		}

		status = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((status & BSTAT_IO) != 0)
		{
			/* XXX:
			 * Selection vs Reselection conflicts.
			 */
#ifdef	STG_STATICS
			stg_statics.arbit_fail_1 ++;
#endif	/* STG_STATICS */
arb_fail:
			bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
			stghw_bcr_write_1(sc, BCTL_BUSFREE);
			scsi_low_arbit_fail(slp, slp->sl_Qnexus);
			goto out;
		}

		/*
		 * selection assert start.
		 */
		SCSI_LOW_SETUP_PHASE(ti, PH_SELSTART);
		scsi_low_arbit_win(slp);

		s = splhigh();
		bus_space_write_1(iot, ioh, tmc_scsiid,
				  sc->sc_idbit | (1 << ti->ti_id));
		stghw_bcr_write_1(sc, sc->sc_imsg | sc->sc_busc | BCTL_SEL);
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcWinit);
		if ((stg_io_control & STG_WAIT_FOR_SELECT) != 0)
		{
			/* selection abort delay 200 + 100 micro sec */
			if (stghw_select_targ_wait(sc, 300) == 0)
			{
				SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
				stg_selection_done_and_expect_msgout(sc);
			}	
		}
		splx(s);
		goto out;

	case PH_SELSTART:
		if ((status & BSTAT_BSY) == 0)
		{
			/* selection timeout delay 250 ms */
			if (stghw_select_targ_wait(sc, 250 * 1000) != 0)
			{
				stg_disconnected(sc, ti);
				goto out;
			}
		}

		SCSI_LOW_SETUP_PHASE(ti, PH_SELECTED);
		stg_selection_done_and_expect_msgout(sc);
		goto out;

	case PH_SELECTED:
		if ((status & BSTAT_REQ) == 0)
			goto out;
		stg_target_nexus_establish(sc);
		break;

	case PH_RESEL:
		if ((status & BSTAT_REQ) == 0)
			goto out;

		/* clear a busy line */
		bus_space_write_1(iot, ioh, tmc_fctl, sc->sc_fcRinit);
		stghw_bcr_write_1(sc, sc->sc_busc);
		stg_target_nexus_establish(sc);
		if ((status & PHASE_MASK) != MESSAGE_IN_PHASE)
		{
			printf("%s: unexpected phase after reselect\n",
			       slp->sl_xname);
			slp->sl_error |= FATALIO;
			scsi_low_assert_msg(slp, ti, SCSI_LOW_MSG_ABORT, 1);
			goto out;
		}
		break;
	}

	/*******************************************
	 * data phase
	 *******************************************/
	if ((slp->sl_flags & HW_PDMASTART) && STG_IS_PHASE_DATA(status) == 0)
	{
		if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
			stg_pio_read(sc, ti, 0);

		stg_pdma_end(sc, ti);
	}

	/*******************************************
	 * scsi seq
	 *******************************************/
	switch (status & PHASE_MASK)
	{
	case COMMAND_PHASE:
		if (stg_expect_signal(sc, COMMAND_PHASE, BSTAT_REQ) <= 0)
			break;

		SCSI_LOW_SETUP_PHASE(ti, PH_CMD);
		if (scsi_low_cmd(slp, ti) != 0)
		{
			scsi_low_attention(slp);
		}

		if (stg_xfer(sc, slp->sl_scp.scp_cmd, slp->sl_scp.scp_cmdlen,
			     COMMAND_PHASE, 0) != 0)
		{
			printf("%s: CMDOUT short\n", slp->sl_xname);
		}
		break;

	case DATA_OUT_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_WRITE) != 0)
		{
			scsi_low_attention(slp);
		}

		pp = physio_proc_enter(bp);
		if ((sc->sc_icinit & ICTL_FIFO) != 0)
			stg_pio_write(sc, ti, sc->sc_wthold);
		else
			stg_pio_write(sc, ti, 0);
		physio_proc_leave(pp);
		break;

	case DATA_IN_PHASE:
		SCSI_LOW_SETUP_PHASE(ti, PH_DATA);
		if (scsi_low_data(slp, ti, &bp, SCSI_LOW_READ) != 0)
		{
			scsi_low_attention(slp);
		}

		pp = physio_proc_enter(bp);
		if ((sc->sc_icinit & ICTL_FIFO) != 0)
			stg_pio_read(sc, ti, sc->sc_rthold);
		else
			stg_pio_read(sc, ti, 0);
		physio_proc_leave(pp);
		break;

	case STATUS_PHASE:
		regv = stg_expect_signal(sc, STATUS_PHASE, BSTAT_REQ);
		if (regv <= 0)
			break;

		SCSI_LOW_SETUP_PHASE(ti, PH_STAT);
		regv = bus_space_read_1(iot, ioh, tmc_sdna);
		if (scsi_low_statusin(slp, ti, regv | derror) != 0)
		{
			scsi_low_attention(slp);
		}
		if (regv != bus_space_read_1(iot, ioh, tmc_rdata))
		{
			printf("%s: STATIN: data mismatch\n", slp->sl_xname);
		}
		stg_negate_signal(sc, BSTAT_ACK, "statin<ACK>");
		break;

	case MESSAGE_OUT_PHASE:
		if (stg_expect_signal(sc, MESSAGE_OUT_PHASE, BSTAT_REQ) <= 0)
			break;

		SCSI_LOW_SETUP_PHASE(ti, PH_MSGOUT);
		flags = (ti->ti_ophase != ti->ti_phase) ? 
				SCSI_LOW_MSGOUT_INIT : 0;
		len = scsi_low_msgout(slp, ti, flags);

		if (len > 1 && slp->sl_atten == 0)
		{
			scsi_low_attention(slp);
		}

		if (stg_xfer(sc, ti->ti_msgoutstr, len, MESSAGE_OUT_PHASE,
			     slp->sl_clear_atten) != 0)
		{
			printf("%s: MSGOUT short\n", slp->sl_xname);
		}
		else
		{
			if (slp->sl_msgphase >= MSGPH_ABORT) 
			{
				stg_disconnected(sc, ti);
			}
		}
		break;

	case MESSAGE_IN_PHASE:
		/* confirm phase and req signal */
		if (stg_expect_signal(sc, MESSAGE_IN_PHASE, BSTAT_REQ) <= 0)
			break;

		SCSI_LOW_SETUP_PHASE(ti, PH_MSGIN);

		/* read data with NOACK */
		regv = bus_space_read_1(iot, ioh, tmc_sdna);

		if (scsi_low_msgin(slp, ti, derror | regv) == 0)
		{
			if (scsi_low_is_msgout_continue(ti, 0) != 0)
			{
				scsi_low_attention(slp);
			}
		}

		/* read data with ACK */
		if (regv != bus_space_read_1(iot, ioh, tmc_rdata))
		{
			printf("%s: MSGIN: data mismatch\n", slp->sl_xname);
		}

		/* wait for the ack negated */
		stg_negate_signal(sc, BSTAT_ACK, "msgin<ACK>");

		if (slp->sl_msgphase != 0 && slp->sl_msgphase < MSGPH_ABORT)
		{
			stg_disconnected(sc, ti);
		}
		break;

	case BUSFREE_PHASE:
		printf("%s: unexpected disconnect\n", slp->sl_xname);
		stg_disconnected(sc, ti);
		break;

	default:
		slp->sl_error |= FATALIO;
		printf("%s: unknown phase bus %x intr %x\n",
			slp->sl_xname, status, astatus);
		break;
	}

out:
	bus_space_write_1(iot, ioh, tmc_ictl, sc->sc_icinit);
	return 1;
}

static int
stg_timeout(sc)
	struct stg_softc *sc;
{
	struct scsi_low_softc *slp = &sc->sc_sclow;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int tout, count;
	u_int8_t status;

	if (slp->sl_Tnexus == NULL)
		return 0;

	status = bus_space_read_1(iot, ioh, tmc_bstat);
	if ((status & PHASE_MASK) == 0)
	{
		if (sc->sc_ubf_timeout ++ == 0)
			return 0;

		printf("%s: unexpected bus free detected\n", slp->sl_xname);
		slp->sl_error |= FATALIO;
		scsi_low_print(slp, slp->sl_Tnexus);
		stg_disconnected(sc, slp->sl_Tnexus);
		return 0;
	}

	switch (status & PHASE_MASK)
	{
	case DATA_OUT_PHASE:
		if (sc->sc_dataout_timeout == 0)
			break;
		if ((status & BSTAT_REQ) == 0)
			break;
		if (bus_space_read_2(iot, ioh, tmc_fdcnt) != 0)
			break;
		if ((-- sc->sc_dataout_timeout) > 0)
			break;	

	        slp->sl_error |= PDMAERR;
		if ((slp->sl_flags & HW_WRITE_PADDING) == 0)
		{
			printf("%s: write padding required\n",
				slp->sl_xname);
			break;
		}	

		bus_space_write_1(iot, ioh, tmc_ictl, 0);

		tout = STG_DELAY_MAX;
		while (tout --)
		{
			status = bus_space_read_1(iot, ioh, tmc_bstat);
			if ((status & PHASE_MASK) != DATA_OUT_PHASE)
				break;

			if (bus_space_read_2(iot, ioh, tmc_fdcnt) != 0)
			{
				SCSI_LOW_DELAY(1);
				continue;
			}

			for (count = sc->sc_maxwsize; count > 0; count --)
				bus_space_write_1(iot, ioh, tmc_wfifo, 0);
		}

		status = bus_space_read_1(iot, ioh, tmc_bstat);
		if ((status & PHASE_MASK) == DATA_OUT_PHASE)
			sc->sc_dataout_timeout = SCSI_LOW_TIMEOUT_HZ;

		bus_space_write_1(iot, ioh, tmc_ictl, sc->sc_icinit);
		break;

	default:
		break;
	}
	return 0;
}
