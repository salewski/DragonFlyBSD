/*
 * Copyright (c) 1992, 1993
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
 *	@(#)nfs_nqlease.c	8.9 (Berkeley) 5/20/95
 * $FreeBSD: src/sys/nfs/nfs_nqlease.c,v 1.50 2000/02/13 03:32:05 peter Exp $
 * $DragonFly: src/sys/vfs/nfs/Attic/nfs_nqlease.c,v 1.23 2005/03/27 23:51:42 dillon Exp $
 */


/*
 * References:
 *	Cary G. Gray and David R. Cheriton, "Leases: An Efficient Fault-Tolerant
 *		Mechanism for Distributed File Cache Consistency",
 *		In Proc. of the Twelfth ACM Symposium on Operating Systems
 *		Principals, pg. 202-210, Litchfield Park, AZ, Dec. 1989.
 *	Michael N. Nelson, Brent B. Welch and John K. Ousterhout, "Caching
 *		in the Sprite Network File System", ACM TOCS 6(1),
 *		pages 134-154, February 1988.
 *	V. Srinivasan and Jeffrey C. Mogul, "Spritely NFS: Implementation and
 *		Performance of Cache-Consistency Protocols", Digital
 *		Equipment Corporation WRL Research Report 89/5, May 1989.
 */
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>

#include <vm/vm_zone.h>

#include <netinet/in.h>
#include "rpcv2.h"
#include "nfsproto.h"
#include "nfs.h"
#include "nfsm_subs.h"
#include "xdr_subs.h"
#include "nqnfs.h"
#include "nfsmount.h"
#include "nfsnode.h"

static MALLOC_DEFINE(M_NQMHOST, "NQNFS Host", "Nqnfs host address table");

time_t nqnfsstarttime = (time_t)0;
int nqsrv_clockskew = NQ_CLOCKSKEW;
int nqsrv_writeslack = NQ_WRITESLACK;
int nqsrv_maxlease = NQ_MAXLEASE;
#ifndef NFS_NOSERVER
static int nqsrv_maxnumlease = NQ_MAXNUMLEASE;
#endif

struct vop_lease_args;

#ifndef NFS_NOSERVER
static int	nqsrv_cmpnam (struct nfssvc_sock *, struct sockaddr *,
			struct nqhost *);
static int	nqnfs_vacated (struct vnode *vp, struct ucred *cred);
static void	nqsrv_addhost (struct nqhost *lph, struct nfssvc_sock *slp,
				   struct sockaddr *nam);
static void	nqsrv_instimeq (struct nqlease *lp, u_int32_t duration);
static void	nqsrv_locklease (struct nqlease *lp);
static void	nqsrv_send_eviction (struct vnode *vp, struct nqlease *lp,
					 struct nfssvc_sock *slp,
					 struct sockaddr *nam, 
					 struct ucred *cred);
static void	nqsrv_unlocklease (struct nqlease *lp);
static void	nqsrv_waitfor_expiry (struct nqlease *lp);
#endif
extern void	nqnfs_lease_updatetime (int deltat);

/*
 * Signifies which rpcs can have piggybacked lease requests
 */
