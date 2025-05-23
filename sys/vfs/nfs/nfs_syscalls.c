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
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 * $FreeBSD: src/sys/nfs/nfs_syscalls.c,v 1.58.2.1 2000/11/26 02:30:06 dillon Exp $
 * $DragonFly: src/sys/vfs/nfs/nfs_syscalls.c,v 1.20 2005/03/27 23:51:42 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/nlookup.h>
#include <vm/vm_zone.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include "xdr_subs.h"
#include "rpcv2.h"
#include "nfsproto.h"
#include "nfs.h"
#include "nfsm_subs.h"
#include "nfsrvcache.h"
#include "nfsmount.h"
#include "nfsnode.h"
#include "nqnfs.h"
#include "nfsrtt.h"

static MALLOC_DEFINE(M_NFSSVC, "NFS srvsock", "Nfs server structure");

/* Global defs. */
extern int32_t (*nfsrv3_procs[NFS_NPROCS]) (struct nfsrv_descript *nd,
					    struct nfssvc_sock *slp,
					    struct thread *td,
					    struct mbuf **mreqp);
extern int nfs_numasync;
extern time_t nqnfsstarttime;
extern int nqsrv_writeslack;
extern int nfsrtton;
extern struct nfsstats nfsstats;
extern int nfsrvw_procrastinate;
extern int nfsrvw_procrastinate_v3;
static int nuidhash_max = NFS_MAXUIDHASH;

#ifndef NFS_NOSERVER
static void	nfsrv_zapsock (struct nfssvc_sock *slp);
#endif
static int	nfssvc_iod (struct thread *);

#define	TRUE	1
#define	FALSE	0

static int nfs_asyncdaemon[NFS_MAXASYNCDAEMON];

SYSCTL_DECL(_vfs_nfs);

#ifndef NFS_NOSERVER
int nfsd_waiting = 0;
static struct nfsdrt nfsdrt;
static int nfs_numnfsd = 0;
static int notstarted = 1;
static int modify_flag = 0;
static void	nfsd_rt (int sotype, struct nfsrv_descript *nd,
			     int cacherep);
static int	nfssvc_addsock (struct file *, struct sockaddr *,
				    struct thread *);
static int	nfssvc_nfsd (struct nfsd_srvargs *,caddr_t,struct thread *);

static int nfs_privport = 0;
SYSCTL_INT(_vfs_nfs, NFS_NFSPRIVPORT, nfs_privport, CTLFLAG_RW, &nfs_privport, 0, "");
SYSCTL_INT(_vfs_nfs, OID_AUTO, gatherdelay, CTLFLAG_RW, &nfsrvw_procrastinate, 0, "");
SYSCTL_INT(_vfs_nfs, OID_AUTO, gatherdelay_v3, CTLFLAG_RW, &nfsrvw_procrastinate_v3, 0, "");
static int	nfs_soreserve = 65535;
SYSCTL_INT(_vfs_nfs, OID_AUTO, soreserve, CTLFLAG_RW, &nfs_soreserve, 0, "");

/*
 * NFS server system calls
 */

#endif /* NFS_NOSERVER */
/*
 * nfssvc_args(int flag, caddr_t argp)
 *
 * Nfs server psuedo system call for the nfsd's
 * Based on the flag value it either:
 * - adds a socket to the selection list
 * - remains in the kernel as an nfsd
 * - remains in the kernel as an nfsiod
 */
