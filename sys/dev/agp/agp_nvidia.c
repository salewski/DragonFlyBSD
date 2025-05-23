/*-
 * Copyright (c) 2003 Matthew N. Dodd <winter@jurai.net>
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
 * Based on FreeBSD v1.2.
 * $DragonFly: src/sys/dev/agp/agp_nvidia.c,v 1.3 2004/07/04 00:24:52 dillon Exp $
 */

/*
 * Written using information gleaned from the
 * NVIDIA nForce/nForce2 AGPGART Linux Kernel Patch.
 */

#include "opt_bus.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>

#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>
#include "agppriv.h"
#include "agpreg.h"

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#define	NVIDIA_VENDORID		0x10de
#define	NVIDIA_DEVICEID_NFORCE	0x01a4
#define	NVIDIA_DEVICEID_NFORCE2	0x01e0

struct agp_nvidia_softc {
	struct agp_softc	agp;
	u_int32_t		initial_aperture; /* aperture size at startup */
	struct agp_gatt *	gatt;

	device_t		dev;		/* AGP Controller */
	device_t		mc1_dev;	/* Memory Controller */
	device_t		mc2_dev;	/* Memory Controller */
	device_t		bdev;		/* Bridge */

	u_int32_t		wbc_mask;
	int			num_dirs;
	int			num_active_entries;
	off_t			pg_offset;
};

static const char *	agp_nvidia_match	(device_t dev);
static int		agp_nvidia_probe	(device_t);
static int		agp_nvidia_attach	(device_t);
static int		agp_nvidia_detach	(device_t);
static u_int32_t	agp_nvidia_get_aperture	(device_t);
static int		agp_nvidia_set_aperture	(device_t, u_int32_t);
static int		agp_nvidia_bind_page	(device_t, int, vm_offset_t);
static int		agp_nvidia_unbind_page	(device_t, int);

static int		nvidia_init_iorr	(u_int32_t, u_int32_t);

static const char *
agp_nvidia_match (device_t dev)
{
	if (pci_get_class(dev) != PCIC_BRIDGE ||
	    pci_get_subclass(dev) != PCIS_BRIDGE_HOST ||
	    pci_get_vendor(dev) != NVIDIA_VENDORID)
		return (NULL);

	switch (pci_get_device(dev)) {
	case NVIDIA_DEVICEID_NFORCE:
		return ("NVIDIA nForce AGP Controller");
	case NVIDIA_DEVICEID_NFORCE2:
		return ("NVIDIA nForce2 AGP Controller");
	}
	return ("NVIDIA Generic AGP Controller");
}

static int
agp_nvidia_probe (device_t dev)
{
	const char *desc;

	desc = agp_nvidia_match(dev);
	if (desc) {
		device_verbose(dev);
		device_set_desc(dev, desc);
		return (0);
	}
	return (ENXIO);
}