int nqnfs_piggy[NFS_NPROCS] = {
	0,
	0,
	ND_WRITE,
	ND_READ,
	0,
	ND_READ,
	ND_READ,
	ND_WRITE,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ND_READ,
	ND_READ,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

extern nfstype nfsv2_type[9];
extern nfstype nfsv3_type[9];
extern struct nfsstats nfsstats;

#define TRUE	1
#define	FALSE	0

#ifndef NFS_NOSERVER 
/*
 * Get or check for a lease for "vp", based on ND_CHECK flag.
 * The rules are as follows:
 * - if a current non-caching lease, reply non-caching
 * - if a current lease for same host only, extend lease
 * - if a read cachable lease and a read lease request
 *	add host to list any reply cachable
 * - else { set non-cachable for read-write sharing }
 *	send eviction notice messages to all other hosts that have lease
 *	wait for lease termination { either by receiving vacated messages
 *					from all the other hosts or expiry
 *					via. timeout }
 *	modify lease to non-cachable
 * - else if no current lease, issue new one
 * - reply
 * - return boolean TRUE iff nam should be m_freem()'d
 * NB: Since nqnfs_serverd() is called from a timer, any potential tsleep()
 *     in here must be framed by nqsrv_locklease() and nqsrv_unlocklease().
 *     nqsrv_locklease() is coded such that at least one of LC_LOCKED and
 *     LC_WANTED is set whenever a process is tsleeping in it. The exception
 *     is when a new lease is being allocated, since it is not in the timer
 *     queue yet. (Ditto for the splsoftclock() and splx(s) calls)
 */
int
nqsrv_getlease(struct vnode *vp, u_int32_t *duration, int flags,
	       struct nfssvc_sock *slp, struct thread *td,
	       struct sockaddr *nam, int *cachablep, u_quad_t *frev,
	       struct ucred *cred)
{
	struct nqlease *lp;
	struct nqfhhashhead *lpp = NULL;
	struct nqhost *lph = NULL;
	struct nqlease *tlp;
	struct nqm **lphp;
	struct vattr vattr;
	fhandle_t fh;
	int i, ok, error, s;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK)
		return (0);
	if (*duration > nqsrv_maxlease)
		*duration = nqsrv_maxlease;
	error = VOP_GETATTR(vp, &vattr, td);
	if (error)
		return (error);
	*frev = vattr.va_filerev;
	s = splsoftclock();
	tlp = vp->v_lease;
	if ((flags & ND_CHECK) == 0)
		nfsstats.srvnqnfs_getleases++;
	if (tlp == 0) {
		/*
		 * Find the lease by searching the hash list.
		 */
		fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
		error = VFS_VPTOFH(vp, &fh.fh_fid);
		if (error) {
			splx(s);
			return (error);
		}
		lpp = NQFHHASH(fh.fh_fid.fid_data);
		for (lp = lpp->lh_first; lp != 0; lp = lp->lc_hash.le_next)
			if (fh.fh_fsid.val[0] == lp->lc_fsid.val[0] &&
			    fh.fh_fsid.val[1] == lp->lc_fsid.val[1] &&
			    !bcmp(fh.fh_fid.fid_data, lp->lc_fiddata,
				  fh.fh_fid.fid_len - sizeof (int32_t))) {
				/* Found it */
				lp->lc_vp = vp;
				vp->v_lease = lp;
				tlp = lp;
				break;
			}
	} else
		lp = tlp;
	if (lp != 0) {
		if ((lp->lc_flag & LC_NONCACHABLE) ||
		    (lp->lc_morehosts == (struct nqm *)0 &&
		     nqsrv_cmpnam(slp, nam, &lp->lc_host)))
			goto doreply;
		if ((flags & ND_READ) && (lp->lc_flag & LC_WRITE) == 0) {
			if (flags & ND_CHECK)
				goto doreply;
			if (nqsrv_cmpnam(slp, nam, &lp->lc_host))
				goto doreply;
			i = 0;
			if (lp->lc_morehosts) {
				lph = lp->lc_morehosts->lpm_hosts;
				lphp = &lp->lc_morehosts->lpm_next;
				ok = 1;
			} else {
				lphp = &lp->lc_morehosts;
				ok = 0;
			}
			while (ok && (lph->lph_flag & LC_VALID)) {
				if (nqsrv_cmpnam(slp, nam, lph))
					goto doreply;
				if (++i == LC_MOREHOSTSIZ) {
					i = 0;
					if (*lphp) {
						lph = (*lphp)->lpm_hosts;
						lphp = &((*lphp)->lpm_next);
					} else
						ok = 0;
				} else
					lph++;
			}
			nqsrv_locklease(lp);
			if (!ok) {
				*lphp = (struct nqm *)
					malloc(sizeof (struct nqm),
						M_NQMHOST, M_WAITOK);
				bzero((caddr_t)*lphp, sizeof (struct nqm));
				lph = (*lphp)->lpm_hosts;
			}
			nqsrv_addhost(lph, slp, nam);
			nqsrv_unlocklease(lp);
		} else {
			lp->lc_flag |= LC_NONCACHABLE;
			nqsrv_locklease(lp);
			nqsrv_send_eviction(vp, lp, slp, nam, cred);
			nqsrv_waitfor_expiry(lp);
			nqsrv_unlocklease(lp);
		}
doreply:
		/*
		 * Update the lease and return
		 */
		if ((flags & ND_CHECK) == 0)
			nqsrv_instimeq(lp, *duration);
		if (lp->lc_flag & LC_NONCACHABLE)
			*cachablep = 0;
		else {
			*cachablep = 1;
			if (flags & ND_WRITE)
				lp->lc_flag |= LC_WRITTEN;
		}
		splx(s);
		return (0);
	}
	splx(s);
	if (flags & ND_CHECK)
		return (0);

	/*
	 * Allocate new lease
	 * The value of nqsrv_maxnumlease should be set generously, so that
	 * the following "printf" happens infrequently.
	 */
	if (nfsstats.srvnqnfs_leases > nqsrv_maxnumlease) {
		printf("Nqnfs server, too many leases\n");
		do {
			(void) tsleep((caddr_t)&lbolt, 0, "nqsrvnuml", 0);
		} while (nfsstats.srvnqnfs_leases > nqsrv_maxnumlease);
	}
	MALLOC(lp, struct nqlease *, sizeof (struct nqlease), M_NQLEASE, M_WAITOK);
	bzero((caddr_t)lp, sizeof (struct nqlease));
	if (flags & ND_WRITE)
		lp->lc_flag |= (LC_WRITE | LC_WRITTEN);
	nqsrv_addhost(&lp->lc_host, slp, nam);
	lp->lc_vp = vp;
	lp->lc_fsid = fh.fh_fsid;
	bcopy(fh.fh_fid.fid_data, lp->lc_fiddata,
		fh.fh_fid.fid_len - sizeof (int32_t));
	if(!lpp)
		panic("nfs_nqlease.c: Phoney lpp");
	LIST_INSERT_HEAD(lpp, lp, lc_hash);
	vp->v_lease = lp;
	s = splsoftclock();
	nqsrv_instimeq(lp, *duration);
	splx(s);
	*cachablep = 1;
	if (++nfsstats.srvnqnfs_leases > nfsstats.srvnqnfs_maxleases)
		nfsstats.srvnqnfs_maxleases = nfsstats.srvnqnfs_leases;
	return (0);
}

/*
 * Local lease check for server syscalls.
 * Just set up args and let nqsrv_getlease() do the rest.
 * nqnfs_vop_lease_check() is the VOP_LEASE() form of the same routine.
 * Ifdef'd code in nfsnode.h renames these routines to whatever a particular
 * OS needs.
 */
int
nqnfs_lease_check(struct vnode *vp, struct thread *td,
		  struct ucred *cred, int flag)
{
	u_int32_t duration = 0;
	int cache;
	u_quad_t frev;

