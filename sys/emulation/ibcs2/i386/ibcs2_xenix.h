/*
 * System call prototypes.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * $DragonFly: src/sys/emulation/ibcs2/i386/Attic/ibcs2_xenix.h,v 1.9 2003/11/20 06:05:29 dillon Exp $
 * created from DragonFly: src/sys/emulation/ibcs2/i386/syscalls.xenix,v 1.3 2003/08/07 21:17:17 dillon Exp 
 */

#ifndef _IBCS2_XENIX_H_
#define	_IBCS2_XENIX_H_

#include <sys/signal.h>

#include <sys/acl.h>

#include <sys/msgport.h>

#include <sys/sysmsg.h>

#define	PAD_(t)	(sizeof(register_t) <= sizeof(t) ? \
		0 : sizeof(register_t) - sizeof(t))

struct	xenix_rdchk_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	fd;	char fd_[PAD_(int)];
};
struct	xenix_chsize_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	fd;	char fd_[PAD_(int)];
	long	size;	char size_[PAD_(long)];
};
struct	xenix_ftime_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	struct timeb *	tp;	char tp_[PAD_(struct timeb *)];
};
struct	xenix_nap_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	millisec;	char millisec_[PAD_(int)];
};
struct	xenix_scoinfo_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	register_t dummy;
};
struct	xenix_eaccess_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	int	flags;	char flags_[PAD_(int)];
};
struct	ibcs2_sigaction_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	sig;	char sig_[PAD_(int)];
	struct ibcs2_sigaction *	act;	char act_[PAD_(struct ibcs2_sigaction *)];
	struct ibcs2_sigaction *	oact;	char oact_[PAD_(struct ibcs2_sigaction *)];
};
struct	ibcs2_sigprocmask_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	how;	char how_[PAD_(int)];
	ibcs2_sigset_t *	set;	char set_[PAD_(ibcs2_sigset_t *)];
	ibcs2_sigset_t *	oset;	char oset_[PAD_(ibcs2_sigset_t *)];
};
struct	ibcs2_sigpending_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	ibcs2_sigset_t *	mask;	char mask_[PAD_(ibcs2_sigset_t *)];
};
struct	ibcs2_sigsuspend_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	ibcs2_sigset_t *	mask;	char mask_[PAD_(ibcs2_sigset_t *)];
};
struct	ibcs2_getgroups_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	gidsetsize;	char gidsetsize_[PAD_(int)];
	ibcs2_gid_t *	gidset;	char gidset_[PAD_(ibcs2_gid_t *)];
};
struct	ibcs2_setgroups_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	gidsetsize;	char gidsetsize_[PAD_(int)];
	ibcs2_gid_t *	gidset;	char gidset_[PAD_(ibcs2_gid_t *)];
};
struct	ibcs2_sysconf_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	name;	char name_[PAD_(int)];
};
struct	ibcs2_pathconf_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	path;	char path_[PAD_(char *)];
	int	name;	char name_[PAD_(int)];
};
struct	ibcs2_fpathconf_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	int	fd;	char fd_[PAD_(int)];
	int	name;	char name_[PAD_(int)];
};
struct	ibcs2_rename_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	char *	from;	char from_[PAD_(char *)];
	char *	to;	char to_[PAD_(char *)];
};
struct	xenix_utsname_args {
#ifdef _KERNEL
	struct sysmsg sysmsg;
#endif
	union usrmsg usrmsg;
	long	addr;	char addr_[PAD_(long)];
};

#ifdef _KERNEL

int	xenix_rdchk (struct xenix_rdchk_args *);
int	xenix_chsize (struct xenix_chsize_args *);
int	xenix_ftime (struct xenix_ftime_args *);
int	xenix_nap (struct xenix_nap_args *);
int	xenix_scoinfo (struct xenix_scoinfo_args *);
int	xenix_eaccess (struct xenix_eaccess_args *);
int	ibcs2_sigaction (struct ibcs2_sigaction_args *);
int	ibcs2_sigprocmask (struct ibcs2_sigprocmask_args *);
int	ibcs2_sigpending (struct ibcs2_sigpending_args *);
int	ibcs2_sigsuspend (struct ibcs2_sigsuspend_args *);
int	ibcs2_getgroups (struct ibcs2_getgroups_args *);
int	ibcs2_setgroups (struct ibcs2_setgroups_args *);
int	ibcs2_sysconf (struct ibcs2_sysconf_args *);
int	ibcs2_pathconf (struct ibcs2_pathconf_args *);
int	ibcs2_fpathconf (struct ibcs2_fpathconf_args *);
int	ibcs2_rename (struct ibcs2_rename_args *);
int	xenix_utsname (struct xenix_utsname_args *);

#endif /* _KERNEL */

#ifdef COMPAT_43


#ifdef _KERNEL


#endif /* _KERNEL */

#endif /* COMPAT_43 */

#undef PAD_

#endif /* !_IBCS2_XENIX_H_ */
