/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_serv.c  8.8 (Berkeley) 7/31/95
 * $FreeBSD: src/sys/nfs/nfs_serv.c,v 1.93.2.6 2002/12/29 18:19:53 dillon Exp $
 * $DragonFly: src/sys/vfs/nfs/nfs_serv.c,v 1.22 2004/12/29 02:41:26 dillon Exp $
 */

/*
 * nfs version 2 and 3 server calls to vnode ops
 * - these routines generally have 3 phases
 *   1 - break down and validate rpc request in mbuf list
 *   2 - do the vnode ops for the request
 *       (surprisingly ?? many are very similar to syscalls in vfs_syscalls.c)
 *   3 - build the rpc reply in an mbuf list
 *   nb:
 *	- do not mix the phases, since the nfsm_?? macros can return failures
 *	  on a bad rpc or similar and do not do any vrele() or vput()'s
 *
 *      - the nfsm_reply() macro generates an nfs rpc reply with the nfs
 *	error number iff error != 0 whereas
 *	returning an error from the server function implies a fatal error
 *	such as a badly constructed rpc request that should be dropped without
 *	a reply.
 *	For Version 3, nfsm_reply() does not return for the error case, since
 *	most version 3 rpcs return more than the status for error cases.
 *
 * Other notes:
 *	Warning: always pay careful attention to resource cleanup on return
 *	and note that nfsm_*() macros can terminate a procedure on certain
 *	errors.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/nlookup.h>
#include <sys/namei.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>
#include <vm/vm_object.h>

#include <sys/buf2.h>

#include "nfsproto.h"
#include "rpcv2.h"
#include "nfs.h"
#include "xdr_subs.h"
#include "nfsm_subs.h"
#include "nqnfs.h"

#ifdef NFSRV_DEBUG
#define nfsdbprintf(info)	printf info
#else
#define nfsdbprintf(info)
#endif

#define MAX_COMMIT_COUNT	(1024 * 1024)

#define NUM_HEURISTIC		1017
#define NHUSE_INIT		64
#define NHUSE_INC		16
#define NHUSE_MAX		2048

static struct nfsheur {
    struct vnode *nh_vp;	/* vp to match (unreferenced pointer) */
    off_t nh_nextr;		/* next offset for sequential detection */
    int nh_use;			/* use count for selection */
    int nh_seqcount;		/* heuristic */
} nfsheur[NUM_HEURISTIC];

nfstype nfsv3_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFSOCK,
		      NFFIFO, NFNON };
#ifndef NFS_NOSERVER 
nfstype nfsv2_type[9] = { NFNON, NFREG, NFDIR, NFBLK, NFCHR, NFLNK, NFNON,
		      NFCHR, NFNON };
/* Global vars */
extern u_int32_t nfs_xdrneg1;
extern u_int32_t nfs_false, nfs_true;
extern enum vtype nv3tov_type[8];
extern struct nfsstats nfsstats;

int nfsrvw_procrastinate = NFS_GATHERDELAY * 1000;
int nfsrvw_procrastinate_v3 = 0;

static struct timespec	nfsver;

SYSCTL_DECL(_vfs_nfs);

static int nfs_async;
SYSCTL_INT(_vfs_nfs, OID_AUTO, async, CTLFLAG_RW, &nfs_async, 0, "");
static int nfs_commit_blks;
static int nfs_commit_miss;
SYSCTL_INT(_vfs_nfs, OID_AUTO, commit_blks, CTLFLAG_RW, &nfs_commit_blks, 0, "");
SYSCTL_INT(_vfs_nfs, OID_AUTO, commit_miss, CTLFLAG_RW, &nfs_commit_miss, 0, "");

static int nfsrv_access (struct vnode *,int,struct ucred *,int,
		struct thread *, int);
static void nfsrvw_coalesce (struct nfsrv_descript *,
		struct nfsrv_descript *);

/*
 * nfs v3 access service
 */
int
nfsrv3_access(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	      struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, getret;
	char *cp2;
	struct mbuf *mb, *mreq, *mb2;
	struct vattr vattr, *vap = &vattr;
	u_long testmode, nfsmode;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly,
	    (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}
	nfsmode = fxdr_unsigned(u_int32_t, *tl);
	if ((nfsmode & NFSV3ACCESS_READ) &&
		nfsrv_access(vp, VREAD, cred, rdonly, td, 0))
		nfsmode &= ~NFSV3ACCESS_READ;
	if (vp->v_type == VDIR)
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND |
			NFSV3ACCESS_DELETE);
	else
		testmode = (NFSV3ACCESS_MODIFY | NFSV3ACCESS_EXTEND);
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VWRITE, cred, rdonly, td, 0))
		nfsmode &= ~testmode;
	if (vp->v_type == VDIR)
		testmode = NFSV3ACCESS_LOOKUP;
	else
		testmode = NFSV3ACCESS_EXECUTE;
	if ((nfsmode & testmode) &&
		nfsrv_access(vp, VEXEC, cred, rdonly, td, 0))
		nfsmode &= ~testmode;
	getret = VOP_GETATTR(vp, vap, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(1) + NFSX_UNSIGNED);
	nfsm_srvpostop_attr(getret, vap);
	nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(nfsmode);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs getattr service
 */
int
nfsrv_getattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	      struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va;
	struct vattr *vap = &va;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(0);
		error = 0;
		goto nfsmout;
	}
	nqsrv_getl(vp, ND_READ);
	error = VOP_GETATTR(vp, vap, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	if (error) {
		error = 0;
		goto nfsmout;
	}
	nfsm_build(fp, struct nfs_fattr *, NFSX_FATTR(nfsd->nd_flag & ND_NFSV3));
	nfsm_srvfillattr(vap, fp);
	/* fall through */

nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs setattr service
 */
int
nfsrv_setattr(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	      struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, preat;
	struct vattr *vap = &va;
	struct nfsv2_sattr *sp;
	struct nfs_fattr *fp;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, preat_ret = 1, postat_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), gcheck = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	u_quad_t frev;
	struct timespec guard;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	VATTR_NULL(vap);
	if (v3) {
		nfsm_srvsattr(vap);
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		gcheck = fxdr_unsigned(int, *tl);
		if (gcheck) {
			nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			fxdr_nfsv3time(tl, &guard);
		}
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		/*
		 * Nah nah nah nah na nah
		 * There is a bug in the Sun client that puts 0xffff in the mode
		 * field of sattr when it should put in 0xffffffff. The u_short
		 * doesn't sign extend.
		 * --> check the low order 2 bytes for 0xffff
		 */
		if ((fxdr_unsigned(int, sp->sa_mode) & 0xffff) != 0xffff)
			vap->va_mode = nfstov_mode(sp->sa_mode);
		if (sp->sa_uid != nfs_xdrneg1)
			vap->va_uid = fxdr_unsigned(uid_t, sp->sa_uid);
		if (sp->sa_gid != nfs_xdrneg1)
			vap->va_gid = fxdr_unsigned(gid_t, sp->sa_gid);
		if (sp->sa_size != nfs_xdrneg1)
			vap->va_size = fxdr_unsigned(u_quad_t, sp->sa_size);
		if (sp->sa_atime.nfsv2_sec != nfs_xdrneg1) {
#ifdef notyet
			fxdr_nfsv2time(&sp->sa_atime, &vap->va_atime);
#else
			vap->va_atime.tv_sec =
				fxdr_unsigned(int32_t, sp->sa_atime.nfsv2_sec);
			vap->va_atime.tv_nsec = 0;
#endif
		}
		if (sp->sa_mtime.nfsv2_sec != nfs_xdrneg1)
			fxdr_nfsv2time(&sp->sa_mtime, &vap->va_mtime);

	}

	/*
	 * Now that we have all the fields, lets do it.
	 */
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly,
		(nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
		error = 0;
		goto nfsmout;
	}

	/*
	 * vp now an active resource, pay careful attention to cleanup
	 */

	nqsrv_getl(vp, ND_WRITE);
	if (v3) {
		error = preat_ret = VOP_GETATTR(vp, &preat, td);
		if (!error && gcheck &&
			(preat.va_ctime.tv_sec != guard.tv_sec ||
			 preat.va_ctime.tv_nsec != guard.tv_nsec))
			error = NFSERR_NOT_SYNC;
		if (error) {
			vput(vp);
			vp = NULL;
			nfsm_reply(NFSX_WCCDATA(v3));
			nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * If the size is being changed write acces is required, otherwise
	 * just check for a read only file system.
	 */
	if (vap->va_size == ((u_quad_t)((quad_t) -1))) {
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			error = EROFS;
			goto out;
		}
	} else {
		if (vp->v_type == VDIR) {
			error = EISDIR;
			goto out;
		} else if ((error = nfsrv_access(vp, VWRITE, cred, rdonly,
			    td, 0)) != 0){ 
			goto out;
		}
	}
	error = VOP_SETATTR(vp, vap, cred, td);
	postat_ret = VOP_GETATTR(vp, vap, td);
	if (!error)
		error = postat_ret;
out:
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_WCCORFATTR(v3));
	if (v3) {
		nfsm_srvwcc_data(preat_ret, &preat, postat_ret, vap);
		error = 0;
		goto nfsmout;
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	/* fall through */

nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs lookup rpc
 */
int
nfsrv_lookup(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct nlookupdata nd;
	struct vnode *vp;
	struct vnode *dirp;
	struct namecache *ncp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache = 0, len, dirattr_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), pubflag;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vattr va, dirattr, *vap = &va;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	pubflag = nfs_ispublicfh(fhp);

	error = nfs_namei(&nd, cred, NAMEI_LOOKUP, NULL, &vp,
		fhp, len, slp, nam, &md, &dpos,
		&dirp, td, (nfsd->nd_flag & ND_KERBAUTH), pubflag);

	/*
	 * namei failure, only dirp to cleanup.  Clear out garbarge from
	 * structure in case macros jump to nfsmout.
	 */

	if (error) {
		if (dirp) {
			if (v3)
				dirattr_ret = VOP_GETATTR(dirp, &dirattr, td);
			vrele(dirp);
			dirp = NULL;
		}
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Locate index file for public filehandle
	 *
	 * error is 0 on entry and 0 on exit from this block.
	 */

	if (pubflag) {
		if (vp->v_type == VDIR && nfs_pub.np_index != NULL) {
			/*
			 * Setup call to lookup() to see if we can find
			 * the index file. Arguably, this doesn't belong
			 * in a kernel.. Ugh.  If an error occurs, do not
			 * try to install an index file and then clear the
			 * error.
			 *
			 * When we replace nd with ind and redirect ndp,
			 * maintenance of ni_startdir and ni_vp shift to
			 * ind and we have to clean them up in the old nd.
			 * However, the cnd resource continues to be maintained
			 * via the original nd.  Confused?  You aren't alone!
			 */
			VOP_UNLOCK(vp, 0, td);
			ncp = cache_hold(nd.nl_ncp);
			nlookup_done(&nd);
			error = nlookup_init_raw(&nd, nfs_pub.np_index,
						UIO_SYSSPACE, 0, cred, ncp);
			cache_drop(ncp);
			if (error == 0)
				error = nlookup(&nd);

			if (error == 0) {
				/*
				 * Found an index file. Get rid of
				 * the old references.  transfer vp and
				 * load up the new vp.  Fortunately we do
				 * not have to deal with dvp, that would be
				 * a huge mess.
				 */
				if (dirp)	
					vrele(dirp);
				dirp = vp;
				vp = NULL;
				error = cache_vget(nd.nl_ncp, nd.nl_cred,
							LK_EXCLUSIVE, &vp);
				KKASSERT(error == 0);
			}
			error = 0;
		}
		/*
		 * If the public filehandle was used, check that this lookup
		 * didn't result in a filehandle outside the publicly exported
		 * filesystem.  We clear the poor vp here to avoid lockups due
		 * to NFS I/O.
		 */

		if (vp->v_mount != nfs_pub.np_mount) {
			vput(vp);
			vp = NULL;
			error = EPERM;
		}
	}

	if (dirp) {
		if (v3)
			dirattr_ret = VOP_GETATTR(dirp, &dirattr, td);
		vrele(dirp);
		dirp = NULL;
	}

	/*
	 * Resources at this point:
	 *	ndp->ni_vp	may not be NULL
	 *
	 */

	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}

#if 0
	/* XXX not sure how to deal with this */
	nqsrv_getl(ndp->ni_startdir, ND_READ);
#endif

	/*
	 * Clear out some resources prior to potentially blocking.  This
	 * is not as critical as ni_dvp resources in other routines, but
	 * it helps.
	 */
	nlookup_done(&nd);

	/*
	 * Get underlying attribute, then release remaining resources ( for
	 * the same potential blocking reason ) and reply.
	 */
	bzero((caddr_t)fhp, sizeof(nfh));
	fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fhp->fh_fid);
	if (!error)
		error = VOP_GETATTR(vp, vap, td);

	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPORFATTR(v3) + NFSX_POSTOPATTR(v3));
	if (error) {
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
		error = 0;
		goto nfsmout;
	}
	nfsm_srvfhtom(fhp, v3);
	if (v3) {
		nfsm_srvpostop_attr(0, vap);
		nfsm_srvpostop_attr(dirattr_ret, &dirattr);
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}

