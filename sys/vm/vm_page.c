/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)vm_page.c	7.4 (Berkeley) 5/7/91
 * $FreeBSD: src/sys/vm/vm_page.c,v 1.147.2.18 2002/03/10 05:03:19 alc Exp $
 * $DragonFly: src/sys/vm/vm_page.c,v 1.28.2.1 2005/07/28 05:46:25 dillon Exp $
 */

/*
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
 */
/*
 * Resident memory management module.  The module manipulates 'VM pages'.
 * A VM page is the core building block for memory management.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>
#include <vm/vm_page2.h>

#include <sys/thread2.h>

static void vm_page_queue_init(void);
static void vm_page_free_wakeup(void);
static vm_page_t vm_page_select_cache(vm_object_t, vm_pindex_t);
static vm_page_t _vm_page_list_find2(int basequeue, int index);

static int vm_page_bucket_count;	/* How big is array? */
static int vm_page_hash_mask;		/* Mask for hash function */
static struct vm_page **vm_page_buckets; /* Array of buckets */
static volatile int vm_page_bucket_generation;
struct vpgqueues vm_page_queues[PQ_COUNT]; /* Array of tailq lists */

#define ASSERT_IN_CRIT_SECTION()	KKASSERT(crit_test(curthread));

static void
vm_page_queue_init(void) 
{
	int i;

	for (i = 0; i < PQ_L2_SIZE; i++)
		vm_page_queues[PQ_FREE+i].cnt = &vmstats.v_free_count;
	for (i = 0; i < PQ_L2_SIZE; i++)
		vm_page_queues[PQ_CACHE+i].cnt = &vmstats.v_cache_count;

	vm_page_queues[PQ_INACTIVE].cnt = &vmstats.v_inactive_count;
	vm_page_queues[PQ_ACTIVE].cnt = &vmstats.v_active_count;
	vm_page_queues[PQ_HOLD].cnt = &vmstats.v_active_count;
	/* PQ_NONE has no queue */

	for (i = 0; i < PQ_COUNT; i++)
		TAILQ_INIT(&vm_page_queues[i].pl);
}

/*
 * note: place in initialized data section?  Is this necessary?
 */
long first_page = 0;
int vm_page_array_size = 0;
int vm_page_zero_count = 0;
vm_page_t vm_page_array = 0;

/*
 * (low level boot)
 *
 * Sets the page size, perhaps based upon the memory size.
 * Must be called before any use of page-size dependent functions.
 */
void
vm_set_page_size(void)
{
	if (vmstats.v_page_size == 0)
		vmstats.v_page_size = PAGE_SIZE;
	if (((vmstats.v_page_size - 1) & vmstats.v_page_size) != 0)
		panic("vm_set_page_size: page size not a power of two");
}

/*
 * (low level boot)
 *
 * Add a new page to the freelist for use by the system.  New pages
 * are added to both the head and tail of the associated free page
 * queue in a bottom-up fashion, so both zero'd and non-zero'd page
 * requests pull 'recent' adds (higher physical addresses) first.
 *
 * Must be called in a critical section.
 */
vm_page_t
vm_add_new_page(vm_paddr_t pa)
{
	struct vpgqueues *vpq;
	vm_page_t m;

	++vmstats.v_page_count;
	++vmstats.v_free_count;
	m = PHYS_TO_VM_PAGE(pa);
	m->phys_addr = pa;
	m->flags = 0;
	m->pc = (pa >> PAGE_SHIFT) & PQ_L2_MASK;
	m->queue = m->pc + PQ_FREE;

	vpq = &vm_page_queues[m->queue];
	if (vpq->flipflop)
		TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
	else
		TAILQ_INSERT_HEAD(&vpq->pl, m, pageq);
	vpq->flipflop = 1 - vpq->flipflop;

	vm_page_queues[m->queue].lcnt++;
	return (m);
}

/*
 * (low level boot)
 *
 * Initializes the resident memory module.
 *
 * Allocates memory for the page cells, and for the object/offset-to-page
 * hash table headers.  Each page cell is initialized and placed on the
 * free list.
 */