	nqsrv_getlease(vp, &duration, ND_CHECK | flag, NQLOCALSLP,
		td, (struct sockaddr *)0, &cache, &frev, cred);
	return(0);
}

/*
 * nqnfs_vop_lease_check(struct vnode *a_vp, struct thread *a_td,
 *			 struct ucred *a_cred, int a_flag)
 */
int
nqnfs_vop_lease_check(struct vop_lease_args *ap)
{
	u_int32_t duration = 0;
	int cache;
	u_quad_t frev;

	(void) nqsrv_getlease(ap->a_vp, &duration, ND_CHECK | ap->a_flag,
			      NQLOCALSLP, ap->a_td, (struct sockaddr *)0,
			      &cache, &frev, ap->a_cred);
	return (0);
}


/*
 * Add a host to an nqhost structure for a lease.
 */
static void
nqsrv_addhost(struct nqhost *lph, struct nfssvc_sock *slp, struct sockaddr *nam)
{
	struct sockaddr_in *saddr;
	struct socket *nsso;

	if (slp == NQLOCALSLP) {
		lph->lph_flag |= (LC_VALID | LC_LOCAL);
		return;
	}
	nsso = slp->ns_so;
	lph->lph_slp = slp;
	if (nsso && nsso->so_proto->pr_protocol == IPPROTO_UDP) {
		saddr = (struct sockaddr_in *)nam;
		lph->lph_flag |= (LC_VALID | LC_UDP);
		lph->lph_inetaddr = saddr->sin_addr.s_addr;
		lph->lph_port = saddr->sin_port;
	} else {
		lph->lph_flag |= (LC_VALID | LC_SREF);
		slp->ns_sref++;
	}
}

/*
 * Update the lease expiry time and position it in the timer queue correctly.
 */
static void
nqsrv_instimeq(struct nqlease *lp, u_int32_t duration)
{
	struct nqlease *tlp;
	time_t newexpiry;

	newexpiry = time_second + duration + nqsrv_clockskew;
	if (lp->lc_expiry == newexpiry)
		return;
	if (lp->lc_timer.cqe_next != 0) {
		CIRCLEQ_REMOVE(&nqtimerhead, lp, lc_timer);
	}
	lp->lc_expiry = newexpiry;

	/*
	 * Find where in the queue it should be.
	 */
	tlp = nqtimerhead.cqh_last;
	while (tlp != (void *)&nqtimerhead && tlp->lc_expiry > newexpiry)
		tlp = tlp->lc_timer.cqe_prev;
#ifdef HASNVRAM
	if (tlp == nqtimerhead.cqh_last)
		NQSTORENOVRAM(newexpiry);
#endif /* HASNVRAM */
	if (tlp == (void *)&nqtimerhead) {
		CIRCLEQ_INSERT_HEAD(&nqtimerhead, lp, lc_timer);
	} else {
		CIRCLEQ_INSERT_AFTER(&nqtimerhead, tlp, lp, lc_timer);
	}
}

/*
 * Compare the requesting host address with the lph entry in the lease.
 * Return true iff it is the same.
 * This is somewhat messy due to the union in the nqhost structure.
 * The local host is indicated by the special value of NQLOCALSLP for slp.
 */
static int
nqsrv_cmpnam(struct nfssvc_sock *slp, struct sockaddr *nam, struct nqhost *lph)
{
	struct sockaddr_in *saddr;
	struct sockaddr *addr;
	union nethostaddr lhaddr;
	struct socket *nsso;
	int ret;

	if (slp == NQLOCALSLP) {
		if (lph->lph_flag & LC_LOCAL)
			return (1);
		else
			return (0);
	}
	nsso = slp->ns_so;
	if (nsso && nsso->so_proto->pr_protocol == IPPROTO_UDP) {
		addr = nam;
	} else {
		addr = slp->ns_nam;
	}
	if (lph->lph_flag & LC_UDP) {
		ret = netaddr_match(AF_INET, &lph->lph_haddr, addr);
	} else {
		if ((lph->lph_slp->ns_flag & SLP_VALID) == 0)
			return (0);
		saddr = (struct sockaddr_in *)lph->lph_slp->ns_nam;
		if (saddr->sin_family == AF_INET)
			lhaddr.had_inetaddr = saddr->sin_addr.s_addr;
		else
			lhaddr.had_nam = lph->lph_slp->ns_nam;
		ret = netaddr_match(saddr->sin_family, &lhaddr, addr);
	}
	return (ret);
}

/*
 * Send out eviction notice messages to all other hosts for the lease.
 */
static void
nqsrv_send_eviction(struct vnode *vp, struct nqlease *lp,
		    struct nfssvc_sock *slp, struct sockaddr *nam,
		    struct ucred *cred)
{
	struct nqhost *lph = &lp->lc_host;
	int siz;
	struct nqm *lphnext = lp->lc_morehosts;
	struct mbuf *m, *mreq, *mb, *mb2, *mheadend;
	struct sockaddr *nam2;
	struct sockaddr_in *saddr;
	nfsfh_t nfh;
	fhandle_t *fhp;
	caddr_t bpos, cp;
	u_int32_t xid, *tl;
	int len = 1, ok = 1, i = 0;

