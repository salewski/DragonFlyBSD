/*
 * 
 *             Coda: an Experimental Distributed File System
 *                              Release 3.1
 * 
 *           Copyright (c) 1987-1998 Carnegie Mellon University
 *                          All Rights Reserved
 * 
 * Permission  to  use, copy, modify and distribute this software and its
 * documentation is hereby granted,  provided  that  both  the  copyright
 * notice  and  this  permission  notice  appear  in  all  copies  of the
 * software, derivative works or  modified  versions,  and  any  portions
 * thereof, and that both notices appear in supporting documentation, and
 * that credit is given to Carnegie Mellon University  in  all  documents
 * and publicity pertaining to direct or indirect use of this code or its
 * derivatives.
 * 
 * CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
 * SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
 * FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
 * DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
 * RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
 * ANY DERIVATIVE WORK.
 * 
 * Carnegie  Mellon  encourages  users  of  this  software  to return any
 * improvements or extensions that  they  make,  and  to  grant  Carnegie
 * Mellon the rights to redistribute these changes without encumbrance.
 * 
 * 	@(#) src/sys/coda/coda.h,v 1.1.1.1 1998/08/29 21:14:52 rvb Exp $ 
 * $FreeBSD: src/sys/coda/coda.h,v 1.9 1999/12/29 04:54:30 peter Exp $
 * $DragonFly: src/sys/vfs/coda/Attic/coda.h,v 1.2 2003/06/17 04:28:19 dillon Exp $
 * 
 */


/*
 *
 * Based on cfs.h from Mach, but revamped for increased simplicity.
 * Linux modifications by Peter Braam, Aug 1996
 */

#ifndef _CODA_HEADER_
#define _CODA_HEADER_



/* Catch new _KERNEL defn for NetBSD */
#ifdef __NetBSD__
#include <sys/types.h>
#endif 

#ifndef CODA_MAXSYMLINKS
#define CODA_MAXSYMLINKS 10
#endif

#if defined(DJGPP) || defined(__CYGWIN32__)
#ifdef _KERNEL
typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef u_long ino_t;
typedef u_long dev_t;
typedef void * caddr_t;
#ifdef DOS
typedef unsigned __int64 u_quad_t;
#else 
typedef unsigned long long u_quad_t;
#endif

#define inline

struct timespec {
        long       ts_sec;
        long       ts_nsec;
};
#else  /* DJGPP but not _KERNEL */
#include <sys/types.h>
#include <sys/time.h>
typedef unsigned long long u_quad_t;
#endif /* !_KERNEL */
#endif /* !DJGPP */


#if defined(__linux__)
#define cdev_t u_quad_t
#if !defined(_UQUAD_T_) && (!defined(__GLIBC__) || __GLIBC__ < 2)
#define _UQUAD_T_ 1
typedef unsigned long long u_quad_t;
#endif
#else
#define cdev_t udev_t
#endif

#ifdef __CYGWIN32__
typedef unsigned char u_int8_t;
struct timespec {
        time_t  tv_sec;         /* seconds */
        long    tv_nsec;        /* nanoseconds */
};
#endif


/*
 * Cfs constants
 */
#define CODA_MAXNAMLEN   255
#define CODA_MAXPATHLEN  1024
#define CODA_MAXSYMLINK  10

/* these are Coda's version of O_RDONLY etc combinations
 * to deal with VFS open modes
 */
#define	C_O_READ	0x001
#define	C_O_WRITE       0x002
#define C_O_TRUNC       0x010
#define C_O_EXCL	0x100
#define C_O_CREAT	0x200

/* these are to find mode bits in Venus */ 
#define C_M_READ  00400
#define C_M_WRITE 00200

/* for access Venus will use */
#define C_A_C_OK    8               /* Test for writing upon create.  */
#define C_A_R_OK    4               /* Test for read permission.  */
#define C_A_W_OK    2               /* Test for write permission.  */
#define C_A_X_OK    1               /* Test for execute permission.  */
#define C_A_F_OK    0               /* Test for existence.  */