int
nfssvc(struct nfssvc_args *uap)
{
#ifndef NFS_NOSERVER
	struct nlookupdata nd;
	struct file *fp;
	struct sockaddr *nam;
	struct nfsd_args nfsdarg;
	struct nfsd_srvargs nfsd_srvargs, *nsd = &nfsd_srvargs;
	struct nfsd_cargs ncd;
	struct nfsd *nfsd;
	struct nfssvc_sock *slp;
	struct nfsuid *nuidp;
	struct nfsmount *nmp;
	struct vnode *vp;
#endif /* NFS_NOSERVER */
	int error;
	struct thread *td = curthread;

	/*
	 * Must be super user
	 */
	error = suser(td);
	if(error)
		return (error);
	KKASSERT(td->td_proc);	/* for ucred and p_fd */
	while (nfssvc_sockhead_flag & SLP_INIT) {
		 nfssvc_sockhead_flag |= SLP_WANTINIT;
		(void) tsleep((caddr_t)&nfssvc_sockhead, 0, "nfsd init", 0);
	}
	if (uap->flag & NFSSVC_BIOD)
		error = nfssvc_iod(td);
#ifdef NFS_NOSERVER
	else
		error = ENXIO;
#else /* !NFS_NOSERVER */
	else if (uap->flag & NFSSVC_MNTD) {
		error = copyin(uap->argp, (caddr_t)&ncd, sizeof (ncd));
		if (error)
			return (error);
		vp = NULL;
		error = nlookup_init(&nd, ncd.ncd_dirp, UIO_USERSPACE, 
					NLC_FOLLOW);
		if (error == 0)
			error = nlookup(&nd);
		if (error == 0)
			error = cache_vget(nd.nl_ncp, nd.nl_cred, LK_EXCLUSIVE, &vp);   
		nlookup_done(&nd);
		if (error)
			return (error);

		if ((vp->v_flag & VROOT) == 0)
			error = EINVAL;
		nmp = VFSTONFS(vp->v_mount);
		vput(vp);
		if (error)
			return (error);
		if ((nmp->nm_state & NFSSTA_MNTD) &&
			(uap->flag & NFSSVC_GOTAUTH) == 0)
			return (0);
		nmp->nm_state |= NFSSTA_MNTD;
		error = nqnfs_clientd(nmp, td->td_proc->p_ucred, &ncd, uap->flag,
			uap->argp, td);
	} else if (uap->flag & NFSSVC_ADDSOCK) {
		error = copyin(uap->argp, (caddr_t)&nfsdarg, sizeof(nfsdarg));
		if (error)
			return (error);
		error = holdsock(td->td_proc->p_fd, nfsdarg.sock, &fp);
		if (error)
			return (error);
		/*
		 * Get the client address for connected sockets.
		 */
		if (nfsdarg.name == NULL || nfsdarg.namelen == 0)
			nam = (struct sockaddr *)0;
		else {
			error = getsockaddr(&nam, nfsdarg.name,
					    nfsdarg.namelen);
			if (error) {
				fdrop(fp, td);
				return (error);
			}
		}
		error = nfssvc_addsock(fp, nam, td);
		fdrop(fp, td);
	} else {
		error = copyin(uap->argp, (caddr_t)nsd, sizeof (*nsd));
		if (error)
			return (error);
		if ((uap->flag & NFSSVC_AUTHIN) &&
		    ((nfsd = nsd->nsd_nfsd)) != NULL &&
		    (nfsd->nfsd_slp->ns_flag & SLP_VALID)) {
			slp = nfsd->nfsd_slp;

			/*
			 * First check to see if another nfsd has already
			 * added this credential.
			 */
			for (nuidp = NUIDHASH(slp,nsd->nsd_cr.cr_uid)->lh_first;
			    nuidp != 0; nuidp = nuidp->nu_hash.le_next) {
				if (nuidp->nu_cr.cr_uid == nsd->nsd_cr.cr_uid &&
				    (!nfsd->nfsd_nd->nd_nam2 ||
				     netaddr_match(NU_NETFAM(nuidp),
				     &nuidp->nu_haddr, nfsd->nfsd_nd->nd_nam2)))
					break;
			}
			if (nuidp) {
			    nfsrv_setcred(&nuidp->nu_cr,&nfsd->nfsd_nd->nd_cr);
			    nfsd->nfsd_nd->nd_flag |= ND_KERBFULL;
			} else {
			    /*
			     * Nope, so we will.
			     */
			    if (slp->ns_numuids < nuidhash_max) {
				slp->ns_numuids++;
				nuidp = (struct nfsuid *)
				   malloc(sizeof (struct nfsuid), M_NFSUID,
					M_WAITOK);
			    } else
				nuidp = (struct nfsuid *)0;
			    if ((slp->ns_flag & SLP_VALID) == 0) {
				if (nuidp)
				    free((caddr_t)nuidp, M_NFSUID);
			    } else {
				if (nuidp == (struct nfsuid *)0) {
				    nuidp = TAILQ_FIRST(&slp->ns_uidlruhead);
				    LIST_REMOVE(nuidp, nu_hash);
				    TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp,
					nu_lru);
				    if (nuidp->nu_flag & NU_NAM)
					FREE(nuidp->nu_nam, M_SONAME);
			        }
				nuidp->nu_flag = 0;
				nuidp->nu_cr = nsd->nsd_cr;
				if (nuidp->nu_cr.cr_ngroups > NGROUPS)
				    nuidp->nu_cr.cr_ngroups = NGROUPS;
				nuidp->nu_cr.cr_ref = 1;
				nuidp->nu_timestamp = nsd->nsd_timestamp;
				nuidp->nu_expire = time_second + nsd->nsd_ttl;
				/*
				 * and save the session key in nu_key.
				 */
				bcopy(nsd->nsd_key, nuidp->nu_key,
				    sizeof (nsd->nsd_key));
				if (nfsd->nfsd_nd->nd_nam2) {
				    struct sockaddr_in *saddr;

				    saddr = (struct sockaddr_in *)
					    nfsd->nfsd_nd->nd_nam2;
				    switch (saddr->sin_family) {
				    case AF_INET:
					nuidp->nu_flag |= NU_INETADDR;
					nuidp->nu_inetaddr =
					     saddr->sin_addr.s_addr;
					break;
				    case AF_ISO:
				    default:
					nuidp->nu_flag |= NU_NAM;
					nuidp->nu_nam = 
					  dup_sockaddr(nfsd->nfsd_nd->nd_nam2);
					break;
				    };
				}
				TAILQ_INSERT_TAIL(&slp->ns_uidlruhead, nuidp,
					nu_lru);
				LIST_INSERT_HEAD(NUIDHASH(slp, nsd->nsd_uid),
					nuidp, nu_hash);
				nfsrv_setcred(&nuidp->nu_cr,
				    &nfsd->nfsd_nd->nd_cr);
				nfsd->nfsd_nd->nd_flag |= ND_KERBFULL;
			    }
			}
		}
		if ((uap->flag & NFSSVC_AUTHINFAIL) && (nfsd = nsd->nsd_nfsd))
			nfsd->nfsd_flag |= NFSD_AUTHFAIL;
		error = nfssvc_nfsd(nsd, uap->argp, td);
	}