nfsmout:
	if (dirp)
		vrele(dirp);
	nlookup_done(&nd);		/* may be called twice */
	if (vp)
		vput(vp);
	return (error);
}

/*
 * nfs readlink service
 */
int
nfsrv_readlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	       struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec iv[(NFS_MAXPATHLEN+MLEN-1)/MLEN];
	struct iovec *ivp = iv;
	struct mbuf *mp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, i, tlen, len, getret;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mb2, *mp2, *mp3, *mreq;
	struct vnode *vp = NULL;
	struct vattr attr;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	mp2 = (struct mbuf *)0;
#endif
	mp3 = NULL;
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	len = 0;
	i = 0;
	while (len < NFS_MAXPATHLEN) {
		MGET(mp, MB_WAIT, MT_DATA);
		MCLGET(mp, MB_WAIT);
		mp->m_len = NFSMSIZ(mp);
		if (len == 0)
			mp3 = mp2 = mp;
		else {
			mp2->m_next = mp;
			mp2 = mp;
		}
		if ((len+mp->m_len) > NFS_MAXPATHLEN) {
			mp->m_len = NFS_MAXPATHLEN-len;
			len = NFS_MAXPATHLEN;
		} else
			len += mp->m_len;
		ivp->iov_base = mtod(mp, caddr_t);
		ivp->iov_len = mp->m_len;
		i++;
		ivp++;
	}
	uiop->uio_iov = iv;
	uiop->uio_iovcnt = i;
	uiop->uio_offset = 0;
	uiop->uio_resid = len;
	uiop->uio_rw = UIO_READ;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_td = NULL;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type != VLNK) {
		if (v3)
			error = EINVAL;
		else
			error = ENXIO;
		goto out;
	}
	nqsrv_getl(vp, ND_READ);
	error = VOP_READLINK(vp, uiop, cred);
out:
	getret = VOP_GETATTR(vp, &attr, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_UNSIGNED);
	if (v3) {
		nfsm_srvpostop_attr(getret, &attr);
		if (error) {
			error = 0;
			goto nfsmout;
		}
	}
	if (uiop->uio_resid > 0) {
		len -= uiop->uio_resid;
		tlen = nfsm_rndup(len);
		nfsm_adj(mp3, NFS_MAXPATHLEN-tlen, tlen-len);
	}
	nfsm_build(tl, u_int32_t *, NFSX_UNSIGNED);
	*tl = txdr_unsigned(len);
	mb->m_next = mp3;
	mp3 = NULL;
nfsmout:
	if (mp3)
		m_freem(mp3);
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs read service
 */
int
nfsrv_read(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	   struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec *iv;
	struct iovec *iv2;
	struct mbuf *m;
	struct nfs_fattr *fp;
	u_int32_t *tl;
	int32_t t1;
	int i;
	caddr_t bpos;
	int error = 0, rdonly, cache, cnt, len, left, siz, tlen, getret;
	int v3 = (nfsd->nd_flag & ND_NFSV3), reqlen;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct mbuf *m2;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	struct vattr va, *vap = &va;
	struct nfsheur *nh;
	off_t off;
	int ioflag = 0;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *tl);
	}
	nfsm_srvstrsiz(reqlen, NFS_SRVMAXDATA(nfsd));

	/*
	 * Reference vp.  If an error occurs, vp will be invalid, but we
	 * have to NULL it just in case.  The macros might goto nfsmout
	 * as well.
	 */

	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		vp = NULL;
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvpostop_attr(1, (struct vattr *)0);
		error = 0;
		goto nfsmout;
	}

	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
	    nqsrv_getl(vp, ND_READ);
	    if ((error = nfsrv_access(vp, VREAD, cred, rdonly, td, 1)) != 0)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 1);
	}
	getret = VOP_GETATTR(vp, vap, td);
	if (!error)
		error = getret;
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, vap);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Calculate byte count to read
	 */

	if (off >= vap->va_size)
		cnt = 0;
	else if ((off + reqlen) > vap->va_size)
		cnt = vap->va_size - off;
	else
		cnt = reqlen;

	/*
	 * Calculate seqcount for heuristic
	 */

	{
		int hi;
		int try = 32;

		/*
		 * Locate best candidate
		 */

		hi = ((int)(vm_offset_t)vp / sizeof(struct vnode)) % NUM_HEURISTIC;
		nh = &nfsheur[hi];

		while (try--) {
			if (nfsheur[hi].nh_vp == vp) {
				nh = &nfsheur[hi];
				break;
			}
			if (nfsheur[hi].nh_use > 0)
				--nfsheur[hi].nh_use;
			hi = (hi + 1) % NUM_HEURISTIC;
			if (nfsheur[hi].nh_use < nh->nh_use)
				nh = &nfsheur[hi];
		}

		if (nh->nh_vp != vp) {
			nh->nh_vp = vp;
			nh->nh_nextr = off;
			nh->nh_use = NHUSE_INIT;
			if (off == 0)
				nh->nh_seqcount = 4;
			else
				nh->nh_seqcount = 1;
		}

		/*
		 * Calculate heuristic
		 */

		if ((off == 0 && nh->nh_seqcount > 0) || off == nh->nh_nextr) {
			if (++nh->nh_seqcount > IO_SEQMAX)
				nh->nh_seqcount = IO_SEQMAX;
		} else if (nh->nh_seqcount > 1) {
			nh->nh_seqcount = 1;
		} else {
			nh->nh_seqcount = 0;
		}
		nh->nh_use += NHUSE_INC;
		if (nh->nh_use > NHUSE_MAX)
			nh->nh_use = NHUSE_MAX;
		ioflag |= nh->nh_seqcount << IO_SEQSHIFT;
        }

	nfsm_reply(NFSX_POSTOPORFATTR(v3) + 3 * NFSX_UNSIGNED+nfsm_rndup(cnt));
	if (v3) {
		nfsm_build(tl, u_int32_t *, NFSX_V3FATTR + 4 * NFSX_UNSIGNED);
		*tl++ = nfs_true;
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V3FATTR / sizeof (u_int32_t));
	} else {
		nfsm_build(tl, u_int32_t *, NFSX_V2FATTR + NFSX_UNSIGNED);
		fp = (struct nfs_fattr *)tl;
		tl += (NFSX_V2FATTR / sizeof (u_int32_t));
	}
	len = left = nfsm_rndup(cnt);
	if (cnt > 0) {
		/*
		 * Generate the mbuf list with the uio_iov ref. to it.
		 */
		i = 0;
		m = m2 = mb;
		while (left > 0) {
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				left -= siz;
				i++;
			}
			if (left > 0) {
				MGET(m, MB_WAIT, MT_DATA);
				MCLGET(m, MB_WAIT);
				m->m_len = 0;
				m2->m_next = m;
				m2 = m;
			}
		}
		MALLOC(iv, struct iovec *, i * sizeof (struct iovec),
		       M_TEMP, M_WAITOK);
		uiop->uio_iov = iv2 = iv;
		m = mb;
		left = len;
		i = 0;
		while (left > 0) {
			if (m == NULL)
				panic("nfsrv_read iov");
			siz = min(M_TRAILINGSPACE(m), left);
			if (siz > 0) {
				iv->iov_base = mtod(m, caddr_t) + m->m_len;
				iv->iov_len = siz;
				m->m_len += siz;
				left -= siz;
				iv++;
				i++;
			}
			m = m->m_next;
		}
		uiop->uio_iovcnt = i;
		uiop->uio_offset = off;
		uiop->uio_resid = len;
		uiop->uio_rw = UIO_READ;
		uiop->uio_segflg = UIO_SYSSPACE;
		error = VOP_READ(vp, uiop, IO_NODELOCKED | ioflag, cred);
		off = uiop->uio_offset;
		nh->nh_nextr = off;
		FREE((caddr_t)iv2, M_TEMP);
		if (error || (getret = VOP_GETATTR(vp, vap, td))) {
			if (!error)
				error = getret;
			m_freem(mreq);
			vput(vp);
			vp = NULL;
			nfsm_reply(NFSX_POSTOPATTR(v3));
			nfsm_srvpostop_attr(getret, vap);
			error = 0;
			goto nfsmout;
		}
	} else {
		uiop->uio_resid = 0;
	}
	vput(vp);
	vp = NULL;
	nfsm_srvfillattr(vap, fp);
	tlen = len - uiop->uio_resid;
	cnt = cnt < tlen ? cnt : tlen;
	tlen = nfsm_rndup(cnt);
	if (len != tlen || tlen != cnt)
		nfsm_adj(mb, len - tlen, tlen - cnt);
	if (v3) {
		*tl++ = txdr_unsigned(cnt);
		if (len < reqlen)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
	}
	*tl = txdr_unsigned(cnt);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs write service
 */
