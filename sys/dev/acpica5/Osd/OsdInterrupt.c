/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 * $FreeBSD: src/sys/dev/acpica/Osd/OsdInterrupt.c,v 1.17 2004/04/14 03:41:06 njl Exp $
 * $DragonFly: src/sys/dev/acpica5/Osd/OsdInterrupt.c,v 1.3 2005/03/12 14:33:40 y0netan1 Exp $
 */

/*
 * 6.5 : Interrupt handling
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include "acpi.h"
#include <dev/acpica5/acpivar.h>

#define _COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("INTERRUPT")

static void		InterruptWrapper(void *arg);

static ACPI_OSD_HANDLER	InterruptHandler;
static UINT32		InterruptOverride = 0;

ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
    ACPI_OSD_HANDLER ServiceRoutine, void *Context)
{
    struct acpi_softc	*sc;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if ((sc = devclass_get_softc(devclass_find("acpi"), 0)) == NULL)
	panic("can't find ACPI device to register interrupt");
    if (sc->acpi_dev == NULL)
	panic("acpi softc has invalid device");

    if (InterruptNumber < 0 || InterruptNumber > 255)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    if (ServiceRoutine == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    if (InterruptHandler != NULL) {
	device_printf(sc->acpi_dev, "interrupt handler already installed\n");
	return_ACPI_STATUS (AE_ALREADY_EXISTS);
    }
    InterruptHandler = ServiceRoutine;

    /*
     * If the MADT contained an interrupt override directive for the SCI,
     * we use that value instead of the one from the FADT.
     */
    if (InterruptOverride != 0) {
	    device_printf(sc->acpi_dev,
		"Overriding SCI Interrupt from IRQ %u to IRQ %u\n",
		InterruptNumber, InterruptOverride);
	    InterruptNumber = InterruptOverride;
    }

    /* Set up the interrupt resource. */
    sc->acpi_irq_rid = 0;
    bus_set_resource(sc->acpi_dev, SYS_RES_IRQ, 0, InterruptNumber, 1);
    sc->acpi_irq = bus_alloc_resource_any(sc->acpi_dev, SYS_RES_IRQ,
	&sc->acpi_irq_rid, RF_SHAREABLE | RF_ACTIVE);
    if (sc->acpi_irq == NULL) {
	device_printf(sc->acpi_dev, "could not allocate interrupt\n");
	goto error;
    }
    if (bus_setup_intr(sc->acpi_dev, sc->acpi_irq, INTR_TYPE_MISC,
	InterruptWrapper, Context, &sc->acpi_irq_handle)) {
	device_printf(sc->acpi_dev, "could not set up interrupt\n");
	goto error;
    }

    return_ACPI_STATUS (AE_OK);

error:
    if (sc->acpi_irq_handle)
	bus_teardown_intr(sc->acpi_dev, sc->acpi_irq, sc->acpi_irq_handle);
    sc->acpi_irq_handle = NULL;
    if (sc->acpi_irq)
	bus_release_resource(sc->acpi_dev, SYS_RES_IRQ, 0, sc->acpi_irq);
    sc->acpi_irq = NULL;
    bus_delete_resource(sc->acpi_dev, SYS_RES_IRQ, 0);
    InterruptHandler = NULL;

    return_ACPI_STATUS (AE_ALREADY_EXISTS);
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine)
{
    struct acpi_softc	*sc;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (InterruptNumber < 0 || InterruptNumber > 255)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    if (ServiceRoutine == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    if ((sc = devclass_get_softc(devclass_find("acpi"), 0)) == NULL)
	panic("can't find ACPI device to deregister interrupt");

    if (sc->acpi_irq == NULL)
	return_ACPI_STATUS (AE_NOT_EXIST);

    bus_teardown_intr(sc->acpi_dev, sc->acpi_irq, sc->acpi_irq_handle);
    bus_release_resource(sc->acpi_dev, SYS_RES_IRQ, 0, sc->acpi_irq);
    bus_delete_resource(sc->acpi_dev, SYS_RES_IRQ, 0);

    sc->acpi_irq = NULL;
    InterruptHandler = NULL;

    return_ACPI_STATUS (AE_OK);
}

ACPI_STATUS
acpi_OverrideInterruptLevel(UINT32 InterruptNumber)
{

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (InterruptOverride != 0)
	return_ACPI_STATUS (AE_ALREADY_EXISTS);
    InterruptOverride = InterruptNumber;
    return_ACPI_STATUS (AE_OK);
}

static void
InterruptWrapper(void *arg)
{
    ACPI_LOCK_DECL;

    ACPI_LOCK;
    InterruptHandler(arg);
    ACPI_UNLOCK;
}
