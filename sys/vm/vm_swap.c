/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)vm_swap.c	8.5 (Berkeley) 2/17/94
 * $FreeBSD: src/sys/vm/vm_swap.c,v 1.96.2.2 2001/10/14 18:46:47 iedowse Exp $
 * $DragonFly: src/sys/vm/vm_swap.c,v 1.17 2004/12/17 00:18:49 dillon Exp $
 */

#include "opt_swap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/nlookup.h>
#include <sys/dmap.h>		/* XXX */
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/blist.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>
#include <vm/vm_zone.h>

/*
 * Indirect driver for multi-controller paging.
 */

#ifndef NSWAPDEV
#define NSWAPDEV	4
#endif
static struct swdevt should_be_malloced[NSWAPDEV];
static struct swdevt *swdevt = should_be_malloced;
static int nswap;		/* first block after the interleaved devs */
static int nswdev = NSWAPDEV;
int vm_swap_size;

static int swapdev_strategy (struct vop_strategy_args *ap);
struct vnode *swapdev_vp;

/*
 *	swapdev_strategy:
 *
 *	VOP_STRATEGY() for swapdev_vp.
 *	Perform swap strategy interleave device selection.
 *
 *	The bp is expected to be locked and *not* B_DONE on call.
 */

static int
swapdev_strategy(struct vop_strategy_args /* {
			 struct vnode *a_vp;
			 struct buf *a_bp;
		 } */ *ap)
{
	int s, sz, off, seg, index;
	struct swdevt *sp;
	struct vnode *vp;
	struct buf *bp;

	bp = ap->a_bp;
	sz = howmany(bp->b_bcount, PAGE_SIZE);

	/*
	 * Convert interleaved swap into per-device swap.  Note that
	 * the block size is left in PAGE_SIZE'd chunks (for the newswap)
	 * here.
	 */
	if (nswdev > 1) {
		off = bp->b_blkno % dmmax;
		if (off + sz > dmmax) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			biodone(bp);
			return 0;
		}
		seg = bp->b_blkno / dmmax;
		index = seg % nswdev;
		seg /= nswdev;
		bp->b_blkno = seg * dmmax + off;
	} else {
		index = 0;
	}
	sp = &swdevt[index];
	if (bp->b_blkno + sz > sp->sw_nblks) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return 0;
	}
	bp->b_dev = sp->sw_device;
	if (sp->sw_vp == NULL) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return 0;
	}

	/*
	 * Convert from PAGE_SIZE'd to DEV_BSIZE'd chunks for the actual I/O
	 */
	bp->b_blkno = ctodb(bp->b_blkno);

	vhold(sp->sw_vp);
	s = splvm();
	if ((bp->b_flags & B_READ) == 0) {
		vp = bp->b_vp;
		if (vp) {
			vp->v_numoutput--;
			if ((vp->v_flag & VBWAIT) && vp->v_numoutput <= 0) {
				vp->v_flag &= ~VBWAIT;
				wakeup(&vp->v_numoutput);
			}
		}
		sp->sw_vp->v_numoutput++;
	}
	pbreassignbuf(bp, sp->sw_vp);
	splx(s);
	VOP_STRATEGY(bp->b_vp, bp);
	return 0;
}

/*
 * Create a special vnode op vector for swapdev_vp - we only use
 * VOP_STRATEGY(), everything else returns an error.
 */
struct vop_ops *swapdev_vnode_vops;
static struct vnodeopv_entry_desc swapdev_vnodeop_entries[] = {  
	{ &vop_default_desc,		vop_defaultop },
	{ &vop_strategy_desc,		(void *) swapdev_strategy },
	{ NULL, NULL }
};
static struct vnodeopv_desc swapdev_vnodeop_opv_desc =
	{ &swapdev_vnode_vops, swapdev_vnodeop_entries };

VNODEOP_SET(swapdev_vnodeop_opv_desc);

/*
 * swapon_args(char *name)
 *
 * System call swapon(name) enables swapping on device name,
 * which must be in the swdevsw.  Return EBUSY
 * if already swapping on this device.
 */
