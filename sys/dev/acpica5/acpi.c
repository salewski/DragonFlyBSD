/*-
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000, 2001 Michael Smith
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
 *	$FreeBSD: src/sys/dev/acpica/acpi.c,v 1.156 2004/06/05 07:25:58 njl Exp $
 *	$DragonFly: src/sys/dev/acpica5/acpi.c,v 1.9 2004/09/15 16:46:19 joerg Exp $
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/ctype.h>
#include <sys/linker.h>
#include <sys/power.h>
#include <sys/sbuf.h>

#include <machine/clock.h>
#include <machine/globaldata.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <bus/isa/isavar.h>

#include "acpi.h"
#include <dev/acpica5/acpivar.h>
#include <dev/acpica5/acpiio.h>
#include <acnamesp.h>

MALLOC_DEFINE(M_ACPIDEV, "acpidev", "ACPI devices");

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("ACPI")

static d_open_t		acpiopen;
static d_close_t	acpiclose;
static d_ioctl_t	acpiioctl;

#define CDEV_MAJOR 152
static struct cdevsw acpi_cdevsw = {
	.d_name	= "acpi",
	.d_maj  = CDEV_MAJOR,
	.d_flags = 0,
	.d_port = NULL,
	.d_clone = NULL,
	.old_open = acpiopen,
	.old_close = acpiclose,
	.old_ioctl = acpiioctl
};

#if __FreeBSD_version >= 500000
struct mtx	acpi_mutex;
#endif

struct acpi_quirks {
    char	*OemId;
    uint32_t	OemRevision;
    char	*value;
};

#define ACPI_OEM_REV_ANY	0

static struct acpi_quirks acpi_quirks_table[] = {
#ifdef notyet
    /* Bad PCI routing table.  Used on some SuperMicro boards. */
    { "PTLTD ", 0x06040000, "pci_link" },
#endif

    { NULL, 0, NULL }
};

static int	acpi_modevent(struct module *mod, int event, void *junk);
static void	acpi_identify(driver_t *driver, device_t parent);
static int	acpi_probe(device_t dev);
static int	acpi_attach(device_t dev);
static int	acpi_shutdown(device_t dev);
static void	acpi_quirks_set(void);
static device_t	acpi_add_child(device_t bus, int order, const char *name,
			int unit);
static int	acpi_print_child(device_t bus, device_t child);
static int	acpi_read_ivar(device_t dev, device_t child, int index,
			uintptr_t *result);
static int	acpi_write_ivar(device_t dev, device_t child, int index,
			uintptr_t value);
static int	acpi_set_resource(device_t dev, device_t child, int type,
			int rid, u_long start, u_long count);
static int	acpi_get_resource(device_t dev, device_t child, int type,
			int rid, u_long *startp, u_long *countp);
static struct resource *acpi_alloc_resource(device_t bus, device_t child,
			int type, int *rid, u_long start, u_long end,
			u_long count, u_int flags);
static int	acpi_release_resource(device_t bus, device_t child, int type,
			int rid, struct resource *r);
static uint32_t	acpi_isa_get_logicalid(device_t dev);
static int	acpi_isa_get_compatid(device_t dev, uint32_t *cids, int count);
static int	acpi_isa_pnp_probe(device_t bus, device_t child,
			struct isa_pnp_id *ids);
static void	acpi_probe_children(device_t bus);
static ACPI_STATUS acpi_probe_child(ACPI_HANDLE handle, UINT32 level,
			void *context, void **status);
static void	acpi_shutdown_pre_sync(void *arg, int howto);
static void	acpi_shutdown_final(void *arg, int howto);
static void	acpi_shutdown_poweroff(void *arg);
static void	acpi_enable_fixed_events(struct acpi_softc *sc);
static int	acpi_parse_prw(ACPI_HANDLE h, struct acpi_prw_data *prw);
static ACPI_STATUS acpi_wake_limit(ACPI_HANDLE h, UINT32 level, void *context,
		    void **status);
static int	acpi_wake_limit_walk(int sstate);
static int	acpi_wake_sysctl_walk(device_t dev);
static int	acpi_wake_set_sysctl(SYSCTL_HANDLER_ARGS);
static void	acpi_system_eventhandler_sleep(void *arg, int state);
static void	acpi_system_eventhandler_wakeup(void *arg, int state);
static int	acpi_supported_sleep_state_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS);
static int	acpi_pm_func(u_long cmd, void *arg, ...);
static int	acpi_child_location_str_method(device_t acdev, device_t child,
					       char *buf, size_t buflen);
static int	acpi_child_pnpinfo_str_method(device_t acdev, device_t child,
					      char *buf, size_t buflen);

static device_method_t acpi_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,		acpi_identify),
    DEVMETHOD(device_probe,		acpi_probe),
    DEVMETHOD(device_attach,		acpi_attach),
    DEVMETHOD(device_shutdown,		acpi_shutdown),
    DEVMETHOD(device_detach,		bus_generic_detach),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_add_child,		acpi_add_child),
    DEVMETHOD(bus_print_child,		acpi_print_child),
    DEVMETHOD(bus_read_ivar,		acpi_read_ivar),
    DEVMETHOD(bus_write_ivar,		acpi_write_ivar),
    DEVMETHOD(bus_set_resource,		acpi_set_resource),
    DEVMETHOD(bus_get_resource,		acpi_get_resource),
    DEVMETHOD(bus_alloc_resource,	acpi_alloc_resource),
    DEVMETHOD(bus_release_resource,	acpi_release_resource),
    DEVMETHOD(bus_child_pnpinfo_str,	acpi_child_pnpinfo_str_method),
    DEVMETHOD(bus_child_location_str,	acpi_child_location_str_method),
    DEVMETHOD(bus_driver_added,		bus_generic_driver_added),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    /* ISA emulation */
    DEVMETHOD(isa_pnp_probe,		acpi_isa_pnp_probe),

    {0, 0}
};

static driver_t acpi_driver = {
    "acpi",
    acpi_methods,
    sizeof(struct acpi_softc),
};

static devclass_t acpi_devclass;
DRIVER_MODULE(acpi, nexus, acpi_driver, acpi_devclass, acpi_modevent, 0);
MODULE_VERSION(acpi, 1);

static const char* sleep_state_names[] = {
    "S0", "S1", "S2", "S3", "S4", "S5", "NONE"};

SYSCTL_NODE(_debug, OID_AUTO, acpi, CTLFLAG_RW, NULL, "ACPI debugging");
static char acpi_ca_version[12];
SYSCTL_STRING(_debug_acpi, OID_AUTO, acpi_ca_version, CTLFLAG_RD,
	      acpi_ca_version, 0, "Version of Intel ACPI-CA");

/*
 * Allow override of whether methods execute in parallel or not.
 * Enable this for serial behavior, which fixes "AE_ALREADY_EXISTS"
 * errors for AML that really can't handle parallel method execution.
 * It is off by default since this breaks recursive methods and
 * some IBMs use such code.
 */
static int acpi_serialize_methods;
TUNABLE_INT("hw.acpi.serialize_methods", &acpi_serialize_methods);

/*
 * ACPI can only be loaded as a module by the loader; activating it after
 * system bootstrap time is not useful, and can be fatal to the system.
 * It also cannot be unloaded, since the entire system bus heirarchy hangs
 * off it.
 */
static int
acpi_modevent(struct module *mod, int event, void *junk)
{
    switch(event) {
    case MOD_LOAD:
	if (!cold) {
	    printf("The ACPI driver cannot be loaded after boot.\n");
	    return (EPERM);
	}
	break;
    case MOD_UNLOAD:
	if (!cold && power_pm_get_type() == POWER_PM_TYPE_ACPI)
	    return (EBUSY);
	break;
    default:
	break;
    }
    return (0);
}

/*
 * Perform early initialization.
 */
ACPI_STATUS
acpi_Startup(void)
{
#ifdef ACPI_DEBUGGER
    char *debugpoint;
#endif
    static int error, started = 0;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (started)
	return_VALUE (error);
    started = 1;

#if __FreeBSD_version >= 500000
    /* Initialise the ACPI mutex */
    mtx_init(&acpi_mutex, "ACPI global lock", NULL, MTX_DEF);
#endif

    /*
     * Set the globals from our tunables.  This is needed because ACPI-CA
     * uses UINT8 for some values and we have no tunable_byte.
     */
    AcpiGbl_AllMethodsSerialized = (UINT8)acpi_serialize_methods;

    /* Start up the ACPI CA subsystem. */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "init"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif
    if (ACPI_FAILURE(error = AcpiInitializeSubsystem())) {
	printf("ACPI: initialisation failed: %s\n", AcpiFormatException(error));
	return_VALUE (error);
    }
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "tables"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    if (ACPI_FAILURE(error = AcpiLoadTables())) {
	printf("ACPI: table load failed: %s\n", AcpiFormatException(error));
	return_VALUE(error);
    }

    /* Set up any quirks we have for this XSDT. */
    acpi_quirks_set();
    if (acpi_disabled("acpi"))
	return_VALUE (AE_ERROR);

    return_VALUE (AE_OK);
}

/*
 * Detect ACPI, perform early initialisation
 */
static void
acpi_identify(driver_t *driver, device_t parent)
{
    device_t	child;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (!cold)
	return_VOID;

    /* Check that we haven't been disabled with a hint. */
    if (resource_disabled("acpi", 0))
	return_VOID;

    /* Make sure we're not being doubly invoked. */
    if (device_find_child(parent, "acpi", 0) != NULL)
	return_VOID;

    /* Initialize ACPI-CA. */
    if (ACPI_FAILURE(acpi_Startup()))
	return_VOID;

    snprintf(acpi_ca_version, sizeof(acpi_ca_version), "%#x", ACPI_CA_VERSION);

    /* Attach the actual ACPI device. */
    if ((child = BUS_ADD_CHILD(parent, 0, "acpi", 0)) == NULL) {
	device_printf(parent, "ACPI: could not attach\n");
	return_VOID;
    }
}

/*
 * Fetch some descriptive data from ACPI to put in our attach message
 */
static int
acpi_probe(device_t dev)
{
    ACPI_TABLE_HEADER	th;
    char		buf[20];
    int			error;
    struct sbuf		sb;
    ACPI_STATUS		status;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (power_pm_get_type() != POWER_PM_TYPE_NONE &&
	power_pm_get_type() != POWER_PM_TYPE_ACPI) {

	device_printf(dev, "Other PM system enabled.\n");
	return_VALUE(ENXIO);
    }

    ACPI_LOCK;

    if (ACPI_FAILURE(status = AcpiGetTableHeader(ACPI_TABLE_XSDT, 1, &th))) {
	device_printf(dev, "couldn't get XSDT header: %s\n",
		      AcpiFormatException(status));
	error = ENXIO;
    } else {
	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_bcat(&sb, th.OemId, 6);
	sbuf_trim(&sb);
	sbuf_putc(&sb, ' ');
	sbuf_bcat(&sb, th.OemTableId, 8);
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	device_set_desc_copy(dev, sbuf_data(&sb));
	sbuf_delete(&sb);
	error = 0;
    }
    ACPI_UNLOCK;
    return_VALUE(error);
}

