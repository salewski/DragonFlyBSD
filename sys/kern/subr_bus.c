/*
 * Copyright (c) 1997,1998 Doug Rabson
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
 * $FreeBSD: src/sys/kern/subr_bus.c,v 1.54.2.9 2002/10/10 15:13:32 jhb Exp $
 * $DragonFly: src/sys/kern/subr_bus.c,v 1.24 2005/02/17 13:59:36 joerg Exp $
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#ifdef DEVICE_SYSCTLS
#include <sys/sysctl.h>
#endif
#include <sys/kobj.h>
#include <sys/bus_private.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/stdarg.h>	/* for device_printf() */

MALLOC_DEFINE(M_BUS, "bus", "Bus data structures");

#ifdef BUS_DEBUG
#define PDEBUG(a)	(printf("%s:%d: ", __func__, __LINE__), printf a, printf("\n"))
#define DEVICENAME(d)	((d)? device_get_name(d): "no device")
#define DRIVERNAME(d)	((d)? d->name : "no driver")
#define DEVCLANAME(d)	((d)? d->name : "no devclass")

/* Produce the indenting, indent*2 spaces plus a '.' ahead of that to 
 * prevent syslog from deleting initial spaces
 */
#define indentprintf(p)	do { int iJ; printf("."); for (iJ=0; iJ<indent; iJ++) printf("  "); printf p ; } while(0)

static void	print_device_short(device_t dev, int indent);
static void	print_device(device_t dev, int indent);
void		print_device_tree_short(device_t dev, int indent);
void		print_device_tree(device_t dev, int indent);
static void	print_driver_short(driver_t *driver, int indent);
static void	print_driver(driver_t *driver, int indent);
static void	print_driver_list(driver_list_t drivers, int indent);
static void	print_devclass_short(devclass_t dc, int indent);
static void	print_devclass(devclass_t dc, int indent);
void		print_devclass_list_short(void);
void		print_devclass_list(void);

#else
/* Make the compiler ignore the function calls */
#define PDEBUG(a)			/* nop */
#define DEVICENAME(d)			/* nop */
#define DRIVERNAME(d)			/* nop */
#define DEVCLANAME(d)			/* nop */

#define print_device_short(d,i)		/* nop */
#define print_device(d,i)		/* nop */
#define print_device_tree_short(d,i)	/* nop */
#define print_device_tree(d,i)		/* nop */
#define print_driver_short(d,i)		/* nop */
#define print_driver(d,i)		/* nop */
#define print_driver_list(d,i)		/* nop */
#define print_devclass_short(d,i)	/* nop */
#define print_devclass(d,i)		/* nop */
#define print_devclass_list_short()	/* nop */
#define print_devclass_list()		/* nop */
#endif

#ifdef DEVICE_SYSCTLS
static void	device_register_oids(device_t dev);
static void	device_unregister_oids(device_t dev);
#endif

kobj_method_t null_methods[] = {
	{ 0, 0 }
};

DEFINE_CLASS(null, null_methods, 0);

/*
 * Devclass implementation
 */

static devclass_list_t devclasses = TAILQ_HEAD_INITIALIZER(devclasses);

static devclass_t
devclass_find_internal(const char *classname, const char *parentname,
		       int create)
{
	devclass_t dc;

	PDEBUG(("looking for %s", classname));
	if (classname == NULL)
		return(NULL);

	TAILQ_FOREACH(dc, &devclasses, link)
		if (!strcmp(dc->name, classname))
			break;

	if (create && !dc) {
		PDEBUG(("creating %s", classname));
		dc = malloc(sizeof(struct devclass) + strlen(classname) + 1,
			    M_BUS, M_INTWAIT | M_ZERO);
		if (!dc)
			return(NULL);
		dc->parent = NULL;
		dc->name = (char*) (dc + 1);
		strcpy(dc->name, classname);
		dc->devices = NULL;
		dc->maxunit = 0;
		TAILQ_INIT(&dc->drivers);
		TAILQ_INSERT_TAIL(&devclasses, dc, link);
	}
	if (parentname && dc && !dc->parent)
		dc->parent = devclass_find_internal(parentname, NULL, FALSE);

	return(dc);
}

devclass_t
devclass_create(const char *classname)
{
	return(devclass_find_internal(classname, NULL, TRUE));
}

devclass_t
devclass_find(const char *classname)
{
	return(devclass_find_internal(classname, NULL, FALSE));
}

int
devclass_add_driver(devclass_t dc, driver_t *driver)
{
	driverlink_t dl;
	int i;

	PDEBUG(("%s", DRIVERNAME(driver)));

	dl = malloc(sizeof *dl, M_BUS, M_INTWAIT | M_ZERO);
	if (!dl)
		return(ENOMEM);

	/*
	 * Compile the driver's methods. Also increase the reference count
	 * so that the class doesn't get freed when the last instance
	 * goes. This means we can safely use static methods and avoids a
	 * double-free in devclass_delete_driver.
	 */
	kobj_class_instantiate(driver);

	/*
	 * Make sure the devclass which the driver is implementing exists.
	 */
	devclass_find_internal(driver->name, NULL, TRUE);

	dl->driver = driver;
	TAILQ_INSERT_TAIL(&dc->drivers, dl, link);

	/*
	 * Call BUS_DRIVER_ADDED for any existing busses in this class.
	 */
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			BUS_DRIVER_ADDED(dc->devices[i], driver);

	return(0);
}

int
devclass_delete_driver(devclass_t busclass, driver_t *driver)
{
	devclass_t dc = devclass_find(driver->name);
	driverlink_t dl;
	device_t dev;
	int i;
	int error;

	PDEBUG(("%s from devclass %s", driver->name, DEVCLANAME(busclass)));

	if (!dc)
		return(0);

	/*
	 * Find the link structure in the bus' list of drivers.
	 */
	TAILQ_FOREACH(dl, &busclass->drivers, link)
		if (dl->driver == driver)
			break;

	if (!dl) {
		PDEBUG(("%s not found in %s list", driver->name, busclass->name));
		return(ENOENT);
	}

	/*
	 * Disassociate from any devices.  We iterate through all the
	 * devices in the devclass of the driver and detach any which are
	 * using the driver and which have a parent in the devclass which
	 * we are deleting from.
	 *
	 * Note that since a driver can be in multiple devclasses, we
	 * should not detach devices which are not children of devices in
	 * the affected devclass.
	 */
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i]) {
			dev = dc->devices[i];
			if (dev->driver == driver && dev->parent &&
			    dev->parent->devclass == busclass) {
				if ((error = device_detach(dev)) != 0)
					return(error);
				device_set_driver(dev, NULL);
		    	}
		}

	TAILQ_REMOVE(&busclass->drivers, dl, link);
	free(dl, M_BUS);

	kobj_class_uninstantiate(driver);

	return(0);
}

static driverlink_t
devclass_find_driver_internal(devclass_t dc, const char *classname)
{
	driverlink_t dl;

	PDEBUG(("%s in devclass %s", classname, DEVCLANAME(dc)));

	TAILQ_FOREACH(dl, &dc->drivers, link)
		if (!strcmp(dl->driver->name, classname))
			return(dl);

	PDEBUG(("not found"));
	return(NULL);
}

kobj_class_t
devclass_find_driver(devclass_t dc, const char *classname)
{
	driverlink_t dl;

	dl = devclass_find_driver_internal(dc, classname);
	if (dl)
		return(dl->driver);
	else
		return(NULL);
}

const char *
devclass_get_name(devclass_t dc)
{
	return(dc->name);
}

device_t
devclass_get_device(devclass_t dc, int unit)
{
	if (dc == NULL || unit < 0 || unit >= dc->maxunit)
		return(NULL);
	return(dc->devices[unit]);
}