int
nfsrv_write(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct iovec *ivp;
	int i, cnt;
	struct mbuf *mp;
	struct nfs_fattr *fp;
	struct iovec *iv;
	struct vattr va, forat;
	struct vattr *vap = &va;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, retlen, zeroing, adjust;
	int stable = NFSV3WRITE_FILESYNC;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io, *uiop = &io;
	off_t off;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (mrep == NULL) {
		*mrq = NULL;
		error = 0;
		goto nfsmout;
	}
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		off = fxdr_hyper(tl);
		tl += 3;
		stable = fxdr_unsigned(int, *tl++);
	} else {
		nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
		if (nfs_async)
	    		stable = NFSV3WRITE_UNSTABLE;
	}
	retlen = len = fxdr_unsigned(int32_t, *tl);
	cnt = i = 0;

	/*
	 * For NFS Version 2, it is not obvious what a write of zero length
	 * should do, but I might as well be consistent with Version 3,
	 * which is to return ok so long as there are no permission problems.
	 */
	if (len > 0) {
	    zeroing = 1;
	    mp = mrep;
	    while (mp) {
		if (mp == md) {
			zeroing = 0;
			adjust = dpos - mtod(mp, caddr_t);
			mp->m_len -= adjust;
			if (mp->m_len > 0 && adjust > 0)
				NFSMADV(mp, adjust);
		}
		if (zeroing)
			mp->m_len = 0;
		else if (mp->m_len > 0) {
			i += mp->m_len;
			if (i > len) {
				mp->m_len -= (i - len);
				zeroing	= 1;
			}
			if (mp->m_len > 0)
				cnt++;
		}
		mp = mp->m_next;
	    }
	}
	if (len > NFS_MAXDATA || len < 0 || i < len) {
		error = EIO;
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		vp = NULL;
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}
	if (v3)
		forat_ret = VOP_GETATTR(vp, &forat, td);
	if (vp->v_type != VREG) {
		if (v3)
			error = EINVAL;
		else
			error = (vp->v_type == VDIR) ? EISDIR : EACCES;
	}
	if (!error) {
		nqsrv_getl(vp, ND_WRITE);
		error = nfsrv_access(vp, VWRITE, cred, rdonly, td, 1);
	}
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		error = 0;
		goto nfsmout;
	}

	if (len > 0) {
	    MALLOC(ivp, struct iovec *, cnt * sizeof (struct iovec), M_TEMP,
		M_WAITOK);
	    uiop->uio_iov = iv = ivp;
	    uiop->uio_iovcnt = cnt;
	    mp = mrep;
	    while (mp) {
		if (mp->m_len > 0) {
			ivp->iov_base = mtod(mp, caddr_t);
			ivp->iov_len = mp->m_len;
			ivp++;
		}
		mp = mp->m_next;
	    }

	    /*
	     * XXX
	     * The IO_METASYNC flag indicates that all metadata (and not just
	     * enough to ensure data integrity) mus be written to stable storage
	     * synchronously.
	     * (IO_METASYNC is not yet implemented in 4.4BSD-Lite.)
	     */
	    if (stable == NFSV3WRITE_UNSTABLE)
		ioflags = IO_NODELOCKED;
	    else if (stable == NFSV3WRITE_DATASYNC)
		ioflags = (IO_SYNC | IO_NODELOCKED);
	    else
		ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
	    uiop->uio_resid = len;
	    uiop->uio_rw = UIO_WRITE;
	    uiop->uio_segflg = UIO_SYSSPACE;
	    uiop->uio_td = NULL;
	    uiop->uio_offset = off;
	    error = VOP_WRITE(vp, uiop, ioflags, cred);
	    nfsstats.srvvop_writes++;
	    FREE((caddr_t)iv, M_TEMP);
	}
	aftat_ret = VOP_GETATTR(vp, vap, td);
	vput(vp);
	vp = NULL;
	if (!error)
		error = aftat_ret;
	nfsm_reply(NFSX_PREOPATTR(v3) + NFSX_POSTOPORFATTR(v3) +
		2 * NFSX_UNSIGNED + NFSX_WRITEVERF(v3));
	if (v3) {
		nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, vap);
		if (error) {
			error = 0;
			goto nfsmout;
		}
		nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		*tl++ = txdr_unsigned(retlen);
		/*
		 * If nfs_async is set, then pretend the write was FILESYNC.
		 */
		if (stable == NFSV3WRITE_UNSTABLE && !nfs_async)
			*tl++ = txdr_unsigned(stable);
		else
			*tl++ = txdr_unsigned(NFSV3WRITE_FILESYNC);
		/*
		 * Actually, there is no need to txdr these fields,
		 * but it may make the values more human readable,
		 * for debugging purposes.
		 */
		if (nfsver.tv_sec == 0)
			nfsver = boottime;
		*tl++ = txdr_unsigned(nfsver.tv_sec);
		*tl = txdr_unsigned(nfsver.tv_nsec / 1000);
	} else {
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * NFS write service with write gathering support. Called when
 * nfsrvw_procrastinate > 0.
 * See: Chet Juszczak, "Improving the Write Performance of an NFS Server",
 * in Proc. of the Winter 1994 Usenix Conference, pg. 247-259, San Franscisco,
 * Jan. 1994.
 */
int
nfsrv_writegather(struct nfsrv_descript **ndp, struct nfssvc_sock *slp,
		  struct thread *td, struct mbuf **mrq)
{
	struct iovec *ivp;
	struct mbuf *mp;
	struct nfsrv_descript *wp, *nfsd, *owp, *swp;
	struct nfs_fattr *fp;
	int i;
	struct iovec *iov;
	struct nfsrvw_delayhash *wpp;
	struct ucred *cred;
	struct vattr va, forat;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos, dpos;
	int error = 0, rdonly, cache, len, forat_ret = 1;
	int ioflags, aftat_ret = 1, s, adjust, v3, zeroing;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq, *mrep, *md;
	struct vnode *vp = NULL;
	struct uio io, *uiop = &io;
	u_quad_t frev, cur_usec;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	i = 0;
	len = 0;
#endif
	*mrq = NULL;
	if (*ndp) {
	    nfsd = *ndp;
	    *ndp = NULL;
	    mrep = nfsd->nd_mrep;
	    md = nfsd->nd_md;
	    dpos = nfsd->nd_dpos;
	    cred = &nfsd->nd_cr;
	    v3 = (nfsd->nd_flag & ND_NFSV3);
	    LIST_INIT(&nfsd->nd_coalesce);
	    nfsd->nd_mreq = NULL;
	    nfsd->nd_stable = NFSV3WRITE_FILESYNC;
	    cur_usec = nfs_curusec();
	    nfsd->nd_time = cur_usec +
		(v3 ? nfsrvw_procrastinate_v3 : nfsrvw_procrastinate);
    
	    /*
	     * Now, get the write header..
	     */
	    nfsm_srvmtofh(&nfsd->nd_fh);
	    if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		nfsd->nd_off = fxdr_hyper(tl);
		tl += 3;
		nfsd->nd_stable = fxdr_unsigned(int, *tl++);
	    } else {
		nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
		nfsd->nd_off = (off_t)fxdr_unsigned(u_int32_t, *++tl);
		tl += 2;
		if (nfs_async)
			nfsd->nd_stable = NFSV3WRITE_UNSTABLE;
	    }
	    len = fxdr_unsigned(int32_t, *tl);
	    nfsd->nd_len = len;
	    nfsd->nd_eoff = nfsd->nd_off + len;
    
	    /*
	     * Trim the header out of the mbuf list and trim off any trailing
	     * junk so that the mbuf list has only the write data.
	     */
	    zeroing = 1;
	    i = 0;
	    mp = mrep;
	    while (mp) {
		if (mp == md) {
		    zeroing = 0;
		    adjust = dpos - mtod(mp, caddr_t);
		    mp->m_len -= adjust;
		    if (mp->m_len > 0 && adjust > 0)
			NFSMADV(mp, adjust);
		}
		if (zeroing)
		    mp->m_len = 0;
		else {
		    i += mp->m_len;
		    if (i > len) {
			mp->m_len -= (i - len);
			zeroing = 1;
		    }
		}
		mp = mp->m_next;
	    }
	    if (len > NFS_MAXDATA || len < 0  || i < len) {
nfsmout:
		m_freem(mrep);
		error = EIO;
		nfsm_writereply(2 * NFSX_UNSIGNED, v3);
		if (v3)
		    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
		nfsd->nd_mreq = mreq;
		nfsd->nd_mrep = NULL;
		nfsd->nd_time = 0;
	    }
    
	    /*
	     * Add this entry to the hash and time queues.
	     */
	    s = splsoftclock();
	    owp = NULL;
	    wp = slp->ns_tq.lh_first;
	    while (wp && wp->nd_time < nfsd->nd_time) {
		owp = wp;
		wp = wp->nd_tq.le_next;
	    }
	    NFS_DPF(WG, ("Q%03x", nfsd->nd_retxid & 0xfff));
	    if (owp) {
		LIST_INSERT_AFTER(owp, nfsd, nd_tq);
	    } else {
		LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
	    }
	    if (nfsd->nd_mrep) {
		wpp = NWDELAYHASH(slp, nfsd->nd_fh.fh_fid.fid_data);
		owp = NULL;
		wp = wpp->lh_first;
		while (wp &&
		    bcmp((caddr_t)&nfsd->nd_fh,(caddr_t)&wp->nd_fh,NFSX_V3FH)) {
		    owp = wp;
		    wp = wp->nd_hash.le_next;
		}
		while (wp && wp->nd_off < nfsd->nd_off &&
		    !bcmp((caddr_t)&nfsd->nd_fh,(caddr_t)&wp->nd_fh,NFSX_V3FH)) {
		    owp = wp;
		    wp = wp->nd_hash.le_next;
		}
		if (owp) {
		    LIST_INSERT_AFTER(owp, nfsd, nd_hash);

		    /*
		     * Search the hash list for overlapping entries and
		     * coalesce.
		     */
		    for(; nfsd && NFSW_CONTIG(owp, nfsd); nfsd = wp) {
			wp = nfsd->nd_hash.le_next;
			if (NFSW_SAMECRED(owp, nfsd))
			    nfsrvw_coalesce(owp, nfsd);
		    }
		} else {
		    LIST_INSERT_HEAD(wpp, nfsd, nd_hash);
		}
	    }
	    splx(s);
	}
    
	/*
	 * Now, do VOP_WRITE()s for any one(s) that need to be done now
	 * and generate the associated reply mbuf list(s).
	 */
loop1:
	cur_usec = nfs_curusec();
	s = splsoftclock();
	for (nfsd = slp->ns_tq.lh_first; nfsd; nfsd = owp) {
		owp = nfsd->nd_tq.le_next;
		if (nfsd->nd_time > cur_usec)
		    break;
		if (nfsd->nd_mreq)
		    continue;
		NFS_DPF(WG, ("P%03x", nfsd->nd_retxid & 0xfff));
		LIST_REMOVE(nfsd, nd_tq);
		LIST_REMOVE(nfsd, nd_hash);
		splx(s);
		mrep = nfsd->nd_mrep;
		nfsd->nd_mrep = NULL;
		cred = &nfsd->nd_cr;
		v3 = (nfsd->nd_flag & ND_NFSV3);
		forat_ret = aftat_ret = 1;
		error = nfsrv_fhtovp(&nfsd->nd_fh, 1, &vp, cred, slp, 
		    nfsd->nd_nam, &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
		if (!error) {
		    if (v3)
			forat_ret = VOP_GETATTR(vp, &forat, td);
		    if (vp->v_type != VREG) {
			if (v3)
			    error = EINVAL;
			else
			    error = (vp->v_type == VDIR) ? EISDIR : EACCES;
		    }
		} else {
		    vp = NULL;
		}
		if (!error) {
		    nqsrv_getl(vp, ND_WRITE);
		    error = nfsrv_access(vp, VWRITE, cred, rdonly, td, 1);
		}
    
		if (nfsd->nd_stable == NFSV3WRITE_UNSTABLE)
		    ioflags = IO_NODELOCKED;
		else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC)
		    ioflags = (IO_SYNC | IO_NODELOCKED);
		else
		    ioflags = (IO_METASYNC | IO_SYNC | IO_NODELOCKED);
		uiop->uio_rw = UIO_WRITE;
		uiop->uio_segflg = UIO_SYSSPACE;
		uiop->uio_td = NULL;
		uiop->uio_offset = nfsd->nd_off;
		uiop->uio_resid = nfsd->nd_eoff - nfsd->nd_off;
		if (uiop->uio_resid > 0) {
		    mp = mrep;
		    i = 0;
		    while (mp) {
			if (mp->m_len > 0)
			    i++;
			mp = mp->m_next;
		    }
		    uiop->uio_iovcnt = i;
		    MALLOC(iov, struct iovec *, i * sizeof (struct iovec), 
			M_TEMP, M_WAITOK);
		    uiop->uio_iov = ivp = iov;
		    mp = mrep;
		    while (mp) {
			if (mp->m_len > 0) {
			    ivp->iov_base = mtod(mp, caddr_t);
			    ivp->iov_len = mp->m_len;
			    ivp++;
			}
			mp = mp->m_next;
		    }
		    if (!error) {
			error = VOP_WRITE(vp, uiop, ioflags, cred);
			nfsstats.srvvop_writes++;
		    }
		    FREE((caddr_t)iov, M_TEMP);
		}
		m_freem(mrep);
		if (vp) {
		    aftat_ret = VOP_GETATTR(vp, &va, td);
		    vput(vp);
		    vp = NULL;
		}

		/*
		 * Loop around generating replies for all write rpcs that have
		 * now been completed.
		 */
		swp = nfsd;
		do {
		    NFS_DPF(WG, ("R%03x", nfsd->nd_retxid & 0xfff));
		    if (error) {
			nfsm_writereply(NFSX_WCCDATA(v3), v3);
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			}
		    } else {
			nfsm_writereply(NFSX_PREOPATTR(v3) +
			    NFSX_POSTOPORFATTR(v3) + 2 * NFSX_UNSIGNED +
			    NFSX_WRITEVERF(v3), v3);
			if (v3) {
			    nfsm_srvwcc_data(forat_ret, &forat, aftat_ret, &va);
			    nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			    *tl++ = txdr_unsigned(nfsd->nd_len);
			    *tl++ = txdr_unsigned(swp->nd_stable);
			    /*
			     * Actually, there is no need to txdr these fields,
			     * but it may make the values more human readable,
			     * for debugging purposes.
			     */
			    if (nfsver.tv_sec == 0)
				    nfsver = boottime;
			    *tl++ = txdr_unsigned(nfsver.tv_sec);
			    *tl = txdr_unsigned(nfsver.tv_nsec / 1000);
			} else {
			    nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
			    nfsm_srvfillattr(&va, fp);
			}
		    }
		    nfsd->nd_mreq = mreq;
		    if (nfsd->nd_mrep)
			panic("nfsrv_write: nd_mrep not free");

		    /*
		     * Done. Put it at the head of the timer queue so that
		     * the final phase can return the reply.
		     */
		    s = splsoftclock();
		    if (nfsd != swp) {
			nfsd->nd_time = 0;
			LIST_INSERT_HEAD(&slp->ns_tq, nfsd, nd_tq);
		    }
		    nfsd = swp->nd_coalesce.lh_first;
		    if (nfsd) {
			LIST_REMOVE(nfsd, nd_tq);
		    }
		    splx(s);
		} while (nfsd);
		s = splsoftclock();
		swp->nd_time = 0;
		LIST_INSERT_HEAD(&slp->ns_tq, swp, nd_tq);
		splx(s);
		goto loop1;
	}
	splx(s);

	/*
	 * Search for a reply to return.
	 */
	s = splsoftclock();
	for (nfsd = slp->ns_tq.lh_first; nfsd; nfsd = nfsd->nd_tq.le_next)
		if (nfsd->nd_mreq) {
		    NFS_DPF(WG, ("X%03x", nfsd->nd_retxid & 0xfff));
		    LIST_REMOVE(nfsd, nd_tq);
		    *mrq = nfsd->nd_mreq;
		    *ndp = nfsd;
		    break;
		}
	splx(s);
	return (0);
}