#ifndef _VENUS_DIRENT_T_
#define _VENUS_DIRENT_T_ 1
struct venus_dirent {
        unsigned long	d_fileno;		/* file number of entry */
        unsigned short	d_reclen;		/* length of this record */
        char 		d_type;			/* file type, see below */
        char		d_namlen;		/* length of string in d_name */
        char		d_name[CODA_MAXNAMLEN + 1];/* name must be no longer than this */
};
#undef DIRSIZ
#define DIRSIZ(dp)      ((sizeof (struct venus_dirent) - (CODA_MAXNAMLEN+1)) + \
                         (((dp)->d_namlen+1 + 3) &~ 3))

/*
 * File types
 */
#define	CDT_UNKNOWN	 0
#define	CDT_FIFO	 1
#define	CDT_CHR		 2
#define	CDT_DIR		 4
#define	CDT_BLK		 6
#define	CDT_REG		 8
#define	CDT_LNK		10
#define	CDT_SOCK	12
#define	CDT_WHT		14

/*
 * Convert between stat structure types and directory types.
 */
#define	IFTOCDT(mode)	(((mode) & 0170000) >> 12)
#define	CDTTOIF(dirtype)	((dirtype) << 12)

#endif

#ifndef	_FID_T_
#define _FID_T_	1
typedef u_long VolumeId;
typedef u_long VnodeId;
typedef u_long Unique_t;
typedef u_long FileVersion;
#endif 

#ifndef	_VICEFID_T_
#define _VICEFID_T_	1
typedef struct ViceFid {
    VolumeId Volume;
    VnodeId Vnode;
    Unique_t Unique;
} ViceFid;
#endif	/* VICEFID */


#ifdef __linux__
static __inline__ ino_t  coda_f2i(struct ViceFid *fid)
{
	if ( ! fid ) 
		return 0; 
	if (fid->Vnode == 0xfffffffe || fid->Vnode == 0xffffffff)
		return ((fid->Volume << 20) | (fid->Unique & 0xfffff));
	else
		return (fid->Unique + (fid->Vnode<<10) + (fid->Volume<<20));
}
	
#else
#define coda_f2i(fid)\
	((fid) ? ((fid)->Unique + ((fid)->Vnode<<10) + ((fid)->Volume<<20)) : 0)
#endif


#ifndef __BIT_TYPES_DEFINED__
#define u_int32_t unsigned int
#endif


#ifndef _VUID_T_
#define _VUID_T_
typedef u_int32_t vuid_t;
typedef u_int32_t vgid_t;
#endif /*_VUID_T_ */

#ifndef _CODACRED_T_
#define _CODACRED_T_
struct coda_cred {
    vuid_t cr_uid, cr_euid, cr_suid, cr_fsuid; /* Real, efftve, set, fs uid*/
    vgid_t cr_groupid,     cr_egid, cr_sgid, cr_fsgid; /* same for groups */
};
#endif 

#ifndef _VENUS_VATTR_T_
#define _VENUS_VATTR_T_
/*
 * Vnode types.  VNON means no type.
 */
enum coda_vtype	{ C_VNON, C_VREG, C_VDIR, C_VBLK, C_VCHR, C_VLNK, C_VSOCK, C_VFIFO, C_VBAD };

