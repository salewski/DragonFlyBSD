/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)vm_kern.c	8.3 (Berkeley) 1/12/94
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/vm/vm_kern.c,v 1.61.2.2 2002/03/12 18:25:26 tegge Exp $
 * $DragonFly: src/sys/vm/vm_kern.c,v 1.21 2005/04/02 15:58:16 joerg Exp $
 */

/*
 *	Kernel memory management.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

vm_map_t kernel_map=0;
vm_map_t exec_map=0;
vm_map_t clean_map=0;
vm_map_t buffer_map=0;

/*
 *	kmem_alloc_pageable:
 *
 *	Allocate pageable memory to the kernel's address map.
 *	"map" must be kernel_map or a submap of kernel_map.
 */
vm_offset_t
kmem_alloc_pageable(vm_map_t map, vm_size_t size)
{
	vm_offset_t addr;
	int result;

	size = round_page(size);
	addr = vm_map_min(map);
	result = vm_map_find(map, NULL, (vm_offset_t) 0,
	    &addr, size, TRUE, VM_PROT_ALL, VM_PROT_ALL, 0);
	if (result != KERN_SUCCESS) {
		return (0);
	}
	return (addr);
}

/*
 *	kmem_alloc_nofault:
 *
 *	Same as kmem_alloc_pageable, except that it create a nofault entry.
 */
vm_offset_t
kmem_alloc_nofault(vm_map_t map, vm_size_t size)
{
	vm_offset_t addr;
	int result;

	size = round_page(size);
	addr = vm_map_min(map);
	result = vm_map_find(map, NULL, (vm_offset_t) 0,
	    &addr, size, TRUE, VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);
	if (result != KERN_SUCCESS) {
		return (0);
	}
	return (addr);
}

/*
 *	Allocate wired-down memory in the kernel's address map
 *	or a submap.
 */
vm_offset_t
kmem_alloc3(vm_map_t map, vm_size_t size, int kmflags)
{
	vm_offset_t addr;
	vm_offset_t offset;
	vm_offset_t i;
	int count;

	size = round_page(size);

	if (kmflags & KM_KRESERVE)
		count = vm_map_entry_kreserve(MAP_RESERVE_COUNT);
	else
		count = vm_map_entry_reserve(MAP_RESERVE_COUNT);

	/*
	 * Use the kernel object for wired-down kernel pages. Assume that no
	 * region of the kernel object is referenced more than once.
	 *
	 * Locate sufficient space in the map.  This will give us the final
	 * virtual address for the new memory, and thus will tell us the
	 * offset within the kernel map.
	 */
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), size, 1, &addr)) {
		vm_map_unlock(map);
		if (kmflags & KM_KRESERVE)
			vm_map_entry_krelease(count);
		else
			vm_map_entry_release(count);
		return (0);
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kernel_object);
	vm_map_insert(map, &count,
		kernel_object, offset, addr, addr + size,
		VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);
	if (kmflags & KM_KRESERVE)
		vm_map_entry_krelease(count);
	else
		vm_map_entry_release(count);

	/*
	 * Guarantee that there are pages already in this object before
	 * calling vm_map_wire.  This is to prevent the following
	 * scenario:
	 *
	 * 1) Threads have swapped out, so that there is a pager for the
	 * kernel_object. 2) The kmsg zone is empty, and so we are
	 * kmem_allocing a new page for it. 3) vm_map_wire calls vm_fault;
	 * there is no page, but there is a pager, so we call
	 * pager_data_request.  But the kmsg zone is empty, so we must
	 * kmem_alloc. 4) goto 1 5) Even if the kmsg zone is not empty: when
	 * we get the data back from the pager, it will be (very stale)
	 * non-zero data.  kmem_alloc is defined to return zero-filled memory.
	 *
	 * We're intentionally not activating the pages we allocate to prevent a
	 * race with page-out.  vm_map_wire will wire the pages.
	 */

	for (i = 0; i < size; i += PAGE_SIZE) {
		vm_page_t mem;

		mem = vm_page_grab(kernel_object, OFF_TO_IDX(offset + i),
			    VM_ALLOC_ZERO | VM_ALLOC_NORMAL | VM_ALLOC_RETRY);
		if ((mem->flags & PG_ZERO) == 0)
			vm_page_zero_fill(mem);
		mem->valid = VM_PAGE_BITS_ALL;
		vm_page_flag_clear(mem, PG_ZERO);
		vm_page_wakeup(mem);
	}

	/*
	 * And finally, mark the data as non-pageable.
	 */

	(void) vm_map_wire(map, (vm_offset_t) addr, addr + size, kmflags);

	return (addr);
}

/*
 *	kmem_free:
 *
 *	Release a region of kernel virtual memory allocated
 *	with kmem_alloc, and return the physical pages
 *	associated with that region.
 *
 *	This routine may not block on kernel maps.
 */
void
kmem_free(vm_map_t map, vm_offset_t addr, vm_size_t size)
{
	(void) vm_map_remove(map, trunc_page(addr), round_page(addr + size));
}