/*
 * Coalesce the write request nfsd into owp. To do this we must:
 * - remove nfsd from the queues
 * - merge nfsd->nd_mrep into owp->nd_mrep
 * - update the nd_eoff and nd_stable for owp
 * - put nfsd on owp's nd_coalesce list
 * NB: Must be called at splsoftclock().
 */
static void
nfsrvw_coalesce(struct nfsrv_descript *owp, struct nfsrv_descript *nfsd)
{
        int overlap;
        struct mbuf *mp;
	struct nfsrv_descript *p;

	NFS_DPF(WG, ("C%03x-%03x",
		     nfsd->nd_retxid & 0xfff, owp->nd_retxid & 0xfff));
        LIST_REMOVE(nfsd, nd_hash);
        LIST_REMOVE(nfsd, nd_tq);
        if (owp->nd_eoff < nfsd->nd_eoff) {
            overlap = owp->nd_eoff - nfsd->nd_off;
            if (overlap < 0)
                panic("nfsrv_coalesce: bad off");
            if (overlap > 0)
                m_adj(nfsd->nd_mrep, overlap);
            mp = owp->nd_mrep;
            while (mp->m_next)
                mp = mp->m_next;
            mp->m_next = nfsd->nd_mrep;
            owp->nd_eoff = nfsd->nd_eoff;
        } else
            m_freem(nfsd->nd_mrep);
        nfsd->nd_mrep = NULL;
        if (nfsd->nd_stable == NFSV3WRITE_FILESYNC)
            owp->nd_stable = NFSV3WRITE_FILESYNC;
        else if (nfsd->nd_stable == NFSV3WRITE_DATASYNC &&
            owp->nd_stable == NFSV3WRITE_UNSTABLE)
            owp->nd_stable = NFSV3WRITE_DATASYNC;
        LIST_INSERT_HEAD(&owp->nd_coalesce, nfsd, nd_tq);

	/*
	 * If nfsd had anything else coalesced into it, transfer them
	 * to owp, otherwise their replies will never get sent.
	 */
	for (p = nfsd->nd_coalesce.lh_first; p;
	     p = nfsd->nd_coalesce.lh_first) {
	    LIST_REMOVE(p, nd_tq);
	    LIST_INSERT_HEAD(&owp->nd_coalesce, p, nd_tq);
	}
}

/*
 * nfs create service
 * now does a truncate to 0 length via. setattr if it already exists
 */
int
nfsrv_create(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	struct nfsv2_sattr *sp;
	u_int32_t *tl;
	struct nlookupdata nd;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdev, cache, len, tsize, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3), how, exclusive_flag = 0;
	caddr_t cp;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *dirp;
	struct vnode *dvp;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev, tempsize;
	u_char cverf[NFSX_V3CREATEVERF];

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	rdev = 0;
#endif
	nlookup_zero(&nd);
	dirp = NULL;
	dvp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	/*
	 * Call namei and do initial cleanup to get a few things
	 * out of the way.  If we get an initial error we cleanup
	 * and return here to avoid special-casing the invalid nd
	 * structure through the rest of the case.  dirp may be
	 * set even if an error occurs, but the nd structure will not
	 * be valid at all if an error occurs so we have to invalidate it
	 * prior to calling nfsm_reply ( which might goto nfsmout ).
	 */
	error = nfs_namei(&nd, cred, NAMEI_CREATE, &dvp, &vp,
			  fhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3) {
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
		} else {
			vrele(dirp);
			dirp = NULL;
		}
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}

	/*
	 * No error.  Continue.  State:
	 *
	 *	dirp 		may be valid
	 *	vp		may be valid or NULL if the target does not
	 *			exist.
	 *	dvp		is valid
	 *
	 * The error state is set through the code and we may also do some
	 * opportunistic releasing of vnodes to avoid holding locks through
	 * NFS I/O.  The cleanup at the end is a catch-all
	 */

	VATTR_NULL(vap);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		how = fxdr_unsigned(int, *tl);
		switch (how) {
		case NFSV3CREATE_GUARDED:
			if (vp) {
				error = EEXIST;
				break;
			}
			/* fall through */
		case NFSV3CREATE_UNCHECKED:
			nfsm_srvsattr(vap);
			break;
		case NFSV3CREATE_EXCLUSIVE:
			nfsm_dissect(cp, caddr_t, NFSX_V3CREATEVERF);
			bcopy(cp, cverf, NFSX_V3CREATEVERF);
			exclusive_flag = 1;
			break;
		};
		vap->va_type = VREG;
	} else {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		vap->va_type = IFTOVT(fxdr_unsigned(u_int32_t, sp->sa_mode));
		if (vap->va_type == VNON)
			vap->va_type = VREG;
		vap->va_mode = nfstov_mode(sp->sa_mode);
		switch (vap->va_type) {
		case VREG:
			tsize = fxdr_unsigned(int32_t, sp->sa_size);
			if (tsize != -1)
				vap->va_size = (u_quad_t)tsize;
			break;
		case VCHR:
		case VBLK:
		case VFIFO:
			rdev = fxdr_unsigned(long, sp->sa_size);
			break;
		default:
			break;
		};
	}

	/*
	 * Iff doesn't exist, create it
	 * otherwise just truncate to 0 length
	 *   should I set the mode too ?
	 *
	 * The only possible error we can have at this point is EEXIST. 
	 * nd.ni_vp will also be non-NULL in that case.
	 */
	if (vp == NULL) {
		if (vap->va_mode == (mode_t)VNOVAL)
			vap->va_mode = 0;
		if (vap->va_type == VREG || vap->va_type == VSOCK) {
			nqsrv_getl(dvp, ND_WRITE);
			error = VOP_NCREATE(nd.nl_ncp, &vp, nd.nl_cred, vap);
			if (error == 0) {
			    	nfsrv_object_create(vp);
				if (exclusive_flag) {
					exclusive_flag = 0;
					VATTR_NULL(vap);
					bcopy(cverf, (caddr_t)&vap->va_atime,
						NFSX_V3CREATEVERF);
					error = VOP_SETATTR(vp, vap, cred, td);
				}
			}
		} else if (
			vap->va_type == VCHR || 
			vap->va_type == VBLK ||
			vap->va_type == VFIFO
		) {
			/*
			 * Handle SysV FIFO node special cases.  All other
			 * devices require super user to access.
			 */
			if (vap->va_type == VCHR && rdev == 0xffffffff)
				vap->va_type = VFIFO;
                        if (vap->va_type != VFIFO &&
                            (error = suser_cred(cred, 0))) {
				goto nfsmreply0;
                        }
			vap->va_rdev = rdev;
			nqsrv_getl(dvp, ND_WRITE);

			error = VOP_NMKNOD(nd.nl_ncp, &vp, nd.nl_cred, vap);
			if (error)
				goto nfsmreply0;
#if 0
			/*
			 * XXX what is this junk supposed to do ?
			 */

			vput(vp);
			vp = NULL;

			/*
			 * release dvp prior to lookup
			 */
			vput(dvp);
			dvp = NULL;

			/*
			 * Setup for lookup. 
			 *
			 * Even though LOCKPARENT was cleared, ni_dvp may
			 * be garbage. 
			 */
			nd.ni_cnd.cn_nameiop = NAMEI_LOOKUP;
			nd.ni_cnd.cn_flags &= ~(CNP_LOCKPARENT);
			nd.ni_cnd.cn_td = td;
			nd.ni_cnd.cn_cred = cred;

			error = lookup(&nd);
			nd.ni_dvp = NULL;

			if (error != 0) {
				nfsm_reply(0);
				/* fall through on certain errors */
			}
			nfsrv_object_create(nd.ni_vp);
			if (nd.ni_cnd.cn_flags & CNP_ISSYMLINK) {
				error = EINVAL;
				goto nfsmreply0;
			}
#endif
		} else {
			error = ENXIO;
		}
	} else {
		if (vap->va_size != -1) {
			error = nfsrv_access(vp, VWRITE, cred,
			    (nd.nl_flags & NLC_NFS_RDONLY), td, 0);
			if (!error) {
				nqsrv_getl(vp, ND_WRITE);
				tempsize = vap->va_size;
				VATTR_NULL(vap);
				vap->va_size = tempsize;
				error = VOP_SETATTR(vp, vap, cred, td);
			}
		}
	}

	if (!error) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, vap, td);
	}
	if (v3) {
		if (exclusive_flag && !error &&
			bcmp(cverf, (caddr_t)&vap->va_atime, NFSX_V3CREATEVERF))
			error = EEXIST;
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
		vrele(dirp);
		dirp = NULL;
	}
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_FATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	} else {
		nfsm_srvfhtom(fhp, v3);
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	goto nfsmout;

nfsmreply0:
	nfsm_reply(0);
	error = 0;
	/* fall through */

nfsmout:
	if (dirp)
		vrele(dirp);
	nlookup_done(&nd);
	if (dvp) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
	}
	if (vp)
		vput(vp);
	return (error);
}