static int
acpi_attach(device_t dev)
{
    struct acpi_softc	*sc;
    ACPI_STATUS		status;
    int			error, state;
    UINT32		flags;
    UINT8		TypeA, TypeB;
    char		*env;
#ifdef ACPI_DEBUGGER
    char		*debugpoint;
#endif
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_LOCK;
    sc = device_get_softc(dev);
    bzero(sc, sizeof(*sc));
    sc->acpi_dev = dev;
    callout_init(&sc->acpi_sleep_timer);

#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "spaces"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    /* Install the default address space handlers. */
    error = ENXIO;
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_SYSTEM_MEMORY, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "Could not initialise SystemMemory handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_SYSTEM_IO, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "Could not initialise SystemIO handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }
    status = AcpiInstallAddressSpaceHandler(ACPI_ROOT_OBJECT,
		ACPI_ADR_SPACE_PCI_CONFIG, ACPI_DEFAULT_HANDLER, NULL, NULL);
    if (ACPI_FAILURE(status)) {
	device_printf(dev, "could not initialise PciConfig handler: %s\n",
		      AcpiFormatException(status));
	goto out;
    }

    /*
     * Bring ACPI fully online.
     *
     * Note that some systems (specifically, those with namespace evaluation
     * issues that require the avoidance of parts of the namespace) must
     * avoid running _INI and _STA on everything, as well as dodging the final
     * object init pass.
     *
     * For these devices, we set ACPI_NO_DEVICE_INIT and ACPI_NO_OBJECT_INIT).
     *
     * XXX We should arrange for the object init pass after we have attached
     *     all our child devices, but on many systems it works here.
     */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "enable"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif
    flags = 0;
    if (testenv("debug.acpi.avoid"))
	flags = ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (ACPI_FAILURE(status = AcpiEnableSubsystem(flags))) {
	device_printf(dev, "Could not enable ACPI: %s\n",
		      AcpiFormatException(status));
	goto out;
    }

    /*
     * Call the ECDT probe function to provide EC functionality before
     * the namespace has been evaluated.
     */
    acpi_ec_ecdt_probe(dev);

    if (ACPI_FAILURE(status = AcpiInitializeObjects(flags))) {
	device_printf(dev, "Could not initialize ACPI objects: %s\n",
		      AcpiFormatException(status));
	goto out;
    }

    /*
     * Setup our sysctl tree.
     *
     * XXX: This doesn't check to make sure that none of these fail.
     */
    sysctl_ctx_init(&sc->acpi_sysctl_ctx);
    sc->acpi_sysctl_tree = SYSCTL_ADD_NODE(&sc->acpi_sysctl_ctx,
			       SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
			       device_get_name(dev), CTLFLAG_RD, 0, "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "supported_sleep_state", CTLTYPE_STRING | CTLFLAG_RD,
	0, 0, acpi_supported_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "power_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_power_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "sleep_button_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_sleep_button_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "lid_switch_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_lid_switch_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "standby_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_standby_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_PROC(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "suspend_state", CTLTYPE_STRING | CTLFLAG_RW,
	&sc->acpi_suspend_sx, 0, acpi_sleep_state_sysctl, "A", "");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "sleep_delay", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_sleep_delay, 0, "sleep delay");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "s4bios", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_s4bios, 0, "S4BIOS mode");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "verbose", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_verbose, 0, "verbose mode");
    SYSCTL_ADD_INT(&sc->acpi_sysctl_ctx, SYSCTL_CHILDREN(sc->acpi_sysctl_tree),
	OID_AUTO, "disable_on_poweroff", CTLFLAG_RD | CTLFLAG_RW,
	&sc->acpi_disable_on_poweroff, 0, "ACPI subsystem disable on poweroff");

    /*
     * Default to 1 second before sleeping to give some machines time to
     * stabilize.
     */
    sc->acpi_sleep_delay = 1;
    sc->acpi_disable_on_poweroff = 0;
    if (bootverbose)
	sc->acpi_verbose = 1;
    if ((env = getenv("hw.acpi.verbose")) && strcmp(env, "0")) {
	sc->acpi_verbose = 1;
	freeenv(env);
    }

    /* Only enable S4BIOS by default if the FACS says it is available. */
    if (AcpiGbl_FACS->S4Bios_f != 0)
	    sc->acpi_s4bios = 1;

    /*
     * Dispatch the default sleep state to devices.  The lid switch is set
     * to NONE by default to avoid surprising users.
     */
    sc->acpi_power_button_sx = ACPI_STATE_S5;
    sc->acpi_lid_switch_sx = ACPI_S_STATES_MAX + 1;
    sc->acpi_standby_sx = ACPI_STATE_S1;
    sc->acpi_suspend_sx = ACPI_STATE_S3;

    /* Pick the first valid sleep state for the sleep button default. */
    sc->acpi_sleep_button_sx = ACPI_S_STATES_MAX + 1;
    for (state = ACPI_STATE_S1; state < ACPI_STATE_S5; state++)
	if (ACPI_SUCCESS(AcpiGetSleepTypeData(state, &TypeA, &TypeB))) {
	    sc->acpi_sleep_button_sx = state;
	    break;
	}

    acpi_enable_fixed_events(sc);

    /*
     * Scan the namespace and attach/initialise children.
     */
#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (!strcmp(debugpoint, "probe"))
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    /* Register our shutdown handlers */
    EVENTHANDLER_REGISTER(shutdown_pre_sync, acpi_shutdown_pre_sync, sc,
	SHUTDOWN_PRI_LAST);
    EVENTHANDLER_REGISTER(shutdown_final, acpi_shutdown_final, sc,
	SHUTDOWN_PRI_LAST);

    /*
     * Register our acpi event handlers.
     * XXX should be configurable eg. via userland policy manager.
     */
    EVENTHANDLER_REGISTER(acpi_sleep_event, acpi_system_eventhandler_sleep,
	sc, ACPI_EVENT_PRI_LAST);
    EVENTHANDLER_REGISTER(acpi_wakeup_event, acpi_system_eventhandler_wakeup,
	sc, ACPI_EVENT_PRI_LAST);

    /* Flag our initial states. */
    sc->acpi_enabled = 1;
    sc->acpi_sstate = ACPI_STATE_S0;
    sc->acpi_sleep_disabled = 0;

    /* Create the control device */
    cdevsw_add(&acpi_cdevsw, 0, 0);
    sc->acpi_dev_t = make_dev(&acpi_cdevsw, 0, UID_ROOT, GID_WHEEL, 0644,
			      "acpi");
    sc->acpi_dev_t->si_drv1 = sc;

#ifdef ACPI_DEBUGGER
    debugpoint = getenv("debug.acpi.debugger");
    if (debugpoint) {
	if (strcmp(debugpoint, "running") == 0)
	    acpi_EnterDebugger();
	freeenv(debugpoint);
    }
#endif

    if ((error = acpi_task_thread_init()))
	goto out;

    if ((error = acpi_machdep_init(dev)))
	goto out;

    /* Register ACPI again to pass the correct argument of pm_func. */
    power_pm_register(POWER_PM_TYPE_ACPI, acpi_pm_func, sc);

    if (!acpi_disabled("bus"))
	acpi_probe_children(dev);

    error = 0;

 out:
    ACPI_UNLOCK;
    return_VALUE (error);
}

static int
acpi_shutdown(device_t dev)
{

    /* Disable all wake GPEs not appropriate for reboot/poweroff. */
    acpi_wake_limit_walk(ACPI_STATE_S5);
    return (0);
}

static void
acpi_quirks_set()
{
    XSDT_DESCRIPTOR *xsdt;
    struct acpi_quirks *quirk;
    char *env, *tmp;
    int len;

    /*
     * If the user loaded a custom table or disabled "quirks", leave
     * the settings alone.
     */
    len = 0;
    if ((env = getenv("acpi_dsdt_load")) != NULL) {
	/* XXX No strcasecmp but this is good enough. */
	if (*env == 'Y' || *env == 'y')
	    goto out;
	freeenv(env);
    }
    if ((env = getenv("debug.acpi.disabled")) != NULL) {
	if (strstr("quirks", env) != NULL)
	    goto out;
	len = strlen(env);
    }

    /*
     * Search through our quirk table and concatenate the disabled
     * values with whatever we find.
     */
    xsdt = AcpiGbl_XSDT;
    for (quirk = acpi_quirks_table; quirk->OemId; quirk++) {
	if (!strncmp(xsdt->OemId, quirk->OemId, strlen(quirk->OemId)) &&
	    (xsdt->OemRevision == quirk->OemRevision ||
	    quirk->OemRevision == ACPI_OEM_REV_ANY)) {
		len += strlen(quirk->value) + 2;
		if ((tmp = malloc(len, M_TEMP, M_NOWAIT)) == NULL)
		    goto out;
		sprintf(tmp, "%s %s", env ? env : "", quirk->value);
#ifdef notyet
		setenv("debug.acpi.disabled", tmp);
#endif /* notyet */
		free(tmp, M_TEMP);
		break;
	}
    }

out:
    if (env)
	freeenv(env);
}

/*
 * Handle a new device being added
 */
static device_t
acpi_add_child(device_t bus, int order, const char *name, int unit)
{
    struct acpi_device	*ad;
    device_t		child;

    ad = malloc(sizeof(*ad), M_ACPIDEV, M_INTWAIT | M_ZERO);

    resource_list_init(&ad->ad_rl);

    child = device_add_child_ordered(bus, order, name, unit);
    if (child != NULL)
	device_set_ivars(child, ad);
    return (child);
}

static int
acpi_print_child(device_t bus, device_t child)
{
    struct acpi_device	 *adev = device_get_ivars(child);
    struct resource_list *rl = &adev->ad_rl;
    int retval = 0;

    retval += bus_print_child_header(bus, child);
    retval += resource_list_print_type(rl, "port",  SYS_RES_IOPORT, "%#lx");
    retval += resource_list_print_type(rl, "iomem", SYS_RES_MEMORY, "%#lx");
    retval += resource_list_print_type(rl, "irq",   SYS_RES_IRQ,    "%ld");
    retval += resource_list_print_type(rl, "drq",   SYS_RES_DRQ,    "%ld");
    retval += bus_print_child_footer(bus, child);

    return (retval);
}

