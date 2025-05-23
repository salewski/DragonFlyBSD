/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *   Connectivity Products:
 *	ABP902/3902	- Bus-Master PCI (16 CDB)
 *	ABP3905		- Bus-Master PCI (16 CDB)
 *	ABP915		- Bus-Master PCI (16 CDB)
 *	ABP920		- Bus-Master PCI (16 CDB)
 *	ABP3922		- Bus-Master PCI (16 CDB)
 *	ABP3925		- Bus-Master PCI (16 CDB)
 *	ABP930		- Bus-Master PCI (16 CDB) *
 *	ABP930U		- Bus-Master PCI Ultra (16 CDB)
 *	ABP930UA	- Bus-Master PCI Ultra (16 CDB)
 *	ABP960		- Bus-Master PCI MAC/PC (16 CDB) **
 *	ABP960U		- Bus-Master PCI MAC/PC (16 CDB) **
 *
 *   Single Channel Products:
 *	ABP940		- Bus-Master PCI (240 CDB)
 *	ABP940U		- Bus-Master PCI Ultra (240 CDB)
 *	ABP940UA/3940UA - Bus-Master PCI Ultra (240 CDB)
 *	ABP3960UA	- Bus-Master PCI MAC/PC (240 CDB)
 *	ABP970		- Bus-Master PCI MAC/PC (240 CDB)
 *	ABP970U		- Bus-Master PCI MAC/PC Ultra (240 CDB)
 *
 *   Dual Channel Products:  
 *	ABP950 - Dual Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980 - Four Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980U - Four Channel Bus-Master PCI Ultra (240 CDB Per Channel)
 *	ABP980UA/3980UA - Four Channel Bus-Master PCI Ultra (16 CDB Per Chan.)
 *
 *   Footnotes:
 *	 * This board has been sold by SIIG as the Fast SCSI Pro PCI.
 *	** This board has been sold by Iomega as a Jaz Jet PCI adapter. 
 *
 * Copyright (c) 1997 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/advansys/adv_pci.c,v 1.11.2.3 2001/06/02 04:38:10 nyan Exp $
 * $DragonFly: src/sys/dev/disk/advansys/adv_pci.c,v 1.3 2003/08/07 21:16:50 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

#include "advansys.h"

#define PCI_BASEADR0	PCIR_MAPS		/* I/O Address */
#define PCI_BASEADR1	PCIR_MAPS + 4		/* Mem I/O Address */

#define	PCI_DEVICE_ID_ADVANSYS_1200A	0x110010CD
#define	PCI_DEVICE_ID_ADVANSYS_1200B	0x120010CD
#define	PCI_DEVICE_ID_ADVANSYS_3000	0x130010CD
#define	PCI_DEVICE_REV_ADVANSYS_3150	0x02
#define	PCI_DEVICE_REV_ADVANSYS_3050	0x03

#define ADV_PCI_MAX_DMA_ADDR    (0xFFFFFFFFL)
#define ADV_PCI_MAX_DMA_COUNT   (0xFFFFFFFFL)

static int adv_pci_probe(device_t);
static int adv_pci_attach(device_t);

/* 
 * The overrun buffer shared amongst all PCI adapters.
 */
static  u_int8_t*	overrun_buf;
static	bus_dma_tag_t	overrun_dmat;
static	bus_dmamap_t	overrun_dmamap;
static	bus_addr_t	overrun_physbase;

static int
adv_pci_probe(device_t dev)
{
	int	rev = pci_get_revid(dev);

	switch (pci_get_devid(dev)) {
	case PCI_DEVICE_ID_ADVANSYS_1200A:
		device_set_desc(dev, "AdvanSys ASC1200A SCSI controller");
		return 0;
	case PCI_DEVICE_ID_ADVANSYS_1200B:
		device_set_desc(dev, "AdvanSys ASC1200B SCSI controller");
		return 0;
	case PCI_DEVICE_ID_ADVANSYS_3000:
		if (rev == PCI_DEVICE_REV_ADVANSYS_3150) {
			device_set_desc(dev,
					"AdvanSys ASC3150 SCSI controller");
			return 0;
		} else if (rev == PCI_DEVICE_REV_ADVANSYS_3050) {
			device_set_desc(dev,
					"AdvanSys ASC3030/50 SCSI controller");
			return 0;
		} else if (rev >= PCI_DEVICE_REV_ADVANSYS_3150) {
			device_set_desc(dev, "Unknown AdvanSys controller");
			return 0;
		}
		break;
	default:
		break;
	}
	return ENXIO;
}

