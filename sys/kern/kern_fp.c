/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * 
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $DragonFly: src/sys/kern/kern_fp.c,v 1.9 2004/11/12 00:09:23 dillon Exp $
 */

/*
 * Direct file pointer API functions for in-kernel operations on files.  These
 * functions provide a open/read/write/close like interface within the kernel
 * for operating on files that are not necessarily associated with processes
 * and which do not (typically) have descriptors.
 *
 * FUTURE: file handle conversion routines to support checkpointing, 
 * and additional file operations (ioctl, fcntl).
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/sysproto.h>
#include <sys/conf.h>
#include <sys/filedesc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/nlookup.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <sys/event.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <sys/file2.h>
#include <machine/limits.h>

typedef struct file *file_t;

/*
 * fp_open:
 *
 *	Open a file as specified.  Use O_* flags for flags.
 *
 *	NOTE! O_ROOTCRED not quite working yet, vn_open() asserts that the
 *	cred must match the process's cred. XXX
 *
 *	NOTE! when fp_open() is called from a pure thread, root creds are
 *	used.
 */
int
fp_open(const char *path, int flags, int mode, file_t *fpp)
{
    struct nlookupdata nd;
    struct thread *td;
    struct file *fp;
    int error;

    if ((error = falloc(NULL, fpp, NULL)) != 0)
	return (error);
    fp = *fpp;
    td = curthread;
    if (td->td_proc) {
	if ((flags & O_ROOTCRED) == 0)
	    fsetcred(fp, td->td_proc->p_ucred);
    }
    error = nlookup_init(&nd, path, UIO_SYSSPACE, NLC_LOCKVP);
    flags = FFLAGS(flags);
    if (error == 0)
	error = vn_open(&nd, fp, flags, mode);
    nlookup_done(&nd);
    if (error) {
	fdrop(fp, td);
	*fpp = NULL;
    }
    return(error);
}


/*
 * fp_vpopen():	convert a vnode to a file pointer, call VOP_OPEN() on the
 * the vnode.  The vnode must be refd and locked.
 *
 * On success the vnode's ref is inherited by the file pointer and the caller
 * should not vrele() it, and the vnode is unlocked.
 *
 * On failure the vnode remains locked and refd and the caller is responsible
 * for vput()ing it.
 */
int
fp_vpopen(struct vnode *vp, int flags, file_t *fpp)
{
    struct thread *td;
    struct file *fp;
    int vmode;
    int error;

    td = curthread;

    /*
     * Vnode checks (from vn_open())
     */
    if (vp->v_type == VLNK) {
	error = EMLINK;
	goto bad2;
    }
    if (vp->v_type == VSOCK) {
	error = EOPNOTSUPP;
	goto bad2;
    }
    flags = FFLAGS(flags);
    vmode = 0;
    if (flags & (FWRITE | O_TRUNC)) {
	if (vp->v_type == VDIR) {
	    error = EISDIR;
	    goto bad2;
	}
	error = vn_writechk(vp);
	if (error)
	    goto bad2;
	vmode |= VWRITE;
    }
    if (flags & FREAD)
	vmode |= VREAD;
    if (vmode) {
	error = VOP_ACCESS(vp, vmode, td->td_proc->p_ucred, td);
	if (error)
	    goto bad2;
    }

    /*
     * File pointer setup
     */
    if ((error = falloc(NULL, fpp, NULL)) != 0)
	goto bad2;
    fp = *fpp;
    if ((flags & O_ROOTCRED) == 0 && td->td_proc)
	fsetcred(fp, td->td_proc->p_ucred);
    fp->f_data = (caddr_t)vp;
    fp->f_flag = flags;
    fp->f_ops = &vnode_fileops;
    fp->f_type = DTYPE_VNODE;

    error = VOP_OPEN(vp, flags, td->td_proc->p_ucred, fp, td);
    if (error)
	goto bad1;

    /*
     * Make sure that a VM object is created for VMIO support.
     */
    if (vn_canvmio(vp) == TRUE) {
	if ((error = vfs_object_create(vp, td)) != 0) {
	    VOP_CLOSE(vp, flags, td);
	    goto bad1;
	}
    }

    /*
     * All done, update v_writecount now that no more errors can occur.
     */
    if (flags & FWRITE)
	vp->v_writecount++;
    VOP_UNLOCK(vp, 0, td);
    return (0);
bad1:
    fp->f_ops = &badfileops;	/* open failed, don't close */
    fp->f_data = NULL;
    fdrop(fp, td);
    /* leave the vnode intact, but fall through and unlock it anyway */
bad2:
    *fpp = NULL;
    return (error);
}

