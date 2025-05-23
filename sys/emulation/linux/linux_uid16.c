/*-
 * Copyright (c) 2001  The FreeBSD Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/linux/linux_uid16.c,v 1.4.2.1 2001/10/21 03:57:35 marcel Exp $
 * $DragonFly: src/sys/emulation/linux/linux_uid16.c,v 1.10 2004/11/12 00:09:18 dillon Exp $
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kern_syscall.h>
#include <sys/nlookup.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/thread.h>

#include <arch_linux/linux.h>
#include <arch_linux/linux_proto.h>
#include "linux_util.h"

DUMMY(setfsuid16);
DUMMY(setfsgid16);
DUMMY(getresuid16);
DUMMY(getresgid16);

#define	CAST_NOCHG(x)	((x == 0xFFFF) ? -1 : x)

int
linux_chown16(struct linux_chown16_args *args)
{
	struct nlookupdata nd;
	char *path;
	int error;

	error = linux_copyin_path(args->path, &path, LINUX_PATH_EXISTS);
	if (error)
		return (error);
#ifdef DEBUG
	if (ldebug(chown16))
		printf(ARGS(chown16, "%s, %d, %d"), path, args->uid,
		    args->gid);
#endif
	error = nlookup_init(&nd, path, UIO_SYSSPACE, NLC_FOLLOW);
	if (error == 0) {
		error = kern_chown(&nd, CAST_NOCHG(args->uid),
				    CAST_NOCHG(args->gid));
	}
	nlookup_done(&nd);
	linux_free_path(&path);
	return(error);
}

int
linux_lchown16(struct linux_lchown16_args *args)
{
	struct nlookupdata nd;
	char *path;
	int error;

	error = linux_copyin_path(args->path, &path, LINUX_PATH_EXISTS);
	if (error)
		return (error);
#ifdef DEBUG
	if (ldebug(lchown16))
		printf(ARGS(lchown16, "%s, %d, %d"), path, args->uid,
		    args->gid);
#endif
	error = nlookup_init(&nd, path, UIO_SYSSPACE, 0);
	if (error == 0) {
		error = kern_chown(&nd, CAST_NOCHG(args->uid),
				    CAST_NOCHG(args->gid));
	}
	nlookup_done(&nd);
	linux_free_path(&path);
	return(error);
}

int
linux_setgroups16(struct linux_setgroups16_args *args)
{
	struct proc *p = curproc;
	struct ucred *newcred, *oldcred;
	l_gid16_t linux_gidset[NGROUPS];
	gid_t *bsd_gidset;
	int ngrp, error;

#ifdef DEBUG
	if (ldebug(setgroups16))
		printf(ARGS(setgroups16, "%d, *"), args->gidsetsize);
#endif

	ngrp = args->gidsetsize;
	oldcred = p->p_ucred;

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = suser_cred(oldcred, PRISON_ROOT)) != 0)
		return (error);

	if (ngrp >= NGROUPS)
		return (EINVAL);

	newcred = crdup(oldcred);
	if (ngrp > 0) {
		error = copyin((caddr_t)args->gidset, linux_gidset,
		    ngrp * sizeof(l_gid16_t));
		if (error)
			return (error);

		newcred->cr_ngroups = ngrp + 1;

		bsd_gidset = newcred->cr_groups;
		ngrp--;
		while (ngrp >= 0) {
			bsd_gidset[ngrp + 1] = linux_gidset[ngrp];
			ngrp--;
		}
	}
	else
		newcred->cr_ngroups = 1;

	setsugid();
	p->p_ucred = newcred;
	crfree(oldcred);
	return (0);
}

int
linux_getgroups16(struct linux_getgroups16_args *args)
{
	struct proc *p = curproc;
	struct ucred *cred;
	l_gid16_t linux_gidset[NGROUPS];
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

#ifdef DEBUG
	if (ldebug(getgroups16))
		printf(ARGS(getgroups16, "%d, *"), args->gidsetsize);
#endif

	cred = p->p_ucred;
	bsd_gidset = cred->cr_groups;
	bsd_gidsetsz = cred->cr_ngroups - 1;

	/*
	 * cr_groups[0] holds egid. Returning the whole set
	 * here will cause a duplicate. Exclude cr_groups[0]
	 * to prevent that.
	 */

	if ((ngrp = args->gidsetsize) == 0) {
		args->sysmsg_result = bsd_gidsetsz;
		return (0);
	}

	if (ngrp < bsd_gidsetsz)
		return (EINVAL);

	ngrp = 0;
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	error = copyout(linux_gidset, (caddr_t)args->gidset,
	    ngrp * sizeof(l_gid16_t));
	if (error)
		return (error);

	args->sysmsg_result = ngrp;
	return (0);
}

/*
 * The FreeBSD native getgid(2) and getuid(2) also modify p->p_retval[1]
 * when COMPAT_43 or COMPAT_SUNOS is defined. This globbers registers that
 * are assumed to be preserved. The following lightweight syscalls fixes
 * this. See also linux_getpid(2), linux_getgid(2) and linux_getuid(2) in
 * linux_misc.c
 *
 * linux_getgid16() - MP SAFE
 * linux_getuid16() - MP SAFE
 */

int
linux_getgid16(struct linux_getgid16_args *args)
{
	struct proc *p = curproc;

	args->sysmsg_result = p->p_ucred->cr_rgid;
	return (0);
}

int
linux_getuid16(struct linux_getuid16_args *args)
{
	struct proc *p = curproc;

	args->sysmsg_result = p->p_ucred->cr_ruid;
	return (0);
}

int
linux_getegid16(struct linux_getegid16_args *args)
{
	struct getegid_args bsd;
	int error;

	bsd.sysmsg_result = 0;

	error = getegid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_geteuid16(struct linux_geteuid16_args *args)
{
	struct geteuid_args bsd;
	int error;

	bsd.sysmsg_result = 0;

	error = geteuid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setgid16(struct linux_setgid16_args *args)
{
	struct setgid_args bsd;
	int error;

	bsd.gid = args->gid;
	bsd.sysmsg_result = 0;

	error = setgid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setuid16(struct linux_setuid16_args *args)
{
	struct setuid_args bsd;
	int error;

	bsd.uid = args->uid;
	bsd.sysmsg_result = 0;

	error = setuid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setregid16(struct linux_setregid16_args *args)
{
	struct setregid_args bsd;
	int error;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	bsd.sysmsg_result = 0;

	error = setregid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setreuid16(struct linux_setreuid16_args *args)
{
	struct setreuid_args bsd;
	int error;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	bsd.sysmsg_result = 0;

	error = setreuid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setresgid16(struct linux_setresgid16_args *args)
{
	struct setresgid_args bsd;
	int error;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	bsd.sgid = CAST_NOCHG(args->sgid);
	bsd.sysmsg_result = 0;

	error = setresgid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

int
linux_setresuid16(struct linux_setresuid16_args *args)
{
	struct setresuid_args bsd;
	int error;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	bsd.suid = CAST_NOCHG(args->suid);
	bsd.sysmsg_result = 0;

	error = setresuid(&bsd);
	args->sysmsg_result = bsd.sysmsg_result;
	return(error);
}

