/*
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)mount.h	8.21 (Berkeley) 5/20/95
 * $FreeBSD: src/sys/sys/mount.h,v 1.89.2.7 2003/04/04 20:35:57 tegge Exp $
 * $DragonFly: src/sys/sys/mount.h,v 1.17 2005/03/22 22:13:33 dillon Exp $
 */

#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/ucred.h>

#ifndef _KERNEL
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
#include <sys/stat.h>
#endif /* !_POSIX_C_SOURCE */
#endif /* !_KERNEL */

#include <sys/queue.h>
#ifdef _KERNEL
#include <sys/lock.h>
#endif

struct thread;
struct journal;
struct vop_ops;
struct vop_mountctl_args;

typedef struct fsid { int32_t val[2]; } fsid_t;	/* file system id type */

/*
 * File identifier.
 * These are unique per filesystem on a single machine.
 */
#define	MAXFIDSZ	16

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	u_short		fid_reserved;		/* force longword alignment */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * file system statistics
 */

#define MFSNAMELEN	16	/* length of fs type name, including null */
#define	MNAMELEN	80	/* length of buffer for returned name */

struct statfs {
	long	f_spare2;		/* placeholder */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	uid_t	f_owner;		/* user that mounted the filesystem */
	int	f_type;			/* type of filesystem */
	int	f_flags;		/* copy of mount exported flags */
	long    f_syncwrites;		/* count of sync writes since mount */
	long    f_asyncwrites;		/* count of async writes since mount */
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	/* directory on which mounted */
	long    f_syncreads;		/* count of sync reads since mount */
	long    f_asyncreads;		/* count of async reads since mount */
	short	f_spares1;		/* unused spare */
	char	f_mntfromname[MNAMELEN];/* mounted filesystem */
	short	f_spares2;		/* unused spare */
	long    f_spare[2];		/* unused spare */
};

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)
/*
 * Structure per mounted file system.  Each mounted file system has an
 * array of operations and an instance record.  The file systems are
 * put on a doubly linked list.
 *
 * NOTE: mnt_nvnodelist and mnt_reservedvnlist.  At the moment vnodes
 * are linked into mnt_nvnodelist.  At some point in the near future the
 * vnode list will be split into a 'dirty' and 'clean' list. mnt_nvnodelist
 * will become the dirty list and mnt_reservedvnlist will become the 'clean'
 * list.  Filesystem kld's syncing code should remain compatible since
 * they only need to scan the dirty vnode list (nvnodelist -> dirtyvnodelist).
 *
 * NOTE: mnt_fsmanage structures.  These structures are required by the new
 * vnode operations vector abstraction.  Each one contains its own operations
 * vector which is registered just like VNODEOP_SET/vnodeopv_desc except it
 * is done in the mount code rather then on vfs initialization.  This
 * structure is responsible for per-mount management, including vfs threading,
 * journaling, and so forth.
 *
 * NOTE: Any vnode marked VPLACEMARKER is a placemarker and should ALWAYS BE
 * SKIPPED.  NO OTHER FIELDS IN SUCH VNODES ARE VALID.
 *
 * NOTE: All VFSs must at least populate mnt_vn_ops or those VOP ops that
 * only take namecache pointers will not be able to find their operations
 * vector via namecache->nc_mount.
 */
TAILQ_HEAD(vnodelst, vnode);
TAILQ_HEAD(journallst, journal);

struct mount {
	TAILQ_ENTRY(mount) mnt_list;		/* mount list */
	struct vfsops	*mnt_op;		/* operations on fs */
	struct vfsconf	*mnt_vfc;		/* configuration info */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode	*mnt_syncer;		/* syncer vnode */
	struct vnodelst	mnt_nvnodelist;		/* list of vnodes this mount */
	struct lock	mnt_lock;		/* mount structure lock */
	int		mnt_flag;		/* flags shared with user */
	int		mnt_kern_flag;		/* kernel only flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	qaddr_t		mnt_data;		/* private data */
	time_t		mnt_time;		/* last time written*/
	u_int		mnt_iosize_max;		/* max IO request size */
	struct vnodelst	mnt_reservedvnlist;	/* (future) dirty vnode list */
	int		mnt_nvnodelistsize;	/* # of vnodes on this mount */

