/*-
 * Copyright (c) 1998 Nicolas Souchu
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
 * $FreeBSD: src/sys/dev/iicbus/iicsmb.c,v 1.5.2.2 2000/08/09 00:59:27 peter Exp $
 * $DragonFly: src/sys/bus/iicbus/iicsmb.c,v 1.4 2005/02/17 13:59:35 joerg Exp $
 *
 */

/*
 * I2C to SMB bridge
 *
 * Example:
 *
 *     smb bttv
 *       \ /
 *      smbus
 *       /  \
 *    iicsmb bti2c
 *       |
 *     iicbus
 *     /  |  \
 *  iicbb pcf ...
 *    |
 *  lpbb
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include "iiconf.h"
#include "iicbus.h"

#include <bus/smbus/smbconf.h>

#include "iicbus_if.h"
#include "smbus_if.h"

struct iicsmb_softc {

#define SMB_WAITING_ADDR	0x0
#define SMB_WAITING_LOW		0x1
#define SMB_WAITING_HIGH	0x2
#define SMB_DONE		0x3
	int state;

	u_char devaddr;			/* slave device address */

	char low;			/* low byte received first */
	char high;			/* high byte */

	device_t smbus;
};

static int iicsmb_probe(device_t);
static int iicsmb_attach(device_t);

static void iicsmb_intr(device_t dev, int event, char *buf);
static int iicsmb_callback(device_t dev, int index, caddr_t data);
static int iicsmb_quick(device_t dev, u_char slave, int how);
static int iicsmb_sendb(device_t dev, u_char slave, char byte);
static int iicsmb_recvb(device_t dev, u_char slave, char *byte);
static int iicsmb_writeb(device_t dev, u_char slave, char cmd, char byte);
static int iicsmb_writew(device_t dev, u_char slave, char cmd, short word);
static int iicsmb_readb(device_t dev, u_char slave, char cmd, char *byte);
static int iicsmb_readw(device_t dev, u_char slave, char cmd, short *word);
static int iicsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata);
static int iicsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf);
static int iicsmb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf);

static devclass_t iicsmb_devclass;

static device_method_t iicsmb_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		iicsmb_probe),
	DEVMETHOD(device_attach,	iicsmb_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* iicbus interface */
	DEVMETHOD(iicbus_intr,		iicsmb_intr),

	/* smbus interface */
	DEVMETHOD(smbus_callback,	iicsmb_callback),
	DEVMETHOD(smbus_quick,		iicsmb_quick),
	DEVMETHOD(smbus_sendb,		iicsmb_sendb),
	DEVMETHOD(smbus_recvb,		iicsmb_recvb),
	DEVMETHOD(smbus_writeb,		iicsmb_writeb),
	DEVMETHOD(smbus_writew,		iicsmb_writew),
	DEVMETHOD(smbus_readb,		iicsmb_readb),
	DEVMETHOD(smbus_readw,		iicsmb_readw),
	DEVMETHOD(smbus_pcall,		iicsmb_pcall),
	DEVMETHOD(smbus_bwrite,		iicsmb_bwrite),
	DEVMETHOD(smbus_bread,		iicsmb_bread),
	
	{ 0, 0 }
};

static driver_t iicsmb_driver = {
	"iicsmb",
	iicsmb_methods,
	sizeof(struct iicsmb_softc),
};

static int
iicsmb_probe(device_t dev)
{
	struct iicsmb_softc *sc = (struct iicsmb_softc *)device_get_softc(dev);

	sc->smbus = smbus_alloc_bus(dev);

	if (!sc->smbus)
		return (EINVAL);	/* XXX don't know what to return else */
		
	return (0);
}

static int
iicsmb_attach(device_t dev)
{
	struct iicsmb_softc *sc = (struct iicsmb_softc *)device_get_softc(dev);

	/* probe and attach the smbus */
	device_probe_and_attach(sc->smbus);

	return (0);
}

/*
 * iicsmb_intr()
 *
 * iicbus interrupt handler
 */