/*
 * fp_*read() is meant to operate like the normal descriptor based syscalls
 * would.  Note that if 'buf' points to user memory a UIO_USERSPACE
 * transfer will be used.
 */
int
fp_pread(file_t fp, void *buf, size_t nbytes, off_t offset, ssize_t *res)
{
    struct uio auio;
    struct iovec aiov;
    size_t count;
    int error;

    if (res)
	*res = 0;
    if (nbytes > INT_MAX)
	return (EINVAL);
    bzero(&auio, sizeof(auio));
    aiov.iov_base = (caddr_t)buf;
    aiov.iov_len = nbytes;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = offset;
    auio.uio_resid = nbytes;
    auio.uio_rw = UIO_READ;
    if ((vm_offset_t)buf < VM_MAXUSER_ADDRESS)
	auio.uio_segflg = UIO_USERSPACE;
    else
	auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_td = curthread;

    count = nbytes;
    error = fo_read(fp, &auio, fp->f_cred, FOF_OFFSET, auio.uio_td);
    if (error) {
	if (auio.uio_resid != nbytes && (error == ERESTART || error == EINTR ||
	    error == EWOULDBLOCK)
	) {
	    error = 0;
	}
    }
    count -= auio.uio_resid;
    if (res)
	*res = count;
    return(error);
}

int
fp_read(file_t fp, void *buf, size_t nbytes, ssize_t *res)
{
    struct uio auio;
    struct iovec aiov;
    size_t count;
    int error;

    if (res)
	*res = 0;
    if (nbytes > INT_MAX)
	return (EINVAL);
    bzero(&auio, sizeof(auio));
    aiov.iov_base = (caddr_t)buf;
    aiov.iov_len = nbytes;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = 0;
    auio.uio_resid = nbytes;
    auio.uio_rw = UIO_READ;
    if ((vm_offset_t)buf < VM_MAXUSER_ADDRESS)
	auio.uio_segflg = UIO_USERSPACE;
    else
	auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_td = curthread;

    count = nbytes;
    error = fo_read(fp, &auio, fp->f_cred, 0, auio.uio_td);
    if (error) {
	if (auio.uio_resid != nbytes && (error == ERESTART || error == EINTR ||
	    error == EWOULDBLOCK)
	) {
	    error = 0;
	}
    }
    count -= auio.uio_resid;
    if (res)
	*res = count;
    return(error);
}

int
fp_pwrite(file_t fp, void *buf, size_t nbytes, off_t offset, ssize_t *res)
{
    struct uio auio;
    struct iovec aiov;
    size_t count;
    int error;

    if (res)
	*res = 0;
    if (nbytes > INT_MAX)
	return (EINVAL);
    bzero(&auio, sizeof(auio));
    aiov.iov_base = (caddr_t)buf;
    aiov.iov_len = nbytes;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = offset;
    auio.uio_resid = nbytes;
    auio.uio_rw = UIO_WRITE;
    if ((vm_offset_t)buf < VM_MAXUSER_ADDRESS)
	auio.uio_segflg = UIO_USERSPACE;
    else
	auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_td = curthread;

    count = nbytes;
    error = fo_write(fp, &auio, fp->f_cred, FOF_OFFSET, auio.uio_td);
    if (error) {
	if (auio.uio_resid != nbytes && (error == ERESTART || error == EINTR ||
	    error == EWOULDBLOCK)
	) {
	    error = 0;
	}
    }
    count -= auio.uio_resid;
    if (res)
	*res = count;
    return(error);
}