/* Location hint for devctl(8) */
static int
acpi_child_location_str_method(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
    struct acpi_device *dinfo = device_get_ivars(child);

    if (dinfo->ad_handle)
	snprintf(buf, buflen, "path=%s", acpi_name(dinfo->ad_handle));
    else
	snprintf(buf, buflen, "magic=unknown");
    return (0);
}

/* PnP information for devctl(8) */
static int
acpi_child_pnpinfo_str_method(device_t cbdev, device_t child, char *buf,
    size_t buflen)
{
    ACPI_BUFFER adbuf = {ACPI_ALLOCATE_BUFFER, NULL};
    ACPI_DEVICE_INFO *adinfo;
    struct acpi_device *dinfo = device_get_ivars(child);
    char *end;
    int error;

    error = AcpiGetObjectInfo(dinfo->ad_handle, &adbuf);
    adinfo = (ACPI_DEVICE_INFO *) adbuf.Pointer;

    if (error)
	snprintf(buf, buflen, "Unknown");
    else
	snprintf(buf, buflen, "_HID=%s _UID=%lu",
		(adinfo->Valid & ACPI_VALID_HID) ?
		adinfo->HardwareId.Value : "UNKNOWN",
		(adinfo->Valid & ACPI_VALID_UID) ?
		strtoul(adinfo->UniqueId.Value, &end, 10) : 0);

    if (adinfo)
	AcpiOsFree(adinfo);

    return (0);
}

/*
 * Handle per-device ivars
 */
static int
acpi_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
    struct acpi_device	*ad;

    if ((ad = device_get_ivars(child)) == NULL) {
	printf("device has no ivars\n");
	return (ENOENT);
    }

    /* ACPI and ISA compatibility ivars */
    switch(index) {
    case ACPI_IVAR_HANDLE:
	*(ACPI_HANDLE *)result = ad->ad_handle;
	break;
    case ACPI_IVAR_MAGIC:
	*(int *)result = ad->ad_magic;
	break;
    case ACPI_IVAR_PRIVATE:
	*(void **)result = ad->ad_private;
	break;
    case ISA_IVAR_VENDORID:
    case ISA_IVAR_SERIAL:
    case ISA_IVAR_COMPATID:
	*(int *)result = -1;
	break;
    case ISA_IVAR_LOGICALID:
	*(int *)result = acpi_isa_get_logicalid(child);
	break;
    default:
	return (ENOENT);
    }

    return (0);
}

static int
acpi_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
    struct acpi_device	*ad;

    if ((ad = device_get_ivars(child)) == NULL) {
	printf("device has no ivars\n");
	return (ENOENT);
    }

    switch(index) {
    case ACPI_IVAR_HANDLE:
	ad->ad_handle = (ACPI_HANDLE)value;
	break;
    case ACPI_IVAR_MAGIC:
	ad->ad_magic = (int)value;
	break;
    case ACPI_IVAR_PRIVATE:
	ad->ad_private = (void *)value;
	break;
    default:
	panic("bad ivar write request (%d)", index);
	return (ENOENT);
    }

    return (0);
}

/*
 * Handle child resource allocation/removal
 */
static int
acpi_set_resource(device_t dev, device_t child, int type, int rid,
		  u_long start, u_long count)
{
    struct acpi_device		*ad = device_get_ivars(child);
    struct resource_list	*rl = &ad->ad_rl;

    resource_list_add(rl, type, rid, start, start + count -1, count);

    return(0);
}

static int
acpi_get_resource(device_t dev, device_t child, int type, int rid,
		  u_long *startp, u_long *countp)
{
    struct acpi_device		*ad = device_get_ivars(child);
    struct resource_list	*rl = &ad->ad_rl;
    struct resource_list_entry	*rle;

    rle = resource_list_find(rl, type, rid);
    if (!rle)
	return(ENOENT);
	
    if (startp)
	*startp = rle->start;
    if (countp)
	*countp = rle->count;

    return (0);
}

static struct resource *
acpi_alloc_resource(device_t bus, device_t child, int type, int *rid,
		    u_long start, u_long end, u_long count, u_int flags)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return (resource_list_alloc(rl, bus, child, type, rid, start, end, count,
	    flags));
}

static int
acpi_release_resource(device_t bus, device_t child, int type, int rid, struct resource *r)
{
    struct acpi_device *ad = device_get_ivars(child);
    struct resource_list *rl = &ad->ad_rl;

    return (resource_list_release(rl, bus, child, type, rid, r));
}

/* Allocate an IO port or memory resource, given its GAS. */
struct resource *
acpi_bus_alloc_gas(device_t dev, int *rid, ACPI_GENERIC_ADDRESS *gas)
{
    int type;

    if (gas == NULL || !ACPI_VALID_ADDRESS(gas->Address) ||
	gas->RegisterBitWidth < 8)
	return (NULL);

    switch (gas->AddressSpaceId) {
    case ACPI_ADR_SPACE_SYSTEM_MEMORY:
	type = SYS_RES_MEMORY;
	break;
    case ACPI_ADR_SPACE_SYSTEM_IO:
	type = SYS_RES_IOPORT;
	break;
    default:
	return (NULL);
    }

    bus_set_resource(dev, type, *rid, gas->Address, gas->RegisterBitWidth / 8);
    return (bus_alloc_resource_any(dev, type, rid, RF_ACTIVE));
}

/*
 * Handle ISA-like devices probing for a PnP ID to match.
 */
#define PNP_EISAID(s)				\
	((((s[0] - '@') & 0x1f) << 2)		\
	 | (((s[1] - '@') & 0x18) >> 3)		\
	 | (((s[1] - '@') & 0x07) << 13)	\
	 | (((s[2] - '@') & 0x1f) << 8)		\
	 | (PNP_HEXTONUM(s[4]) << 16)		\
	 | (PNP_HEXTONUM(s[3]) << 20)		\
	 | (PNP_HEXTONUM(s[6]) << 24)		\
	 | (PNP_HEXTONUM(s[5]) << 28))

static uint32_t
acpi_isa_get_logicalid(device_t dev)
{
    ACPI_DEVICE_INFO	*devinfo;
    ACPI_BUFFER		buf;
    ACPI_HANDLE		h;
    ACPI_STATUS		error;
    u_int32_t		pnpid;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    pnpid = 0;
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;

    ACPI_LOCK;
    
    /* Fetch and validate the HID. */
    if ((h = acpi_get_handle(dev)) == NULL)
	goto out;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	goto out;
    devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;

    if ((devinfo->Valid & ACPI_VALID_HID) != 0)
	pnpid = PNP_EISAID(devinfo->HardwareId.Value);

out:
    if (buf.Pointer != NULL)
	AcpiOsFree(buf.Pointer);
    ACPI_UNLOCK;
    return_VALUE (pnpid);
}

static int
acpi_isa_get_compatid(device_t dev, uint32_t *cids, int count)
{
    ACPI_DEVICE_INFO	*devinfo;
    ACPI_BUFFER		buf;
    ACPI_HANDLE		h;
    ACPI_STATUS		error;
    uint32_t		*pnpid;
    int			valid, i;
    ACPI_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    pnpid = cids;
    valid = 0;
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;

    ACPI_LOCK;
    
    /* Fetch and validate the CID */
    if ((h = acpi_get_handle(dev)) == NULL)
	goto out;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	goto out;
    devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;
    if ((devinfo->Valid & ACPI_VALID_CID) == 0)
	goto out;

    if (devinfo->CompatibilityId.Count < count)
	count = devinfo->CompatibilityId.Count;
    for (i = 0; i < count; i++) {
	if (strncmp(devinfo->CompatibilityId.Id[i].Value, "PNP", 3) != 0)
	    continue;
	*pnpid++ = PNP_EISAID(devinfo->CompatibilityId.Id[i].Value);
	valid++;
    }

out:
    if (buf.Pointer != NULL)
	AcpiOsFree(buf.Pointer);
    ACPI_UNLOCK;
    return_VALUE (valid);
}

static int
acpi_isa_pnp_probe(device_t bus, device_t child, struct isa_pnp_id *ids)
{
    int			result, cid_count, i;
    uint32_t		lid, cids[8];

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /*
     * ISA-style drivers attached to ACPI may persist and
     * probe manually if we return ENOENT.  We never want
     * that to happen, so don't ever return it.
     */
    result = ENXIO;

    /* Scan the supplied IDs for a match */
    lid = acpi_isa_get_logicalid(child);
    cid_count = acpi_isa_get_compatid(child, cids, 8);
    while (ids && ids->ip_id) {
	if (lid == ids->ip_id) {
	    result = 0;
	    goto out;
	}
	for (i = 0; i < cid_count; i++) {
	    if (cids[i] == ids->ip_id) {
		result = 0;
		goto out;
	    }
	}
	ids++;
    }

 out:
    return_VALUE (result);
}

/*
 * Scan relevant portions of the ACPI namespace and attach child devices.
 *
 * Note that we only expect to find devices in the \_PR_, \_TZ_, \_SI_ and
 * \_SB_ scopes, and \_PR_ and \_TZ_ become obsolete in the ACPI 2.0 spec.
 */
static void
acpi_probe_children(device_t bus)
{
    ACPI_HANDLE	parent;
    ACPI_STATUS	status;
    static char	*scopes[] = {"\\_PR_", "\\_TZ_", "\\_SI", "\\_SB_", NULL};
    int		i;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    /* Create any static children by calling device identify methods. */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "device identify routines\n"));
    bus_generic_probe(bus);

    /*
     * Scan the namespace and insert placeholders for all the devices that
     * we find.
     *
     * Note that we use AcpiWalkNamespace rather than AcpiGetDevices because
     * we want to create nodes for all devices, not just those that are
     * currently present. (This assumes that we don't want to create/remove
     * devices as they appear, which might be smarter.)
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "namespace scan\n"));
    for (i = 0; scopes[i] != NULL; i++) {
	status = AcpiGetHandle(ACPI_ROOT_OBJECT, scopes[i], &parent);
	if (ACPI_SUCCESS(status)) {
	    AcpiWalkNamespace(ACPI_TYPE_ANY, parent, 100, acpi_probe_child,
			      bus, NULL);
	}
    }

    /*
     * Scan all of the child devices we have created and let them probe/attach.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "first bus_generic_attach\n"));
    bus_generic_attach(bus);

    /*
     * Some of these children may have attached others as part of their attach
     * process (eg. the root PCI bus driver), so rescan.
     */
    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "second bus_generic_attach\n"));
    bus_generic_attach(bus);

    /* Attach wake sysctls. */
    acpi_wake_sysctl_walk(bus);

    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "done attaching children\n"));
    return_VOID;
}

/*
 * Evaluate a child device and determine whether we might attach a device to
 * it.
 */
