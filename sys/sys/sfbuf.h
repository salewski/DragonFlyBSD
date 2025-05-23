/*
 * Copyright (c) 2003 Alan L. Cox <alc@cs.rice.edu>.  All rights reserved.
 * Copyright (c) 1998 David Greenman.  All rights reserved.
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
 * $DragonFly: src/sys/sys/sfbuf.h,v 1.8 2005/03/02 18:42:09 hmp Exp $
 */

#ifndef _SFBUF_H_
#define _SFBUF_H_

#if !defined(_KERNEL) && !defined(_KERNEL_STRUCTURES)
#error "This file should not be included by userland programs."
#endif

struct sf_buf {
	LIST_ENTRY(sf_buf) list_entry;	/* hash chain of active buffers */
	TAILQ_ENTRY(sf_buf) free_entry;	/* list of free buffers */
	struct		vm_page *m;	/* currently mapped page */
	vm_offset_t	kva;		/* va of mapping */
	int		refcnt;		/* usage of this mapping */
	int		flags;		/* global SFBA flags */
	cpumask_t	cpumask;	/* cpu mapping synchronization */
};

/*
 * sf_buf_alloc() flags (not all are stored in sf->flags)
 */
#define SFB_CPUPRIVATE	0x0001		/* sync mapping to current cpu only */
#define SFBA_ONFREEQ	0x0002		/* on the free queue (lazy move) */
#define SFB_CATCH	0x0004		/* allow interruption */

static __inline vm_offset_t
sf_buf_kva(struct sf_buf *sf)
{
	return(sf->kva);
}

static __inline struct vm_page *
sf_buf_page(struct sf_buf *sf)
{
	 return(sf->m);
}


#if defined(_KERNEL)

extern int nsfbufs;

struct sf_buf  *sf_buf_alloc(struct vm_page *, int flags);
void		sf_buf_free(struct sf_buf *);
void		sf_buf_ref(struct sf_buf *);

#endif

#endif