vm_offset_t
vm_page_startup(vm_offset_t starta, vm_offset_t enda, vm_offset_t vaddr)
{
	vm_offset_t mapped;
	struct vm_page **bucket;
	vm_size_t npages;
	vm_paddr_t page_range;
	vm_paddr_t new_end;
	int i;
	vm_paddr_t pa;
	int nblocks;
	vm_paddr_t last_pa;

	/* the biggest memory array is the second group of pages */
	vm_paddr_t end;
	vm_paddr_t biggestone, biggestsize;

	vm_paddr_t total;

	total = 0;
	biggestsize = 0;
	biggestone = 0;
	nblocks = 0;
	vaddr = round_page(vaddr);

	for (i = 0; phys_avail[i + 1]; i += 2) {
		phys_avail[i] = round_page(phys_avail[i]);
		phys_avail[i + 1] = trunc_page(phys_avail[i + 1]);
	}

	for (i = 0; phys_avail[i + 1]; i += 2) {
		vm_paddr_t size = phys_avail[i + 1] - phys_avail[i];

		if (size > biggestsize) {
			biggestone = i;
			biggestsize = size;
		}
		++nblocks;
		total += size;
	}

	end = phys_avail[biggestone+1];

	/*
	 * Initialize the queue headers for the free queue, the active queue
	 * and the inactive queue.
	 */

	vm_page_queue_init();

	/*
	 * Allocate (and initialize) the hash table buckets.
	 *
	 * The number of buckets MUST BE a power of 2, and the actual value is
	 * the next power of 2 greater than the number of physical pages in
	 * the system.  
	 *
	 * We make the hash table approximately 2x the number of pages to
	 * reduce the chain length.  This is about the same size using the 
	 * singly-linked list as the 1x hash table we were using before 
	 * using TAILQ but the chain length will be smaller.
	 *
	 * Note: This computation can be tweaked if desired.
	 */
	vm_page_buckets = (struct vm_page **)vaddr;
	bucket = vm_page_buckets;
	if (vm_page_bucket_count == 0) {
		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < atop(total))
			vm_page_bucket_count <<= 1;
	}
	vm_page_bucket_count <<= 1;
	vm_page_hash_mask = vm_page_bucket_count - 1;

	/*
	 * Validate these addresses.
	 */
	new_end = end - vm_page_bucket_count * sizeof(struct vm_page *);
	new_end = trunc_page(new_end);
	mapped = round_page(vaddr);
	vaddr = pmap_map(mapped, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);
	vaddr = round_page(vaddr);
	bzero((caddr_t) mapped, vaddr - mapped);

	for (i = 0; i < vm_page_bucket_count; i++) {
		*bucket = NULL;
		bucket++;
	}

	/*
	 * Compute the number of pages of memory that will be available for
	 * use (taking into account the overhead of a page structure per
	 * page).
	 */
	first_page = phys_avail[0] / PAGE_SIZE;
	page_range = phys_avail[(nblocks - 1) * 2 + 1] / PAGE_SIZE - first_page;
	npages = (total - (page_range * sizeof(struct vm_page)) -
	    (end - new_end)) / PAGE_SIZE;

	end = new_end;

	/*
	 * Initialize the mem entry structures now, and put them in the free
	 * queue.
	 */
	vm_page_array = (vm_page_t) vaddr;
	mapped = vaddr;

	/*
	 * Validate these addresses.
	 */
	new_end = trunc_page(end - page_range * sizeof(struct vm_page));
	mapped = pmap_map(mapped, new_end, end,
	    VM_PROT_READ | VM_PROT_WRITE);

	/*
	 * Clear all of the page structures
	 */
	bzero((caddr_t) vm_page_array, page_range * sizeof(struct vm_page));
	vm_page_array_size = page_range;

	/*
	 * Construct the free queue(s) in ascending order (by physical
	 * address) so that the first 16MB of physical memory is allocated
	 * last rather than first.  On large-memory machines, this avoids
	 * the exhaustion of low physical memory before isa_dmainit has run.
	 */
	vmstats.v_page_count = 0;
	vmstats.v_free_count = 0;
	for (i = 0; phys_avail[i + 1] && npages > 0; i += 2) {
		pa = phys_avail[i];
		if (i == biggestone)
			last_pa = new_end;
		else
			last_pa = phys_avail[i + 1];
		while (pa < last_pa && npages-- > 0) {
			vm_add_new_page(pa);
			pa += PAGE_SIZE;
		}
	}
	return (mapped);
}

/*
 * Distributes the object/offset key pair among hash buckets.
 *
 * NOTE:  This macro depends on vm_page_bucket_count being a power of 2.
 * This routine may not block.
 *
 * We try to randomize the hash based on the object to spread the pages
 * out in the hash table without it costing us too much.
 */
static __inline int
vm_page_hash(vm_object_t object, vm_pindex_t pindex)
{
	int i = ((uintptr_t)object + pindex) ^ object->hash_rand;

	return(i & vm_page_hash_mask);
}

/*
 * The opposite of vm_page_hold().  A page can be freed while being held,
 * which places it on the PQ_HOLD queue.  We must call vm_page_free_toq()
 * in this case to actually free it once the hold count drops to 0.
 *
 * This routine must be called at splvm().
 */
void
vm_page_unhold(vm_page_t mem)
{
	--mem->hold_count;
	KASSERT(mem->hold_count >= 0, ("vm_page_unhold: hold count < 0!!!"));
	if (mem->hold_count == 0 && mem->queue == PQ_HOLD) {
		vm_page_busy(mem);
		vm_page_free_toq(mem);
	}
}

/*
 * Inserts the given mem entry into the object and object list.
 *
 * The pagetables are not updated but will presumably fault the page
 * in if necessary, or if a kernel page the caller will at some point
 * enter the page into the kernel's pmap.  We are not allowed to block
 * here so we *can't* do this anyway.
 *
 * This routine may not block.
 * This routine must be called with a critical section held.
 */
void
vm_page_insert(vm_page_t m, vm_object_t object, vm_pindex_t pindex)
{
	struct vm_page **bucket;

	ASSERT_IN_CRIT_SECTION();
	if (m->object != NULL)
		panic("vm_page_insert: already inserted");

	/*
	 * Record the object/offset pair in this page
	 */
	m->object = object;
	m->pindex = pindex;

	/*
	 * Insert it into the object_object/offset hash table
	 */
	bucket = &vm_page_buckets[vm_page_hash(object, pindex)];
	m->hnext = *bucket;
	*bucket = m;
	vm_page_bucket_generation++;

	/*
	 * Now link into the object's list of backed pages.
	 */
	TAILQ_INSERT_TAIL(&object->memq, m, listq);
	object->generation++;

	/*
	 * show that the object has one more resident page.
	 */
	object->resident_page_count++;

	/*
	 * Since we are inserting a new and possibly dirty page,
	 * update the object's OBJ_WRITEABLE and OBJ_MIGHTBEDIRTY flags.
	 */
	if (m->flags & PG_WRITEABLE)
		vm_object_set_writeable_dirty(object);
}

/*
 * Removes the given vm_page_t from the global (object,index) hash table
 * and from the object's memq.
 *
 * The underlying pmap entry (if any) is NOT removed here.
 * This routine may not block.
 *
 * The page must be BUSY and will remain BUSY on return.  No spl needs to be
 * held on call to this routine.
 *
 * note: FreeBSD side effect was to unbusy the page on return.  We leave
 * it busy.
 */
void
vm_page_remove(vm_page_t m)
{
	vm_object_t object;
	struct vm_page **bucket;

	crit_enter();
	if (m->object == NULL) {
		crit_exit();
		return;
	}

	if ((m->flags & PG_BUSY) == 0)
		panic("vm_page_remove: page not busy");

	object = m->object;

	/*
	 * Remove from the object_object/offset hash table.  The object
	 * must be on the hash queue, we will panic if it isn't
	 *
	 * Note: we must NULL-out m->hnext to prevent loops in detached
	 * buffers with vm_page_lookup().
	 */
	bucket = &vm_page_buckets[vm_page_hash(m->object, m->pindex)];
	while (*bucket != m) {
		if (*bucket == NULL)
		    panic("vm_page_remove(): page not found in hash");
		bucket = &(*bucket)->hnext;
	}
	*bucket = m->hnext;
	m->hnext = NULL;
	vm_page_bucket_generation++;

	/*
	 * Now remove from the object's list of backed pages.
	 */
	TAILQ_REMOVE(&object->memq, m, listq);

	/*
	 * And show that the object has one fewer resident page.
	 */
	object->resident_page_count--;
	object->generation++;

	m->object = NULL;
	crit_exit();
}

