/*
 * Copyright (c) 1989, 1993, 1995
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
 *	@(#)nfs.h	8.4 (Berkeley) 5/1/95
 * $FreeBSD: src/sys/nfs/nfs.h,v 1.53.2.5 2002/02/20 01:35:34 iedowse Exp $
 * $DragonFly: src/sys/vfs/nfs/nfs.h,v 1.10 2005/03/27 23:51:42 dillon Exp $
 */

#ifndef _NFS_NFS_H_
#define _NFS_NFS_H_

#ifdef _KERNEL
#include "opt_nfs.h"
#endif

/*
 * Tunable constants for nfs
 */

#define	NFS_MAXIOVEC	34
#define NFS_TICKINTVL	5		/* Desired time for a tick (msec) */
#define NFS_HZ		(hz / nfs_ticks) /* Ticks/sec */
#define	NFS_TIMEO	(1 * NFS_HZ)	/* Default timeout = 1 second */
#define	NFS_MINTIMEO	(1 * NFS_HZ)	/* Min timeout to use */
#define	NFS_MAXTIMEO	(60 * NFS_HZ)	/* Max timeout to backoff to */
#define	NFS_MINIDEMTIMEO (5 * NFS_HZ)	/* Min timeout for non-idempotent ops*/
#define	NFS_MAXREXMIT	100		/* Stop counting after this many */
#define	NFS_MAXWINDOW	1024		/* Max number of outstanding requests */
#define	NFS_RETRANS	10		/* Num of retrans for soft mounts */
#define	NFS_MAXGRPS	16		/* Max. size of groups list */
#ifndef NFS_MINATTRTIMO
#define	NFS_MINATTRTIMO 3		/* VREG attrib cache timeout in sec */
#endif
#ifndef NFS_MAXATTRTIMO
#define	NFS_MAXATTRTIMO 60
#endif
#ifndef NFS_MINDIRATTRTIMO
#define	NFS_MINDIRATTRTIMO 30		/* VDIR attrib cache timeout in sec */
#endif
#ifndef NFS_MAXDIRATTRTIMO
#define	NFS_MAXDIRATTRTIMO 60
#endif
#define	NFS_WSIZE	8192		/* Def. write data size <= 8192 */
#define	NFS_RSIZE	8192		/* Def. read data size <= 8192 */
#define NFS_READDIRSIZE	8192		/* Def. readdir size */
#define	NFS_DEFRAHEAD	1		/* Def. read ahead # blocks */
#define	NFS_MAXRAHEAD	4		/* Max. read ahead # blocks */
#define	NFS_MAXUIDHASH	64		/* Max. # of hashed uid entries/mp */
#define	NFS_MAXASYNCDAEMON 	20	/* Max. number async_daemons runnable */
#define NFS_MAXGATHERDELAY	100	/* Max. write gather delay (msec) */
#ifndef NFS_GATHERDELAY
#define NFS_GATHERDELAY		10	/* Default write gather delay (msec) */
#endif
#define	NFS_DIRBLKSIZ	4096		/* Must be a multiple of DIRBLKSIZ */
#ifdef _KERNEL
#define	DIRBLKSIZ	512		/* XXX we used to use ufs's DIRBLKSIZ */
#endif

/*
 * Oddballs
 */
#define	NMOD(a)		((a) % nfs_asyncdaemons)
#define NFS_CMPFH(n, f, s) \
	((n)->n_fhsize == (s) && !bcmp((caddr_t)(n)->n_fhp, (caddr_t)(f), (s)))
#define NFS_ISV3(v)	(VFSTONFS((v)->v_mount)->nm_flag & NFSMNT_NFSV3)
#define NFS_SRVMAXDATA(n) \
		(((n)->nd_flag & ND_NFSV3) ? (((n)->nd_nam2) ? \
		 NFS_MAXDGRAMDATA : NFS_MAXDATA) : NFS_V2MAXDATA)

/*
 * XXX
 * The B_INVAFTERWRITE flag should be set to whatever is required by the
 * buffer cache code to say "Invalidate the block after it is written back".
 */
#define	B_INVAFTERWRITE	B_NOCACHE

