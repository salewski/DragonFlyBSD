/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)vnode_pager.h	8.1 (Berkeley) 6/11/93
 * $FreeBSD: src/sys/vm/vnode_pager.h,v 1.14 1999/12/29 04:55:12 peter Exp $
 * $DragonFly: src/sys/vm/vnode_pager.h,v 1.4 2004/05/08 04:11:45 dillon Exp $
 */

#ifndef	_VNODE_PAGER_
#define	_VNODE_PAGER_	1

#ifdef _KERNEL
vm_object_t vnode_pager_alloc (void *, vm_ooffset_t, vm_prot_t, vm_ooffset_t);
void vnode_pager_freepage (vm_page_t);
struct vnode *vnode_pager_lock (vm_object_t);

/*
 * XXX Generic routines; currently called by badly written FS code; these
 * XXX should go away soon.
 */
int vnode_pager_generic_getpages (struct vnode *, vm_page_t *, int, int);
int vnode_pager_generic_putpages (struct vnode *, vm_page_t *, int,
					boolean_t, int *);
#endif

#endif				/* _VNODE_PAGER_ */