/*
 * Locate and return the page at (object, pindex), or NULL if the
 * page could not be found.
 *
 * This routine will operate properly without spl protection, but
 * the returned page could be in flux if it is busy.  Because an
 * interrupt can race a caller's busy check (unbusying and freeing the
 * page we return before the caller is able to check the busy bit),
 * the caller should generally call this routine with a critical
 * section held.
 *
 * Callers may call this routine without spl protection if they know
 * 'for sure' that the page will not be ripped out from under them
 * by an interrupt.
 */
vm_page_t
vm_page_lookup(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;
	struct vm_page **bucket;
	int generation;

	/*
	 * Search the hash table for this object/offset pair
	 */
retry:
	generation = vm_page_bucket_generation;
	bucket = &vm_page_buckets[vm_page_hash(object, pindex)];
	for (m = *bucket; m != NULL; m = m->hnext) {
		if ((m->object == object) && (m->pindex == pindex)) {
			if (vm_page_bucket_generation != generation)
				goto retry;
			return (m);
		}
	}
	if (vm_page_bucket_generation != generation)
		goto retry;
	return (NULL);
}

/*
 * vm_page_rename()
 *
 * Move the given memory entry from its current object to the specified
 * target object/offset.
 *
 * The object must be locked.
 * This routine may not block.
 *
 * Note: This routine will raise itself to splvm(), the caller need not. 
 *
 * Note: Swap associated with the page must be invalidated by the move.  We
 *       have to do this for several reasons:  (1) we aren't freeing the
 *       page, (2) we are dirtying the page, (3) the VM system is probably
 *       moving the page from object A to B, and will then later move
 *       the backing store from A to B and we can't have a conflict.
 *
 * Note: We *always* dirty the page.  It is necessary both for the
 *       fact that we moved it, and because we may be invalidating
 *	 swap.  If the page is on the cache, we have to deactivate it
 *	 or vm_page_dirty() will panic.  Dirty pages are not allowed
 *	 on the cache.
 */
void
vm_page_rename(vm_page_t m, vm_object_t new_object, vm_pindex_t new_pindex)
{
	crit_enter();
	vm_page_remove(m);
	vm_page_insert(m, new_object, new_pindex);
	if (m->queue - m->pc == PQ_CACHE)
		vm_page_deactivate(m);
	vm_page_dirty(m);
	vm_page_wakeup(m);
	crit_exit();
}

/*
 * vm_page_unqueue() without any wakeup.  This routine is used when a page
 * is being moved between queues or otherwise is to remain BUSYied by the
 * caller.
 *
 * This routine must be called at splhigh().
 * This routine may not block.
 */
void
vm_page_unqueue_nowakeup(vm_page_t m)
{
	int queue = m->queue;
	struct vpgqueues *pq;

	if (queue != PQ_NONE) {
		pq = &vm_page_queues[queue];
		m->queue = PQ_NONE;
		TAILQ_REMOVE(&pq->pl, m, pageq);
		(*pq->cnt)--;
		pq->lcnt--;
	}
}

/*
 * vm_page_unqueue() - Remove a page from its queue, wakeup the pagedemon
 * if necessary.
 *
 * This routine must be called at splhigh().
 * This routine may not block.
 */
void
vm_page_unqueue(vm_page_t m)
{
	int queue = m->queue;
	struct vpgqueues *pq;

	if (queue != PQ_NONE) {
		m->queue = PQ_NONE;
		pq = &vm_page_queues[queue];
		TAILQ_REMOVE(&pq->pl, m, pageq);
		(*pq->cnt)--;
		pq->lcnt--;
		if ((queue - m->pc) == PQ_CACHE) {
			if (vm_paging_needed())
				pagedaemon_wakeup();
		}
	}
}

/*
 * vm_page_list_find()
 *
 * Find a page on the specified queue with color optimization.
 *
 * The page coloring optimization attempts to locate a page that does
 * not overload other nearby pages in the object in the cpu's L1 or L2
 * caches.  We need this optimization because cpu caches tend to be
 * physical caches, while object spaces tend to be virtual.
 *
 * This routine must be called at splvm().
 * This routine may not block.
 *
 * Note that this routine is carefully inlined.  A non-inlined version
 * is available for outside callers but the only critical path is
 * from within this source file.
 */
static __inline
vm_page_t
_vm_page_list_find(int basequeue, int index, boolean_t prefer_zero)
{
	vm_page_t m;

	if (prefer_zero)
		m = TAILQ_LAST(&vm_page_queues[basequeue+index].pl, pglist);
	else
		m = TAILQ_FIRST(&vm_page_queues[basequeue+index].pl);
	if (m == NULL)
		m = _vm_page_list_find2(basequeue, index);
	return(m);
}

static vm_page_t
_vm_page_list_find2(int basequeue, int index)
{
	int i;
	vm_page_t m = NULL;
	struct vpgqueues *pq;

	pq = &vm_page_queues[basequeue];

	/*
	 * Note that for the first loop, index+i and index-i wind up at the
	 * same place.  Even though this is not totally optimal, we've already
	 * blown it by missing the cache case so we do not care.
	 */

	for(i = PQ_L2_SIZE / 2; i > 0; --i) {
		if ((m = TAILQ_FIRST(&pq[(index + i) & PQ_L2_MASK].pl)) != NULL)
			break;

		if ((m = TAILQ_FIRST(&pq[(index - i) & PQ_L2_MASK].pl)) != NULL)
			break;
	}
	return(m);
}

vm_page_t
vm_page_list_find(int basequeue, int index, boolean_t prefer_zero)
{
	return(_vm_page_list_find(basequeue, index, prefer_zero));
}

/*
 * Find a page on the cache queue with color optimization.  As pages
 * might be found, but not applicable, they are deactivated.  This
 * keeps us from using potentially busy cached pages.
 *
 * This routine must be called with a critical section held.
 * This routine may not block.
 */