/*
 * The IO_METASYNC flag should be implemented for local file systems.
 * (Until then, it is nothin at all.)
 */
#ifndef IO_METASYNC
#define IO_METASYNC	0
#endif

/*
 * Arguments to mount NFS
 */
#define NFS_ARGSVERSION	3		/* change when nfs_args changes */
struct nfs_args {
	int		version;	/* args structure version number */
	struct sockaddr	*addr;		/* file server address */
	int		addrlen;	/* length of address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	u_char		*fh;		/* File handle to be mounted */
	int		fhsize;		/* Size, in bytes, of fh */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		readdirsize;	/* readdir size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	int		maxgrouplist;	/* Max. size of group list */
	int		readahead;	/* # of blocks to readahead */
	int		leaseterm;	/* Term (sec) of lease */
	int		deadthresh;	/* Retrans threshold */
	char		*hostname;	/* server's name */
	int		acregmin;	/* cache attrs for reg files min time */
	int		acregmax;	/* cache attrs for reg files max time */
	int		acdirmin;	/* cache attrs for dirs min time */
	int		acdirmax;	/* cache attrs for dirs max time */
};

/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT		0x00000001  /* soft mount (hard is default) */
#define	NFSMNT_WSIZE		0x00000002  /* set write size */
#define	NFSMNT_RSIZE		0x00000004  /* set read size */
#define	NFSMNT_TIMEO		0x00000008  /* set initial timeout */
#define	NFSMNT_RETRANS		0x00000010  /* set number of request retries */
#define	NFSMNT_MAXGRPS		0x00000020  /* set maximum grouplist size */
#define	NFSMNT_INT		0x00000040  /* allow interrupts on hard mount */
#define	NFSMNT_NOCONN		0x00000080  /* Don't Connect the socket */
#define	NFSMNT_NQNFS		0x00000100  /* Use Nqnfs protocol */
#define	NFSMNT_NFSV3		0x00000200  /* Use NFS Version 3 protocol */
#define	NFSMNT_KERB		0x00000400  /* Use Kerberos authentication */
#define	NFSMNT_DUMBTIMR		0x00000800  /* Don't estimate rtt dynamically */
#define	NFSMNT_LEASETERM	0x00001000  /* set lease term (nqnfs) */
#define	NFSMNT_READAHEAD	0x00002000  /* set read ahead */
#define	NFSMNT_DEADTHRESH	0x00004000  /* set dead server retry thresh */
#define	NFSMNT_RESVPORT		0x00008000  /* Allocate a reserved port */
#define	NFSMNT_RDIRPLUS		0x00010000  /* Use Readdirplus for V3 */
#define	NFSMNT_READDIRSIZE	0x00020000  /* Set readdir size */
#define	NFSMNT_ACREGMIN		0x00040000
#define	NFSMNT_ACREGMAX		0x00080000
#define	NFSMNT_ACDIRMIN		0x00100000
#define	NFSMNT_ACDIRMAX		0x00200000

#define NFSSTA_HASWRITEVERF	0x00040000  /* Has write verifier for V3 */
#define NFSSTA_GOTPATHCONF	0x00080000  /* Got the V3 pathconf info */
#define NFSSTA_GOTFSINFO	0x00100000  /* Got the V3 fsinfo */
#define	NFSSTA_MNTD		0x00200000  /* Mnt server for mnt point */
#define	NFSSTA_DISMINPROG	0x00400000  /* Dismount in progress */
#define	NFSSTA_DISMNT		0x00800000  /* Dismounted */
#define	NFSSTA_SNDLOCK		0x01000000  /* Send socket lock */
#define	NFSSTA_WANTSND		0x02000000  /* Want above */
#define	NFSSTA_RCVLOCK		0x04000000  /* Rcv socket lock */
#define	NFSSTA_WANTRCV		0x08000000  /* Want above */
#define	NFSSTA_WAITAUTH		0x10000000  /* Wait for authentication */
#define	NFSSTA_HASAUTH		0x20000000  /* Has authenticator */
#define	NFSSTA_WANTAUTH		0x40000000  /* Wants an authenticator */
#define	NFSSTA_AUTHERR		0x80000000  /* Authentication error */