	/*
	 * ops vectors have a fixed stacking order.  All primary calls run
	 * through mnt_vn_ops.  This field is typically assigned to 
	 * mnt_vn_norm_ops.  If journaling has been enabled this field is
	 * usually assigned to mnt_vn_journal_ops.
	 */
	struct vop_ops	*mnt_vn_use_ops;	/* current ops set */

	struct vop_ops	*mnt_vn_coherency_ops;	/* cache coherency ops */
	struct vop_ops	*mnt_vn_journal_ops;	/* journaling ops */
	struct vop_ops  *mnt_vn_norm_ops;	/* for use by the VFS */
	struct vop_ops	*mnt_vn_spec_ops;	/* for use by the VFS */
	struct vop_ops	*mnt_vn_fifo_ops;	/* for use by the VFS */
	struct namecache *mnt_ncp;		/* NCF_MNTPT ncp */

	struct journallst mnt_jlist;		/* list of active journals */
};

#endif /* _KERNEL || _KERNEL_STRUCTURES */

/*
 * User specifiable flags.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_UNION	0x00000020	/* union with underlying filesystem */
#define	MNT_ASYNC	0x00000040	/* file system written asynchronously */
#define	MNT_SUIDDIR	0x00100000	/* special handling of SUID on dirs */
#define	MNT_SOFTDEP	0x00200000	/* soft updates being done */
#define	MNT_NOSYMFOLLOW	0x00400000	/* do not follow symlinks */
#define	MNT_NOATIME	0x10000000	/* disable update of file access time */
#define	MNT_NOCLUSTERR	0x40000000	/* disable cluster read */
#define	MNT_NOCLUSTERW	0x80000000	/* disable cluster write */

/*
 * NFS export related mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
#define	MNT_DEFEXPORTED	0x00000200	/* exported to the world */
#define	MNT_EXPORTANON	0x00000400	/* use anon uid mapping for everyone */
#define	MNT_EXKERB	0x00000800	/* exported with Kerberos uid mapping */
#define	MNT_EXPUBLIC	0x20000000	/* public export (WebNFS) */

/*
 * Flags set by internal operations,
 * but visible to the user.
 * XXX some of these are not quite right.. (I've never seen the root flag set)
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */
#define	MNT_ROOTFS	0x00004000	/* identifies the root filesystem */
#define	MNT_USER	0x00008000	/* mounted by a user */
#define	MNT_IGNORE	0x00800000	/* do not show entry in df */

/*
 * Mask of flags that are visible to statfs()
 * XXX I think that this could now become (~(MNT_CMDFLAGS))
 * but the 'mount' program may need changing to handle this.
 */
#define	MNT_VISFLAGMASK	(MNT_RDONLY	| MNT_SYNCHRONOUS | MNT_NOEXEC	| \
			MNT_NOSUID	| MNT_NODEV	| MNT_UNION	| \
			MNT_ASYNC	| MNT_EXRDONLY	| MNT_EXPORTED	| \
			MNT_DEFEXPORTED	| MNT_EXPORTANON| MNT_EXKERB	| \
			MNT_LOCAL	| MNT_USER	| MNT_QUOTA	| \
			MNT_ROOTFS	| MNT_NOATIME	| MNT_NOCLUSTERR| \
			MNT_NOCLUSTERW	| MNT_SUIDDIR	| MNT_SOFTDEP	| \
			MNT_IGNORE	| MNT_NOSYMFOLLOW | MNT_EXPUBLIC )
/*
 * External filesystem command modifier flags.
 * Unmount can use the MNT_FORCE flag.
 * XXX These are not STATES and really should be somewhere else.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
#define MNT_CMDFLAGS	(MNT_UPDATE|MNT_DELEXPORT|MNT_RELOAD|MNT_FORCE)
/*
 * Internal filesystem control flags stored in mnt_kern_flag.
 *
 * MNTK_UNMOUNT locks the mount entry so that name lookup cannot proceed
 * past the mount point.  This keeps the subtree stable during mounts
 * and unmounts.
 *
 * MNTK_UNMOUNTF permits filesystems to detect a forced unmount while
 * dounmount() is still waiting to lock the mountpoint. This allows
 * the filesystem to cancel operations that might otherwise deadlock
 * with the unmount attempt (used by NFS).
 */
