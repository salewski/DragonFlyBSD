/*	$NetBSD: mii.c,v 1.12 1999/08/03 19:41:49 drochner Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/mii/mii.c,v 1.6.2.2 2002/08/19 16:56:33 ambrisko Exp $
 * $DragonFly: src/sys/dev/netif/mii_layer/mii.c,v 1.6 2004/04/07 05:45:28 dillon Exp $
 */

/*
 * MII bus layer, glues MII-capable network interface drivers to sharable
 * PHY drivers.  This exports an interface compatible with BSD/OS 3.0's,
 * plus some NetBSD extensions.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h> 

#include <net/if.h>
#include <net/if_media.h>

#include "mii.h"
#include "miivar.h"

#include "miibus_if.h"

static int miibus_readreg	(device_t, int, int);
static int miibus_writereg	(device_t, int, int, int);
static void miibus_statchg	(device_t);
static void miibus_mediainit	(device_t);

static device_method_t miibus_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		miibus_probe),
	DEVMETHOD(device_attach,	miibus_attach),
	DEVMETHOD(device_detach,	miibus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	miibus_readreg),
	DEVMETHOD(miibus_writereg,	miibus_writereg),
	DEVMETHOD(miibus_statchg,	miibus_statchg),    
	DEVMETHOD(miibus_mediainit,	miibus_mediainit),    

	{ 0, 0 }
};

devclass_t miibus_devclass;

driver_t miibus_driver = {
	"miibus",
	miibus_methods,
	sizeof(struct mii_data)
};

/*
 * Helper function used by network interface drivers, attaches PHYs
 * to the network interface driver parent.
 */

int miibus_probe(dev)
	device_t		dev;
{
	struct mii_attach_args	ma, *args;
	struct mii_data		*mii;
	device_t		child = NULL, parent;
	int			bmsr, capmask = 0xFFFFFFFF;

	mii = device_get_softc(dev);
	parent = device_get_parent(dev);
	LIST_INIT(&mii->mii_phys);

	for (ma.mii_phyno = 0; ma.mii_phyno < MII_NPHY; ma.mii_phyno++) {
		/*
		 * Check to see if there is a PHY at this address.  Note,
		 * many braindead PHYs report 0/0 in their ID registers,
		 * so we test for media in the BMSR.
	 	 */
		bmsr = MIIBUS_READREG(parent, ma.mii_phyno, MII_BMSR);
		if (bmsr == 0 || bmsr == 0xffff ||
		    (bmsr & BMSR_MEDIAMASK) == 0) {
			/* Assume no PHY at this address. */
			continue;
		}

		/*
		 * Extract the IDs. Braindead PHYs will be handled by
		 * the `ukphy' driver, as we have no ID information to
		 * match on.
	 	 */
		ma.mii_id1 = MIIBUS_READREG(parent, ma.mii_phyno,
		    MII_PHYIDR1);
		ma.mii_id2 = MIIBUS_READREG(parent, ma.mii_phyno,
		    MII_PHYIDR2);

		ma.mii_data = mii;
		ma.mii_capmask = capmask;

		args = malloc(sizeof(struct mii_attach_args),
		    M_DEVBUF, M_INTWAIT);
		bcopy((char *)&ma, (char *)args, sizeof(ma));
		child = device_add_child(dev, NULL, -1);
		device_set_ivars(child, args);
	}

	if (child == NULL)
		return(ENXIO);

	device_set_desc(dev, "MII bus");

	return(0);
}

int miibus_attach(dev)
	device_t		dev;
{
	void			**v;
	ifm_change_cb_t		ifmedia_upd;
	ifm_stat_cb_t		ifmedia_sts;
	struct mii_data		*mii;

	mii = device_get_softc(dev);
	mii->mii_ifp = device_get_softc(device_get_parent(dev));
	v = device_get_ivars(dev);
	ifmedia_upd = v[0];
	ifmedia_sts = v[1];
	ifmedia_init(&mii->mii_media, IFM_IMASK, ifmedia_upd, ifmedia_sts);
	bus_generic_attach(dev);

	return(0);
}

int miibus_detach(dev)
	device_t		dev;
{
	struct mii_data		*mii;

	bus_generic_detach(dev);
	mii = device_get_softc(dev);
	ifmedia_removeall(&mii->mii_media);
	mii->mii_ifp = NULL;

	return(0);
}

static int miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	device_t		parent;

	parent = device_get_parent(dev);
	return(MIIBUS_READREG(parent, phy, reg));
}

static int miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	device_t		parent;

	parent = device_get_parent(dev);
	return(MIIBUS_WRITEREG(parent, phy, reg, data));
}

static void miibus_statchg(dev)
	device_t		dev;
{
	device_t		parent;

	parent = device_get_parent(dev);
	MIIBUS_STATCHG(parent);
	return;
}

static void miibus_mediainit(dev)
	device_t		dev;
{
	struct mii_data		*mii;
	struct ifmedia_entry	*m;
	int			media = 0;

	/* Poke the parent in case it has any media of its own to add. */
	MIIBUS_MEDIAINIT(device_get_parent(dev));

	mii = device_get_softc(dev);
	for (m = LIST_FIRST(&mii->mii_media.ifm_list); m != NULL;
	     m = LIST_NEXT(m, ifm_list)) {
		media = m->ifm_media;
		if (media == (IFM_ETHER|IFM_AUTO))
			break;
	}

	ifmedia_set(&mii->mii_media, media);

	return;
}

int mii_phy_probe(dev, child, ifmedia_upd, ifmedia_sts)
	device_t		dev;
	device_t		*child;
	ifm_change_cb_t		ifmedia_upd;
	ifm_stat_cb_t		ifmedia_sts;
{
	void			**v;
	int			bmsr, i;

	v = malloc(sizeof(vm_offset_t) * 2, M_DEVBUF, M_INTWAIT);
	v[0] = ifmedia_upd;
	v[1] = ifmedia_sts;
	*child = device_add_child(dev, "miibus", -1);
	device_set_ivars(*child, v);

	for (i = 0; i < MII_NPHY; i++) {
		bmsr = MIIBUS_READREG(dev, i, MII_BMSR);
                if (bmsr == 0 || bmsr == 0xffff ||
                    (bmsr & BMSR_MEDIAMASK) == 0) {
                        /* Assume no PHY at this address. */
                        continue;
                } else
			break;
	}

	if (i == MII_NPHY) {
		device_delete_child(dev, *child);
		*child = NULL;
		return(ENXIO);
	}

	bus_generic_attach(dev);

	return(0);
}

/*
 * Media changed; notify all PHYs.
 */
int
mii_mediachg(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;
	int rv;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list)) {
		rv = (*child->mii_service)(child, mii, MII_MEDIACHG);
		if (rv)
			return (rv);
	}
	return (0);
}

/*
 * Call the PHY tick routines, used during autonegotiation.
 */
void
mii_tick(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list))
		(void) (*child->mii_service)(child, mii, MII_TICK);
}

/*
 * Get media status from PHYs.
 */
void
mii_pollstat(mii)
	struct mii_data *mii;
{
	struct mii_softc *child;

	mii->mii_media_status = 0;
	mii->mii_media_active = IFM_NONE;

	for (child = LIST_FIRST(&mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list))
		(void) (*child->mii_service)(child, mii, MII_POLLSTAT);
}

static moduledata_t miibus_mod = { "miibus" };

DECLARE_MODULE(miibus, miibus_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);