/*
 * Structures for the nfssvc(2) syscall. Not that anyone but nfsd and mount_nfs
 * should ever try and use it.
 */
struct nfsd_args {
	int	sock;		/* Socket to serve */
	caddr_t	name;		/* Client addr for connection based sockets */
	int	namelen;	/* Length of name */
};

struct nfsd_srvargs {
	struct nfsd	*nsd_nfsd;	/* Pointer to in kernel nfsd struct */
	uid_t		nsd_uid;	/* Effective uid mapped to cred */
	u_int32_t	nsd_haddr;	/* Ip address of client */
	struct ucred	nsd_cr;		/* Cred. uid maps to */
	u_int		nsd_authlen;	/* Length of auth string (ret) */
	u_char		*nsd_authstr;	/* Auth string (ret) */
	u_int		nsd_verflen;	/* and the verfier */
	u_char		*nsd_verfstr;
	struct timeval	nsd_timestamp;	/* timestamp from verifier */
	u_int32_t	nsd_ttl;	/* credential ttl (sec) */
	NFSKERBKEY_T	nsd_key;	/* Session key */
};

struct nfsd_cargs {
	char		*ncd_dirp;	/* Mount dir path */
	uid_t		ncd_authuid;	/* Effective uid */
	int		ncd_authtype;	/* Type of authenticator */
	u_int		ncd_authlen;	/* Length of authenticator string */
	u_char		*ncd_authstr;	/* Authenticator string */
	u_int		ncd_verflen;	/* and the verifier */
	u_char		*ncd_verfstr;
	NFSKERBKEY_T	ncd_key;	/* Session key */
};

/*
 * XXX to allow amd to include nfs.h without nfsproto.h
 */
#ifdef NFS_NPROCS
/*
 * Stats structure
 */
struct nfsstats {
	int	attrcache_hits;
	int	attrcache_misses;
	int	lookupcache_hits;
	int	lookupcache_misses;
	int	direofcache_hits;
	int	direofcache_misses;
	int	biocache_reads;
	int	read_bios;
	int	read_physios;
	int	biocache_writes;
	int	write_bios;
	int	write_physios;
	int	biocache_readlinks;
	int	readlink_bios;
	int	biocache_readdirs;
	int	readdir_bios;
	int	rpccnt[NFS_NPROCS];
	int	rpcretries;
	int	srvrpccnt[NFS_NPROCS];
	int	srvrpc_errs;
	int	srv_errs;
	int	rpcrequests;
	int	rpctimeouts;
	int	rpcunexpected;
	int	rpcinvalid;
	int	srvcache_inproghits;
	int	srvcache_idemdonehits;
	int	srvcache_nonidemdonehits;
	int	srvcache_misses;
	int	srvnqnfs_leases;
	int	srvnqnfs_maxleases;
	int	srvnqnfs_getleases;
	int	srvvop_writes;
	int	accesscache_hits;
	int	accesscache_misses;
};
#endif

/*
 * Flags for nfssvc() system call.
 */
#define	NFSSVC_BIOD	0x002
#define	NFSSVC_NFSD	0x004
#define	NFSSVC_ADDSOCK	0x008
#define	NFSSVC_AUTHIN	0x010
#define	NFSSVC_GOTAUTH	0x040
#define	NFSSVC_AUTHINFAIL 0x080
#define	NFSSVC_MNTD	0x100

/*
 * fs.nfs sysctl(3) identifiers
 */
#define NFS_NFSSTATS	1		/* struct: struct nfsstats */
#define NFS_NFSPRIVPORT	2		/* int: prohibit nfs to resvports */

#define FS_NFS_NAMES { \
		       { 0, 0 }, \
		       { "nfsstats", CTLTYPE_STRUCT }, \
		       { "nfsprivport", CTLTYPE_INT }, \
}

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NFSREQ);
MALLOC_DECLARE(M_NFSDIROFF);
MALLOC_DECLARE(M_NFSRVDESC);
MALLOC_DECLARE(M_NFSUID);
MALLOC_DECLARE(M_NQLEASE);
MALLOC_DECLARE(M_NFSD);
MALLOC_DECLARE(M_NFSBIGFH);
MALLOC_DECLARE(M_NFSHASH);
#endif

