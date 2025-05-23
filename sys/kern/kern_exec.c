/*
 * Copyright (c) 1993, David Greenman
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
 * $FreeBSD: src/sys/kern/kern_exec.c,v 1.107.2.15 2002/07/30 15:40:46 nectar Exp $
 * $DragonFly: src/sys/kern/kern_exec.c,v 1.31 2005/03/02 18:42:08 hmp Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/kern_syscall.h>
#include <sys/wait.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/pioctl.h>
#include <sys/nlookup.h>
#include <sys/sfbuf.h>
#include <sys/sysent.h>
#include <sys/shm.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/aio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <sys/user.h>
#include <machine/reg.h>

#include <sys/thread2.h>

MALLOC_DEFINE(M_PARGS, "proc-args", "Process arguments");

static register_t *exec_copyout_strings (struct image_params *);

/* XXX This should be vm_size_t. */
static u_long ps_strings = PS_STRINGS;
SYSCTL_ULONG(_kern, KERN_PS_STRINGS, ps_strings, CTLFLAG_RD, &ps_strings, 0, "");

/* XXX This should be vm_size_t. */
static u_long usrstack = USRSTACK;
SYSCTL_ULONG(_kern, KERN_USRSTACK, usrstack, CTLFLAG_RD, &usrstack, 0, "");

u_long ps_arg_cache_limit = PAGE_SIZE / 16;
SYSCTL_LONG(_kern, OID_AUTO, ps_arg_cache_limit, CTLFLAG_RW, 
    &ps_arg_cache_limit, 0, "");

int ps_argsopen = 1;
SYSCTL_INT(_kern, OID_AUTO, ps_argsopen, CTLFLAG_RW, &ps_argsopen, 0, "");

void print_execve_args(struct image_args *args);
int debug_execve_args = 0;
SYSCTL_INT(_kern, OID_AUTO, debug_execve_args, CTLFLAG_RW, &debug_execve_args,
    0, "");

void
print_execve_args(struct image_args *args)
{
	char *cp;
	int ndx;

	cp = args->begin_argv;
	for (ndx = 0; ndx < args->argc; ndx++) {
		printf("\targv[%d]: %s\n", ndx, cp);
		while (*cp++ != '\0');
	}
	for (ndx = 0; ndx < args->envc; ndx++) {
		printf("\tenvv[%d]: %s\n", ndx, cp);
		while (*cp++ != '\0');
	}
}

/*
 * Each of the items is a pointer to a `const struct execsw', hence the
 * double pointer here.
 */
static const struct execsw **execsw;

int
kern_execve(struct nlookupdata *nd, struct image_args *args)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	register_t *stack_base;
	int error, len, i;
	struct image_params image_params, *imgp;
	struct vattr attr;
	int (*img_first) (struct image_params *);

	if (debug_execve_args) {
		printf("%s()\n", __func__);
		print_execve_args(args);
	}

	KKASSERT(p);
	imgp = &image_params;

	/*
	 * Lock the process and set the P_INEXEC flag to indicate that
	 * it should be left alone until we're done here.  This is
	 * necessary to avoid race conditions - e.g. in ptrace() -
	 * that might allow a local user to illicitly obtain elevated
	 * privileges.
	 */
	p->p_flag |= P_INEXEC;

	/*
	 * Initialize part of the common data
	 */
	imgp->proc = p;
	imgp->args = args;
	imgp->attr = &attr;
	imgp->entry_addr = 0;
	imgp->resident = 0;
	imgp->vmspace_destroyed = 0;
	imgp->interpreted = 0;
	imgp->interpreter_name[0] = 0;
	imgp->auxargs = NULL;
	imgp->vp = NULL;
	imgp->firstpage = NULL;
	imgp->ps_strings = 0;
	imgp->image_header = NULL;

