/*
 * Copyright (c) 1997, 1998, 1999
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
 * $FreeBSD: src/sys/dev/an/if_an_isa.c,v 1.1.2.5 2003/02/01 03:25:12 ambrisko Exp $
 * $DragonFly: src/sys/dev/netif/an/if_an_isa.c,v 1.6 2005/02/21 18:40:36 joerg Exp $
 */

/*
 * Aironet 4500/4800 802.11 PCMCIA/ISA/PCI driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include "opt_inet.h"
#ifdef INET
#define ANCACHE
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <bus/isa/isavar.h>
#include <bus/isa/pnpvar.h>

#include "if_aironet_ieee.h"
#include "if_anreg.h"

static struct isa_pnp_id an_ids[] = {
	{ 0x0100ec06, "Aironet ISA4500/ISA4800" },
	{ 0, NULL }
};

static int an_probe_isa		(device_t);
static int an_attach_isa	(device_t);
static int an_detach_isa	(device_t);

static int
an_probe_isa(dev)
	device_t		dev;
{
	int			error = 0;

	error = ISA_PNP_PROBE(device_get_parent(dev), dev, an_ids);
	if (error == ENXIO)
		return(error);

	error = an_probe(dev);
	an_release_resources(dev);
	if (error == 0)
		return (ENXIO);

	error = an_alloc_irq(dev, 0, 0);
	an_release_resources(dev);
	if (!error)
		device_set_desc(dev, "Aironet ISA4500/ISA4800");
	return (error);
}

static int
an_attach_isa(dev)
	device_t dev;
{
	struct an_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error;

	an_alloc_port(dev, sc->port_rid, 1);
	an_alloc_irq(dev, sc->irq_rid, 0);

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       an_intr, sc, &sc->irq_handle);
	if (error) {
		an_release_resources(dev);
		return (error);
	}

	sc->an_bhandle = rman_get_bushandle(sc->port_res);
	sc->an_btag = rman_get_bustag(sc->port_res);
	sc->an_dev = dev;

	return an_attach(sc, device_get_unit(dev), flags);
}

static int
an_detach_isa(device_t dev)
{
	struct an_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	an_stop(sc);
	ifmedia_removeall(&sc->an_ifmedia);
	ether_ifdetach(ifp);
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	an_release_resources(dev);

	return (0);
}

static device_method_t an_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		an_probe_isa),
	DEVMETHOD(device_attach,	an_attach_isa),
	DEVMETHOD(device_detach,	an_detach_isa),
	DEVMETHOD(device_shutdown,	an_shutdown),
	{ 0, 0 }
};

static driver_t an_isa_driver = {
	"an",
	an_isa_methods,
	sizeof(struct an_softc)
};

static devclass_t an_isa_devclass;

DRIVER_MODULE(if_an, isa, an_isa_driver, an_isa_devclass, 0, 0);