#ifdef ZONE_INTERRUPT
extern vm_zone_t nfsmount_zone;
#endif

extern struct callout nfs_timer_handle;

struct uio;
struct buf;
struct vattr;
struct nlookupdata;

/*
 * The set of signals the interrupt an I/O in progress for NFSMNT_INT mounts.
 * What should be in this set is open to debate, but I believe that since
 * I/O system calls on ufs are never interrupted by signals the set should
 * be minimal. My reasoning is that many current programs that use signals
 * such as SIGALRM will not expect file I/O system calls to be interrupted
 * by them and break.
 */
#define	NFSINT_SIGMASK(set) 						\
	(SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) ||	\
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) ||	\
	 SIGISMEMBER(set, SIGQUIT))

/*
 * Socket errors ignored for connectionless sockets??
 * For now, ignore them all
 */
#define	NFSIGNORE_SOERROR(s, e) \
		((e) != EINTR && (e) != ERESTART && (e) != EWOULDBLOCK && \
		((s) & PR_CONNREQUIRED) == 0)

/*
 * Nfs outstanding request list element
 */
struct nfsreq {
	TAILQ_ENTRY(nfsreq) r_chain;
	struct mbuf	*r_mreq;
	struct mbuf	*r_mrep;
	struct mbuf	*r_md;
	caddr_t		r_dpos;
	struct nfsmount *r_nmp;
	struct vnode	*r_vp;
	u_int32_t	r_xid;
	int		r_flags;	/* flags on request, see below */
	int		r_retry;	/* max retransmission count */
	int		r_rexmit;	/* current retrans count */
	int		r_timer;	/* tick counter on reply */
	u_int32_t	r_procnum;	/* NFS procedure number */
	int		r_rtt;		/* RTT for rpc */
	struct thread	*r_td;		/* Thread that did I/O system call */
};

/*
 * Queue head for nfsreq's
 */
extern TAILQ_HEAD(nfs_reqq, nfsreq) nfs_reqq;

/* Flag values for r_flags */
#define R_TIMING	0x0001		/* timing request (in mntp) */
#define R_SENT		0x0002		/* request has been sent */
#define	R_SOFTTERM	0x0004		/* soft mnt, too many retries */
#define	R_INTR		0x0008		/* intr mnt, signal pending */
#define	R_SOCKERR	0x0010		/* Fatal error on socket */
#define	R_TPRINTFMSG	0x0020		/* Did a tprintf msg. */
#define	R_MUSTRESEND	0x0040		/* Must resend request */
#define	R_GETONEREP	0x0080		/* Probe for one reply only */
#define R_MASKTIMER	0x0100		/* Timer should ignore this req */

/*
 * A list of nfssvc_sock structures is maintained with all the sockets
 * that require service by the nfsd.
 * The nfsuid structs hang off of the nfssvc_sock structs in both lru
 * and uid hash lists.
 */
#ifndef NFS_UIDHASHSIZ
#define	NFS_UIDHASHSIZ	29	/* Tune the size of nfssvc_sock with this */
#endif
#define	NUIDHASH(sock, uid) \
	(&(sock)->ns_uidhashtbl[(uid) % NFS_UIDHASHSIZ])
#ifndef NFS_WDELAYHASHSIZ
#define	NFS_WDELAYHASHSIZ 16	/* and with this */
#endif
#define	NWDELAYHASH(sock, f) \
	(&(sock)->ns_wdelayhashtbl[(*((u_int32_t *)(f))) % NFS_WDELAYHASHSIZ])
#ifndef NFS_MUIDHASHSIZ
#define NFS_MUIDHASHSIZ	63	/* Tune the size of nfsmount with this */
#endif
#define	NMUIDHASH(nmp, uid) \
	(&(nmp)->nm_uidhashtbl[(uid) % NFS_MUIDHASHSIZ])
#define	NFSNOHASH(fhsum) \
	(&nfsnodehashtbl[(fhsum) & nfsnodehash])