/*
 * nfs v3 mknod service
 */
int
nfsrv_mknod(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	u_int32_t *tl;
	struct nlookupdata nd;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache, len, dirfor_ret = 1, diraft_ret = 1;
	u_int32_t major, minor;
	enum vtype vtyp;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *dirp;
	struct vnode *dvp;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = NULL;
	dvp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	/*
	 * Handle nfs_namei() call.  If an error occurs, the nd structure
	 * is not valid.  However, nfsm_*() routines may still jump to
	 * nfsmout.
	 */

	error = nfs_namei(&nd, cred, NAMEI_CREATE, &dvp, &vp,
			  fhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp)
		dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	if (error) {
		nfsm_reply(NFSX_WCCDATA(1));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
	nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
	vtyp = nfsv3tov_type(*tl);
	if (vtyp != VCHR && vtyp != VBLK && vtyp != VSOCK && vtyp != VFIFO) {
		error = NFSERR_BADTYPE;
		goto out;
	}
	VATTR_NULL(vap);
	nfsm_srvsattr(vap);
	if (vtyp == VCHR || vtyp == VBLK) {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		major = fxdr_unsigned(u_int32_t, *tl++);
		minor = fxdr_unsigned(u_int32_t, *tl);
		vap->va_rdev = makeudev(major, minor);
	}

	/*
	 * Iff doesn't exist, create it.
	 */
	if (vp) {
		error = EEXIST;
		goto out;
	}
	vap->va_type = vtyp;
	if (vap->va_mode == (mode_t)VNOVAL)
		vap->va_mode = 0;
	if (vtyp == VSOCK) {
		nqsrv_getl(dvp, ND_WRITE);
		error = VOP_NCREATE(nd.nl_ncp, &vp, nd.nl_cred, vap);
	} else {
		if (vtyp != VFIFO && (error = suser_cred(cred, 0)))
			goto out;
		nqsrv_getl(dvp, ND_WRITE);

		error = VOP_NMKNOD(nd.nl_ncp, &vp, nd.nl_cred, vap);
		if (error)
			goto out;

#if 0
		vput(vp);
		vp = NULL;

		/*
		 * Release dvp prior to lookup
		 */
		vput(dvp);
		dvp = NULL;

		/*
		 * XXX what is this stuff for?
		 */
		KKASSERT(td->td_proc);
		nd.ni_cnd.cn_nameiop = NAMEI_LOOKUP;
		nd.ni_cnd.cn_flags &= ~(CNP_LOCKPARENT);
		nd.ni_cnd.cn_td = td;
		nd.ni_cnd.cn_cred = td->td_proc->p_ucred;

		error = lookup(&nd);
		nd.ni_dvp = NULL;

		if (error)
			goto out;
		if (nd.ni_cnd.cn_flags & CNP_ISSYMLINK)
			error = EINVAL;
#endif
	}

	/*
	 * send response, cleanup, return.
	 */
out:
	nlookup_done(&nd);
	if (dvp) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
		dvp = NULL;
	}
	if (!error) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, vap, td);
	}
	if (vp) {
		vput(vp);
		vp = NULL;
	}
	diraft_ret = VOP_GETATTR(dirp, &diraft, td);
	if (dirp) {
		vrele(dirp);
		dirp = NULL;
	}
	nfsm_reply(NFSX_SRVFH(1) + NFSX_POSTOPATTR(1) + NFSX_WCCDATA(1));
	if (!error) {
		nfsm_srvpostop_fh(fhp);
		nfsm_srvpostop_attr(0, vap);
	}
	nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	return (0);
nfsmout:
	if (dirp)
		vrele(dirp);
	nlookup_done(&nd);
	if (dvp) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
	}
	if (vp)
		vput(vp);
	return (error);
}

/*
 * nfs remove service
 */
int
nfsrv_remove(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nlookupdata nd;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *dirp;
	struct vnode *dvp;
	struct vnode *vp;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = NULL;
	dvp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	error = nfs_namei(&nd, cred, NAMEI_DELETE, &dvp, &vp,
			  fhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	}
	if (error == 0) {
		if (vp->v_type == VDIR) {
			error = EPERM;		/* POSIX */
			goto out;
		}
		/*
		 * The root of a mounted filesystem cannot be deleted.
		 */
		if (vp->v_flag & VROOT) {
			error = EBUSY;
			goto out;
		}
out:
		if (!error) {
			nqsrv_getl(dvp, ND_WRITE);
			nqsrv_getl(vp, ND_WRITE);
			error = VOP_NREMOVE(nd.nl_ncp, nd.nl_cred);
		}
	}
	if (dirp && v3)
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
nfsmout:
	nlookup_done(&nd);
	if (dirp)
		vrele(dirp);
	if (dvp) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
	}
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs rename service
 */