void *
devclass_get_softc(devclass_t dc, int unit)
{
	device_t dev;

	dev = devclass_get_device(dc, unit);
	if (!dev)
		return(NULL);

	return(device_get_softc(dev));
}

int
devclass_get_devices(devclass_t dc, device_t **devlistp, int *devcountp)
{
	int i;
	int count;
	device_t *list;
    
	count = 0;
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			count++;

	list = malloc(count * sizeof(device_t), M_TEMP, M_INTWAIT | M_ZERO);
	if (list == NULL)
		return(ENOMEM);

	count = 0;
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i]) {
			list[count] = dc->devices[i];
			count++;
		}

	*devlistp = list;
	*devcountp = count;

	return(0);
}

int
devclass_get_maxunit(devclass_t dc)
{
	return(dc->maxunit);
}

void
devclass_set_parent(devclass_t dc, devclass_t pdc)
{
        dc->parent = pdc;
}

devclass_t
devclass_get_parent(devclass_t dc)
{
	return(dc->parent);
}

static int
devclass_alloc_unit(devclass_t dc, int *unitp)
{
	int unit = *unitp;

	PDEBUG(("unit %d in devclass %s", unit, DEVCLANAME(dc)));

	/* If we have been given a wired unit number, check for existing device */
	if (unit != -1) {
		if (unit >= 0 && unit < dc->maxunit &&
		    dc->devices[unit] != NULL) {
			if (bootverbose)
				printf("%s-: %s%d exists, using next available unit number\n",
				       dc->name, dc->name, unit);
			/* find the next available slot */
			while (++unit < dc->maxunit && dc->devices[unit] != NULL)
				;
		}
	} else {
		/* Unwired device, find the next available slot for it */
		unit = 0;
		while (unit < dc->maxunit && dc->devices[unit] != NULL)
			unit++;
	}

	/*
	 * We've selected a unit beyond the length of the table, so let's
	 * extend the table to make room for all units up to and including
	 * this one.
	 */
	if (unit >= dc->maxunit) {
		device_t *newlist;
		int newsize;

		newsize = roundup((unit + 1), MINALLOCSIZE / sizeof(device_t));
		newlist = malloc(sizeof(device_t) * newsize, M_BUS,
				 M_INTWAIT | M_ZERO);
		if (newlist == NULL)
			return(ENOMEM);
		bcopy(dc->devices, newlist, sizeof(device_t) * dc->maxunit);
		if (dc->devices)
			free(dc->devices, M_BUS);
		dc->devices = newlist;
		dc->maxunit = newsize;
	}
	PDEBUG(("now: unit %d in devclass %s", unit, DEVCLANAME(dc)));

	*unitp = unit;
	return(0);
}

static int
devclass_add_device(devclass_t dc, device_t dev)
{
	int buflen, error;

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	buflen = strlen(dc->name) + 5;
	dev->nameunit = malloc(buflen, M_BUS, M_INTWAIT | M_ZERO);
	if (!dev->nameunit)
		return(ENOMEM);

	if ((error = devclass_alloc_unit(dc, &dev->unit)) != 0) {
		free(dev->nameunit, M_BUS);
		dev->nameunit = NULL;
		return(error);
	}
	dc->devices[dev->unit] = dev;
	dev->devclass = dc;
	snprintf(dev->nameunit, buflen, "%s%d", dc->name, dev->unit);

#ifdef DEVICE_SYSCTLS
	device_register_oids(dev);
#endif

	return(0);
}

static int
devclass_delete_device(devclass_t dc, device_t dev)
{
	if (!dc || !dev)
		return(0);

	PDEBUG(("%s in devclass %s", DEVICENAME(dev), DEVCLANAME(dc)));

	if (dev->devclass != dc || dc->devices[dev->unit] != dev)
		panic("devclass_delete_device: inconsistent device class");
	dc->devices[dev->unit] = NULL;
	if (dev->flags & DF_WILDCARD)
		dev->unit = -1;
	dev->devclass = NULL;
	free(dev->nameunit, M_BUS);
	dev->nameunit = NULL;

#ifdef DEVICE_SYSCTLS
	device_unregister_oids(dev);
#endif

	return(0);
}

static device_t
make_device(device_t parent, const char *name, int unit)
{
	device_t dev;
	devclass_t dc;

	PDEBUG(("%s at %s as unit %d", name, DEVICENAME(parent), unit));

	if (name != NULL) {
		dc = devclass_find_internal(name, NULL, TRUE);
		if (!dc) {
			printf("make_device: can't find device class %s\n", name);
			return(NULL);
		}
	} else
		dc = NULL;

	dev = malloc(sizeof(struct device), M_BUS, M_INTWAIT | M_ZERO);
	if (!dev)
		return(0);

	dev->parent = parent;
	TAILQ_INIT(&dev->children);
	kobj_init((kobj_t) dev, &null_class);
	dev->driver = NULL;
	dev->devclass = NULL;
	dev->unit = unit;
	dev->nameunit = NULL;
	dev->desc = NULL;
	dev->busy = 0;
	dev->devflags = 0;
	dev->flags = DF_ENABLED;
	dev->order = 0;
	if (unit == -1)
		dev->flags |= DF_WILDCARD;
	if (name) {
		dev->flags |= DF_FIXEDCLASS;
		if (devclass_add_device(dc, dev) != 0) {
			kobj_delete((kobj_t)dev, M_BUS);
			return(NULL);
		}
    	}
	dev->ivars = NULL;
	dev->softc = NULL;

	dev->state = DS_NOTPRESENT;

	return(dev);
}

static int
device_print_child(device_t dev, device_t child)
{
	int retval = 0;

	if (device_is_alive(child))
		retval += BUS_PRINT_CHILD(dev, child);
	else
		retval += device_printf(child, " not found\n");

	return(retval);
}

device_t
device_add_child(device_t dev, const char *name, int unit)
{
	return device_add_child_ordered(dev, 0, name, unit);
}

device_t
device_add_child_ordered(device_t dev, int order, const char *name, int unit)
{
	device_t child;
	device_t place;

	PDEBUG(("%s at %s with order %d as unit %d", name, DEVICENAME(dev),
		order, unit));

	child = make_device(dev, name, unit);
	if (child == NULL)
		return child;
	child->order = order;

	TAILQ_FOREACH(place, &dev->children, link)
		if (place->order > order)
			break;

	if (place) {
		/*
		 * The device 'place' is the first device whose order is
		 * greater than the new child.
		 */
		TAILQ_INSERT_BEFORE(place, child, link);
	} else {
		/*
		 * The new child's order is greater or equal to the order of
		 * any existing device. Add the child to the tail of the list.
		 */
		TAILQ_INSERT_TAIL(&dev->children, child, link);
    	}

	return(child);
}

int
device_delete_child(device_t dev, device_t child)
{
	int error;
	device_t grandchild;

	PDEBUG(("%s from %s", DEVICENAME(child), DEVICENAME(dev)));

	/* remove children first */
	while ( (grandchild = TAILQ_FIRST(&child->children)) ) {
        	error = device_delete_child(child, grandchild);
		if (error)
			return(error);
	}

	if ((error = device_detach(child)) != 0)
		return(error);
	if (child->devclass)
		devclass_delete_device(child->devclass, child);
	TAILQ_REMOVE(&dev->children, child, link);
	device_set_desc(child, NULL);
	kobj_delete((kobj_t)child, M_BUS);

	return(0);
}

/*
 * Find only devices attached to this bus.
 */