/*
 * Network address hash list element
 */
union nethostaddr {
	u_int32_t had_inetaddr;
	struct sockaddr *had_nam;
};

struct nfsuid {
	TAILQ_ENTRY(nfsuid) nu_lru;	/* LRU chain */
	LIST_ENTRY(nfsuid) nu_hash;	/* Hash list */
	int		nu_flag;	/* Flags */
	union nethostaddr nu_haddr;	/* Host addr. for dgram sockets */
	struct ucred	nu_cr;		/* Cred uid mapped to */
	int		nu_expire;	/* Expiry time (sec) */
	struct timeval	nu_timestamp;	/* Kerb. timestamp */
	u_int32_t	nu_nickname;	/* Nickname on server */
	NFSKERBKEY_T	nu_key;		/* and session key */
};

#define	nu_inetaddr	nu_haddr.had_inetaddr
#define	nu_nam		nu_haddr.had_nam
/* Bits for nu_flag */
#define	NU_INETADDR	0x1
#define NU_NAM		0x2
#define NU_NETFAM(u)	(((u)->nu_flag & NU_INETADDR) ? AF_INET : AF_ISO)

struct nfsrv_rec {
	STAILQ_ENTRY(nfsrv_rec) nr_link;
	struct sockaddr	*nr_address;
	struct mbuf	*nr_packet;
};

struct nfssvc_sock {
	TAILQ_ENTRY(nfssvc_sock) ns_chain;	/* List of all nfssvc_sock's */
	TAILQ_HEAD(, nfsuid) ns_uidlruhead;
	struct file	*ns_fp;
	struct socket	*ns_so;
	struct sockaddr	*ns_nam;
	struct mbuf	*ns_raw;
	struct mbuf	*ns_rawend;
	STAILQ_HEAD(, nfsrv_rec) ns_rec;
	struct mbuf	*ns_frag;
	int		ns_numrec;
	int		ns_flag;
	int		ns_solock;
	int		ns_cc;
	int		ns_reclen;
	int		ns_numuids;
	u_int32_t	ns_sref;
	LIST_HEAD(, nfsrv_descript) ns_tq;	/* Write gather lists */
	LIST_HEAD(, nfsuid) ns_uidhashtbl[NFS_UIDHASHSIZ];
	LIST_HEAD(nfsrvw_delayhash, nfsrv_descript) ns_wdelayhashtbl[NFS_WDELAYHASHSIZ];
};

/* Bits for "ns_flag" */
#define	SLP_VALID	0x01
#define	SLP_DOREC	0x02
#define	SLP_NEEDQ	0x04
#define	SLP_DISCONN	0x08
#define	SLP_GETSTREAM	0x10
#define	SLP_LASTFRAG	0x20
#define SLP_ALLFLAGS	0xff

extern TAILQ_HEAD(nfssvc_sockhead, nfssvc_sock) nfssvc_sockhead;
extern int nfssvc_sockhead_flag;
#define	SLP_INIT	0x01
#define	SLP_WANTINIT	0x02

/*
 * One of these structures is allocated for each nfsd.
 */
struct nfsd {
	TAILQ_ENTRY(nfsd) nfsd_chain;	/* List of all nfsd's */
	int		nfsd_flag;	/* NFSD_ flags */
	struct nfssvc_sock *nfsd_slp;	/* Current socket */
	int		nfsd_authlen;	/* Authenticator len */
	u_char		nfsd_authstr[RPCAUTH_MAXSIZ]; /* Authenticator data */
	int		nfsd_verflen;	/* and the Verifier */
	u_char		nfsd_verfstr[RPCVERF_MAXSIZ];
	struct thread	*nfsd_td;	/* thread ptr */
	struct nfsrv_descript *nfsd_nd;	/* Associated nfsrv_descript */
};

/* Bits for "nfsd_flag" */
#define	NFSD_WAITING	0x01
#define	NFSD_REQINPROG	0x02
#define	NFSD_NEEDAUTH	0x04
#define	NFSD_AUTHFAIL	0x08