#endif /* NFS_NOSERVER */
	if (error == EINTR || error == ERESTART)
		error = 0;
	return (error);
}

#ifndef NFS_NOSERVER
/*
 * Adds a socket to the list for servicing by nfsds.
 */
static int
nfssvc_addsock(struct file *fp, struct sockaddr *mynam, struct thread *td)
{
	int siz;
	struct nfssvc_sock *slp;
	struct socket *so;
	int error, s;

	so = (struct socket *)fp->f_data;
#if 0
	tslp = (struct nfssvc_sock *)0;
	/*
	 * Add it to the list, as required.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_UDP) {
		tslp = nfs_udpsock;
		if (tslp->ns_flag & SLP_VALID) {
			if (mynam != NULL)
				FREE(mynam, M_SONAME);
			return (EPERM);
		}
	}
#endif
	/*
	 * Reserve buffer space in the socket.  Note that due to bugs in
	 * Linux's delayed-ack code, serious performance degredation may
	 * occur with linux hosts if the minimum is used.
	 */
	if (so->so_type == SOCK_STREAM)
		siz = NFS_MAXPACKET + sizeof (u_long);
	else
		siz = NFS_MAXPACKET;
	if (siz < nfs_soreserve)
	    siz = nfs_soreserve;
	if (siz > sb_max_adj) {
	    printf("Warning: vfs.nfs.soreserve (%d) "
		"limited to adjusted sb_max (%ld)\n",
		nfs_soreserve, sb_max_adj);
	    siz = sb_max_adj;
	}

	error = soreserve(so, siz, siz, &td->td_proc->p_rlimit[RLIMIT_SBSIZE]);
	if (error) {
		if (mynam != NULL)
			FREE(mynam, M_SONAME);
		return (error);
	}

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
	if (so->so_type == SOCK_STREAM) {
		struct sockopt sopt;
		int val;

		bzero(&sopt, sizeof sopt);
		sopt.sopt_level = SOL_SOCKET;
		sopt.sopt_name = SO_KEEPALIVE;
		sopt.sopt_val = &val;
		sopt.sopt_valsize = sizeof val;
		val = 1;
		sosetopt(so, &sopt);
	}
	if (so->so_proto->pr_domain->dom_family == AF_INET &&
	    so->so_proto->pr_protocol == IPPROTO_TCP) {
		struct sockopt sopt;
		int val;

		bzero(&sopt, sizeof sopt);
		sopt.sopt_level = IPPROTO_TCP;
		sopt.sopt_name = TCP_NODELAY;
		sopt.sopt_val = &val;
		sopt.sopt_valsize = sizeof val;
		val = 1;
		sosetopt(so, &sopt);
	}
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;

	slp = (struct nfssvc_sock *)
		malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
	bzero((caddr_t)slp, sizeof (struct nfssvc_sock));
	STAILQ_INIT(&slp->ns_rec);
	TAILQ_INIT(&slp->ns_uidlruhead);
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, slp, ns_chain);

	slp->ns_so = so;
	slp->ns_nam = mynam;
	fp->f_count++;
	slp->ns_fp = fp;
	s = splnet();
	so->so_upcallarg = (caddr_t)slp;
	so->so_upcall = nfsrv_rcv;
	so->so_rcv.sb_flags |= SB_UPCALL;
	slp->ns_flag = (SLP_VALID | SLP_NEEDQ);
	nfsrv_wakenfsd(slp, 1);
	splx(s);
	return (0);
}