/* ARGSUSED */
int
swapon(struct swapon_args *uap)
{
	struct thread *td = curthread;
	struct vattr attr;
	struct vnode *vp;
	struct nlookupdata nd;
	int error;
	struct ucred *cred;

	KKASSERT(td->td_proc);
	cred = td->td_proc->p_ucred;

	error = suser(td);
	if (error)
		return (error);

	vp = NULL;
	error = nlookup_init(&nd, uap->name, UIO_USERSPACE, NLC_FOLLOW);
	if (error == 0)
		error = nlookup(&nd);
	if (error == 0)
		error = cache_vref(nd.nl_ncp, nd.nl_cred, &vp);
	nlookup_done(&nd);
	if (error)
		return (error);

	if (vn_isdisk(vp, &error))
		error = swaponvp(td, vp, 0);
	else if (vp->v_type == VREG && vp->v_tag == VT_NFS &&
	    (error = VOP_GETATTR(vp, &attr, td)) == 0) {
		/*
		 * Allow direct swapping to NFS regular files in the same
		 * way that nfs_mountroot() sets up diskless swapping.
		 */
		error = swaponvp(td, vp, attr.va_size / DEV_BSIZE);
	}

	if (error)
		vrele(vp);

	return (error);
}

/*
 * Swfree(index) frees the index'th portion of the swap map.
 * Each of the nswdev devices provides 1/nswdev'th of the swap
 * space, which is laid out with blocks of dmmax pages circularly
 * among the devices.
 *
 * The new swap code uses page-sized blocks.  The old swap code used
 * DEV_BSIZE'd chunks.
 *
 * XXX locking when multiple swapon's run in parallel
 */
int
swaponvp(struct thread *td, struct vnode *vp, u_long nblks)
{
	u_long aligned_nblks;
	struct ucred *cred;
	struct swdevt *sp;
	swblk_t vsbase;
	swblk_t dvbase;
	dev_t dev;
	int index;
	int error;
	long blk;

	KKASSERT(td->td_proc);
	cred = td->td_proc->p_ucred;

	if (!swapdev_vp) {
		error = getspecialvnode(VT_NON, NULL, &swapdev_vnode_vops,
				    &swapdev_vp, 0, 0);
		if (error)
			panic("Cannot get vnode for swapdev");
		swapdev_vp->v_type = VNON;	/* Untyped */
		vx_unlock(swapdev_vp);
	}

	ASSERT_VOP_UNLOCKED(vp, "swaponvp");
	for (sp = swdevt, index = 0 ; index < nswdev; index++, sp++) {
		if (sp->sw_vp == vp)
			return EBUSY;
		if (!sp->sw_vp)
			goto found;

	}
	return EINVAL;
    found:
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_OPEN(vp, FREAD | FWRITE, cred, NULL, td);
	VOP_UNLOCK(vp, 0, td);
	if (error)
		return (error);

	/*
	 * v_rdev is not valid until after the VOP_OPEN() call.  dev_psize()
	 * must be supported if a character device has been specified.
	 */
	if (vp->v_type == VCHR)
		dev = vp->v_rdev;
	else
		dev = NODEV;

	if (nblks == 0 && dev != NODEV && ((nblks = dev_dpsize(dev)) == -1)) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td);
		return (ENXIO);
	}
	if (nblks == 0) {
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td);
		return (ENXIO);
	}

	/*
	 * If we go beyond this, we get overflows in the radix
	 * tree bitmap code.
	 */
	if (nblks > 0x40000000 / BLIST_META_RADIX / nswdev) {
		printf("exceeded maximum of %d blocks per swap unit\n",
			0x40000000 / BLIST_META_RADIX / nswdev);
		(void) VOP_CLOSE(vp, FREAD | FWRITE, td);
		return (ENXIO);
	}
	/*
	 * nblks is in DEV_BSIZE'd chunks, convert to PAGE_SIZE'd chunks.
	 * First chop nblks off to page-align it, then convert.
	 * 
	 * sw->sw_nblks is in page-sized chunks now too.
	 */
	nblks &= ~(ctodb(1) - 1);
	nblks = dbtoc(nblks);

	sp->sw_vp = vp;
	sp->sw_dev = dev2udev(dev);
	sp->sw_device = dev;
	sp->sw_flags |= SW_FREED;
	sp->sw_nblks = nblks;

	/*
	 * nblks, nswap, and dmmax are PAGE_SIZE'd parameters now, not
	 * DEV_BSIZE'd.   aligned_nblks is used to calculate the
	 * size of the swap bitmap, taking into account the stripe size.
	 */
	aligned_nblks = (nblks + (dmmax - 1)) & ~(u_long)(dmmax - 1);

	if (aligned_nblks * nswdev > nswap)
		nswap = aligned_nblks * nswdev;

	if (swapblist == NULL)
		swapblist = blist_create(nswap);
	else
		blist_resize(&swapblist, nswap, 0);

	for (dvbase = dmmax; dvbase < nblks; dvbase += dmmax) {
		blk = min(nblks - dvbase, dmmax);
		vsbase = index * dmmax + dvbase * nswdev;
		blist_free(swapblist, vsbase, blk);
		vm_swap_size += blk;
	}

	return (0);
}