static ACPI_STATUS
acpi_probe_child(ACPI_HANDLE handle, UINT32 level, void *context, void **status)
{
    ACPI_OBJECT_TYPE	type;
    device_t		child, bus = (device_t)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    /* Skip this device if we think we'll have trouble with it. */
    if (acpi_avoid(handle))
	return_ACPI_STATUS (AE_OK);

    if (ACPI_SUCCESS(AcpiGetType(handle, &type))) {
	switch(type) {
	case ACPI_TYPE_DEVICE:
	case ACPI_TYPE_PROCESSOR:
	case ACPI_TYPE_THERMAL:
	case ACPI_TYPE_POWER:
	    if (acpi_disabled("children"))
		break;

	    /* 
	     * Create a placeholder device for this node.  Sort the placeholder
	     * so that the probe/attach passes will run breadth-first.
	     */
	    ACPI_DEBUG_PRINT((ACPI_DB_OBJECTS, "scanning '%s'\n",
			     acpi_name(handle)));
	    child = BUS_ADD_CHILD(bus, level * 10, NULL, -1);
	    if (child == NULL)
		break;
	    acpi_set_handle(child, handle);

	    /* Check if the device can generate wake events. */
	    if (ACPI_SUCCESS(AcpiEvaluateObject(handle, "_PRW", NULL, NULL)))
		device_set_flags(child, ACPI_FLAG_WAKE_CAPABLE);

	    /*
	     * Check that the device is present.  If it's not present,
	     * leave it disabled (so that we have a device_t attached to
	     * the handle, but we don't probe it).
	     */
	    if (type == ACPI_TYPE_DEVICE && !acpi_DeviceIsPresent(child)) {
		device_disable(child);
		break;
	    }

	    /*
	     * Get the device's resource settings and attach them.
	     * Note that if the device has _PRS but no _CRS, we need
	     * to decide when it's appropriate to try to configure the
	     * device.  Ignore the return value here; it's OK for the
	     * device not to have any resources.
	     */
	    acpi_parse_resources(child, handle, &acpi_res_parse_set, NULL);

	    /* If we're debugging, probe/attach now rather than later */
	    ACPI_DEBUG_EXEC(device_probe_and_attach(child));
	    break;
	}
    }

    return_ACPI_STATUS (AE_OK);
}

static void
acpi_shutdown_pre_sync(void *arg, int howto)
{
    struct acpi_softc *sc = arg;

    ACPI_ASSERTLOCK;

    /*
     * Disable all ACPI events before soft off, otherwise the system
     * will be turned on again on some laptops.
     *
     * XXX this should probably be restricted to masking some events just
     *     before powering down, since we may still need ACPI during the
     *     shutdown process.
     */
    if (sc->acpi_disable_on_poweroff)
	acpi_Disable(sc);
}

static void
acpi_shutdown_final(void *arg, int howto)
{
    ACPI_STATUS	status;
    ACPI_ASSERTLOCK;

    /*
     * If powering off, run the actual shutdown code on each processor.
     * It will only perform the shutdown on the BSP.  Some chipsets do
     * not power off the system correctly if called from an AP.
     */
    if ((howto & RB_POWEROFF) != 0) {
	status = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
	if (ACPI_FAILURE(status)) {
	    printf("AcpiEnterSleepStatePrep failed - %s\n",
		   AcpiFormatException(status));
	    return;
	}
	printf("Powering system off using ACPI\n");
#ifdef notyet
	smp_rendezvous(NULL, acpi_shutdown_poweroff, NULL, NULL);
#else
	acpi_shutdown_poweroff(NULL);
#endif /* notyet */
    } else {
	printf("Shutting down ACPI\n");
	AcpiTerminate();
    }
}

/*
 * Since this function may be called with locks held or in an unknown
 * context, it cannot allocate memory, acquire locks, sleep, etc.
 */
static void
acpi_shutdown_poweroff(void *arg)
{
    ACPI_STATUS	status;

    ACPI_ASSERTLOCK;

    /* Only attempt to power off if this is the BSP (cpuid 0). */
    if (mdcpu->mi.gd_cpuid != 0)
	return;

    ACPI_DISABLE_IRQS();
    status = AcpiEnterSleepState(ACPI_STATE_S5);
    if (ACPI_FAILURE(status)) {
	printf("ACPI power-off failed - %s\n", AcpiFormatException(status));
    } else {
	DELAY(1000000);
	printf("ACPI power-off failed - timeout\n");
    }
}

static void
acpi_enable_fixed_events(struct acpi_softc *sc)
{
    static int	first_time = 1;

    ACPI_ASSERTLOCK;

    /* Enable and clear fixed events and install handlers. */
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->PwrButton == 0) {
	AcpiClearEvent(ACPI_EVENT_POWER_BUTTON);
	AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
				     acpi_event_power_button_sleep, sc);
	if (first_time)
	    device_printf(sc->acpi_dev, "Power Button (fixed)\n");
    }
    if (AcpiGbl_FADT != NULL && AcpiGbl_FADT->SleepButton == 0) {
	AcpiClearEvent(ACPI_EVENT_SLEEP_BUTTON);
	AcpiInstallFixedEventHandler(ACPI_EVENT_SLEEP_BUTTON,
				     acpi_event_sleep_button_sleep, sc);
	if (first_time)
	    device_printf(sc->acpi_dev, "Sleep Button (fixed)\n");
    }

    first_time = 0;
}

/*
 * Returns true if the device is actually present and should
 * be attached to.  This requires the present, enabled, UI-visible 
 * and diagnostics-passed bits to be set.
 */
BOOLEAN
acpi_DeviceIsPresent(device_t dev)
{
    ACPI_DEVICE_INFO	*devinfo;
    ACPI_HANDLE		h;
    ACPI_BUFFER		buf;
    ACPI_STATUS		error;
    int			ret;

    ACPI_ASSERTLOCK;
    
    ret = FALSE;
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);
    devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;

    /* If no _STA method, must be present */
    if ((devinfo->Valid & ACPI_VALID_STA) == 0)
	ret = TRUE;

    /* Return true for 'present' and 'functioning' */
    if ((devinfo->CurrentStatus & 0x9) == 0x9)
	ret = TRUE;

    AcpiOsFree(buf.Pointer);
    return (ret);
}

/*
 * Returns true if the battery is actually present and inserted.
 */
BOOLEAN
acpi_BatteryIsPresent(device_t dev)
{
    ACPI_DEVICE_INFO	*devinfo;
    ACPI_HANDLE		h;
    ACPI_BUFFER		buf;
    ACPI_STATUS		error;
    int			ret;

    ACPI_ASSERTLOCK;
    
    ret = FALSE;
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);
    devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;

    /* If no _STA method, must be present */
    if ((devinfo->Valid & ACPI_VALID_STA) == 0)
	ret = TRUE;

    /* Return true for 'present' and 'functioning' */
    if ((devinfo->CurrentStatus & 0x19) == 0x19)
	ret = TRUE;

    AcpiOsFree(buf.Pointer);
    return (ret);
}

/*
 * Match a HID string against a device
 */
BOOLEAN
acpi_MatchHid(device_t dev, char *hid) 
{
    ACPI_DEVICE_INFO	*devinfo;
    ACPI_HANDLE		h;
    ACPI_BUFFER		buf;
    ACPI_STATUS		error;
    int			ret, i;

    ACPI_ASSERTLOCK;

    ret = FALSE;
    if (hid == NULL)
	return (FALSE);
    if ((h = acpi_get_handle(dev)) == NULL)
	return (FALSE);
    buf.Pointer = NULL;
    buf.Length = ACPI_ALLOCATE_BUFFER;
    error = AcpiGetObjectInfo(h, &buf);
    if (ACPI_FAILURE(error))
	return (FALSE);
    devinfo = (ACPI_DEVICE_INFO *)buf.Pointer;

    if ((devinfo->Valid & ACPI_VALID_HID) != 0 &&
	strcmp(hid, devinfo->HardwareId.Value) == 0)
	    ret = TRUE;
    else if ((devinfo->Valid & ACPI_VALID_CID) != 0) {
	for (i = 0; i < devinfo->CompatibilityId.Count; i++) {
	    if (strcmp(hid, devinfo->CompatibilityId.Id[i].Value) == 0) {
		ret = TRUE;
		break;
	    }
	}
    }

    AcpiOsFree(buf.Pointer);
    return (ret);
}

/*
 * Return the handle of a named object within our scope, ie. that of (parent)
 * or one if its parents.
 */
ACPI_STATUS
acpi_GetHandleInScope(ACPI_HANDLE parent, char *path, ACPI_HANDLE *result)
{
    ACPI_HANDLE		r;
    ACPI_STATUS		status;

    ACPI_ASSERTLOCK;

    /* Walk back up the tree to the root */
    for (;;) {
	status = AcpiGetHandle(parent, path, &r);
	if (ACPI_SUCCESS(status)) {
	    *result = r;
	    return (AE_OK);
	}
	if (status != AE_NOT_FOUND)
	    return (AE_OK);
	if (ACPI_FAILURE(AcpiGetParent(parent, &r)))
	    return (AE_NOT_FOUND);
	parent = r;
    }
}

/* Find the difference between two PM tick counts. */
uint32_t
acpi_TimerDelta(uint32_t end, uint32_t start)
{
    uint32_t delta;

    if (end >= start)
	delta = end - start;
    else if (AcpiGbl_FADT->TmrValExt == 0)
	delta = ((0x00FFFFFF - start) + end + 1) & 0x00FFFFFF;
    else
	delta = ((0xFFFFFFFF - start) + end + 1);
    return (delta);
}

/*
 * Allocate a buffer with a preset data size.
 */
ACPI_BUFFER *
acpi_AllocBuffer(int size)
{
    ACPI_BUFFER	*buf;

    buf = malloc(size + sizeof(*buf), M_ACPIDEV, M_INTWAIT);
    buf->Length = size;
    buf->Pointer = (void *)(buf + 1);
    return (buf);
}

ACPI_STATUS
acpi_SetInteger(ACPI_HANDLE handle, char *path, UINT32 number)
{
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;

    ACPI_ASSERTLOCK;

    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = number;
    args.Count = 1;
    args.Pointer = &arg1;

    return (AcpiEvaluateObject(handle, path, &args, NULL));
}

/*
 * Evaluate a path that should return an integer.
 */