static int
agp_nvidia_attach (device_t dev)
{
	struct agp_nvidia_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	u_int32_t apbase;
	u_int32_t aplimit;
	u_int32_t temp;
	int size;
	int i;
	int error;

	switch (pci_get_device(dev)) {
	case NVIDIA_DEVICEID_NFORCE:
		sc->wbc_mask = 0x00010000;
		break;
	case NVIDIA_DEVICEID_NFORCE2:
		sc->wbc_mask = 0x80000000;
		break;
	default:
		sc->wbc_mask = 0;
		break;
	}

	/* AGP Controller */
	sc->dev = dev;

	/* Memory Controller 1 */
	sc->mc1_dev = pci_find_bsf(pci_get_bus(dev), 0, 1);
	if (sc->mc1_dev == NULL) {
		device_printf(dev,
			"Unable to find NVIDIA Memory Controller 1.\n");
		return (ENODEV);
	}

	/* Memory Controller 2 */
	sc->mc2_dev = pci_find_bsf(pci_get_bus(dev), 0, 2);
	if (sc->mc2_dev == NULL) {
		device_printf(dev,
			"Unable to find NVIDIA Memory Controller 2.\n");
		return (ENODEV);
	}

	/* AGP Host to PCI Bridge */
	sc->bdev = pci_find_bsf(pci_get_bus(dev), 30, 0);
	if (sc->bdev == NULL) {
		device_printf(dev,
			"Unable to find NVIDIA AGP Host to PCI Bridge.\n");
		return (ENODEV);
	}

	error = agp_generic_attach(dev);
	if (error)
		return (error);

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	for (;;) {
		gatt = agp_alloc_gatt(dev);
		if (gatt)
			break;
		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(dev, AGP_GET_APERTURE(dev) / 2))
			goto fail;
	}
	sc->gatt = gatt;

	apbase = rman_get_start(sc->agp.as_aperture);
	aplimit = apbase + AGP_GET_APERTURE(dev) - 1;
	pci_write_config(sc->mc2_dev, AGP_NVIDIA_2_APBASE, apbase, 4);
	pci_write_config(sc->mc2_dev, AGP_NVIDIA_2_APLIMIT, aplimit, 4);
	pci_write_config(sc->bdev, AGP_NVIDIA_3_APBASE, apbase, 4);
	pci_write_config(sc->bdev, AGP_NVIDIA_3_APLIMIT, aplimit, 4);

	error = nvidia_init_iorr(apbase, AGP_GET_APERTURE(dev));
	if (error) {
		device_printf(dev, "Failed to setup IORRs\n");
		goto fail;
	}

	/* directory size is 64k */
	size = AGP_GET_APERTURE(dev) / 1024 / 1024;
	sc->num_dirs = size / 64;
	sc->num_active_entries = (size == 32) ? 16384 : ((size * 1024) / 4);
	sc->pg_offset = 0;
	if (sc->num_dirs == 0) {
		sc->num_dirs = 1;
		sc->num_active_entries /= (64 / size);
		sc->pg_offset = (apbase & (64 * 1024 * 1024 - 1) &
				 ~(AGP_GET_APERTURE(dev) - 1)) / PAGE_SIZE;
	}

	/* (G)ATT Base Address */
	for (i = 0; i < 8; i++) {
		pci_write_config(sc->mc2_dev, AGP_NVIDIA_2_ATTBASE(i),
				 (sc->gatt->ag_physical +
				   (i % sc->num_dirs) * 64 * 1024),
				 4);
	}

	/* GTLB Control */
	temp = pci_read_config(sc->mc2_dev, AGP_NVIDIA_2_GARTCTRL, 4);
	pci_write_config(sc->mc2_dev, AGP_NVIDIA_2_GARTCTRL, temp | 0x11, 4);

	/* GART Control */
	temp = pci_read_config(sc->dev, AGP_NVIDIA_0_APSIZE, 4);
	pci_write_config(sc->dev, AGP_NVIDIA_0_APSIZE, temp | 0x100, 4);

	return (0);
fail:
	agp_generic_detach(dev);
	return (ENOMEM);
}

static int
agp_nvidia_detach (device_t dev)
{
	struct agp_nvidia_softc *sc = device_get_softc(dev);
	int error;
	u_int32_t temp;

	error = agp_generic_detach(dev);
	if (error)
		return (error);

	/* GART Control */
	temp = pci_read_config(sc->dev, AGP_NVIDIA_0_APSIZE, 4);
	pci_write_config(sc->dev, AGP_NVIDIA_0_APSIZE, temp & ~(0x100), 4);

	/* GTLB Control */
	temp = pci_read_config(sc->mc2_dev, AGP_NVIDIA_2_GARTCTRL, 4);
	pci_write_config(sc->mc2_dev, AGP_NVIDIA_2_GARTCTRL, temp & ~(0x11), 4);

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(dev, sc->initial_aperture);

	/* restore iorr for previous aperture size */
	nvidia_init_iorr(rman_get_start(sc->agp.as_aperture),
			 sc->initial_aperture);

	agp_free_gatt(sc->gatt);

	return (0);
}

static u_int32_t
agp_nvidia_get_aperture(device_t dev)
{
	u_int8_t	key;

	key = ffs(pci_read_config(dev, AGP_NVIDIA_0_APSIZE, 1) & 0x0f);
	return (1 << (24 + (key ? key : 5)));
}

static int
agp_nvidia_set_aperture(device_t dev, u_int32_t aperture)
{
	u_int8_t val;
	u_int8_t key;

	switch (aperture) {
	case (512 * 1024 * 1024): key = 0; break;
	case (256 * 1024 * 1024): key = 8; break;
	case (128 * 1024 * 1024): key = 12; break;
	case (64 * 1024 * 1024): key = 14; break;
	case (32 * 1024 * 1024): key = 15; break;
	default:
		device_printf(dev, "Invalid aperture size (%dMb)\n",
				aperture / 1024 / 1024);
		return (EINVAL);
	}
	val = pci_read_config(dev, AGP_NVIDIA_0_APSIZE, 1);
	pci_write_config(dev, AGP_NVIDIA_0_APSIZE, ((val & ~0x0f) | key), 1);

	return (0);
}

static int
agp_nvidia_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_nvidia_softc *sc = device_get_softc(dev);
	u_int32_t index;

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	index = (sc->pg_offset + offset) >> AGP_PAGE_SHIFT;
	sc->gatt->ag_virtual[index] = physical;

	return (0);
}

