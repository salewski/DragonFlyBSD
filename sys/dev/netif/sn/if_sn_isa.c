/*
 * Copyright (c) 1999 M. Warner Losh
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
 *    must display the following acknowledgement:
 *      This product includes software developed by Herb Peyerl.
 * 4. The name of Herb Peyerl may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/sn/if_sn_isa.c,v 1.3.2.1 2001/01/25 19:38:18 imp Exp $
 * $DragonFly: src/sys/dev/netif/sn/if_sn_isa.c,v 1.3 2003/08/07 21:17:05 dillon Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <net/ethernet.h> 
#include <net/if.h>
#include <net/if_arp.h>

#include <bus/isa/isavar.h>

#include "if_snvar.h"

static int		sn_isa_probe	(device_t);
static int		sn_isa_attach	(device_t);

static int
sn_isa_probe (device_t dev)
{
	if (isa_get_logicalid(dev))		/* skip PnP probes */
		return (ENXIO);
	if (sn_probe(dev, 0) != 0)
		return (ENXIO);
	return (0);
}

static int
sn_isa_attach (device_t dev)
{
 	struct sn_softc *sc = device_get_softc(dev);

 	sc->pccard_enaddr = 0;
	return (sn_attach(dev));
}

static device_method_t sn_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sn_isa_probe),
	DEVMETHOD(device_attach,	sn_isa_attach),

	{ 0, 0 }
};

static driver_t sn_isa_driver = {
	"sn",
	sn_isa_methods,
	sizeof(struct sn_softc),
};

extern devclass_t sn_devclass;

DRIVER_MODULE(if_sn, isa, sn_isa_driver, sn_devclass, 0, 0);