int
fp_write(file_t fp, void *buf, size_t nbytes, ssize_t *res)
{
    struct uio auio;
    struct iovec aiov;
    size_t count;
    int error;

    if (res)
	*res = 0;
    if (nbytes > INT_MAX)
	return (EINVAL);
    bzero(&auio, sizeof(auio));
    aiov.iov_base = (caddr_t)buf;
    aiov.iov_len = nbytes;
    auio.uio_iov = &aiov;
    auio.uio_iovcnt = 1;
    auio.uio_offset = 0;
    auio.uio_resid = nbytes;
    auio.uio_rw = UIO_WRITE;
    if ((vm_offset_t)buf < VM_MAXUSER_ADDRESS)
	auio.uio_segflg = UIO_USERSPACE;
    else
	auio.uio_segflg = UIO_SYSSPACE;
    auio.uio_td = curthread;

    count = nbytes;
    error = fo_write(fp, &auio, fp->f_cred, 0, auio.uio_td);
    if (error) {
	if (auio.uio_resid != nbytes && (error == ERESTART || error == EINTR ||
	    error == EWOULDBLOCK)
	) {
	    error = 0;
	}
    }
    count -= auio.uio_resid;
    if (res)
	*res = count;
    return(error);
}

int
fp_stat(file_t fp, struct stat *ub)
{
    int error;

    error = fo_stat(fp, ub, curthread);
    return(error);
}

/*
 * non-anonymous, non-stack descriptor mappings only!
 *
 * This routine mostly snarfed from vm/vm_mmap.c
 */