static int
agp_nvidia_unbind_page(device_t dev, int offset)
{
	struct agp_nvidia_softc *sc = device_get_softc(dev);
	u_int32_t index;

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return (EINVAL);

	index = (sc->pg_offset + offset) >> AGP_PAGE_SHIFT;
	sc->gatt->ag_virtual[index] = 0;

	return (0);
}

static int
agp_nvidia_flush_tlb (device_t dev, int offset)
{
	struct agp_nvidia_softc *sc;
	u_int32_t wbc_reg, temp;
	int i;

	sc = (struct agp_nvidia_softc *)device_get_softc(dev);

	if (sc->wbc_mask) {
		wbc_reg = pci_read_config(sc->mc1_dev, AGP_NVIDIA_1_WBC, 4);
		wbc_reg |= sc->wbc_mask;
		pci_write_config(sc->mc1_dev, AGP_NVIDIA_1_WBC, wbc_reg, 4);

		/* Wait no more than 3 seconds. */
		for (i = 0; i < 3000; i++) {
			wbc_reg = pci_read_config(sc->mc1_dev,
						  AGP_NVIDIA_1_WBC, 4);
			if ((sc->wbc_mask & wbc_reg) == 0)
				break;
			else
				DELAY(1000);
		}
		if (i == 3000)
			device_printf(dev,
				"TLB flush took more than 3 seconds.\n");
	}

	/* Flush TLB entries. */
	for(i = 0; i < 32 + 1; i++)
		temp = sc->gatt->ag_virtual[i * PAGE_SIZE / sizeof(u_int32_t)];
	for(i = 0; i < 32 + 1; i++)
		temp = sc->gatt->ag_virtual[i * PAGE_SIZE / sizeof(u_int32_t)];

	return (0);
}

#define	SYSCFG		0xC0010010
#define	IORR_BASE0	0xC0010016
#define	IORR_MASK0	0xC0010017
#define	AMD_K7_NUM_IORR	2

static int
nvidia_init_iorr(u_int32_t addr, u_int32_t size)
{
	quad_t base, mask, sys;
	u_int32_t iorr_addr, free_iorr_addr;

	/* Find the iorr that is already used for the addr */
	/* If not found, determine the uppermost available iorr */
	free_iorr_addr = AMD_K7_NUM_IORR;
	for(iorr_addr = 0; iorr_addr < AMD_K7_NUM_IORR; iorr_addr++) {
		base = rdmsr(IORR_BASE0 + 2 * iorr_addr);
		mask = rdmsr(IORR_MASK0 + 2 * iorr_addr);

		if ((base & 0xfffff000ULL) == (addr & 0xfffff000))
			break;

		if ((mask & 0x00000800ULL) == 0)
			free_iorr_addr = iorr_addr;
	}

	if (iorr_addr >= AMD_K7_NUM_IORR) {
		iorr_addr = free_iorr_addr;
		if (iorr_addr >= AMD_K7_NUM_IORR)
			return (EINVAL);
	}

	base = (addr & ~0xfff) | 0x18;
	mask = (0xfULL << 32) | ((~(size - 1)) & 0xfffff000) | 0x800;
	wrmsr(IORR_BASE0 + 2 * iorr_addr, base);
	wrmsr(IORR_MASK0 + 2 * iorr_addr, mask);

	sys = rdmsr(SYSCFG);
	sys |= 0x00100000ULL;
	wrmsr(SYSCFG, sys);

	return (0);
}

static device_method_t agp_nvidia_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		agp_nvidia_probe),
	DEVMETHOD(device_attach,	agp_nvidia_attach),
	DEVMETHOD(device_detach,	agp_nvidia_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_nvidia_get_aperture),
	DEVMETHOD(agp_set_aperture,	agp_nvidia_set_aperture),
	DEVMETHOD(agp_bind_page,	agp_nvidia_bind_page),
	DEVMETHOD(agp_unbind_page,	agp_nvidia_unbind_page),
	DEVMETHOD(agp_flush_tlb,	agp_nvidia_flush_tlb),

	DEVMETHOD(agp_enable,		agp_generic_enable),
	DEVMETHOD(agp_alloc_memory,	agp_generic_alloc_memory),
	DEVMETHOD(agp_free_memory,	agp_generic_free_memory),
	DEVMETHOD(agp_bind_memory,	agp_generic_bind_memory),
	DEVMETHOD(agp_unbind_memory,	agp_generic_unbind_memory),

	{ 0, 0 }
};

static driver_t agp_nvidia_driver = {
	"agp",
	agp_nvidia_methods,
	sizeof(struct agp_nvidia_softc),
};

static devclass_t agp_devclass;

DRIVER_MODULE(agp_nvidia, pci, agp_nvidia_driver, agp_devclass, 0, 0);
MODULE_DEPEND(agp_nvidia, agp, 1, 1, 1);
MODULE_DEPEND(agp_nvidia, pci, 1, 1, 1);
