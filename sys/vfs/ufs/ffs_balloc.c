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
 *	@(#)ffs_balloc.c	8.8 (Berkeley) 6/16/95
 * $FreeBSD: src/sys/ufs/ffs/ffs_balloc.c,v 1.26.2.1 2002/10/10 19:48:20 dillon Exp $
 * $DragonFly: src/sys/vfs/ufs/ffs_balloc.c,v 1.11.2.1 2005/09/15 18:56:34 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include "quota.h"
#include "inode.h"
#include "ufs_extern.h"

#include "fs.h"
#include "ffs_extern.h"

/*
 * Balloc defines the structure of filesystem storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 *
 * ffs_balloc(struct vnode *a_vp, ufs_daddr_t a_lbn, int a_size,
 *	      struct ucred *a_cred, int a_flags, struct buf *a_bpp)
 */
int
ffs_balloc(struct vop_balloc_args *ap)
{
	struct inode *ip;
	ufs_daddr_t lbn;
	int size;
	struct ucred *cred;
	int flags;
	struct fs *fs;
	ufs_daddr_t nb;
	struct buf *bp, *nbp, *dbp;
	struct vnode *vp;
	struct indir indirs[NIADDR + 2];
	ufs_daddr_t newb, *bap, pref;
	int deallocated, osize, nsize, num, i, error;
	ufs_daddr_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];
	struct thread *td = curthread;	/* XXX */
	int unwindidx;
	int seqcount;

	vp = ap->a_vp;
	ip = VTOI(vp);
	fs = ip->i_fs;
	lbn = lblkno(fs, ap->a_startoffset);
	size = blkoff(fs, ap->a_startoffset) + ap->a_size;
	if (size > fs->fs_bsize)
		panic("ffs_balloc: blk too big");
	*ap->a_bpp = NULL;
	if (lbn < 0)
		return (EFBIG);
	cred = ap->a_cred;
	flags = ap->a_flags;

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
	nb = lblkno(fs, ip->i_size);
	if (nb < NDADDR && nb < lbn) {
		/*
		 * The filesize prior to this write can fit in direct
		 * blocks (ex. fragmentation is possibly done)
		 * we are now extending the file write beyond
		 * the block which has end of the file prior to this write.
		 */
		osize = blksize(fs, ip, nb);
		/*
		 * osize gives disk allocated size in the last block. It is
		 * either in fragments or a file system block size.
		 */
		if (osize < fs->fs_bsize && osize > 0) {
			/* A few fragments are already allocated, since the
			 * current extends beyond this block allocated the
			 * complete block as fragments are on in last block.
			 */
			error = ffs_realloccg(ip, nb,
				ffs_blkpref(ip, nb, (int)nb, &ip->i_db[0]),
				osize, (int)fs->fs_bsize, cred, &bp);
			if (error)
				return (error);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, nb,
				    dbtofsb(fs, bp->b_blkno), ip->i_db[nb],
				    fs->fs_bsize, osize, bp);
			/* adjust the inode size, we just grew */
			ip->i_size = smalllblktosize(fs, nb + 1);
			ip->i_db[nb] = dbtofsb(fs, bp->b_blkno);
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
			if (flags & B_SYNC)
				bwrite(bp);
			else
				bawrite(bp);
			/* bp is already released here */
		}
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {
		nb = ip->i_db[lbn];
		if (nb != 0 && ip->i_size >= smalllblktosize(fs, lbn + 1)) {
			error = bread(vp, lbn, fs->fs_bsize, &bp);
			if (error) {
				brelse(bp);
				return (error);
			}
			bp->b_blkno = fsbtodb(fs, nb);
			*ap->a_bpp = bp;
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
				error = bread(vp, lbn, osize, &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
				bp->b_blkno = fsbtodb(fs, nb);
			} else {
				error = ffs_realloccg(ip, lbn,
				    ffs_blkpref(ip, lbn, (int)lbn,
					&ip->i_db[0]), osize, nsize, cred, &bp);
				if (error)
					return (error);
				if (DOINGSOFTDEP(vp))
					softdep_setup_allocdirect(ip, lbn,
					    dbtofsb(fs, bp->b_blkno), nb,
					    nsize, osize, bp);
			}
		} else {
			if (ip->i_size < smalllblktosize(fs, lbn + 1))
				nsize = fragroundup(fs, size);
			else
				nsize = fs->fs_bsize;
			error = ffs_alloc(ip, lbn,
			    ffs_blkpref(ip, lbn, (int)lbn, &ip->i_db[0]),
			    nsize, cred, &newb);
			if (error)
				return (error);
			bp = getblk(vp, lbn, nsize, 0, 0);
			bp->b_blkno = fsbtodb(fs, newb);
			if (flags & B_CLRBUF)
				vfs_bio_clrbuf(bp);
			if (DOINGSOFTDEP(vp))
				softdep_setup_allocdirect(ip, lbn, newb, 0,
				    nsize, 0, bp);
		}
		ip->i_db[lbn] = dbtofsb(fs, bp->b_blkno);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		*ap->a_bpp = bp;
		return (0);
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ufs_getlbns(vp, lbn, indirs, &num)) != 0)
		return(error);
