/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Libretto PCMCIA floppy support by David Horwitt (dhorwitt@ucsd.edu)
 * aided by the Linux floppy driver modifications from David Bateman
 * (dbateman@eng.uts.edu.au).
 *
 * Copyright (c) 1993, 1994 by
 *  jc@irbs.UUCP (John Capo)
 *  vak@zebub.msk.su (Serge Vakulenko)
 *  ache@astral.msk.su (Andrew A. Chernov)
 *
 * Copyright (c) 1993, 1994, 1995 by
 *  joerg_wunsch@uriah.sax.de (Joerg Wunsch)
 *  dufault@hda.com (Peter Dufault)
 *
 * Copyright (c) 2001 Joerg Wunsch,
 *  joerg_wunsch@uriah.sax.de (Joerg Wunsch)
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from:	@(#)fd.c	7.4 (Berkeley) 5/25/91
 * $FreeBSD: src/sys/isa/fd.c,v 1.176.2.8 2002/05/15 21:56:14 joerg Exp $
 * $DragonFly: src/sys/dev/disk/fd/fd.c,v 1.20 2004/11/12 09:50:38 joerg Exp $
 *
 */

#include "opt_fdc.h"
#include "use_pccard.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bootmaj.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/devicestat.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <sys/buf2.h>

#include <machine/clock.h>
#include <machine/ioctl_fd.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <bus/isa/isavar.h>
#include <bus/isa/isareg.h>
#include "fdreg.h"
#include "fdc.h"
#include <bus/isa/rtc.h>

/* misuse a flag to identify format operation */
#define B_FORMAT B_XXX

/* configuration flags */
#define FDC_PRETEND_D0	(1 << 0)	/* pretend drive 0 to be there */
#define FDC_NO_FIFO	(1 << 2)	/* do not enable FIFO  */

/* internally used only, not really from CMOS: */
#define RTCFDT_144M_PRETENDED	0x1000

/* error returns for fd_cmd() */
#define FD_FAILED -1
#define FD_NOT_VALID -2
#define FDC_ERRMAX	100	/* do not log more */
/*
 * Stop retrying after this many DMA overruns.  Since each retry takes
 * one revolution, with 300 rpm., 25 retries take approximately 10
 * seconds which the read attempt will block in case the DMA overrun
 * is persistent.
 */
#define FDC_DMAOV_MAX	25

/*
 * Timeout value for the PIO loops to wait until the FDC main status
 * register matches our expectations (request for master, direction
 * bit).  This is supposed to be a number of microseconds, although
 * timing might actually not be very accurate.
 *
 * Timeouts of 100 msec are believed to be required for some broken
 * (old) hardware.
 */
#define	FDSTS_TIMEOUT	100000

#define NUMTYPES 17
#define NUMDENS  (NUMTYPES - 7)

/* These defines (-1) must match index for fd_types */
#define F_TAPE_TYPE	0x020	/* bit for fd_types to indicate tape */
#define NO_TYPE		0	/* must match NO_TYPE in ft.c */
#define FD_1720         1
#define FD_1480         2
#define FD_1440         3
#define FD_1200         4
#define FD_820          5
#define FD_800          6
#define FD_720          7
#define FD_360          8
#define FD_640          9
#define FD_1232         10

#define FD_1480in5_25   11
#define FD_1440in5_25   12
#define FD_820in5_25    13
#define FD_800in5_25    14
#define FD_720in5_25    15
#define FD_360in5_25    16
#define FD_640in5_25    17


static struct fd_type fd_types[NUMTYPES] =
{
{ 21,2,0xFF,0x04,82,3444,1,FDC_500KBPS,2,0x0C,2 }, /* 1.72M in HD 3.5in */
{ 18,2,0xFF,0x1B,82,2952,1,FDC_500KBPS,2,0x6C,1 }, /* 1.48M in HD 3.5in */
{ 18,2,0xFF,0x1B,80,2880,1,FDC_500KBPS,2,0x6C,1 }, /* 1.44M in HD 3.5in */
{ 15,2,0xFF,0x1B,80,2400,1,FDC_500KBPS,2,0x54,1 }, /*  1.2M in HD 5.25/3.5 */
{ 10,2,0xFF,0x10,82,1640,1,FDC_250KBPS,2,0x2E,1 }, /*  820K in HD 3.5in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_250KBPS,2,0x2E,1 }, /*  800K in HD 3.5in */
{  9,2,0xFF,0x20,80,1440,1,FDC_250KBPS,2,0x50,1 }, /*  720K in HD 3.5in */
{  9,2,0xFF,0x2A,40, 720,1,FDC_250KBPS,2,0x50,1 }, /*  360K in DD 5.25in */
{  8,2,0xFF,0x2A,80,1280,1,FDC_250KBPS,2,0x50,1 }, /*  640K in DD 5.25in */
{  8,3,0xFF,0x35,77,1232,1,FDC_500KBPS,2,0x74,1 }, /* 1.23M in HD 5.25in */

{ 18,2,0xFF,0x02,82,2952,1,FDC_500KBPS,2,0x02,2 }, /* 1.48M in HD 5.25in */
{ 18,2,0xFF,0x02,80,2880,1,FDC_500KBPS,2,0x02,2 }, /* 1.44M in HD 5.25in */
{ 10,2,0xFF,0x10,82,1640,1,FDC_300KBPS,2,0x2E,1 }, /*  820K in HD 5.25in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_300KBPS,2,0x2E,1 }, /*  800K in HD 5.25in */
{  9,2,0xFF,0x20,80,1440,1,FDC_300KBPS,2,0x50,1 }, /*  720K in HD 5.25in */
{  9,2,0xFF,0x23,40, 720,2,FDC_300KBPS,2,0x50,1 }, /*  360K in HD 5.25in */
{  8,2,0xFF,0x2A,80,1280,1,FDC_300KBPS,2,0x50,1 }, /*  640K in HD 5.25in */
};

#define DRVS_PER_CTLR 2		/* 2 floppies */

/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
devclass_t fdc_devclass;

/***********************************************************************\
* Per drive structure.							*
* N per controller  (DRVS_PER_CTLR)					*
\***********************************************************************/
struct fd_data {
	struct	fdc_data *fdc;	/* pointer to controller structure */
	int	fdsu;		/* this units number on this controller */
	int	type;		/* Drive type (FD_1440...) */
	struct	fd_type *ft;	/* pointer to the type descriptor */
	int	flags;
#define	FD_OPEN		0x01	/* it's open		*/
#define	FD_ACTIVE	0x02	/* it's active		*/
#define	FD_MOTOR	0x04	/* motor should be on	*/
#define	FD_MOTOR_WAIT	0x08	/* motor coming up	*/
	int	skip;
	int	hddrv;
#define FD_NO_TRACK -2
	int	track;		/* where we think the head is */
	int	options;	/* user configurable options, see ioctl_fd.h */
	struct	callout	toffhandle;
	struct	callout	tohandle;
	struct	callout motor;
	struct	devstat device_stats;
	device_t dev;
	fdu_t	fdu;
};

struct fdc_ivars {
	int	fdunit;
};
static devclass_t fd_devclass;

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/

/* internal functions */
static	void fdc_intr(void *);
static void set_motor(struct fdc_data *, int, int);
#  define TURNON 1
#  define TURNOFF 0
static timeout_t fd_turnoff;
static timeout_t fd_motor_on;
static void fd_turnon(struct fd_data *);
static void fdc_reset(fdc_p);
static int fd_in(struct fdc_data *, int *);
static int out_fdc(struct fdc_data *, int);
static void fdstart(struct fdc_data *);
static timeout_t fd_iotimeout;
static timeout_t fd_pseudointr;
static int fdstate(struct fdc_data *);
static int retrier(struct fdc_data *);
static int fdformat(dev_t, struct fd_formb *, struct thread *);

static int enable_fifo(fdc_p fdc);

static int fifo_threshold = 8;	/* XXX: should be accessible via sysctl */


#define DEVIDLE		0
#define FINDWORK	1
#define	DOSEEK		2
#define SEEKCOMPLETE 	3
#define	IOCOMPLETE	4
#define RECALCOMPLETE	5
#define	STARTRECAL	6
#define	RESETCTLR	7
#define	SEEKWAIT	8
#define	RECALWAIT	9
#define	MOTORWAIT	10
#define	IOTIMEDOUT	11
#define	RESETCOMPLETE	12
#define PIOREAD		13

#ifdef	FDC_DEBUG
static char const * const fdstates[] =
{
"DEVIDLE",
"FINDWORK",
"DOSEEK",
"SEEKCOMPLETE",
"IOCOMPLETE",
"RECALCOMPLETE",
"STARTRECAL",
"RESETCTLR",
"SEEKWAIT",
"RECALWAIT",
"MOTORWAIT",
"IOTIMEDOUT",
"RESETCOMPLETE",
"PIOREAD",
};

/* CAUTION: fd_debug causes huge amounts of logging output */
static int volatile fd_debug = 0;
#define TRACE0(arg) if(fd_debug) printf(arg)
#define TRACE1(arg1, arg2) if(fd_debug) printf(arg1, arg2)
#else /* FDC_DEBUG */
#define TRACE0(arg)
#define TRACE1(arg1, arg2)
#endif /* FDC_DEBUG */

void
fdout_wr(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->portt, fdc->porth, FDOUT+fdc->port_off, v);
}