vm_page_t
vm_page_select_cache(vm_object_t object, vm_pindex_t pindex)
{
	vm_page_t m;

	while (TRUE) {
		m = _vm_page_list_find(
		    PQ_CACHE,
		    (pindex + object->pg_color) & PQ_L2_MASK,
		    FALSE
		);
		if (m && ((m->flags & (PG_BUSY|PG_UNMANAGED)) || m->busy ||
			       m->hold_count || m->wire_count)) {
			vm_page_deactivate(m);
			continue;
		}
		return m;
	}
	/* not reached */
}

/*
 * Find a free or zero page, with specified preference.  We attempt to
 * inline the nominal case and fall back to _vm_page_select_free() 
 * otherwise.
 *
 * This routine must be called with a critical section held.
 * This routine may not block.
 */
static __inline vm_page_t
vm_page_select_free(vm_object_t object, vm_pindex_t pindex, boolean_t prefer_zero)
{
	vm_page_t m;

	m = _vm_page_list_find(
		PQ_FREE,
		(pindex + object->pg_color) & PQ_L2_MASK,
		prefer_zero
	);
	return(m);
}

/*
 * vm_page_alloc()
 *
 * Allocate and return a memory cell associated with this VM object/offset
 * pair.
 *
 *	page_req classes:
 *
 *	VM_ALLOC_NORMAL		allow use of cache pages, nominal free drain
 *	VM_ALLOC_SYSTEM		greater free drain
 *	VM_ALLOC_INTERRUPT	allow free list to be completely drained
 *	VM_ALLOC_ZERO		advisory request for pre-zero'd page
 *
 * The object must be locked.
 * This routine may not block.
 * The returned page will be marked PG_BUSY
 *
 * Additional special handling is required when called from an interrupt
 * (VM_ALLOC_INTERRUPT).  We are not allowed to mess with the page cache
 * in this case.
 */
vm_page_t
vm_page_alloc(vm_object_t object, vm_pindex_t pindex, int page_req)
{
	vm_page_t m = NULL;

	KASSERT(!vm_page_lookup(object, pindex),
		("vm_page_alloc: page already allocated"));
	KKASSERT(page_req & 
		(VM_ALLOC_NORMAL|VM_ALLOC_INTERRUPT|VM_ALLOC_SYSTEM));

	/*
	 * The pager is allowed to eat deeper into the free page list.
	 */
	if (curthread == pagethread)
		page_req |= VM_ALLOC_SYSTEM;

	crit_enter();
loop:
	if (vmstats.v_free_count > vmstats.v_free_reserved ||
	    ((page_req & VM_ALLOC_INTERRUPT) && vmstats.v_free_count > 0) ||
	    ((page_req & VM_ALLOC_SYSTEM) && vmstats.v_cache_count == 0 &&
		vmstats.v_free_count > vmstats.v_interrupt_free_min)
	) {
		/*
		 * The free queue has sufficient free pages to take one out.
		 */
		if (page_req & VM_ALLOC_ZERO)
			m = vm_page_select_free(object, pindex, TRUE);
		else
			m = vm_page_select_free(object, pindex, FALSE);
	} else if (page_req & VM_ALLOC_NORMAL) {
		/*
		 * Allocatable from the cache (non-interrupt only).  On
		 * success, we must free the page and try again, thus
		 * ensuring that vmstats.v_*_free_min counters are replenished.
		 */
#ifdef INVARIANTS
		if (curthread->td_preempted) {
			printf("vm_page_alloc(): warning, attempt to allocate"
				" cache page from preempting interrupt\n");
			m = NULL;
		} else {
			m = vm_page_select_cache(object, pindex);
		}
#else
		m = vm_page_select_cache(object, pindex);
#endif
		/*
		 * On success move the page into the free queue and loop.
		 */
		if (m != NULL) {
			KASSERT(m->dirty == 0,
			    ("Found dirty cache page %p", m));
			vm_page_busy(m);
			vm_page_protect(m, VM_PROT_NONE);
			vm_page_free(m);
			goto loop;
		}

		/*
		 * On failure return NULL
		 */
		crit_exit();
#if defined(DIAGNOSTIC)
		if (vmstats.v_cache_count > 0)
			printf("vm_page_alloc(NORMAL): missing pages on cache queue: %d\n", vmstats.v_cache_count);
#endif
		vm_pageout_deficit++;
		pagedaemon_wakeup();
		return (NULL);
	} else {
		/*
		 * No pages available, wakeup the pageout daemon and give up.
		 */
		crit_exit();
		vm_pageout_deficit++;
		pagedaemon_wakeup();
		return (NULL);
	}

	/*
	 * Good page found.  The page has not yet been busied.  We are in
	 * a critical section.
	 */
	KASSERT(m != NULL, ("vm_page_alloc(): missing page on free queue\n"));

	/*
	 * Remove from free queue
	 */
	vm_page_unqueue_nowakeup(m);

	/*
	 * Initialize structure.  Only the PG_ZERO flag is inherited.  Set
	 * the page PG_BUSY
	 */
	if (m->flags & PG_ZERO) {
		vm_page_zero_count--;
		m->flags = PG_ZERO | PG_BUSY;
	} else {
		m->flags = PG_BUSY;
	}
	m->wire_count = 0;
	m->hold_count = 0;
	m->act_count = 0;
	m->busy = 0;
	m->valid = 0;
	KASSERT(m->dirty == 0, 
		("vm_page_alloc: free/cache page %p was dirty", m));

	/*
	 * vm_page_insert() is safe prior to the crit_exit().  Note also that
	 * inserting a page here does not insert it into the pmap (which
	 * could cause us to block allocating memory).  We cannot block 
	 * anywhere.
	 */
	vm_page_insert(m, object, pindex);

	/*
	 * Don't wakeup too often - wakeup the pageout daemon when
	 * we would be nearly out of memory.
	 */
	if (vm_paging_needed())
		pagedaemon_wakeup();

	crit_exit();

	/*
	 * A PG_BUSY page is returned.
	 */
	return (m);
}

/*
 * Block until free pages are available for allocation, called in various
 * places before memory allocations.
 */
void
vm_wait(void)
{
	int s;

	s = splvm();
	if (curthread == pagethread) {
		vm_pageout_pages_needed = 1;
		tsleep(&vm_pageout_pages_needed, 0, "VMWait", 0);
	} else {
		if (!vm_pages_needed) {
			vm_pages_needed = 1;
			wakeup(&vm_pages_needed);
		}
		tsleep(&vmstats.v_free_count, 0, "vmwait", 0);
	}
	splx(s);
}