/*
 * Called by nfssvc() for nfsds. Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
static int
nfssvc_nfsd(struct nfsd_srvargs *nsd, caddr_t argp, struct thread *td)
{
	int siz;
	struct nfssvc_sock *slp;
	struct nfsd *nfsd = nsd->nsd_nfsd;
	struct nfsrv_descript *nd = NULL;
	struct mbuf *m, *mreq;
	int error = 0, cacherep, s, sotype, writes_todo;
	int procrastinate;
	u_quad_t cur_usec;

#ifndef nolint
	cacherep = RC_DOIT;
	writes_todo = 0;
#endif
	if (nfsd == (struct nfsd *)0) {
		nsd->nsd_nfsd = nfsd = (struct nfsd *)
			malloc(sizeof (struct nfsd), M_NFSD, M_WAITOK);
		bzero((caddr_t)nfsd, sizeof (struct nfsd));
		s = splnet();
		nfsd->nfsd_td = td;
		TAILQ_INSERT_TAIL(&nfsd_head, nfsd, nfsd_chain);
		nfs_numnfsd++;
	} else
		s = splnet();

	/*
	 * Loop getting rpc requests until SIGKILL.
	 */
	for (;;) {
		if ((nfsd->nfsd_flag & NFSD_REQINPROG) == 0) {
			while (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) == 0) {
				nfsd->nfsd_flag |= NFSD_WAITING;
				nfsd_waiting++;
				error = tsleep((caddr_t)nfsd, PCATCH, "nfsd", 0);
				nfsd_waiting--;
				if (error)
					goto done;
			}
			if (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) != 0) {
				TAILQ_FOREACH(slp, &nfssvc_sockhead, ns_chain) {
				    if ((slp->ns_flag & (SLP_VALID | SLP_DOREC))
					== (SLP_VALID | SLP_DOREC)) {
					    slp->ns_flag &= ~SLP_DOREC;
					    slp->ns_sref++;
					    nfsd->nfsd_slp = slp;
					    break;
				    }
				}
				if (slp == 0)
					nfsd_head_flag &= ~NFSD_CHECKSLP;
			}
			if ((slp = nfsd->nfsd_slp) == (struct nfssvc_sock *)0)
				continue;
			if (slp->ns_flag & SLP_VALID) {
				if (slp->ns_flag & SLP_DISCONN)
					nfsrv_zapsock(slp);
				else if (slp->ns_flag & SLP_NEEDQ) {
					slp->ns_flag &= ~SLP_NEEDQ;
					(void) nfs_slplock(slp, 1);
					nfsrv_rcv(slp->ns_so, (caddr_t)slp,
						MB_WAIT);
					nfs_slpunlock(slp);
				}
				error = nfsrv_dorec(slp, nfsd, &nd);
				cur_usec = nfs_curusec();
				if (error && slp->ns_tq.lh_first &&
				    slp->ns_tq.lh_first->nd_time <= cur_usec) {
					error = 0;
					cacherep = RC_DOIT;
					writes_todo = 1;
				} else
					writes_todo = 0;
				nfsd->nfsd_flag |= NFSD_REQINPROG;
			}
		} else {
			error = 0;
			slp = nfsd->nfsd_slp;
		}
		if (error || (slp->ns_flag & SLP_VALID) == 0) {
			if (nd) {
				free((caddr_t)nd, M_NFSRVDESC);
				nd = NULL;
			}
			nfsd->nfsd_slp = (struct nfssvc_sock *)0;
			nfsd->nfsd_flag &= ~NFSD_REQINPROG;
			nfsrv_slpderef(slp);
			continue;
		}
		splx(s);
		sotype = slp->ns_so->so_type;
		if (nd) {
		    getmicrotime(&nd->nd_starttime);
		    if (nd->nd_nam2)
			nd->nd_nam = nd->nd_nam2;
		    else
			nd->nd_nam = slp->ns_nam;

		    /*
		     * Check to see if authorization is needed.
		     */
		    if (nfsd->nfsd_flag & NFSD_NEEDAUTH) {
			nfsd->nfsd_flag &= ~NFSD_NEEDAUTH;
			nsd->nsd_haddr = 
				((struct sockaddr_in *)
				 nd->nd_nam)->sin_addr.s_addr;
			nsd->nsd_authlen = nfsd->nfsd_authlen;
			nsd->nsd_verflen = nfsd->nfsd_verflen;
			if (!copyout(nfsd->nfsd_authstr,nsd->nsd_authstr,
				nfsd->nfsd_authlen) &&
			    !copyout(nfsd->nfsd_verfstr, nsd->nsd_verfstr,
				nfsd->nfsd_verflen) &&
			    !copyout((caddr_t)nsd, argp, sizeof (*nsd)))
			    return (ENEEDAUTH);
			cacherep = RC_DROPIT;
		    } else
			cacherep = nfsrv_getcache(nd, slp, &mreq);

		    /*
		     * Check for just starting up for NQNFS and send
		     * fake "try again later" replies to the NQNFS clients.
		     */
		    if (notstarted && nqnfsstarttime <= time_second) {
			if (modify_flag) {
				nqnfsstarttime = time_second + nqsrv_writeslack;
				modify_flag = 0;
			} else
				notstarted = 0;
		    }
		    if (notstarted) {
			if ((nd->nd_flag & ND_NQNFS) == 0)
				cacherep = RC_DROPIT;
			else if (nd->nd_procnum != NFSPROC_WRITE) {
				nd->nd_procnum = NFSPROC_NOOP;
				nd->nd_repstat = NQNFS_TRYLATER;
				cacherep = RC_DOIT;
			} else
				modify_flag = 1;
		    } else if (nfsd->nfsd_flag & NFSD_AUTHFAIL) {
			nfsd->nfsd_flag &= ~NFSD_AUTHFAIL;
			nd->nd_procnum = NFSPROC_NOOP;
			nd->nd_repstat = (NFSERR_AUTHERR | AUTH_TOOWEAK);
			cacherep = RC_DOIT;
		    } else if (nfs_privport) {
			/* Check if source port is privileged */
			u_short port;
			struct sockaddr *nam = nd->nd_nam;
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)nam;
			port = ntohs(sin->sin_port);
			if (port >= IPPORT_RESERVED && 
			    nd->nd_procnum != NFSPROC_NULL) {
			    nd->nd_procnum = NFSPROC_NOOP;
			    nd->nd_repstat = (NFSERR_AUTHERR | AUTH_TOOWEAK);
			    cacherep = RC_DOIT;
			    printf("NFS request from unprivileged port (%s:%d)\n",
				   inet_ntoa(sin->sin_addr), port);
			}
		    }

		}

		/*
		 * Loop to get all the write rpc relies that have been
		 * gathered together.
		 */
		do {
		    switch (cacherep) {
		    case RC_DOIT:
			if (nd && (nd->nd_flag & ND_NFSV3))
			    procrastinate = nfsrvw_procrastinate_v3;
			else
			    procrastinate = nfsrvw_procrastinate;
			if (writes_todo || (nd->nd_procnum == NFSPROC_WRITE &&
			    procrastinate > 0 && !notstarted))
			    error = nfsrv_writegather(&nd, slp,
				nfsd->nfsd_td, &mreq);
			else
			    error = (*(nfsrv3_procs[nd->nd_procnum]))(nd,
				slp, nfsd->nfsd_td, &mreq);
			if (mreq == NULL)
				break;
			if (error != 0 && error != NFSERR_RETVOID) {
				if (nd->nd_procnum != NQNFSPROC_VACATED)
					nfsstats.srv_errs++;
				nfsrv_updatecache(nd, FALSE, mreq);
				if (nd->nd_nam2)
					FREE(nd->nd_nam2, M_SONAME);
				break;
			}
			nfsstats.srvrpccnt[nd->nd_procnum]++;
			nfsrv_updatecache(nd, TRUE, mreq);
			nd->nd_mrep = (struct mbuf *)0;
		    case RC_REPLY:
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
			m = mreq;
			m->m_pkthdr.len = siz;
			m->m_pkthdr.rcvif = (struct ifnet *)0;
			/*
			 * For stream protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (sotype == SOCK_STREAM) {
				M_PREPEND(m, NFSX_UNSIGNED, MB_WAIT);
				if (m == NULL)
					return (ENOBUFS);
				*mtod(m, u_int32_t *) = htonl(0x80000000 | siz);
			}
			if (slp->ns_so->so_proto->pr_flags & PR_CONNREQUIRED)
				(void) nfs_slplock(slp, 1);
			if (slp->ns_flag & SLP_VALID)
			    error = nfs_send(slp->ns_so, nd->nd_nam2, m, NULL);
			else {
			    error = EPIPE;
			    m_freem(m);
			}
			if (nfsrtton)
				nfsd_rt(sotype, nd, cacherep);
			if (nd->nd_nam2)
				FREE(nd->nd_nam2, M_SONAME);
			if (nd->nd_mrep)
				m_freem(nd->nd_mrep);
			if (error == EPIPE)
				nfsrv_zapsock(slp);
			if (slp->ns_so->so_proto->pr_flags & PR_CONNREQUIRED)
				nfs_slpunlock(slp);
			if (error == EINTR || error == ERESTART) {
				free((caddr_t)nd, M_NFSRVDESC);
				nfsrv_slpderef(slp);
				s = splnet();
				goto done;
			}
			break;
		    case RC_DROPIT:
			if (nfsrtton)
				nfsd_rt(sotype, nd, cacherep);
			m_freem(nd->nd_mrep);
			if (nd->nd_nam2)
				FREE(nd->nd_nam2, M_SONAME);
			break;
		    };
		    if (nd) {
			FREE((caddr_t)nd, M_NFSRVDESC);
			nd = NULL;
		    }

		    /*
		     * Check to see if there are outstanding writes that
		     * need to be serviced.
		     */
		    cur_usec = nfs_curusec();
		    s = splsoftclock();
		    if (slp->ns_tq.lh_first &&
			slp->ns_tq.lh_first->nd_time <= cur_usec) {
			cacherep = RC_DOIT;
			writes_todo = 1;
		    } else
			writes_todo = 0;
		    splx(s);
		} while (writes_todo);
		s = splnet();
		if (nfsrv_dorec(slp, nfsd, &nd)) {
			nfsd->nfsd_flag &= ~NFSD_REQINPROG;
			nfsd->nfsd_slp = NULL;
			nfsrv_slpderef(slp);
		}
	}