interpret:

	/*
	 * Translate the file name to a vnode.  Unlock the cache entry to
	 * improve parallelism for programs exec'd in parallel.
	 */
	if ((error = nlookup(nd)) != 0)
		goto exec_fail;
	error = cache_vget(nd->nl_ncp, nd->nl_cred, LK_EXCLUSIVE, &imgp->vp);
	KKASSERT(nd->nl_flags & NLC_NCPISLOCKED);
	nd->nl_flags &= ~NLC_NCPISLOCKED;
	cache_unlock(nd->nl_ncp);
	if (error)
		goto exec_fail;

	/*
	 * Check file permissions (also 'opens' file)
	 */
	error = exec_check_permissions(imgp);
	if (error) {
		VOP_UNLOCK(imgp->vp, 0, td);
		goto exec_fail_dealloc;
	}

	error = exec_map_first_page(imgp);
	VOP_UNLOCK(imgp->vp, 0, td);
	if (error)
		goto exec_fail_dealloc;

	if (debug_execve_args && imgp->interpreted) {
		printf("    target is interpreted -- recursive pass\n");
		printf("    interpreter: %s\n", imgp->interpreter_name);
		print_execve_args(args);
	}

	/*
	 *	If the current process has a special image activator it
	 *	wants to try first, call it.   For example, emulating shell 
	 *	scripts differently.
	 */
	error = -1;
	if ((img_first = imgp->proc->p_sysent->sv_imgact_try) != NULL)
		error = img_first(imgp);

	/*
	 *	If the vnode has a registered vmspace, exec the vmspace
	 */
	if (error == -1 && imgp->vp->v_resident) {
		error = exec_resident_imgact(imgp);
	}

	/*
	 *	Loop through the list of image activators, calling each one.
	 *	An activator returns -1 if there is no match, 0 on success,
	 *	and an error otherwise.
	 */
	for (i = 0; error == -1 && execsw[i]; ++i) {
		if (execsw[i]->ex_imgact == NULL ||
		    execsw[i]->ex_imgact == img_first) {
			continue;
		}
		error = (*execsw[i]->ex_imgact)(imgp);
	}

	if (error) {
		if (error == -1)
			error = ENOEXEC;
		goto exec_fail_dealloc;
	}

	/*
	 * Special interpreter operation, cleanup and loop up to try to
	 * activate the interpreter.
	 */
	if (imgp->interpreted) {
		exec_unmap_first_page(imgp);
		nlookup_done(nd);
		vrele(imgp->vp);
		imgp->vp = NULL;
		error = nlookup_init(nd, imgp->interpreter_name, UIO_SYSSPACE,
					NLC_FOLLOW);
		if (error)
			goto exec_fail;
		goto interpret;
	}

	/*
	 * Copy out strings (args and env) and initialize stack base
	 */
	stack_base = exec_copyout_strings(imgp);
	p->p_vmspace->vm_minsaddr = (char *)stack_base;

	/*
	 * If custom stack fixup routine present for this process
	 * let it do the stack setup.  If we are running a resident
	 * image there is no auxinfo or other image activator context
	 * so don't try to add fixups to the stack.
	 *
	 * Else stuff argument count as first item on stack
	 */
	if (p->p_sysent->sv_fixup && imgp->resident == 0)
		(*p->p_sysent->sv_fixup)(&stack_base, imgp);
	else
		suword(--stack_base, imgp->args->argc);

	/*
	 * For security and other reasons, the file descriptor table cannot
	 * be shared after an exec.
	 */
	if (p->p_fd->fd_refcnt > 1) {
		struct filedesc *tmp;

		tmp = fdcopy(p);
		fdfree(p);
		p->p_fd = tmp;
	}

	/*
	 * For security and other reasons, signal handlers cannot
	 * be shared after an exec. The new proces gets a copy of the old
	 * handlers. In execsigs(), the new process will have its signals
	 * reset.
	 */
	if (p->p_procsig->ps_refcnt > 1) {
		struct procsig *newprocsig;

		MALLOC(newprocsig, struct procsig *, sizeof(struct procsig),
		       M_SUBPROC, M_WAITOK);
		bcopy(p->p_procsig, newprocsig, sizeof(*newprocsig));
		p->p_procsig->ps_refcnt--;
		p->p_procsig = newprocsig;
		p->p_procsig->ps_refcnt = 1;
		if (p->p_sigacts == &p->p_addr->u_sigacts)
			panic("shared procsig but private sigacts?");

		p->p_addr->u_sigacts = *p->p_sigacts;
		p->p_sigacts = &p->p_addr->u_sigacts;
	}

	/* Stop profiling */
	stopprofclock(p);

	/* close files on exec */
	fdcloseexec(p);

	/* reset caught signals */
	execsigs(p);

	/* name this process - nameiexec(p, ndp) */
	len = min(nd->nl_ncp->nc_nlen, MAXCOMLEN);
	bcopy(nd->nl_ncp->nc_name, p->p_comm, len);
	p->p_comm[len] = 0;

	/*
	 * mark as execed, wakeup the process that vforked (if any) and tell
	 * it that it now has its own resources back
	 */
	p->p_flag |= P_EXEC;
	if (p->p_pptr && (p->p_flag & P_PPWAIT)) {
		p->p_flag &= ~P_PPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}

	/*
	 * Implement image setuid/setgid.
	 *
	 * Don't honor setuid/setgid if the filesystem prohibits it or if
	 * the process is being traced.
	 */
	if ((((attr.va_mode & VSUID) && p->p_ucred->cr_uid != attr.va_uid) ||
	     ((attr.va_mode & VSGID) && p->p_ucred->cr_gid != attr.va_gid)) &&
	    (imgp->vp->v_mount->mnt_flag & MNT_NOSUID) == 0 &&
	    (p->p_flag & P_TRACED) == 0) {
		/*
		 * Turn off syscall tracing for set-id programs, except for
		 * root.  Record any set-id flags first to make sure that
		 * we do not regain any tracing during a possible block.
		 */
		setsugid();
		if (p->p_tracep && suser(td)) {
			struct vnode *vtmp;

			if ((vtmp = p->p_tracep) != NULL) {
				p->p_tracep = NULL;
				p->p_traceflag = 0;
				vrele(vtmp);
			}
		}
		/* Close any file descriptors 0..2 that reference procfs */
		setugidsafety(p);
		/* Make sure file descriptors 0..2 are in use. */
		error = fdcheckstd(p);
		if (error != 0)
			goto exec_fail_dealloc;
		/*
		 * Set the new credentials.
		 */
		cratom(&p->p_ucred);
		if (attr.va_mode & VSUID)
			change_euid(attr.va_uid);
		if (attr.va_mode & VSGID)
			p->p_ucred->cr_gid = attr.va_gid;

		/*
		 * Clear local varsym variables
		 */
		varsymset_clean(&p->p_varsymset);
	} else {
		if (p->p_ucred->cr_uid == p->p_ucred->cr_ruid &&
		    p->p_ucred->cr_gid == p->p_ucred->cr_rgid)
			p->p_flag &= ~P_SUGID;
	}

	/*
	 * Implement correct POSIX saved-id behavior.
	 */
	if (p->p_ucred->cr_svuid != p->p_ucred->cr_uid ||
	    p->p_ucred->cr_svgid != p->p_ucred->cr_gid) {
		cratom(&p->p_ucred);
		p->p_ucred->cr_svuid = p->p_ucred->cr_uid;
		p->p_ucred->cr_svgid = p->p_ucred->cr_gid;
	}

	/*
	 * Store the vp for use in procfs
	 */
	if (p->p_textvp)		/* release old reference */
		vrele(p->p_textvp);
	p->p_textvp = imgp->vp;
	vref(p->p_textvp);

        /*
         * Notify others that we exec'd, and clear the P_INEXEC flag
         * as we're now a bona fide freshly-execed process.
         */
	KNOTE(&p->p_klist, NOTE_EXEC);
	p->p_flag &= ~P_INEXEC;

	/*
	 * If tracing the process, trap to debugger so breakpoints
	 * 	can be set before the program executes.
	 */
	STOPEVENT(p, S_EXEC, 0);

	if (p->p_flag & P_TRACED)
		psignal(p, SIGTRAP);

	/* clear "fork but no exec" flag, as we _are_ execing */
	p->p_acflag &= ~AFORK;

	/* Set values passed into the program in registers. */
	setregs(p, imgp->entry_addr, (u_long)(uintptr_t)stack_base,
	    imgp->ps_strings);

	/* Free any previous argument cache */
	if (p->p_args && --p->p_args->ar_ref == 0)
		FREE(p->p_args, M_PARGS);
	p->p_args = NULL;

	/* Cache arguments if they fit inside our allowance */
	i = imgp->args->begin_envv - imgp->args->begin_argv;
	if (ps_arg_cache_limit >= i + sizeof(struct pargs)) {
		MALLOC(p->p_args, struct pargs *, sizeof(struct pargs) + i, 
		    M_PARGS, M_WAITOK);
		p->p_args->ar_ref = 1;
		p->p_args->ar_length = i;
		bcopy(imgp->args->begin_argv, p->p_args->ar_args, i);
	}