/*
 * Block until free pages are available for allocation
 *
 * Called only in vm_fault so that processes page faulting can be
 * easily tracked.
 *
 * Sleeps at a lower priority than vm_wait() so that vm_wait()ing
 * processes will be able to grab memory first.  Do not change
 * this balance without careful testing first.
 */
void
vm_waitpfault(void)
{
	int s;

	s = splvm();
	if (!vm_pages_needed) {
		vm_pages_needed = 1;
		wakeup(&vm_pages_needed);
	}
	tsleep(&vmstats.v_free_count, 0, "pfault", 0);
	splx(s);
}

/*
 * Put the specified page on the active list (if appropriate).  Ensure
 * that act_count is at least ACT_INIT but do not otherwise mess with it.
 *
 * The page queues must be locked.
 * This routine may not block.
 */
void
vm_page_activate(vm_page_t m)
{
	crit_enter();
	if (m->queue != PQ_ACTIVE) {
		if ((m->queue - m->pc) == PQ_CACHE)
			mycpu->gd_cnt.v_reactivated++;

		vm_page_unqueue(m);

		if (m->wire_count == 0 && (m->flags & PG_UNMANAGED) == 0) {
			m->queue = PQ_ACTIVE;
			vm_page_queues[PQ_ACTIVE].lcnt++;
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl,
					    m, pageq);
			if (m->act_count < ACT_INIT)
				m->act_count = ACT_INIT;
			vmstats.v_active_count++;
		}
	} else {
		if (m->act_count < ACT_INIT)
			m->act_count = ACT_INIT;
	}
	crit_exit();
}

/*
 * Helper routine for vm_page_free_toq() and vm_page_cache().  This
 * routine is called when a page has been added to the cache or free
 * queues.
 *
 * This routine may not block.
 * This routine must be called at splvm()
 */
static __inline void
vm_page_free_wakeup(void)
{
	/*
	 * if pageout daemon needs pages, then tell it that there are
	 * some free.
	 */
	if (vm_pageout_pages_needed &&
	    vmstats.v_cache_count + vmstats.v_free_count >= 
	    vmstats.v_pageout_free_min
	) {
		wakeup(&vm_pageout_pages_needed);
		vm_pageout_pages_needed = 0;
	}

	/*
	 * wakeup processes that are waiting on memory if we hit a
	 * high water mark. And wakeup scheduler process if we have
	 * lots of memory. this process will swapin processes.
	 */
	if (vm_pages_needed && !vm_page_count_min()) {
		vm_pages_needed = 0;
		wakeup(&vmstats.v_free_count);
	}
}

/*
 *	vm_page_free_toq:
 *
 *	Returns the given page to the PQ_FREE list, disassociating it with
 *	any VM object.
 *
 *	The vm_page must be PG_BUSY on entry.  PG_BUSY will be released on
 *	return (the page will have been freed).  No particular spl is required
 *	on entry.
 *
 *	This routine may not block.
 */
void
vm_page_free_toq(vm_page_t m)
{
	struct vpgqueues *pq;

	crit_enter();
	mycpu->gd_cnt.v_tfree++;

	if (m->busy || ((m->queue - m->pc) == PQ_FREE)) {
		printf(
		"vm_page_free: pindex(%lu), busy(%d), PG_BUSY(%d), hold(%d)\n",
		    (u_long)m->pindex, m->busy, (m->flags & PG_BUSY) ? 1 : 0,
		    m->hold_count);
		if ((m->queue - m->pc) == PQ_FREE)
			panic("vm_page_free: freeing free page");
		else
			panic("vm_page_free: freeing busy page");
	}

	/*
	 * unqueue, then remove page.  Note that we cannot destroy
	 * the page here because we do not want to call the pager's
	 * callback routine until after we've put the page on the
	 * appropriate free queue.
	 */
	vm_page_unqueue_nowakeup(m);
	vm_page_remove(m);

	/*
	 * No further management of fictitious pages occurs beyond object
	 * and queue removal.
	 */
	if ((m->flags & PG_FICTITIOUS) != 0) {
		vm_page_wakeup(m);
		crit_exit();
		return;
	}

	m->valid = 0;
	vm_page_undirty(m);

	if (m->wire_count != 0) {
		if (m->wire_count > 1) {
		    panic(
			"vm_page_free: invalid wire count (%d), pindex: 0x%lx",
			m->wire_count, (long)m->pindex);
		}
		panic("vm_page_free: freeing wired page");
	}

	/*
	 * Clear the UNMANAGED flag when freeing an unmanaged page.
	 */
	if (m->flags & PG_UNMANAGED) {
	    m->flags &= ~PG_UNMANAGED;
	} else {
#ifdef __alpha__
	    pmap_page_is_free(m);
#endif
	}

	if (m->hold_count != 0) {
		m->flags &= ~PG_ZERO;
		m->queue = PQ_HOLD;
	} else {
		m->queue = PQ_FREE + m->pc;
	}
	pq = &vm_page_queues[m->queue];
	pq->lcnt++;
	++(*pq->cnt);

	/*
	 * Put zero'd pages on the end ( where we look for zero'd pages
	 * first ) and non-zerod pages at the head.
	 */
	if (m->flags & PG_ZERO) {
		TAILQ_INSERT_TAIL(&pq->pl, m, pageq);
		++vm_page_zero_count;
	} else {
		TAILQ_INSERT_HEAD(&pq->pl, m, pageq);
	}
	vm_page_wakeup(m);
	vm_page_free_wakeup();
	crit_exit();
}

/*
 * vm_page_unmanage()
 *
 * Prevent PV management from being done on the page.  The page is
 * removed from the paging queues as if it were wired, and as a 
 * consequence of no longer being managed the pageout daemon will not
 * touch it (since there is no way to locate the pte mappings for the
 * page).  madvise() calls that mess with the pmap will also no longer
 * operate on the page.
 *
 * Beyond that the page is still reasonably 'normal'.  Freeing the page
 * will clear the flag.
 *
 * This routine is used by OBJT_PHYS objects - objects using unswappable
 * physical memory as backing store rather then swap-backed memory and
 * will eventually be extended to support 4MB unmanaged physical 
 * mappings.
 *
 * Must be called with a critical section held.
 */
