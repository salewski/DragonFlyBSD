/*
 * Copyright (c) 1995, David Greenman
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David Greenman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * The default pager is responsible for supplying backing store to unbacked
 * storage.  The backing store is usually swap so we just fall through to
 * the swap routines.  However, since swap metadata has not been assigned,
 * the swap routines assign and manage the swap backing store through the
 * vm_page->swapblk field.  The object is only converted when the page is 
 * physically freed after having been cleaned and even then vm_page->swapblk
 * is maintained whenever a resident page also has swap backing store.
 *
 * $FreeBSD: src/sys/vm/default_pager.c,v 1.23 1999/11/07 20:03:53 alc Exp $
 * $DragonFly: src/sys/vm/default_pager.c,v 1.4 2004/03/23 22:54:32 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

static vm_object_t default_pager_alloc (void *, vm_ooffset_t, vm_prot_t,
		vm_ooffset_t);
static void default_pager_dealloc (vm_object_t);
static int default_pager_getpages (vm_object_t, vm_page_t *, int, int);
static void default_pager_putpages (vm_object_t, vm_page_t *, int, 
		boolean_t, int *);
static boolean_t default_pager_haspage (vm_object_t, vm_pindex_t, int *, 
		int *);
/*
 * pagerops for OBJT_DEFAULT - "default pager".
 */
struct pagerops defaultpagerops = {
	NULL,
	default_pager_alloc,
	default_pager_dealloc,
	default_pager_getpages,
	default_pager_putpages,
	default_pager_haspage,
	NULL
};

/*
 * no_pager_alloc just returns an initialized object.
 */
static vm_object_t
default_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
		    vm_ooffset_t offset)
{
	if (handle != NULL)
		panic("default_pager_alloc: handle specified");

	return vm_object_allocate(OBJT_DEFAULT, OFF_TO_IDX(round_page(offset + size)));
}

/*
 * deallocate resources associated with default objects.   The default objects
 * have no special resources allocated to them, but the vm_page's being used
 * in this object might.  Still, we do not have to do anything - we will free
 * the swapblk in the underlying vm_page's when we free the vm_page or
 * garbage collect the vm_page cache list.
 */

static void
default_pager_dealloc(vm_object_t object)
{
	/*
	 * OBJT_DEFAULT objects have no special resources allocated to them.
	 */
}

/*
 * Load pages from backing store.  Since OBJT_DEFAULT is converted to
 * OBJT_SWAP at the time a swap-backed vm_page_t is freed, we will never
 * see a vm_page with assigned swap here.
 */

static int
default_pager_getpages(vm_object_t object, vm_page_t *m, int count, int reqpage)
{
	return VM_PAGER_FAIL;
}

/*
 * Store pages to backing store.  We should assign swap and initiate
 * I/O.  We do not actually convert the object to OBJT_SWAP here.  The
 * object will be converted when the written-out vm_page_t is moved from the
 * cache to the free list.
 */

static void
default_pager_putpages(vm_object_t object, vm_page_t *m, int c, boolean_t sync,
    int *rtvals)
{
	swap_pager_putpages(object, m, c, sync, rtvals);
}

/*
 * Tell us whether the backing store for the requested (object,index) is
 * synchronized.  i.e. tell us whether we can throw the page away and 
 * reload it later.  So, for example, if we are in the process of writing
 * the page to its backing store, or if no backing store has been assigned,
 * it is not yet synchronized.
 *
 * It is possible to have fully-synchronized swap assigned without the
 * object having been converted.  We just call swap_pager_haspage() to
 * deal with it since it must already deal with it plus deal with swap
 * meta-data structures.
 */

static boolean_t
default_pager_haspage(vm_object_t object, vm_pindex_t pindex, int *before,
    int *after)
{
	return FALSE;
}