static int
adv_pci_attach(device_t dev)
{
	struct		adv_softc *adv;
	u_int32_t	id;
	u_int32_t	command;
	int		error, rid, irqrid;
	void		*ih;
	struct resource	*iores, *irqres;

	/*
	 * Determine the chip version.
	 */
	id = pci_read_config(dev, PCIR_DEVVENDOR, /*bytes*/4);
	command = pci_read_config(dev, PCIR_COMMAND, /*bytes*/1);

	/*
	 * These cards do not allow memory mapped accesses, so we must
	 * ensure that I/O accesses are available or we won't be able
	 * to talk to them.
	 */
	if ((command & (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN))
	 != (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN)) {
		command |= PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN;
		pci_write_config(dev, PCIR_COMMAND, command, /*bytes*/1);
	}

	/*
	 * Early chips can't handle non-zero latency timer settings.
	 */
	if (id == PCI_DEVICE_ID_ADVANSYS_1200A
	 || id == PCI_DEVICE_ID_ADVANSYS_1200B) {
		pci_write_config(dev, PCIR_LATTIMER, /*value*/0, /*bytes*/1);
	}

	rid = PCI_BASEADR0;
	iores = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
				   RF_ACTIVE);
	if (iores == NULL)
		return ENXIO;

	if (adv_find_signature(rman_get_bustag(iores),
			       rman_get_bushandle(iores)) == 0) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
		return ENXIO;
	}

	adv = adv_alloc(dev, rman_get_bustag(iores), rman_get_bushandle(iores));
	if (adv == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
		return ENXIO;
	}

	/* Allocate a dmatag for our transfer DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   /*lowaddr*/ADV_PCI_MAX_DMA_ADDR,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/BUS_SPACE_MAXSIZE_32BIT,
				   /*nsegments*/BUS_SPACE_UNRESTRICTED,
				   /*maxsegsz*/ADV_PCI_MAX_DMA_COUNT,
				   /*flags*/0,
				   &adv->parent_dmat);
 
	if (error != 0) {
		printf("%s: Could not allocate DMA tag - error %d\n",
		       adv_name(adv), error);
		adv_free(adv);
		bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
		return ENXIO;
	}

	adv->init_level++;

	if (overrun_buf == NULL) {
		/* Need to allocate our overrun buffer */
		if (bus_dma_tag_create(adv->parent_dmat,
				       /*alignment*/8, /*boundary*/0,
				       ADV_PCI_MAX_DMA_ADDR, BUS_SPACE_MAXADDR,
				       /*filter*/NULL, /*filterarg*/NULL,
				       ADV_OVERRUN_BSIZE, /*nsegments*/1,
				       BUS_SPACE_MAXSIZE_32BIT, /*flags*/0,
				       &overrun_dmat) != 0) {
			bus_dma_tag_destroy(adv->parent_dmat);
			adv_free(adv);
			bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
			return ENXIO;
       		}
		if (bus_dmamem_alloc(overrun_dmat,
				     (void **)&overrun_buf,
				     BUS_DMA_NOWAIT,
				     &overrun_dmamap) != 0) {
			bus_dma_tag_destroy(overrun_dmat);
			bus_dma_tag_destroy(adv->parent_dmat);
			adv_free(adv);
			bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
			return ENXIO;
		}
		/* And permanently map it in */  
		bus_dmamap_load(overrun_dmat, overrun_dmamap,
				overrun_buf, ADV_OVERRUN_BSIZE,
				adv_map, &overrun_physbase,
				/*flags*/0);
	}

	adv->overrun_physbase = overrun_physbase;
			
	/*
	 * Stop the chip.
	 */
	ADV_OUTB(adv, ADV_CHIP_CTRL, ADV_CC_HALT);
	ADV_OUTW(adv, ADV_CHIP_STATUS, 0);

	adv->chip_version = ADV_INB(adv, ADV_NONEISA_CHIP_REVISION);
	adv->type = ADV_PCI;
	
	/*
	 * Setup active negation and signal filtering.
	 */
	{
		u_int8_t extra_cfg;

		if (adv->chip_version >= ADV_CHIP_VER_PCI_ULTRA_3150)
			adv->type |= ADV_ULTRA;
		if (adv->chip_version == ADV_CHIP_VER_PCI_ULTRA_3050)
			extra_cfg = ADV_IFC_ACT_NEG | ADV_IFC_WR_EN_FILTER;
		else
			extra_cfg = ADV_IFC_ACT_NEG | ADV_IFC_SLEW_RATE;
		ADV_OUTB(adv, ADV_REG_IFC, extra_cfg);
	}

	if (adv_init(adv) != 0) {
		adv_free(adv);
		bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
		return ENXIO;
	}

	adv->max_dma_count = ADV_PCI_MAX_DMA_COUNT;
	adv->max_dma_addr = ADV_PCI_MAX_DMA_ADDR;

#if CC_DISABLE_PCI_PARITY_INT
	{
		u_int16_t config_msw;

		config_msw = ADV_INW(adv, ADV_CONFIG_MSW);
		config_msw &= 0xFFC0;
		ADV_OUTW(adv, ADV_CONFIG_MSW, config_msw); 
	}
#endif
 
	if (id == PCI_DEVICE_ID_ADVANSYS_1200A
	 || id == PCI_DEVICE_ID_ADVANSYS_1200B) {
		adv->bug_fix_control |= ADV_BUG_FIX_IF_NOT_DWB;
		adv->bug_fix_control |= ADV_BUG_FIX_ASYN_USE_SYN;
		adv->fix_asyn_xfer = ~0;
	}

	irqrid = 0;
	irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &irqrid, 0, ~0, 1,
				    RF_SHAREABLE | RF_ACTIVE);
	if (irqres == NULL ||
	    bus_setup_intr(dev, irqres, INTR_TYPE_CAM, adv_intr, adv, &ih)) {
		adv_free(adv);
		bus_release_resource(dev, SYS_RES_IOPORT, rid, iores);
		return ENXIO;
	}

	adv_attach(adv);
	return 0;
}

static device_method_t adv_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adv_pci_probe),
	DEVMETHOD(device_attach,	adv_pci_attach),
	{ 0, 0 }
};

static driver_t adv_pci_driver = {
	"adv", adv_pci_methods, sizeof(struct adv_softc)
};

static devclass_t adv_pci_devclass;
DRIVER_MODULE(adv, pci, adv_pci_driver, adv_pci_devclass, 0, 0);