#define MNTK_UNMOUNTF	0x00000001	/* forced unmount in progress */
#define MNTK_UNMOUNT	0x01000000	/* unmount in progress */
#define	MNTK_MWAIT	0x02000000	/* waiting for unmount to finish */
#define MNTK_WANTRDWR	0x04000000	/* upgrade to read/write requested */

/*
 * Sysctl CTL_VFS definitions.
 *
 * Second level identifier specifies which filesystem. Second level
 * identifier VFS_VFSCONF returns information about all filesystems.
 * Second level identifier VFS_GENERIC is non-terminal.
 */
#define	VFS_VFSCONF		0	/* get configured filesystems */
#define	VFS_GENERIC		0	/* generic filesystem information */
/*
 * Third level identifiers for VFS_GENERIC are given below; third
 * level identifiers for specific filesystems are given in their
 * mount specific header files.
 */
#define VFS_MAXTYPENUM	1	/* int: highest defined filesystem type */
#define VFS_CONF	2	/* struct: vfsconf for filesystem given
				   as next argument */

/*
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_WAIT	1	/* synchronously wait for I/O to complete */
#define MNT_NOWAIT	2	/* start all I/O, but do not wait for it */
#define MNT_LAZY	3	/* push data not written by filesystem syncer */

/*
 * Generic file handle
 */
struct fhandle {
	fsid_t	fh_fsid;	/* File system id of mount point */
	struct	fid fh_fid;	/* File sys specific id */
};
typedef struct fhandle	fhandle_t;

/*
 * Export arguments for local filesystem mount calls.
 */
struct export_args {
	int	ex_flags;		/* export related flags */
	uid_t	ex_root;		/* mapping for root uid */
	struct	ucred ex_anon;		/* mapping for anonymous user */
	struct	sockaddr *ex_addr;	/* net address to which exported */
	int	ex_addrlen;		/* and the net address length */
	struct	sockaddr *ex_mask;	/* mask of valid bits in saddr */
	int	ex_masklen;		/* and the smask length */
	char	*ex_indexfile;		/* index file for WebNFS URLs */
};

/*
 * Structure holding information for a publicly exported filesystem
 * (WebNFS). Currently the specs allow just for one such filesystem.
 */
struct nfs_public {
	int		np_valid;	/* Do we hold valid information */
	fhandle_t	np_handle;	/* Filehandle for pub fs (internal) */
	struct mount	*np_mount;	/* Mountpoint of exported fs */
	char		*np_index;	/* Index file */
};

/*
 * Filesystem configuration information. One of these exists for each
 * type of filesystem supported by the kernel. These are searched at
 * mount time to identify the requested filesystem.
 */
struct vfsconf {
	struct	vfsops *vfc_vfsops;	/* filesystem operations vector */
	char	vfc_name[MFSNAMELEN];	/* filesystem type name */
	int	vfc_typenum;		/* historic filesystem type number */
	int	vfc_refcount;		/* number mounted of this type */
	int	vfc_flags;		/* permanent flags */
	struct	vfsconf *vfc_next;	/* next in list */
};

struct ovfsconf {
	void	*vfc_vfsops;
	char	vfc_name[32];
	int	vfc_index;
	int	vfc_refcount;
	int	vfc_flags;
};

/*
 * NB: these flags refer to IMPLEMENTATION properties, not properties of
 * any actual mounts; i.e., it does not make sense to change the flags.
 */
#define	VFCF_STATIC	0x00010000	/* statically compiled into kernel */
#define	VFCF_NETWORK	0x00020000	/* may get data over the network */
#define	VFCF_READONLY	0x00040000	/* writes are not implemented */
#define VFCF_SYNTHETIC	0x00080000	/* data does not represent real files */
#define	VFCF_LOOPBACK	0x00100000	/* aliases some other mounted FS */
#define	VFCF_UNICODE	0x00200000	/* stores file names as Unicode*/

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_MOUNT);
#endif
extern int maxvfsconf;		/* highest defined filesystem type */
extern int nfs_mount_type;	/* vfc_typenum for nfs, or -1 */
extern struct vfsconf *vfsconf;	/* head of list of filesystem types */