/* Bits for loadattrcache */
#define NFS_LATTR_NOSHRINK	0x01
#define NFS_LATTR_NOMTIMECHECK	0x02

/*
 * This structure is used by the server for describing each request.
 * Some fields are used only when write request gathering is performed.
 */
struct nfsrv_descript {
	u_quad_t		nd_time;	/* Write deadline (usec) */
	off_t			nd_off;		/* Start byte offset */
	off_t			nd_eoff;	/* and end byte offset */
	LIST_ENTRY(nfsrv_descript) nd_hash;	/* Hash list */
	LIST_ENTRY(nfsrv_descript) nd_tq;		/* and timer list */
	LIST_HEAD(,nfsrv_descript) nd_coalesce;	/* coalesced writes */
	struct mbuf		*nd_mrep;	/* Request mbuf list */
	struct mbuf		*nd_md;		/* Current dissect mbuf */
	struct mbuf		*nd_mreq;	/* Reply mbuf list */
	struct sockaddr		*nd_nam;	/* and socket addr */
	struct sockaddr		*nd_nam2;	/* return socket addr */
	caddr_t			nd_dpos;	/* Current dissect pos */
	u_int32_t		nd_procnum;	/* RPC # */
	int			nd_stable;	/* storage type */
	int			nd_flag;	/* nd_flag */
	int			nd_len;		/* Length of this write */
	int			nd_repstat;	/* Reply status */
	u_int32_t		nd_retxid;	/* Reply xid */
	u_int32_t		nd_duration;	/* Lease duration */
	struct timeval		nd_starttime;	/* Time RPC initiated */
	fhandle_t		nd_fh;		/* File handle */
	struct ucred		nd_cr;		/* Credentials */
};

/* Bits for "nd_flag" */
#define	ND_READ		LEASE_READ
#define ND_WRITE	LEASE_WRITE
#define ND_CHECK	0x04
#define ND_LEASE	(ND_READ | ND_WRITE | ND_CHECK)
#define ND_NFSV3	0x08
#define ND_NQNFS	0x10
#define ND_KERBNICK	0x20
#define ND_KERBFULL	0x40
#define ND_KERBAUTH	(ND_KERBNICK | ND_KERBFULL)

extern TAILQ_HEAD(nfsd_head, nfsd) nfsd_head;
extern int nfsd_head_flag;
extern int nfsd_waiting;
#define	NFSD_CHECKSLP	0x01

/*
 * These macros compare nfsrv_descript structures.
 */
#define NFSW_CONTIG(o, n) \
		((o)->nd_eoff >= (n)->nd_off && \
		 !bcmp((caddr_t)&(o)->nd_fh, (caddr_t)&(n)->nd_fh, NFSX_V3FH))

#define NFSW_SAMECRED(o, n) \
	(((o)->nd_flag & ND_KERBAUTH) == ((n)->nd_flag & ND_KERBAUTH) && \
 	 !bcmp((caddr_t)&(o)->nd_cr, (caddr_t)&(n)->nd_cr, \
		sizeof (struct ucred)))

/*
 * Defines for WebNFS
 */

#define WEBNFS_ESC_CHAR		'%'
#define WEBNFS_SPECCHAR_START	0x80

#define WEBNFS_NATIVE_CHAR	0x80
/*
 * ..
 * Possibly more here in the future.
 */

/*
 * Macro for converting escape characters in WebNFS pathnames.
 * Should really be in libkern.
 */

#define HEXTOC(c) \
	((c) >= 'a' ? ((c) - ('a' - 10)) : \
	    ((c) >= 'A' ? ((c) - ('A' - 10)) : ((c) - '0')))
#define HEXSTRTOI(p) \
	((HEXTOC(p[0]) << 4) + HEXTOC(p[1]))

#ifdef NFS_DEBUG

extern int nfs_debug;
#define NFS_DEBUG_ASYNCIO	1 /* asynchronous i/o */
#define NFS_DEBUG_WG		2 /* server write gathering */
#define NFS_DEBUG_RC		4 /* server request caching */

