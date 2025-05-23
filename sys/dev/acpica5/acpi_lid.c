/*-
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith <msmith@freebd.org>
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
 * $FreeBSD: src/sys/dev/acpica/acpi_lid.c,v 1.22 2004/05/30 20:08:23 phk Exp $
 * $DragonFly: src/sys/dev/acpica5/acpi_lid.c,v 1.3 2004/07/05 00:07:35 dillon Exp $
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/proc.h>

#include "acpi.h"
#include <dev/acpica5/acpivar.h>

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_BUTTON
ACPI_MODULE_NAME("LID")

struct acpi_lid_softc {
    device_t	lid_dev;
    ACPI_HANDLE	lid_handle;
    int		lid_status;	/* open or closed */
};

static int	acpi_lid_probe(device_t dev);
static int	acpi_lid_attach(device_t dev);
static int	acpi_lid_suspend(device_t dev);
static int	acpi_lid_resume(device_t dev);
static void	acpi_lid_notify_status_changed(void *arg);
static void 	acpi_lid_notify_handler(ACPI_HANDLE h, UINT32 notify,
					void *context);

static device_method_t acpi_lid_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_lid_probe),
    DEVMETHOD(device_attach,	acpi_lid_attach),
    DEVMETHOD(device_suspend,	acpi_lid_suspend),
    DEVMETHOD(device_resume,	acpi_lid_resume),

    {0, 0}
};

static driver_t acpi_lid_driver = {
    "acpi_lid",
    acpi_lid_methods,
    sizeof(struct acpi_lid_softc),
};

static devclass_t acpi_lid_devclass;
DRIVER_MODULE(acpi_lid, acpi, acpi_lid_driver, acpi_lid_devclass, 0, 0);
MODULE_DEPEND(acpi_lid, acpi, 1, 1, 1);

static int
acpi_lid_probe(device_t dev)
{
    if (acpi_get_type(dev) == ACPI_TYPE_DEVICE && !acpi_disabled("lid") &&
	acpi_MatchHid(dev, "PNP0C0D")) {

	device_set_desc(dev, "Control Method Lid Switch");
	return (0);
    }
    return (ENXIO);
}

static int
acpi_lid_attach(device_t dev)
{
    struct acpi_lid_softc	*sc;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->lid_dev = dev;
    sc->lid_handle = acpi_get_handle(dev);

    /*
     * If a system does not get lid events, it may make sense to change
     * the type to ACPI_ALL_NOTIFY.  Some systems generate both a wake and
     * runtime notify in that case though.
     */
    AcpiInstallNotifyHandler(sc->lid_handle, ACPI_DEVICE_NOTIFY,
			     acpi_lid_notify_handler, sc);

    /* Enable the GPE for wake/runtime. */
    acpi_wake_init(dev, ACPI_GPE_TYPE_WAKE_RUN);
    acpi_wake_set_enable(dev, 1);

    return_VALUE (0);
}

static int
acpi_lid_suspend(device_t dev)
{
    struct acpi_softc		*acpi_sc;

    acpi_sc = acpi_device_get_parent_softc(dev);
    acpi_wake_sleep_prep(dev, acpi_sc->acpi_sstate);
    return (0);
}

static int
acpi_lid_resume(device_t dev)
{
    acpi_wake_run_prep(dev);
    return (0);
}

static void
acpi_lid_notify_status_changed(void *arg)
{
    struct acpi_lid_softc	*sc;
    struct acpi_softc		*acpi_sc;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = (struct acpi_lid_softc *)arg;

    /*
     * Evaluate _LID and check the return value, update lid status.
     *	Zero:		The lid is closed
     *	Non-zero:	The lid is open
     */
    status = acpi_GetInteger(sc->lid_handle, "_LID", &sc->lid_status);
    if (ACPI_FAILURE(status))
	return_VOID;

    acpi_sc = acpi_device_get_parent_softc(sc->lid_dev);
    if (acpi_sc == NULL)
        return_VOID;

    ACPI_VPRINT(sc->lid_dev, acpi_sc, "Lid %s\n",
		sc->lid_status ? "opened" : "closed");

    acpi_UserNotify("Lid", sc->lid_handle, sc->lid_status);

    if (sc->lid_status == 0)
	EVENTHANDLER_INVOKE(acpi_sleep_event, acpi_sc->acpi_lid_switch_sx);
    else
	EVENTHANDLER_INVOKE(acpi_wakeup_event, acpi_sc->acpi_lid_switch_sx);

    return_VOID;
}

/* XXX maybe not here */
#define	ACPI_NOTIFY_STATUS_CHANGED	0x80

static void 
acpi_lid_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_lid_softc	*sc;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

    sc = (struct acpi_lid_softc *)context;
    switch (notify) {
    case ACPI_NOTIFY_STATUS_CHANGED:
	AcpiOsQueueForExecution(OSD_PRIORITY_LO,
				acpi_lid_notify_status_changed, sc);
	break;
    default:
	device_printf(sc->lid_dev, "unknown notify %#x\n", notify);
	break;
    }

    return_VOID;
}