exec_fail_dealloc:

	/*
	 * free various allocated resources
	 */
	if (imgp->firstpage)
		exec_unmap_first_page(imgp);

	if (imgp->vp) {
		vrele(imgp->vp);
		imgp->vp = NULL;
	}

	if (error == 0) {
		++mycpu->gd_cnt.v_exec;
		return (0);
	}

exec_fail:
	/* we're done here, clear P_INEXEC */
	p->p_flag &= ~P_INEXEC;
	if (imgp->vmspace_destroyed) {
		/* sorry, no more process anymore. exit gracefully */
		exit1(W_EXITCODE(0, SIGABRT));
		/* NOT REACHED */
		return(0);
	} else {
		return(error);
	}
}

/*
 * execve() system call.
 */
int
execve(struct execve_args *uap)
{
	struct nlookupdata nd;
	struct image_args args;
	int error;

	error = nlookup_init(&nd, uap->fname, UIO_USERSPACE, NLC_FOLLOW);
	if (error == 0) {
		error = exec_copyin_args(&args, uap->fname, PATH_USERSPACE,
					uap->argv, uap->envv);
	}
	if (error == 0)
		error = kern_execve(&nd, &args);
	nlookup_done(&nd);
	exec_free_args(&args);

	/*
	 * The syscall result is returned in registers to the new program.
	 * Linux will register %edx as an atexit function and we must be
	 * sure to set it to 0.  XXX
	 */
	if (error == 0)
		uap->sysmsg_result64 = 0;

	return (error);
}