#define NFS_DPF(cat, args)					\
	do {							\
		if (nfs_debug & NFS_DEBUG_##cat) printf args;	\
	} while (0)

#else

#define NFS_DPF(cat, args)

#endif

u_quad_t nfs_curusec (void);
int	nfs_init (struct vfsconf *vfsp);
int	nfs_uninit (struct vfsconf *vfsp);
int	nfs_reply (struct nfsreq *);
int	nfs_getreq (struct nfsrv_descript *,struct nfsd *,int);
int	nfs_send (struct socket *, struct sockaddr *, struct mbuf *, 
		      struct nfsreq *);
int	nfs_rephead (int, struct nfsrv_descript *, struct nfssvc_sock *,
			 int, int, u_quad_t *, struct mbuf **, struct mbuf **,
			 caddr_t *);
int	nfs_sndlock (struct nfsreq *);
void	nfs_sndunlock (struct nfsreq *);
int	nfs_slplock (struct nfssvc_sock *, int);
void	nfs_slpunlock (struct nfssvc_sock *);
int	nfs_disct (struct mbuf **, caddr_t *, int, int, caddr_t *);
int	nfs_vinvalbuf (struct vnode *, int, struct thread *, int);
int	nfs_readrpc (struct vnode *, struct uio *);
int	nfs_writerpc (struct vnode *, struct uio *, int *, int *);
int	nfs_commit (struct vnode *vp, u_quad_t offset, int cnt, 
			struct thread *td);
int	nfs_readdirrpc (struct vnode *, struct uio *);
int	nfs_asyncio (struct buf *, struct thread *);
int	nfs_doio (struct buf *, struct thread *);
int	nfs_readlinkrpc (struct vnode *, struct uio *);
int	nfs_sigintr (struct nfsmount *, struct nfsreq *, struct thread *);
int	nfs_readdirplusrpc (struct vnode *, struct uio *);
int	nfsm_disct (struct mbuf **, caddr_t *, int, int, caddr_t *);
void	nfsm_srvfattr (struct nfsrv_descript *, struct vattr *, 
			   struct nfs_fattr *);
void	nfsm_srvwcc (struct nfsrv_descript *, int, struct vattr *, int,
			 struct vattr *, struct mbuf **, char **);
void	nfsm_srvpostopattr (struct nfsrv_descript *, int, struct vattr *,
				struct mbuf **, char **);
int	netaddr_match (int, union nethostaddr *, struct sockaddr *);
int	nfs_request (struct vnode *, struct mbuf *, int, struct thread *,
			 struct ucred *, struct mbuf **, struct mbuf **,
			 caddr_t *);
int	nfs_loadattrcache (struct vnode **, struct mbuf **, caddr_t *,
			       struct vattr *, int);
int	nfs_namei (struct nlookupdata *, struct ucred *, int, 
		    struct vnode **, struct vnode **, fhandle_t *, int,
		    struct nfssvc_sock *, struct sockaddr *, struct mbuf **,
		    caddr_t *, struct vnode **, struct thread *, int, int);
void	nfsm_adj (struct mbuf *, int, int);
int	nfsm_mbuftouio (struct mbuf **, struct uio *, int, caddr_t *);
void	nfsrv_initcache (void);
int	nfs_getauth (struct nfsmount *, struct nfsreq *, struct ucred *, 
			 char **, int *, char *, int *, NFSKERBKEY_T);
int	nfs_getnickauth (struct nfsmount *, struct ucred *, char **, 
			     int *, char *, int);
int	nfs_savenickauth (struct nfsmount *, struct ucred *, int, 
			      NFSKERBKEY_T, struct mbuf **, char **,
			      struct mbuf *);
int	nfs_adv (struct mbuf **, caddr_t *, int, int);
void	nfs_nhinit (void);
int	nfs_nmcancelreqs (struct nfsmount *);
void	nfs_timer (void*);
int	nfsrv_dorec (struct nfssvc_sock *, struct nfsd *, 
			 struct nfsrv_descript **);
int	nfsrv_getcache (struct nfsrv_descript *, struct nfssvc_sock *,
			    struct mbuf **);
void	nfsrv_updatecache (struct nfsrv_descript *, int, struct mbuf *);
void	nfsrv_cleancache (void);
int	nfs_connect (struct nfsmount *, struct nfsreq *);
void	nfs_disconnect (struct nfsmount *);
void	nfs_safedisconnect (struct nfsmount *);
int	nfs_getattrcache (struct vnode *, struct vattr *);
int	nfsm_strtmbuf (struct mbuf **, char **, const char *, long);
int	nfs_bioread (struct vnode *, struct uio *, int);
int	nfsm_uiotombuf (struct uio *, struct mbuf **, int, caddr_t *);
void	nfsrv_init (int);
void	nfs_clearcommit (struct mount *);
int	nfsrv_errmap (struct nfsrv_descript *, int);
void	nfsrvw_sort (gid_t *, int);
void	nfsrv_setcred (struct ucred *, struct ucred *);
int	nfs_writebp (struct buf *, int, struct thread *);
int	nfsrv_object_create (struct vnode *);
void	nfsrv_wakenfsd (struct nfssvc_sock *slp, int nparallel);
int	nfsrv_writegather (struct nfsrv_descript **, struct nfssvc_sock *,
			       struct thread *, struct mbuf **);
int	nfs_fsinfo (struct nfsmount *, struct vnode *, struct thread *p);

int	nfsrv3_access (struct nfsrv_descript *nfsd, 
			   struct nfssvc_sock *slp,
			   struct thread *td, struct mbuf **mrq);
int	nfsrv_commit (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_create (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_fhtovp (fhandle_t *, int, struct vnode **, struct ucred *,
			  struct nfssvc_sock *, struct sockaddr *, int *, 
			  int, int);
int	nfsrv_setpublicfs (struct mount *, struct netexport *,
			       struct export_args *);
int	nfs_ispublicfh (fhandle_t *);
int	nfsrv_fsinfo (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_getattr (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			   struct thread *td, struct mbuf **mrq);
int	nfsrv_link (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			struct thread *td, struct mbuf **mrq);
int	nfsrv_lookup (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_mkdir (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			 struct thread *td, struct mbuf **mrq);
int	nfsrv_mknod (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			 struct thread *td, struct mbuf **mrq);
int	nfsrv_noop (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			struct thread *td, struct mbuf **mrq);
int	nfsrv_null (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			struct thread *td, struct mbuf **mrq);
int	nfsrv_pathconf (struct nfsrv_descript *nfsd,
			    struct nfssvc_sock *slp, struct thread *td,
			    struct mbuf **mrq);
int	nfsrv_read (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			struct thread *td, struct mbuf **mrq);
int	nfsrv_readdir (struct nfsrv_descript *nfsd, 
			   struct nfssvc_sock *slp,
			   struct thread *td, struct mbuf **mrq);
int	nfsrv_readdirplus (struct nfsrv_descript *nfsd,
			       struct nfssvc_sock *slp, struct thread *td,
			       struct mbuf **mrq);
int	nfsrv_readlink (struct nfsrv_descript *nfsd,
			    struct nfssvc_sock *slp, struct thread *td,
			    struct mbuf **mrq);
int	nfsrv_remove (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_rename (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_rmdir (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			 struct thread *td, struct mbuf **mrq);
int	nfsrv_setattr (struct nfsrv_descript *nfsd, 
			   struct nfssvc_sock *slp,
			   struct thread *td, struct mbuf **mrq);
int	nfsrv_statfs (struct nfsrv_descript *nfsd, 
			  struct nfssvc_sock *slp,
			  struct thread *td, struct mbuf **mrq);
int	nfsrv_symlink (struct nfsrv_descript *nfsd, 
			   struct nfssvc_sock *slp,
			   struct thread *td, struct mbuf **mrq);
int	nfsrv_write (struct nfsrv_descript *nfsd, struct nfssvc_sock *slp,
			 struct thread *td, struct mbuf **mrq);
void	nfsrv_rcv (struct socket *so, void *arg, int waitflag);
void	nfsrv_slpderef (struct nfssvc_sock *slp);
int	nfs_meta_setsize (struct vnode *vp, struct thread *td, u_quad_t nsize);

#endif	/* _KERNEL */

#endif