#endif

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

TAILQ_HEAD(mntlist, mount);	/* struct mntlist */

/*
 * Operations supported on mounted file system.
 */
#ifdef __STDC__
struct nlookupdata;
struct nlookupdata;
struct mbuf;
#endif

struct vfsops {
	int	(*vfs_mount)	(struct mount *mp, char *path, caddr_t data,
				    struct thread *td);
	int	(*vfs_start)	(struct mount *mp, int flags,
				    struct thread *td);
	int	(*vfs_unmount)	(struct mount *mp, int mntflags,
				    struct thread *td);
	int	(*vfs_root)	(struct mount *mp, struct vnode **vpp);
	int	(*vfs_quotactl)	(struct mount *mp, int cmds, uid_t uid,
				    caddr_t arg, struct thread *td);
	int	(*vfs_statfs)	(struct mount *mp, struct statfs *sbp,
				    struct thread *td);
	int	(*vfs_sync)	(struct mount *mp, int waitfor,
				    struct thread *td);
	int	(*vfs_vget)	(struct mount *mp, ino_t ino,
				    struct vnode **vpp);
	int	(*vfs_fhtovp)	(struct mount *mp, struct fid *fhp,
				    struct vnode **vpp);
	int	(*vfs_checkexp) (struct mount *mp, struct sockaddr *nam,
				    int *extflagsp, struct ucred **credanonp);
	int	(*vfs_vptofh)	(struct vnode *vp, struct fid *fhp);
	int	(*vfs_init)	(struct vfsconf *);
	int	(*vfs_uninit)	(struct vfsconf *);
	int	(*vfs_extattrctl) (struct mount *mp, int cmd,
					const char *attrname, caddr_t arg,
					struct thread *td);
};

#define VFS_MOUNT(MP, PATH, DATA, P) \
	(*(MP)->mnt_op->vfs_mount)(MP, PATH, DATA, P)
#define VFS_START(MP, FLAGS, P)	  (*(MP)->mnt_op->vfs_start)(MP, FLAGS, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, VPP)	  (*(MP)->mnt_op->vfs_root)(MP, VPP)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  (*(MP)->mnt_op->vfs_statfs)(MP, SBP, P)
#define VFS_SYNC(MP, WAIT, P)	  (*(MP)->mnt_op->vfs_sync)(MP, WAIT, P)
#define VFS_VGET(MP, INO, VPP)	  (*(MP)->mnt_op->vfs_vget)(MP, INO, VPP)
#define VFS_FHTOVP(MP, FIDP, VPP) \
	(*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, VPP)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)
#define VFS_CHECKEXP(MP, NAM, EXFLG, CRED) \
	(*(MP)->mnt_op->vfs_checkexp)(MP, NAM, EXFLG, CRED)
#define VFS_EXTATTRCTL(MP, C, N, A, P) \
	(*(MP)->mnt_op->vfs_extattrctl)(MP, C, N, A, P)

#endif

#ifdef _KERNEL

#include <sys/module.h>