static void
iicsmb_intr(device_t dev, int event, char *buf)
{
	struct iicsmb_softc *sc = (struct iicsmb_softc *)device_get_softc(dev);

	switch (event) {
	case INTR_GENERAL:
	case INTR_START:
		sc->state = SMB_WAITING_ADDR;
		break;

	case INTR_STOP:
		/* call smbus intr handler */
		smbus_intr(sc->smbus, sc->devaddr,
				sc->low, sc->high, SMB_ENOERR);
		break;

	case INTR_RECEIVE:
		switch (sc->state) {
		case SMB_DONE:
			/* XXX too much data, discard */
			printf("%s: too much data from 0x%x\n", __func__,
				sc->devaddr & 0xff);
			goto end;

		case SMB_WAITING_ADDR:
			sc->devaddr = (u_char)*buf;
			sc->state = SMB_WAITING_LOW;
			break;

		case SMB_WAITING_LOW:
			sc->low = *buf;
			sc->state = SMB_WAITING_HIGH;
			break;

		case SMB_WAITING_HIGH:
			sc->high = *buf;
			sc->state = SMB_DONE;
			break;
		}
end:
		break;

	case INTR_TRANSMIT:
	case INTR_NOACK:
		break;

	case INTR_ERROR:
		switch (*buf) {
		case IIC_EBUSERR:
			smbus_intr(sc->smbus, sc->devaddr, 0, 0, SMB_EBUSERR);
			break;

		default:
			printf("%s unknown error 0x%x!\n", __func__,
								(int)*buf);
			break;
		}
		break;

	default:
		panic("%s: unknown event (%d)!", __func__, event);
	}

	return;
}

static int
iicsmb_callback(device_t dev, int index, caddr_t data)
{
	device_t parent = device_get_parent(dev);
	int error = 0;
	int how;

	switch (index) {
	case SMB_REQUEST_BUS:
		/* request underlying iicbus */
		how = *(int *)data;
		error = iicbus_request_bus(parent, dev, how);
		break;

	case SMB_RELEASE_BUS:
		/* release underlying iicbus */
		error = iicbus_release_bus(parent, dev);
		break;

	default:
		error = EINVAL;
	}

	return (error);
}

static int
iicsmb_quick(device_t dev, u_char slave, int how)
{
	device_t parent = device_get_parent(dev);
	int error;

	switch (how) {
	case SMB_QWRITE:
		error = iicbus_start(parent, slave & ~LSB, 0);
		break;

	case SMB_QREAD:
		error = iicbus_start(parent, slave | LSB, 0);
		break;

	default:
		error = EINVAL;
		break;
	}

	if (!error)
		error = iicbus_stop(parent);

	return (error);
}

static int
iicsmb_sendb(device_t dev, u_char slave, char byte)
{
	device_t parent = device_get_parent(dev);
	int error, sent;

	error = iicbus_start(parent, slave & ~LSB, 0);

	if (!error) {
		error = iicbus_write(parent, &byte, 1, &sent, 0);

		iicbus_stop(parent);
	}

	return (error);
}

static int
iicsmb_recvb(device_t dev, u_char slave, char *byte)
{
	device_t parent = device_get_parent(dev);
	int error, read;

	error = iicbus_start(parent, slave | LSB, 0);

	if (!error) {
		error = iicbus_read(parent, byte, 1, &read, IIC_LAST_READ, 0);

		iicbus_stop(parent);
	}

	return (error);
}

static int
iicsmb_writeb(device_t dev, u_char slave, char cmd, char byte)
{
	device_t parent = device_get_parent(dev);
	int error, sent;

	error = iicbus_start(parent, slave & ~LSB, 0);

	if (!error) {
		if (!(error = iicbus_write(parent, &cmd, 1, &sent, 0)))
			error = iicbus_write(parent, &byte, 1, &sent, 0);

		iicbus_stop(parent);
	}

	return (error);
}