static u_int8_t
fdsts_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDSTS+fdc->port_off);
}

static void
fddata_wr(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->portt, fdc->porth, FDDATA+fdc->port_off, v);
}

static u_int8_t
fddata_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDDATA+fdc->port_off);
}

static void
fdctl_wr_isa(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->ctlt, fdc->ctlh, 0, v);
}

#if 0

static u_int8_t
fdin_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDIN);
}

#endif

static	d_open_t	Fdopen;	/* NOTE, not fdopen */
static	d_close_t	fdclose;
static	d_ioctl_t	fdioctl;
static	d_strategy_t	fdstrategy;

static struct cdevsw fd_cdevsw = {
	/* name */	"fd",
	/* maj */	FD_CDEV_MAJOR,
	/* flags */	D_DISK,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */	Fdopen,
	/* close */	fdclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	fdioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	fdstrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};

static int
fdc_err(struct fdc_data *fdc, const char *s)
{
	fdc->fdc_errs++;
	if (s) {
		if (fdc->fdc_errs < FDC_ERRMAX)
			device_printf(fdc->fdc_dev, "%s", s);
		else if (fdc->fdc_errs == FDC_ERRMAX)
			device_printf(fdc->fdc_dev, "too many errors, not "
						    "logging any more\n");
	}

	return FD_FAILED;
}

/*
 * fd_cmd: Send a command to the chip.  Takes a varargs with this structure:
 * Unit number,
 * # of output bytes, output bytes as ints ...,
 * # of input bytes, input bytes as ints ...
 */
int
fd_cmd(struct fdc_data *fdc, int n_out, ...)
{
	u_char cmd;
	int n_in;
	int n;
	__va_list ap;

	__va_start(ap, n_out);
	cmd = (u_char)(__va_arg(ap, int));
	__va_end(ap);
	__va_start(ap, n_out);
	for (n = 0; n < n_out; n++)
	{
		if (out_fdc(fdc, __va_arg(ap, int)) < 0)
		{
			char msg[50];
			snprintf(msg, sizeof(msg),
				"cmd %x failed at out byte %d of %d\n",
				cmd, n + 1, n_out);
			return fdc_err(fdc, msg);
		}
	}
	n_in = __va_arg(ap, int);
	for (n = 0; n < n_in; n++)
	{
		int *ptr = __va_arg(ap, int *);
		if (fd_in(fdc, ptr) < 0)
		{
			char msg[50];
			snprintf(msg, sizeof(msg),
				"cmd %02x failed at in byte %d of %d\n",
				cmd, n + 1, n_in);
			return fdc_err(fdc, msg);
		}
	}

	return 0;
}

static int 
enable_fifo(fdc_p fdc)
{
	int i, j;

	if ((fdc->flags & FDC_HAS_FIFO) == 0) {
		
		/*
		 * XXX: 
		 * Cannot use fd_cmd the normal way here, since
		 * this might be an invalid command. Thus we send the
		 * first byte, and check for an early turn of data directon.
		 */
		
		if (out_fdc(fdc, I8207X_CONFIGURE) < 0)
			return fdc_err(fdc, "Enable FIFO failed\n");
		
		/* If command is invalid, return */
		j = FDSTS_TIMEOUT;
		while ((i = fdsts_rd(fdc) & (NE7_DIO | NE7_RQM))
		       != NE7_RQM && j-- > 0) {
			if (i == (NE7_DIO | NE7_RQM)) {
				fdc_reset(fdc);
				return FD_FAILED;
			}
			DELAY(1);
		}
		if (j<0 || 
		    fd_cmd(fdc, 3,
			   0, (fifo_threshold - 1) & 0xf, 0, 0) < 0) {
			fdc_reset(fdc);
			return fdc_err(fdc, "Enable FIFO failed\n");
		}
		fdc->flags |= FDC_HAS_FIFO;
		return 0;
	}
	if (fd_cmd(fdc, 4,
		   I8207X_CONFIGURE, 0, (fifo_threshold - 1) & 0xf, 0, 0) < 0)
		return fdc_err(fdc, "Re-enable FIFO failed\n");
	return 0;
}

static int
fd_sense_drive_status(fdc_p fdc, int *st3p)
{
	int st3;

	if (fd_cmd(fdc, 2, NE7CMD_SENSED, fdc->fdu, 1, &st3))
	{
		return fdc_err(fdc, "Sense Drive Status failed\n");
	}
	if (st3p)
		*st3p = st3;

	return 0;
}

static int
fd_sense_int(fdc_p fdc, int *st0p, int *cylp)
{
	int cyl, st0, ret;

	ret = fd_cmd(fdc, 1, NE7CMD_SENSEI, 1, &st0);
	if (ret) {
		(void)fdc_err(fdc,
			      "sense intr err reading stat reg 0\n");
		return ret;
	}

	if (st0p)
		*st0p = st0;

	if ((st0 & NE7_ST0_IC) == NE7_ST0_IC_IV) {
		/*
		 * There doesn't seem to have been an interrupt.
		 */
		return FD_NOT_VALID;
	}

	if (fd_in(fdc, &cyl) < 0) {
		return fdc_err(fdc, "can't get cyl num\n");
	}

	if (cylp)
		*cylp = cyl;

	return 0;
}


static int
fd_read_status(fdc_p fdc, int fdsu)
{
	int i, ret;

	for (i = 0; i < 7; i++) {
		/*
		 * XXX types are poorly chosen.  Only bytes can by read
		 * from the hardware, but fdc->status[] wants u_ints and
		 * fd_in() gives ints.
		 */
		int status;

		ret = fd_in(fdc, &status);
		fdc->status[i] = status;
		if (ret != 0)
			break;
	}

	if (ret == 0)
		fdc->flags |= FDC_STAT_VALID;
	else
		fdc->flags &= ~FDC_STAT_VALID;

	return ret;
}

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/