ACPI_STATUS
acpi_GetInteger(ACPI_HANDLE handle, char *path, UINT32 *number)
{
    ACPI_STATUS	status;
    ACPI_BUFFER	buf;
    ACPI_OBJECT	param;

    ACPI_ASSERTLOCK;

    if (handle == NULL)
	handle = ACPI_ROOT_OBJECT;

    /*
     * Assume that what we've been pointed at is an Integer object, or
     * a method that will return an Integer.
     */
    buf.Pointer = &param;
    buf.Length = sizeof(param);
    status = AcpiEvaluateObject(handle, path, NULL, &buf);
    if (ACPI_SUCCESS(status)) {
	if (param.Type == ACPI_TYPE_INTEGER)
	    *number = param.Integer.Value;
	else
	    status = AE_TYPE;
    }

    /* 
     * In some applications, a method that's expected to return an Integer
     * may instead return a Buffer (probably to simplify some internal
     * arithmetic).  We'll try to fetch whatever it is, and if it's a Buffer,
     * convert it into an Integer as best we can.
     *
     * This is a hack.
     */
    if (status == AE_BUFFER_OVERFLOW) {
	if ((buf.Pointer = AcpiOsAllocate(buf.Length)) == NULL) {
	    status = AE_NO_MEMORY;
	} else {
	    status = AcpiEvaluateObject(handle, path, NULL, &buf);
	    if (ACPI_SUCCESS(status))
		status = acpi_ConvertBufferToInteger(&buf, number);
	    AcpiOsFree(buf.Pointer);
	}
    }
    return (status);
}

ACPI_STATUS
acpi_ConvertBufferToInteger(ACPI_BUFFER *bufp, UINT32 *number)
{
    ACPI_OBJECT	*p;
    UINT8	*val;
    int		i;

    p = (ACPI_OBJECT *)bufp->Pointer;
    if (p->Type == ACPI_TYPE_INTEGER) {
	*number = p->Integer.Value;
	return (AE_OK);
    }
    if (p->Type != ACPI_TYPE_BUFFER)
	return (AE_TYPE);
    if (p->Buffer.Length > sizeof(int))
	return (AE_BAD_DATA);

    *number = 0;
    val = p->Buffer.Pointer;
    for (i = 0; i < p->Buffer.Length; i++)
	*number += val[i] << (i * 8);
    return (AE_OK);
}

/*
 * Iterate over the elements of an a package object, calling the supplied
 * function for each element.
 *
 * XXX possible enhancement might be to abort traversal on error.
 */
ACPI_STATUS
acpi_ForeachPackageObject(ACPI_OBJECT *pkg,
	void (*func)(ACPI_OBJECT *comp, void *arg), void *arg)
{
    ACPI_OBJECT	*comp;
    int		i;
    
    if (pkg == NULL || pkg->Type != ACPI_TYPE_PACKAGE)
	return (AE_BAD_PARAMETER);

    /* Iterate over components */
    i = 0;
    comp = pkg->Package.Elements;
    for (; i < pkg->Package.Count; i++, comp++)
	func(comp, arg);

    return (AE_OK);
}

/*
 * Find the (index)th resource object in a set.
 */
ACPI_STATUS
acpi_FindIndexedResource(ACPI_BUFFER *buf, int index, ACPI_RESOURCE **resp)
{
    ACPI_RESOURCE	*rp;
    int			i;

    rp = (ACPI_RESOURCE *)buf->Pointer;
    i = index;
    while (i-- > 0) {
	/* Range check */	
	if (rp > (ACPI_RESOURCE *)((u_int8_t *)buf->Pointer + buf->Length))
	    return (AE_BAD_PARAMETER);

	/* Check for terminator */
	if (rp->Id == ACPI_RSTYPE_END_TAG || rp->Length == 0)
	    return (AE_NOT_FOUND);
	rp = ACPI_NEXT_RESOURCE(rp);
    }
    if (resp != NULL)
	*resp = rp;

    return (AE_OK);
}

/*
 * Append an ACPI_RESOURCE to an ACPI_BUFFER.
 *
 * Given a pointer to an ACPI_RESOURCE structure, expand the ACPI_BUFFER
 * provided to contain it.  If the ACPI_BUFFER is empty, allocate a sensible
 * backing block.  If the ACPI_RESOURCE is NULL, return an empty set of
 * resources.
 */
#define ACPI_INITIAL_RESOURCE_BUFFER_SIZE	512

ACPI_STATUS
acpi_AppendBufferResource(ACPI_BUFFER *buf, ACPI_RESOURCE *res)
{
    ACPI_RESOURCE	*rp;
    void		*newp;
    
    /* Initialise the buffer if necessary. */
    if (buf->Pointer == NULL) {
	buf->Length = ACPI_INITIAL_RESOURCE_BUFFER_SIZE;
	if ((buf->Pointer = AcpiOsAllocate(buf->Length)) == NULL)
	    return (AE_NO_MEMORY);
	rp = (ACPI_RESOURCE *)buf->Pointer;
	rp->Id = ACPI_RSTYPE_END_TAG;
	rp->Length = 0;
    }
    if (res == NULL)
	return (AE_OK);
    
    /*
     * Scan the current buffer looking for the terminator.
     * This will either find the terminator or hit the end
     * of the buffer and return an error.
     */
    rp = (ACPI_RESOURCE *)buf->Pointer;
    for (;;) {
	/* Range check, don't go outside the buffer */
	if (rp >= (ACPI_RESOURCE *)((u_int8_t *)buf->Pointer + buf->Length))
	    return (AE_BAD_PARAMETER);
	if (rp->Id == ACPI_RSTYPE_END_TAG || rp->Length == 0)
	    break;
	rp = ACPI_NEXT_RESOURCE(rp);
    }

    /*
     * Check the size of the buffer and expand if required.
     *
     * Required size is:
     *	size of existing resources before terminator + 
     *	size of new resource and header +
     * 	size of terminator.
     *
     * Note that this loop should really only run once, unless
     * for some reason we are stuffing a *really* huge resource.
     */
    while ((((u_int8_t *)rp - (u_int8_t *)buf->Pointer) + 
	    res->Length + ACPI_RESOURCE_LENGTH_NO_DATA +
	    ACPI_RESOURCE_LENGTH) >= buf->Length) {
	if ((newp = AcpiOsAllocate(buf->Length * 2)) == NULL)
	    return (AE_NO_MEMORY);
	bcopy(buf->Pointer, newp, buf->Length);
	rp = (ACPI_RESOURCE *)((u_int8_t *)newp +
			       ((u_int8_t *)rp - (u_int8_t *)buf->Pointer));
	AcpiOsFree(buf->Pointer);
	buf->Pointer = newp;
	buf->Length += buf->Length;
    }
    
    /* Insert the new resource. */
    bcopy(res, rp, res->Length + ACPI_RESOURCE_LENGTH_NO_DATA);
    
    /* And add the terminator. */
    rp = ACPI_NEXT_RESOURCE(rp);
    rp->Id = ACPI_RSTYPE_END_TAG;
    rp->Length = 0;

    return (AE_OK);
}

/*
 * Set interrupt model.
 */
ACPI_STATUS
acpi_SetIntrModel(int model)
{
    return (acpi_SetInteger(ACPI_ROOT_OBJECT, "_PIC", model));
}

#define ACPI_MINIMUM_AWAKETIME	5

static void
acpi_sleep_enable(void *arg)
{
    ((struct acpi_softc *)arg)->acpi_sleep_disabled = 0;
}

/*
 * Set the system sleep state
 *
 * Currently we support S1-S5 but S4 is only S4BIOS
 */
ACPI_STATUS
acpi_SetSleepState(struct acpi_softc *sc, int state)
{
    ACPI_STATUS	status = AE_OK;
    UINT8	TypeA;
    UINT8	TypeB;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);
    ACPI_ASSERTLOCK;

    /* Avoid reentry if already attempting to suspend. */
    if (sc->acpi_sstate != ACPI_STATE_S0)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    /* We recently woke up so don't suspend again for a while. */
    if (sc->acpi_sleep_disabled)
	return_ACPI_STATUS (AE_OK);

    switch (state) {
    case ACPI_STATE_S1:
    case ACPI_STATE_S2:
    case ACPI_STATE_S3:
    case ACPI_STATE_S4:
	status = AcpiGetSleepTypeData((UINT8)state, &TypeA, &TypeB);
	if (status == AE_NOT_FOUND) {
	    device_printf(sc->acpi_dev,
			  "Sleep state S%d not supported by BIOS\n", state);
	    break;
	} else if (ACPI_FAILURE(status)) {
	    device_printf(sc->acpi_dev, "AcpiGetSleepTypeData failed - %s\n",
			  AcpiFormatException(status));
	    break;
	}

	sc->acpi_sstate = state;
	sc->acpi_sleep_disabled = 1;

	/* Disable all wake GPEs not appropriate for this state. */
	acpi_wake_limit_walk(state);

	/* Inform all devices that we are going to sleep. */
	if (DEVICE_SUSPEND(root_bus) != 0) {
	    /*
	     * Re-wake the system.
	     *
	     * XXX note that a better two-pass approach with a 'veto' pass
	     *     followed by a "real thing" pass would be better, but the
	     *     current bus interface does not provide for this.
	     */
	    DEVICE_RESUME(root_bus);
	    return_ACPI_STATUS (AE_ERROR);
	}

	status = AcpiEnterSleepStatePrep(state);
	if (ACPI_FAILURE(status)) {
	    device_printf(sc->acpi_dev, "AcpiEnterSleepStatePrep failed - %s\n",
			  AcpiFormatException(status));
	    break;
	}

	if (sc->acpi_sleep_delay > 0)
	    DELAY(sc->acpi_sleep_delay * 1000000);

	if (state != ACPI_STATE_S1) {
	    acpi_sleep_machdep(sc, state);

	    /* AcpiEnterSleepState() may be incomplete, unlock if locked. */
	    if (AcpiGbl_MutexInfo[ACPI_MTX_HARDWARE].OwnerId !=
		ACPI_MUTEX_NOT_ACQUIRED) {

		AcpiUtReleaseMutex(ACPI_MTX_HARDWARE);
	    }

	    /* Re-enable ACPI hardware on wakeup from sleep state 4. */
	    if (state == ACPI_STATE_S4)
		AcpiEnable();
	} else {
	    status = AcpiEnterSleepState((UINT8)state);
	    if (ACPI_FAILURE(status)) {
		device_printf(sc->acpi_dev, "AcpiEnterSleepState failed - %s\n",
			      AcpiFormatException(status));
		break;
	    }
	}
	AcpiLeaveSleepState((UINT8)state);
	DEVICE_RESUME(root_bus);
	sc->acpi_sstate = ACPI_STATE_S0;
	acpi_enable_fixed_events(sc);
	break;
    case ACPI_STATE_S5:
	/*
	 * Shut down cleanly and power off.  This will call us back through the
	 * shutdown handlers.
	 */
	shutdown_nice(RB_POWEROFF);
	break;
    case ACPI_STATE_S0:
    default:
	status = AE_BAD_PARAMETER;
	break;
    }

    /* Disable a second sleep request for a short period */
    if (sc->acpi_sleep_disabled)
	callout_reset(&sc->acpi_sleep_timer, hz * ACPI_MINIMUM_AWAKETIME,
		      acpi_sleep_enable, sc);

    return_ACPI_STATUS (status);
}