device_t
device_find_child(device_t dev, const char *classname, int unit)
{
	devclass_t dc;
	device_t child;

	dc = devclass_find(classname);
	if (!dc)
		return(NULL);

	child = devclass_get_device(dc, unit);
	if (child && child->parent == dev)
		return(child);
	return(NULL);
}

static driverlink_t
first_matching_driver(devclass_t dc, device_t dev)
{
	if (dev->devclass)
		return(devclass_find_driver_internal(dc, dev->devclass->name));
	else
		return(TAILQ_FIRST(&dc->drivers));
}

static driverlink_t
next_matching_driver(devclass_t dc, device_t dev, driverlink_t last)
{
	if (dev->devclass) {
		driverlink_t dl;
		for (dl = TAILQ_NEXT(last, link); dl; dl = TAILQ_NEXT(dl, link))
			if (!strcmp(dev->devclass->name, dl->driver->name))
				return(dl);
		return(NULL);
	} else
		return(TAILQ_NEXT(last, link));
}

static int
device_probe_child(device_t dev, device_t child)
{
	devclass_t dc;
	driverlink_t best = 0;
	driverlink_t dl;
	int result, pri = 0;
	int hasclass = (child->devclass != 0);

	dc = dev->devclass;
	if (!dc)
		panic("device_probe_child: parent device has no devclass");

	if (child->state == DS_ALIVE)
		return(0);

	for (; dc; dc = dc->parent) {
    		for (dl = first_matching_driver(dc, child); dl;
		     dl = next_matching_driver(dc, child, dl)) {
			PDEBUG(("Trying %s", DRIVERNAME(dl->driver)));
			device_set_driver(child, dl->driver);
			if (!hasclass)
				device_set_devclass(child, dl->driver->name);
			result = DEVICE_PROBE(child);
			if (!hasclass)
				device_set_devclass(child, 0);

			/*
			 * If the driver returns SUCCESS, there can be
			 * no higher match for this device.
			 */
			if (result == 0) {
				best = dl;
				pri = 0;
				break;
			}

			/*
			 * The driver returned an error so it
			 * certainly doesn't match.
			 */
			if (result > 0) {
				device_set_driver(child, 0);
				continue;
			}

			/*
			 * A priority lower than SUCCESS, remember the
			 * best matching driver. Initialise the value
			 * of pri for the first match.
			 */
			if (best == 0 || result > pri) {
				best = dl;
				pri = result;
				continue;
			}
	        }
		/*
	         * If we have unambiguous match in this devclass,
	         * don't look in the parent.
	         */
	        if (best && pri == 0)
	    	        break;
	}

	/*
	 * If we found a driver, change state and initialise the devclass.
	 */
	if (best) {
		if (!child->devclass)
			device_set_devclass(child, best->driver->name);
		device_set_driver(child, best->driver);
		if (pri < 0) {
			/*
			 * A bit bogus. Call the probe method again to make
			 * sure that we have the right description.
			 */
			DEVICE_PROBE(child);
		}
		child->state = DS_ALIVE;
		return(0);
	}

	return(ENXIO);
}

device_t
device_get_parent(device_t dev)
{
	return dev->parent;
}

int
device_get_children(device_t dev, device_t **devlistp, int *devcountp)
{
	int count;
	device_t child;
	device_t *list;
    
	count = 0;
	TAILQ_FOREACH(child, &dev->children, link)
		count++;

	list = malloc(count * sizeof(device_t), M_TEMP, M_INTWAIT | M_ZERO);
	if (!list)
		return(ENOMEM);

	count = 0;
	TAILQ_FOREACH(child, &dev->children, link) {
		list[count] = child;
		count++;
	}

	*devlistp = list;
	*devcountp = count;

	return(0);
}

driver_t *
device_get_driver(device_t dev)
{
	return(dev->driver);
}

devclass_t
device_get_devclass(device_t dev)
{
	return(dev->devclass);
}

const char *
device_get_name(device_t dev)
{
	if (dev->devclass)
		return devclass_get_name(dev->devclass);
	return(NULL);
}

const char *
device_get_nameunit(device_t dev)
{
	return(dev->nameunit);
}

int
device_get_unit(device_t dev)
{
	return(dev->unit);
}

const char *
device_get_desc(device_t dev)
{
	return(dev->desc);
}

uint32_t
device_get_flags(device_t dev)
{
	return(dev->devflags);
}

int
device_print_prettyname(device_t dev)
{
	const char *name = device_get_name(dev);

	if (name == 0)
		return printf("unknown: ");
	else
		return printf("%s%d: ", name, device_get_unit(dev));
}

int
device_printf(device_t dev, const char * fmt, ...)
{
	__va_list ap;
	int retval;

	retval = device_print_prettyname(dev);
	__va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	__va_end(ap);
	return retval;
}

static void
device_set_desc_internal(device_t dev, const char* desc, int copy)
{
	if (dev->desc && (dev->flags & DF_DESCMALLOCED)) {
		free(dev->desc, M_BUS);
		dev->flags &= ~DF_DESCMALLOCED;
		dev->desc = NULL;
	}

	if (copy && desc) {
		dev->desc = malloc(strlen(desc) + 1, M_BUS, M_INTWAIT);
		if (dev->desc) {
			strcpy(dev->desc, desc);
			dev->flags |= DF_DESCMALLOCED;
		}
	} else
		/* Avoid a -Wcast-qual warning */
		dev->desc = (char *)(uintptr_t) desc;

#ifdef DEVICE_SYSCTLS
	{
		struct sysctl_oid *oid = &dev->oid[1];
		oid->oid_arg1 = dev->desc ? dev->desc : "";
		oid->oid_arg2 = dev->desc ? strlen(dev->desc) : 0;
	}
#endif
}

void
device_set_desc(device_t dev, const char* desc)
{
	device_set_desc_internal(dev, desc, FALSE);
}

void
device_set_desc_copy(device_t dev, const char* desc)
{
	device_set_desc_internal(dev, desc, TRUE);
}

void
device_set_flags(device_t dev, uint32_t flags)
{
	dev->devflags = flags;
}

void *
device_get_softc(device_t dev)
{
	return dev->softc;
}

void
device_set_softc(device_t dev, void *softc)
{
	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC))
		free(dev->softc, M_BUS);
	dev->softc = softc;
	if (dev->softc)
		dev->flags |= DF_EXTERNALSOFTC;
	else
		dev->flags &= ~DF_EXTERNALSOFTC;
}

void *
device_get_ivars(device_t dev)
{
	return dev->ivars;
}

void
device_set_ivars(device_t dev, void * ivars)
{
	if (!dev)
		return;

	dev->ivars = ivars;
}

device_state_t
device_get_state(device_t dev)
{
	return(dev->state);
}

void
device_enable(device_t dev)
{
	dev->flags |= DF_ENABLED;
}

void
device_disable(device_t dev)
{
	dev->flags &= ~DF_ENABLED;
}

/*
 * YYY cannot block
 */
void
device_busy(device_t dev)
{
	if (dev->state < DS_ATTACHED)
		panic("device_busy: called for unattached device");
	if (dev->busy == 0 && dev->parent)
		device_busy(dev->parent);
	dev->busy++;
	dev->state = DS_BUSY;
}

/*
 * YYY cannot block
 */
void
device_unbusy(device_t dev)
{
	if (dev->state != DS_BUSY)
		panic("device_unbusy: called for non-busy device");
	dev->busy--;
	if (dev->busy == 0) {
		if (dev->parent)
			device_unbusy(dev->parent);
		dev->state = DS_ATTACHED;
	}
}

void
device_quiet(device_t dev)
{
	dev->flags |= DF_QUIET;
}

void
device_verbose(device_t dev)
{
	dev->flags &= ~DF_QUIET;
}