#define VFS_SET(vfsops, fsname, flags) \
	static struct vfsconf fsname ## _vfsconf = {		\
		&vfsops,					\
		#fsname,					\
		-1,						\
		0,						\
		flags,						\
		NULL						\
	};							\
	static moduledata_t fsname ## _mod = {			\
		#fsname,					\
		vfs_modevent,					\
		& fsname ## _vfsconf				\
	};							\
	DECLARE_MODULE(fsname, fsname ## _mod, SI_SUB_VFS, SI_ORDER_MIDDLE)

#endif

#if defined(_KERNEL) || defined(_KERNEL_STRUCTURES)

#include <net/radix.h>

#define	AF_MAX		33	/* XXX */

/*
 * Network address lookup element
 */
struct netcred {
	struct	radix_node netc_rnodes[2];
	int	netc_exflags;
	struct	ucred netc_anon;
};

/*
 * Network export information
 */
struct netexport {
	struct	netcred ne_defexported;		      /* Default export */
	struct	radix_node_head *ne_rtable[AF_MAX+1]; /* Individual exports */
};

#endif

#ifdef _KERNEL

extern	char *mountrootfsname;

/*
 * exported vnode operations
 */
int	dounmount (struct mount *, int, struct thread *);
int	vfs_setpublicfs			    /* set publicly exported fs */
	  (struct mount *, struct netexport *, struct export_args *);
int	vfs_lock (struct mount *);         /* lock a vfs */
void	vfs_msync (struct mount *, int);
void	vfs_unlock (struct mount *);       /* unlock a vfs */
int	vfs_busy (struct mount *, int, struct lwkt_tokref *, struct thread *);
void	vfs_bufstats(void);
int	vfs_export			    /* process mount export info */
	  (struct mount *, struct netexport *, struct export_args *);
struct	netcred *vfs_export_lookup	    /* lookup host in fs export list */
	  (struct mount *, struct netexport *, struct sockaddr *);
int	vfs_allocate_syncvnode (struct mount *);
void	vfs_getnewfsid (struct mount *);
dev_t	vfs_getrootfsid (struct mount *);
struct	mount *vfs_getvfs (fsid_t *);      /* return vfs given fsid */
int	vfs_modevent (module_t, int, void *);
int	vfs_mountedon (struct vnode *);    /* is a vfs mounted on vp */
int	vfs_rootmountalloc (char *, char *, struct mount **);
void	vfs_unbusy (struct mount *, struct thread *);
void	vfs_unmountall (void);
int	vfs_register (struct vfsconf *);
int	vfs_unregister (struct vfsconf *);
extern	struct mntlist mountlist;	    /* mounted filesystem list */
extern	struct lwkt_token mountlist_token;
extern	struct nfs_public nfs_pub;

/* 
 * Declarations for these vfs default operations are located in 
 * kern/vfs_default.c, they should be used instead of making "dummy" 
 * functions or casting entries in the VFS op table to "enopnotsupp()".
 */ 
int	vfs_stdmount (struct mount *mp, char *path, caddr_t data, 
		struct nlookupdata *ndp, struct thread *p);
int	vfs_stdstart (struct mount *mp, int flags, struct thread *p);
int	vfs_stdunmount (struct mount *mp, int mntflags, struct thread *p);
int	vfs_stdroot (struct mount *mp, struct vnode **vpp);
int	vfs_stdquotactl (struct mount *mp, int cmds, uid_t uid,
		caddr_t arg, struct thread *p);
int	vfs_stdstatfs (struct mount *mp, struct statfs *sbp, struct thread *p);
int	vfs_stdsync (struct mount *mp, int waitfor, struct thread *td);
int	vfs_stdvget (struct mount *mp, ino_t ino, struct vnode **vpp);
int	vfs_stdfhtovp (struct mount *mp, struct fid *fhp, struct vnode **vpp);
int	vfs_stdcheckexp (struct mount *mp, struct sockaddr *nam,
	   int *extflagsp, struct ucred **credanonp);
int	vfs_stdvptofh (struct vnode *vp, struct fid *fhp);
int	vfs_stdinit (struct vfsconf *);
int	vfs_stduninit (struct vfsconf *);
int	vfs_stdextattrctl (struct mount *mp, int cmd, const char *attrname,
		caddr_t arg, struct thread *p);
int     journal_mountctl(struct vop_mountctl_args *ap);
void	journal_remove_all_journals(struct mount *mp, int flags);


#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	fstatfs (int, struct statfs *);
int	getfh (const char *, fhandle_t *);
int	getfsstat (struct statfs *, long, int);
int	getmntinfo (struct statfs **, int);
int	mount (const char *, const char *, int, void *);
int	statfs (const char *, struct statfs *);
int	unmount (const char *, int);
int	fhopen (const struct fhandle *, int);
int	fhstat (const struct fhandle *, struct stat *);
int	fhstatfs (const struct fhandle *, struct statfs *);

/* C library stuff */
void	endvfsent (void);
struct	ovfsconf *getvfsbyname (const char *);
struct	ovfsconf *getvfsbytype (int);
struct	ovfsconf *getvfsent (void);
#define	getvfsbyname	new_getvfsbyname
int	new_getvfsbyname (const char *, struct vfsconf *);
void	setvfsent (int);
int	vfsisloadable (const char *);
int	vfsload (const char *);
__END_DECLS

#endif /* _KERNEL */

#endif /* !_SYS_MOUNT_H_ */

