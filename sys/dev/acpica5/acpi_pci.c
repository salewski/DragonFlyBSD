/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD: src/sys/dev/acpica/acpi_pci.c,v 1.16 2004/05/29 04:32:50 njl Exp $
 * $DragonFly: src/sys/dev/acpica5/acpi_pci.c,v 1.3 2004/07/05 00:07:35 dillon Exp $
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include "acpi.h"
#include "acpivar.h"

#include <sys/pciio.h>
#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>
#include <bus/pci/pci_private.h>

#include "pcib_if.h"
#include "pci_if.h"

/* Hooks for the ACPI CA debugging infrastructure. */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("PCI")

struct acpi_pci_devinfo {
	struct pci_devinfo	ap_dinfo;
	ACPI_HANDLE		ap_handle;
};

static int	acpi_pci_probe(device_t dev);
static int	acpi_pci_attach(device_t dev);
static int	acpi_pci_read_ivar(device_t dev, device_t child, int which,
		    uintptr_t *result);
static int	acpi_pci_child_location_str_method(device_t cbdev,
		    device_t child, char *buf, size_t buflen);


static int	acpi_pci_set_powerstate_method(device_t dev, device_t child,
		    int state);
static ACPI_STATUS acpi_pci_save_handle(ACPI_HANDLE handle, UINT32 level,
		    void *context, void **status);

static device_method_t acpi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		acpi_pci_probe),
	DEVMETHOD(device_attach,	acpi_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	acpi_pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD(bus_get_resource_list,pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, acpi_pci_child_location_str_method),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	acpi_pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt, pci_assign_interrupt_method),

	{ 0, 0 }
};

static driver_t acpi_pci_driver = {
	"pci",
	acpi_pci_methods,
	0,			/* no softc */
};

DRIVER_MODULE(acpi_pci, pcib, acpi_pci_driver, pci_devclass, 0, 0);
MODULE_DEPEND(acpi_pci, acpi, 1, 1, 1);
MODULE_VERSION(acpi_pci, 1);

static int
acpi_pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
    struct acpi_pci_devinfo *dinfo;

    switch (which) {
    case ACPI_IVAR_HANDLE:
	dinfo = device_get_ivars(child);
	*result = (uintptr_t)dinfo->ap_handle;
	return (0);
    }
    return (pci_read_ivar(dev, child, which, result));
}

static int
acpi_pci_child_location_str_method(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
    struct acpi_pci_devinfo *dinfo = device_get_ivars(child);

    pci_child_location_str_method(cbdev, child, buf, buflen);

    if (dinfo->ap_handle) {
	strlcat(buf, " path=", buflen);
	strlcat(buf, acpi_name(dinfo->ap_handle), buflen);
    }
    return (0);
}

/*
 * PCI power manangement
 */
static int
acpi_pci_set_powerstate_method(device_t dev, device_t child, int state)
{
	ACPI_HANDLE h;
	ACPI_STATUS status;
	int acpi_state, old_state, error;

	switch (state) {
	case PCI_POWERSTATE_D0:
		acpi_state = ACPI_STATE_D0;
		break;
	case PCI_POWERSTATE_D1:
		acpi_state = ACPI_STATE_D1;
		break;
	case PCI_POWERSTATE_D2:
		acpi_state = ACPI_STATE_D2;
		break;
	case PCI_POWERSTATE_D3:
		acpi_state = ACPI_STATE_D3;
		break;
	default:
		return (EINVAL);
	}

	/*
	 * We set the state using PCI Power Management outside of setting
	 * the ACPI state.  This means that when powering down a device, we
	 * first shut it down using PCI, and then using ACPI, which lets ACPI
	 * try to power down any Power Resources that are now no longer used.
	 * When powering up a device, we let ACPI set the state first so that
	 * it can enable any needed Power Resources before changing the PCI
	 * power state.
	 */
	old_state = pci_get_powerstate(child);
	if (old_state < state) {
		error = pci_set_powerstate_method(dev, child, state);
		if (error)
			return (error);
	}
	h = acpi_get_handle(child);
	if (h != NULL) {
		status = acpi_pwr_switch_consumer(h, acpi_state);
		if (ACPI_FAILURE(status))
			device_printf(dev,
			    "Failed to set ACPI power state D%d on %s: %s\n",
			    acpi_state, device_get_nameunit(child),
			    AcpiFormatException(status));
	}
	if (old_state > state)
		return (pci_set_powerstate_method(dev, child, state));
	else
		return (0);
}

static ACPI_STATUS
acpi_pci_save_handle(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	struct acpi_pci_devinfo *dinfo;
	device_t *devlist;
	int devcount, i, func, slot;
	UINT32 address;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	if (ACPI_FAILURE(acpi_GetInteger(handle, "_ADR", &address)))
		return_ACPI_STATUS (AE_OK);
	slot = address >> 16;
	func = address & 0xffff;
	if (device_get_children((device_t)context, &devlist, &devcount) != 0)
		return_ACPI_STATUS (AE_OK);
	for (i = 0; i < devcount; i++) {
		dinfo = device_get_ivars(devlist[i]);
		if (dinfo->ap_dinfo.cfg.func == func &&
		    dinfo->ap_dinfo.cfg.slot == slot) {
			dinfo->ap_handle = handle;
			free(devlist, M_TEMP);
			return_ACPI_STATUS (AE_OK);
		}
	}
	free(devlist, M_TEMP);
	return_ACPI_STATUS (AE_OK);
}

static int
acpi_pci_probe(device_t dev)
{

	if (pcib_get_bus(dev) < 0)
		return (ENXIO);
	if (acpi_get_handle(dev) == NULL)
		return (ENXIO);
	device_set_desc(dev, "ACPI PCI bus");
	return (0);
}

static int
acpi_pci_attach(device_t dev)
{
	int busno;

	/*
	 * Since there can be multiple independantly numbered PCI
	 * busses on some large alpha systems, we can't use the unit
	 * number to decide what bus we are probing. We ask the parent 
	 * pcib what our bus number is.
	 */
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "physical bus=%d\n", busno);

	/*
	 * First, PCI devices are added as in the normal PCI bus driver.
	 * Afterwards, the ACPI namespace under the bridge driver is
	 * walked to save ACPI handles to all the devices that appear in
	 * the ACPI namespace as immediate descendants of the bridge.
	 *
	 * XXX: Sometimes PCI devices show up in the ACPI namespace that
	 * pci_add_children() doesn't find.  We currently just ignore
	 * these devices.
	 */
	pci_add_children(dev, busno, sizeof(struct acpi_pci_devinfo));
	AcpiWalkNamespace(ACPI_TYPE_DEVICE, acpi_get_handle(dev), 1,
	    acpi_pci_save_handle, dev, NULL);

	return (bus_generic_attach(dev));
}