done:
	TAILQ_REMOVE(&nfsd_head, nfsd, nfsd_chain);
	splx(s);
	free((caddr_t)nfsd, M_NFSD);
	nsd->nsd_nfsd = (struct nfsd *)0;
	if (--nfs_numnfsd == 0)
		nfsrv_init(TRUE);	/* Reinitialize everything */
	return (error);
}

/*
 * Shut down a socket associated with an nfssvc_sock structure.
 * Should be called with the send lock set, if required.
 * The trick here is to increment the sref at the start, so that the nfsds
 * will stop using it and clear ns_flag at the end so that it will not be
 * reassigned during cleanup.
 */
static void
nfsrv_zapsock(struct nfssvc_sock *slp)
{
	struct nfsuid *nuidp, *nnuidp;
	struct nfsrv_descript *nwp, *nnwp;
	struct socket *so;
	struct file *fp;
	struct nfsrv_rec *rec;
	int s;

	slp->ns_flag &= ~SLP_ALLFLAGS;
	fp = slp->ns_fp;
	if (fp) {
		slp->ns_fp = (struct file *)0;
		so = slp->ns_so;
		so->so_rcv.sb_flags &= ~SB_UPCALL;
		so->so_upcall = NULL;
		so->so_upcallarg = NULL;
		soshutdown(so, 2);
		closef(fp, NULL);
		if (slp->ns_nam)
			FREE(slp->ns_nam, M_SONAME);
		m_freem(slp->ns_raw);
		while ((rec = STAILQ_FIRST(&slp->ns_rec)) != NULL) {
			--slp->ns_numrec;
			STAILQ_REMOVE_HEAD(&slp->ns_rec, nr_link);
			if (rec->nr_address)
				FREE(rec->nr_address, M_SONAME);
			m_freem(rec->nr_packet);
			free(rec, M_NFSRVDESC);
		}
		TAILQ_FOREACH_MUTABLE(nuidp, &slp->ns_uidlruhead, nu_lru,
				      nnuidp) {
			LIST_REMOVE(nuidp, nu_hash);
			TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp, nu_lru);
			if (nuidp->nu_flag & NU_NAM)
				FREE(nuidp->nu_nam, M_SONAME);
			free((caddr_t)nuidp, M_NFSUID);
		}
		s = splsoftclock();
		for (nwp = slp->ns_tq.lh_first; nwp; nwp = nnwp) {
			nnwp = nwp->nd_tq.le_next;
			LIST_REMOVE(nwp, nd_tq);
			free((caddr_t)nwp, M_NFSRVDESC);
		}
		LIST_INIT(&slp->ns_tq);
		splx(s);
	}
}