/* Initialize a device's wake GPE. */
int
acpi_wake_init(device_t dev, int type)
{
    struct acpi_prw_data prw;

    /* Check that the device can wake the system. */
    if ((device_get_flags(dev) & ACPI_FLAG_WAKE_CAPABLE) == 0)
	return (ENXIO);

    /* Evaluate _PRW to find the GPE. */
    if (acpi_parse_prw(acpi_get_handle(dev), &prw) != 0)
	return (ENXIO);

    /* Set the requested type for the GPE (runtime, wake, or both). */
    if (ACPI_FAILURE(AcpiSetGpeType(prw.gpe_handle, prw.gpe_bit, type))) {
	device_printf(dev, "set GPE type failed\n");
	return (ENXIO);
    }

    return (0);
}

/* Enable or disable the device's wake GPE. */
int
acpi_wake_set_enable(device_t dev, int enable)
{
    struct acpi_prw_data prw;
    ACPI_HANDLE handle;
    ACPI_STATUS status;
    int flags;

    /* Make sure the device supports waking the system. */
    flags = device_get_flags(dev);
    handle = acpi_get_handle(dev);
    if ((flags & ACPI_FLAG_WAKE_CAPABLE) == 0 || handle == NULL)
	return (ENXIO);

    /* Evaluate _PRW to find the GPE. */
    if (acpi_parse_prw(handle, &prw) != 0)
	return (ENXIO);

    if (enable) {
	status = AcpiEnableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);
	if (ACPI_FAILURE(status)) {
	    device_printf(dev, "enable wake failed\n");
	    return (ENXIO);
	}
	device_set_flags(dev, flags | ACPI_FLAG_WAKE_ENABLED);
    } else {
	status = AcpiDisableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);
	if (ACPI_FAILURE(status)) {
	    device_printf(dev, "disable wake failed\n");
	    return (ENXIO);
	}
	device_set_flags(dev, flags & ~ACPI_FLAG_WAKE_ENABLED);
    }

    return (0);
}

/* Configure a device's GPE appropriately for the new sleep state. */
int
acpi_wake_sleep_prep(device_t dev, int sstate)
{
    struct acpi_prw_data prw;
    ACPI_HANDLE handle;
    int flags;

    /* Check that this is an ACPI device and get its GPE. */
    flags = device_get_flags(dev);
    handle = acpi_get_handle(dev);
    if ((flags & ACPI_FLAG_WAKE_CAPABLE) == 0 || handle == NULL)
	return (ENXIO);

    /* Evaluate _PRW to find the GPE. */
    if (acpi_parse_prw(handle, &prw) != 0)
	return (ENXIO);

    /*
     * TBD: All Power Resources referenced by elements 2 through N
     *      of the _PRW object are put into the ON state.
     */

    /*
     * If the user requested that this device wake the system and the next
     * sleep state is valid for this GPE, enable it and the device's wake
     * capability.  The sleep state must be less than (i.e., higher power)
     * or equal to the value specified by _PRW.  Return early, leaving
     * the appropriate power resources enabled.
     */
    if ((flags & ACPI_FLAG_WAKE_ENABLED) != 0 &&
	sstate <= prw.lowest_wake) {
	if (bootverbose)
	    device_printf(dev, "wake_prep enabled gpe %#x for state %d\n",
		prw.gpe_bit, sstate);
	AcpiEnableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);
	acpi_SetInteger(handle, "_PSW", 1);
	return (0);
    }

    /*
     * If the device wake was disabled or this sleep state is too low for
     * this device, disable its wake capability and GPE.
     */
    AcpiDisableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);
    acpi_SetInteger(handle, "_PSW", 0);
    if (bootverbose)
	device_printf(dev, "wake_prep disabled gpe %#x for state %d\n",
	    prw.gpe_bit, sstate);

    /*
     * TBD: All Power Resources referenced by elements 2 through N
     *      of the _PRW object are put into the OFF state.
     */

    return (0);
}

/* Re-enable GPEs after wake. */
int
acpi_wake_run_prep(device_t dev)
{
    struct acpi_prw_data prw;
    ACPI_HANDLE handle;
    int flags;

    /* Check that this is an ACPI device and get its GPE. */
    flags = device_get_flags(dev);
    handle = acpi_get_handle(dev);
    if ((flags & ACPI_FLAG_WAKE_CAPABLE) == 0 || handle == NULL)
	return (ENXIO);

    /* Evaluate _PRW to find the GPE. */
    if (acpi_parse_prw(handle, &prw) != 0)
	return (ENXIO);

    /*
     * TBD: Be sure all Power Resources referenced by elements 2 through N
     *      of the _PRW object are in the ON state.
     */

    /* Disable wake capability and if the user requested, enable the GPE. */
    acpi_SetInteger(handle, "_PSW", 0);
    if ((flags & ACPI_FLAG_WAKE_ENABLED) != 0)
	AcpiEnableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);
    return (0);
}

static ACPI_STATUS
acpi_wake_limit(ACPI_HANDLE h, UINT32 level, void *context, void **status)
{
    struct acpi_prw_data prw;
    int *sstate;

    /* It's ok not to have _PRW if the device can't wake the system. */
    if (acpi_parse_prw(h, &prw) != 0)
	return (AE_OK);

    sstate = (int *)context;
    if (*sstate > prw.lowest_wake)
	AcpiDisableGpe(prw.gpe_handle, prw.gpe_bit, ACPI_NOT_ISR);

    return (AE_OK);
}

/* Walk all system devices, disabling them if necessary for sstate. */
static int
acpi_wake_limit_walk(int sstate)
{
    ACPI_HANDLE sb_handle;

    if (ACPI_SUCCESS(AcpiGetHandle(ACPI_ROOT_OBJECT, "\\_SB_", &sb_handle)))
	AcpiWalkNamespace(ACPI_TYPE_ANY, sb_handle, 100,
	    acpi_wake_limit, &sstate, NULL);
    return (0);
}

/* Walk the tree rooted at acpi0 to attach per-device wake sysctls. */
static int
acpi_wake_sysctl_walk(device_t dev)
{
    int error, i, numdevs;
    device_t *devlist;
    device_t child;

    error = device_get_children(dev, &devlist, &numdevs);
    if (error != 0 || numdevs == 0)
	return (error);
    for (i = 0; i < numdevs; i++) {
	child = devlist[i];
	if (!device_is_attached(child))
	    continue;
	if (device_get_flags(child) & ACPI_FLAG_WAKE_CAPABLE) {
#ifdef dfly_notyet
	    SYSCTL_ADD_PROC(device_get_sysctl_ctx(child),
		SYSCTL_CHILDREN(device_get_sysctl_tree(child)), OID_AUTO,
		"wake", CTLTYPE_INT | CTLFLAG_RW, child, 0,
		acpi_wake_set_sysctl, "I", "Device set to wake the system");
#endif /* dfly_notyet */
	}
	acpi_wake_sysctl_walk(child);
    }
    free(devlist, M_TEMP);

    return (0);
}

/* Enable or disable wake from userland. */
static int
acpi_wake_set_sysctl(SYSCTL_HANDLER_ARGS)
{
    int enable, error;
    device_t dev;

    dev = (device_t)arg1;
    enable = (device_get_flags(dev) & ACPI_FLAG_WAKE_ENABLED) ? 1 : 0;

    error = sysctl_handle_int(oidp, &enable, 0, req);
    if (error != 0 || req->newptr == NULL)
	return (error);
    if (enable != 0 && enable != 1)
	return (EINVAL);

    return (acpi_wake_set_enable(dev, enable));
}

/* Parse a device's _PRW into a structure. */
static int
acpi_parse_prw(ACPI_HANDLE h, struct acpi_prw_data *prw)
{
    ACPI_STATUS			status;
    ACPI_BUFFER			prw_buffer;
    ACPI_OBJECT			*res, *res2;
    int error;

    if (h == NULL || prw == NULL)
	return (EINVAL);

    /*
     * The _PRW object (7.2.9) is only required for devices that have the
     * ability to wake the system from a sleeping state.
     */
    error = EINVAL;
    prw_buffer.Pointer = NULL;
    prw_buffer.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiEvaluateObject(h, "_PRW", NULL, &prw_buffer);
    if (ACPI_FAILURE(status))
	return (ENOENT);
    res = (ACPI_OBJECT *)prw_buffer.Pointer;
    if (res == NULL)
	return (ENOENT);
    if (!ACPI_PKG_VALID(res, 2))
	goto out;

    /*
     * Element 1 of the _PRW object:
     * The lowest power system sleeping state that can be entered while still
     * providing wake functionality.  The sleeping state being entered must
     * be less than (i.e., higher power) or equal to this value.
     */
    if (acpi_PkgInt32(res, 1, &prw->lowest_wake) != 0)
	goto out;

    /*
     * Element 0 of the _PRW object:
     */
    switch (res->Package.Elements[0].Type) {
    case ACPI_TYPE_INTEGER:
	/*
	 * If the data type of this package element is numeric, then this
	 * _PRW package element is the bit index in the GPEx_EN, in the
	 * GPE blocks described in the FADT, of the enable bit that is
	 * enabled for the wake event.
	 */
	prw->gpe_handle = NULL;
	prw->gpe_bit = res->Package.Elements[0].Integer.Value;
	error = 0;
	break;
    case ACPI_TYPE_PACKAGE:
	/*
	 * If the data type of this package element is a package, then this
	 * _PRW package element is itself a package containing two
	 * elements.  The first is an object reference to the GPE Block
	 * device that contains the GPE that will be triggered by the wake
	 * event.  The second element is numeric and it contains the bit
	 * index in the GPEx_EN, in the GPE Block referenced by the
	 * first element in the package, of the enable bit that is enabled for
	 * the wake event.
	 *
	 * For example, if this field is a package then it is of the form:
	 * Package() {\_SB.PCI0.ISA.GPE, 2}
	 */
	res2 = &res->Package.Elements[0];
	if (!ACPI_PKG_VALID(res2, 2))
	    goto out;
	prw->gpe_handle = acpi_GetReference(NULL, &res2->Package.Elements[0]);
	if (prw->gpe_handle == NULL)
	    goto out;
	if (acpi_PkgInt32(res2, 1, &prw->gpe_bit) != 0)
	    goto out;
	error = 0;
	break;
    default:
	goto out;
    }

    /* XXX No power resource handling yet. */
    prw->power_res = NULL;

out:
    if (prw_buffer.Pointer != NULL)
	AcpiOsFree(prw_buffer.Pointer);
    return (error);
}

/*
 * Enable/Disable ACPI
 */