void
vm_page_unmanage(vm_page_t m)
{
	ASSERT_IN_CRIT_SECTION();
	if ((m->flags & PG_UNMANAGED) == 0) {
		if (m->wire_count == 0)
			vm_page_unqueue(m);
	}
	vm_page_flag_set(m, PG_UNMANAGED);
}

/*
 * Mark this page as wired down by yet another map, removing it from
 * paging queues as necessary.
 *
 * The page queues must be locked.
 * This routine may not block.
 */
void
vm_page_wire(vm_page_t m)
{
	/*
	 * Only bump the wire statistics if the page is not already wired,
	 * and only unqueue the page if it is on some queue (if it is unmanaged
	 * it is already off the queues).  Don't do anything with fictitious
	 * pages because they are always wired.
	 */
	crit_enter();
	if ((m->flags & PG_FICTITIOUS) == 0) {
		if (m->wire_count == 0) {
			if ((m->flags & PG_UNMANAGED) == 0)
				vm_page_unqueue(m);
			vmstats.v_wire_count++;
		}
		m->wire_count++;
		KASSERT(m->wire_count != 0,
		    ("vm_page_wire: wire_count overflow m=%p", m));
	}
	vm_page_flag_set(m, PG_MAPPED);
	crit_exit();
}

/*
 * Release one wiring of this page, potentially enabling it to be paged again.
 *
 * Many pages placed on the inactive queue should actually go
 * into the cache, but it is difficult to figure out which.  What
 * we do instead, if the inactive target is well met, is to put
 * clean pages at the head of the inactive queue instead of the tail.
 * This will cause them to be moved to the cache more quickly and
 * if not actively re-referenced, freed more quickly.  If we just
 * stick these pages at the end of the inactive queue, heavy filesystem
 * meta-data accesses can cause an unnecessary paging load on memory bound 
 * processes.  This optimization causes one-time-use metadata to be
 * reused more quickly.
 *
 * BUT, if we are in a low-memory situation we have no choice but to
 * put clean pages on the cache queue.
 *
 * A number of routines use vm_page_unwire() to guarantee that the page
 * will go into either the inactive or active queues, and will NEVER
 * be placed in the cache - for example, just after dirtying a page.
 * dirty pages in the cache are not allowed.
 *
 * The page queues must be locked.
 * This routine may not block.
 */
void
vm_page_unwire(vm_page_t m, int activate)
{
	crit_enter();
	if (m->flags & PG_FICTITIOUS) {
		/* do nothing */
	} else if (m->wire_count <= 0) {
		panic("vm_page_unwire: invalid wire count: %d", m->wire_count);
	} else {
		if (--m->wire_count == 0) {
			--vmstats.v_wire_count;
			if (m->flags & PG_UNMANAGED) {
				;
			} else if (activate) {
				TAILQ_INSERT_TAIL(
				    &vm_page_queues[PQ_ACTIVE].pl, m, pageq);
				m->queue = PQ_ACTIVE;
				vm_page_queues[PQ_ACTIVE].lcnt++;
				vmstats.v_active_count++;
			} else {
				vm_page_flag_clear(m, PG_WINATCFLS);
				TAILQ_INSERT_TAIL(
				    &vm_page_queues[PQ_INACTIVE].pl, m, pageq);
				m->queue = PQ_INACTIVE;
				vm_page_queues[PQ_INACTIVE].lcnt++;
				vmstats.v_inactive_count++;
			}
		}
	}
	crit_exit();
}


/*
 * Move the specified page to the inactive queue.  If the page has
 * any associated swap, the swap is deallocated.
 *
 * Normally athead is 0 resulting in LRU operation.  athead is set
 * to 1 if we want this page to be 'as if it were placed in the cache',
 * except without unmapping it from the process address space.
 *
 * This routine may not block.
 */
static __inline void
_vm_page_deactivate(vm_page_t m, int athead)
{
	/*
	 * Ignore if already inactive.
	 */
	if (m->queue == PQ_INACTIVE)
		return;

	if (m->wire_count == 0 && (m->flags & PG_UNMANAGED) == 0) {
		if ((m->queue - m->pc) == PQ_CACHE)
			mycpu->gd_cnt.v_reactivated++;
		vm_page_flag_clear(m, PG_WINATCFLS);
		vm_page_unqueue(m);
		if (athead)
			TAILQ_INSERT_HEAD(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
		else
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
		m->queue = PQ_INACTIVE;
		vm_page_queues[PQ_INACTIVE].lcnt++;
		vmstats.v_inactive_count++;
	}
}

void
vm_page_deactivate(vm_page_t m)
{
    crit_enter();
    _vm_page_deactivate(m, 0);
    crit_exit();
}

/*
 * vm_page_try_to_cache:
 *
 * Returns 0 on failure, 1 on success
 */
int
vm_page_try_to_cache(vm_page_t m)
{
	crit_enter();
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->flags & (PG_BUSY|PG_UNMANAGED))) {
		crit_exit();
		return(0);
	}
	vm_page_test_dirty(m);
	if (m->dirty) {
		crit_exit();
		return(0);
	}
	vm_page_cache(m);
	crit_exit();
	return(1);
}

/*
 * Attempt to free the page.  If we cannot free it, we do nothing.
 * 1 is returned on success, 0 on failure.
 */
int
vm_page_try_to_free(vm_page_t m)
{
	crit_enter();
	if (m->dirty || m->hold_count || m->busy || m->wire_count ||
	    (m->flags & (PG_BUSY|PG_UNMANAGED))) {
		crit_exit();
		return(0);
	}
	vm_page_test_dirty(m);
	if (m->dirty) {
		crit_exit();
		return(0);
	}
	vm_page_busy(m);
	vm_page_protect(m, VM_PROT_NONE);
	vm_page_free(m);
	crit_exit();
	return(1);
}

/*
 * vm_page_cache
 *
 * Put the specified page onto the page cache queue (if appropriate).
 *
 * This routine may not block.
 */