int
device_is_quiet(device_t dev)
{
	return((dev->flags & DF_QUIET) != 0);
}

int
device_is_enabled(device_t dev)
{
	return((dev->flags & DF_ENABLED) != 0);
}

int
device_is_alive(device_t dev)
{
	return(dev->state >= DS_ALIVE);
}

int
device_is_attached(device_t dev)
{
	return(dev->state >= DS_ATTACHED);
}

int
device_set_devclass(device_t dev, const char *classname)
{
	devclass_t dc;

	if (!classname) {
		if (dev->devclass)
			devclass_delete_device(dev->devclass, dev);
		return(0);
	}

	if (dev->devclass) {
		printf("device_set_devclass: device class already set\n");
		return(EINVAL);
	}

	dc = devclass_find_internal(classname, NULL, TRUE);
	if (!dc)
		return(ENOMEM);

	return(devclass_add_device(dc, dev));
}

int
device_set_driver(device_t dev, driver_t *driver)
{
	if (dev->state >= DS_ATTACHED)
		return(EBUSY);

	if (dev->driver == driver)
		return(0);

	if (dev->softc && !(dev->flags & DF_EXTERNALSOFTC)) {
		free(dev->softc, M_BUS);
		dev->softc = NULL;
	}
	kobj_delete((kobj_t) dev, 0);
	dev->driver = driver;
	if (driver) {
		kobj_init((kobj_t) dev, (kobj_class_t) driver);
		if (!(dev->flags & DF_EXTERNALSOFTC)) {
			dev->softc = malloc(driver->size, M_BUS,
					    M_INTWAIT | M_ZERO);
			if (!dev->softc) {
				kobj_delete((kobj_t)dev, 0);
				kobj_init((kobj_t) dev, &null_class);
				dev->driver = NULL;
				return(ENOMEM);
	    		}
		}
	} else
		kobj_init((kobj_t) dev, &null_class);
	return(0);
}

int
device_probe_and_attach(device_t dev)
{
	device_t bus = dev->parent;
	int error = 0;
	int hasclass = (dev->devclass != 0);

	if (dev->state >= DS_ALIVE)
		return(0);

	if ((dev->flags & DF_ENABLED) == 0) {
		if (bootverbose) {
			device_print_prettyname(dev);
			printf("not probed (disabled)\n");
		}
		return(0);
	}

	error = device_probe_child(bus, dev);
	if (error) {
		if (!(dev->flags & DF_DONENOMATCH)) {
			BUS_PROBE_NOMATCH(bus, dev);
			dev->flags |= DF_DONENOMATCH;
		}
		return(error);
	}

	/*
	 * Output the exact device chain prior to the attach in case the  
	 * system locks up during attach, and generate the full info after
	 * the attach so correct irq and other information is displayed.
	 */
	if (bootverbose && !device_is_quiet(dev)) {
		device_t tmp;

		printf("%s", device_get_nameunit(dev));
		for (tmp = dev->parent; tmp; tmp = tmp->parent) {
		    const char *desc;

		    if ((desc = device_get_desc(tmp)) != NULL)
			printf(".%s[%s]", device_get_nameunit(tmp), desc);
		    else
			printf(".%s", device_get_nameunit(tmp));
		}
		printf("\n");
	}
	if (!device_is_quiet(dev))
		device_print_child(bus, dev);
	error = DEVICE_ATTACH(dev);
	if (error == 0) {
		dev->state = DS_ATTACHED;
		if (bootverbose && !device_is_quiet(dev))
			device_print_child(bus, dev);
	} else {
		printf("device_probe_and_attach: %s%d attach returned %d\n",
		       dev->driver->name, dev->unit, error);
		/* Unset the class that was set in device_probe_child */
		if (!hasclass)
			device_set_devclass(dev, 0);
		device_set_driver(dev, NULL);
		dev->state = DS_NOTPRESENT;
	}

	return(error);
}

int
device_detach(device_t dev)
{
	int error;

	PDEBUG(("%s", DEVICENAME(dev)));
	if (dev->state == DS_BUSY)
		return(EBUSY);
	if (dev->state != DS_ATTACHED)
		return(0);

	if ((error = DEVICE_DETACH(dev)) != 0)
		return(error);
	device_printf(dev, "detached\n");
	if (dev->parent)
		BUS_CHILD_DETACHED(dev->parent, dev);

	if (!(dev->flags & DF_FIXEDCLASS))
		devclass_delete_device(dev->devclass, dev);

	dev->state = DS_NOTPRESENT;
	device_set_driver(dev, NULL);

	return(0);
}

int
device_shutdown(device_t dev)
{
	if (dev->state < DS_ATTACHED)
		return 0;
	PDEBUG(("%s", DEVICENAME(dev)));
	return DEVICE_SHUTDOWN(dev);
}

int
device_set_unit(device_t dev, int unit)
{
	devclass_t dc;
	int err;

	dc = device_get_devclass(dev);
	if (unit < dc->maxunit && dc->devices[unit])
		return(EBUSY);
	err = devclass_delete_device(dc, dev);
	if (err)
		return(err);
	dev->unit = unit;
	err = devclass_add_device(dc, dev);
	return(err);
}

#ifdef DEVICE_SYSCTLS

/*
 * Sysctl nodes for devices.
 */

SYSCTL_NODE(_hw, OID_AUTO, devices, CTLFLAG_RW, 0, "A list of all devices");

static int
sysctl_handle_children(SYSCTL_HANDLER_ARGS)
{
	device_t dev = arg1;
	device_t child;
	int first = 1, error = 0;

	TAILQ_FOREACH(child, &dev->children, link)
		if (child->nameunit) {
			if (!first) {
				error = SYSCTL_OUT(req, ",", 1);
				if (error)
					return error;
			} else
				first = 0;
			error = SYSCTL_OUT(req, child->nameunit,
					   strlen(child->nameunit));
			if (error)
				return(error);
		}

	error = SYSCTL_OUT(req, "", 1);

	return(error);
}

static int
sysctl_handle_state(SYSCTL_HANDLER_ARGS)
{
	device_t dev = arg1;

	switch (dev->state) {
	case DS_NOTPRESENT:
		return SYSCTL_OUT(req, "notpresent", sizeof("notpresent"));
	case DS_ALIVE:
		return SYSCTL_OUT(req, "alive", sizeof("alive"));
	case DS_ATTACHED:
		return SYSCTL_OUT(req, "attached", sizeof("attached"));
	case DS_BUSY:
		return SYSCTL_OUT(req, "busy", sizeof("busy"));
	default:
		return (0);
	}
}