ACPI_STATUS
acpi_Enable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;
    u_int32_t	flags;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    flags = ACPI_NO_ADDRESS_SPACE_INIT | ACPI_NO_HARDWARE_INIT |
	    ACPI_NO_DEVICE_INIT | ACPI_NO_OBJECT_INIT;
    if (!sc->acpi_enabled)
	status = AcpiEnableSubsystem(flags);
    else
	status = AE_OK;

    if (status == AE_OK)
	sc->acpi_enabled = 1;

    return_ACPI_STATUS (status);
}

ACPI_STATUS
acpi_Disable(struct acpi_softc *sc)
{
    ACPI_STATUS	status;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);
    ACPI_ASSERTLOCK;

    if (sc->acpi_enabled)
	status = AcpiDisable();
    else
	status = AE_OK;

    if (status == AE_OK)
	sc->acpi_enabled = 0;

    return_ACPI_STATUS (status);
}

/*
 * ACPI Event Handlers
 */

/* System Event Handlers (registered by EVENTHANDLER_REGISTER) */

static void
acpi_system_eventhandler_sleep(void *arg, int state)
{
    ACPI_LOCK_DECL;
    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);

    ACPI_LOCK;
    if (state >= ACPI_STATE_S0 && state <= ACPI_S_STATES_MAX)
	acpi_SetSleepState((struct acpi_softc *)arg, state);
    ACPI_UNLOCK;
    return_VOID;
}

static void
acpi_system_eventhandler_wakeup(void *arg, int state)
{
    ACPI_LOCK_DECL;
    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, state);

    /* Well, what to do? :-) */

    ACPI_LOCK;
    ACPI_UNLOCK;

    return_VOID;
}

/* 
 * ACPICA Event Handlers (FixedEvent, also called from button notify handler)
 */