int
fdc_alloc_resources(struct fdc_data *fdc)
{
	device_t dev;
	int ispnp, ispcmcia;

	dev = fdc->fdc_dev;
	ispnp = (fdc->flags & FDC_ISPNP) != 0;
	ispcmcia = (fdc->flags & FDC_ISPCMCIA) != 0;
	fdc->rid_ioport = fdc->rid_irq = fdc->rid_drq = 0;
	fdc->res_ioport = fdc->res_irq = fdc->res_drq = 0;

	/*
	 * On standard ISA, we don't just use an 8 port range
	 * (e.g. 0x3f0-0x3f7) since that covers an IDE control
	 * register at 0x3f6.
	 *
	 * Isn't PC hardware wonderful.
	 *
	 * The Y-E Data PCMCIA FDC doesn't have this problem, it
	 * uses the register with offset 6 for pseudo-DMA, and the
	 * one with offset 7 as control register.
	 */
	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
					     &fdc->rid_ioport, 0ul, ~0ul, 
					     ispcmcia ? 8 : (ispnp ? 1 : 6),
					     RF_ACTIVE);
	if (fdc->res_ioport == 0) {
		device_printf(dev, "cannot reserve I/O port range\n");
		return ENXIO;
	}
	fdc->portt = rman_get_bustag(fdc->res_ioport);
	fdc->porth = rman_get_bushandle(fdc->res_ioport);

	if (!ispcmcia) {
		/*
		 * Some BIOSen report the device at 0x3f2-0x3f5,0x3f7
		 * and some at 0x3f0-0x3f5,0x3f7. We detect the former
		 * by checking the size and adjust the port address
		 * accordingly.
		 */
		if (bus_get_resource_count(dev, SYS_RES_IOPORT, 0) == 4)
			fdc->port_off = -2;

		/*
		 * Register the control port range as rid 1 if it
		 * isn't there already. Most PnP BIOSen will have
		 * already done this but non-PnP configurations don't.
		 *
		 * And some (!!) report 0x3f2-0x3f5 and completely
		 * leave out the control register!  It seems that some
		 * non-antique controller chips have a different
		 * method of programming the transfer speed which
		 * doesn't require the control register, but it's
		 * mighty bogus as the chip still responds to the
		 * address for the control register.
		 */
		if (bus_get_resource_count(dev, SYS_RES_IOPORT, 1) == 0) {
			u_long ctlstart;

			/* Find the control port, usually 0x3f7 */
			ctlstart = rman_get_start(fdc->res_ioport) +
				fdc->port_off + 7;

			bus_set_resource(dev, SYS_RES_IOPORT, 1, ctlstart, 1);
		}

		/*
		 * Now (finally!) allocate the control port.
		 */
		fdc->rid_ctl = 1;
		fdc->res_ctl = bus_alloc_resource(dev, SYS_RES_IOPORT,
						  &fdc->rid_ctl,
						  0ul, ~0ul, 1, RF_ACTIVE);
		if (fdc->res_ctl == 0) {
			device_printf(dev,
				      "cannot reserve control I/O port range\n");
			return ENXIO;
		}
		fdc->ctlt = rman_get_bustag(fdc->res_ctl);
		fdc->ctlh = rman_get_bushandle(fdc->res_ctl);
	}

	fdc->res_irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					  &fdc->rid_irq, 0ul, ~0ul, 1, 
					  RF_ACTIVE);
	if (fdc->res_irq == 0) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return ENXIO;
	}

	if ((fdc->flags & FDC_NODMA) == 0) {
		fdc->res_drq = bus_alloc_resource(dev, SYS_RES_DRQ,
						  &fdc->rid_drq, 0ul, ~0ul, 1, 
						  RF_ACTIVE);
		if (fdc->res_drq == 0) {
			device_printf(dev, "cannot reserve DMA request line\n");
			return ENXIO;
		}
		fdc->dmachan = fdc->res_drq->r_start;
	}

	return 0;
}

void
fdc_release_resources(struct fdc_data *fdc)
{
	device_t dev;

	dev = fdc->fdc_dev;
	if (fdc->res_irq != 0) {
		bus_deactivate_resource(dev, SYS_RES_IRQ, fdc->rid_irq,
					fdc->res_irq);
		bus_release_resource(dev, SYS_RES_IRQ, fdc->rid_irq,
				     fdc->res_irq);
	}
	if (fdc->res_ctl != 0) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
					fdc->res_ctl);
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
				     fdc->res_ctl);
	}
	if (fdc->res_ioport != 0) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT, fdc->rid_ioport,
					fdc->res_ioport);
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ioport,
				     fdc->res_ioport);
	}
	if (fdc->res_drq != 0) {
		bus_deactivate_resource(dev, SYS_RES_DRQ, fdc->rid_drq,
					fdc->res_drq);
		bus_release_resource(dev, SYS_RES_DRQ, fdc->rid_drq,
				     fdc->res_drq);
	}
}

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/

static struct isa_pnp_id fdc_ids[] = {
	{0x0007d041, "PC standard floppy disk controller"}, /* PNP0700 */
	{0x0107d041, "Standard floppy controller supporting MS Device Bay Spec"}, /* PNP0701 */
	{0}
};

int
fdc_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	struct fdc_ivars *ivars = device_get_ivars(child);

	switch (which) {
	case FDC_IVAR_FDUNIT:
		*result = ivars->fdunit;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

/*
 * fdc controller section.
 */
static int
fdc_probe(device_t dev)
{
	int	error, ic_type;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	bzero(fdc, sizeof *fdc);
	fdc->fdc_dev = dev;
	fdc->fdctl_wr = fdctl_wr_isa;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == ENXIO)
		return ENXIO;
	if (error == 0)
		fdc->flags |= FDC_ISPNP;

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_alloc_resources(fdc);
	if (error)
		goto out;

	/* First - lets reset the floppy controller */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);

	/* see if it can handle a command */
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}

	if (fd_cmd(fdc, 1, NE7CMD_VERSION, 1, &ic_type) == 0) {
		ic_type = (u_char)ic_type;
		switch (ic_type) {
		case 0x80:
			device_set_desc(dev, "NEC 765 or clone");
			fdc->fdct = FDC_NE765;
			break;
		case 0x81:
			device_set_desc(dev, "Intel 82077 or clone");
			fdc->fdct = FDC_I82077;
			break;
		case 0x90:
			device_set_desc(dev, "NEC 72065B or clone");
			fdc->fdct = FDC_NE72065;
			break;
		default:
			device_set_desc(dev, "generic floppy controller");
			fdc->fdct = FDC_UNKNOWN;
			break;
		}
	}

out:
	fdc_release_resources(fdc);
	return (error);
}

/*
 * Add a child device to the fdc controller.  It will then be probed etc.
 */
static void
fdc_add_child(device_t dev, const char *name, int unit)
{
	int	disabled;
	struct fdc_ivars *ivar;
	device_t child;

	ivar = malloc(sizeof *ivar, M_DEVBUF /* XXX */, M_WAITOK | M_ZERO);
	if (resource_int_value(name, unit, "drive", &ivar->fdunit) != 0)
		ivar->fdunit = 0;
	child = device_add_child(dev, name, unit);
	if (child == NULL)
		return;
	device_set_ivars(child, ivar);
	if (resource_int_value(name, unit, "disabled", &disabled) == 0
	    && disabled != 0)
		device_disable(child);
}

int
fdc_attach(device_t dev)
{
	struct	fdc_data *fdc;
	int	i, error;

	fdc = device_get_softc(dev);

	callout_init(&fdc->pseudointr_ch);

	error = fdc_alloc_resources(fdc);
	if (error) {
		device_printf(dev, "cannot re-aquire resources\n");
		return error;
	}
	error = BUS_SETUP_INTR(device_get_parent(dev), dev, fdc->res_irq,
			       INTR_TYPE_BIO, fdc_intr, fdc, &fdc->fdc_intr);
	if (error) {
		device_printf(dev, "cannot setup interrupt\n");
		return error;
	}
	fdc->fdcu = device_get_unit(dev);
	fdc->flags |= FDC_ATTACHED;

	if ((fdc->flags & FDC_NODMA) == 0) {
		/* Acquire the DMA channel forever, The driver will do the rest */
				/* XXX should integrate with rman */
		isa_dma_acquire(fdc->dmachan);
		isa_dmainit(fdc->dmachan, 128 << 3 /* XXX max secsize */);
	}
	fdc->state = DEVIDLE;

	/* reset controller, turn motor off, clear fdout mirror reg */
	fdout_wr(fdc, ((fdc->fdout = 0)));
	bufq_init(&fdc->head);

	/*
	 * Probe and attach any children.  We should probably detect
	 * devices from the BIOS unless overridden.
	 */
	for (i = resource_query_string(-1, "at", device_get_nameunit(dev));
	     i != -1;
	     i = resource_query_string(i, "at", device_get_nameunit(dev)))
		fdc_add_child(dev, resource_query_name(i),
			       resource_query_unit(i));

	return (bus_generic_attach(dev));
}

int
fdc_print_child(device_t me, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(me, child);
	retval += printf(" on %s drive %d\n", device_get_nameunit(me),
	       fdc_get_fdunit(child));
	
	return (retval);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_probe),
	DEVMETHOD(device_attach,	fdc_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdc_print_child),
	DEVMETHOD(bus_read_ivar,	fdc_read_ivar),
	/* Our children never use any other bus interface methods. */

	{ 0, 0 }
};

static driver_t fdc_driver = {
	"fdc",
	fdc_methods,
	sizeof(struct fdc_data)
};

DRIVER_MODULE(fdc, isa, fdc_driver, fdc_devclass, 0, 0);

/******************************************************************/
/*
 * devices attached to the controller section.  
 */