static void
device_register_oids(device_t dev)
{
	struct sysctl_oid* oid;

	oid = &dev->oid[0];
	bzero(oid, sizeof(*oid));
	oid->oid_parent = &sysctl__hw_devices_children;
	oid->oid_number = OID_AUTO;
	oid->oid_kind = CTLTYPE_NODE | CTLFLAG_RW;
	oid->oid_arg1 = &dev->oidlist[0];
	oid->oid_arg2 = 0;
	oid->oid_name = dev->nameunit;
	oid->oid_handler = 0;
	oid->oid_fmt = "N";
	SLIST_INIT(&dev->oidlist[0]);
	sysctl_register_oid(oid);

	oid = &dev->oid[1];
	bzero(oid, sizeof(*oid));
	oid->oid_parent = &dev->oidlist[0];
	oid->oid_number = OID_AUTO;
	oid->oid_kind = CTLTYPE_STRING | CTLFLAG_RD;
	oid->oid_arg1 = dev->desc ? dev->desc : "";
	oid->oid_arg2 = dev->desc ? strlen(dev->desc) : 0;
	oid->oid_name = "desc";
	oid->oid_handler = sysctl_handle_string;
	oid->oid_fmt = "A";
	sysctl_register_oid(oid);

	oid = &dev->oid[2];
	bzero(oid, sizeof(*oid));
	oid->oid_parent = &dev->oidlist[0];
	oid->oid_number = OID_AUTO;
	oid->oid_kind = CTLTYPE_INT | CTLFLAG_RD;
	oid->oid_arg1 = dev;
	oid->oid_arg2 = 0;
	oid->oid_name = "children";
	oid->oid_handler = sysctl_handle_children;
	oid->oid_fmt = "A";
	sysctl_register_oid(oid);

	oid = &dev->oid[3];
	bzero(oid, sizeof(*oid));
	oid->oid_parent = &dev->oidlist[0];
	oid->oid_number = OID_AUTO;
	oid->oid_kind = CTLTYPE_INT | CTLFLAG_RD;
	oid->oid_arg1 = dev;
	oid->oid_arg2 = 0;
	oid->oid_name = "state";
	oid->oid_handler = sysctl_handle_state;
	oid->oid_fmt = "A";
	sysctl_register_oid(oid);
}

static void
device_unregister_oids(device_t dev)
{
	sysctl_unregister_oid(&dev->oid[0]);
	sysctl_unregister_oid(&dev->oid[1]);
	sysctl_unregister_oid(&dev->oid[2]);
}

#endif

/*======================================*/
/*
 * Access functions for device resources.
 */

/* Supplied by config(8) in ioconf.c */
extern struct config_device config_devtab[];
extern int devtab_count;

/* Runtime version */
struct config_device *devtab = config_devtab;

static int
resource_new_name(const char *name, int unit)
{
	struct config_device *new;

	new = malloc((devtab_count + 1) * sizeof(*new), M_TEMP,
		     M_INTWAIT | M_ZERO);
	if (new == NULL)
		return(-1);
	if (devtab && devtab_count > 0)
		bcopy(devtab, new, devtab_count * sizeof(*new));
	new[devtab_count].name = malloc(strlen(name) + 1, M_TEMP, M_INTWAIT);
	if (new[devtab_count].name == NULL) {
		free(new, M_TEMP);
		return(-1);
	}
	strcpy(new[devtab_count].name, name);
	new[devtab_count].unit = unit;
	new[devtab_count].resource_count = 0;
	new[devtab_count].resources = NULL;
	if (devtab && devtab != config_devtab)
		free(devtab, M_TEMP);
	devtab = new;
	return devtab_count++;
}

static int
resource_new_resname(int j, const char *resname, resource_type type)
{
	struct config_resource *new;
	int i;

	i = devtab[j].resource_count;
	new = malloc((i + 1) * sizeof(*new), M_TEMP, M_INTWAIT | M_ZERO);
	if (new == NULL)
		return(-1);
	if (devtab[j].resources && i > 0)
		bcopy(devtab[j].resources, new, i * sizeof(*new));
	new[i].name = malloc(strlen(resname) + 1, M_TEMP, M_INTWAIT);
	if (new[i].name == NULL) {
		free(new, M_TEMP);
		return(-1);
	}
	strcpy(new[i].name, resname);
	new[i].type = type;
	if (devtab[j].resources)
		free(devtab[j].resources, M_TEMP);
	devtab[j].resources = new;
	devtab[j].resource_count = i + 1;
	return(i);
}

static int
resource_match_string(int i, const char *resname, const char *value)
{
	int j;
	struct config_resource *res;

	for (j = 0, res = devtab[i].resources;
	     j < devtab[i].resource_count; j++, res++)
		if (!strcmp(res->name, resname)
		    && res->type == RES_STRING
		    && !strcmp(res->u.stringval, value))
			return(j);
	return(-1);
}

static int
resource_find(const char *name, int unit, const char *resname, 
	      struct config_resource **result)
{
	int i, j;
	struct config_resource *res;

	/*
	 * First check specific instances, then generic.
	 */
	for (i = 0; i < devtab_count; i++) {
		if (devtab[i].unit < 0)
			continue;
		if (!strcmp(devtab[i].name, name) && devtab[i].unit == unit) {
			res = devtab[i].resources;
			for (j = 0; j < devtab[i].resource_count; j++, res++)
				if (!strcmp(res->name, resname)) {
					*result = res;
					return(0);
				}
		}
	}
	for (i = 0; i < devtab_count; i++) {
		if (devtab[i].unit >= 0)
			continue;
		/* XXX should this `&& devtab[i].unit == unit' be here? */
		/* XXX if so, then the generic match does nothing */
		if (!strcmp(devtab[i].name, name) && devtab[i].unit == unit) {
			res = devtab[i].resources;
			for (j = 0; j < devtab[i].resource_count; j++, res++)
				if (!strcmp(res->name, resname)) {
					*result = res;
					return(0);
				}
		}
	}
	return(ENOENT);
}

int
resource_int_value(const char *name, int unit, const char *resname, int *result)
{
	int error;
	struct config_resource *res;

	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return(error);
	if (res->type != RES_INT)
		return(EFTYPE);
	*result = res->u.intval;
	return(0);
}

int
resource_long_value(const char *name, int unit, const char *resname,
		    long *result)
{
	int error;
	struct config_resource *res;

	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return(error);
	if (res->type != RES_LONG)
		return(EFTYPE);
	*result = res->u.longval;
	return(0);
}

int
resource_string_value(const char *name, int unit, const char *resname,
		      char **result)
{
	int error;
	struct config_resource *res;

	if ((error = resource_find(name, unit, resname, &res)) != 0)
		return(error);
	if (res->type != RES_STRING)
		return(EFTYPE);
	*result = res->u.stringval;
	return(0);
}

int
resource_query_string(int i, const char *resname, const char *value)
{
	if (i < 0)
		i = 0;
	else
		i = i + 1;
	for (; i < devtab_count; i++)
		if (resource_match_string(i, resname, value) >= 0)
			return(i);
	return(-1);
}

int
resource_locate(int i, const char *resname)
{
	if (i < 0)
		i = 0;
	else
		i = i + 1;
	for (; i < devtab_count; i++)
		if (!strcmp(devtab[i].name, resname))
			return(i);
	return(-1);
}

int
resource_count(void)
{
	return(devtab_count);
}

char *
resource_query_name(int i)
{
	return(devtab[i].name);
}

int
resource_query_unit(int i)
{
	return(devtab[i].unit);
}

static int
resource_create(const char *name, int unit, const char *resname,
		resource_type type, struct config_resource **result)
{
	int i, j;
	struct config_resource *res = NULL;

	for (i = 0; i < devtab_count; i++)
		if (!strcmp(devtab[i].name, name) && devtab[i].unit == unit) {
			res = devtab[i].resources;
			break;
		}
	if (res == NULL) {
		i = resource_new_name(name, unit);
		if (i < 0)
			return(ENOMEM);
		res = devtab[i].resources;
	}
	for (j = 0; j < devtab[i].resource_count; j++, res++)
		if (!strcmp(res->name, resname)) {
			*result = res;
			return(0);
		}
	j = resource_new_resname(i, resname, type);
	if (j < 0)
		return(ENOMEM);
	res = &devtab[i].resources[j];
	*result = res;
	return(0);
}

int
resource_set_int(const char *name, int unit, const char *resname, int value)
{
	int error;
	struct config_resource *res;

	error = resource_create(name, unit, resname, RES_INT, &res);
	if (error)
		return(error);
	if (res->type != RES_INT)
		return(EFTYPE);
	res->u.intval = value;
	return(0);
}