struct coda_vattr {
	int     	va_type;	/* vnode type (for create) */
	u_short		va_mode;	/* files access mode and type */
	short		va_nlink;	/* number of references to file */
	vuid_t		va_uid;		/* owner user id */
	vgid_t		va_gid;		/* owner group id */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	cdev_t	        va_rdev;	/* device special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
};

#endif 

/*
 * Kernel <--> Venus communications.
 */

#define CODA_ROOT	2
#define CODA_SYNC	3
#define CODA_OPEN	4
#define CODA_CLOSE	5
#define CODA_IOCTL	6
#define CODA_GETATTR	7
#define CODA_SETATTR	8
#define CODA_ACCESS	9
#define CODA_LOOKUP	10
#define CODA_CREATE	11
#define CODA_REMOVE	12
#define CODA_LINK	13
#define CODA_RENAME	14
#define CODA_MKDIR	15
#define CODA_RMDIR	16
#define CODA_READDIR	17
#define CODA_SYMLINK	18
#define CODA_READLINK	19
#define CODA_FSYNC	20
#define CODA_INACTIVE	21
#define CODA_VGET	22
#define CODA_SIGNAL	23
#define CODA_REPLACE	24
#define CODA_FLUSH       25
#define CODA_PURGEUSER   26
#define CODA_ZAPFILE     27
#define CODA_ZAPDIR      28
#define CODA_PURGEFID    30
#define CODA_OPEN_BY_PATH 31
#define CODA_RESOLVE     32
#define CODA_REINTEGRATE 33
#define CODA_NCALLS 34

#define DOWNCALL(opcode) (opcode >= CODA_REPLACE && opcode <= CODA_PURGEFID)

#define VC_MAXDATASIZE	    8192
#define VC_MAXMSGSIZE      sizeof(union inputArgs)+sizeof(union outputArgs) +\
                            VC_MAXDATASIZE  

#define CIOC_KERNEL_VERSION _IOWR('c', 10, sizeof (int))
#if	0
	/* don't care about kernel version number */
#define CODA_KERNEL_VERSION 0
	/* The old venus 4.6 compatible interface */
#define CODA_KERNEL_VERSION 1
#endif
	/* venus_lookup gets an extra parameter to aid windows.*/
#define CODA_KERNEL_VERSION 2

/*
 *        Venus <-> Coda  RPC arguments
 */
struct coda_in_hdr {
    unsigned long opcode;
    unsigned long unique;	    /* Keep multiple outstanding msgs distinct */
    u_short pid;		    /* Common to all */
    u_short pgid;		    /* Common to all */
    u_short sid;                    /* Common to all */
    struct coda_cred cred;	    /* Common to all */
};

/* Really important that opcode and unique are 1st two fields! */
struct coda_out_hdr {
    unsigned long opcode;
    unsigned long unique;	
    unsigned long result;
};

/* coda_root: NO_IN */
struct coda_root_out {
    struct coda_out_hdr oh;
    ViceFid VFid;
};

struct coda_root_in {
    struct coda_in_hdr in;
};

/* coda_sync: */
/* Nothing needed for coda_sync */

/* coda_open: */
struct coda_open_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct coda_open_out {
    struct coda_out_hdr oh;
    cdev_t	dev;
    ino_t	inode;
};


/* coda_close: */
struct coda_close_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct coda_close_out {
    struct coda_out_hdr out;
};

/* coda_ioctl: */
struct coda_ioctl_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
    int	cmd;
    int	len;
    int	rwflag;
    char *data;			/* Place holder for data. */
};

struct coda_ioctl_out {
    struct coda_out_hdr oh;
    int	len;
    caddr_t	data;		/* Place holder for data. */
};


/* coda_getattr: */
struct coda_getattr_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
};

struct coda_getattr_out {
    struct coda_out_hdr oh;
    struct coda_vattr attr;
};


/* coda_setattr: NO_OUT */
struct coda_setattr_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
    struct coda_vattr attr;
};

struct coda_setattr_out {
    struct coda_out_hdr out;
};

/* coda_access: NO_OUT */
struct coda_access_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct coda_access_out {
    struct coda_out_hdr out;
};


/* lookup flags */
#define CLU_CASE_SENSITIVE     0x01
#define CLU_CASE_INSENSITIVE   0x02

/* coda_lookup: */
struct  coda_lookup_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int         name;		/* Place holder for data. */
    int         flags;	
};

struct coda_lookup_out {
    struct coda_out_hdr oh;
    ViceFid VFid;
    int	vtype;
};


/* coda_create: */
struct coda_create_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
    struct coda_vattr attr;
    int excl;
    int mode;
    int 	name;		/* Place holder for data. */
};

struct coda_create_out {
    struct coda_out_hdr oh;
    ViceFid VFid;
    struct coda_vattr attr;
};


/* coda_remove: NO_OUT */
struct coda_remove_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int name;		/* Place holder for data. */
};

struct coda_remove_out {
    struct coda_out_hdr out;
};

