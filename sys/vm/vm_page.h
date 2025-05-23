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
 *	from: @(#)vm_page.h	8.2 (Berkeley) 12/13/93
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
 * $FreeBSD: src/sys/vm/vm_page.h,v 1.75.2.8 2002/03/06 01:07:09 dillon Exp $
 * $DragonFly: src/sys/vm/vm_page.h,v 1.18 2005/03/04 00:44:49 dillon Exp $
 */

/*
 *	Resident memory system definitions.
 */

#ifndef	_VM_PAGE_H_
#define	_VM_PAGE_H_

#if !defined(KLD_MODULE) && defined(_KERNEL)
#include "opt_vmpage.h"
#endif

#include <vm/pmap.h>
#include <machine/atomic.h>

/*
 *	Management of resident (logical) pages.
 *
 *	A small structure is kept for each resident
 *	page, indexed by page number.  Each structure
 *	is an element of several lists:
 *
 *		A hash table bucket used to quickly
 *		perform object/offset lookups
 *
 *		A list of all pages for a given object,
 *		so they can be quickly deactivated at
 *		time of deallocation.
 *
 *		An ordered list of pages due for pageout.
 *
 *	In addition, the structure contains the object
 *	and offset to which this page belongs (for pageout),
 *	and sundry status bits.
 *
 *	Fields in this structure are locked either by the lock on the
 *	object that the page belongs to (O) or by the lock on the page
 *	queues (P).
 *
 *	The 'valid' and 'dirty' fields are distinct.  A page may have dirty
 *	bits set without having associated valid bits set.  This is used by
 *	NFS to implement piecemeal writes.
 */

TAILQ_HEAD(pglist, vm_page);

struct msf_buf;
struct vm_page {
	TAILQ_ENTRY(vm_page) pageq;	/* vm_page_queues[] list (P)	*/
	struct vm_page	*hnext;		/* hash table link (O,P)	*/
	TAILQ_ENTRY(vm_page) listq;	/* pages in same object (O) 	*/

	vm_object_t object;		/* which object am I in (O,P)*/
	vm_pindex_t pindex;		/* offset into object (O,P) */
	vm_paddr_t phys_addr;		/* physical address of page */
	struct md_page md;		/* machine dependant stuff */
	u_short	queue;			/* page queue index */
	u_short	flags;			/* see below */
	u_short	pc;			/* page color */
	u_short wire_count;		/* wired down maps refs (P) */
	short hold_count;		/* page hold count */
	u_char	act_count;		/* page usage count */
	u_char	busy;			/* page busy count */

	/*
	 * NOTE that these must support one bit per DEV_BSIZE in a page!!!
	 * so, on normal X86 kernels, they must be at least 8 bits wide.
	 */
#if PAGE_SIZE == 4096
	u_char	valid;			/* map of valid DEV_BSIZE chunks */
	u_char	dirty;			/* map of dirty DEV_BSIZE chunks */
	u_char	unused1;
	u_char	unused2;
#elif PAGE_SIZE == 8192
	u_short	valid;			/* map of valid DEV_BSIZE chunks */
	u_short	dirty;			/* map of dirty DEV_BSIZE chunks */
#endif
	struct msf_buf *msf_hint; 	/* first page of an msfbuf map */
};

/*
 * note: currently use SWAPBLK_NONE as an absolute value rather then 
 * a flag bit.
 */
#define SWAPBLK_MASK	((daddr_t)((u_daddr_t)-1 >> 1))		/* mask */
#define SWAPBLK_NONE	((daddr_t)((u_daddr_t)SWAPBLK_MASK + 1))/* flag */

/*
 * Page coloring parameters.  We default to a middle of the road optimization.
 * Larger selections would not really hurt us but if a machine does not have
 * a lot of memory it could cause vm_page_alloc() to eat more cpu cycles 
 * looking for free pages.
 *
 * Page coloring cannot be disabled.  Modules do not have access to most PQ
 * constants because they can change between builds.
 */
#if defined(_KERNEL) && !defined(KLD_MODULE)

#if !defined(PQ_CACHESIZE)
#define PQ_CACHESIZE 256	/* max is 1024 (MB) */
#endif

#if PQ_CACHESIZE >= 1024
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 256	/* A number of colors opt for 1M cache */

#elif PQ_CACHESIZE >= 512
#define PQ_PRIME1 31	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 23	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 128	/* A number of colors opt for 512K cache */

#elif PQ_CACHESIZE >= 256
#define PQ_PRIME1 13	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 7	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 64	/* A number of colors opt for 256K cache */

#elif PQ_CACHESIZE >= 128
#define PQ_PRIME1 9	/* Produces a good PQ_L2_SIZE/3 + PQ_PRIME1 */
#define PQ_PRIME2 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 32	/* A number of colors opt for 128k cache */

#else
#define PQ_PRIME1 5	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_PRIME2 3	/* Prime number somewhat less than PQ_HASH_SIZE */
#define PQ_L2_SIZE 16	/* A reasonable number of colors (opt for 64K cache) */

#endif

#define PQ_L2_MASK	(PQ_L2_SIZE - 1)

#endif /* KERNEL && !KLD_MODULE */

/*
 *
 * The queue array is always based on PQ_MAXL2_SIZE regardless of the actual
 * cache size chosen in order to present a uniform interface for modules.
 */
#define PQ_MAXL2_SIZE	256	/* fixed maximum (in pages) / module compat */

#if PQ_L2_SIZE > PQ_MAXL2_SIZE
#error "Illegal PQ_L2_SIZE"
#endif

#define PQ_NONE		0
#define PQ_FREE		1
#define PQ_INACTIVE	(1 + 1*PQ_MAXL2_SIZE)
#define PQ_ACTIVE	(2 + 1*PQ_MAXL2_SIZE)
#define PQ_CACHE	(3 + 1*PQ_MAXL2_SIZE)
#define PQ_HOLD		(3 + 2*PQ_MAXL2_SIZE)
#define PQ_COUNT	(4 + 2*PQ_MAXL2_SIZE)

struct vpgqueues {
	struct pglist pl;
	int	*cnt;
	int	lcnt;
	int	flipflop;	/* probably not the best place */
};

extern struct vpgqueues vm_page_queues[PQ_COUNT];

/*
 * These are the flags defined for vm_page.
 *
 * Note: PG_UNMANAGED (used by OBJT_PHYS) indicates that the page is
 * 	 not under PV management but otherwise should be treated as a
 *	 normal page.  Pages not under PV management cannot be paged out
 *	 via the object/vm_page_t because there is no knowledge of their
 *	 pte mappings, nor can they be removed from their objects via 
 *	 the object, and such pages are also not on any PQ queue.
 */
#define	PG_BUSY		0x0001		/* page is in transit (O) */
#define	PG_WANTED	0x0002		/* someone is waiting for page (O) */
#define PG_WINATCFLS	0x0004		/* flush dirty page on inactive q */
#define	PG_FICTITIOUS	0x0008		/* physical page doesn't exist (O) */
#define	PG_WRITEABLE	0x0010		/* page is mapped writeable */
#define PG_MAPPED	0x0020		/* page is mapped */
#define	PG_ZERO		0x0040		/* page is zeroed */
#define PG_REFERENCED	0x0080		/* page has been referenced */
#define PG_CLEANCHK	0x0100		/* page will be checked for cleaning */
#define PG_SWAPINPROG	0x0200		/* swap I/O in progress on page	     */
#define PG_NOSYNC	0x0400		/* do not collect for syncer */
#define PG_UNMANAGED	0x0800		/* No PV management for page */
#define PG_MARKER	0x1000		/* special queue marker page */

/*
 * Misc constants.
 */

#define ACT_DECLINE		1
#define ACT_ADVANCE		3
#define ACT_INIT		5
#define ACT_MAX			64

#ifdef _KERNEL
/*
 * Each pageable resident page falls into one of four lists:
 *
 *	free
 *		Available for allocation now.
 *
 * The following are all LRU sorted:
 *
 *	cache
 *		Almost available for allocation. Still in an
 *		object, but clean and immediately freeable at
 *		non-interrupt times.
 *
 *	inactive
 *		Low activity, candidates for reclamation.
 *		This is the list of pages that should be
 *		paged out next.
 *
 *	active
 *		Pages that are "active" i.e. they have been
 *		recently referenced.
 *
 *	zero
 *		Pages that are really free and have been pre-zeroed
 *
 */

extern int vm_page_zero_count;
extern vm_page_t vm_page_array;		/* First resident page in table */
extern int vm_page_array_size;		/* number of vm_page_t's */
extern long first_page;			/* first physical page number */

#define VM_PAGE_TO_PHYS(entry)	\
		((entry)->phys_addr)

#define PHYS_TO_VM_PAGE(pa)	\
		(&vm_page_array[atop(pa) - first_page])

/*
 *	Functions implemented as macros
 */

static __inline void
vm_page_flag_set(vm_page_t m, unsigned int bits)
{
	atomic_set_short(&(m)->flags, bits);
}

static __inline void
vm_page_flag_clear(vm_page_t m, unsigned int bits)
{
	atomic_clear_short(&(m)->flags, bits);
}

static __inline void
vm_page_busy(vm_page_t m)
{
	KASSERT((m->flags & PG_BUSY) == 0, 
		("vm_page_busy: page already busy!!!"));
	vm_page_flag_set(m, PG_BUSY);
}

/*
 *	vm_page_flash:
 *
 *	wakeup anyone waiting for the page.
 */

static __inline void
vm_page_flash(vm_page_t m)
{
	if (m->flags & PG_WANTED) {
		vm_page_flag_clear(m, PG_WANTED);
		wakeup(m);
	}
}

/*
 * Clear the PG_BUSY flag and wakeup anyone waiting for the page.  This
 * is typically the last call you make on a page before moving onto
 * other things.
 */
static __inline void
vm_page_wakeup(vm_page_t m)
{
	KASSERT(m->flags & PG_BUSY, ("vm_page_wakeup: page not busy!!!"));
	vm_page_flag_clear(m, PG_BUSY);
	vm_page_flash(m);
}

/*
 * These routines manipulate the 'soft busy' count for a page.  A soft busy
 * is almost like PG_BUSY except that it allows certain compatible operations
 * to occur on the page while it is busy.  For example, a page undergoing a
 * write can still be mapped read-only.
 */
static __inline void
vm_page_io_start(vm_page_t m)
{
	atomic_add_char(&(m)->busy, 1);
}

static __inline void
vm_page_io_finish(vm_page_t m)
{
	atomic_subtract_char(&m->busy, 1);
	if (m->busy == 0)
		vm_page_flash(m);
}


#if PAGE_SIZE == 4096
#define VM_PAGE_BITS_ALL 0xff
#endif

#if PAGE_SIZE == 8192
#define VM_PAGE_BITS_ALL 0xffff
#endif

/*
 * Note: the code will always use nominally free pages from the free list
 * before trying other flag-specified sources. 
 *
 * At least one of VM_ALLOC_NORMAL|VM_ALLOC_SYSTEM|VM_ALLOC_INTERRUPT 
 * must be specified.  VM_ALLOC_RETRY may only be specified if VM_ALLOC_NORMAL
 * is also specified.
 */
#define VM_ALLOC_NORMAL		0x01	/* ok to use cache pages */
#define VM_ALLOC_SYSTEM		0x02	/* ok to exhaust most of free list */
#define VM_ALLOC_INTERRUPT	0x04	/* ok to exhaust entire free list */
#define	VM_ALLOC_ZERO		0x08	/* req pre-zero'd memory if avail */
#define	VM_ALLOC_RETRY		0x80	/* indefinite block (vm_page_grab()) */

void vm_page_unhold(vm_page_t mem);
void vm_page_activate (vm_page_t);
vm_page_t vm_page_alloc (vm_object_t, vm_pindex_t, int);
vm_page_t vm_page_grab (vm_object_t, vm_pindex_t, int);
void vm_page_cache (vm_page_t);
int vm_page_try_to_cache (vm_page_t);
int vm_page_try_to_free (vm_page_t);
void vm_page_dontneed (vm_page_t);
void vm_page_deactivate (vm_page_t);
void vm_page_insert (vm_page_t, vm_object_t, vm_pindex_t);
vm_page_t vm_page_lookup (vm_object_t, vm_pindex_t);
void vm_page_remove (vm_page_t);
void vm_page_rename (vm_page_t, vm_object_t, vm_pindex_t);
vm_offset_t vm_page_startup (vm_offset_t, vm_offset_t, vm_offset_t);
vm_page_t vm_add_new_page (vm_paddr_t pa);
void vm_page_unmanage (vm_page_t);
void vm_page_unwire (vm_page_t, int);
void vm_page_wire (vm_page_t);
void vm_page_unqueue (vm_page_t);
void vm_page_unqueue_nowakeup (vm_page_t);
void vm_page_set_validclean (vm_page_t, int, int);
void vm_page_set_dirty (vm_page_t, int, int);
void vm_page_clear_dirty (vm_page_t, int, int);
void vm_page_set_invalid (vm_page_t, int, int);
int vm_page_is_valid (vm_page_t, int, int);
void vm_page_test_dirty (vm_page_t);
int vm_page_bits (int, int);
vm_page_t vm_page_list_find(int basequeue, int index, boolean_t prefer_zero);
void vm_page_zero_invalid(vm_page_t m, boolean_t setvalid);
void vm_page_free_toq(vm_page_t m);
vm_offset_t vm_contig_pg_kmap(int, u_long, vm_map_t, int);
void vm_contig_pg_free(int, u_long);

/*
 * Holding a page keeps it from being reused.  Other parts of the system
 * can still disassociate the page from its current object and free it, or
 * perform read or write I/O on it and/or otherwise manipulate the page,
 * but if the page is held the VM system will leave the page and its data
 * intact and not reuse the page for other purposes until the last hold
 * reference is released.  (see vm_page_wire() if you want to prevent the
 * page from being disassociated from its object too).
 *
 * This routine must be called while at splvm() or better.
 *
 * The caller must still validate the contents of the page and, if necessary,
 * wait for any pending I/O (e.g. vm_page_sleep_busy() loop) to complete
 * before manipulating the page.
 */
static __inline void
vm_page_hold(vm_page_t mem)
{
	mem->hold_count++;
}

/*
 * Reduce the protection of a page.  This routine never raises the 
 * protection and therefore can be safely called if the page is already
 * at VM_PROT_NONE (it will be a NOP effectively ).
 *
 * VM_PROT_NONE will remove all user mappings of a page.  This is often
 * necessary when a page changes state (for example, turns into a copy-on-write
 * page or needs to be frozen for write I/O) in order to force a fault, or
 * to force a page's dirty bits to be synchronized and avoid hardware
 * (modified/accessed) bit update races with pmap changes.
 *
 * Since 'prot' is usually a constant, this inline usually winds up optimizing
 * out the primary conditional.
 */
static __inline void
vm_page_protect(vm_page_t mem, int prot)
{
	if (prot == VM_PROT_NONE) {
		if (mem->flags & (PG_WRITEABLE|PG_MAPPED)) {
			pmap_page_protect(mem, VM_PROT_NONE);
			vm_page_flag_clear(mem, PG_WRITEABLE|PG_MAPPED);
		}
	} else if ((prot == VM_PROT_READ) && (mem->flags & PG_WRITEABLE)) {
		pmap_page_protect(mem, VM_PROT_READ);
		vm_page_flag_clear(mem, PG_WRITEABLE);
	}
}

/*
 * Zero-fill the specified page.  The entire contents of the page will be
 * zero'd out.
 */
static __inline boolean_t
vm_page_zero_fill(vm_page_t m)
{
	pmap_zero_page(VM_PAGE_TO_PHYS(m));
	return (TRUE);
}

/*
 * Copy the contents of src_m to dest_m.  The pages must be stable but spl
 * and other protections depend on context.
 */
static __inline void
vm_page_copy(vm_page_t src_m, vm_page_t dest_m)
{
	pmap_copy_page(VM_PAGE_TO_PHYS(src_m), VM_PAGE_TO_PHYS(dest_m));
	dest_m->valid = VM_PAGE_BITS_ALL;
}

/*
 * Free a page.  The page must be marked BUSY.
 *
 * The clearing of PG_ZERO is a temporary safety until the code can be
 * reviewed to determine that PG_ZERO is being properly cleared on
 * write faults or maps.  PG_ZERO was previously cleared in 
 * vm_page_alloc().
 */
static __inline void
vm_page_free(vm_page_t m)
{
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_free_toq(m);
}

/*
 * Free a page to the zerod-pages queue
 */
static __inline void
vm_page_free_zero(vm_page_t m)
{
	vm_page_flag_set(m, PG_ZERO);
	vm_page_free_toq(m);
}

/*
 * Wait until page is no longer PG_BUSY or (if also_m_busy is TRUE)
 * m->busy is zero.  Returns TRUE if it had to sleep ( including if 
 * it almost had to sleep and made temporary spl*() mods), FALSE 
 * otherwise.
 *
 * This routine assumes that interrupts can only remove the busy
 * status from a page, not set the busy status or change it from
 * PG_BUSY to m->busy or vise versa (which would create a timing
 * window).
 *
 * Note: as an inline, 'also_m_busy' is usually a constant and well
 * optimized.
 */
static __inline int
vm_page_sleep_busy(vm_page_t m, int also_m_busy, const char *msg)
{
	if ((m->flags & PG_BUSY) || (also_m_busy && m->busy))  {
		int s = splvm();
		if ((m->flags & PG_BUSY) || (also_m_busy && m->busy)) {
			/*
			 * Page is busy. Wait and retry.
			 */
			vm_page_flag_set(m, PG_WANTED | PG_REFERENCED);
			tsleep(m, 0, msg, 0);
		}
		splx(s);
		return(TRUE);
		/* not reached */
	}
	return(FALSE);
}

/*
 * Make page all dirty
 */
static __inline void
vm_page_dirty(vm_page_t m)
{
	KASSERT(m->queue - m->pc != PQ_CACHE,
		("vm_page_dirty: page in cache!"));
	m->dirty = VM_PAGE_BITS_ALL;
}

/*
 * Set page to not be dirty.  Note: does not clear pmap modify bits .
 */
static __inline void
vm_page_undirty(vm_page_t m)
{
	m->dirty = 0;
}

#endif				/* _KERNEL */
#endif				/* !_VM_PAGE_ */