int
resource_set_long(const char *name, int unit, const char *resname, long value)
{
	int error;
	struct config_resource *res;

	error = resource_create(name, unit, resname, RES_LONG, &res);
	if (error)
		return(error);
	if (res->type != RES_LONG)
		return(EFTYPE);
	res->u.longval = value;
	return(0);
}

int
resource_set_string(const char *name, int unit, const char *resname,
		    const char *value)
{
	int error;
	struct config_resource *res;

	error = resource_create(name, unit, resname, RES_STRING, &res);
	if (error)
		return(error);
	if (res->type != RES_STRING)
		return(EFTYPE);
	if (res->u.stringval)
		free(res->u.stringval, M_TEMP);
	res->u.stringval = malloc(strlen(value) + 1, M_TEMP, M_INTWAIT);
	if (res->u.stringval == NULL)
		return(ENOMEM);
	strcpy(res->u.stringval, value);
	return(0);
}

static void
resource_cfgload(void *dummy __unused)
{
	struct config_resource *res, *cfgres;
	int i, j;
	int error;
	char *name, *resname;
	int unit;
	resource_type type;
	char *stringval;
	int config_devtab_count;

	config_devtab_count = devtab_count;
	devtab = NULL;
	devtab_count = 0;

	for (i = 0; i < config_devtab_count; i++) {
		name = config_devtab[i].name;
		unit = config_devtab[i].unit;

		for (j = 0; j < config_devtab[i].resource_count; j++) {
			cfgres = config_devtab[i].resources;
			resname = cfgres[j].name;
			type = cfgres[j].type;
			error = resource_create(name, unit, resname, type,
						&res);
			if (error) {
				printf("create resource %s%d: error %d\n",
					name, unit, error);
				continue;
			}
			if (res->type != type) {
				printf("type mismatch %s%d: %d != %d\n",
					name, unit, res->type, type);
				continue;
			}
			switch (type) {
			case RES_INT:
				res->u.intval = cfgres[j].u.intval;
				break;
			case RES_LONG:
				res->u.longval = cfgres[j].u.longval;
				break;
			case RES_STRING:
				if (res->u.stringval)
					free(res->u.stringval, M_TEMP);
				stringval = cfgres[j].u.stringval;
				res->u.stringval = malloc(strlen(stringval) + 1,
							  M_TEMP, M_INTWAIT);
				if (res->u.stringval == NULL)
					break;
				strcpy(res->u.stringval, stringval);
				break;
			default:
				panic("unknown resource type %d", type);
			}
		}
	}
}
SYSINIT(cfgload, SI_SUB_KMEM, SI_ORDER_ANY + 50, resource_cfgload, 0)


/*======================================*/
/*
 * Some useful method implementations to make life easier for bus drivers.
 */

void
resource_list_init(struct resource_list *rl)
{
	SLIST_INIT(rl);
}

void
resource_list_free(struct resource_list *rl)
{
	struct resource_list_entry *rle;

	while ((rle = SLIST_FIRST(rl)) != NULL) {
		if (rle->res)
			panic("resource_list_free: resource entry is busy");
		SLIST_REMOVE_HEAD(rl, link);
		free(rle, M_BUS);
	}
}

void
resource_list_add(struct resource_list *rl,
		  int type, int rid,
		  u_long start, u_long end, u_long count)
{
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (rle == NULL) {
		rle = malloc(sizeof(struct resource_list_entry), M_BUS,
			     M_INTWAIT);
		if (!rle)
			panic("resource_list_add: can't record entry");
		SLIST_INSERT_HEAD(rl, rle, link);
		rle->type = type;
		rle->rid = rid;
		rle->res = NULL;
	}

	if (rle->res)
		panic("resource_list_add: resource entry is busy");

	rle->start = start;
	rle->end = end;
	rle->count = count;
}

struct resource_list_entry*
resource_list_find(struct resource_list *rl,
		   int type, int rid)
{
	struct resource_list_entry *rle;

	SLIST_FOREACH(rle, rl, link)
		if (rle->type == type && rle->rid == rid)
			return(rle);
	return(NULL);
}

void
resource_list_delete(struct resource_list *rl,
		     int type, int rid)
{
	struct resource_list_entry *rle = resource_list_find(rl, type, rid);

	if (rle) {
		SLIST_REMOVE(rl, rle, resource_list_entry, link);
		free(rle, M_BUS);
	}
}

struct resource *
resource_list_alloc(struct resource_list *rl,
		    device_t bus, device_t child,
		    int type, int *rid,
		    u_long start, u_long end,
		    u_long count, u_int flags)
{
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);

	if (passthrough) {
		return(BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
					  type, rid,
					  start, end, count, flags));
	}

	rle = resource_list_find(rl, type, *rid);

	if (!rle)
		return(0);		/* no resource of that type/rid */
	if (rle->res)
		panic("resource_list_alloc: resource entry is busy");

	if (isdefault) {
		start = rle->start;
		count = max(count, rle->count);
		end = max(rle->end, start + count - 1);
	}

	rle->res = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
				      type, rid, start, end, count, flags);

	/*
	 * Record the new range.
	 */
	if (rle->res) {
		rle->start = rman_get_start(rle->res);
		rle->end = rman_get_end(rle->res);
		rle->count = count;
	}

	return(rle->res);
}

int
resource_list_release(struct resource_list *rl,
		      device_t bus, device_t child,
		      int type, int rid, struct resource *res)
{
	struct resource_list_entry *rle = 0;
	int passthrough = (device_get_parent(child) != bus);
	int error;

	if (passthrough) {
		return(BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
					    type, rid, res));
	}

	rle = resource_list_find(rl, type, rid);

	if (!rle)
		panic("resource_list_release: can't find resource");
	if (!rle->res)
		panic("resource_list_release: resource entry is not busy");

	error = BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
				     type, rid, res);
	if (error)
		return(error);

	rle->res = NULL;
	return(0);
}

int
resource_list_print_type(struct resource_list *rl, const char *name, int type,
			 const char *format)
{
	struct resource_list_entry *rle;
	int printed, retval;

	printed = 0;
	retval = 0;
	/* Yes, this is kinda cheating */
	SLIST_FOREACH(rle, rl, link) {
		if (rle->type == type) {
			if (printed == 0)
				retval += printf(" %s ", name);
			else
				retval += printf(",");
			printed++;
			retval += printf(format, rle->start);
			if (rle->count > 1) {
				retval += printf("-");
				retval += printf(format, rle->start +
						 rle->count - 1);
			}
		}
	}
	return(retval);
}

/*
 * Call DEVICE_IDENTIFY for each driver.
 */
int
bus_generic_probe(device_t dev)
{
	devclass_t dc = dev->devclass;
	driverlink_t dl;

	TAILQ_FOREACH(dl, &dc->drivers, link)
		DEVICE_IDENTIFY(dl->driver, dev);

	return(0);
}

int
bus_generic_attach(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link)
		device_probe_and_attach(child);

	return(0);
}

int
bus_generic_detach(device_t dev)
{
	device_t child;
	int error;

	if (dev->state != DS_ATTACHED)
		return(EBUSY);

	TAILQ_FOREACH(child, &dev->children, link)
		if ((error = device_detach(child)) != 0)
			return(error);

	return 0;
}

int
bus_generic_shutdown(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link)
		device_shutdown(child);

	return(0);
}

int
bus_generic_suspend(device_t dev)
{
	int error;
	device_t child, child2;

	TAILQ_FOREACH(child, &dev->children, link) {
		error = DEVICE_SUSPEND(child);
		if (error) {
			for (child2 = TAILQ_FIRST(&dev->children);
			     child2 && child2 != child; 
			     child2 = TAILQ_NEXT(child2, link))
				DEVICE_RESUME(child2);
			return(error);
		}
	}
	return(0);
}