int
exec_map_first_page(struct image_params *imgp)
{
	int rv, i;
	int initial_pagein;
	vm_page_t ma[VM_INITIAL_PAGEIN];
	vm_page_t m;
	vm_object_t object;
	int error;

	if (imgp->firstpage)
		exec_unmap_first_page(imgp);

	/*
	 * XXX the callers should really use vn_open so we don't have to
	 * do this junk.
	 */
	if ((error = VOP_GETVOBJECT(imgp->vp, &object)) != 0) {
		if (vn_canvmio(imgp->vp) == TRUE) {
			error = vfs_object_create(imgp->vp, curthread);
			if (error == 0)
				error = VOP_GETVOBJECT(imgp->vp, &object);
		}
	}
	if (error)
		return (EIO);

	/*
	 * We shouldn't need protection for vm_page_grab() but we certainly
	 * need it for the lookup loop below (lookup/busy race), since
	 * an interrupt can unbusy and free the page before our busy check.
	 */
	crit_enter();
	m = vm_page_grab(object, 0, VM_ALLOC_NORMAL | VM_ALLOC_RETRY);

	if ((m->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL) {
		ma[0] = m;
		initial_pagein = VM_INITIAL_PAGEIN;
		if (initial_pagein > object->size)
			initial_pagein = object->size;
		for (i = 1; i < initial_pagein; i++) {
			if ((m = vm_page_lookup(object, i)) != NULL) {
				if ((m->flags & PG_BUSY) || m->busy)
					break;
				if (m->valid)
					break;
				vm_page_busy(m);
			} else {
				m = vm_page_alloc(object, i, VM_ALLOC_NORMAL);
				if (m == NULL)
					break;
			}
			ma[i] = m;
		}
		initial_pagein = i;

		/*
		 * get_pages unbusies all the requested pages except the
		 * primary page (at index 0 in this case).
		 */
		rv = vm_pager_get_pages(object, ma, initial_pagein, 0);
		m = vm_page_lookup(object, 0);

		if (rv != VM_PAGER_OK || m == NULL || m->valid == 0) {
			if (m) {
				vm_page_protect(m, VM_PROT_NONE);
				vm_page_free(m);
			}
			crit_exit();
			return EIO;
		}
	}
	vm_page_hold(m);
	vm_page_wakeup(m);	/* unbusy the page */
	crit_exit();

	imgp->firstpage = sf_buf_alloc(m, SFB_CPUPRIVATE);
	imgp->image_header = (void *)sf_buf_kva(imgp->firstpage);

	return 0;
}

void
exec_unmap_first_page(imgp)
	struct image_params *imgp;
{
	vm_page_t m;

	crit_enter();
	if (imgp->firstpage != NULL) {
		m = sf_buf_page(imgp->firstpage);
		sf_buf_free(imgp->firstpage);
		imgp->firstpage = NULL;
		imgp->image_header = NULL;
		vm_page_unhold(m);
	}
	crit_exit();
}

/*
 * Destroy old address space, and allocate a new stack
 *	The new stack is only SGROWSIZ large because it is grown
 *	automatically in trap.c.
 */
int
exec_new_vmspace(struct image_params *imgp, struct vmspace *vmcopy)
{
	int error;
	struct vmspace *vmspace = imgp->proc->p_vmspace;
	vm_offset_t stack_addr = USRSTACK - maxssiz;
	vm_map_t map;

	imgp->vmspace_destroyed = 1;

	/*
	 * Prevent a pending AIO from modifying the new address space.
	 */
	aio_proc_rundown(imgp->proc);

	/*
	 * Blow away entire process VM, if address space not shared,
	 * otherwise, create a new VM space so that other threads are
	 * not disrupted.  If we are execing a resident vmspace we
	 * create a duplicate of it and remap the stack.
	 *
	 * The exitingcnt test is not strictly necessary but has been
	 * included for code sanity (to make the code more deterministic).
	 */
	map = &vmspace->vm_map;
	if (vmcopy) {
		vmspace_exec(imgp->proc, vmcopy);
		vmspace = imgp->proc->p_vmspace;
		pmap_remove_pages(vmspace_pmap(vmspace), stack_addr, USRSTACK);
		map = &vmspace->vm_map;
	} else if (vmspace->vm_refcnt == 1 && vmspace->vm_exitingcnt == 0) {
		shmexit(vmspace);
		if (vmspace->vm_upcalls)
			upc_release(vmspace, imgp->proc);
		pmap_remove_pages(vmspace_pmap(vmspace), 0, VM_MAXUSER_ADDRESS);
		vm_map_remove(map, 0, VM_MAXUSER_ADDRESS);
	} else {
		vmspace_exec(imgp->proc, NULL);
		vmspace = imgp->proc->p_vmspace;
		map = &vmspace->vm_map;
	}

	/* Allocate a new stack */
	error = vm_map_stack(&vmspace->vm_map, stack_addr, (vm_size_t)maxssiz,
	    VM_PROT_ALL, VM_PROT_ALL, 0);
	if (error)
		return (error);

	/* vm_ssize and vm_maxsaddr are somewhat antiquated concepts in the
	 * VM_STACK case, but they are still used to monitor the size of the
	 * process stack so we can check the stack rlimit.
	 */
	vmspace->vm_ssize = sgrowsiz >> PAGE_SHIFT;
	vmspace->vm_maxsaddr = (char *)USRSTACK - maxssiz;

	return(0);
}

/*
 * Copy out argument and environment strings from the old process
 *	address space into the temporary string buffer.
 */
int
exec_copyin_args(struct image_args *args, char *fname,
		enum exec_path_segflg segflg, char **argv, char **envv)
{
	char	*argp, *envp;
	int	error = 0;
	size_t	length;

	bzero(args, sizeof(*args));
	args->buf = (char *) kmem_alloc_wait(exec_map, PATH_MAX + ARG_MAX);
	if (args->buf == NULL)
		return (ENOMEM);
	args->begin_argv = args->buf;
	args->endp = args->begin_argv;
	args->space = ARG_MAX;

	args->fname = args->buf + ARG_MAX;

	/*
	 * Copy the file name.
	 */
	if (segflg == PATH_SYSSPACE) {
		error = copystr(fname, args->fname, PATH_MAX, &length);
	} else if (segflg == PATH_USERSPACE) {
		error = copyinstr(fname, args->fname, PATH_MAX, &length);
	}

	/*
	 * Extract argument strings.  argv may not be NULL.  The argv
	 * array is terminated by a NULL entry.  We special-case the
	 * situation where argv[0] is NULL by passing { filename, NULL }
	 * to the new program to guarentee that the interpreter knows what
	 * file to open in case we exec an interpreted file.   Note that
	 * a NULL argv[0] terminates the argv[] array.
	 *
	 * XXX the special-casing of argv[0] is historical and needs to be
	 * revisited.
	 */
	if (argv == NULL)
		error = EFAULT;
	if (error == 0) {
		while ((argp = (caddr_t)(intptr_t)fuword(argv++)) != NULL) {
			if (argp == (caddr_t)-1) {
				error = EFAULT;
				break;
			}
			error = copyinstr(argp, args->endp,
					    args->space, &length);
			if (error) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				break;
			}
			args->space -= length;
			args->endp += length;
			args->argc++;
		}
		if (args->argc == 0 && error == 0) {
			length = strlen(args->fname) + 1;
			if (length > args->space) {
				error = E2BIG;
			} else {
				bcopy(args->fname, args->endp, length);
				args->space -= length;
				args->endp += length;
				args->argc++;
			}
		}
	}	

	args->begin_envv = args->endp;

	/*
	 * extract environment strings.  envv may be NULL.
	 */
	if (envv && error == 0) {
		while ((envp = (caddr_t) (intptr_t) fuword(envv++))) {
			if (envp == (caddr_t) -1) {
				error = EFAULT;
				break;
			}
			error = copyinstr(envp, args->endp, args->space,
			    &length);
			if (error) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				break;
			}
			args->space -= length;
			args->endp += length;
			args->envc++;
		}
	}
	return (error);
}