void
vm_page_cache(vm_page_t m)
{
	ASSERT_IN_CRIT_SECTION();

	if ((m->flags & (PG_BUSY|PG_UNMANAGED)) || m->busy ||
			m->wire_count || m->hold_count) {
		printf("vm_page_cache: attempting to cache busy/held page\n");
		return;
	}
	if ((m->queue - m->pc) == PQ_CACHE)
		return;

	/*
	 * Remove all pmaps and indicate that the page is not
	 * writeable or mapped.
	 */

	vm_page_protect(m, VM_PROT_NONE);
	if (m->dirty != 0) {
		panic("vm_page_cache: caching a dirty page, pindex: %ld",
			(long)m->pindex);
	}
	vm_page_unqueue_nowakeup(m);
	m->queue = PQ_CACHE + m->pc;
	vm_page_queues[m->queue].lcnt++;
	TAILQ_INSERT_TAIL(&vm_page_queues[m->queue].pl, m, pageq);
	vmstats.v_cache_count++;
	vm_page_free_wakeup();
}

/*
 * vm_page_dontneed()
 *
 * Cache, deactivate, or do nothing as appropriate.  This routine
 * is typically used by madvise() MADV_DONTNEED.
 *
 * Generally speaking we want to move the page into the cache so
 * it gets reused quickly.  However, this can result in a silly syndrome
 * due to the page recycling too quickly.  Small objects will not be
 * fully cached.  On the otherhand, if we move the page to the inactive
 * queue we wind up with a problem whereby very large objects 
 * unnecessarily blow away our inactive and cache queues.
 *
 * The solution is to move the pages based on a fixed weighting.  We
 * either leave them alone, deactivate them, or move them to the cache,
 * where moving them to the cache has the highest weighting.
 * By forcing some pages into other queues we eventually force the
 * system to balance the queues, potentially recovering other unrelated
 * space from active.  The idea is to not force this to happen too
 * often.
 */
void
vm_page_dontneed(vm_page_t m)
{
	static int dnweight;
	int dnw;
	int head;

	dnw = ++dnweight;

	/*
	 * occassionally leave the page alone
	 */
	crit_enter();
	if ((dnw & 0x01F0) == 0 ||
	    m->queue == PQ_INACTIVE || 
	    m->queue - m->pc == PQ_CACHE
	) {
		if (m->act_count >= ACT_INIT)
			--m->act_count;
		crit_exit();
		return;
	}

	if (m->dirty == 0)
		vm_page_test_dirty(m);

	if (m->dirty || (dnw & 0x0070) == 0) {
		/*
		 * Deactivate the page 3 times out of 32.
		 */
		head = 0;
	} else {
		/*
		 * Cache the page 28 times out of every 32.  Note that
		 * the page is deactivated instead of cached, but placed
		 * at the head of the queue instead of the tail.
		 */
		head = 1;
	}
	_vm_page_deactivate(m, head);
	crit_exit();
}

/*
 * Grab a page, blocking if it is busy and allocating a page if necessary.
 * A busy page is returned or NULL.
 *
 * If VM_ALLOC_RETRY is specified VM_ALLOC_NORMAL must also be specified.
 * If VM_ALLOC_RETRY is not specified
 *
 * This routine may block, but if VM_ALLOC_RETRY is not set then NULL is
 * always returned if we had blocked.  
 * This routine will never return NULL if VM_ALLOC_RETRY is set.
 * This routine may not be called from an interrupt.
 * The returned page may not be entirely valid.
 *
 * This routine may be called from mainline code without spl protection and
 * be guarenteed a busied page associated with the object at the specified
 * index.
 */
vm_page_t
vm_page_grab(vm_object_t object, vm_pindex_t pindex, int allocflags)
{
	vm_page_t m;
	int generation;

	KKASSERT(allocflags &
		(VM_ALLOC_NORMAL|VM_ALLOC_INTERRUPT|VM_ALLOC_SYSTEM));
	crit_enter();
retrylookup:
	if ((m = vm_page_lookup(object, pindex)) != NULL) {
		if (m->busy || (m->flags & PG_BUSY)) {
			generation = object->generation;

			while ((object->generation == generation) &&
					(m->busy || (m->flags & PG_BUSY))) {
				vm_page_flag_set(m, PG_WANTED | PG_REFERENCED);
				tsleep(m, 0, "pgrbwt", 0);
				if ((allocflags & VM_ALLOC_RETRY) == 0) {
					m = NULL;
					goto done;
				}
			}
			goto retrylookup;
		} else {
			vm_page_busy(m);
			goto done;
		}
	}
	m = vm_page_alloc(object, pindex, allocflags & ~VM_ALLOC_RETRY);
	if (m == NULL) {
		vm_wait();
		if ((allocflags & VM_ALLOC_RETRY) == 0)
			goto done;
		goto retrylookup;
	}
done:
	crit_exit();
	return(m);
}

/*
 * Mapping function for valid bits or for dirty bits in
 * a page.  May not block.
 *
 * Inputs are required to range within a page.
 */
__inline int
vm_page_bits(int base, int size)
{
	int first_bit;
	int last_bit;

	KASSERT(
	    base + size <= PAGE_SIZE,
	    ("vm_page_bits: illegal base/size %d/%d", base, size)
	);

	if (size == 0)		/* handle degenerate case */
		return(0);

	first_bit = base >> DEV_BSHIFT;
	last_bit = (base + size - 1) >> DEV_BSHIFT;

	return ((2 << last_bit) - (1 << first_bit));
}

/*
 * Sets portions of a page valid and clean.  The arguments are expected
 * to be DEV_BSIZE aligned but if they aren't the bitmap is inclusive
 * of any partial chunks touched by the range.  The invalid portion of
 * such chunks will be zero'd.
 *
 * This routine may not block.
 *
 * (base + size) must be less then or equal to PAGE_SIZE.
 */