static int
fd_probe(device_t dev)
{
	int	i;
	u_int	fdt, st0, st3;
	struct	fd_data *fd;
	struct	fdc_data *fdc;
	fdsu_t	fdsu;
	static int fd_fifo = 0;

	fdsu = *(int *)device_get_ivars(dev); /* xxx cheat a bit... */
	fd = device_get_softc(dev);
	fdc = device_get_softc(device_get_parent(dev));

	bzero(fd, sizeof *fd);
	fd->dev = dev;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->fdu = device_get_unit(dev);

#ifdef __i386__
	/* look up what bios thinks we have */
	switch (fd->fdu) {
	case 0:
		if ((fdc->flags & FDC_ISPCMCIA))
			fdt = RTCFDT_144M;
		else if (device_get_flags(fdc->fdc_dev) & FDC_PRETEND_D0)
			fdt = RTCFDT_144M | RTCFDT_144M_PRETENDED;
		else
			fdt = (rtcin(RTC_FDISKETTE) & 0xf0);
		break;
	case 1:
		fdt = ((rtcin(RTC_FDISKETTE) << 4) & 0xf0);
		break;
	default:
		fdt = RTCFDT_NONE;
		break;
	}
#else
	fdt = RTCFDT_144M;	/* XXX probably */
#endif

	/* is there a unit? */
	if (fdt == RTCFDT_NONE)
		return (ENXIO);

	/* select it */
	set_motor(fdc, fdsu, TURNON);
	DELAY(1000000);	/* 1 sec */

	/* XXX This doesn't work before the first set_motor() */
	if (fd_fifo == 0 && fdc->fdct != FDC_NE765 && fdc->fdct != FDC_UNKNOWN
	    && (device_get_flags(fdc->fdc_dev) & FDC_NO_FIFO) == 0
	    && enable_fifo(fdc) == 0) {
		device_printf(device_get_parent(dev),
		    "FIFO enabled, %d bytes threshold\n", fifo_threshold);
	}
	fd_fifo = 1;

	if ((fd_cmd(fdc, 2, NE7CMD_SENSED, fdsu, 1, &st3) == 0)
	    && (st3 & NE7_ST3_T0)) {
		/* if at track 0, first seek inwards */
		/* seek some steps: */
		fd_cmd(fdc, 3, NE7CMD_SEEK, fdsu, 10, 0);
		DELAY(300000); /* ...wait a moment... */
		fd_sense_int(fdc, 0, 0); /* make ctrlr happy */
	}

	/* If we're at track 0 first seek inwards. */
	if ((fd_sense_drive_status(fdc, &st3) == 0) && (st3 & NE7_ST3_T0)) {
		/* Seek some steps... */
		if (fd_cmd(fdc, 3, NE7CMD_SEEK, fdsu, 10, 0) == 0) {
			/* ...wait a moment... */
			DELAY(300000);
			/* make ctrlr happy: */
			fd_sense_int(fdc, 0, 0);
		}
	}

	for (i = 0; i < 2; i++) {
		/*
		 * we must recalibrate twice, just in case the
		 * heads have been beyond cylinder 76, since most
		 * FDCs still barf when attempting to recalibrate
		 * more than 77 steps
		 */
		/* go back to 0: */
		if (fd_cmd(fdc, 2, NE7CMD_RECAL, fdsu, 0) == 0) {
			/* a second being enough for full stroke seek*/
			DELAY(i == 0 ? 1000000 : 300000);

			/* anything responding? */
			if (fd_sense_int(fdc, &st0, 0) == 0 &&
			    (st0 & NE7_ST0_EC) == 0)
				break; /* already probed succesfully */
		}
	}

	set_motor(fdc, fdsu, TURNOFF);

	if (st0 & NE7_ST0_EC) /* no track 0 -> no drive present */
		return (ENXIO);

	fd->track = FD_NO_TRACK;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->options = 0;
	callout_init(&fd->toffhandle);
	callout_init(&fd->tohandle);
	callout_init(&fd->motor);

	switch (fdt) {
	case RTCFDT_12M:
		device_set_desc(dev, "1200-KB 5.25\" drive");
		fd->type = FD_1200;
		break;
	case RTCFDT_144M | RTCFDT_144M_PRETENDED:
		device_set_desc(dev, "config-pretended 1440-MB 3.5\" drive");
		fdt = RTCFDT_144M;
		fd->type = FD_1440;
	case RTCFDT_144M:
		device_set_desc(dev, "1440-KB 3.5\" drive");
		fd->type = FD_1440;
		break;
	case RTCFDT_288M:
	case RTCFDT_288M_1:
		device_set_desc(dev, "2880-KB 3.5\" drive (in 1440-KB mode)");
		fd->type = FD_1440;
		break;
	case RTCFDT_360K:
		device_set_desc(dev, "360-KB 5.25\" drive");
		fd->type = FD_360;
		break;
	case RTCFDT_720K:
		printf("720-KB 3.5\" drive");
		fd->type = FD_720;
		break;
	default:
		return (ENXIO);
	}
	return (0);
}

static int
fd_attach(device_t dev)
{
	struct	fd_data *fd;
#if 0
	int	i;
	int	mynor;
	int	typemynor;
	int	typesize;
#endif

	fd = device_get_softc(dev);

	cdevsw_add(&fd_cdevsw, -1 << 6, fd->fdu << 6);
	make_dev(&fd_cdevsw, (fd->fdu << 6),
		UID_ROOT, GID_OPERATOR, 0640, "rfd%d", fd->fdu);

#if 0
	/* Other make_dev() go here. */
#endif

	/*
	 * Export the drive to the devstat interface.
	 */
	devstat_add_entry(&fd->device_stats, device_get_name(dev), 
			  device_get_unit(dev), 512, DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_FLOPPY | DEVSTAT_TYPE_IF_OTHER,
			  DEVSTAT_PRIORITY_FD);
	return (0);
}

static int
fd_detach(device_t dev)
{
	struct	fd_data *fd;

	fd = device_get_softc(dev);
	callout_stop(&fd->toffhandle);
	callout_stop(&fd->motor);

	return (0);
}

static device_method_t fd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fd_probe),
	DEVMETHOD(device_attach,	fd_attach),
	DEVMETHOD(device_detach,	fd_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend), /* XXX */
	DEVMETHOD(device_resume,	bus_generic_resume), /* XXX */

	{ 0, 0 }
};

static driver_t fd_driver = {
	"fd",
	fd_methods,
	sizeof(struct fd_data)
};

DRIVER_MODULE(fd, fdc, fd_driver, fd_devclass, 0, 0);

/****************************************************************************/
/*                            motor control stuff                           */
/*		remember to not deselect the drive we're working on         */
/****************************************************************************/
static void
set_motor(struct fdc_data *fdc, int fdsu, int turnon)
{
	int fdout = fdc->fdout;
	int needspecify = 0;

	if(turnon) {
		fdout &= ~FDO_FDSEL;
		fdout |= (FDO_MOEN0 << fdsu) + fdsu;
	} else
		fdout &= ~(FDO_MOEN0 << fdsu);

	if(!turnon
	   && (fdout & (FDO_MOEN0+FDO_MOEN1+FDO_MOEN2+FDO_MOEN3)) == 0)
		/* gonna turn off the last drive, put FDC to bed */
		fdout &= ~ (FDO_FRST|FDO_FDMAEN);
	else {
		/* make sure controller is selected and specified */
		if((fdout & (FDO_FRST|FDO_FDMAEN)) == 0)
			needspecify = 1;
		fdout |= (FDO_FRST|FDO_FDMAEN);
	}

	fdout_wr(fdc, fdout);
	fdc->fdout = fdout;
	TRACE1("[0x%x->FDOUT]", fdout);

	if (needspecify) {
		/*
		 * XXX
		 * special case: since we have just woken up the FDC
		 * from its sleep, we silently assume the command will
		 * be accepted, and do not test for a timeout
		 */
		(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
			     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
			     0);
		if (fdc->flags & FDC_HAS_FIFO)
			(void) enable_fifo(fdc);
	}
}

static void
fd_turnoff(void *xfd)
{
	int	s;
	fd_p fd = xfd;

	TRACE1("[fd%d: turnoff]", fd->fdu);

	s = splbio();
	/*
	 * Don't turn off the motor yet if the drive is active.
	 *
	 * If we got here, this could only mean we missed an interrupt.
	 * This can e. g. happen on the Y-E Date PCMCIA floppy controller
	 * after a controller reset.  Just schedule a pseudo-interrupt
	 * so the state machine gets re-entered.
	 */
	if (fd->fdc->state != DEVIDLE && fd->fdc->fdu == fd->fdu) {
		fdc_intr(fd->fdc);
		splx(s);
		return;
	}

	fd->flags &= ~FD_MOTOR;
	set_motor(fd->fdc, fd->fdsu, TURNOFF);
	splx(s);
}

