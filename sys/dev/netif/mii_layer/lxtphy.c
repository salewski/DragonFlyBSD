/*	OpenBSD: lxtphy.c,v 1.5 2000/08/26 20:04:17 nate Exp 	*/
/*	NetBSD: lxtphy.c,v 1.19 2000/02/02 23:34:57 thorpej Exp 	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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
 * $FreeBSD: src/sys/dev/mii/lxtphy.c,v 1.1.2.1 2001/06/08 19:58:33 semenu Exp $
 * $DragonFly: src/sys/dev/netif/mii_layer/lxtphy.c,v 1.5 2004/09/18 19:32:59 dillon Exp $
 */
 
/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * driver for Level One's LXT-970 ethernet 10/100 PHY
 * datasheet from www.level1.com
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include "mii.h"
#include "miivar.h"
#include "miidevs.h"

#include "lxtphyreg.h"

#include "miibus_if.h"

static int lxtphy_probe		(device_t);
static int lxtphy_attach	(device_t);
static int lxtphy_detach	(device_t);

static device_method_t lxtphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		lxtphy_probe),
	DEVMETHOD(device_attach,	lxtphy_attach),
	DEVMETHOD(device_detach,	lxtphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t lxtphy_devclass;

static driver_t lxtphy_driver = {
	"lxtphy",
	lxtphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(lxtphy, miibus, lxtphy_driver, lxtphy_devclass, 0, 0);

static int	lxtphy_service (struct mii_softc *, struct mii_data *, int);
static void	lxtphy_status (struct mii_softc *);
static void	lxtphy_set_tp (struct mii_softc *);
static void	lxtphy_set_fx (struct mii_softc *);

static int lxtphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxLEVEL1 &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxLEVEL1_LXT970) {
		device_set_desc(dev, MII_STR_xxLEVEL1_LXT970);
	} else
		return (ENXIO);

	return (0);
}

static int lxtphy_attach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	mii_softc_init(sc);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = lxtphy_service;
	sc->mii_pdata = mii;
	sc->mii_flags |= MIIF_NOISOLATE;

	mii_phy_reset(sc);

	mii->mii_instance++;

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, 0, sc->mii_inst),
	    BMCR_S100);
	printf("100baseFX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_FX, IFM_FDX, sc->mii_inst),
	    BMCR_S100|BMCR_FDX);
	printf("100baseFX-FDX, ");
#undef ADD

	if (sc->mii_capabilities & BMSR_MEDIAMASK)
		mii_add_media(mii, sc->mii_capabilities,
		    sc->mii_inst);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}


static int lxtphy_detach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_parent(dev));
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}

static int
lxtphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);

			lxtphy_set_tp(sc);

			(void) mii_phy_auto(sc, 1);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);

		case IFM_100_FX:
			lxtphy_set_fx(sc);

		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, MII_BMSR) |
		    PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;
		mii_phy_reset(sc);
		if (mii_phy_auto(sc, 0) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	lxtphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

static void
lxtphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr, csr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	/*
	 * Get link status from the CSR; we need to read the CSR
	 * for media type anyhow, and the link status in the CSR
	 * doens't latch, so fewer register reads are required.
	 */
	csr = PHY_READ(sc, MII_LXTPHY_CSR);
	if (csr & CSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		if (csr & CSR_SPEED)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (csr & CSR_DUPLEX)
			mii->mii_media_active |= IFM_FDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
lxtphy_set_tp(sc)
	struct mii_softc *sc;
{
	int cfg;

	cfg = PHY_READ(sc, MII_LXTPHY_CONFIG);
	cfg &= ~CONFIG_100BASEFX;
	PHY_WRITE(sc, MII_LXTPHY_CONFIG, cfg);
}

static void
lxtphy_set_fx(sc)
	struct mii_softc *sc;
{
	int cfg;

	cfg = PHY_READ(sc, MII_LXTPHY_CONFIG);
	cfg |= CONFIG_100BASEFX;
	PHY_WRITE(sc, MII_LXTPHY_CONFIG, cfg);
}