/*
 * Derefence a server socket structure. If it has no more references and
 * is no longer valid, you can throw it away.
 */
void
nfsrv_slpderef(struct nfssvc_sock *slp)
{
	if (--(slp->ns_sref) == 0 && (slp->ns_flag & SLP_VALID) == 0) {
		TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
		free((caddr_t)slp, M_NFSSVC);
	}
}

/*
 * Lock a socket against others.
 */
int
nfs_slplock(struct nfssvc_sock *slp, int wait)
{
	int *statep = &slp->ns_solock;

	if (!wait && (*statep & NFSSTA_SNDLOCK))
		return(0);	/* already locked, fail */
	while (*statep & NFSSTA_SNDLOCK) {
		*statep |= NFSSTA_WANTSND;
		(void) tsleep((caddr_t)statep, 0, "nfsslplck", 0);
	}
	*statep |= NFSSTA_SNDLOCK;
	return (1);
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_slpunlock(struct nfssvc_sock *slp)
{
	int *statep = &slp->ns_solock;

	if ((*statep & NFSSTA_SNDLOCK) == 0)
		panic("nfs slpunlock");
	*statep &= ~NFSSTA_SNDLOCK;
	if (*statep & NFSSTA_WANTSND) {
		*statep &= ~NFSSTA_WANTSND;
		wakeup((caddr_t)statep);
	}
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrv_init(int terminating)
{
	struct nfssvc_sock *slp, *nslp;

	if (nfssvc_sockhead_flag & SLP_INIT)
		panic("nfsd init");
	nfssvc_sockhead_flag |= SLP_INIT;
	if (terminating) {
		TAILQ_FOREACH_MUTABLE(slp, &nfssvc_sockhead, ns_chain, nslp) {
			if (slp->ns_flag & SLP_VALID)
				nfsrv_zapsock(slp);
			TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
			free((caddr_t)slp, M_NFSSVC);
		}
		nfsrv_cleancache();	/* And clear out server cache */
	} else
		nfs_pub.np_valid = 0;

	TAILQ_INIT(&nfssvc_sockhead);
	nfssvc_sockhead_flag &= ~SLP_INIT;
	if (nfssvc_sockhead_flag & SLP_WANTINIT) {
		nfssvc_sockhead_flag &= ~SLP_WANTINIT;
		wakeup((caddr_t)&nfssvc_sockhead);
	}

	TAILQ_INIT(&nfsd_head);
	nfsd_head_flag &= ~NFSD_CHECKSLP;

#if 0
	nfs_udpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
	bzero((caddr_t)nfs_udpsock, sizeof (struct nfssvc_sock));
	STAILQ_INIT(&nfs_udpsock->ns_rec);
	TAILQ_INIT(&nfs_udpsock->ns_uidlruhead);
	TAILQ_INSERT_HEAD(&nfssvc_sockhead, nfs_udpsock, ns_chain);

	nfs_cltpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
	bzero((caddr_t)nfs_cltpsock, sizeof (struct nfssvc_sock));
	STAILQ_INIT(&nfs_cltpsock->ns_rec);
	TAILQ_INIT(&nfs_cltpsock->ns_uidlruhead);
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, nfs_cltpsock, ns_chain);
#endif
}

/*
 * Add entries to the server monitor log.
 */
static void
nfsd_rt(int sotype, struct nfsrv_descript *nd, int cacherep)
{
	struct drt *rt;

	rt = &nfsdrt.drt[nfsdrt.pos];
	if (cacherep == RC_DOIT)
		rt->flag = 0;
	else if (cacherep == RC_REPLY)
		rt->flag = DRT_CACHEREPLY;
	else
		rt->flag = DRT_CACHEDROP;
	if (sotype == SOCK_STREAM)
		rt->flag |= DRT_TCP;
	if (nd->nd_flag & ND_NQNFS)
		rt->flag |= DRT_NQNFS;
	else if (nd->nd_flag & ND_NFSV3)
		rt->flag |= DRT_NFSV3;
	rt->proc = nd->nd_procnum;
	if (nd->nd_nam->sa_family == AF_INET)
	    rt->ipadr = ((struct sockaddr_in *)nd->nd_nam)->sin_addr.s_addr;
	else
	    rt->ipadr = INADDR_ANY;
	rt->resptime = nfs_curusec() - (nd->nd_starttime.tv_sec * 1000000 + nd->nd_starttime.tv_usec);
	getmicrotime(&rt->tstamp);
	nfsdrt.pos = (nfsdrt.pos + 1) % NFSRTTLOGSIZ;
}
#endif /* NFS_NOSERVER */

static int nfs_defect = 0;
SYSCTL_INT(_vfs_nfs, OID_AUTO, defect, CTLFLAG_RW, &nfs_defect, 0, "");

/*
 * Asynchronous I/O daemons for client nfs.
 * They do read-ahead and write-behind operations on the block I/O cache.
 * Never returns unless it fails or gets killed.
 */
static int
nfssvc_iod(struct thread *td)
{
	struct buf *bp;
	int i, myiod;
	struct nfsmount *nmp;
	int error = 0;

	/*
	 * Assign my position or return error if too many already running
	 */
	myiod = -1;
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++)
		if (nfs_asyncdaemon[i] == 0) {
			nfs_asyncdaemon[i]++;
			myiod = i;
			break;
		}
	if (myiod == -1)
		return (EBUSY);
	nfs_numasync++;
	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
	    while (((nmp = nfs_iodmount[myiod]) == NULL
	             || TAILQ_EMPTY(&nmp->nm_bufq))
		   && error == 0) {
		if (nmp)
		    nmp->nm_bufqiods--;
		nfs_iodwant[myiod] = td;
		nfs_iodmount[myiod] = NULL;
		error = tsleep((caddr_t)&nfs_iodwant[myiod],
			PCATCH, "nfsidl", 0);
	    }
	    if (error) {
		nfs_asyncdaemon[myiod] = 0;
		if (nmp)
		    nmp->nm_bufqiods--;
		nfs_iodwant[myiod] = NULL;
		nfs_iodmount[myiod] = NULL;
		nfs_numasync--;
		return (error);
	    }
	    while ((bp = TAILQ_FIRST(&nmp->nm_bufq)) != NULL) {
		/* Take one off the front of the list */
		TAILQ_REMOVE(&nmp->nm_bufq, bp, b_freelist);
		nmp->nm_bufqlen--;
		if (nmp->nm_bufqwant && nmp->nm_bufqlen <= nfs_numasync) {
		    nmp->nm_bufqwant = FALSE;
		    wakeup(&nmp->nm_bufq);
		}
		(void) nfs_doio(bp, NULL);
		/*
		 * If there are more than one iod on this mount, then defect
		 * so that the iods can be shared out fairly between the mounts
		 */
		if (nfs_defect && nmp->nm_bufqiods > 1) {
		    NFS_DPF(ASYNCIO,
			    ("nfssvc_iod: iod %d defecting from mount %p\n",
			     myiod, nmp));
		    nfs_iodmount[myiod] = NULL;
		    nmp->nm_bufqiods--;
		    break;
		}
	    }
	}
}