int
fp_mmap(void *addr_arg, size_t size, int prot, int flags, struct file *fp,
    off_t pos, void **resp)
{
    struct thread *td = curthread;
    struct proc *p = td->td_proc;
    vm_size_t pageoff;
    vm_prot_t maxprot;
    vm_offset_t addr;
    void *handle;
    int error;
    vm_object_t obj;
    struct vmspace *vms = p->p_vmspace;
    struct vnode *vp;
    int disablexworkaround;

    prot &= VM_PROT_ALL;

    if ((ssize_t)size < 0 || (flags & MAP_ANON))
	return(EINVAL);

    pageoff = (pos & PAGE_MASK);
    pos -= pageoff;

    /* Adjust size for rounding (on both ends). */
    size += pageoff;				/* low end... */
    size = (vm_size_t)round_page(size);		/* hi end */
    addr = (vm_offset_t)addr_arg;

    /*
     * Check for illegal addresses.  Watch out for address wrap... Note
     * that VM_*_ADDRESS are not constants due to casts (argh).
     */
    if (flags & MAP_FIXED) {
	/*
	 * The specified address must have the same remainder
	 * as the file offset taken modulo PAGE_SIZE, so it
	 * should be aligned after adjustment by pageoff.
	 */
	addr -= pageoff;
	if (addr & PAGE_MASK)
	    return (EINVAL);
	/* Address range must be all in user VM space. */
	if (VM_MAXUSER_ADDRESS > 0 && addr + size > VM_MAXUSER_ADDRESS)
	    return (EINVAL);
#ifndef i386
	if (VM_MIN_ADDRESS > 0 && addr < VM_MIN_ADDRESS)
	    return (EINVAL);
#endif
	if (addr + size < addr)
	    return (EINVAL);
    } else if (addr == 0 ||
	(addr >= round_page((vm_offset_t)vms->vm_taddr) &&
	 addr < round_page((vm_offset_t)vms->vm_daddr + maxdsiz))
    ) {
	/*
	 * XXX for non-fixed mappings where no hint is provided or
	 * the hint would fall in the potential heap space,
	 * place it after the end of the largest possible heap.
	 *
	 * There should really be a pmap call to determine a reasonable
	 * location.
	 */
	addr = round_page((vm_offset_t)vms->vm_daddr + maxdsiz);
    }

    /*
     * Mapping file, get fp for validation. Obtain vnode and make
     * sure it is of appropriate type.
     */
    if (fp->f_type != DTYPE_VNODE)
	return (EINVAL);

    /*
     * POSIX shared-memory objects are defined to have
     * kernel persistence, and are not defined to support
     * read(2)/write(2) -- or even open(2).  Thus, we can
     * use MAP_ASYNC to trade on-disk coherence for speed.
     * The shm_open(3) library routine turns on the FPOSIXSHM
     * flag to request this behavior.
     */
    if (fp->f_flag & FPOSIXSHM)
	flags |= MAP_NOSYNC;
    vp = (struct vnode *) fp->f_data;
    if (vp->v_type != VREG && vp->v_type != VCHR)
	return (EINVAL);

    /*
     * Get the proper underlying object
     */
    if (vp->v_type == VREG) {
	if (VOP_GETVOBJECT(vp, &obj) != 0)
	    return (EINVAL);
	vp = (struct vnode*)obj->handle;
    }

    /*
     * XXX hack to handle use of /dev/zero to map anon memory (ala
     * SunOS).
     */
    if (vp->v_type == VCHR && iszerodev(vp->v_rdev)) {
	handle = NULL;
	maxprot = VM_PROT_ALL;
	flags |= MAP_ANON;
	pos = 0;
    } else {
	/*
	 * cdevs does not provide private mappings of any kind.
	 */
	/*
	 * However, for XIG X server to continue to work,
	 * we should allow the superuser to do it anyway.
	 * We only allow it at securelevel < 1.
	 * (Because the XIG X server writes directly to video
	 * memory via /dev/mem, it should never work at any
	 * other securelevel.
	 * XXX this will have to go
	 */
	if (securelevel >= 1)
	    disablexworkaround = 1;
	else
	    disablexworkaround = suser(td);
	if (vp->v_type == VCHR && disablexworkaround &&
	    (flags & (MAP_PRIVATE|MAP_COPY))) {
		error = EINVAL;
		goto done;
	}
	/*
	 * Ensure that file and memory protections are
	 * compatible.  Note that we only worry about
	 * writability if mapping is shared; in this case,
	 * current and max prot are dictated by the open file.
	 * XXX use the vnode instead?  Problem is: what
	 * credentials do we use for determination? What if
	 * proc does a setuid?
	 */
	maxprot = VM_PROT_EXECUTE;	/* ??? */
	if (fp->f_flag & FREAD) {
	    maxprot |= VM_PROT_READ;
	} else if (prot & PROT_READ) {
	    error = EACCES;
	    goto done;
	}
	/*
	 * If we are sharing potential changes (either via
	 * MAP_SHARED or via the implicit sharing of character
	 * device mappings), and we are trying to get write
	 * permission although we opened it without asking
	 * for it, bail out.  Check for superuser, only if
	 * we're at securelevel < 1, to allow the XIG X server
	 * to continue to work.
	 */

	if ((flags & MAP_SHARED) != 0 ||
	    (vp->v_type == VCHR && disablexworkaround)
	) {
	    if ((fp->f_flag & FWRITE) != 0) {
		struct vattr va;
		if ((error = VOP_GETATTR(vp, &va, td))) {
		    goto done;
		}
		if ((va.va_flags & (IMMUTABLE|APPEND)) == 0) {
		    maxprot |= VM_PROT_WRITE;
		} else if (prot & PROT_WRITE) {
		    error = EPERM;
		    goto done;
		}
	    } else if ((prot & PROT_WRITE) != 0) {
		error = EACCES;
		goto done;
	    }
	} else {
	    maxprot |= VM_PROT_WRITE;
	}
	handle = (void *)vp;
    }
    error = vm_mmap(&vms->vm_map, &addr, size, prot, 
		    maxprot, flags, handle, pos);
    if (error == 0 && addr_arg)
	*resp = (void *)addr;
done:
    return (error);
}

int
fp_close(file_t fp)
{
    return(fdrop(fp, curthread));
}

