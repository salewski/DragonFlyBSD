/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD: src/sys/pci/agppriv.h,v 1.3.2.1 2000/07/19 09:48:04 ru Exp $
 *	$DragonFly: src/sys/dev/agp/agppriv.h,v 1.4 2004/05/19 22:52:40 dillon Exp $
 */

#ifndef _PCI_AGPPRIV_H_
#define _PCI_AGPPRIV_H_

/*
 * This file *must not* be included by code outside the agp driver itself.
 */

#include <sys/agpio.h>
#include "agpvar.h"

#define AGP_DEBUGxx

#ifdef AGP_DEBUG
#define AGP_DPF(x...) do {			\
    printf("agp: ");				\
    printf(##x);				\
} while (0)
#else
#define AGP_DPF(x...) do {} while (0)
#endif

#include "agp_if.h"

/*
 * Data structure to describe an AGP memory allocation.
 */
TAILQ_HEAD(agp_memory_list, agp_memory);
struct agp_memory {
	TAILQ_ENTRY(agp_memory) am_link;	/* wiring for the tailq */
	int		am_id;			/* unique id for block */
	vm_size_t	am_size;		/* number of bytes allocated */
	int		am_type;		/* chipset specific type */
	struct vm_object *am_obj;		/* VM object owning pages */
	vm_offset_t	am_physical;		/* bogus hack for i810 */
	vm_offset_t	am_offset;		/* page offset if bound */
	int		am_is_bound;		/* non-zero if bound */
};

/*
 * All chipset drivers must have this at the start of their softc.
 */
struct agp_softc {
	struct resource	        *as_aperture;	/* location of aperture */
	u_int32_t		as_maxmem;	/* allocation upper bound */
	u_int32_t		as_allocated;	/* amount allocated */
	enum agp_acquire_state	as_state;
	struct agp_memory_list	as_memory;	/* list of allocated memory */
	int			as_nextid;	/* next memory block id */
	int			as_isopen;	/* user device is open */
	struct lock		as_lock;	/* lock for access to GATT */
};

struct agp_gatt {
	u_int32_t	ag_entries;
	u_int32_t      *ag_virtual;
	vm_offset_t	ag_physical;
};

void			agp_flush_cache(void);
u_int8_t		agp_find_caps(device_t dev);
struct agp_gatt	       *agp_alloc_gatt(device_t dev);
void		        agp_free_gatt(struct agp_gatt *gatt);
int			agp_generic_attach(device_t dev);
int			agp_generic_detach(device_t dev);
int			agp_generic_enable(device_t dev, u_int32_t mode);
struct agp_memory      *agp_generic_alloc_memory(device_t dev, int type,
						 vm_size_t size);
int			agp_generic_free_memory(device_t dev,
						struct agp_memory *mem);
int			agp_generic_bind_memory(device_t dev,
						struct agp_memory *mem,
						vm_offset_t offset);
int			agp_generic_unbind_memory(device_t dev,
						  struct agp_memory *mem);

#endif /* !_PCI_AGPPRIV_H_ */