void
vm_page_set_validclean(vm_page_t m, int base, int size)
{
	int pagebits;
	int frag;
	int endoff;

	if (size == 0)	/* handle degenerate case */
		return;

	/*
	 * If the base is not DEV_BSIZE aligned and the valid
	 * bit is clear, we have to zero out a portion of the
	 * first block.
	 */

	if ((frag = base & ~(DEV_BSIZE - 1)) != base &&
	    (m->valid & (1 << (base >> DEV_BSHIFT))) == 0
	) {
		pmap_zero_page_area(
		    VM_PAGE_TO_PHYS(m),
		    frag,
		    base - frag
		);
	}

	/*
	 * If the ending offset is not DEV_BSIZE aligned and the 
	 * valid bit is clear, we have to zero out a portion of
	 * the last block.
	 */

	endoff = base + size;

	if ((frag = endoff & ~(DEV_BSIZE - 1)) != endoff &&
	    (m->valid & (1 << (endoff >> DEV_BSHIFT))) == 0
	) {
		pmap_zero_page_area(
		    VM_PAGE_TO_PHYS(m),
		    endoff,
		    DEV_BSIZE - (endoff & (DEV_BSIZE - 1))
		);
	}

	/*
	 * Set valid, clear dirty bits.  If validating the entire
	 * page we can safely clear the pmap modify bit.  We also
	 * use this opportunity to clear the PG_NOSYNC flag.  If a process
	 * takes a write fault on a MAP_NOSYNC memory area the flag will
	 * be set again.
	 *
	 * We set valid bits inclusive of any overlap, but we can only
	 * clear dirty bits for DEV_BSIZE chunks that are fully within
	 * the range.
	 */

	pagebits = vm_page_bits(base, size);
	m->valid |= pagebits;
#if 0	/* NOT YET */
	if ((frag = base & (DEV_BSIZE - 1)) != 0) {
		frag = DEV_BSIZE - frag;
		base += frag;
		size -= frag;
		if (size < 0)
		    size = 0;
	}
	pagebits = vm_page_bits(base, size & (DEV_BSIZE - 1));
#endif
	m->dirty &= ~pagebits;
	if (base == 0 && size == PAGE_SIZE) {
		pmap_clear_modify(m);
		vm_page_flag_clear(m, PG_NOSYNC);
	}
}

void
vm_page_clear_dirty(vm_page_t m, int base, int size)
{
	m->dirty &= ~vm_page_bits(base, size);
}

/*
 * Invalidates DEV_BSIZE'd chunks within a page.  Both the
 * valid and dirty bits for the effected areas are cleared.
 *
 * May not block.
 */
void
vm_page_set_invalid(vm_page_t m, int base, int size)
{
	int bits;

	bits = vm_page_bits(base, size);
	m->valid &= ~bits;
	m->dirty &= ~bits;
	m->object->generation++;
}

/*
 * The kernel assumes that the invalid portions of a page contain 
 * garbage, but such pages can be mapped into memory by user code.
 * When this occurs, we must zero out the non-valid portions of the
 * page so user code sees what it expects.
 *
 * Pages are most often semi-valid when the end of a file is mapped 
 * into memory and the file's size is not page aligned.
 */
void
vm_page_zero_invalid(vm_page_t m, boolean_t setvalid)
{
	int b;
	int i;

	/*
	 * Scan the valid bits looking for invalid sections that
	 * must be zerod.  Invalid sub-DEV_BSIZE'd areas ( where the
	 * valid bit may be set ) have already been zerod by
	 * vm_page_set_validclean().
	 */
	for (b = i = 0; i <= PAGE_SIZE / DEV_BSIZE; ++i) {
		if (i == (PAGE_SIZE / DEV_BSIZE) || 
		    (m->valid & (1 << i))
		) {
			if (i > b) {
				pmap_zero_page_area(
				    VM_PAGE_TO_PHYS(m), 
				    b << DEV_BSHIFT,
				    (i - b) << DEV_BSHIFT
				);
			}
			b = i + 1;
		}
	}

	/*
	 * setvalid is TRUE when we can safely set the zero'd areas
	 * as being valid.  We can do this if there are no cache consistency
	 * issues.  e.g. it is ok to do with UFS, but not ok to do with NFS.
	 */
	if (setvalid)
		m->valid = VM_PAGE_BITS_ALL;
}

/*
 * Is a (partial) page valid?  Note that the case where size == 0
 * will return FALSE in the degenerate case where the page is entirely
 * invalid, and TRUE otherwise.
 *
 * May not block.
 */
int
vm_page_is_valid(vm_page_t m, int base, int size)
{
	int bits = vm_page_bits(base, size);

	if (m->valid && ((m->valid & bits) == bits))
		return 1;
	else
		return 0;
}

/*
 * update dirty bits from pmap/mmu.  May not block.
 */
void
vm_page_test_dirty(vm_page_t m)
{
	if ((m->dirty != VM_PAGE_BITS_ALL) && pmap_is_modified(m)) {
		vm_page_dirty(m);
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <sys/kernel.h>

#include <ddb/ddb.h>

DB_SHOW_COMMAND(page, vm_page_print_page_info)
{
	db_printf("vmstats.v_free_count: %d\n", vmstats.v_free_count);
	db_printf("vmstats.v_cache_count: %d\n", vmstats.v_cache_count);
	db_printf("vmstats.v_inactive_count: %d\n", vmstats.v_inactive_count);
	db_printf("vmstats.v_active_count: %d\n", vmstats.v_active_count);
	db_printf("vmstats.v_wire_count: %d\n", vmstats.v_wire_count);
	db_printf("vmstats.v_free_reserved: %d\n", vmstats.v_free_reserved);
	db_printf("vmstats.v_free_min: %d\n", vmstats.v_free_min);
	db_printf("vmstats.v_free_target: %d\n", vmstats.v_free_target);
	db_printf("vmstats.v_cache_min: %d\n", vmstats.v_cache_min);
	db_printf("vmstats.v_inactive_target: %d\n", vmstats.v_inactive_target);
}

DB_SHOW_COMMAND(pageq, vm_page_print_pageq_info)
{
	int i;
	db_printf("PQ_FREE:");
	for(i=0;i<PQ_L2_SIZE;i++) {
		db_printf(" %d", vm_page_queues[PQ_FREE + i].lcnt);
	}
	db_printf("\n");
		
	db_printf("PQ_CACHE:");
	for(i=0;i<PQ_L2_SIZE;i++) {
		db_printf(" %d", vm_page_queues[PQ_CACHE + i].lcnt);
	}
	db_printf("\n");

	db_printf("PQ_ACTIVE: %d, PQ_INACTIVE: %d\n",
		vm_page_queues[PQ_ACTIVE].lcnt,
		vm_page_queues[PQ_INACTIVE].lcnt);
}
#endif /* DDB */
