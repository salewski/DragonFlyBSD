/*-
 * Copyright (c) 1999 Doug Rabson
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
 * $FreeBSD: src/sys/isa/isahint.c,v 1.8.2.1 2001/03/21 11:18:25 nyan Exp $
 * $DragonFly: src/sys/bus/isa/isahint.c,v 1.3 2003/08/07 21:16:46 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include "isavar.h"
#include <machine/resource.h>

static void
isahint_add_device(device_t parent, const char *name, int unit)
{
	device_t	child;
	int		sensitive, start, count, t;
	int		order;

	/* device-specific flag overrides any wildcard */
	sensitive = 0;
	if (resource_int_value(name, unit, "sensitive", &sensitive) != 0)
		resource_int_value(name, -1, "sensitive", &sensitive);

	if (sensitive)
		order = ISA_ORDER_SENSITIVE;
	else
		order = ISA_ORDER_SPECULATIVE;

	child = BUS_ADD_CHILD(parent, order, name, unit);
	if (child == 0)
		return;

	start = 0;
	count = 0;
	resource_int_value(name, unit, "port", &start);
	resource_int_value(name, unit, "portsize", &count);
	if (start > 0 || count > 0)
		bus_set_resource(child, SYS_RES_IOPORT, 0, start, count);

	start = 0;
	count = 0;
	resource_int_value(name, unit, "maddr", &start);
	resource_int_value(name, unit, "msize", &count);
	if (start > 0 || count > 0)
		bus_set_resource(child, SYS_RES_MEMORY, 0, start, count);

	if (resource_int_value(name, unit, "irq", &start) == 0 && start > 0)
		bus_set_resource(child, SYS_RES_IRQ, 0, start, 1);

	if (resource_int_value(name, unit, "drq", &start) == 0 && start >= 0)
		bus_set_resource(child, SYS_RES_DRQ, 0, start, 1);

	if (resource_int_value(name, unit, "flags", &t) == 0)
		device_set_flags(child, t);

	if (resource_int_value(name, unit, "disabled", &t) == 0 && t != 0)
		device_disable(child);
}

static void
isahint_identify(driver_t *driver, device_t parent)
{
	int i;
	static char buf[] = "isaXXX";

	/*
	 * Add all devices configured to be attached to parent.
	 */
	sprintf(buf, "isa%d", device_get_unit(parent));
	for (i = resource_query_string(-1, "at", buf);
	     i != -1;
	     i = resource_query_string(i, "at", buf)) {
		if (strcmp(resource_query_name(i), "atkbd") == 0)
			continue;	/* old GENERIC kludge */
		isahint_add_device(parent,
				   resource_query_name(i),
				   resource_query_unit(i));
	}

	/*
	 * and isa?
	 */
	for (i = resource_query_string(-1, "at", "isa");
	     i != -1;
	     i = resource_query_string(i, "at", "isa")) {
		if (strcmp(resource_query_name(i), "atkbd") == 0)
			continue;	/* old GENERIC kludge */
		isahint_add_device(parent,
				   resource_query_name(i),
				   resource_query_unit(i));
	}
}

static device_method_t isahint_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	isahint_identify),

	{ 0, 0 }
};

static driver_t isahint_driver = {
	"hint",
	isahint_methods,
	1,			/* no softc */
};

static devclass_t hint_devclass;

DRIVER_MODULE(isahint, isa, isahint_driver, hint_devclass, 0, 0);
