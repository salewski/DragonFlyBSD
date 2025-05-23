/*
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 * $FreeBSD: src/sys/dev/if_ndis/if_ndis_pci.c,v 1.7 2004/07/11 00:19:30 wpaul Exp $
 * $DragonFly: src/sys/dev/netif/ndis/if_ndis_pci.c,v 1.2 2004/10/14 18:31:02 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <netproto/802_11/ieee80211_var.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

#include <emulation/ndis/regcall.h>
#include <emulation/ndis/pe_var.h>
#include <emulation/ndis/resource_var.h>
#include <emulation/ndis/ntoskrnl_var.h>
#include <emulation/ndis/ndis_var.h>
#include <emulation/ndis/cfg_var.h>
#include "if_ndisvar.h"

#include "ndis_driver_data.h"

#ifdef NDIS_PCI_DEV_TABLE 

MODULE_DEPEND(ndis, pci, 1, 1, 1);
MODULE_DEPEND(ndis, wlan, 1, 1, 1);
MODULE_DEPEND(ndis, ndisapi, 1, 1, 1);

/*
 * Various supported device vendors/types and their names.
 * These are defined in the ndis_driver_data.h file.
 */
static struct ndis_pci_type ndis_devs[] = {
#ifdef NDIS_PCI_DEV_TABLE
	NDIS_PCI_DEV_TABLE
#endif
	{ 0, 0, 0, NULL }
};

static int ndis_probe_pci	(device_t);
static int ndis_attach_pci	(device_t);
static struct resource_list *ndis_get_resource_list
				(device_t, device_t);
extern int ndis_attach		(device_t);
extern int ndis_shutdown	(device_t);
extern int ndis_detach		(device_t);
extern int ndis_suspend		(device_t);
extern int ndis_resume		(device_t);

static device_method_t ndis_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ndis_probe_pci),
	DEVMETHOD(device_attach,	ndis_attach_pci),
	DEVMETHOD(device_detach,	ndis_detach),
	DEVMETHOD(device_shutdown,	ndis_shutdown),
	DEVMETHOD(device_suspend,	ndis_suspend),
	DEVMETHOD(device_resume,	ndis_resume),

	/* Bus interface */
	DEVMETHOD(bus_get_resource_list, ndis_get_resource_list),

	{ 0, 0 }
};

static driver_t ndis_driver = {
#ifdef NDIS_DEVNAME
	NDIS_DEVNAME,
#else
	"ndis",
#endif
	ndis_methods,
	sizeof(struct ndis_softc)
};

static devclass_t ndis_devclass;

#ifdef NDIS_MODNAME
#define NDIS_MODNAME_OVERRIDE_PCI(x)					\
	DRIVER_MODULE(x, pci, ndis_driver, ndis_devclass, 0, 0)
#define NDIS_MODNAME_OVERRIDE_CARDBUS(x)				\
	DRIVER_MODULE(x, cardbus, ndis_driver, ndis_devclass, 0, 0)
NDIS_MODNAME_OVERRIDE_PCI(NDIS_MODNAME);
NDIS_MODNAME_OVERRIDE_CARDBUS(NDIS_MODNAME);
#else
DRIVER_MODULE(ndis, pci, ndis_driver, ndis_devclass, 0, 0);
DRIVER_MODULE(ndis, cardbus, ndis_driver, ndis_devclass, 0, 0);
#endif

