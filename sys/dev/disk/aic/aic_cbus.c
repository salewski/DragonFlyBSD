/*-
 * Copyright (c) 1999 Luoqi Chen.
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
 * $FreeBSD: src/sys/dev/aic/aic_cbus.c,v 1.1.2.2 2000/06/21 09:37:09 nyan Exp $
 * $DragonFly: src/sys/dev/disk/aic/Attic/aic_cbus.c,v 1.4 2003/08/27 10:35:16 rob Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include <bus/isa/isavar.h>
#include "aic6360reg.h"
#include "aicvar.h"
  
struct aic_isa_softc {
	struct	aic_softc sc_aic;
	struct	resource *sc_port;
	struct	resource *sc_irq;
	struct	resource *sc_drq;
	void	*sc_ih;
};

static int aic_isa_alloc_resources (device_t);
static void aic_isa_release_resources (device_t);
static int aic_isa_probe (device_t);
static int aic_isa_attach (device_t);

#ifdef PC98
static u_int aic_isa_ports[] = { 0x1840 };
#else
static u_int aic_isa_ports[] = { 0x340, 0x140 };
#endif
#define	AIC_ISA_NUMPORTS (sizeof(aic_isa_ports) / sizeof(aic_isa_ports[0]))
#define	AIC_ISA_PORTSIZE 0x20

#ifdef PC98
#define	AIC98_GENERIC		0x00
#define	AIC98_NEC100		0x01
#define	AIC_TYPE98(x)		(((x) >> 16) & 0x01)

static bus_addr_t aicport_generic[AIC_ISA_PORTSIZE] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
static bus_addr_t aicport_100[AIC_ISA_PORTSIZE] = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
	0x10, 0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e,
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e,
	0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c, 0x3e,
};

static struct isa_pnp_id aic_ids[] = {
        { 0xa180a3b8,   "NEC PC9801-100"},
        {0}
};
#endif

static int
aic_isa_alloc_resources(device_t dev)
{
	struct aic_isa_softc *sc = device_get_softc(dev);
	int rid;
#ifdef PC98
	bus_addr_t *bs_iat;

	if ((isa_get_logicalid(dev) == 0xa180a3b8) ||
	    (AIC_TYPE98(device_get_flags(dev)) == AIC98_NEC100))
		bs_iat = aicport_100;
	else
		bs_iat = aicport_generic;
#endif

	sc->sc_port = sc->sc_irq = sc->sc_drq = 0;

	rid = 0;
#ifdef PC98
	sc->sc_port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					  bs_iat, AIC_ISA_PORTSIZE, RF_ACTIVE);
#else
	sc->sc_port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					0ul, ~0ul, AIC_ISA_PORTSIZE, RF_ACTIVE);
#endif
	if (!sc->sc_port)
		return (ENOMEM);
#ifdef PC98
	isa_load_resourcev(sc->sc_port, bs_iat, AIC_ISA_PORTSIZE);
#endif

	if (isa_get_irq(dev) != -1) {
		rid = 0;
		sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
						0ul, ~0ul, 1, RF_ACTIVE);
		if (!sc->sc_irq) {
			aic_isa_release_resources(dev);
			return (ENOMEM);
		}
	}

	if (isa_get_drq(dev) != -1) {
		rid = 0;
		sc->sc_drq = bus_alloc_resource(dev, SYS_RES_DRQ, &rid,
						0ul, ~0ul, 1, RF_ACTIVE);
		if (!sc->sc_drq) {
			aic_isa_release_resources(dev);
			return (ENOMEM);
		}
	}

	sc->sc_aic.unit = device_get_unit(dev);
	sc->sc_aic.tag = rman_get_bustag(sc->sc_port);
	sc->sc_aic.bsh = rman_get_bushandle(sc->sc_port);
	return (0);
}

static void
aic_isa_release_resources(device_t dev)
{
	struct aic_isa_softc *sc = device_get_softc(dev);

	if (sc->sc_port)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->sc_port);
	if (sc->sc_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);
	if (sc->sc_drq)
		bus_release_resource(dev, SYS_RES_DRQ, 0, sc->sc_drq);
	sc->sc_port = sc->sc_irq = sc->sc_drq = 0;
}

static int
aic_isa_probe(device_t dev)
{
	struct aic_isa_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;
	int numports, i;
	u_int port, *ports;
	u_int8_t porta;

#ifdef PC98
	if (ISA_PNP_PROBE(device_get_parent(dev), dev, aic_ids) == ENXIO)
#else
	if (isa_get_vendorid(dev))
#endif
		return (ENXIO);

	port = isa_get_port(dev);
	if (port != -1) {
		ports = &port;
		numports = 1;
	} else {
		ports = aic_isa_ports;
		numports = AIC_ISA_NUMPORTS;
	}

	for (i = 0; i < numports; i++) {
#ifdef PC98
		if (bus_set_resource(dev, SYS_RES_IOPORT, 0, ports[i], 1))
			continue;
#else
		if (bus_set_resource(dev, SYS_RES_IOPORT, 0, ports[i],
				     AIC_ISA_PORTSIZE))
			continue;
#endif
		if (aic_isa_alloc_resources(dev))
			continue;
		if (!aic_probe(aic)) {
			aic_isa_release_resources(dev);
			break;
		}
		aic_isa_release_resources(dev);
	}

	if (i == numports)
		return (ENXIO);

	porta = aic_inb(aic, PORTA);
	if (isa_get_irq(dev) == -1)
		bus_set_resource(dev, SYS_RES_IRQ, 0, PORTA_IRQ(porta), 1);
	if ((aic->flags & AIC_DMA_ENABLE) && isa_get_drq(dev) == -1)
		bus_set_resource(dev, SYS_RES_DRQ, 0, PORTA_DRQ(porta), 1);
	device_set_desc(dev, "Adaptec 6260/6360 SCSI controller");
	return (0);
}

static int
aic_isa_attach(device_t dev)
{
	struct aic_isa_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;
	int error;

	error = aic_isa_alloc_resources(dev);
	if (error) {
		device_printf(dev, "resource allocation failed\n");
		return (error);
	}

	error = aic_attach(aic);
	if (error) {
		device_printf(dev, "attach failed\n");
		aic_isa_release_resources(dev);
		return (error);
	}

	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_CAM, aic_intr,
				aic, &sc->sc_ih);
	if (error) {
		device_printf(dev, "failed to register interrupt handler\n");
		aic_isa_release_resources(dev);
		return (error);
	}
	return (0);
}

static int
aic_isa_detach(device_t dev)
{
	struct aic_isa_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;
	int error;

	error = aic_detach(aic);
	if (error) {
		device_printf(dev, "detach failed\n");
		return (error);
	}

	error = bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
	if (error) {
		device_printf(dev, "failed to unregister interrupt handler\n");
	}

	aic_isa_release_resources(dev);
	return (0);
}

static device_method_t aic_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aic_isa_probe),
	DEVMETHOD(device_attach,	aic_isa_attach),
	DEVMETHOD(device_detach,	aic_isa_detach),
	{ 0, 0 }
};

static driver_t aic_isa_driver = {
	"aic",
	aic_isa_methods, sizeof(struct aic_isa_softc),
};

extern devclass_t aic_devclass;

DRIVER_MODULE(aic, isa, aic_isa_driver, aic_devclass, 0, 0);
