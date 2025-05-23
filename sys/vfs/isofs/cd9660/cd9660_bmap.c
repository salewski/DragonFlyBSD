/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_bmap.c	8.3 (Berkeley) 1/23/94
 * $FreeBSD: src/sys/isofs/cd9660/cd9660_bmap.c,v 1.8 1999/08/28 00:46:06 peter Exp $
 * $DragonFly: src/sys/vfs/isofs/cd9660/cd9660_bmap.c,v 1.4 2004/04/12 23:18:55 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>

#include "iso.h"
#include "cd9660_node.h"

/*
 * Bmap converts a the logical block number of a file to its physical block
 * number on the disk. The conversion is done by using the logical block
 * number to index into the data block (extent) for the file.
 *
 * cd9660_bmap(struct vnode *a_vp, daddr_t a_bn, struct vnode **a_vpp,
 *		daddr_t *a_bnp, int *a_runp, int *a_runb)
 */
int
cd9660_bmap(struct vop_bmap_args *ap)
{
	struct iso_node *ip = VTOI(ap->a_vp);
	daddr_t lblkno = ap->a_bn;
	int bshift;

	/*
	 * Check for underlying vnode requests and ensure that logical
	 * to physical mapping is requested.
	 */
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ip->i_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	/*
	 * Compute the requested block number
	 */
	bshift = ip->i_mnt->im_bshift;
	*ap->a_bnp = (ip->iso_start + lblkno) << (bshift - DEV_BSHIFT);

	/*
	 * Determine maximum number of readahead blocks following the
	 * requested block.
	 */
	if (ap->a_runp) {
		int nblk;

		nblk = (ip->i_size >> bshift) - (lblkno + 1);
		if (nblk <= 0)
			*ap->a_runp = 0;
		else if (nblk >= (MAXBSIZE >> bshift))
			*ap->a_runp = (MAXBSIZE >> bshift) - 1;
		else
			*ap->a_runp = nblk;
	}

	if (ap->a_runb) {
		*ap->a_runb = 0;
	}

	return 0;
}