/*
 * Probe for an NDIS device. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
ndis_probe_pci(dev)
	device_t		dev;
{
	struct ndis_pci_type	*t;

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did) &&
		    ((pci_read_config(dev, PCIR_SUBVEND_0, 4) ==
		    t->ndis_subsys) || t->ndis_subsys == 0)) {
			device_set_desc(dev, t->ndis_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
ndis_attach_pci(dev)
	device_t		dev;
{
	struct ndis_softc	*sc;
	int			unit, error = 0, rid;
	struct ndis_pci_type	*t;
	int			devidx = 0, defidx = 0;
	struct resource_list	*rl;
	struct resource_list_entry	*rle;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->ndis_dev = dev;

	/*
	 * Map control/status registers.
	 */

	pci_enable_busmaster(dev);

	rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
	if (rl != NULL) {
		SLIST_FOREACH(rle, rl, link) {
			switch (rle->type) {
			case SYS_RES_IOPORT:
				sc->ndis_io_rid = rle->rid;
				sc->ndis_res_io = bus_alloc_resource(dev,
				    SYS_RES_IOPORT, &sc->ndis_io_rid,
				    0, ~0, 1, RF_ACTIVE);
				if (sc->ndis_res_io == NULL) {
					device_printf(dev,
					    "couldn't map iospace\n");
					error = ENXIO;
					goto fail;
				}
				break;
			case SYS_RES_MEMORY:
				if (sc->ndis_res_altmem != NULL &&
				    sc->ndis_res_mem != NULL) {
					device_printf(dev,
					    "too many memory resources\n");
					error = ENXIO;
					goto fail;
				}
				if (rle->rid == PCIR_BAR(2)) {
					sc->ndis_altmem_rid = rle->rid;
					sc->ndis_res_altmem =
					    bus_alloc_resource(dev,
					        SYS_RES_MEMORY,
						&sc->ndis_altmem_rid,
						0, ~0, 1, RF_ACTIVE);
					if (sc->ndis_res_altmem == NULL) {
						device_printf(dev,
						    "couldn't map alt "
						    "memory\n");
						error = ENXIO;
						goto fail;
					}
				} else {
					sc->ndis_mem_rid = rle->rid;
					sc->ndis_res_mem =
					    bus_alloc_resource(dev,
					        SYS_RES_MEMORY,
						&sc->ndis_mem_rid,
						0, ~0, 1, RF_ACTIVE);
					if (sc->ndis_res_mem == NULL) {
						device_printf(dev,
						    "couldn't map memory\n");
						error = ENXIO;
						goto fail;
					}
				}
				break;
			case SYS_RES_IRQ:
				rid = rle->rid;
				sc->ndis_irq = bus_alloc_resource(dev,
				    SYS_RES_IRQ, &rid, 0, ~0, 1,
	    			    RF_SHAREABLE | RF_ACTIVE);
				if (sc->ndis_irq == NULL) {
					device_printf(dev,
					    "couldn't map interrupt\n");
					error = ENXIO;
					goto fail;
				}
				break;
			default:
				break;
			}
			sc->ndis_rescnt++;
		}
	}

	/*
	 * If the BIOS did not set up an interrupt for this device,
	 * the resource traversal code above will fail to set up
	 * an IRQ resource. This is usually a bad thing, so try to
	 * force the allocation of an interrupt here. If one was
	 * not assigned to us by the BIOS, bus_alloc_resource()
	 * should route one for us.
	 */
	if (sc->ndis_irq == NULL) {
		rid = 0;
		sc->ndis_irq = bus_alloc_resource(dev, SYS_RES_IRQ,
		    &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
		if (sc->ndis_irq == NULL) {
			device_printf(dev, "couldn't route interrupt\n");
			error = ENXIO;
			goto fail;
		}
		sc->ndis_rescnt++;
	}

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define NDIS_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
                        BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, NDIS_NSEG_NEW,/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,       /* flags */
			&sc->ndis_parent_tag);

        if (error)
                goto fail;

	sc->ndis_iftype = PCIBus;

	/* Figure out exactly which device we matched. */

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did)) {
			if (t->ndis_subsys == 0)
				defidx = devidx;
			else {
				if (t->ndis_subsys ==
				    pci_read_config(dev, PCIR_SUBVEND_0, 4))
					break;
			}
		}
		t++;
		devidx++;
	}

	if (ndis_devs[devidx].ndis_name == NULL)
		sc->ndis_devidx = defidx;
	else
		sc->ndis_devidx = devidx;

	error = ndis_attach(dev);

fail:
	return(error);
}

static struct resource_list *
ndis_get_resource_list(dev, child)
	device_t		dev;
	device_t		child;
{
	struct ndis_softc	*sc;

	sc = device_get_softc(dev);
	return (BUS_GET_RESOURCE_LIST(device_get_parent(sc->ndis_dev), dev));
}

#endif /* NDIS_PCI_DEV_TABLE */