	while (ok && (lph->lph_flag & LC_VALID)) {
		if (nqsrv_cmpnam(slp, nam, lph)) {
			lph->lph_flag |= LC_VACATED;
		} else if ((lph->lph_flag & (LC_LOCAL | LC_VACATED)) == 0) {
			struct socket *so;
			int sotype;
			int *solockp = NULL;

			so = lph->lph_slp->ns_so;
			if (lph->lph_flag & LC_UDP) {
				MALLOC(nam2, struct sockaddr *,
				       sizeof *nam2, M_SONAME, M_WAITOK);
				saddr = (struct sockaddr_in *)nam2;
				saddr->sin_len = sizeof *saddr;
				saddr->sin_family = AF_INET;
				saddr->sin_addr.s_addr = lph->lph_inetaddr;
				saddr->sin_port = lph->lph_port;
			} else if (lph->lph_slp->ns_flag & SLP_VALID) {
				nam2 = (struct sockaddr *)0;
			} else {
				goto nextone;
			}
			sotype = so->so_type;
			if (so->so_proto->pr_flags & PR_CONNREQUIRED)
				solockp = &lph->lph_slp->ns_solock;
			nfsm_reqhead((struct vnode *)0, NQNFSPROC_EVICTED,
				NFSX_V3FH + NFSX_UNSIGNED);
			fhp = &nfh.fh_generic;
			bzero((caddr_t)fhp, sizeof(nfh));
			fhp->fh_fsid = vp->v_mount->mnt_stat.f_fsid;
			VFS_VPTOFH(vp, &fhp->fh_fid);
			nfsm_srvfhtom(fhp, 1);
			m = mreq;
			siz = 0;
			while (m) {
				siz += m->m_len;
				m = m->m_next;
			}
			if (siz <= 0 || siz > NFS_MAXPACKET) {
				printf("mbuf siz=%d\n",siz);
				panic("Bad nfs svc reply");
			}
			m = nfsm_rpchead(cred, (NFSMNT_NFSV3 | NFSMNT_NQNFS),
				NQNFSPROC_EVICTED,
				RPCAUTH_UNIX, 5 * NFSX_UNSIGNED, (char *)0,
				0, (char *)NULL, mreq, siz, &mheadend, &xid);
			/*
			 * For stream protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (sotype == SOCK_STREAM) {
				M_PREPEND(m, NFSX_UNSIGNED, MB_WAIT);
				/* XXX-MBUF */
				printf("nfs_nqlease: M_PREPEND failed\n");
				*mtod(m, u_int32_t *) = htonl(0x80000000 |
					(m->m_pkthdr.len - NFSX_UNSIGNED));
			}
			/*
			 * nfs_sndlock if PR_CONNREQUIRED XXX
			 */

			if ((lph->lph_flag & LC_UDP) == 0 &&
			    ((lph->lph_slp->ns_flag & SLP_VALID) == 0 ||
			    nfs_slplock(lph->lph_slp, 0) == 0)) {
				m_freem(m);
			} else {
				(void) nfs_send(so, nam2, m,
						(struct nfsreq *)0);
				if (solockp)
					nfs_slpunlock(lph->lph_slp);
			}
			if (lph->lph_flag & LC_UDP)
				FREE(nam2, M_SONAME);
		}
nextone:
		if (++i == len) {
			if (lphnext) {
				i = 0;
				len = LC_MOREHOSTSIZ;
				lph = lphnext->lpm_hosts;
				lphnext = lphnext->lpm_next;
			} else
				ok = 0;
		} else
			lph++;
	}
}

/*
 * Wait for the lease to expire.
 * This will occur when all clients have sent "vacated" messages to
 * this server OR when it expires do to timeout.
 */
static void
nqsrv_waitfor_expiry(struct nqlease *lp)
{
	struct nqhost *lph;
	int i;
	struct nqm *lphnext;
	int len, ok;

tryagain:
	if (time_second > lp->lc_expiry)
		return;
	lph = &lp->lc_host;
	lphnext = lp->lc_morehosts;
	len = 1;
	i = 0;
	ok = 1;
	while (ok && (lph->lph_flag & LC_VALID)) {
		if ((lph->lph_flag & (LC_LOCAL | LC_VACATED)) == 0) {
			lp->lc_flag |= LC_EXPIREDWANTED;
			(void) tsleep((caddr_t)&lp->lc_flag, 0, "nqexp", 0);
			goto tryagain;
		}
		if (++i == len) {
			if (lphnext) {
				i = 0;
				len = LC_MOREHOSTSIZ;
				lph = lphnext->lpm_hosts;
				lphnext = lphnext->lpm_next;
			} else
				ok = 0;
		} else
			lph++;
	}
}

/*
 * Nqnfs server timer that maintains the server lease queue.
 * Scan the lease queue for expired entries:
 * - when one is found, wakeup anyone waiting for it
 *   else dequeue and free
 */