/*
 * Get an authorization string for the uid by having the mount_nfs sitting
 * on this mount point porpous out of the kernel and do it.
 */
int
nfs_getauth(struct nfsmount *nmp, struct nfsreq *rep,
	    struct ucred *cred, char **auth_str, int *auth_len, char *verf_str,
	    int *verf_len, NFSKERBKEY_T key /* return session key */)
{
	int error = 0;

	while ((nmp->nm_state & NFSSTA_WAITAUTH) == 0) {
		nmp->nm_state |= NFSSTA_WANTAUTH;
		(void) tsleep((caddr_t)&nmp->nm_authtype, 0,
			"nfsauth1", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_td);
		if (error) {
			nmp->nm_state &= ~NFSSTA_WANTAUTH;
			return (error);
		}
	}
	nmp->nm_state &= ~(NFSSTA_WAITAUTH | NFSSTA_WANTAUTH);
	nmp->nm_authstr = *auth_str = (char *)malloc(RPCAUTH_MAXSIZ, M_TEMP, M_WAITOK);
	nmp->nm_authlen = RPCAUTH_MAXSIZ;
	nmp->nm_verfstr = verf_str;
	nmp->nm_verflen = *verf_len;
	nmp->nm_authuid = cred->cr_uid;
	wakeup((caddr_t)&nmp->nm_authstr);

	/*
	 * And wait for mount_nfs to do its stuff.
	 */
	while ((nmp->nm_state & NFSSTA_HASAUTH) == 0 && error == 0) {
		(void) tsleep((caddr_t)&nmp->nm_authlen, 0,
			"nfsauth2", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_td);
	}
	if (nmp->nm_state & NFSSTA_AUTHERR) {
		nmp->nm_state &= ~NFSSTA_AUTHERR;
		error = EAUTH;
	}
	if (error)
		free((caddr_t)*auth_str, M_TEMP);
	else {
		*auth_len = nmp->nm_authlen;
		*verf_len = nmp->nm_verflen;
		bcopy((caddr_t)nmp->nm_key, (caddr_t)key, sizeof (key));
	}
	nmp->nm_state &= ~NFSSTA_HASAUTH;
	nmp->nm_state |= NFSSTA_WAITAUTH;
	if (nmp->nm_state & NFSSTA_WANTAUTH) {
		nmp->nm_state &= ~NFSSTA_WANTAUTH;
		wakeup((caddr_t)&nmp->nm_authtype);
	}
	return (error);
}