/* coda_link: NO_OUT */
struct coda_link_in {
    struct coda_in_hdr ih;
    ViceFid sourceFid;          /* cnode to link *to* */
    ViceFid destFid;            /* Directory in which to place link */
    int tname;		/* Place holder for data. */
};

struct coda_link_out {
    struct coda_out_hdr out;
};


/* coda_rename: NO_OUT */
struct coda_rename_in {
    struct coda_in_hdr ih;
    ViceFid	sourceFid;
    int 	srcname;
    ViceFid destFid;
    int 	destname;
};

struct coda_rename_out {
    struct coda_out_hdr out;
};

/* coda_mkdir: */
struct coda_mkdir_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    struct coda_vattr attr;
    int	   name;		/* Place holder for data. */
};

struct coda_mkdir_out {
    struct coda_out_hdr oh;
    ViceFid VFid;
    struct coda_vattr attr;
};


/* coda_rmdir: NO_OUT */
struct coda_rmdir_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int name;		/* Place holder for data. */
};

struct coda_rmdir_out {
    struct coda_out_hdr out;
};

/* coda_readdir: */
struct coda_readdir_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	count;
    int	offset;
};

struct coda_readdir_out {
    struct coda_out_hdr oh;
    int	size;
    caddr_t	data;		/* Place holder for data. */
};

/* coda_symlink: NO_OUT */
struct coda_symlink_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;          /* Directory to put symlink in */
    int srcname;
    struct coda_vattr attr;
    int tname;
};

struct coda_symlink_out {
    struct coda_out_hdr out;
};

/* coda_readlink: */
struct coda_readlink_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
};

struct coda_readlink_out {
    struct coda_out_hdr oh;
    int	count;
    caddr_t	data;		/* Place holder for data. */
};


/* coda_fsync: NO_OUT */
struct coda_fsync_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
};

struct coda_fsync_out {
    struct coda_out_hdr out;
};

/* coda_inactive: NO_OUT */
struct coda_inactive_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
};

/* coda_vget: */
struct coda_vget_in {
    struct coda_in_hdr ih;
    ViceFid VFid;
};

struct coda_vget_out {
    struct coda_out_hdr oh;
    ViceFid VFid;
    int	vtype;
};


/* CODA_SIGNAL is out-of-band, doesn't need data. */
/* CODA_INVALIDATE is a venus->kernel call */
/* CODA_FLUSH is a venus->kernel call */

/* coda_purgeuser: */
/* CODA_PURGEUSER is a venus->kernel call */
struct coda_purgeuser_out {
    struct coda_out_hdr oh;
    struct coda_cred cred;
};

/* coda_zapfile: */
/* CODA_ZAPFILE is a venus->kernel call */
struct coda_zapfile_out {  
    struct coda_out_hdr oh;
    ViceFid CodaFid;
};

/* coda_zapdir: */
/* CODA_ZAPDIR is a venus->kernel call */	
struct coda_zapdir_out {	  
    struct coda_out_hdr oh;
    ViceFid CodaFid;
};

/* coda_zapnode: */
/* CODA_ZAPVNODE is a venus->kernel call */	
struct coda_zapvnode_out { 
    struct coda_out_hdr oh;
    struct coda_cred cred;
    ViceFid VFid;
};

/* coda_purgefid: */
/* CODA_PURGEFID is a venus->kernel call */	
struct coda_purgefid_out { 
    struct coda_out_hdr oh;
    ViceFid CodaFid;
};

/* coda_rdwr: */
struct coda_rdwr_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	rwflag;
    int	count;
    int	offset;
    int	ioflag;
    caddr_t	data;		/* Place holder for data. */	
};

struct coda_rdwr_out {
    struct coda_out_hdr oh;
    int	rwflag;
    int	count;
    caddr_t	data;	/* Place holder for data. */
};


/* coda_replace: */
/* CODA_REPLACE is a venus->kernel call */	
struct coda_replace_out { /* coda_replace is a venus->kernel call */
    struct coda_out_hdr oh;
    ViceFid NewFid;
    ViceFid OldFid;
};