/*
 *	kmem_suballoc:
 *
 *	Allocates a map to manage a subrange
 *	of the kernel virtual address space.
 *
 *	Arguments are as follows:
 *
 *	parent		Map to take range from
 *	size		Size of range to find
 *	min, max	Returned endpoints of map
 *	pageable	Can the region be paged
 */
vm_map_t
kmem_suballoc(vm_map_t parent, vm_offset_t *min, vm_offset_t *max,
    vm_size_t size)
{
	int ret;
	vm_map_t result;

	size = round_page(size);

	*min = (vm_offset_t) vm_map_min(parent);
	ret = vm_map_find(parent, NULL, (vm_offset_t) 0,
	    min, size, TRUE, VM_PROT_ALL, VM_PROT_ALL, 0);
	if (ret != KERN_SUCCESS) {
		printf("kmem_suballoc: bad status return of %d.\n", ret);
		panic("kmem_suballoc");
	}
	*max = *min + size;
	pmap_reference(vm_map_pmap(parent));
	result = vm_map_create(vm_map_pmap(parent), *min, *max);
	if (result == NULL)
		panic("kmem_suballoc: cannot create submap");
	if ((ret = vm_map_submap(parent, *min, *max, result)) != KERN_SUCCESS)
		panic("kmem_suballoc: unable to change range to submap");
	return (result);
}

/*
 *	kmem_malloc:
 *
 * 	Allocate wired-down memory in the kernel's address map for the higher
 * 	level kernel memory allocator (kern/kern_malloc.c).  We cannot use
 * 	kmem_alloc() because we may need to allocate memory at interrupt
 * 	level where we cannot block (canwait == FALSE).
 *
 * 	We don't worry about expanding the map (adding entries) since entries
 * 	for wired maps are statically allocated.
 *
 *	NOTE:  Please see kmem_slab_alloc() for a better explanation of the
 *	M_* flags.
 */
vm_offset_t
kmem_malloc(vm_map_t map, vm_size_t size, int flags)
{
	vm_offset_t offset, i;
	vm_map_entry_t entry;
	vm_offset_t addr;
	vm_page_t m;
	int count, vmflags, wanted_reserve;
	thread_t td;

	if (map != kernel_map)
		panic("kmem_malloc: map != kernel_map");

	size = round_page(size);
	addr = vm_map_min(map);

	/*
	 * Locate sufficient space in the map.  This will give us the final
	 * virtual address for the new memory, and thus will tell us the
	 * offset within the kernel map.  If we are unable to allocate space
	 * and neither RNOWAIT or NULLOK is set, we panic.
	 */
	count = vm_map_entry_reserve(MAP_RESERVE_COUNT);
	vm_map_lock(map);
	if (vm_map_findspace(map, vm_map_min(map), size, 1, &addr)) {
		vm_map_unlock(map);
		vm_map_entry_release(count);
		if ((flags & M_NULLOK) == 0) {
			panic("kmem_malloc(%ld): kernel_map too small: "
				"%ld total allocated",
				(long)size, (long)map->size);
		}
		return (0);
	}
	offset = addr - VM_MIN_KERNEL_ADDRESS;
	vm_object_reference(kmem_object);
	vm_map_insert(map, &count,
		kmem_object, offset, addr, addr + size,
		VM_PROT_ALL, VM_PROT_ALL, 0);

	td = curthread;
	wanted_reserve = 0;

	vmflags = VM_ALLOC_SYSTEM;	/* XXX M_USE_RESERVE? */
	if ((flags & (M_WAITOK|M_RNOWAIT)) == 0)
		panic("kmem_malloc: bad flags %08x (%p)\n", flags, ((int **)&map)[-1]);
	if (flags & M_USE_INTERRUPT_RESERVE)
		vmflags |= VM_ALLOC_INTERRUPT;

	for (i = 0; i < size; i += PAGE_SIZE) {
		/*
		 * Only allocate PQ_CACHE pages for M_WAITOK requests and 
		 * then only if we are not preempting.
		 */
		if (flags & M_WAITOK) {
			if (td->td_preempted) {
				vmflags &= ~VM_ALLOC_NORMAL;
				wanted_reserve = 1;
			} else {
				vmflags |= VM_ALLOC_NORMAL;
				wanted_reserve = 0;
			}
		}

		m = vm_page_alloc(kmem_object, OFF_TO_IDX(offset + i), vmflags);

		/*
		 * Ran out of space, free everything up and return.  Don't need
		 * to lock page queues here as we know that the pages we got
		 * aren't on any queues.
		 *
		 * If M_WAITOK is set we can yield or block.
		 */
		if (m == NULL) {
			if (flags & M_WAITOK) {
				if (wanted_reserve) {
					vm_map_unlock(map);
					lwkt_yield();
					vm_map_lock(map);
				} else {
					vm_map_unlock(map);
					vm_wait();
					vm_map_lock(map);
				}
				i -= PAGE_SIZE;	/* retry */
				continue;
			}
			/* 
			 * Free the pages before removing the map entry.
			 * They are already marked busy.  Calling
			 * vm_map_delete before the pages has been freed or
			 * unbusied will cause a deadlock.
			 */
			while (i != 0) {
				i -= PAGE_SIZE;
				m = vm_page_lookup(kmem_object,
						   OFF_TO_IDX(offset + i));
				vm_page_free(m);
			}
			vm_map_delete(map, addr, addr + size, &count);
			vm_map_unlock(map);
			vm_map_entry_release(count);
			return (0);
		}
		vm_page_flag_clear(m, PG_ZERO);
		m->valid = VM_PAGE_BITS_ALL;
	}

	/*
	 * Mark map entry as non-pageable. Assert: vm_map_insert() will never
	 * be able to extend the previous entry so there will be a new entry
	 * exactly corresponding to this address range and it will have
	 * wired_count == 0.
	 */
	if (!vm_map_lookup_entry(map, addr, &entry) ||
	    entry->start != addr || entry->end != addr + size ||
	    entry->wired_count != 0)
		panic("kmem_malloc: entry not found or misaligned");
	entry->wired_count = 1;

	vm_map_simplify_entry(map, entry, &count);

	/*
	 * Loop thru pages, entering them in the pmap. (We cannot add them to
	 * the wired count without wrapping the vm_page_queue_lock in
	 * splimp...)
	 */
	for (i = 0; i < size; i += PAGE_SIZE) {
		m = vm_page_lookup(kmem_object, OFF_TO_IDX(offset + i));
		vm_page_wire(m);
		vm_page_wakeup(m);
		/*
		 * Because this is kernel_pmap, this call will not block.
		 */
		pmap_enter(kernel_pmap, addr + i, m, VM_PROT_ALL, 1);
		vm_page_flag_set(m, PG_MAPPED | PG_WRITEABLE | PG_REFERENCED);
	}
	vm_map_unlock(map);
	vm_map_entry_release(count);

	return (addr);
}