void
exec_free_args(struct image_args *args)
{
	if (args->buf) {
		kmem_free_wakeup(exec_map,
				(vm_offset_t)args->buf, PATH_MAX + ARG_MAX);
		args->buf = NULL;
	}
}

/*
 * Copy strings out to the new process address space, constructing
 *	new arg and env vector tables. Return a pointer to the base
 *	so that it can be used as the initial stack pointer.
 */
register_t *
exec_copyout_strings(struct image_params *imgp)
{
	int argc, envc;
	char **vectp;
	char *stringp, *destp;
	register_t *stack_base;
	struct ps_strings *arginfo;
	int szsigcode;

	/*
	 * Calculate string base and vector table pointers.
	 * Also deal with signal trampoline code for this exec type.
	 */
	arginfo = (struct ps_strings *)PS_STRINGS;
	szsigcode = *(imgp->proc->p_sysent->sv_szsigcode);
	destp =	(caddr_t)arginfo - szsigcode - SPARE_USRSPACE -
	    roundup((ARG_MAX - imgp->args->space), sizeof(char *));

	/*
	 * install sigcode
	 */
	if (szsigcode)
		copyout(imgp->proc->p_sysent->sv_sigcode,
		    ((caddr_t)arginfo - szsigcode), szsigcode);

	/*
	 * If we have a valid auxargs ptr, prepare some room
	 * on the stack.
	 *
	 * The '+ 2' is for the null pointers at the end of each of the
	 * arg and env vector sets, and 'AT_COUNT*2' is room for the
	 * ELF Auxargs data.
	 */
	if (imgp->auxargs) {
		vectp = (char **)(destp - (imgp->args->argc +
			imgp->args->envc + 2 + AT_COUNT * 2) * sizeof(char*));
	} else {
		vectp = (char **)(destp - (imgp->args->argc +
			imgp->args->envc + 2) * sizeof(char*));
	}

	/*
	 * NOTE: don't bother aligning the stack here for GCC 2.x, it will
	 * be done in crt1.o.  Note that GCC 3.x aligns the stack in main.
	 */

	/*
	 * vectp also becomes our initial stack base
	 */
	stack_base = (register_t *)vectp;

	stringp = imgp->args->begin_argv;
	argc = imgp->args->argc;
	envc = imgp->args->envc;

	/*
	 * Copy out strings - arguments and environment.
	 */
	copyout(stringp, destp, ARG_MAX - imgp->args->space);

	/*
	 * Fill in "ps_strings" struct for ps, w, etc.
	 */
	suword(&arginfo->ps_argvstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nargvstr, argc);

	/*
	 * Fill in argument portion of vector table.
	 */
	for (; argc > 0; --argc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* a null vector table pointer separates the argp's from the envp's */
	suword(vectp++, 0);

	suword(&arginfo->ps_envstr, (long)(intptr_t)vectp);
	suword(&arginfo->ps_nenvstr, envc);

	/*
	 * Fill in environment portion of vector table.
	 */
	for (; envc > 0; --envc) {
		suword(vectp++, (long)(intptr_t)destp);
		while (*stringp++ != 0)
			destp++;
		destp++;
	}

	/* end of vector table is a null pointer */
	suword(vectp, 0);

	return (stack_base);
}

/*
 * Check permissions of file to execute.
 *	Return 0 for success or error code on failure.
 */
int
exec_check_permissions(imgp)
	struct image_params *imgp;
{
	struct proc *p = imgp->proc;
	struct vnode *vp = imgp->vp;
	struct vattr *attr = imgp->attr;
	struct thread *td = p->p_thread;
	int error;

	/* Get file attributes */
	error = VOP_GETATTR(vp, attr, td);
	if (error)
		return (error);

	/*
	 * 1) Check if file execution is disabled for the filesystem that this
	 *	file resides on.
	 * 2) Insure that at least one execute bit is on - otherwise root
	 *	will always succeed, and we don't want to happen unless the
	 *	file really is executable.
	 * 3) Insure that the file is a regular file.
	 */
	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr->va_mode & 0111) == 0) ||
	    (attr->va_type != VREG)) {
		return (EACCES);
	}

	/*
	 * Zero length files can't be exec'd
	 */
	if (attr->va_size == 0)
		return (ENOEXEC);

	/*
	 *  Check for execute permission to file based on current credentials.
	 */
	error = VOP_ACCESS(vp, VEXEC, p->p_ucred, td);
	if (error)
		return (error);

	/*
	 * Check number of open-for-writes on the file and deny execution
	 * if there are any.
	 */
	if (vp->v_writecount)
		return (ETXTBSY);

	/*
	 * Call filesystem specific open routine (which does nothing in the
	 * general case).
	 */
	error = VOP_OPEN(vp, FREAD, p->p_ucred, NULL, td);
	if (error)
		return (error);

	return (0);
}

/*
 * Exec handler registration
 */
int
exec_register(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 2;	/* New slot and trailing NULL */

	if (execsw)
		for (es = execsw; *es; es++)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return ENOMEM;
	xs = newexecsw;
	if (execsw)
		for (es = execsw; *es; es++)
			*xs++ = *es;
	*xs++ = execsw_arg;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return 0;
}

int
exec_unregister(execsw_arg)
	const struct execsw *execsw_arg;
{
	const struct execsw **es, **xs, **newexecsw;
	int count = 1;

	if (execsw == NULL)
		panic("unregister with no handlers left?");

	for (es = execsw; *es; es++) {
		if (*es == execsw_arg)
			break;
	}
	if (*es == NULL)
		return ENOENT;
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			count++;
	newexecsw = malloc(count * sizeof(*es), M_TEMP, M_WAITOK);
	if (newexecsw == NULL)
		return ENOMEM;
	xs = newexecsw;
	for (es = execsw; *es; es++)
		if (*es != execsw_arg)
			*xs++ = *es;
	*xs = NULL;
	if (execsw)
		free(execsw, M_TEMP);
	execsw = newexecsw;
	return 0;
}