int
bus_generic_resume(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->children, link)
		DEVICE_RESUME(child);
		/* if resume fails, there's nothing we can usefully do... */

	return(0);
}

int
bus_print_child_header(device_t dev, device_t child)
{
	int retval = 0;

	if (device_get_desc(child))
		retval += device_printf(child, "<%s>", device_get_desc(child));
	else
		retval += printf("%s", device_get_nameunit(child));
	if (bootverbose) {
		if (child->state != DS_ATTACHED)
			printf(" [tentative]");
		else
			printf(" [attached!]");
	}
	return(retval);
}

int
bus_print_child_footer(device_t dev, device_t child)
{
	return(printf(" on %s\n", device_get_nameunit(dev)));
}

int
bus_generic_print_child(device_t dev, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += bus_print_child_footer(dev, child);

	return(retval);
}

int
bus_generic_read_ivar(device_t dev, device_t child, int index, 
		      uintptr_t * result)
{
    return(ENOENT);
}

int
bus_generic_write_ivar(device_t dev, device_t child, int index, 
		       uintptr_t value)
{
    return(ENOENT);
}

struct resource_list *
bus_generic_get_resource_list(device_t dev, device_t child)
{
    return(NULL);
}

void
bus_generic_driver_added(device_t dev, driver_t *driver)
{
	device_t child;

	DEVICE_IDENTIFY(driver, dev);
	TAILQ_FOREACH(child, &dev->children, link)
		if (child->state == DS_NOTPRESENT)
			device_probe_and_attach(child);
}

int
bus_generic_setup_intr(device_t dev, device_t child, struct resource *irq, 
		       int flags, driver_intr_t *intr, void *arg,
		       void **cookiep)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_SETUP_INTR(dev->parent, child, irq, flags,
				      intr, arg, cookiep));
	else
		return(EINVAL);
}

int
bus_generic_teardown_intr(device_t dev, device_t child, struct resource *irq,
			  void *cookie)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_TEARDOWN_INTR(dev->parent, child, irq, cookie));
	else
		return(EINVAL);
}

struct resource *
bus_generic_alloc_resource(device_t dev, device_t child, int type, int *rid,
			   u_long start, u_long end, u_long count, u_int flags)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_ALLOC_RESOURCE(dev->parent, child, type, rid, 
					   start, end, count, flags));
	else
		return(NULL);
}

int
bus_generic_release_resource(device_t dev, device_t child, int type, int rid,
			     struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_RELEASE_RESOURCE(dev->parent, child, type, rid, r));
	else
		return(EINVAL);
}

int
bus_generic_activate_resource(device_t dev, device_t child, int type, int rid,
			      struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_ACTIVATE_RESOURCE(dev->parent, child, type, rid, r));
	else
		return(EINVAL);
}

int
bus_generic_deactivate_resource(device_t dev, device_t child, int type,
				int rid, struct resource *r)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_DEACTIVATE_RESOURCE(dev->parent, child, type, rid,
					       r));
	else
		return(EINVAL);
}

int
bus_generic_config_intr(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	/* Propagate up the bus hierarchy until someone handles it. */
	if (dev->parent)
		return(BUS_CONFIG_INTR(dev->parent, irq, trig, pol));
	else
		return(EINVAL);
}

int
bus_generic_rl_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct resource_list *rl = NULL;
	struct resource_list_entry *rle = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return(EINVAL);

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return(ENOENT);

	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return(0);
}

int
bus_generic_rl_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct resource_list *rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return(EINVAL);

	resource_list_add(rl, type, rid, start, (start + count - 1), count);

	return(0);
}

void
bus_generic_rl_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct resource_list *rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return;

	resource_list_delete(rl, type, rid);
}

int
bus_generic_rl_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct resource_list *rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return(EINVAL);

	return(resource_list_release(rl, dev, child, type, rid, r));
}

struct resource *
bus_generic_rl_alloc_resource(device_t dev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list *rl = NULL;

	rl = BUS_GET_RESOURCE_LIST(dev, child);
	if (!rl)
		return(NULL);

	return(resource_list_alloc(rl, dev, child, type, rid,
	    start, end, count, flags));
}

int
bus_generic_child_present(device_t bus, device_t child)
{
	return(BUS_CHILD_PRESENT(device_get_parent(bus), bus));
}


/*
 * Some convenience functions to make it easier for drivers to use the
 * resource-management functions.  All these really do is hide the
 * indirection through the parent's method table, making for slightly
 * less-wordy code.  In the future, it might make sense for this code
 * to maintain some sort of a list of resources allocated by each device.
 */
struct resource *
bus_alloc_resource(device_t dev, int type, int *rid, u_long start, u_long end,
		   u_long count, u_int flags)
{
	if (dev->parent == 0)
		return(0);
	return(BUS_ALLOC_RESOURCE(dev->parent, dev, type, rid, start, end,
				  count, flags));
}

int
bus_activate_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return(EINVAL);
	return(BUS_ACTIVATE_RESOURCE(dev->parent, dev, type, rid, r));
}

int
bus_deactivate_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return(EINVAL);
	return(BUS_DEACTIVATE_RESOURCE(dev->parent, dev, type, rid, r));
}

int
bus_release_resource(device_t dev, int type, int rid, struct resource *r)
{
	if (dev->parent == 0)
		return(EINVAL);
	return(BUS_RELEASE_RESOURCE(dev->parent, dev, type, rid, r));
}

int
bus_setup_intr(device_t dev, struct resource *r, int flags,
	       driver_intr_t handler, void *arg, void **cookiep)
{
	if (dev->parent == 0)
		return(EINVAL);
	return(BUS_SETUP_INTR(dev->parent, dev, r, flags, handler, arg,
	       cookiep));
}

int
bus_teardown_intr(device_t dev, struct resource *r, void *cookie)
{
	if (dev->parent == 0)
		return(EINVAL);
	return(BUS_TEARDOWN_INTR(dev->parent, dev, r, cookie));
}

int
bus_set_resource(device_t dev, int type, int rid,
		 u_long start, u_long count)
{
	return(BUS_SET_RESOURCE(device_get_parent(dev), dev, type, rid,
				start, count));
}

int
bus_get_resource(device_t dev, int type, int rid,
		 u_long *startp, u_long *countp)
{
	return(BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
				startp, countp));
}

u_long
bus_get_resource_start(device_t dev, int type, int rid)
{
	u_long start, count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
				 &start, &count);
	if (error)
		return(0);
	return(start);
}

u_long
bus_get_resource_count(device_t dev, int type, int rid)
{
	u_long start, count;
	int error;

	error = BUS_GET_RESOURCE(device_get_parent(dev), dev, type, rid,
				 &start, &count);
	if (error)
		return(0);
	return(count);
}

void
bus_delete_resource(device_t dev, int type, int rid)
{
	BUS_DELETE_RESOURCE(device_get_parent(dev), dev, type, rid);
}

int
bus_child_present(device_t child)
{
	return (BUS_CHILD_PRESENT(device_get_parent(child), child));
}

int
bus_child_pnpinfo_str(device_t child, char *buf, size_t buflen)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL) {
		*buf = '\0';
		return (0);
	}
	return (BUS_CHILD_PNPINFO_STR(parent, child, buf, buflen));
}

int
bus_child_location_str(device_t child, char *buf, size_t buflen)
{
	device_t parent;

	parent = device_get_parent(child);
	if (parent == NULL) {
		*buf = '\0';
		return (0);
	}
	return (BUS_CHILD_LOCATION_STR(parent, child, buf, buflen));
}