void
nqnfs_serverd(void)
{
	struct nqlease *lp;
	struct nqhost *lph;
	struct nqlease *nextlp;
	struct nqm *lphnext, *olphnext;
	int i, len, ok;

	for (lp = nqtimerhead.cqh_first; lp != (void *)&nqtimerhead;
	    lp = nextlp) {
		if (lp->lc_expiry >= time_second)
			break;
		nextlp = lp->lc_timer.cqe_next;
		if (lp->lc_flag & LC_EXPIREDWANTED) {
			lp->lc_flag &= ~LC_EXPIREDWANTED;
			wakeup((caddr_t)&lp->lc_flag);
		} else if ((lp->lc_flag & (LC_LOCKED | LC_WANTED)) == 0) {
		    /*
		     * Make a best effort at keeping a write caching lease long
		     * enough by not deleting it until it has been explicitly
		     * vacated or there have been no writes in the previous
		     * write_slack seconds since expiry and the nfsds are not
		     * all busy. The assumption is that if the nfsds are not
		     * all busy now (no queue of nfs requests), then the client
		     * would have been able to do at least one write to the
		     * file during the last write_slack seconds if it was still
		     * trying to push writes to the server.
		     */
		    if ((lp->lc_flag & (LC_WRITE | LC_VACATED)) == LC_WRITE &&
			((lp->lc_flag & LC_WRITTEN) || nfsd_waiting == 0)) {
			lp->lc_flag &= ~LC_WRITTEN;
			nqsrv_instimeq(lp, nqsrv_writeslack);
		    } else {
			CIRCLEQ_REMOVE(&nqtimerhead, lp, lc_timer);
			LIST_REMOVE(lp, lc_hash);
			/*
			 * This soft reference may no longer be valid, but
			 * no harm done. The worst case is if the vnode was
			 * recycled and has another valid lease reference,
			 * which is dereferenced prematurely.
			 */
			lp->lc_vp->v_lease = (struct nqlease *)0;
			lph = &lp->lc_host;
			lphnext = lp->lc_morehosts;
			olphnext = (struct nqm *)0;
			len = 1;
			i = 0;
			ok = 1;
			while (ok && (lph->lph_flag & LC_VALID)) {
				if (lph->lph_flag & LC_SREF)
					nfsrv_slpderef(lph->lph_slp);
				if (++i == len) {
					if (olphnext) {
						free((caddr_t)olphnext, M_NQMHOST);
						olphnext = (struct nqm *)0;
					}
					if (lphnext) {
						olphnext = lphnext;
						i = 0;
						len = LC_MOREHOSTSIZ;
						lph = lphnext->lpm_hosts;
						lphnext = lphnext->lpm_next;
					} else
						ok = 0;
				} else
					lph++;
			}
			FREE((caddr_t)lp, M_NQLEASE);
			if (olphnext)
				free((caddr_t)olphnext, M_NQMHOST);
			nfsstats.srvnqnfs_leases--;
		    }
		}
	}
}

/*
 * Called from nfssvc_nfsd() for a getlease rpc request.
 * Do the from/to xdr translation and call nqsrv_getlease() to
 * do the real work.
 */