/*
 * Get a nickname authenticator and verifier.
 */
int
nfs_getnickauth(struct nfsmount *nmp, struct ucred *cred, char **auth_str,
		int *auth_len, char *verf_str, int verf_len)
{
	struct nfsuid *nuidp;
	u_int32_t *nickp, *verfp;
	struct timeval ktvin, ktvout;

#ifdef DIAGNOSTIC
	if (verf_len < (4 * NFSX_UNSIGNED))
		panic("nfs_getnickauth verf too small");
#endif
	for (nuidp = NMUIDHASH(nmp, cred->cr_uid)->lh_first;
	    nuidp != 0; nuidp = nuidp->nu_hash.le_next) {
		if (nuidp->nu_cr.cr_uid == cred->cr_uid)
			break;
	}
	if (!nuidp || nuidp->nu_expire < time_second)
		return (EACCES);

	/*
	 * Move to the end of the lru list (end of lru == most recently used).
	 */
	TAILQ_REMOVE(&nmp->nm_uidlruhead, nuidp, nu_lru);
	TAILQ_INSERT_TAIL(&nmp->nm_uidlruhead, nuidp, nu_lru);

	nickp = (u_int32_t *)malloc(2 * NFSX_UNSIGNED, M_TEMP, M_WAITOK);
	*nickp++ = txdr_unsigned(RPCAKN_NICKNAME);
	*nickp = txdr_unsigned(nuidp->nu_nickname);
	*auth_str = (char *)nickp;
	*auth_len = 2 * NFSX_UNSIGNED;

	/*
	 * Now we must encrypt the verifier and package it up.
	 */
	verfp = (u_int32_t *)verf_str;
	*verfp++ = txdr_unsigned(RPCAKN_NICKNAME);
	if (time_second > nuidp->nu_timestamp.tv_sec ||
	    (time_second == nuidp->nu_timestamp.tv_sec &&
	     time_second > nuidp->nu_timestamp.tv_usec))
		getmicrotime(&nuidp->nu_timestamp);
	else
		nuidp->nu_timestamp.tv_usec++;
	ktvin.tv_sec = txdr_unsigned(nuidp->nu_timestamp.tv_sec);
	ktvin.tv_usec = txdr_unsigned(nuidp->nu_timestamp.tv_usec);

	/*
	 * Now encrypt the timestamp verifier in ecb mode using the session
	 * key.
	 */
#ifdef NFSKERB
	XXX
#endif

	*verfp++ = ktvout.tv_sec;
	*verfp++ = ktvout.tv_usec;
	*verfp = 0;
	return (0);
}

/*
 * Save the current nickname in a hash list entry on the mount point.
 */
int
nfs_savenickauth(struct nfsmount *nmp, struct ucred *cred, int len,
		 NFSKERBKEY_T key, struct mbuf **mdp, char **dposp,
		 struct mbuf *mrep)
{
	struct nfsuid *nuidp;
	u_int32_t *tl;
	int32_t t1;
	struct mbuf *md = *mdp;
	struct timeval ktvin, ktvout;
	u_int32_t nick;
	char *dpos = *dposp, *cp2;
	int deltasec, error = 0;

	if (len == (3 * NFSX_UNSIGNED)) {
		nfsm_dissect(tl, u_int32_t *, 3 * NFSX_UNSIGNED);
		ktvin.tv_sec = *tl++;
		ktvin.tv_usec = *tl++;
		nick = fxdr_unsigned(u_int32_t, *tl);

		/*
		 * Decrypt the timestamp in ecb mode.
		 */
#ifdef NFSKERB
		XXX
#endif
		ktvout.tv_sec = fxdr_unsigned(long, ktvout.tv_sec);
		ktvout.tv_usec = fxdr_unsigned(long, ktvout.tv_usec);
		deltasec = time_second - ktvout.tv_sec;
		if (deltasec < 0)
			deltasec = -deltasec;
		/*
		 * If ok, add it to the hash list for the mount point.
		 */
		if (deltasec <= NFS_KERBCLOCKSKEW) {
			if (nmp->nm_numuids < nuidhash_max) {
				nmp->nm_numuids++;
				nuidp = (struct nfsuid *)
				   malloc(sizeof (struct nfsuid), M_NFSUID,
					M_WAITOK);
			} else {
				nuidp = TAILQ_FIRST(&nmp->nm_uidlruhead);
				LIST_REMOVE(nuidp, nu_hash);
				TAILQ_REMOVE(&nmp->nm_uidlruhead, nuidp,
					nu_lru);
			}
			nuidp->nu_flag = 0;
			nuidp->nu_cr.cr_uid = cred->cr_uid;
			nuidp->nu_expire = time_second + NFS_KERBTTL;
			nuidp->nu_timestamp = ktvout;
			nuidp->nu_nickname = nick;
			bcopy(key, nuidp->nu_key, sizeof (key));
			TAILQ_INSERT_TAIL(&nmp->nm_uidlruhead, nuidp,
				nu_lru);
			LIST_INSERT_HEAD(NMUIDHASH(nmp, cred->cr_uid),
				nuidp, nu_hash);
		}
	} else
		nfsm_adv(nfsm_rndup(len));
nfsmout:
	*mdp = md;
	*dposp = dpos;
	return (error);
}