static int
iicsmb_writew(device_t dev, u_char slave, char cmd, short word)
{
	device_t parent = device_get_parent(dev);
	int error, sent;

	char low = (char)(word & 0xff);
	char high = (char)((word & 0xff00) >> 8);

	error = iicbus_start(parent, slave & ~LSB, 0);

	if (!error) {
		if (!(error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		  if (!(error = iicbus_write(parent, &low, 1, &sent, 0)))
		    error = iicbus_write(parent, &high, 1, &sent, 0);

		iicbus_stop(parent);
	}

	return (error);
}

static int
iicsmb_readb(device_t dev, u_char slave, char cmd, char *byte)
{
	device_t parent = device_get_parent(dev);
	int error, sent, read;

	if ((error = iicbus_start(parent, slave & ~LSB, 0)))
		return (error);

	if ((error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		goto error;

	if ((error = iicbus_repeated_start(parent, slave | LSB, 0)))
		goto error;

	if ((error = iicbus_read(parent, byte, 1, &read, IIC_LAST_READ, 0)))
		goto error;

error:
	iicbus_stop(parent);
	return (error);
}

#define BUF2SHORT(low,high) \
	((short)(((high) & 0xff) << 8) | (short)((low) & 0xff))

static int
iicsmb_readw(device_t dev, u_char slave, char cmd, short *word)
{
	device_t parent = device_get_parent(dev);
	int error, sent, read;
	char buf[2];

	if ((error = iicbus_start(parent, slave & ~LSB, 0)))
		return (error);

	if ((error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		goto error;

	if ((error = iicbus_repeated_start(parent, slave | LSB, 0)))
		goto error;

	if ((error = iicbus_read(parent, buf, 2, &read, IIC_LAST_READ, 0)))
		goto error;

	/* first, receive low, then high byte */
	*word = BUF2SHORT(buf[0], buf[1]);

error:
	iicbus_stop(parent);
	return (error);
}

static int
iicsmb_pcall(device_t dev, u_char slave, char cmd, short sdata, short *rdata)
{
	device_t parent = device_get_parent(dev);
	int error, sent, read;
	char buf[2];

	if ((error = iicbus_start(parent, slave & ~LSB, 0)))
		return (error);

	if ((error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		goto error;

	/* first, send low, then high byte */
	buf[0] = (char)(sdata & 0xff);
	buf[1] = (char)((sdata & 0xff00) >> 8);

	if ((error = iicbus_write(parent, buf, 2, &sent, 0)))
		goto error;

	if ((error = iicbus_repeated_start(parent, slave | LSB, 0)))
		goto error;

	if ((error = iicbus_read(parent, buf, 2, &read, IIC_LAST_READ, 0)))
		goto error;

	/* first, receive low, then high byte */
	*rdata = BUF2SHORT(buf[0], buf[1]);

error:
	iicbus_stop(parent);
	return (error);
}

static int
iicsmb_bwrite(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	device_t parent = device_get_parent(dev);
	int error, sent;

	if ((error = iicbus_start(parent, slave & ~LSB, 0)))
		goto error;

	if ((error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		goto error;

	if ((error = iicbus_write(parent, buf, (int)count, &sent, 0)))
		goto error;

	if ((error = iicbus_stop(parent)))
		goto error;

error:
	return (error);
}

static int
iicsmb_bread(device_t dev, u_char slave, char cmd, u_char count, char *buf)
{
	device_t parent = device_get_parent(dev);
	int error, sent, read;

	if ((error = iicbus_start(parent, slave & ~LSB, 0)))
		return (error);

	if ((error = iicbus_write(parent, &cmd, 1, &sent, 0)))
		goto error;

	if ((error = iicbus_repeated_start(parent, slave | LSB, 0)))
		goto error;

	if ((error = iicbus_read(parent, buf, (int)count, &read,
							IIC_LAST_READ, 0)))
		goto error;

error:
	iicbus_stop(parent);
	return (error);
}

DRIVER_MODULE(iicsmb, iicbus, iicsmb_driver, iicsmb_devclass, 0, 0);