int
nqnfsrv_getlease(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
		 struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct ucred *cred = &nfsd->nd_cr;
	struct nfs_fattr *fp;
	struct vattr va;
	struct vattr *vap = &va;
	struct vnode *vp;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	u_quad_t frev;
	caddr_t bpos;
	int error = 0;
	char *cp2;
	struct mbuf *mb, *mb2, *mreq;
	int flags, rdonly, cache;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	nfsm_dissect(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	flags = fxdr_unsigned(int, *tl++);
	nfsd->nd_duration = fxdr_unsigned(int, *tl);
	error = nfsrv_fhtovp(fhp, 1, &vp, cred, slp, nam, &rdonly,
		(nfsd->nd_flag & ND_KERBAUTH), TRUE);
	if (error) {
		nfsm_reply(0);
		goto nfsmout;
	}
	if (rdonly && flags == ND_WRITE) {
		error = EROFS;
		vput(vp);
		nfsm_reply(0);
	}
	(void) nqsrv_getlease(vp, &nfsd->nd_duration, flags, slp, td,
		nam, &cache, &frev, cred);
	error = VOP_GETATTR(vp, vap, td);
	vput(vp);
	nfsm_reply(NFSX_V3FATTR + 4 * NFSX_UNSIGNED);
	nfsm_build(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(cache);
	*tl++ = txdr_unsigned(nfsd->nd_duration);
	txdr_hyper(frev, tl);
	nfsm_build(fp, struct nfs_fattr *, NFSX_V3FATTR);
	nfsm_srvfillattr(vap, fp);
	nfsm_srvdone;
}

/*
 * Called from nfssvc_nfsd() when a "vacated" message is received from a
 * client. Find the entry and expire it.
 */
int
nqnfsrv_vacated(struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
		struct thread *td, struct mbuf **mrq)
{
	struct mbuf *mrep = nfsd->nd_mrep, *md = nfsd->nd_md;
	struct sockaddr *nam = nfsd->nd_nam;
	caddr_t dpos = nfsd->nd_dpos;
	struct nqlease *lp;
	struct nqhost *lph;
	struct nqlease *tlp = (struct nqlease *)0;
	nfsfh_t nfh;
	fhandle_t *fhp;
	u_int32_t *tl;
	int32_t t1;
	struct nqm *lphnext;
	struct mbuf *mreq, *mb;
	int error = 0, i, len, ok, gotit = 0, cache = 0;
	char *cp2, *bpos;
	u_quad_t frev;

	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	m_freem(mrep);
	/*
	 * Find the lease by searching the hash list.
	 */
	for (lp = NQFHHASH(fhp->fh_fid.fid_data)->lh_first; lp != 0;
	    lp = lp->lc_hash.le_next)
		if (fhp->fh_fsid.val[0] == lp->lc_fsid.val[0] &&
		    fhp->fh_fsid.val[1] == lp->lc_fsid.val[1] &&
		    !bcmp(fhp->fh_fid.fid_data, lp->lc_fiddata,
			  MAXFIDSZ)) {
			/* Found it */
			tlp = lp;
			break;
		}
	if (tlp != 0) {
		lp = tlp;
		len = 1;
		i = 0;
		lph = &lp->lc_host;
		lphnext = lp->lc_morehosts;
		ok = 1;
		while (ok && (lph->lph_flag & LC_VALID)) {
			if (nqsrv_cmpnam(slp, nam, lph)) {
				lph->lph_flag |= LC_VACATED;
				gotit++;
				break;
			}
			if (++i == len) {
				if (lphnext) {
					len = LC_MOREHOSTSIZ;
					i = 0;
					lph = lphnext->lpm_hosts;
					lphnext = lphnext->lpm_next;
				} else
					ok = 0;
			} else
				lph++;
		}
		if ((lp->lc_flag & LC_EXPIREDWANTED) && gotit) {
			lp->lc_flag &= ~LC_EXPIREDWANTED;
			wakeup((caddr_t)&lp->lc_flag);
		}
nfsmout:
		return (EPERM);
	}
	return (EPERM);
}

#endif /* NFS_NOSERVER */

/*
 * Client get lease rpc function.
 */
int
nqnfs_getlease(struct vnode *vp, int rwflag, struct thread *td)
{
	u_int32_t *tl;
	caddr_t cp;
	int32_t t1, t2;
	struct nfsnode *np;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t bpos, dpos, cp2;
	time_t reqtime;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	int cachable;
	u_quad_t frev;

	nfsstats.rpccnt[NQNFSPROC_GETLEASE]++;
	mb = mreq = nfsm_reqh(vp, NQNFSPROC_GETLEASE, NFSX_V3FH+2*NFSX_UNSIGNED,
		 &bpos);
	nfsm_fhtom(vp, 1);
	nfsm_build(tl, u_int32_t *, 2 * NFSX_UNSIGNED);
	*tl++ = txdr_unsigned(rwflag);
	*tl = txdr_unsigned(nmp->nm_leaseterm);
	reqtime = time_second;
	nfsm_request(vp, NQNFSPROC_GETLEASE, td, nfs_vpcred(vp, rwflag));
	np = VTONFS(vp);
	nfsm_dissect(tl, u_int32_t *, 4 * NFSX_UNSIGNED);
	cachable = fxdr_unsigned(int, *tl++);
	reqtime += fxdr_unsigned(int, *tl++);
	if (reqtime > time_second) {
		frev = fxdr_hyper(tl);
		nqnfs_clientlease(nmp, np, rwflag, cachable, reqtime, frev);
		nfsm_loadattr(vp, (struct vattr *)0);
	} else {
		error = NQNFS_EXPIRED;
	}
	m_freem(mrep);
nfsmout:
	return (error);
}

#ifndef NFS_NOSERVER 
/*
 * Client vacated message function.
 */
static int
nqnfs_vacated(struct vnode *vp, struct ucred *cred)
{
	caddr_t cp;
	int i;
	u_int32_t *tl;
	int32_t t2;
	caddr_t bpos;
	u_int32_t xid;
	int error = 0;
	struct mbuf *m, *mreq, *mb, *mb2, *mheadend;
	struct nfsmount *nmp;
	struct nfsreq myrep;

	nmp = VFSTONFS(vp->v_mount);
	nfsstats.rpccnt[NQNFSPROC_VACATED]++;
	nfsm_reqhead(vp, NQNFSPROC_VACATED, NFSX_FH(1));
	nfsm_fhtom(vp, 1);
	m = mreq;
	i = 0;
	while (m) {
		i += m->m_len;
		m = m->m_next;
	}
	m = nfsm_rpchead(cred, nmp->nm_flag, NQNFSPROC_VACATED,
		RPCAUTH_UNIX, 5 * NFSX_UNSIGNED, (char *)0,
		0, (char *)NULL, mreq, i, &mheadend, &xid);
	if (nmp->nm_sotype == SOCK_STREAM) {
		M_PREPEND(m, NFSX_UNSIGNED, MB_WAIT);
		if (m == NULL)
			return (ENOBUFS);
		*mtod(m, u_int32_t *) = htonl(0x80000000 | (m->m_pkthdr.len -
			NFSX_UNSIGNED));
	}
	myrep.r_flags = 0;
	myrep.r_nmp = nmp;
	myrep.r_td = NULL;
	if (nmp->nm_soflags & PR_CONNREQUIRED)
		(void) nfs_sndlock(&myrep);
	(void) nfs_send(nmp->nm_so, nmp->nm_nam, m, &myrep);
	if (nmp->nm_soflags & PR_CONNREQUIRED)
		nfs_sndunlock(&myrep);
nfsmout:
	return (error);
}

/*
 * Called for client side callbacks
 */
int
nqnfs_callback(struct nfsmount *nmp, struct mbuf *mrep, struct mbuf *md,
	       caddr_t dpos)
{
	struct vnode *vp;
	u_int32_t *tl;
	int32_t t1;
	nfsfh_t nfh;
	fhandle_t *fhp;
	struct nfsnode *np;
	struct nfsd tnfsd;
	struct nfssvc_sock *slp;
	struct nfsrv_descript ndesc;
	struct nfsrv_descript *nfsd = &ndesc;
	struct mbuf **mrq = (struct mbuf **)0, *mb, *mreq;
	int error = 0, cache = 0;
	char *cp2, *bpos;
	u_quad_t frev;

#ifndef nolint
	slp = NULL;
#endif
	nfsd->nd_mrep = mrep;
	nfsd->nd_md = md;
	nfsd->nd_dpos = dpos;
	error = nfs_getreq(nfsd, &tnfsd, FALSE);
	if (error)
		return (error);
	md = nfsd->nd_md;
	dpos = nfsd->nd_dpos;
	if (nfsd->nd_procnum != NQNFSPROC_EVICTED) {
		m_freem(mrep);
		return (EPERM);
	}
	fhp = &nfh.fh_generic;
	nfsm_srvmtofh(fhp);
	m_freem(mrep);
	error = nfs_nget(nmp->nm_mountp, (nfsfh_t *)fhp, NFSX_V3FH, &np);
	if (error)
		return (error);
	vp = NFSTOV(np);
	if (np->n_timer.cqe_next != 0) {
		np->n_expiry = 0;
		np->n_flag |= NQNFSEVICTED;
		if (nmp->nm_timerhead.cqh_first != np) {
			CIRCLEQ_REMOVE(&nmp->nm_timerhead, np, n_timer);
			CIRCLEQ_INSERT_HEAD(&nmp->nm_timerhead, np, n_timer);
		}
	}
	vput(vp);
	nfsm_srvdone;
}


/*
 * Nqnfs client helper daemon. Runs once a second to expire leases.
 * It also get authorization strings for "kerb" mounts.
 * It must start at the beginning of the list again after any potential
 * "sleep" since nfs_reclaim() called from vclean() can pull a node off
 * the list asynchronously.
 */
int
nqnfs_clientd(struct nfsmount *nmp, struct ucred *cred, struct nfsd_cargs *ncd,
	      int flag, caddr_t argp, struct thread *td)
{
	struct nfsnode *np;
	struct vnode *vp;
	struct nfsreq myrep;
	struct nfsuid *nuidp, *nnuidp;
	int error = 0, vpid;

	/*
	 * First initialize some variables
	 */

	/*
	 * If an authorization string is being passed in, get it.
	 */
	if ((flag & NFSSVC_GOTAUTH) &&
	    (nmp->nm_state & (NFSSTA_WAITAUTH | NFSSTA_DISMNT)) == 0) {
	    if (nmp->nm_state & NFSSTA_HASAUTH)
		panic("cld kerb");
	    if ((flag & NFSSVC_AUTHINFAIL) == 0) {
		if (ncd->ncd_authlen <= nmp->nm_authlen &&
		    ncd->ncd_verflen <= nmp->nm_verflen &&
		    !copyin(ncd->ncd_authstr,nmp->nm_authstr,ncd->ncd_authlen)&&
		    !copyin(ncd->ncd_verfstr,nmp->nm_verfstr,ncd->ncd_verflen)){
		    nmp->nm_authtype = ncd->ncd_authtype;
		    nmp->nm_authlen = ncd->ncd_authlen;
		    nmp->nm_verflen = ncd->ncd_verflen;
#ifdef NFSKERB
		    nmp->nm_key = ncd->ncd_key;
#endif
		} else
		    nmp->nm_state |= NFSSTA_AUTHERR;
	    } else
		nmp->nm_state |= NFSSTA_AUTHERR;
	    nmp->nm_state |= NFSSTA_HASAUTH;
	    wakeup((caddr_t)&nmp->nm_authlen);
	} else
	    nmp->nm_state |= NFSSTA_WAITAUTH;

	/*
	 * Loop every second updating queue until there is a termination sig.
	 */
	while ((nmp->nm_state & NFSSTA_DISMNT) == 0) {
	    if (nmp->nm_flag & NFSMNT_NQNFS) {
		/*
		 * If there are no outstanding requests (and therefore no
		 * processes in nfs_reply) and there is data in the receive
		 * queue, poke for callbacks.
		 */
		if (TAILQ_EMPTY(&nfs_reqq) && nmp->nm_so &&
		    nmp->nm_so->so_rcv.sb_cc > 0) {
			myrep.r_flags = R_GETONEREP;
			myrep.r_nmp = nmp;
			myrep.r_mrep = (struct mbuf *)0;
			myrep.r_td = NULL;
			nfs_reply(&myrep);
		}

		/*
		 * Loop through the leases, updating as required.
		 */
		np = nmp->nm_timerhead.cqh_first;
		while (np != (void *)&nmp->nm_timerhead &&
		       (nmp->nm_state & NFSSTA_DISMINPROG) == 0) {
			vp = NFSTOV(np);
			vpid = vp->v_id;
			if (np->n_expiry < time_second) {
			   if (vget(vp, LK_EXCLUSIVE, td) == 0) {
			     nmp->nm_inprog = vp;
			     if (vpid == vp->v_id) {
				CIRCLEQ_REMOVE(&nmp->nm_timerhead, np, n_timer);
				np->n_timer.cqe_next = 0;
				if (np->n_flag & (NLMODIFIED | NQNFSEVICTED)) {
					if (np->n_flag & NQNFSEVICTED) {
						if (vp->v_type == VDIR)
							nfs_invaldir(vp);
						cache_inval_vp(vp, 0);
						(void) nfs_vinvalbuf(vp,
						       V_SAVE, td, 0);
						np->n_flag &= ~NQNFSEVICTED;
						(void) nqnfs_vacated(vp, cred);
					} else if (vp->v_type == VREG) {
						(void) VOP_FSYNC(vp, MNT_WAIT, td);
						np->n_flag &= ~NLMODIFIED;
					}
				}
			      }
			      vput(vp);
			      nmp->nm_inprog = NULLVP;
			    }
			} else if ((np->n_expiry - NQ_RENEWAL) < time_second) {
			    if ((np->n_flag & (NQNFSWRITE | NQNFSNONCACHE))
				 == NQNFSWRITE &&
				 !TAILQ_EMPTY(&vp->v_dirtyblkhd) &&
				 vget(vp, LK_EXCLUSIVE, td) == 0) {
				 nmp->nm_inprog = vp;
				 if (vpid == vp->v_id &&
				     nqnfs_getlease(vp, ND_WRITE, td)==0)
					np->n_brev = np->n_lrev;
				 vput(vp);
				 nmp->nm_inprog = NULLVP;
			    }
			} else
				break;
			if (np == nmp->nm_timerhead.cqh_first)
				break;
			np = nmp->nm_timerhead.cqh_first;
		}
	    }

	    /*
	     * Get an authorization string, if required.
	     */
	    if ((nmp->nm_state & (NFSSTA_WAITAUTH | NFSSTA_DISMNT | NFSSTA_HASAUTH)) == 0) {
		ncd->ncd_authuid = nmp->nm_authuid;
		if (copyout((caddr_t)ncd, argp, sizeof (struct nfsd_cargs)))
			nmp->nm_state |= NFSSTA_WAITAUTH;
		else
			return (ENEEDAUTH);
	    }

	    /*
	     * Wait a bit (no pun) and do it again.
	     */
	    if ((nmp->nm_state & NFSSTA_DISMNT) == 0 &&
		(nmp->nm_state & (NFSSTA_WAITAUTH | NFSSTA_HASAUTH))) {
		    error = tsleep((caddr_t)&nmp->nm_authstr, PCATCH,
			"nqnfstimr", hz / 3);
		    if (error == EINTR || error == ERESTART)
			(void) dounmount(nmp->nm_mountp, 0, td);
	    }
	}

	/*
	 * Finally, we can free up the mount structure.
	 */
	TAILQ_FOREACH_MUTABLE(nuidp, &nmp->nm_uidlruhead, nu_lru, nnuidp) {
		LIST_REMOVE(nuidp, nu_hash);
		TAILQ_REMOVE(&nmp->nm_uidlruhead, nuidp, nu_lru);
		free((caddr_t)nuidp, M_NFSUID);
	}
	nfs_free_mount(nmp);
	if (error == EWOULDBLOCK)
		error = 0;
	return (error);
}

#endif /* NFS_NOSERVER */

/*
 * Adjust all timer queue expiry times when the time of day clock is changed.
 * Called from the settimeofday() syscall.
 */
void
nqnfs_lease_updatetime(int deltat)
{
	struct thread *td = curthread;	/* XXX */
	struct nqlease *lp;
	struct nfsnode *np;
	struct mount *mp, *nxtmp;
	struct nfsmount *nmp;
	lwkt_tokref ilock;
	int s;

	if (nqnfsstarttime != 0)
		nqnfsstarttime += deltat;
	s = splsoftclock();
	for (lp = nqtimerhead.cqh_first; lp != (void *)&nqtimerhead;
	    lp = lp->lc_timer.cqe_next)
		lp->lc_expiry += deltat;
	splx(s);

	/*
	 * Search the mount list for all nqnfs mounts and do their timer
	 * queues.
	 */
	lwkt_gettoken(&ilock, &mountlist_token);
	for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nxtmp) {
		if (vfs_busy(mp, LK_NOWAIT, &ilock, td)) {
			nxtmp = TAILQ_NEXT(mp, mnt_list);
			continue;
		}
		if (mp->mnt_stat.f_type == nfs_mount_type) {
			nmp = VFSTONFS(mp);
			if (nmp->nm_flag & NFSMNT_NQNFS) {
				for (np = nmp->nm_timerhead.cqh_first;
				    np != (void *)&nmp->nm_timerhead;
				    np = np->n_timer.cqe_next) {
					np->n_expiry += deltat;
				}
			}
		}
		lwkt_gettokref(&ilock);
		nxtmp = TAILQ_NEXT(mp, mnt_list);
		vfs_unbusy(mp, td);
	}
	lwkt_reltoken(&ilock);
}

#ifndef NFS_NOSERVER 
/*
 * Lock a server lease.
 */
static void
nqsrv_locklease(struct nqlease *lp)
{

	while (lp->lc_flag & LC_LOCKED) {
		lp->lc_flag |= LC_WANTED;
		(void) tsleep((caddr_t)lp, 0, "nqlc", 0);
	}
	lp->lc_flag |= LC_LOCKED;
	lp->lc_flag &= ~LC_WANTED;
}

/*
 * Unlock a server lease.
 */
static void
nqsrv_unlocklease(struct nqlease *lp)
{

	lp->lc_flag &= ~LC_LOCKED;
	if (lp->lc_flag & LC_WANTED)
		wakeup((caddr_t)lp);
}
#endif /* NFS_NOSERVER */

/*
 * Update a client lease.
 */
void
nqnfs_clientlease(struct nfsmount *nmp, struct nfsnode *np, int rwflag,
		  int cachable, time_t expiry, u_quad_t frev)
{
	struct nfsnode *tp;

	if (np->n_timer.cqe_next != 0) {
		CIRCLEQ_REMOVE(&nmp->nm_timerhead, np, n_timer);
		if (rwflag == ND_WRITE)
			np->n_flag |= NQNFSWRITE;
	} else if (rwflag == ND_READ)
		np->n_flag &= ~NQNFSWRITE;
	else
		np->n_flag |= NQNFSWRITE;
	if (cachable)
		np->n_flag &= ~NQNFSNONCACHE;
	else
		np->n_flag |= NQNFSNONCACHE;
	np->n_expiry = expiry;
	np->n_lrev = frev;
	tp = nmp->nm_timerhead.cqh_last;
	while (tp != (void *)&nmp->nm_timerhead && tp->n_expiry > np->n_expiry)
		tp = tp->n_timer.cqe_prev;
	if (tp == (void *)&nmp->nm_timerhead) {
		CIRCLEQ_INSERT_HEAD(&nmp->nm_timerhead, np, n_timer);
	} else {
		CIRCLEQ_INSERT_AFTER(&nmp->nm_timerhead, tp, np, n_timer);
	}
}