static void
fd_motor_on(void *xfd)
{
	int	s;
	fd_p fd = xfd;

	s = splbio();
	fd->flags &= ~FD_MOTOR_WAIT;
	if((fd->fdc->fd == fd) && (fd->fdc->state == MOTORWAIT))
	{
		fdc_intr(fd->fdc);
	}
	splx(s);
}

static void
fd_turnon(fd_p fd)
{
	if(!(fd->flags & FD_MOTOR))
	{
		fd->flags |= (FD_MOTOR + FD_MOTOR_WAIT);
		set_motor(fd->fdc, fd->fdsu, TURNON);
		callout_reset(&fd->motor, hz, fd_motor_on, fd);
	}
}

static void
fdc_reset(fdc_p fdc)
{
	/* Try a reset, keep motor on */
	fdout_wr(fdc, fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	DELAY(100);
	/* enable FDC, but defer interrupts a moment */
	fdout_wr(fdc, fdc->fdout & ~FDO_FDMAEN);
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~FDO_FDMAEN);
	DELAY(100);
	fdout_wr(fdc, fdc->fdout);
	TRACE1("[0x%x->FDOUT]", fdc->fdout);

	/* XXX after a reset, silently believe the FDC will accept commands */
	(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
		     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
		     0);
	if (fdc->flags & FDC_HAS_FIFO)
		(void) enable_fifo(fdc);
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
/*
 * FDC IO functions, take care of the main status register, timeout
 * in case the desired status bits are never set.
 *
 * These PIO loops initially start out with short delays between
 * each iteration in the expectation that the required condition
 * is usually met quickly, so it can be handled immediately.  After
 * about 1 ms, stepping is increased to achieve a better timing
 * accuracy in the calls to DELAY().
 */
static int
fd_in(struct fdc_data *fdc, int *ptr)
{
	int i, j, step;

	for (j = 0, step = 1;
	    (i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM)) != (NE7_DIO|NE7_RQM) &&
	    j < FDSTS_TIMEOUT;
	    j += step) {
		if (i == NE7_RQM)
			return (fdc_err(fdc, "ready for output in input\n"));
		if (j == 1000)
			step = 1000;
		DELAY(step);
	}
	if (j >= FDSTS_TIMEOUT)
		return (fdc_err(fdc, bootverbose? "input ready timeout\n": 0));
#ifdef	FDC_DEBUG
	i = fddata_rd(fdc);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	*ptr = i;
	return (0);
#else	/* !FDC_DEBUG */
	i = fddata_rd(fdc);
	if (ptr)
		*ptr = i;
	return (0);
#endif	/* FDC_DEBUG */
}

static int
out_fdc(struct fdc_data *fdc, int x)
{
	int i, j, step;

	for (j = 0, step = 1;
	    (i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM)) != NE7_RQM &&
	    j < FDSTS_TIMEOUT;
	    j += step) {
		if (i == (NE7_DIO|NE7_RQM))
			return (fdc_err(fdc, "ready for input in output\n"));
		if (j == 1000)
			step = 1000;
		DELAY(step);
	}
	if (j >= FDSTS_TIMEOUT)
		return (fdc_err(fdc, bootverbose? "output ready timeout\n": 0));

	/* Send the command and return */
	fddata_wr(fdc, x);
	TRACE1("[0x%x->FDDATA]", x);
	return (0);
}

/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
int
Fdopen(dev_t dev, int flags, int mode, struct thread *td)
{
 	fdu_t fdu = FDUNIT(minor(dev));
	int type = FDTYPE(minor(dev));
	fd_p	fd;
	fdc_p	fdc;

	/* check bounds */
	if ((fd = devclass_get_softc(fd_devclass, fdu)) == 0)
		return (ENXIO);
	fdc = fd->fdc;
	if ((fdc == NULL) || (fd->type == NO_TYPE))
		return (ENXIO);
	if (type > NUMDENS)
		return (ENXIO);
	if (type == 0)
		type = fd->type;
	else {
		/*
		 * For each type of basic drive, make sure we are trying
		 * to open a type it can do,
		 */
		if (type != fd->type) {
			switch (fd->type) {
			case FD_360:
				return (ENXIO);
			case FD_720:
				if (   type != FD_820
				    && type != FD_800
				    && type != FD_640
				   )
					return (ENXIO);
				break;
			case FD_1200:
				switch (type) {
				case FD_1480:
					type = FD_1480in5_25;
					break;
				case FD_1440:
					type = FD_1440in5_25;
					break;
				case FD_1232:
					break;
				case FD_820:
					type = FD_820in5_25;
					break;
				case FD_800:
					type = FD_800in5_25;
					break;
				case FD_720:
					type = FD_720in5_25;
					break;
				case FD_640:
					type = FD_640in5_25;
					break;
				case FD_360:
					type = FD_360in5_25;
					break;
				default:
					return(ENXIO);
				}
				break;
			case FD_1440:
				if (   type != FD_1720
				    && type != FD_1480
				    && type != FD_1200
				    && type != FD_820
				    && type != FD_800
				    && type != FD_720
				    && type != FD_640
				    )
					return(ENXIO);
				break;
			}
		}
	}
	fd->ft = fd_types + type - 1;
	fd->flags |= FD_OPEN;
	/*
	 * Clearing the DMA overrun counter at open time is a bit messy.
	 * Since we're only managing one counter per controller, opening
	 * the second drive could mess it up.  Anyway, if the DMA overrun
	 * condition is really persistent, it will eventually time out
	 * still.  OTOH, clearing it here will ensure we'll at least start
	 * trying again after a previous (maybe even long ago) failure.
	 * Also, this is merely a stop-gap measure only that should not
	 * happen during normal operation, so we can tolerate it to be a
	 * bit sloppy about this.
	 */
	fdc->dma_overruns = 0;

	return 0;
}

int
fdclose(dev_t dev, int flags, int mode, struct thread *td)
{
 	fdu_t fdu = FDUNIT(minor(dev));
	struct fd_data *fd;

	fd = devclass_get_softc(fd_devclass, fdu);
	fd->flags &= ~FD_OPEN;
	fd->options &= ~(FDOPT_NORETRY | FDOPT_NOERRLOG);

	return (0);
}

/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
void
fdstrategy(struct buf *bp)
{
	unsigned nblocks, blknum, cando;
 	int	s;
 	fdu_t	fdu;
 	fdc_p	fdc;
 	fd_p	fd;
	size_t	fdblk;

 	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	if (fd == 0)
		panic("fdstrategy: buf for nonexistent device (%#lx, %#lx)",
		      (u_long)major(bp->b_dev), (u_long)minor(bp->b_dev));
	fdc = fd->fdc;
	if (fd->type == NO_TYPE) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto bad;
	};

	fdblk = 128 << (fd->ft->secsize);
	if (!(bp->b_flags & B_FORMAT)) {
		if (bp->b_blkno < 0) {
			printf(
		"fd%d: fdstrat: bad request blkno = %lu, bcount = %ld\n",
			       fdu, (u_long)bp->b_blkno, bp->b_bcount);
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
		if ((bp->b_bcount % fdblk) != 0) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
	}

	/*
	 * Set up block calculations.
	 */
	if (bp->b_blkno > 20000000) {
		/*
		 * Reject unreasonably high block number, prevent the
		 * multiplication below from overflowing.
		 */
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	blknum = (unsigned) bp->b_blkno * DEV_BSIZE/fdblk;
 	nblocks = fd->ft->size;
	bp->b_resid = 0;
	if (blknum + (bp->b_bcount / fdblk) > nblocks) {
		if (blknum <= nblocks) {
			cando = (nblocks - blknum) * fdblk;
			bp->b_resid = bp->b_bcount - cando;
			if (cando == 0)
				goto bad;	/* not actually bad but EOF */
		} else {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
	}
 	bp->b_pblkno = bp->b_blkno;
	s = splbio();
	bufqdisksort(&fdc->head, bp);
	callout_stop(&fd->toffhandle);

	/* Tell devstat we are starting on the transaction */
	devstat_start_transaction(&fd->device_stats);
	device_busy(fd->dev);

	fdstart(fdc);
	splx(s);
	return;

bad:
	biodone(bp);
}

/***************************************************************\
*				fdstart				*
* We have just queued something.. if the controller is not busy	*
* then simulate the case where it has just finished a command	*
* So that it (the interrupt routine) looks on the queue for more*
* work to do and picks up what we just added.			*
* If the controller is already busy, we need do nothing, as it	*
* will pick up our work when the present work completes		*
\***************************************************************/
static void
fdstart(struct fdc_data *fdc)
{
	int s;

	s = splbio();
	if(fdc->state == DEVIDLE)
	{
		fdc_intr(fdc);
	}
	splx(s);
}

static void
fd_iotimeout(void *xfdc)
{
 	fdc_p fdc;
	int s;

	fdc = xfdc;
	TRACE1("fd%d[fd_iotimeout()]", fdc->fdu);

	/*
	 * Due to IBM's brain-dead design, the FDC has a faked ready
	 * signal, hardwired to ready == true. Thus, any command
	 * issued if there's no diskette in the drive will _never_
	 * complete, and must be aborted by resetting the FDC.
	 * Many thanks, Big Blue!
	 * The FDC must not be reset directly, since that would
	 * interfere with the state machine.  Instead, pretend that
	 * the command completed but was invalid.  The state machine
	 * will reset the FDC and retry once.
	 */
	s = splbio();
	fdc->status[0] = NE7_ST0_IC_IV;
	fdc->flags &= ~FDC_STAT_VALID;
	fdc->state = IOTIMEDOUT;
	fdc_intr(fdc);
	splx(s);
}

/* just ensure it has the right spl */
static void
fd_pseudointr(void *xfdc)
{
	int	s;

	s = splbio();
	fdc_intr(xfdc);
	splx(s);
}

/***********************************************************************\
*                                 fdintr				*
* keep calling the state machine until it returns a 0			*
* ALWAYS called at SPLBIO 						*
\***********************************************************************/
static void
fdc_intr(void *xfdc)
{
	fdc_p fdc = xfdc;
	while(fdstate(fdc))
		;
}

/*
 * magic pseudo-DMA initialization for YE FDC. Sets count and
 * direction
 */
#define SET_BCDR(fdc,wr,cnt,port) \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port,	 \
	    ((cnt)-1) & 0xff);						 \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port + 1, \
	    ((wr ? 0x80 : 0) | ((((cnt)-1) >> 8) & 0x7f)));