UINT32
acpi_event_power_button_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_power_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_event_power_button_wake(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_power_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_event_sleep_button_sleep(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_sleep_event, sc->acpi_sleep_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

UINT32
acpi_event_sleep_button_wake(void *context)
{
    struct acpi_softc	*sc = (struct acpi_softc *)context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    EVENTHANDLER_INVOKE(acpi_wakeup_event, sc->acpi_sleep_button_sx);

    return_VALUE (ACPI_INTERRUPT_HANDLED);
}

/*
 * XXX This is kinda ugly, and should not be here.
 */
struct acpi_staticbuf {
    ACPI_BUFFER	buffer;
    char	data[512];
};

char *
acpi_name(ACPI_HANDLE handle)
{
    static struct acpi_staticbuf	buf;

    ACPI_ASSERTLOCK;

    buf.buffer.Length = 512;
    buf.buffer.Pointer = &buf.data[0];

    if (ACPI_SUCCESS(AcpiGetName(handle, ACPI_FULL_PATHNAME, &buf.buffer)))
	return (buf.buffer.Pointer);

    return ("(unknown path)");
}

/*
 * Debugging/bug-avoidance.  Avoid trying to fetch info on various
 * parts of the namespace.
 */
int
acpi_avoid(ACPI_HANDLE handle)
{
    char	*cp, *env, *np;
    int		len;

    np = acpi_name(handle);
    if (*np == '\\')
	np++;
    if ((env = getenv("debug.acpi.avoid")) == NULL)
	return (0);

    /* Scan the avoid list checking for a match */
    cp = env;
    for (;;) {
	while ((*cp != 0) && isspace(*cp))
	    cp++;
	if (*cp == 0)
	    break;
	len = 0;
	while ((cp[len] != 0) && !isspace(cp[len]))
	    len++;
	if (!strncmp(cp, np, len)) {
	    freeenv(env);
	    return(1);
	}
	cp += len;
    }
    freeenv(env);

    return (0);
}

/*
 * Debugging/bug-avoidance.  Disable ACPI subsystem components.
 */
int
acpi_disabled(char *subsys)
{
    char	*cp, *env;
    int		len;

    if ((env = getenv("debug.acpi.disabled")) == NULL)
	return (0);
    if (strcmp(env, "all") == 0) {
	freeenv(env);
	return (1);
    }

    /* Scan the disable list, checking for a match. */
    cp = env;
    for (;;) {
	while (*cp != '\0' && isspace(*cp))
	    cp++;
	if (*cp == '\0')
	    break;
	len = 0;
	while (cp[len] != '\0' && !isspace(cp[len]))
	    len++;
	if (strncmp(cp, subsys, len) == 0) {
	    freeenv(env);
	    return (1);
	}
	cp += len;
    }
    freeenv(env);

    return (0);
}

/*
 * Control interface.
 *
 * We multiplex ioctls for all participating ACPI devices here.  Individual 
 * drivers wanting to be accessible via /dev/acpi should use the
 * register/deregister interface to make their handlers visible.
 */
struct acpi_ioctl_hook
{
    TAILQ_ENTRY(acpi_ioctl_hook) link;
    u_long			 cmd;
    acpi_ioctl_fn		 fn;
    void			 *arg;
};

static TAILQ_HEAD(,acpi_ioctl_hook)	acpi_ioctl_hooks;
static int				acpi_ioctl_hooks_initted;

/*
 * Register an ioctl handler.
 */
int
acpi_register_ioctl(u_long cmd, acpi_ioctl_fn fn, void *arg)
{
    struct acpi_ioctl_hook	*hp;

    hp = malloc(sizeof(*hp), M_ACPIDEV, M_INTWAIT);
    hp->cmd = cmd;
    hp->fn = fn;
    hp->arg = arg;
    if (acpi_ioctl_hooks_initted == 0) {
	TAILQ_INIT(&acpi_ioctl_hooks);
	acpi_ioctl_hooks_initted = 1;
    }
    TAILQ_INSERT_TAIL(&acpi_ioctl_hooks, hp, link);
    return (0);
}

/*
 * Deregister an ioctl handler.
 */
void	
acpi_deregister_ioctl(u_long cmd, acpi_ioctl_fn fn)
{
    struct acpi_ioctl_hook	*hp;

    TAILQ_FOREACH(hp, &acpi_ioctl_hooks, link)
	if ((hp->cmd == cmd) && (hp->fn == fn))
	    break;

    if (hp != NULL) {
	TAILQ_REMOVE(&acpi_ioctl_hooks, hp, link);
	free(hp, M_ACPIDEV);
    }
}

static int
acpiopen(dev_t dev, int flag, int fmt, d_thread_t *td)
{
    return (0);
}

static int
acpiclose(dev_t dev, int flag, int fmt, d_thread_t *td)
{
    return (0);
}

static int
acpiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, d_thread_t *td)
{
    struct acpi_softc		*sc;
    struct acpi_ioctl_hook	*hp;
    int				error, xerror, state;
    ACPI_LOCK_DECL;

    ACPI_LOCK;

    error = state = 0;
    sc = dev->si_drv1;

    /*
     * Scan the list of registered ioctls, looking for handlers.
     */
    if (acpi_ioctl_hooks_initted) {
	TAILQ_FOREACH(hp, &acpi_ioctl_hooks, link) {
	    if (hp->cmd == cmd) {
		xerror = hp->fn(cmd, addr, hp->arg);
		if (xerror != 0)
		    error = xerror;
		goto out;
	    }
	}
    }

    /*
     * Core ioctls are not permitted for non-writable user.
     * Currently, other ioctls just fetch information.
     * Not changing system behavior.
     */
    if((flag & FWRITE) == 0)
	return (EPERM);

    /* Core system ioctls. */
    switch (cmd) {
    case ACPIIO_ENABLE:
	if (ACPI_FAILURE(acpi_Enable(sc)))
	    error = ENXIO;
	break;
    case ACPIIO_DISABLE:
	if (ACPI_FAILURE(acpi_Disable(sc)))
	    error = ENXIO;
	break;
    case ACPIIO_SETSLPSTATE:
	if (!sc->acpi_enabled) {
	    error = ENXIO;
	    break;
	}
	state = *(int *)addr;
	if (state >= ACPI_STATE_S0  && state <= ACPI_S_STATES_MAX) {
	    if (ACPI_FAILURE(acpi_SetSleepState(sc, state)))
		error = EINVAL;
	} else {
	    error = EINVAL;
	}
	break;
    default:
	if (error == 0)
	    error = EINVAL;
	break;
    }

out:
    ACPI_UNLOCK;
    return (error);
}

static int
acpi_supported_sleep_state_sysctl(SYSCTL_HANDLER_ARGS)
{
    char sleep_state[4];
    char buf[16];
    int error;
    UINT8 state, TypeA, TypeB;

    buf[0] = '\0';
    for (state = ACPI_STATE_S1; state < ACPI_S_STATES_MAX + 1; state++) {
	if (ACPI_SUCCESS(AcpiGetSleepTypeData(state, &TypeA, &TypeB))) {
	    sprintf(sleep_state, "S%d ", state);
	    strcat(buf, sleep_state);
	}
    }
    error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
    return (error);
}

static int
acpi_sleep_state_sysctl(SYSCTL_HANDLER_ARGS)
{
    char sleep_state[10];
    int error;
    u_int new_state, old_state;

    old_state = *(u_int *)oidp->oid_arg1;
    if (old_state > ACPI_S_STATES_MAX + 1) {
	strcpy(sleep_state, "unknown");
    } else {
	bzero(sleep_state, sizeof(sleep_state));
	strncpy(sleep_state, sleep_state_names[old_state],
		sizeof(sleep_state_names[old_state]));
    }
    error = sysctl_handle_string(oidp, sleep_state, sizeof(sleep_state), req);
    if (error == 0 && req->newptr != NULL) {
	new_state = ACPI_STATE_S0;
	for (; new_state <= ACPI_S_STATES_MAX + 1; new_state++) {
	    if (strncmp(sleep_state, sleep_state_names[new_state],
			sizeof(sleep_state)) == 0)
		break;
	}
	if (new_state <= ACPI_S_STATES_MAX + 1) {
	    if (new_state != old_state)
		*(u_int *)oidp->oid_arg1 = new_state;
	} else {
	    error = EINVAL;
	}
    }

    return (error);
}

/* Inform devctl(4) when we receive a Notify. */
void
acpi_UserNotify(const char *subsystem, ACPI_HANDLE h, uint8_t notify)
{
    char		notify_buf[16];
    ACPI_BUFFER		handle_buf;
    ACPI_STATUS		status;

    if (subsystem == NULL)
	return;

    handle_buf.Pointer = NULL;
    handle_buf.Length = ACPI_ALLOCATE_BUFFER;
    status = AcpiNsHandleToPathname(h, &handle_buf);
    if (ACPI_FAILURE(status))
	return;
    snprintf(notify_buf, sizeof(notify_buf), "notify=0x%02x", notify);
#if 0
    devctl_notify("ACPI", subsystem, handle_buf.Pointer, notify_buf);
#endif
    AcpiOsFree(handle_buf.Pointer);
}

#ifdef ACPI_DEBUG
/*
 * Support for parsing debug options from the kernel environment.
 *
 * Bits may be set in the AcpiDbgLayer and AcpiDbgLevel debug registers
 * by specifying the names of the bits in the debug.acpi.layer and
 * debug.acpi.level environment variables.  Bits may be unset by 
 * prefixing the bit name with !.
 */
struct debugtag
{
    char	*name;
    UINT32	value;
};

static struct debugtag	dbg_layer[] = {
    {"ACPI_UTILITIES",		ACPI_UTILITIES},
    {"ACPI_HARDWARE",		ACPI_HARDWARE},
    {"ACPI_EVENTS",		ACPI_EVENTS},
    {"ACPI_TABLES",		ACPI_TABLES},
    {"ACPI_NAMESPACE",		ACPI_NAMESPACE},
    {"ACPI_PARSER",		ACPI_PARSER},
    {"ACPI_DISPATCHER",		ACPI_DISPATCHER},
    {"ACPI_EXECUTER",		ACPI_EXECUTER},
    {"ACPI_RESOURCES",		ACPI_RESOURCES},
    {"ACPI_CA_DEBUGGER",	ACPI_CA_DEBUGGER},
    {"ACPI_OS_SERVICES",	ACPI_OS_SERVICES},
    {"ACPI_CA_DISASSEMBLER",	ACPI_CA_DISASSEMBLER},
    {"ACPI_ALL_COMPONENTS",	ACPI_ALL_COMPONENTS},

    {"ACPI_AC_ADAPTER",		ACPI_AC_ADAPTER},
    {"ACPI_BATTERY",		ACPI_BATTERY},
    {"ACPI_BUS",		ACPI_BUS},
    {"ACPI_BUTTON",		ACPI_BUTTON},
    {"ACPI_EC", 		ACPI_EC},
    {"ACPI_FAN",		ACPI_FAN},
    {"ACPI_POWERRES",		ACPI_POWERRES},
    {"ACPI_PROCESSOR",		ACPI_PROCESSOR},
    {"ACPI_THERMAL",		ACPI_THERMAL},
    {"ACPI_TIMER",		ACPI_TIMER},
    {"ACPI_ALL_DRIVERS",	ACPI_ALL_DRIVERS},
    {NULL, 0}
};

static struct debugtag dbg_level[] = {
    {"ACPI_LV_ERROR",		ACPI_LV_ERROR},
    {"ACPI_LV_WARN",		ACPI_LV_WARN},
    {"ACPI_LV_INIT",		ACPI_LV_INIT},
    {"ACPI_LV_DEBUG_OBJECT",	ACPI_LV_DEBUG_OBJECT},
    {"ACPI_LV_INFO",		ACPI_LV_INFO},
    {"ACPI_LV_ALL_EXCEPTIONS",	ACPI_LV_ALL_EXCEPTIONS},

    /* Trace verbosity level 1 [Standard Trace Level] */
    {"ACPI_LV_INIT_NAMES",	ACPI_LV_INIT_NAMES},
    {"ACPI_LV_PARSE",		ACPI_LV_PARSE},
    {"ACPI_LV_LOAD",		ACPI_LV_LOAD},
    {"ACPI_LV_DISPATCH",	ACPI_LV_DISPATCH},
    {"ACPI_LV_EXEC",		ACPI_LV_EXEC},
    {"ACPI_LV_NAMES",		ACPI_LV_NAMES},
    {"ACPI_LV_OPREGION",	ACPI_LV_OPREGION},
    {"ACPI_LV_BFIELD",		ACPI_LV_BFIELD},
    {"ACPI_LV_TABLES",		ACPI_LV_TABLES},
    {"ACPI_LV_VALUES",		ACPI_LV_VALUES},
    {"ACPI_LV_OBJECTS",		ACPI_LV_OBJECTS},
    {"ACPI_LV_RESOURCES",	ACPI_LV_RESOURCES},
    {"ACPI_LV_USER_REQUESTS",	ACPI_LV_USER_REQUESTS},
    {"ACPI_LV_PACKAGE",		ACPI_LV_PACKAGE},
    {"ACPI_LV_VERBOSITY1",	ACPI_LV_VERBOSITY1},

    /* Trace verbosity level 2 [Function tracing and memory allocation] */
    {"ACPI_LV_ALLOCATIONS",	ACPI_LV_ALLOCATIONS},
    {"ACPI_LV_FUNCTIONS",	ACPI_LV_FUNCTIONS},
    {"ACPI_LV_OPTIMIZATIONS",	ACPI_LV_OPTIMIZATIONS},
    {"ACPI_LV_VERBOSITY2",	ACPI_LV_VERBOSITY2},
    {"ACPI_LV_ALL",		ACPI_LV_ALL},

    /* Trace verbosity level 3 [Threading, I/O, and Interrupts] */
    {"ACPI_LV_MUTEX",		ACPI_LV_MUTEX},
    {"ACPI_LV_THREADS",		ACPI_LV_THREADS},
    {"ACPI_LV_IO",		ACPI_LV_IO},
    {"ACPI_LV_INTERRUPTS",	ACPI_LV_INTERRUPTS},
    {"ACPI_LV_VERBOSITY3",	ACPI_LV_VERBOSITY3},

    /* Exceptionally verbose output -- also used in the global "DebugLevel"  */
    {"ACPI_LV_AML_DISASSEMBLE",	ACPI_LV_AML_DISASSEMBLE},
    {"ACPI_LV_VERBOSE_INFO",	ACPI_LV_VERBOSE_INFO},
    {"ACPI_LV_FULL_TABLES",	ACPI_LV_FULL_TABLES},
    {"ACPI_LV_EVENTS",		ACPI_LV_EVENTS},
    {"ACPI_LV_VERBOSE",		ACPI_LV_VERBOSE},
    {NULL, 0}
};    

static void
acpi_parse_debug(char *cp, struct debugtag *tag, UINT32 *flag)
{
    char	*ep;
    int		i, l;
    int		set;

    while (*cp) {
	if (isspace(*cp)) {
	    cp++;
	    continue;
	}
	ep = cp;
	while (*ep && !isspace(*ep))
	    ep++;
	if (*cp == '!') {
	    set = 0;
	    cp++;
	    if (cp == ep)
		continue;
	} else {
	    set = 1;
	}
	l = ep - cp;
	for (i = 0; tag[i].name != NULL; i++) {
	    if (!strncmp(cp, tag[i].name, l)) {
		if (set)
		    *flag |= tag[i].value;
		else
		    *flag &= ~tag[i].value;
	    }
	}
	cp = ep;
    }
}

static void
acpi_set_debugging(void *junk)
{
    char	*layer, *level;

    if (cold) {
	AcpiDbgLayer = 0;
	AcpiDbgLevel = 0;
    }

    layer = getenv("debug.acpi.layer");
    level = getenv("debug.acpi.level");
    if (layer == NULL && level == NULL)
	return;

    printf("ACPI set debug");
    if (layer != NULL) {
	if (strcmp("NONE", layer) != 0)
	    printf(" layer '%s'", layer);
	acpi_parse_debug(layer, &dbg_layer[0], &AcpiDbgLayer);
	freeenv(layer);
    }
    if (level != NULL) {
	if (strcmp("NONE", level) != 0)
	    printf(" level '%s'", level);
	acpi_parse_debug(level, &dbg_level[0], &AcpiDbgLevel);
	freeenv(level);
    }
    printf("\n");
}
SYSINIT(acpi_debugging, SI_SUB_TUNABLES, SI_ORDER_ANY, acpi_set_debugging,
	NULL);

static int
acpi_debug_sysctl(SYSCTL_HANDLER_ARGS)
{
    int		 error, *dbg;
    struct	 debugtag *tag;
    struct	 sbuf sb;

    if (sbuf_new(&sb, NULL, 128, SBUF_AUTOEXTEND) == NULL)
	return (ENOMEM);
    if (strcmp(oidp->oid_arg1, "debug.acpi.layer") == 0) {
	tag = &dbg_layer[0];
	dbg = &AcpiDbgLayer;
    } else {
	tag = &dbg_level[0];
	dbg = &AcpiDbgLevel;
    }

    /* Get old values if this is a get request. */
    if (*dbg == 0) {
	sbuf_cpy(&sb, "NONE");
    } else if (req->newptr == NULL) {
	for (; tag->name != NULL; tag++) {
	    if ((*dbg & tag->value) == tag->value)
		sbuf_printf(&sb, "%s ", tag->name);
	}
    }
    sbuf_trim(&sb);
    sbuf_finish(&sb);

    /* Copy out the old values to the user. */
    error = SYSCTL_OUT(req, sbuf_data(&sb), sbuf_len(&sb));
    sbuf_delete(&sb);

    /* If the user is setting a string, parse it. */
    if (error == 0 && req->newptr != NULL) {
	*dbg = 0;
	/* XXX setenv((char *)oidp->oid_arg1, (char *)req->newptr); */
	acpi_set_debugging(NULL);
    }

    return (error);
}
SYSCTL_PROC(_debug_acpi, OID_AUTO, layer, CTLFLAG_RW | CTLTYPE_STRING,
	    "debug.acpi.layer", 0, acpi_debug_sysctl, "A", "");
SYSCTL_PROC(_debug_acpi, OID_AUTO, level, CTLFLAG_RW | CTLTYPE_STRING,
	    "debug.acpi.level", 0, acpi_debug_sysctl, "A", "");
#endif

static int
acpi_pm_func(u_long cmd, void *arg, ...)
{
	int	state, acpi_state;
	int	error;
	struct	acpi_softc *sc;
	va_list	ap;

	error = 0;
	switch (cmd) {
	case POWER_CMD_SUSPEND:
		sc = (struct acpi_softc *)arg;
		if (sc == NULL) {
			error = EINVAL;
			goto out;
		}

		va_start(ap, arg);
		state = va_arg(ap, int);
		va_end(ap);	

		switch (state) {
		case POWER_SLEEP_STATE_STANDBY:
			acpi_state = sc->acpi_standby_sx;
			break;
		case POWER_SLEEP_STATE_SUSPEND:
			acpi_state = sc->acpi_suspend_sx;
			break;
		case POWER_SLEEP_STATE_HIBERNATE:
			acpi_state = ACPI_STATE_S4;
			break;
		default:
			error = EINVAL;
			goto out;
		}

		acpi_SetSleepState(sc, acpi_state);
		break;
	default:
		error = EINVAL;
		goto out;
	}

out:
	return (error);
}

static void
acpi_pm_register(void *arg)
{
    if (!cold || resource_disabled("acpi", 0))
	return;

    power_pm_register(POWER_PM_TYPE_ACPI, acpi_pm_func, NULL);
}

SYSINIT(power, SI_SUB_KLD, SI_ORDER_ANY, acpi_pm_register, 0);
