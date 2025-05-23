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
 * $FreeBSD: src/sys/dev/aic/aic_pccard.c,v 1.1 2000/01/14 23:42:36 imp Exp $
 * $DragonFly: src/sys/dev/disk/aic/aic_pccard.c,v 1.5 2004/02/19 15:48:26 joerg Exp $
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
 
#include "aic6360reg.h"
#include "aicvar.h"

#include <bus/pccard/pccardvar.h>
#include <bus/pccard/pccarddevs.h>

#include "card_if.h"

struct aic_pccard_softc {
	struct	aic_softc sc_aic;
	struct	resource *sc_port;
	struct	resource *sc_irq;
	void	*sc_ih;
};

static int aic_pccard_alloc_resources (device_t);
static void aic_pccard_release_resources (device_t);
static int aic_pccard_match(device_t);
static int aic_pccard_probe (device_t);
static int aic_pccard_attach (device_t);

static const struct pccard_product aic_pccard_products[] = {
	PCMCIA_CARD(ADAPTEC, APA1460, 0),
	PCMCIA_CARD(ADAPTEC, APA1460A, 0),
	PCMCIA_CARD(NEWMEDIA, BUSTOASTER, 0),
	PCMCIA_CARD(NEWMEDIA, BUSTOASTER2, 0),
	PCMCIA_CARD(NEWMEDIA, BUSTOASTER3, 0),
	{ NULL }
};

#define	AIC_PCCARD_PORTSIZE 0x20

static int
aic_pccard_alloc_resources(device_t dev)
{
	struct aic_pccard_softc *sc = device_get_softc(dev);
	int rid;

	sc->sc_port = sc->sc_irq = 0;

	rid = 0;
	sc->sc_port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
	    0ul, ~0ul, AIC_PCCARD_PORTSIZE, RF_ACTIVE);
	if (!sc->sc_port)
		return (ENOMEM);

	rid = 0;
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    0ul, ~0ul, 1, RF_ACTIVE);
	if (!sc->sc_irq) {
		aic_pccard_release_resources(dev);
		return (ENOMEM);
	}

	sc->sc_aic.unit = device_get_unit(dev);
	sc->sc_aic.tag = rman_get_bustag(sc->sc_port);
	sc->sc_aic.bsh = rman_get_bushandle(sc->sc_port);
	return (0);
}

static void
aic_pccard_release_resources(device_t dev)
{
	struct aic_pccard_softc *sc = device_get_softc(dev);

	if (sc->sc_port)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->sc_port);
	if (sc->sc_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_irq);
	sc->sc_port = sc->sc_irq = 0;
}

static int
aic_pccard_match(device_t dev)
{
	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, aic_pccard_products,
	    sizeof(aic_pccard_products[0]), NULL)) != NULL) {
		if (pp->pp_name != NULL)
			device_set_desc(dev, pp->pp_name);
		return 0;
	}
	return ENXIO;
}

static int
aic_pccard_probe(device_t dev)
{
	struct aic_pccard_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;

	if (aic_pccard_alloc_resources(dev))
		return (ENXIO);
	if (aic_probe(aic)) {
		aic_pccard_release_resources(dev);
		return (ENXIO);
	}
	aic_pccard_release_resources(dev);

	device_set_desc(dev, "Adaptec 6260/6360 SCSI controller");
	return (0);
}

static int
aic_pccard_attach(device_t dev)
{
	struct aic_pccard_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;
	int error;

	error = aic_pccard_alloc_resources(dev);
	if (error) {
		device_printf(dev, "resource allocation failed\n");
		return (error);
	}

	error = aic_attach(aic);
	if (error) {
		device_printf(dev, "attach failed\n");
		aic_pccard_release_resources(dev);
		return (error);
	}

	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_CAM, aic_intr,
	    aic, &sc->sc_ih);
	if (error) {
		device_printf(dev, "failed to register interrupt handler\n");
		aic_pccard_release_resources(dev);
		return (error);
	}
	return (0);
}

static int
aic_pccard_detach(device_t dev)
{
	struct aic_pccard_softc *sc = device_get_softc(dev);
	struct aic_softc *aic = &sc->sc_aic;
	int error;

	error = bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
	if (error) {
		device_printf(dev, "failed to unregister interrupt handler\n");
	}

	error = aic_detach(aic);
	if (error) {
		device_printf(dev, "detach failed\n");
		return (error);
	}

	aic_pccard_release_resources(dev);
	return (0);
}

static device_method_t aic_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	aic_pccard_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	aic_pccard_match),
	DEVMETHOD(card_compat_probe,	aic_pccard_probe),
	DEVMETHOD(card_compat_attach,	aic_pccard_attach),
	{ 0, 0 }
};

static driver_t aic_pccard_driver = {
	"aic",
	aic_pccard_methods, sizeof(struct aic_pccard_softc),
};

extern devclass_t aic_devclass;

DRIVER_MODULE(aic, pccard, aic_pccard_driver, aic_devclass, 0, 0);