int
nfsrv_rename(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache = 0, len, len2, fdirfor_ret = 1, fdiraft_ret = 1;
	int tdirfor_ret = 1, tdiraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq;
	struct nlookupdata fromnd, tond;
	struct vnode *fvp, *fdirp;
	struct vnode *tvp, *tdirp;
	struct namecache *ncp;
	struct vattr fdirfor, fdiraft, tdirfor, tdiraft;
	nfsfh_t fnfh, tnfh;
	fhandle_t *ffhp, *tfhp;
	u_quad_t frev;
	uid_t saved_uid;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	fvp = (struct vnode *)0;
#endif
	ffhp = &fnfh.fh_generic;
	tfhp = &tnfh.fh_generic;

	/*
	 * Clear fields incase goto nfsmout occurs from macro.
	 */

	nlookup_zero(&fromnd);
	nlookup_zero(&tond);
	fdirp = NULL;
	tdirp = NULL;

	nfsm_srvmtofh(ffhp);
	nfsm_srvnamesiz(len);
	/*
	 * Remember our original uid so that we can reset cr_uid before
	 * the second nfs_namei() call, in case it is remapped.
	 */
	saved_uid = cred->cr_uid;
	error = nfs_namei(&fromnd, cred, NAMEI_DELETE, NULL, NULL,
			  ffhp, len, slp, nam, &md, &dpos, &fdirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (fdirp) {
		if (v3)
			fdirfor_ret = VOP_GETATTR(fdirp, &fdirfor, td);
	}
	if (error) {
		nfsm_reply(2 * NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
		error = 0;
		goto nfsmout;
	}

	/*
	 * We have to unlock the from ncp before we can safely lookup
	 * the target ncp.
	 */
	KKASSERT(fromnd.nl_flags & NLC_NCPISLOCKED);
	cache_unlock(fromnd.nl_ncp);
	fromnd.nl_flags &= ~NLC_NCPISLOCKED;
	nfsm_srvmtofh(tfhp);
	nfsm_strsiz(len2, NFS_MAXNAMLEN);
	cred->cr_uid = saved_uid;

	error = nfs_namei(&tond, cred, NAMEI_RENAME, NULL, NULL,
			  tfhp, len2, slp, nam, &md, &dpos, &tdirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (tdirp) {
		if (v3)
			tdirfor_ret = VOP_GETATTR(tdirp, &tdirfor, td);
	}
	if (error)
		goto out1;

	/*
	 * relock the source
	 */
	if (cache_lock_nonblock(fromnd.nl_ncp) == 0) {
		cache_resolve(fromnd.nl_ncp, fromnd.nl_cred);
	} else if (fromnd.nl_ncp > tond.nl_ncp) {
		cache_lock(fromnd.nl_ncp);
		cache_resolve(fromnd.nl_ncp, fromnd.nl_cred);
	} else {
		cache_unlock(tond.nl_ncp);
		cache_lock(fromnd.nl_ncp);
		cache_resolve(fromnd.nl_ncp, fromnd.nl_cred);
		cache_lock(tond.nl_ncp);
		cache_resolve(tond.nl_ncp, tond.nl_cred);
	}
	fromnd.nl_flags |= NLC_NCPISLOCKED;

	tvp = tond.nl_ncp->nc_vp;
	fvp = fromnd.nl_ncp->nc_vp;

	if (tvp != NULL) {
		if (fvp->v_type == VDIR && tvp->v_type != VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = EISDIR;
			goto out;
		} else if (fvp->v_type != VDIR && tvp->v_type == VDIR) {
			if (v3)
				error = EEXIST;
			else
				error = ENOTDIR;
			goto out;
		}
		if (tvp->v_type == VDIR && tvp->v_mountedhere) {
			if (v3)
				error = EXDEV;
			else
				error = ENOTEMPTY;
			goto out;
		}
	}
	if (fvp->v_type == VDIR && fvp->v_mountedhere) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fromnd.nl_ncp->nc_mount != tond.nl_ncp->nc_mount) {
		if (v3)
			error = EXDEV;
		else
			error = ENOTEMPTY;
		goto out;
	}
	if (fromnd.nl_ncp == tond.nl_ncp->nc_parent) {
		if (v3)
			error = EINVAL;
		else
			error = ENOTEMPTY;
	}

	/*
	 * You cannot rename a source into itself or a subdirectory of itself.
	 * We check this by travsering the target directory upwards looking
	 * for a match against the source.
	 */
	if (error == 0) {
		for (ncp = tond.nl_ncp; ncp; ncp = ncp->nc_parent) {
			if (fromnd.nl_ncp == ncp) {
				error = EINVAL;
				break;
			}
		}
	}

	/*
	 * If source is the same as the destination (that is the
	 * same vnode with the same name in the same directory),
	 * then there is nothing to do.
	 */
	if (fromnd.nl_ncp == tond.nl_ncp)
		error = -1;
out:
	if (!error) {
		/*
		 * The VOP_NRENAME function releases all vnode references &
		 * locks prior to returning so we need to clear the pointers
		 * to bypass cleanup code later on.
		 */
#if 0
		/* XXX should be handled by NRENAME */
		nqsrv_getl(fdvp, ND_WRITE);
		nqsrv_getl(tdvp, ND_WRITE);
		if (tvp) {
			nqsrv_getl(tvp, ND_WRITE);
		}
#endif
		error = VOP_NRENAME(fromnd.nl_ncp, tond.nl_ncp, tond.nl_cred);
	} else {
		if (error == -1)
			error = 0;
	}
	/* fall through */

out1:
	if (fdirp)
		fdiraft_ret = VOP_GETATTR(fdirp, &fdiraft, td);
	if (tdirp)
		tdiraft_ret = VOP_GETATTR(tdirp, &tdiraft, td);
	nfsm_reply(2 * NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(fdirfor_ret, &fdirfor, fdiraft_ret, &fdiraft);
		nfsm_srvwcc_data(tdirfor_ret, &tdirfor, tdiraft_ret, &tdiraft);
	}
	error = 0;
	/* fall through */

nfsmout:
	if (tdirp)
		vrele(tdirp);
	nlookup_done(&tond);
	if (fdirp)
		vrele(fdirp);
	nlookup_done(&fromnd);
	return (error);
}

/*
 * nfs link service
 */
int
nfsrv_link(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	   struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nlookupdata nd;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int getret = 1, v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *dirp;
	struct vnode *dvp;
	struct vnode *vp;
	struct vnode *xp;
	struct vattr dirfor, diraft, at;
	nfsfh_t nfh, dnfh;
	fhandle_t *fhp, *dfhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = dvp = vp = xp = NULL;

	fhp = &nfh.fh_generic;
	dfhp = &dnfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvmtofh(dfhp);
	nfsm_srvnamesiz(len);

	error = nfsrv_fhtovp(fhp, FALSE, &xp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
		nfsm_srvpostop_attr(getret, &at);
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		xp = NULL;
		error = 0;
		goto nfsmout;
	}
	if (xp->v_type == VDIR) {
		error = EPERM;		/* POSIX */
		goto out1;
	}

	error = nfs_namei(&nd, cred, NAMEI_CREATE, &dvp, &vp,
			  dfhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	}
	if (error)
		goto out1;

	if (vp != NULL) {
		error = EEXIST;
		goto out;
	}
	if (xp->v_mount != dvp->v_mount)
		error = EXDEV;
out:
	if (!error) {
#if 0
		nqsrv_getl(xp, ND_WRITE);
		nqsrv_getl(vp, ND_WRITE);
#endif
		error = VOP_NLINK(nd.nl_ncp, xp, nd.nl_cred);
	}
	/* fall through */

out1:
	if (v3)
		getret = VOP_GETATTR(xp, &at, td);
	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
	/* fall through */

nfsmout:
	nlookup_done(&nd);
	if (dirp)
		vrele(dirp);
	if (xp)
		vrele(xp);
	if (dvp) {
		if (dvp == vp)
			vrele(dvp);
		else
			vput(dvp);
	}
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs symbolic link service
 */
int
nfsrv_symlink(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	      struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct nlookupdata nd;
	struct vattr *vap = &va;
	u_int32_t *tl;
	int32_t t1;
	struct nfsv2_sattr *sp;
	char *bpos, *pathcp = (char *)0, *cp2;
	struct uio io;
	struct iovec iv;
	int error = 0, cache = 0, len, len2, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	struct mbuf *mb, *mreq, *mb2;
	struct vnode *dirp;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	error = nfs_namei(&nd, cred, NAMEI_CREATE, NULL, &vp,
			fhp, len, slp, nam, &md, &dpos, &dirp,
			td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	}
	if (error)
		goto out;

	VATTR_NULL(vap);
	if (v3)
		nfsm_srvsattr(vap);
	nfsm_strsiz(len2, NFS_MAXPATHLEN);
	MALLOC(pathcp, caddr_t, len2 + 1, M_TEMP, M_WAITOK);
	iv.iov_base = pathcp;
	iv.iov_len = len2;
	io.uio_resid = len2;
	io.uio_offset = 0;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_td = NULL;
	nfsm_mtouio(&io, len2);
	if (!v3) {
		nfsm_dissect(sp, struct nfsv2_sattr *, NFSX_V2SATTR);
		vap->va_mode = nfstov_mode(sp->sa_mode);
	}
	*(pathcp + len2) = '\0';
	if (vp) {
		error = EEXIST;
		goto out;
	}

	if (vap->va_mode == (mode_t)VNOVAL)
		vap->va_mode = 0;
#if 0
	nqsrv_getl(dvp, ND_WRITE);
#endif
	error = VOP_NSYMLINK(nd.nl_ncp, &vp, nd.nl_cred, vap, pathcp);
	if (error == 0) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (!error)
			error = VOP_GETATTR(vp, vap, td);
	}

#if 0
	/*
	 * We have a vp in hand from the new API call, we do not have to
	 * look it up again.
	 */
	if (error == 0) {
	    if (v3) {
		/*
		 * Issue lookup.  Leave SAVESTART set so we can easily free
		 * the name buffer later on.
		 *
		 * since LOCKPARENT is not set, ni_dvp will be garbage on
		 * return whether an error occurs or not.
		 */
		nd.ni_cnd.cn_nameiop = NAMEI_LOOKUP;
		nd.ni_cnd.cn_flags &= ~(CNP_LOCKPARENT | CNP_FOLLOW);
		nd.ni_cnd.cn_td = td;
		nd.ni_cnd.cn_cred = cred;

		error = lookup(&nd);
		nd.ni_dvp = NULL;

		if (error == 0) {
			bzero((caddr_t)fhp, sizeof(nfh));
			fhp->fh_fsid = nd.ni_vp->v_mount->mnt_stat.f_fsid;
			error = VFS_VPTOFH(nd.ni_vp, &fhp->fh_fid);
			if (!error)
				error = VOP_GETATTR(nd.ni_vp, vap, td);
			vput(nd.ni_vp);
			nd.ni_vp = NULL;
		}
	    }
	}
#endif
out:
	if (vp) {
		vput(vp);
		vp = NULL;
	}
	if (pathcp) {
		FREE(pathcp, M_TEMP);
		pathcp = NULL;
	}
	if (dirp) {
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
		vrele(dirp);
		dirp = NULL;
	}
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	}
	error = 0;
	/* fall through */

nfsmout:
	nlookup_done(&nd);
	if (vp)
		vput(vp);
	if (dirp)
		vrele(dirp);
	if (pathcp)
		FREE(pathcp, M_TEMP);
	return (error);
}

/*
 * nfs mkdir service
 */
int
nfsrv_mkdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr va, dirfor, diraft;
	struct vattr *vap = &va;
	struct nfs_fattr *fp;
	struct nlookupdata nd;
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *dirp;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	error = nfs_namei(&nd, cred, NAMEI_CREATE, NULL, &vp,
			  fhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
	VATTR_NULL(vap);
	if (v3) {
		nfsm_srvsattr(vap);
	} else {
		nfsm_dissect(tl, u_int32_t *, NFSX_UNSIGNED);
		vap->va_mode = nfstov_mode(*tl++);
	}

	/*
	 * At this point nd.ni_dvp is referenced and exclusively locked and
	 * nd.ni_vp, if it exists, is referenced but not locked.
	 */

	vap->va_type = VDIR;
	if (vp != NULL) {
		error = EEXIST;
		goto out;
	}

	/*
	 * Issue mkdir op.  Since SAVESTART is not set, the pathname 
	 * component is freed by the VOP call.  This will fill-in
	 * nd.ni_vp, reference, and exclusively lock it.
	 */
	if (vap->va_mode == (mode_t)VNOVAL)
		vap->va_mode = 0;
#if 0
	nqsrv_getl(nd.ni_dvp, ND_WRITE);
#endif
	error = VOP_NMKDIR(nd.nl_ncp, &vp, nd.nl_cred, vap);

	if (error == 0) {
		bzero((caddr_t)fhp, sizeof(nfh));
		fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fhp->fh_fid);
		if (error == 0)
			error = VOP_GETATTR(vp, vap, td);
	}
out:
	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
	nfsm_reply(NFSX_SRVFH(v3) + NFSX_POSTOPATTR(v3) + NFSX_WCCDATA(v3));
	if (v3) {
		if (!error) {
			nfsm_srvpostop_fh(fhp);
			nfsm_srvpostop_attr(0, vap);
		}
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
	} else {
		nfsm_srvfhtom(fhp, v3);
		nfsm_build(fp, struct nfs_fattr *, NFSX_V2FATTR);
		nfsm_srvfillattr(vap, fp);
	}
	error = 0;
	/* fall through */

nfsmout:
	nlookup_done(&nd);
	if (dirp)
		vrele(dirp);
	if (vp)
		vput(vp);
	return (error);
}

/*
 * nfs rmdir service
 */
int
nfsrv_rmdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	    struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, cache = 0, len, dirfor_ret = 1, diraft_ret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mreq;
	struct vnode *dirp;
	struct vnode *vp;
	struct vattr dirfor, diraft;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct nlookupdata nd;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	nlookup_zero(&nd);
	dirp = NULL;
	vp = NULL;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_srvnamesiz(len);

	error = nfs_namei(&nd, cred, NAMEI_DELETE, NULL, &vp,
			  fhp, len, slp, nam, &md, &dpos, &dirp,
			  td, (nfsd->nd_flag & ND_KERBAUTH), FALSE);
	if (dirp) {
		if (v3)
			dirfor_ret = VOP_GETATTR(dirp, &dirfor, td);
	}
	if (error) {
		nfsm_reply(NFSX_WCCDATA(v3));
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
		goto nfsmout;
	}
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	/*
	 * The root of a mounted filesystem cannot be deleted.
	 */
	if (vp->v_flag & VROOT)
		error = EBUSY;
out:
	/*
	 * Issue or abort op.  Since SAVESTART is not set, path name
	 * component is freed by the VOP after either.
	 */
	if (!error) {
#if 0
		nqsrv_getl(nd.ni_dvp, ND_WRITE);
		nqsrv_getl(vp, ND_WRITE);
#endif
		vput(vp);
		vp = NULL;
		error = VOP_NRMDIR(nd.nl_ncp, nd.nl_cred);
	}
	nlookup_done(&nd);

	if (dirp)
		diraft_ret = VOP_GETATTR(dirp, &diraft, td);
	nfsm_reply(NFSX_WCCDATA(v3));
	if (v3) {
		nfsm_srvwcc_data(dirfor_ret, &dirfor, diraft_ret, &diraft);
		error = 0;
	}
	/* fall through */

nfsmout:
	nlookup_done(&nd);
	if (dirp)
		vrele(dirp);
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs readdir service
 * - mallocs what it thinks is enough to read
 *	count rounded up to a multiple of NFS_DIRBLKSIZ <= NFS_MAXREADDIR
 * - calls VOP_READDIR()
 * - loops around building the reply
 *	if the output generated exceeds count break out of loop
 *	The nfsm_clget macro is used here so that the reply will be packed
 *	tightly in mbuf clusters.
 * - it only knows that it has encountered eof when the VOP_READDIR()
 *	reads nothing
 * - as such one readdir rpc will return eof false although you are there
 *	and then the next will return eof
 * - it trims out records with d_fileno == 0
 *	this doesn't matter for Unix clients, but they might confuse clients
 *	for other os'.
 * NB: It is tempting to set eof to true if the VOP_READDIR() reads less
 *	than requested, but this may not apply to all filesystems. For
 *	example, client NFS does not { although it is never remote mounted
 *	anyhow }
 *     The alternate call nfsrv_readdirplus() does lookups as well.
 * PS: The NFS protocol spec. does not clarify what the "count" byte
 *	argument is a count of.. just name strings and file id's or the
 *	entire reply rpc or ...
 *	I tried just file name and id sizes and it confused the Sun client,
 *	so I am using the full rpc size now. The "paranoia.." comment refers
 *	to including the status longwords that are not a part of the dir.
 *	"entry" structures, but are in the rpc.
 */
struct flrep {
	nfsuint64	fl_off;
	u_int32_t	fl_postopok;
	u_int32_t	fl_fattr[NFSX_V3FATTR / sizeof (u_int32_t)];
	u_int32_t	fl_fhok;
	u_int32_t	fl_fhsize;
	u_int32_t	fl_nfh[NFSX_V3FH / sizeof (u_int32_t)];
};