#ifdef DIAGNOSTIC
	if (num < 1)
		panic ("ffs_balloc: ufs_bmaparray returned indirect block");
#endif
	/*
	 * Get a handle on the data block buffer before working through 
	 * indirect blocks to avoid a deadlock between the VM system holding
	 * a locked VM page and issuing a BMAP (which tries to lock the
	 * indirect blocks), and the filesystem holding a locked indirect
	 * block and then trying to read a data block (which tries to lock
	 * the underlying VM pages).
	 */
	dbp = getblk(vp, lbn, fs->fs_bsize, 0, 0);

	/*
	 * Setup undo history
	 */
	allocib = NULL;
	allocblk = allociblk;
	unwindidx = -1;

	/*
	 * Fetch the first indirect block directly from the inode, allocating
	 * one if necessary. 
	 */
	--num;
	nb = ip->i_ib[indirs[0].in_off];
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		/*
		 * If the filesystem has run out of space we can skip the
		 * full fsync/undo of the main [fail] case since no undo
		 * history has been built yet.  Hence the goto fail2.
		 */
	        if ((error = ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize,
		    cred, &newb)) != 0)
			goto fail2;
		nb = newb;
		*allocblk++ = nb;
		bp = getblk(vp, indirs[1].in_lbn, fs->fs_bsize, 0, 0);
		bp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(bp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocdirect(ip, NDADDR + indirs[0].in_off,
			    newb, 0, fs->fs_bsize, 0, bp);
			bdwrite(bp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if (DOINGASYNC(vp))
				bdwrite(bp);
			else if ((error = bwrite(bp)) != 0)
				goto fail;
		}
		allocib = &ip->i_ib[indirs[0].in_off];
		*allocib = nb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}

	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = bread(vp, indirs[i].in_lbn, (int)fs->fs_bsize, &bp);
		if (error) {
			brelse(bp);
			goto fail;
		}
		bap = (ufs_daddr_t *)bp->b_data;
		nb = bap[indirs[i].in_off];
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			bqrelse(bp);
			continue;
		}
		if (pref == 0)
			pref = ffs_blkpref(ip, lbn, 0, (ufs_daddr_t *)0);
		if ((error =
		    ffs_alloc(ip, lbn, pref, (int)fs->fs_bsize, cred, &newb)) != 0) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = getblk(vp, indirs[i].in_lbn, fs->fs_bsize, 0, 0);
		nbp->b_blkno = fsbtodb(fs, nb);
		vfs_bio_clrbuf(nbp);
		if (DOINGSOFTDEP(vp)) {
			softdep_setup_allocindir_meta(nbp, ip, bp,
			    indirs[i - 1].in_off, nb);
			bdwrite(nbp);
		} else {
			/*
			 * Write synchronously so that indirect blocks
			 * never point at garbage.
			 */
			if ((error = bwrite(nbp)) != 0) {
				brelse(bp);
				goto fail;
			}
		}
		bap[indirs[i - 1].in_off] = nb;
		if (allocib == NULL && unwindidx < 0)
			unwindidx = i - 1;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
	}

	/*
	 * Get the data block, allocating if necessary.  We have already
	 * called getblk() on the data block buffer, dbp.  If we have to
	 * allocate it and B_CLRBUF has been set the inference is an intention
	 * to zero out the related disk blocks, so we do not have to issue
	 * a read.  Instead we simply call vfs_bio_clrbuf().  If B_CLRBUF is
	 * not set the caller intends to overwrite the entire contents of the
	 * buffer and we don't waste time trying to clean up the contents.
	 *
	 * bp references the current indirect block.  When allocating, 
	 * the block must be updated.
	 */
	if (nb == 0) {
		pref = ffs_blkpref(ip, lbn, indirs[i].in_off, &bap[0]);
		error = ffs_alloc(ip,
		    lbn, pref, (int)fs->fs_bsize, cred, &newb);
		if (error) {
			brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		dbp->b_blkno = fsbtodb(fs, nb);
		if (flags & B_CLRBUF)
			vfs_bio_clrbuf(dbp);
		if (DOINGSOFTDEP(vp))
			softdep_setup_allocindir_page(ip, lbn, bp,
			    indirs[i].in_off, nb, 0, dbp);
		bap[indirs[i].in_off] = nb;
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			bwrite(bp);
		} else {
			if (bp->b_bufsize == fs->fs_bsize)
				bp->b_flags |= B_CLUSTEROK;
			bdwrite(bp);
		}
		*ap->a_bpp = dbp;
		return (0);
	}
	brelse(bp);

	/*
	 * At this point all related indirect blocks have been allocated
	 * if necessary and released.  bp is no longer valid.  dbp holds
	 * our getblk()'d data block.
	 *
	 * XXX we previously performed a cluster_read operation here.
	 */
	if (flags & B_CLRBUF) {
		/*
		 * If B_CLRBUF is set we must validate the invalid portions
		 * of the buffer.  This typically requires a read-before-
		 * write.  The strategy call will fill in b_blkno in that
		 * case.
		 *
		 * If we hit this case we do a cluster read if possible
		 * since nearby data blocks are likely to be accessed soon
		 * too.
		 */
		if ((dbp->b_flags & B_CACHE) == 0) {
			bqrelse(dbp);
			seqcount = (flags & B_SEQMASK) >> B_SEQSHIFT;
			if (seqcount &&
			    (vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0) {
				error = cluster_read(vp, ip->i_size, lbn,
					    (int)fs->fs_bsize, 
					    MAXBSIZE, seqcount, &dbp);
			} else {
				error = bread(vp, lbn, (int)fs->fs_bsize, &dbp);
			}
			if (error)
				goto fail;
		} else {
			dbp->b_blkno = fsbtodb(fs, nb);
		}
	} else {
		/*
		 * If B_CLRBUF is not set the caller intends to overwrite
		 * the entire contents of the buffer.  We can simply set
		 * b_blkno and we are done.
		 */
		dbp->b_blkno = fsbtodb(fs, nb);
	}
	*ap->a_bpp = dbp;
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 * We have to fsync the file before we start to get rid of all
	 * of its dependencies so that we do not leave them dangling.
	 * We have to sync it at the end so that the soft updates code
	 * does not find any untracked changes. Although this is really
	 * slow, running out of disk space is not expected to be a common
	 * occurence. The error return from fsync is ignored as we already
	 * have an error to return to the user.
	 */
	(void) VOP_FSYNC(vp, MNT_WAIT, td);
	for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ffs_blkfree(ip, *blkp, fs->fs_bsize);
		deallocated += fs->fs_bsize;
	}
	if (allocib != NULL) {
		*allocib = 0;
	} else if (unwindidx >= 0) {
		int r;

		r = bread(vp, indirs[unwindidx].in_lbn, (int)fs->fs_bsize, &bp);
		if (r) {
			panic("Could not unwind indirect block, error %d", r);
			brelse(bp);
		} else {
			bap = (ufs_daddr_t *)bp->b_data;
			bap[indirs[unwindidx].in_off] = 0;
			if (flags & B_SYNC) {
				bwrite(bp);
			} else {
				if (bp->b_bufsize == fs->fs_bsize)
					bp->b_flags |= B_CLUSTEROK;
				bdwrite(bp);
			}
		}
	}
	if (deallocated) {
#ifdef QUOTA
		/*
		 * Restore user's disk quota because allocation failed.
		 */
		(void) chkdq(ip, (long)-btodb(deallocated), cred, FORCE);
#endif
		ip->i_blocks -= btodb(deallocated);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	(void) VOP_FSYNC(vp, MNT_WAIT, td);

	/*
	 * Cleanup the data block we getblk()'d before returning.
	 */
fail2:
	brelse(dbp);
	return (error);
}