/*
 *	kmem_alloc_wait:
 *
 *	Allocates pageable memory from a sub-map of the kernel.  If the submap
 *	has no room, the caller sleeps waiting for more memory in the submap.
 *
 *	This routine may block.
 */

vm_offset_t
kmem_alloc_wait(vm_map_t map, vm_size_t size)
{
	vm_offset_t addr;
	int count;

	size = round_page(size);

	count = vm_map_entry_reserve(MAP_RESERVE_COUNT);

	for (;;) {
		/*
		 * To make this work for more than one map, use the map's lock
		 * to lock out sleepers/wakers.
		 */
		vm_map_lock(map);
		if (vm_map_findspace(map, vm_map_min(map), size, 1, &addr) == 0)
			break;
		/* no space now; see if we can ever get space */
		if (vm_map_max(map) - vm_map_min(map) < size) {
			vm_map_entry_release(count);
			vm_map_unlock(map);
			return (0);
		}
		vm_map_unlock(map);
		tsleep(map, 0, "kmaw", 0);
	}
	vm_map_insert(map, &count,
		    NULL, (vm_offset_t) 0,
		    addr, addr + size, VM_PROT_ALL, VM_PROT_ALL, 0);
	vm_map_unlock(map);
	vm_map_entry_release(count);
	return (addr);
}

/*
 *	kmem_free_wakeup:
 *
 *	Returns memory to a submap of the kernel, and wakes up any processes
 *	waiting for memory in that map.
 */
void
kmem_free_wakeup(vm_map_t map, vm_offset_t addr, vm_size_t size)
{
	int count;

	count = vm_map_entry_reserve(MAP_RESERVE_COUNT);
	vm_map_lock(map);
	(void) vm_map_delete(map, trunc_page(addr), round_page(addr + size), &count);
	wakeup(map);
	vm_map_unlock(map);
	vm_map_entry_release(count);
}

/*
 * 	kmem_init:
 *
 *	Create the kernel map; insert a mapping covering kernel text, 
 *	data, bss, and all space allocated thus far (`boostrap' data).  The 
 *	new map will thus map the range between VM_MIN_KERNEL_ADDRESS and 
 *	`start' as allocated, and the range between `start' and `end' as free.
 *
 *	Depend on the zalloc bootstrap cache to get our vm_map_entry_t.
 */
void
kmem_init(vm_offset_t start, vm_offset_t end)
{
	vm_map_t m;
	int count;

	m = vm_map_create(kernel_pmap, VM_MIN_KERNEL_ADDRESS, end);
	vm_map_lock(m);
	/* N.B.: cannot use kgdb to debug, starting with this assignment ... */
	kernel_map = m;
	kernel_map->system_map = 1;
	count = vm_map_entry_reserve(MAP_RESERVE_COUNT);
	(void) vm_map_insert(m, &count, NULL, (vm_offset_t) 0,
	    VM_MIN_KERNEL_ADDRESS, start, VM_PROT_ALL, VM_PROT_ALL, 0);
	/* ... and ending with the completion of the above `insert' */
	vm_map_unlock(m);
	vm_map_entry_release(count);
}