/* coda_open_by_path: */
struct coda_open_by_path_in {
    struct coda_in_hdr ih;
    ViceFid	VFid;
    int	flags;
};

struct coda_open_by_path_out {
    struct coda_out_hdr oh;
	int path;
};

/* 
 * Occasionally, we don't cache the fid returned by CODA_LOOKUP. 
 * For instance, if the fid is inconsistent. 
 * This case is handled by setting the top bit of the type result parameter.
 */
#define CODA_NOCACHE          0x80000000

union inputArgs {
    struct coda_in_hdr ih;		/* NB: every struct below begins with an ih */
    struct coda_open_in coda_open;
    struct coda_close_in coda_close;
    struct coda_ioctl_in coda_ioctl;
    struct coda_getattr_in coda_getattr;
    struct coda_setattr_in coda_setattr;
    struct coda_access_in coda_access;
    struct coda_lookup_in coda_lookup;
    struct coda_create_in coda_create;
    struct coda_remove_in coda_remove;
    struct coda_link_in coda_link;
    struct coda_rename_in coda_rename;
    struct coda_mkdir_in coda_mkdir;
    struct coda_rmdir_in coda_rmdir;
    struct coda_readdir_in coda_readdir;
    struct coda_symlink_in coda_symlink;
    struct coda_readlink_in coda_readlink;
    struct coda_fsync_in coda_fsync;
    struct coda_inactive_in coda_inactive;
    struct coda_vget_in coda_vget;
    struct coda_rdwr_in coda_rdwr;
	struct coda_open_by_path_in coda_open_by_path;
};

union outputArgs {
    struct coda_out_hdr oh;		/* NB: every struct below begins with an oh */
    struct coda_root_out coda_root;
    struct coda_open_out coda_open;
    struct coda_ioctl_out coda_ioctl;
    struct coda_getattr_out coda_getattr;
    struct coda_lookup_out coda_lookup;
    struct coda_create_out coda_create;
    struct coda_mkdir_out coda_mkdir;
    struct coda_readdir_out coda_readdir;
    struct coda_readlink_out coda_readlink;
    struct coda_vget_out coda_vget;
    struct coda_purgeuser_out coda_purgeuser;
    struct coda_zapfile_out coda_zapfile;
    struct coda_zapdir_out coda_zapdir;
    struct coda_zapvnode_out coda_zapvnode;
    struct coda_purgefid_out coda_purgefid;
    struct coda_rdwr_out coda_rdwr;
    struct coda_replace_out coda_replace;
	struct coda_open_by_path_out coda_open_by_path;
};    

union coda_downcalls {
    /* CODA_INVALIDATE is a venus->kernel call */
    /* CODA_FLUSH is a venus->kernel call */
    struct coda_purgeuser_out purgeuser;
    struct coda_zapfile_out zapfile;
    struct coda_zapdir_out zapdir;
    struct coda_zapvnode_out zapvnode;
    struct coda_purgefid_out purgefid;
    struct coda_replace_out replace;
};


/*
 * Used for identifying usage of "Control" and pioctls
 */

#define PIOCPARM_MASK 0x0000ffff
struct ViceIoctl {
        caddr_t in, out;        /* Data to be transferred in, or out */
        short in_size;          /* Size of input buffer <= 2K */
        short out_size;         /* Maximum size of output buffer, <= 2K */
};

#if defined(__CYGWIN32__) || defined(DJGPP)
struct PioctlData {
	unsigned long cmd;
        const char *path;
        int follow;
        struct ViceIoctl vi;
};
#else
struct PioctlData {
        const char *path;
        int follow;
        struct ViceIoctl vi;
};
#endif

#define	CODA_CONTROL		".CONTROL"
#define CODA_CONTROLLEN           8
#define	CTL_VOL			-1
#define	CTL_VNO			-1
#define	CTL_UNI			-1
#define CTL_INO                 -1
#define	CTL_FILE		"/coda/.CONTROL"


#define	IS_CTL_FID(fidp)	((fidp)->Volume == CTL_VOL &&\
				 (fidp)->Vnode == CTL_VNO &&\
				 (fidp)->Unique == CTL_UNI)
#endif 

