/*
 * Copyright (c) 1998, 2001 Nicolas Souchu
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
 * $FreeBSD: src/sys/dev/bktr/bktr_i2c.c,v 1.25 2003/12/08 07:59:18 obrien Exp $
 * $DragonFly: src/sys/dev/video/bktr/bktr_i2c.c,v 1.5 2004/05/15 17:54:12 joerg Exp $
 */

/*
 * I2C support for the bti2c chipset.
 *
 * From brooktree848.c <fsmp@freefall.org>
 */

#include "opt_bktr.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>

#include <sys/select.h>

#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>

#include <machine/bus_memio.h>          /* for bus space */
#include <machine/bus.h>
#include <sys/bus.h>

#include <dev/video/meteor/ioctl_meteor.h>
#include <dev/video/bktr/ioctl_bt848.h>	/* extensions to ioctl_meteor.h */
#include <dev/video/bktr/bktr_reg.h>
#include <dev/video/bktr/bktr_i2c.h>

#include <bus/smbus/smbconf.h>
#include <bus/iicbus/iiconf.h>

#define I2C_DELAY	40

/* Compilation is void if BKTR_USE_FREEBSD_SMBUS is not
 * defined. This allows bktr owners to have smbus active for there
 * motherboard and still use their bktr without smbus.
 */
#if defined(BKTR_USE_FREEBSD_SMBUS)

#define BTI2C_DEBUG(x)	if (bti2c_debug) (x)
static int bti2c_debug = 0;

/*
 * Call this to pass the address of the bktr device to the
 * bti2c_i2c layer and initialize all the I2C bus architecture
 */
int bt848_i2c_attach(device_t dev)
{
	struct bktr_softc *bktr_sc = (struct bktr_softc *)device_get_softc(dev);
	struct bktr_i2c_softc *sc = &bktr_sc->i2c_sc;

	sc->smbus = device_add_child(dev, "smbus", -1);
	sc->iicbb = device_add_child(dev, "iicbb", -1);

	if (!sc->iicbb || !sc->smbus)
		return ENXIO;

	bus_generic_attach(dev);

	return (0);
};

int bt848_i2c_detach(device_t dev)
{
	struct bktr_softc *bktr_sc = (struct bktr_softc *)device_get_softc(dev);
	struct bktr_i2c_softc *sc = &bktr_sc->i2c_sc;
	int error = 0;

	if ((error = bus_generic_detach(dev)))
		goto error;

	if (sc->iicbb && (error = device_delete_child(dev, sc->iicbb)))
		goto error;

	if (sc->smbus && (error = device_delete_child(dev, sc->smbus)))
		goto error;

error:
	return (error);
}

int bti2c_smb_callback(device_t dev, int index, caddr_t *data)
{
	struct bktr_softc *bktr_sc = (struct bktr_softc *)device_get_softc(dev);
	struct bktr_i2c_softc *sc = &bktr_sc->i2c_sc;
	int error = 0;

	/* test each time if we already have/haven't the iicbus
	 * to avoid deadlocks
	 */
	switch (index) {
	case SMB_REQUEST_BUS:
		/* XXX test & set */
		if (!sc->bus_owned) {
			sc->bus_owned = 1;
		} else
			error = EWOULDBLOCK;
		break;

	case SMB_RELEASE_BUS:
		/* XXX test & set */
		if (sc->bus_owned) {
			sc->bus_owned = 0;
		} else
			error = EINVAL;
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

int bti2c_iic_callback(device_t dev, int index, caddr_t *data)
{
	struct bktr_softc *bktr_sc = (struct bktr_softc *)device_get_softc(dev);
	struct bktr_i2c_softc *sc = &bktr_sc->i2c_sc;
	int error = 0;

	/* test each time if we already have/haven't the smbus
	 * to avoid deadlocks
	 */
	switch (index) {
	case IIC_REQUEST_BUS:
		/* XXX test & set */
		if (!sc->bus_owned) {
			sc->bus_owned = 1;
		} else
			error = EWOULDBLOCK;
		break;

	case IIC_RELEASE_BUS:
		/* XXX test & set */
		if (sc->bus_owned) {
			sc->bus_owned = 0;
		} else
			error = EINVAL;
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

int bti2c_iic_reset(device_t dev, u_char speed, u_char addr, u_char * oldaddr)
{
	if (oldaddr)
		*oldaddr = 0;			/* XXX */

	return (IIC_ENOADDR);
}

void bti2c_iic_setsda(device_t dev, int val)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);
	int clock;

	clock = INL(sc, BKTR_I2C_DATA_CTL) & 0x2;

	if (val)
		OUTL(sc, BKTR_I2C_DATA_CTL, clock | 1);
	else
		OUTL(sc, BKTR_I2C_DATA_CTL, clock);

	DELAY(I2C_DELAY);

	return;
}

void bti2c_iic_setscl(device_t dev, int val)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);
	int data;

	data = INL(sc, BKTR_I2C_DATA_CTL) & 0x1;

	if (val)
		OUTL(sc, BKTR_I2C_DATA_CTL, 0x2 | data);
	else
		OUTL(sc, BKTR_I2C_DATA_CTL, data);

	DELAY(I2C_DELAY);

	return;
}