int
nfsrv_readdir(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	      struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	struct mbuf *mb, *mb2, *mreq, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct uio io;
	struct iovec iv;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, cache, ncookies;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	u_quad_t frev, off, toff, verf;
	u_long *cookies = NULL, *cookiep; /* needs to be int64_t or off_t */

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	if (v3) {
		nfsm_dissect(tl, u_int32_t *, 5 * NFSX_UNSIGNED);
		toff = fxdr_hyper(tl);
		tl += 2;
		verf = fxdr_hyper(tl);
		tl += 2;
	} else {
		nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		toff = fxdr_unsigned(u_quad_t, *tl++);
		verf = 0;	/* shut up gcc */
	}
	off = toff;
	cnt = fxdr_unsigned(int, *tl);
	siz = ((cnt + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (cnt > xfer)
		cnt = xfer;
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
		vp = NULL;
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}

	/*
	 * Obtain lock on vnode for this section of the code
	 */

	nqsrv_getl(vp, ND_READ);
	if (v3) {
		error = getret = VOP_GETATTR(vp, &at, td);
#if 0
		/*
		 * XXX This check may be too strict for Solaris 2.5 clients.
		 */
		if (!error && toff && verf && verf != at.va_filerev)
			error = NFSERR_BAD_COOKIE;
#endif
	}
	if (!error)
		error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 0);
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, td);

	/*
	 * end section.  Allocate rbuf and continue
	 */
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_td = NULL;
	eofflag = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}
	error = VOP_READDIR(vp, &io, cred, &eofflag, &ncookies, &cookies);
	off = (off_t)io.uio_offset;
	if (!cookies && !error)
		error = NFSERR_PERM;
	if (v3) {
		getret = VOP_GETATTR(vp, &at, td);
		if (!error)
			error = getret;
	}
	VOP_UNLOCK(vp, 0, td);
	if (error) {
		vrele(vp);
		vp = NULL;
		free((caddr_t)rbuf, M_TEMP);
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		nfsm_reply(NFSX_POSTOPATTR(v3));
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			vp = NULL;
			nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) +
				2 * NFSX_UNSIGNED);
			if (v3) {
				nfsm_srvpostop_attr(getret, &at);
				nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
				txdr_hyper(at.va_filerev, tl);
				tl += 2;
			} else
				nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)rbuf, M_TEMP);
			FREE((caddr_t)cookies, M_TEMP);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;
	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that preceed the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 */
	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT ||
		 ((u_quad_t)(*cookiep)) <= toff)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	len = 3 * NFSX_UNSIGNED;	/* paranoia, probably can be 0 */
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_COOKIEVERF(v3) + siz);
	if (v3) {
		nfsm_srvpostop_attr(getret, &at);
		nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
		txdr_hyper(at.va_filerev, tl);
	}
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen) - nlen;
			len += (4 * NFSX_UNSIGNED + nlen + rem);
			if (v3)
				len += 2 * NFSX_UNSIGNED;
			if (len > cnt) {
				eofflag = 0;
				break;
			}
			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			if (v3) {
				nfsm_clget;
				*tl = 0;
				bp += NFSX_UNSIGNED;
			}
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;

			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp+xfer) > be)
					tsiz = be-bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to a int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
			nfsm_clget;

			/* Finish off the record */
			if (v3) {
				*tl = 0;
				bp += NFSX_UNSIGNED;
				nfsm_clget;
			}
			*tl = txdr_unsigned(*cookiep);
			bp += NFSX_UNSIGNED;
		}
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	vp = NULL;
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, caddr_t);
	} else
		mp->m_len += bp - bpos;
	FREE((caddr_t)rbuf, M_TEMP);
	FREE((caddr_t)cookies, M_TEMP);

nfsmout:
	if (vp)
		vrele(vp);
	return(error);
}

int
nfsrv_readdirplus(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
		  struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	char *bp, *be;
	struct mbuf *mp;
	struct dirent *dp;
	caddr_t cp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	struct mbuf *mb, *mb2, *mreq, *mp2;
	char *cpos, *cend, *cp2, *rbuf;
	struct vnode *vp = NULL, *nvp;
	struct flrep fl;
	nfsfh_t nfh;
	fhandle_t *fhp, *nfhp = (fhandle_t *)fl.fl_nfh;
	struct uio io;
	struct iovec iv;
	struct vattr va, at, *vap = &va;
	struct nfs_fattr *fp;
	int len, nlen, rem, xfer, tsiz, i, error = 0, getret = 1;
	int siz, cnt, fullsiz, eofflag, rdonly, cache, dirlen, ncookies;
	u_quad_t frev, off, toff, verf;
	u_long *cookies = NULL, *cookiep; /* needs to be int64_t or off_t */

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, 6 * NFSX_UNSIGNED);
	toff = fxdr_hyper(tl);
	tl += 2;
	verf = fxdr_hyper(tl);
	tl += 2;
	siz = fxdr_unsigned(int, *tl++);
	cnt = fxdr_unsigned(int, *tl);
	off = toff;
	siz = ((siz + DIRBLKSIZ - 1) & ~(DIRBLKSIZ - 1));
	xfer = NFS_SRVMAXDATA(nfsd);
	if (cnt > xfer)
		cnt = xfer;
	if (siz > xfer)
		siz = xfer;
	fullsiz = siz;
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (!error && vp->v_type != VDIR) {
		error = ENOTDIR;
		vput(vp);
		vp = NULL;
	}
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	error = getret = VOP_GETATTR(vp, &at, td);
#if 0
	/*
	 * XXX This check may be too strict for Solaris 2.5 clients.
	 */
	if (!error && toff && verf && verf != at.va_filerev)
		error = NFSERR_BAD_COOKIE;
#endif
	if (!error) {
		nqsrv_getl(vp, ND_READ);
		error = nfsrv_access(vp, VEXEC, cred, rdonly, td, 0);
	}
	if (error) {
		vput(vp);
		vp = NULL;
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	VOP_UNLOCK(vp, 0, td);
	MALLOC(rbuf, caddr_t, siz, M_TEMP, M_WAITOK);
again:
	iv.iov_base = rbuf;
	iv.iov_len = fullsiz;
	io.uio_iov = &iv;
	io.uio_iovcnt = 1;
	io.uio_offset = (off_t)off;
	io.uio_resid = fullsiz;
	io.uio_segflg = UIO_SYSSPACE;
	io.uio_rw = UIO_READ;
	io.uio_td = NULL;
	eofflag = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	if (cookies) {
		free((caddr_t)cookies, M_TEMP);
		cookies = NULL;
	}
	error = VOP_READDIR(vp, &io, cred, &eofflag, &ncookies, &cookies);
	off = (u_quad_t)io.uio_offset;
	getret = VOP_GETATTR(vp, &at, td);
	VOP_UNLOCK(vp, 0, td);
	if (!cookies && !error)
		error = NFSERR_PERM;
	if (!error)
		error = getret;
	if (error) {
		vrele(vp);
		vp = NULL;
		if (cookies)
			free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	if (io.uio_resid) {
		siz -= io.uio_resid;

		/*
		 * If nothing read, return eof
		 * rpc reply
		 */
		if (siz == 0) {
			vrele(vp);
			vp = NULL;
			nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF +
				2 * NFSX_UNSIGNED);
			nfsm_srvpostop_attr(getret, &at);
			nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
			txdr_hyper(at.va_filerev, tl);
			tl += 2;
			*tl++ = nfs_false;
			*tl = nfs_true;
			FREE((caddr_t)cookies, M_TEMP);
			FREE((caddr_t)rbuf, M_TEMP);
			error = 0;
			goto nfsmout;
		}
	}

	/*
	 * Check for degenerate cases of nothing useful read.
	 * If so go try again
	 */
	cpos = rbuf;
	cend = rbuf + siz;
	dp = (struct dirent *)cpos;
	cookiep = cookies;
	/*
	 * For some reason FreeBSD's ufs_readdir() chooses to back the
	 * directory offset up to a block boundary, so it is necessary to
	 * skip over the records that preceed the requested offset. This
	 * requires the assumption that file offset cookies monotonically
	 * increase.
	 */
	while (cpos < cend && ncookies > 0 &&
		(dp->d_fileno == 0 || dp->d_type == DT_WHT ||
		 ((u_quad_t)(*cookiep)) <= toff)) {
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	if (cpos >= cend || ncookies == 0) {
		toff = off;
		siz = fullsiz;
		goto again;
	}

	/*
	 * Probe one of the directory entries to see if the filesystem
	 * supports VGET.
	 */
	if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp) == EOPNOTSUPP) {
		error = NFSERR_NOTSUPP;
		vrele(vp);
		vp = NULL;
		free((caddr_t)cookies, M_TEMP);
		free((caddr_t)rbuf, M_TEMP);
		nfsm_reply(NFSX_V3POSTOPATTR);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	if (nvp) {
		vput(nvp);
		nvp = NULL;
	}
	    
	dirlen = len = NFSX_V3POSTOPATTR + NFSX_V3COOKIEVERF + 2 * NFSX_UNSIGNED;
	nfsm_reply(cnt);
	nfsm_srvpostop_attr(getret, &at);
	nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	txdr_hyper(at.va_filerev, tl);
	mp = mp2 = mb;
	bp = bpos;
	be = bp + M_TRAILINGSPACE(mp);

	/* Loop through the records and build reply */
	while (cpos < cend && ncookies > 0) {
		if (dp->d_fileno != 0 && dp->d_type != DT_WHT) {
			nlen = dp->d_namlen;
			rem = nfsm_rndup(nlen)-nlen;

			/*
			 * For readdir_and_lookup get the vnode using
			 * the file number.
			 */
			if (VFS_VGET(vp->v_mount, dp->d_fileno, &nvp))
				goto invalid;
			bzero((caddr_t)nfhp, NFSX_V3FH);
			nfhp->fh_fsid =
				nvp->v_mount->mnt_stat.f_fsid;
			if (VFS_VPTOFH(nvp, &nfhp->fh_fid)) {
				vput(nvp);
				nvp = NULL;
				goto invalid;
			}
			if (VOP_GETATTR(nvp, vap, td)) {
				vput(nvp);
				nvp = NULL;
				goto invalid;
			}
			vput(nvp);
			nvp = NULL;

			/*
			 * If either the dircount or maxcount will be
			 * exceeded, get out now. Both of these lengths
			 * are calculated conservatively, including all
			 * XDR overheads.
			 */
			len += (8 * NFSX_UNSIGNED + nlen + rem + NFSX_V3FH +
				NFSX_V3POSTOPATTR);
			dirlen += (6 * NFSX_UNSIGNED + nlen + rem);
			if (len > cnt || dirlen > fullsiz) {
				eofflag = 0;
				break;
			}

			/*
			 * Build the directory record xdr from
			 * the dirent entry.
			 */
			fp = (struct nfs_fattr *)&fl.fl_fattr;
			nfsm_srvfillattr(vap, fp);
			fl.fl_fhsize = txdr_unsigned(NFSX_V3FH);
			fl.fl_fhok = nfs_true;
			fl.fl_postopok = nfs_true;
			fl.fl_off.nfsuquad[0] = 0;
			fl.fl_off.nfsuquad[1] = txdr_unsigned(*cookiep);

			nfsm_clget;
			*tl = nfs_true;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = 0;
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(dp->d_fileno);
			bp += NFSX_UNSIGNED;
			nfsm_clget;
			*tl = txdr_unsigned(nlen);
			bp += NFSX_UNSIGNED;

			/* And loop around copying the name */
			xfer = nlen;
			cp = dp->d_name;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
			/* And null pad to a int32_t boundary */
			for (i = 0; i < rem; i++)
				*bp++ = '\0';
	
			/*
			 * Now copy the flrep structure out.
			 */
			xfer = sizeof (struct flrep);
			cp = (caddr_t)&fl;
			while (xfer > 0) {
				nfsm_clget;
				if ((bp + xfer) > be)
					tsiz = be - bp;
				else
					tsiz = xfer;
				bcopy(cp, bp, tsiz);
				bp += tsiz;
				xfer -= tsiz;
				if (xfer > 0)
					cp += tsiz;
			}
		}
invalid:
		cpos += dp->d_reclen;
		dp = (struct dirent *)cpos;
		cookiep++;
		ncookies--;
	}
	vrele(vp);
	vp = NULL;
	nfsm_clget;
	*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	nfsm_clget;
	if (eofflag)
		*tl = nfs_true;
	else
		*tl = nfs_false;
	bp += NFSX_UNSIGNED;
	if (mp != mb) {
		if (bp < be)
			mp->m_len = bp - mtod(mp, caddr_t);
	} else
		mp->m_len += bp - bpos;
	FREE((caddr_t)cookies, M_TEMP);
	FREE((caddr_t)rbuf, M_TEMP);
nfsmout:
	if (vp)
		vrele(vp);
	return(error);
}

