/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * Implements bitmap resource lists.
 *
 *	Usage:
 *		blist = blist_create(blocks)
 *		(void)  blist_destroy(blist)
 *		blkno = blist_alloc(blist, count)
 *		(void)  blist_free(blist, blkno, count)
 *		(void)  blist_resize(&blist, count, freeextra)
 *		
 *
 *	Notes:
 *		on creation, the entire list is marked reserved.  You should
 *		first blist_free() the sections you want to make available
 *		for allocation before doing general blist_alloc()/free()
 *		ops.
 *
 *		SWAPBLK_NONE is returned on failure.  This module is typically
 *		capable of managing up to (2^31) blocks per blist, though
 *		the memory utilization would be insane if you actually did
 *		that.  Managing something like 512MB worth of 4K blocks 
 *		eats around 32 KBytes of memory. 
 *
 * $FreeBSD: src/sys/sys/blist.h,v 1.2.2.1 2003/01/12 09:23:12 dillon Exp $
 * $DragonFly: src/sys/sys/blist.h,v 1.4 2004/12/30 07:01:52 cpressey Exp $
 */

#ifndef _SYS_BLIST_H_
#define _SYS_BLIST_H_

#if !defined(_KERNEL) && !defined(_KERNEL_STRUCTURES)
#error "This file should not be included by userland programs."
#endif

/*
 * blmeta and bl_bitmap_t MUST be a power of 2 in size.
 */

typedef struct blmeta {
	union {
	    daddr_t	bmu_avail;	/* space available under us	*/
	    u_daddr_t	bmu_bitmap;	/* bitmap if we are a leaf	*/
	} u;
	daddr_t		bm_bighint;	/* biggest contiguous block hint*/
} blmeta_t;

typedef struct blist {
	daddr_t		bl_blocks;	/* area of coverage		*/
	daddr_t		bl_radix;	/* coverage radix		*/
	daddr_t		bl_skip;	/* starting skip		*/
	daddr_t		bl_free;	/* number of free blocks	*/
	blmeta_t	*bl_root;	/* root of radix tree		*/
	daddr_t		bl_rootblks;	/* daddr_t blks allocated for tree */
} *blist_t;

#define BLIST_META_RADIX	16
#define BLIST_BMAP_RADIX	(sizeof(u_daddr_t)*8)

#define BLIST_MAX_ALLOC		BLIST_BMAP_RADIX

#ifdef _KERNEL
extern blist_t blist_create(daddr_t blocks);
extern void blist_destroy(blist_t blist);
extern daddr_t blist_alloc(blist_t blist, daddr_t count);
extern void blist_free(blist_t blist, daddr_t blkno, daddr_t count);
extern void blist_print(blist_t blist);
extern void blist_resize(blist_t *pblist, daddr_t count, int freenew);
#endif  /* _KERNEL */

#endif	/* _SYS_BLIST_H_ */