int
bti2c_iic_getsda(device_t dev)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);

	return (INL(sc,BKTR_I2C_DATA_CTL) & 0x1);
}

int
bti2c_iic_getscl(device_t dev)
{
	return (0);
}

static int
bti2c_write(struct bktr_softc *sc, u_long data)
{
	u_long		x;

	/* clear status bits */
	OUTL(sc, BKTR_INT_STAT, (BT848_INT_RACK | BT848_INT_I2CDONE));

	BTI2C_DEBUG(printf("w%lx", data));

	/* write the address and data */
	OUTL(sc, BKTR_I2C_DATA_CTL, data);

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( INL(sc, BKTR_INT_STAT) & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !( INL(sc, BKTR_INT_STAT) & BT848_INT_RACK) ) {
		BTI2C_DEBUG(printf("%c%c", (!x)?'+':'-',
			(!( INL(sc, BKTR_INT_STAT) & BT848_INT_RACK))?'+':'-'));
		return (SMB_ENOACK);
	}
	BTI2C_DEBUG(printf("+"));

	/* return OK */
	return( 0 );
}

int
bti2c_smb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);
	u_long data;

	data = ((slave & 0xff) << 24) | ((byte & 0xff) << 16) | (u_char)cmd;

	return (bti2c_write(sc, data));
}

/*
 * byte1 becomes low byte of word
 * byte2 becomes high byte of word
 */
int
bti2c_smb_writew(device_t dev, u_char slave, char cmd, short word)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);
	u_long data;
	char low, high;

	low = (char)(word & 0xff);
	high = (char)((word & 0xff00) >> 8);

	data = ((slave & 0xff) << 24) | ((low & 0xff) << 16) |
		((high & 0xff) << 8) | BT848_DATA_CTL_I2CW3B | (u_char)cmd;

	return (bti2c_write(sc, data));
}

/*
 * The Bt878 and Bt879 differed on the treatment of i2c commands
 */
int
bti2c_smb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	struct bktr_softc *sc  = (struct bktr_softc *)device_get_softc(dev);
	u_long		x;

	/* clear status bits */
	OUTL(sc,BKTR_INT_STAT, (BT848_INT_RACK | BT848_INT_I2CDONE));

	OUTL(sc,BKTR_I2C_DATA_CTL, ((slave & 0xff) << 24) | (u_char)cmd);;

	BTI2C_DEBUG(printf("r%lx/", (u_long)(((slave & 0xff) << 24) | (u_char)cmd)));

	/* wait for completion */
	for ( x = 0x7fffffff; x; --x ) {	/* safety valve */
		if ( INL(sc,BKTR_INT_STAT) & BT848_INT_I2CDONE )
			break;
	}

	/* check for ACK */
	if ( !x || !(INL(sc,BKTR_INT_STAT) & BT848_INT_RACK) ) {
		BTI2C_DEBUG(printf("r%c%c", (!x)?'+':'-',
			(!( INL(sc,BKTR_INT_STAT) & BT848_INT_RACK))?'+':'-'));
		return (SMB_ENOACK);
	}

	*byte = (char)((INL(sc,BKTR_I2C_DATA_CTL) >> 8) & 0xff);
	BTI2C_DEBUG(printf("r%x+", *byte));

	return (0);
}

#endif /* defined(BKTR_USE_FREEBSD_SMBUS) */
