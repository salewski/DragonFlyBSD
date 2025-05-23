/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD: src/sys/i386/isa/isa.c,v 1.132.2.5 2002/03/03 05:42:50 nyan Exp $
 * $DragonFly: src/sys/bus/isa/i386/isa.c,v 1.5 2004/04/07 05:54:35 dillon Exp $
 */

/*
 * Modifications for Intel architecture by Garrett A. Wollman.
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/resource.h>

#include "../isavar.h"
#include "../isa_common.h"

#include "opt_compat_oldisa.h"

void
isa_init(void)
{
#ifdef COMPAT_OLDISA
	isa_wrap_old_drivers();
#endif
}

/*
 * This implementation simply passes the request up to the parent
 * bus, which in our case is the special i386 nexus, substituting any
 * configured values if the caller defaulted.  We can get away with
 * this because there is no special mapping for ISA resources on an Intel
 * platform.  When porting this code to another architecture, it may be
 * necessary to interpose a mapping layer here.
 */
struct resource *
isa_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	/*
	 * Consider adding a resource definition. We allow rid 0-1 for
	 * irq and drq, 0-3 for memory and 0-7 for ports which is
	 * sufficient for isapnp.
	 */
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;
	
	if (!passthrough && !isdefault) {
		rle = resource_list_find(rl, type, *rid);
		if (!rle) {
			if (*rid < 0)
				return 0;
			switch (type) {
			case SYS_RES_IRQ:
				if (*rid >= ISA_NIRQ)
					return 0;
				break;
			case SYS_RES_DRQ:
				if (*rid >= ISA_NDRQ)
					return 0;
				break;
			case SYS_RES_MEMORY:
				if (*rid >= ISA_NMEM)
					return 0;
				break;
			case SYS_RES_IOPORT:
				if (*rid >= ISA_NPORT)
					return 0;
				break;
			default:
				return 0;
			}
			resource_list_add(rl, type, *rid, start, end, count);
		}
	}

	return resource_list_alloc(rl, bus, child, type, rid,
				   start, end, count, flags);
}

#ifdef PC98
/*
 * Indirection support.  The type of bus_space_handle_t is
 * defined in sys/i386/include/bus_pc98.h.
 */
struct resource *
isa_alloc_resourcev(device_t child, int type, int *rid,
		    bus_addr_t *res, bus_size_t count, u_int flags)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;

	device_t	bus = device_get_parent(child);
	bus_addr_t	start;
	struct resource *re;
	struct resource	**bsre;
	int		i, j, k, linear_cnt, ressz, bsrid;

	start = bus_get_resource_start(child, type, *rid);

	linear_cnt = count;
	ressz = 1;
	for (i = 1; i < count; ++i) {
		if (res[i] != res[i - 1] + 1) {
			if (i < linear_cnt)
				linear_cnt = i;
			++ressz;
		}
	}

	re = isa_alloc_resource(bus, child, type, rid,
				start + res[0], start + res[linear_cnt - 1],
				linear_cnt, flags);
	if (re == NULL)
		return NULL;

	bsre = malloc(sizeof(struct resource *) * ressz, M_DEVBUF, M_INTWAIT);
	bsre[0] = re;

	for (i = linear_cnt, k = 1; i < count; i = j, k++) {
		for (j = i + 1; j < count; j++) {
			if (res[j] != res[j - 1] + 1)
				break;
		}
		bsrid = *rid + k;
		bsre[k] = isa_alloc_resource(bus, child, type, &bsrid,
			start + res[i], start + res[j - 1], j - i, flags);
		if (bsre[k] == NULL) {
			for (k--; k >= 0; k--)
				resource_list_release(rl, bus, child, type,
						      *rid + k, bsre[k]);
			free(bsre, M_DEVBUF);
			return NULL;
		}
	}

	re->r_bushandle->bsh_res = bsre;
	re->r_bushandle->bsh_ressz = ressz;

	return re;
}

int
isa_load_resourcev(struct resource *re, bus_addr_t *res, bus_size_t count)
{
	bus_addr_t	start;
	int		i;

	if (count > re->r_bushandle->bsh_maxiatsz) {
		printf("isa_load_resourcev: map size too large\n");
		return EINVAL;
	}

	start = rman_get_start(re);
	for (i = 0; i < re->r_bushandle->bsh_maxiatsz; i++) {
		if (i < count)
			re->r_bushandle->bsh_iat[i] = start + res[i];
		else
			re->r_bushandle->bsh_iat[i] = start;
	}

	re->r_bushandle->bsh_iatsz = count;
	re->r_bushandle->bsh_bam = re->r_bustag->bs_ra;	/* relocate access */

	return 0;
}
#endif	/* PC98 */

int
isa_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *r)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
#ifdef PC98
	/*
	 * Indirection support.  The type of bus_space_handle_t is
	 * defined in sys/i386/include/bus_pc98.h.
	 */
	int	i;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		for (i = 1; i < r->r_bushandle->bsh_ressz; i++)
			resource_list_release(rl, bus, child, type, rid + i,
					      r->r_bushandle->bsh_res[i]);
		if (r->r_bushandle->bsh_res != NULL)
			free(r->r_bushandle->bsh_res, M_DEVBUF);
	}
#endif
	return resource_list_release(rl, bus, child, type, rid, r);
}

/*
 * We can't use the bus_generic_* versions of these methods because those
 * methods always pass the bus param as the requesting device, and we need
 * to pass the child (the i386 nexus knows about this and is prepared to
 * deal).
 */
int
isa_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
	       void (*ihand)(void *), void *arg, void **cookiep)
{
	return (BUS_SETUP_INTR(device_get_parent(bus), child, r, flags,
			       ihand, arg, cookiep));
}

int
isa_teardown_intr(device_t bus, device_t child, struct resource *r,
		  void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(bus), child, r, cookie));
}