/*
 * fdcpio(): perform programmed IO read/write for YE PCMCIA floppy
 */
static int fdcpio(fdc_p fdc, long flags, caddr_t addr, u_int count)
{
	u_char *cptr = (u_char *)addr;

	if (flags & B_READ) {
		if (fdc->state != PIOREAD) {
			fdc->state = PIOREAD;
			return(0);
		};
		SET_BCDR(fdc, 0, count, 0);
		bus_space_read_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
	} else {
		bus_space_write_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
		SET_BCDR(fdc, 0, count, 0);
	};
	return(1);
}

/***********************************************************************\
* The controller state machine.						*
* if it returns a non zero value, it should be called again immediatly	*
\***********************************************************************/
static int
fdstate(fdc_p fdc)
{
	int read, format, head, i, sec = 0, sectrac, st0, cyl, st3;
	unsigned blknum = 0, b_cylinder = 0;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	struct buf *bp;
	struct fd_formb *finfo = NULL;
	size_t fdblk;

	bp = fdc->bp;
	if (bp == NULL) {
		bp = bufq_first(&fdc->head);
		if (bp != NULL) {
			bufq_remove(&fdc->head, bp);
			fdc->bp = bp;
		}
	}
	if (bp == NULL) {
		/***********************************************\
		* nothing left for this controller to do	*
		* Force into the IDLE state,			*
		\***********************************************/
		fdc->state = DEVIDLE;
		if (fdc->fd) {
			device_printf(fdc->fdc_dev,
			    "unexpected valid fd pointer\n");
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
		}
		TRACE1("[fdc%d IDLE]", fdc->fdcu);
 		return (0);
	}
	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;
	if (fdc->fd && (fd != fdc->fd))
		device_printf(fd->dev, "confused fd pointers\n");
	read = bp->b_flags & B_READ;
	format = bp->b_flags & B_FORMAT;
	if (format) {
		finfo = (struct fd_formb *)bp->b_data;
		fd->skip = (char *)&(finfo->fd_formb_cylno(0))
			- (char *)finfo;
	}
	if (fdc->state == DOSEEK || fdc->state == SEEKCOMPLETE) {
		blknum = (unsigned) bp->b_pblkno * DEV_BSIZE/fdblk +
			fd->skip/fdblk;
		b_cylinder = blknum / (fd->ft->sectrac * fd->ft->heads);
	}
	TRACE1("fd%d", fdu);
	TRACE1("[%s]", fdstates[fdc->state]);
	TRACE1("(0x%x)", fd->flags);
	callout_reset(&fd->toffhandle, 4 * hz, fd_turnoff, fd);
	switch (fdc->state)
	{
	case DEVIDLE:
	case FINDWORK:	/* we have found new work */
		fdc->retry = 0;
		fd->skip = 0;
		fdc->fd = fd;
		fdc->fdu = fdu;
		fdc->fdctl_wr(fdc, fd->ft->trans);
		TRACE1("[0x%x->FDCTL]", fd->ft->trans);
		/*******************************************************\
		* If the next drive has a motor startup pending, then	*
		* it will start up in its own good time		*
		\*******************************************************/
		if(fd->flags & FD_MOTOR_WAIT) {
			fdc->state = MOTORWAIT;
			return (0); /* come back later */
		}
		/*******************************************************\
		* Maybe if it's not starting, it SHOULD be starting	*
		\*******************************************************/
		if (!(fd->flags & FD_MOTOR))
		{
			fdc->state = MOTORWAIT;
			fd_turnon(fd);
			return (0);
		}
		else	/* at least make sure we are selected */
		{
			set_motor(fdc, fd->fdsu, TURNON);
		}
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else
			fdc->state = DOSEEK;
		break;
	case DOSEEK:
		if (b_cylinder == (unsigned)fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			break;
		}
		if (fd_cmd(fdc, 3, NE7CMD_SEEK,
			   fd->fdsu, b_cylinder * fd->ft->steptrac,
			   0))
		{
			/*
			 * seek command not accepted, looks like
			 * the FDC went off to the Saints...
			 */
			fdc->retry = 6;	/* try a reset */
			return(retrier(fdc));
		}
		fd->track = FD_NO_TRACK;
		fdc->state = SEEKWAIT;
		return(0);	/* will return later */
	case SEEKWAIT:
		/* allow heads to settle */
		callout_reset(&fdc->pseudointr_ch, hz / 16,
			       fd_pseudointr, fdc);
		fdc->state = SEEKCOMPLETE;
		return(0);	/* will return later */
	case SEEKCOMPLETE : /* SEEK DONE, START DMA */
		/* Make sure seek really happened*/
		if(fd->track == FD_NO_TRACK) {
			int descyl = b_cylinder * fd->ft->steptrac;
			do {
				/*
				 * This might be a "ready changed" interrupt,
				 * which cannot really happen since the
				 * RDY pin is hardwired to + 5 volts.  This
				 * generally indicates a "bouncing" intr
				 * line, so do one of the following:
				 *
				 * When running on an enhanced FDC that is
				 * known to not go stuck after responding
				 * with INVALID, fetch all interrupt states
				 * until seeing either an INVALID or a
				 * real interrupt condition.
				 *
				 * When running on a dumb old NE765, give
				 * up immediately.  The controller will
				 * provide up to four dummy RC interrupt
				 * conditions right after reset (for the
				 * corresponding four drives), so this is
				 * our only chance to get notice that it
				 * was not the FDC that caused the interrupt.
				 */
				if (fd_sense_int(fdc, &st0, &cyl)
				    == FD_NOT_VALID)
					return 0;
				if(fdc->fdct == FDC_NE765
				   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
					return 0; /* hope for a real intr */
			} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);

			if (0 == descyl) {
				int failed = 0;
				/*
				 * seek to cyl 0 requested; make sure we are
				 * really there
				 */
				if (fd_sense_drive_status(fdc, &st3))
					failed = 1;
				if ((st3 & NE7_ST3_T0) == 0) {
					printf(
		"fd%d: Seek to cyl 0, but not really there (ST3 = %b)\n",
					       fdu, st3, NE7_ST3BITS);
					failed = 1;
				}

				if (failed) {
					if(fdc->retry < 3)
						fdc->retry = 3;
					return (retrier(fdc));
				}
			}

			if (cyl != descyl) {
				printf(
		"fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n",
				       fdu, descyl, cyl, st0);
				if (fdc->retry < 3)
					fdc->retry = 3;
				return (retrier(fdc));
			}
		}

		fd->track = b_cylinder;
		if (!(fdc->flags & FDC_NODMA))
			isa_dmastart(bp->b_flags, bp->b_data+fd->skip,
				format ? bp->b_bcount : fdblk, fdc->dmachan);
		sectrac = fd->ft->sectrac;
		sec = blknum %  (sectrac * fd->ft->heads);
		head = sec / sectrac;
		sec = sec % sectrac + 1;
		fd->hddrv = ((head&1)<<2)+fdu;

		if(format || !read)
		{
			/* make sure the drive is writable */
			if(fd_sense_drive_status(fdc, &st3) != 0)
			{
				/* stuck controller? */
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(bp->b_flags,
						    bp->b_data + fd->skip,
						    format ? bp->b_bcount : fdblk,
						    fdc->dmachan);
				fdc->retry = 6;	/* reset the beast */
				return (retrier(fdc));
			}
			if(st3 & NE7_ST3_WP)
			{
				/*
				 * XXX YES! this is ugly.
				 * in order to force the current operation
				 * to fail, we will have to fake an FDC
				 * error - all error handling is done
				 * by the retrier()
				 */
				fdc->status[0] = NE7_ST0_IC_AT;
				fdc->status[1] = NE7_ST1_NW;
				fdc->status[2] = 0;
				fdc->status[3] = fd->track;
				fdc->status[4] = head;
				fdc->status[5] = sec;
				fdc->retry = 8;	/* break out immediately */
				fdc->state = IOTIMEDOUT; /* not really... */
				return (1);
			}
		}

		if (format) {
			if (fdc->flags & FDC_NODMA) {
				/*
				 * This seems to be necessary for
				 * whatever obscure reason; if we omit
				 * it, we end up filling the sector ID
				 * fields of the newly formatted track
				 * entirely with garbage, causing
				 * `wrong cylinder' errors all over
				 * the place when trying to read them
				 * back.
				 *
				 * Umpf.
				 */
				SET_BCDR(fdc, 1, bp->b_bcount, 0);

				(void)fdcpio(fdc,bp->b_flags,
					bp->b_data+fd->skip,
					bp->b_bcount);

			}
			/* formatting */
			if(fd_cmd(fdc, 6,  NE7CMD_FORMAT, head << 2 | fdu,
				  finfo->fd_formb_secshift,
				  finfo->fd_formb_nsecs,
				  finfo->fd_formb_gaplen,
				  finfo->fd_formb_fillbyte, 0)) {
				/* controller fell over */
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(bp->b_flags,
						    bp->b_data + fd->skip,
						    format ? bp->b_bcount : fdblk,
						    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		} else {
			if (fdc->flags & FDC_NODMA) {
				/*
				 * this seems to be necessary even when
				 * reading data
				 */
				SET_BCDR(fdc, 1, fdblk, 0);

				/*
				 * perform the write pseudo-DMA before
				 * the WRITE command is sent
				 */
				if (!read)
					(void)fdcpio(fdc,bp->b_flags,
					    bp->b_data+fd->skip,
					    fdblk);
			}
			if (fd_cmd(fdc, 9,
				   (read ? NE7CMD_READ : NE7CMD_WRITE),
				   head << 2 | fdu,  /* head & unit */
				   fd->track,        /* track */
				   head,
				   sec,              /* sector + 1 */
				   fd->ft->secsize,  /* sector size */
				   sectrac,          /* sectors/track */
				   fd->ft->gap,      /* gap size */
				   fd->ft->datalen,  /* data length */
				   0)) {
				/* the beast is sleeping again */
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(bp->b_flags,
						    bp->b_data + fd->skip,
						    format ? bp->b_bcount : fdblk,
						    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		}
		if (fdc->flags & FDC_NODMA)
			/*
			 * if this is a read, then simply await interrupt
			 * before performing PIO
			 */
			if (read && !fdcpio(fdc,bp->b_flags,
			    bp->b_data+fd->skip,fdblk)) {
				callout_reset(&fd->tohandle, hz,
						fd_iotimeout, fdc);
				return(0);      /* will return later */
			};

		/*
		 * write (or format) operation will fall through and
		 * await completion interrupt
		 */
		fdc->state = IOCOMPLETE;
		callout_reset(&fd->tohandle, hz, fd_iotimeout, fdc);
		return (0);	/* will return later */
	case PIOREAD:
		/* 
		 * actually perform the PIO read.  The IOCOMPLETE case
		 * removes the timeout for us.  
		 */
		(void)fdcpio(fdc,bp->b_flags,bp->b_data+fd->skip,fdblk);
		fdc->state = IOCOMPLETE;
		/* FALLTHROUGH */
	case IOCOMPLETE: /* IO DONE, post-analyze */
		callout_stop(&fd->tohandle);

		if (fd_read_status(fdc, fd->fdsu)) {
			if (!(fdc->flags & FDC_NODMA))
				isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
					    format ? bp->b_bcount : fdblk,
					    fdc->dmachan);
			if (fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			return (retrier(fdc));
  		}

		fdc->state = IOTIMEDOUT;

		/* FALLTHROUGH */

	case IOTIMEDOUT:
		if (!(fdc->flags & FDC_NODMA))
			isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
				format ? bp->b_bcount : fdblk, fdc->dmachan);
		if (fdc->status[0] & NE7_ST0_IC) {
                        if ((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
			    && fdc->status[1] & NE7_ST1_OR) {
                                /*
				 * DMA overrun. Someone hogged the bus and
				 * didn't release it in time for the next
				 * FDC transfer.
				 *
				 * We normally restart this without bumping
				 * the retry counter.  However, in case
				 * something is seriously messed up (like
				 * broken hardware), we rather limit the
				 * number of retries so the IO operation
				 * doesn't block indefinately.
				 */
				if (fdc->dma_overruns++ < FDC_DMAOV_MAX) {
					fdc->state = SEEKCOMPLETE;
					return (1);
				} /* else fall through */
                        }
			if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_IV
				&& fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			else if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
				&& fdc->status[2] & NE7_ST2_WC
				&& fdc->retry < 3)
				fdc->retry = 3;	/* force recalibrate */
			return (retrier(fdc));
		}
		/* All OK */
		/* Operation successful, retry DMA overruns again next time. */
		fdc->dma_overruns = 0;
		fd->skip += fdblk;
		if (!format && fd->skip < bp->b_bcount - bp->b_resid) {
			/* set up next transfer */
			fdc->state = DOSEEK;
		} else {
			/* ALL DONE */
			fd->skip = 0;
			fdc->bp = NULL;
			device_unbusy(fd->dev);
			devstat_end_transaction_buf(&fd->device_stats, bp);
			biodone(bp);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
			fdc->state = FINDWORK;
		}
		return (1);
	case RESETCTLR:
		fdc_reset(fdc);
		fdc->retry++;
		fdc->state = RESETCOMPLETE;
		return (0);
	case RESETCOMPLETE:
		/*
		 * Discard all the results from the reset so that they
		 * can't cause an unexpected interrupt later.
		 */
		for (i = 0; i < 4; i++)
			(void)fd_sense_int(fdc, &st0, &cyl);
		fdc->state = STARTRECAL;
		/* Fall through. */
	case STARTRECAL:
		if(fd_cmd(fdc, 2, NE7CMD_RECAL, fdu, 0)) {
			/* arrgl */
			fdc->retry = 6;
			return (retrier(fdc));
		}
		fdc->state = RECALWAIT;
		return (0);	/* will return later */
	case RECALWAIT:
		/* allow heads to settle */
		callout_reset(&fdc->pseudointr_ch, hz / 8, fd_pseudointr, fdc);
		fdc->state = RECALCOMPLETE;
		return (0);	/* will return later */
	case RECALCOMPLETE:
		do {
			/*
			 * See SEEKCOMPLETE for a comment on this:
			 */
			if (fd_sense_int(fdc, &st0, &cyl) == FD_NOT_VALID)
				return 0;
			if(fdc->fdct == FDC_NE765
			   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
				return 0; /* hope for a real intr */
		} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);
		if ((st0 & NE7_ST0_IC) != NE7_ST0_IC_NT || cyl != 0)
		{
			if(fdc->retry > 3)
				/*
				 * a recalibrate from beyond cylinder 77
				 * will "fail" due to the FDC limitations;
				 * since people used to complain much about
				 * the failure message, try not logging
				 * this one if it seems to be the first
				 * time in a line
				 */
				printf("fd%d: recal failed ST0 %b cyl %d\n",
				       fdu, st0, NE7_ST0BITS, cyl);
			if(fdc->retry < 3) fdc->retry = 3;
			return (retrier(fdc));
		}
		fd->track = 0;
		/* Seek (probably) necessary */
		fdc->state = DOSEEK;
		return (1);	/* will return immediatly */
	case MOTORWAIT:
		if(fd->flags & FD_MOTOR_WAIT)
		{
			return (0); /* time's not up yet */
		}
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else {
			/*
			 * If all motors were off, then the controller was
			 * reset, so it has lost track of the current
			 * cylinder.  Recalibrate to handle this case.
			 * But first, discard the results of the reset.
			 */
			fdc->state = RESETCOMPLETE;
		}
		return (1);	/* will return immediatly */
	default:
		device_printf(fdc->fdc_dev, "unexpected FD int->");
		if (fd_read_status(fdc, fd->fdsu) == 0)
			printf("FDC status :%x %x %x %x %x %x %x   ",
			       fdc->status[0],
			       fdc->status[1],
			       fdc->status[2],
			       fdc->status[3],
			       fdc->status[4],
			       fdc->status[5],
			       fdc->status[6] );
		else
			printf("No status available   ");
		if (fd_sense_int(fdc, &st0, &cyl) != 0)
		{
			printf("[controller is dead now]\n");
			return (0);
		}
		printf("ST0 = %x, PCN = %x\n", st0, cyl);
		return (0);
	}
	/*XXX confusing: some branches return immediately, others end up here*/
	return (1); /* Come back immediatly to new state */
}

static int
retrier(struct fdc_data *fdc)
{
	struct buf *bp;
	struct fd_data *fd;
	int fdu;

	bp = fdc->bp;

	/* XXX shouldn't this be cached somewhere?  */
	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	if (fd->options & FDOPT_NORETRY)
		goto fail;

	switch (fdc->retry) {
	case 0: case 1: case 2:
		fdc->state = SEEKCOMPLETE;
		break;
	case 3: case 4: case 5:
		fdc->state = STARTRECAL;
		break;
	case 6:
		fdc->state = RESETCTLR;
		break;
	case 7:
		break;
	default:
	fail:
		{
			int printerror = (fd->options & FDOPT_NOERRLOG) == 0;

			if (printerror) {
				/*
				 * note: use the correct device for more
				 * verbose error reporting.
				 */
				dev_t subdev;

				subdev = make_sub_dev(bp->b_dev,
				    (FDUNIT(minor(bp->b_dev))<<3)|RAW_PART);
				diskerr(bp, subdev,
					"hard error", LOG_PRINTF,
					fdc->fd->skip / DEV_BSIZE,
					(struct disklabel *)NULL);
			}
			if (printerror) {
				if (fdc->flags & FDC_STAT_VALID)
					printf(
			" (ST0 %b ST1 %b ST2 %b cyl %u hd %u sec %u)\n",
					       fdc->status[0], NE7_ST0BITS,
					       fdc->status[1], NE7_ST1BITS,
					       fdc->status[2], NE7_ST2BITS,
					       fdc->status[3], fdc->status[4],
					       fdc->status[5]);
				else
					printf(" (No status)\n");
			}
		}
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid += bp->b_bcount - fdc->fd->skip;
		fdc->bp = NULL;
		fdc->fd->skip = 0;
		device_unbusy(fd->dev);
		devstat_end_transaction_buf(&fdc->fd->device_stats, bp);
		biodone(bp);
		fdc->state = FINDWORK;
		fdc->flags |= FDC_NEEDS_RESET;
		fdc->fd = (fd_p) 0;
		fdc->fdu = -1;
		return (1);
	}
	fdc->retry++;
	return (1);
}

static int
fdformat(dev_t dev, struct fd_formb *finfo, struct thread *td)
{
	struct proc *p = td->td_proc;
 	fdu_t	fdu;
 	fd_p	fd;

	struct buf *bp;
	int rv = 0, s;
	size_t fdblk;

 	fdu	= FDUNIT(minor(dev));
	fd	= devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;

	/* set up a buffer header for fdstrategy() */
	bp = malloc(sizeof(struct buf), M_TEMP, M_WAITOK | M_ZERO);

	/*
	 * keep the process from being swapped
	 */
	PHOLD(p);
	BUF_LOCKINIT(bp);
	BUF_LOCK(bp, LK_EXCLUSIVE);
	bp->b_flags = B_PHYS | B_FORMAT;

	/*
	 * calculate a fake blkno, so fdstrategy() would initiate a
	 * seek to the requested cylinder
	 */
	bp->b_blkno = (finfo->cyl * (fd->ft->sectrac * fd->ft->heads)
		+ finfo->head * fd->ft->sectrac) * fdblk / DEV_BSIZE;

	bp->b_bcount = sizeof(struct fd_idfield_data) * finfo->fd_formb_nsecs;
	bp->b_data = (caddr_t)finfo;

	/* now do the format */
	bp->b_dev = dev;
	BUF_STRATEGY(bp, 0);

	/* ...and wait for it to complete */
	s = splbio();
	while(!(bp->b_flags & B_DONE)) {
		rv = tsleep((caddr_t)bp, 0, "fdform", 20 * hz);
		if (rv == EWOULDBLOCK)
			break;
	}
	splx(s);

	if (rv == EWOULDBLOCK) {
		/* timed out */
		rv = EIO;
		device_unbusy(fd->dev);
		biodone(bp);
	}
	if (bp->b_flags & B_ERROR)
		rv = bp->b_error;
	/*
	 * allow the process to be swapped
	 */
	PRELE(p);
	BUF_UNLOCK(bp);
	BUF_LOCKFREE(bp);
	free(bp, M_TEMP);
	return rv;
}

/*
 * TODO: don't allocate buffer on stack.
 */

static int
fdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
 	fdu_t	fdu = FDUNIT(minor(dev));
 	fd_p	fd = devclass_get_softc(fd_devclass, fdu);
	size_t fdblk;

	struct fd_type *fdt;
	struct disklabel *dl;
	struct fdc_status *fsp;
	char buffer[DEV_BSIZE];
	int error = 0;

	fdblk = 128 << fd->ft->secsize;

	switch (cmd) {
	case DIOCGDINFO:
		bzero(buffer, sizeof (buffer));
		dl = (struct disklabel *)buffer;
		dl->d_secsize = fdblk;
		fdt = fd->ft;
		dl->d_secpercyl = fdt->size / fdt->tracks;
		dl->d_type = DTYPE_FLOPPY;

		if (readdisklabel(dev, dl)
		    == NULL)
			error = 0;
		else
			error = EINVAL;

		*(struct disklabel *)addr = *dl;
		break;

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}

		dl = (struct disklabel *)addr;

		if ((error = setdisklabel((struct disklabel *)buffer, dl,
					  (u_long)0)) != 0)
			break;

		error = writedisklabel(dev, (struct disklabel *)buffer);
		break;
	case FD_FORM:
		if ((flag & FWRITE) == 0)
			error = EBADF;	/* must be opened for writing */
		else if (((struct fd_formb *)addr)->format_version !=
			FD_FORMAT_VERSION)
			error = EINVAL;	/* wrong version of formatting prog */
		else
			error = fdformat(dev, (struct fd_formb *)addr, td);
		break;

	case FD_GTYPE:                  /* get drive type */
		*(struct fd_type *)addr = *fd->ft;
		break;

	case FD_STYPE:                  /* set drive type */
		/* this is considered harmful; only allow for superuser */
		if (suser(td) != 0)
			return EPERM;
		*fd->ft = *(struct fd_type *)addr;
		break;

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd->options;
		break;

	case FD_SOPTS:			/* set drive options */
		fd->options = *(int *)addr;
		break;

	case FD_GSTAT:
		fsp = (struct fdc_status *)addr;
		if ((fd->fdc->flags & FDC_STAT_VALID) == 0)
			return EINVAL;
		memcpy(fsp->status, fd->fdc->status, 7 * sizeof(u_int));
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*
 * Hello emacs, these are the
 * Local Variables:
 *  c-indent-level:               8
 *  c-continued-statement-offset: 8
 *  c-continued-brace-offset:     0
 *  c-brace-offset:              -8
 *  c-brace-imaginary-offset:     0
 *  c-argdecl-indent:             8
 *  c-label-offset:              -8
 *  c++-hanging-braces:           1
 *  c++-access-specifier-offset: -8
 *  c++-empty-arglist-indent:     8
 *  c++-friend-offset:            0
 * End:
 */