static int
root_print_child(device_t dev, device_t child)
{
	return(0);
}

static int
root_setup_intr(device_t dev, device_t child, driver_intr_t *intr, void *arg,
		void **cookiep)
{
	/*
	 * If an interrupt mapping gets to here something bad has happened.
	 */
	panic("root_setup_intr");
}

/*
 * If we get here, assume that the device is permanant and really is
 * present in the system.  Removable bus drivers are expected to intercept
 * this call long before it gets here.  We return -1 so that drivers that
 * really care can check vs -1 or some ERRNO returned higher in the food
 * chain.
 */
static int
root_child_present(device_t dev, device_t child)
{
	return(-1);
}

/*
 * XXX NOTE! other defaults may be set in bus_if.m
 */
static kobj_method_t root_methods[] = {
	/* Device interface */
	KOBJMETHOD(device_shutdown,	bus_generic_shutdown),
	KOBJMETHOD(device_suspend,	bus_generic_suspend),
	KOBJMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	KOBJMETHOD(bus_print_child,	root_print_child),
	KOBJMETHOD(bus_read_ivar,	bus_generic_read_ivar),
	KOBJMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	KOBJMETHOD(bus_setup_intr,	root_setup_intr),
	KOBJMETHOD(bus_child_present,   root_child_present),

	{ 0, 0 }
};

static driver_t root_driver = {
	"root",
	root_methods,
	1,			/* no softc */
};

device_t	root_bus;
devclass_t	root_devclass;

static int
root_bus_module_handler(module_t mod, int what, void* arg)
{
	switch (what) {
	case MOD_LOAD:
		root_bus = make_device(NULL, "root", 0);
		root_bus->desc = "System root bus";
		kobj_init((kobj_t) root_bus, (kobj_class_t) &root_driver);
		root_bus->driver = &root_driver;
		root_bus->state = DS_ATTACHED;
		root_devclass = devclass_find_internal("root", NULL, FALSE);
		return(0);

	case MOD_SHUTDOWN:
		device_shutdown(root_bus);
		return(0);
	default:
		return(0);
	}
}

static moduledata_t root_bus_mod = {
	"rootbus",
	root_bus_module_handler,
	0
};
DECLARE_MODULE(rootbus, root_bus_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);

void
root_bus_configure(void)
{
	device_t dev;

	PDEBUG(("."));

	TAILQ_FOREACH(dev, &root_bus->children, link)
		device_probe_and_attach(dev);
}

int
driver_module_handler(module_t mod, int what, void *arg)
{
	int error;
	struct driver_module_data *dmd;
	devclass_t bus_devclass;
	kobj_class_t driver;
        const char *parentname;

	dmd = (struct driver_module_data *)arg;
	bus_devclass = devclass_find_internal(dmd->dmd_busname, NULL, TRUE);
	error = 0;

	switch (what) {
	case MOD_LOAD:
		if (dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);

		driver = dmd->dmd_driver;
		PDEBUG(("Loading module: driver %s on bus %s",
		        DRIVERNAME(driver), dmd->dmd_busname));
		error = devclass_add_driver(bus_devclass, driver);
		if (error)
			break;

		/*
		 * If the driver has any base classes, make the
		 * devclass inherit from the devclass of the driver's
		 * first base class. This will allow the system to
		 * search for drivers in both devclasses for children
		 * of a device using this driver.
		 */
		if (driver->baseclasses)
			parentname = driver->baseclasses[0]->name;
		else
			parentname = NULL;
	    	*dmd->dmd_devclass = devclass_find_internal(driver->name,
							    parentname, TRUE);
		break;

	case MOD_UNLOAD:
		PDEBUG(("Unloading module: driver %s from bus %s",
			DRIVERNAME(dmd->dmd_driver), dmd->dmd_busname));
		error = devclass_delete_driver(bus_devclass, dmd->dmd_driver);

		if (!error && dmd->dmd_chainevh)
			error = dmd->dmd_chainevh(mod,what,dmd->dmd_chainarg);
		break;
	}

	return (error);
}

#ifdef BUS_DEBUG

/*
 * The _short versions avoid iteration by not calling anything that prints
 * more than oneliners. I love oneliners.
 */

static void
print_device_short(device_t dev, int indent)
{
	if (!dev)
		return;

	indentprintf(("device %d: <%s> %sparent,%schildren,%s%s%s%s,%sivars,%ssoftc,busy=%d\n",
		      dev->unit, dev->desc,
		      (dev->parent? "":"no "),
		      (TAILQ_EMPTY(&dev->children)? "no ":""),
		      (dev->flags&DF_ENABLED? "enabled,":"disabled,"),
		      (dev->flags&DF_FIXEDCLASS? "fixed,":""),
		      (dev->flags&DF_WILDCARD? "wildcard,":""),
		      (dev->flags&DF_DESCMALLOCED? "descmalloced,":""),
		      (dev->ivars? "":"no "),
		      (dev->softc? "":"no "),
		      dev->busy));
}

static void
print_device(device_t dev, int indent)
{
	if (!dev)
		return;

	print_device_short(dev, indent);

	indentprintf(("Parent:\n"));
	print_device_short(dev->parent, indent+1);
	indentprintf(("Driver:\n"));
	print_driver_short(dev->driver, indent+1);
	indentprintf(("Devclass:\n"));
	print_devclass_short(dev->devclass, indent+1);
}

/*
 * Print the device and all its children (indented).
 */
void
print_device_tree_short(device_t dev, int indent)
{
	device_t child;

	if (!dev)
		return;

	print_device_short(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link)
		print_device_tree_short(child, indent+1);
}

/*
 * Print the device and all its children (indented).
 */
void
print_device_tree(device_t dev, int indent)
{
	device_t child;

	if (!dev)
		return;

	print_device(dev, indent);

	TAILQ_FOREACH(child, &dev->children, link)
		print_device_tree(child, indent+1);
}

static void
print_driver_short(driver_t *driver, int indent)
{
	if (!driver)
		return;

	indentprintf(("driver %s: softc size = %d\n",
		      driver->name, driver->size));
}

static void
print_driver(driver_t *driver, int indent)
{
	if (!driver)
		return;

	print_driver_short(driver, indent);
}


static void
print_driver_list(driver_list_t drivers, int indent)
{
	driverlink_t driver;

	TAILQ_FOREACH(driver, &drivers, link)
		print_driver(driver->driver, indent);
}

static void
print_devclass_short(devclass_t dc, int indent)
{
	if (!dc)
		return;

	indentprintf(("devclass %s: max units = %d\n", dc->name, dc->maxunit));
}

static void
print_devclass(devclass_t dc, int indent)
{
	int i;

	if (!dc)
		return;

	print_devclass_short(dc, indent);
	indentprintf(("Drivers:\n"));
	print_driver_list(dc->drivers, indent+1);

	indentprintf(("Devices:\n"));
	for (i = 0; i < dc->maxunit; i++)
		if (dc->devices[i])
			print_device(dc->devices[i], indent+1);
}

void
print_devclass_list_short(void)
{
	devclass_t dc;

	printf("Short listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass_short(dc, 0);
	}
}

void
print_devclass_list(void)
{
	devclass_t dc;

	printf("Full listing of devclasses, drivers & devices:\n");
	TAILQ_FOREACH(dc, &devclasses, link) {
		print_devclass(dc, 0);
	}
}

#endif

/*
 * Check to see if a device is disabled via a disabled hint.
 */
int
resource_disabled(const char *name, int unit)
{
	int error, value;

	error = resource_int_value(name, unit, "disabled", &value);
	if (error)
	       return(0);
	return(value);
}