/*
 * nfs commit service
 */
int
nfsrv_commit(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct vattr bfor, aft;
	struct vnode *vp = NULL;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, for_ret = 1, aft_ret = 1, cnt, cache;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	u_quad_t frev, off;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);

	/*
	 * XXX At this time VOP_FSYNC() does not accept offset and byte
	 * count parameters, so these arguments are useless (someday maybe).
	 */
	off = fxdr_hyper(tl);
	tl += 2;
	cnt = fxdr_unsigned(int, *tl);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(2 * NFSX_UNSIGNED);
		nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
		error = 0;
		goto nfsmout;
	}
	for_ret = VOP_GETATTR(vp, &bfor, td);

	if (cnt > MAX_COMMIT_COUNT) {
		/*
		 * Give up and do the whole thing
		 */
		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			vm_object_page_clean(vp->v_object, 0, 0, OBJPC_SYNC);
		}
		error = VOP_FSYNC(vp, MNT_WAIT, td);
	} else {
		/*
		 * Locate and synchronously write any buffers that fall
		 * into the requested range.  Note:  we are assuming that
		 * f_iosize is a power of 2.
		 */
		int iosize = vp->v_mount->mnt_stat.f_iosize;
		int iomask = iosize - 1;
		int s;
		daddr_t lblkno;

		/*
		 * Align to iosize boundry, super-align to page boundry.
		 */
		if (off & iomask) {
			cnt += off & iomask;
			off &= ~(u_quad_t)iomask;
		}
		if (off & PAGE_MASK) {
			cnt += off & PAGE_MASK;
			off &= ~(u_quad_t)PAGE_MASK;
		}
		lblkno = off / iosize;

		if (vp->v_object &&
		   (vp->v_object->flags & OBJ_MIGHTBEDIRTY)) {
			vm_object_page_clean(vp->v_object, off / PAGE_SIZE, (cnt + PAGE_MASK) / PAGE_SIZE, OBJPC_SYNC);
		}

		s = splbio();
		while (cnt > 0) {
			struct buf *bp;

			/*
			 * If we have a buffer and it is marked B_DELWRI we
			 * have to lock and write it.  Otherwise the prior
			 * write is assumed to have already been committed.
			 */
			if ((bp = gbincore(vp, lblkno)) != NULL && (bp->b_flags & B_DELWRI)) {
				if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT)) {
					BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL);
					continue; /* retry */
				}
				bremfree(bp);
				bp->b_flags &= ~B_ASYNC;
				VOP_BWRITE(bp->b_vp, bp);
				++nfs_commit_miss;
			}
			++nfs_commit_blks;
			if (cnt < iosize)
				break;
			cnt -= iosize;
			++lblkno;
		}
		splx(s);
	}

	aft_ret = VOP_GETATTR(vp, &aft, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_V3WCCDATA + NFSX_V3WRITEVERF);
	nfsm_srvwcc_data(for_ret, &bfor, aft_ret, &aft);
	if (!error) {
		nfsm_build(tl, u_int32_t *, NFSX_V3WRITEVERF);
		if (nfsver.tv_sec == 0)
			nfsver = boottime;
		*tl++ = txdr_unsigned(nfsver.tv_sec);
		*tl = txdr_unsigned(nfsver.tv_nsec / 1000);
	} else {
		error = 0;
	}
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs statfs service
 */
int
nfsrv_statfs(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct statfs *sf;
	struct nfs_statfs *sfp;
	u_int32_t *tl;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, getret = 1;
	int v3 = (nfsd->nd_flag & ND_NFSV3);
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct statfs statfs;
	u_quad_t frev, tval;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	sf = &statfs;
	error = VFS_STATFS(vp->v_mount, sf, td);
	getret = VOP_GETATTR(vp, &at, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_POSTOPATTR(v3) + NFSX_STATFS(v3));
	if (v3)
		nfsm_srvpostop_attr(getret, &at);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	nfsm_build(sfp, struct nfs_statfs *, NFSX_STATFS(v3));
	if (v3) {
		tval = (u_quad_t)sf->f_blocks;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_tbytes);
		tval = (u_quad_t)sf->f_bfree;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_fbytes);
		tval = (u_quad_t)sf->f_bavail;
		tval *= (u_quad_t)sf->f_bsize;
		txdr_hyper(tval, &sfp->sf_abytes);
		sfp->sf_tfiles.nfsuquad[0] = 0;
		sfp->sf_tfiles.nfsuquad[1] = txdr_unsigned(sf->f_files);
		sfp->sf_ffiles.nfsuquad[0] = 0;
		sfp->sf_ffiles.nfsuquad[1] = txdr_unsigned(sf->f_ffree);
		sfp->sf_afiles.nfsuquad[0] = 0;
		sfp->sf_afiles.nfsuquad[1] = txdr_unsigned(sf->f_ffree);
		sfp->sf_invarsec = 0;
	} else {
		sfp->sf_tsize = txdr_unsigned(NFS_MAXDGRAMDATA);
		sfp->sf_bsize = txdr_unsigned(sf->f_bsize);
		sfp->sf_blocks = txdr_unsigned(sf->f_blocks);
		sfp->sf_bfree = txdr_unsigned(sf->f_bfree);
		sfp->sf_bavail = txdr_unsigned(sf->f_bavail);
	}
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs fsinfo service
 */
int
nfsrv_fsinfo(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	     struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_fsinfo *sip;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, getret = 1, pref;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev, maxfsize;
	struct statfs sb;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}

	/* XXX Try to make a guess on the max file size. */
	VFS_STATFS(vp->v_mount, &sb, td);
	maxfsize = (u_quad_t)0x80000000 * sb.f_bsize - 1;

	getret = VOP_GETATTR(vp, &at, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3FSINFO);
	nfsm_srvpostop_attr(getret, &at);
	nfsm_build(sip, struct nfsv3_fsinfo *, NFSX_V3FSINFO);

	/*
	 * XXX
	 * There should be file system VFS OP(s) to get this information.
	 * For now, assume ufs.
	 */
	if (slp->ns_so->so_type == SOCK_DGRAM)
		pref = NFS_MAXDGRAMDATA;
	else
		pref = NFS_MAXDATA;
	sip->fs_rtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_rtpref = txdr_unsigned(pref);
	sip->fs_rtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_wtmax = txdr_unsigned(NFS_MAXDATA);
	sip->fs_wtpref = txdr_unsigned(pref);
	sip->fs_wtmult = txdr_unsigned(NFS_FABLKSIZE);
	sip->fs_dtpref = txdr_unsigned(pref);
	txdr_hyper(maxfsize, &sip->fs_maxfilesize);
	sip->fs_timedelta.nfsv3_sec = 0;
	sip->fs_timedelta.nfsv3_nsec = txdr_unsigned(1);
	sip->fs_properties = txdr_unsigned(NFSV3FSINFO_LINK |
		NFSV3FSINFO_SYMLINK | NFSV3FSINFO_HOMOGENEOUS |
		NFSV3FSINFO_CANSETTIME);
nfsmout:
	if (vp)
		vput(vp);
	return(error);
}

/*
 * nfs pathconf service
 */
int
nfsrv_pathconf(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	       struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	u_int32_t *tl;
	struct nfsv3_pathconf *pc;
	int32_t t1;
	caddr_t bpos;
	int error = 0, rdonly, cache, getret = 1;
	register_t linkmax, namemax, chownres, notrunc;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	struct vnode *vp = NULL;
	struct vattr at;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam,
		 &rdonly, (nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(NFSX_UNSIGNED);
		nfsm_srvpostop_attr(getret, &at);
		error = 0;
		goto nfsmout;
	}
	error = VOP_PATHCONF(vp, _PC_LINK_MAX, &linkmax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NAME_MAX, &namemax);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_CHOWN_RESTRICTED, &chownres);
	if (!error)
		error = VOP_PATHCONF(vp, _PC_NO_TRUNC, &notrunc);
	getret = VOP_GETATTR(vp, &at, td);
	vput(vp);
	vp = NULL;
	nfsm_reply(NFSX_V3POSTOPATTR + NFSX_V3PATHCONF);
	nfsm_srvpostop_attr(getret, &at);
	if (error) {
		error = 0;
		goto nfsmout;
	}
	nfsm_build(pc, struct nfsv3_pathconf *, NFSX_V3PATHCONF);

	pc->pc_linkmax = txdr_unsigned(linkmax);
	pc->pc_namemax = txdr_unsigned(namemax);
	pc->pc_notrunc = txdr_unsigned(notrunc);
	pc->pc_chownrestricted = txdr_unsigned(chownres);

	/*
	 * These should probably be supported by VOP_PATHCONF(), but
	 * until msdosfs is exportable (why would you want to?), the
	 * Unix defaults should be ok.
	 */
	pc->pc_caseinsensitive = nfs_false;
	pc->pc_casepreserving = nfs_true;
nfsmout:
	if (vp)	
		vput(vp);
	return(error);
}

/*
 * Null operation, used by clients to ping server
 */
/* ARGSUSED */
int
nfsrv_null(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	   struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	caddr_t bpos;
	int error = NFSERR_RETVOID, cache;
	struct mbuf *mb, *mreq;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	nfsm_reply(0);
	nfsm_srvdone;
}

/*
 * No operation, used for obsolete procedures
 */
/* ARGSUSED */
int
nfsrv_noop(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
	   struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep;
	caddr_t bpos;
	int error, cache;
	struct mbuf *mb, *mreq;
	u_quad_t frev;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
#ifndef nolint
	cache = 0;
#endif
	if (nfsd->nd_repstat)
		error = nfsd->nd_repstat;
	else
		error = EPROCUNAVAIL;
	nfsm_reply(0);
	error = 0;
	nfsm_srvdone;
}

/*
 * Perform access checking for vnodes obtained from file handles that would
 * refer to files already opened by a Unix client. You cannot just use
 * vn_writechk() and VOP_ACCESS() for two reasons.
 * 1 - You must check for exported rdonly as well as MNT_RDONLY for the write case
 * 2 - The owner is to be given access irrespective of mode bits for some
 *     operations, so that processes that chmod after opening a file don't
 *     break. I don't like this because it opens a security hole, but since
 *     the nfs server opens a security hole the size of a barn door anyhow,
 *     what the heck.
 *
 * The exception to rule 2 is EPERM. If a file is IMMUTABLE, VOP_ACCESS()
 * will return EPERM instead of EACCESS. EPERM is always an error.
 */
static int
nfsrv_access(struct vnode *vp, int flags, struct ucred *cred,
	     int rdonly, struct thread *td, int override)
{
	struct vattr vattr;
	int error;

	nfsdbprintf(("%s %d\n", __FILE__, __LINE__));
	if (flags & VWRITE) {
		/* Just vn_writechk() changed to check rdonly */
		/*
		 * Disallow write attempts on read-only file systems;
		 * unless the file is a socket or a block or character
		 * device resident on the file system.
		 */
		if (rdonly || (vp->v_mount->mnt_flag & MNT_RDONLY)) {
			switch (vp->v_type) {
			case VREG:
			case VDIR:
			case VLNK:
				return (EROFS);
			default:
				break;
			}
		}
		/*
		 * If there's shared text associated with
		 * the inode, we can't allow writing.
		 */
		if (vp->v_flag & VTEXT)
			return (ETXTBSY);
	}
	error = VOP_GETATTR(vp, &vattr, td);
	if (error)
		return (error);
	error = VOP_ACCESS(vp, flags, cred, td);
	/*
	 * Allow certain operations for the owner (reads and writes
	 * on files that are already open).
	 */
	if (override && error == EACCES && cred->cr_uid == vattr.va_uid)
		error = 0;
	return error;
}
#endif /* NFS_NOSERVER */

