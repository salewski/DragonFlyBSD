/*
 * Copyright (c) 2001 Alexander Kabaev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/linux/linux_ptrace.c,v 1.7.4.3 2003/01/03 17:13:23 kan Exp $
 * $DragonFly: src/sys/emulation/linux/i386/linux_ptrace.c,v 1.8 2003/08/07 21:17:18 dillon Exp $
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>

#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include "linux.h"
#include "linux_proto.h"

/*
 *   Linux ptrace requests numbers. Mostly identical to FreeBSD,
 *   except for MD ones and PT_ATTACH/PT_DETACH.
 */
#define	PTRACE_TRACEME		0
#define	PTRACE_PEEKTEXT		1
#define	PTRACE_PEEKDATA		2
#define	PTRACE_PEEKUSR		3
#define	PTRACE_POKETEXT		4
#define	PTRACE_POKEDATA		5
#define	PTRACE_POKEUSR		6
#define	PTRACE_CONT		7
#define	PTRACE_KILL		8
#define	PTRACE_SINGLESTEP	9

#define PTRACE_ATTACH		16
#define PTRACE_DETACH		17

#define	PTRACE_SYSCALL		24

#define PTRACE_GETREGS		12
#define PTRACE_SETREGS		13
#define PTRACE_GETFPREGS	14
#define PTRACE_SETFPREGS	15
#define PTRACE_GETFPXREGS	18
#define PTRACE_SETFPXREGS	19

#define PTRACE_SETOPTIONS	21

/*
 * Linux keeps debug registers at the following
 * offset in the user struct
 */
#define LINUX_DBREG_OFFSET	252
#define LINUX_DBREG_SIZE	(8*sizeof(l_int))

static __inline__ int
map_signum(int signum)
{

	if (signum > 0 && signum <= LINUX_SIGTBLSZ)
		signum = linux_to_bsd_signal[_SIG_IDX(signum)];
	return ((signum == SIGSTOP)? 0 : signum);
}

struct linux_pt_reg {
	l_long	ebx;
	l_long	ecx;
	l_long	edx;
	l_long	esi;
	l_long	edi;
	l_long	ebp;
	l_long	eax;
	l_int	xds;
	l_int	xes;
	l_int	xfs;
	l_int	xgs;
	l_long	orig_eax;
	l_long	eip;
	l_int	xcs;
	l_long	eflags;
	l_long	esp;
	l_int	xss;
};

/*
 *   Translate i386 ptrace registers between Linux and FreeBSD formats.
 *   The translation is pretty straighforward, for all registers, but
 *   orig_eax on Linux side and r_trapno and r_err in FreeBSD
 */
static void
map_regs_to_linux(struct reg *bsd_r, struct linux_pt_reg *linux_r)
{
	linux_r->ebx = bsd_r->r_ebx;
	linux_r->ecx = bsd_r->r_ecx;
	linux_r->edx = bsd_r->r_edx;
	linux_r->esi = bsd_r->r_esi;
	linux_r->edi = bsd_r->r_edi;
	linux_r->ebp = bsd_r->r_ebp;
	linux_r->eax = bsd_r->r_eax;
	linux_r->xds = bsd_r->r_ds;
	linux_r->xes = bsd_r->r_es;
	linux_r->xfs = bsd_r->r_fs;
	linux_r->xgs = bsd_r->r_gs;
	linux_r->orig_eax = bsd_r->r_eax;
	linux_r->eip = bsd_r->r_eip;
	linux_r->xcs = bsd_r->r_cs;
	linux_r->eflags = bsd_r->r_eflags;
	linux_r->esp = bsd_r->r_esp;
	linux_r->xss = bsd_r->r_ss;
}

static void
map_regs_from_linux(struct reg *bsd_r, struct linux_pt_reg *linux_r)
{
	bsd_r->r_ebx = linux_r->ebx;
	bsd_r->r_ecx = linux_r->ecx;
	bsd_r->r_edx = linux_r->edx;
	bsd_r->r_esi = linux_r->esi;
	bsd_r->r_edi = linux_r->edi;
	bsd_r->r_ebp = linux_r->ebp;
	bsd_r->r_eax = linux_r->eax;
	bsd_r->r_ds  = linux_r->xds;
	bsd_r->r_es  = linux_r->xes;
	bsd_r->r_fs  = linux_r->xfs;
	bsd_r->r_gs  = linux_r->xgs;
	bsd_r->r_eip = linux_r->eip;
	bsd_r->r_cs  = linux_r->xcs;
	bsd_r->r_eflags = linux_r->eflags;
	bsd_r->r_esp = linux_r->esp;
	bsd_r->r_ss = linux_r->xss;
}

struct linux_pt_fpreg {
	l_long cwd;
	l_long swd;
	l_long twd;
	l_long fip;
	l_long fcs;
	l_long foo;
	l_long fos;
	l_long st_space[2*10];
};

static void
map_fpregs_to_linux(struct fpreg *bsd_r, struct linux_pt_fpreg *linux_r)
{
	linux_r->cwd = bsd_r->fpr_env[0];
	linux_r->swd = bsd_r->fpr_env[1];
	linux_r->twd = bsd_r->fpr_env[2];
	linux_r->fip = bsd_r->fpr_env[3];
	linux_r->fcs = bsd_r->fpr_env[4];
	linux_r->foo = bsd_r->fpr_env[5];
	linux_r->fos = bsd_r->fpr_env[6];
	bcopy(bsd_r->fpr_acc, linux_r->st_space, sizeof(linux_r->st_space));
}

static void
map_fpregs_from_linux(struct fpreg *bsd_r, struct linux_pt_fpreg *linux_r)
{
	bsd_r->fpr_env[0] = linux_r->cwd;
	bsd_r->fpr_env[1] = linux_r->swd;
	bsd_r->fpr_env[2] = linux_r->twd;
	bsd_r->fpr_env[3] = linux_r->fip;
	bsd_r->fpr_env[4] = linux_r->fcs;
	bsd_r->fpr_env[5] = linux_r->foo;
	bsd_r->fpr_env[6] = linux_r->fos;
	bcopy(bsd_r->fpr_acc, linux_r->st_space, sizeof(bsd_r->fpr_acc));
}

struct linux_pt_fpxreg {
	l_ushort	cwd;
	l_ushort	swd;
	l_ushort	twd;
	l_ushort	fop;
	l_long		fip;
	l_long		fcs;
	l_long		foo;
	l_long		fos;
	l_long		mxcsr;
	l_long		reserved;
	l_long		st_space[32];
	l_long		xmm_space[32];
	l_long		padding[56];
};

#ifndef CPU_DISABLE_SSE
static int
linux_proc_read_fpxregs(struct proc *p, struct linux_pt_fpxreg *fpxregs)
{
	int error;

	error = 0;
	if (cpu_fxsr == 0 || (p->p_flag & P_INMEM) == 0)
		error = EIO;
	else
		bcopy(&p->p_thread->td_pcb->pcb_save.sv_xmm,
		    fpxregs, sizeof(*fpxregs));
	return (error);
}

static int
linux_proc_write_fpxregs(struct proc *p, struct linux_pt_fpxreg *fpxregs)
{
	int error;

	error = 0;
	if (cpu_fxsr == 0 || (p->p_flag & P_INMEM) == 0)
		error = EIO;
	else
		bcopy(fpxregs, &p->p_thread->td_pcb->pcb_save.sv_xmm,
		    sizeof(*fpxregs));
	return (error);
}
#endif

int
linux_ptrace(struct linux_ptrace_args *uap)
{
	struct proc *curp = curproc;
	union {
		struct linux_pt_reg	reg;
		struct linux_pt_fpreg	fpreg;
		struct linux_pt_fpxreg	fpxreg;
	} r;
	union {
		struct reg		bsd_reg;
		struct fpreg		bsd_fpreg;
		struct dbreg		bsd_dbreg;
	} u;
	void *addr;
	pid_t pid;
	int error, req;

	error = 0;

	/* by default, just copy data intact */
	req  = uap->req;
	pid  = (pid_t)uap->pid;
	addr = (void *)uap->addr;

	switch (req) {
	case PTRACE_TRACEME:
	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
	case PTRACE_KILL:
		error = kern_ptrace(curp, req, pid, addr, uap->data,
				&uap->sysmsg_result);
		break;
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA: {
		/* need to preserve return value, use dummy */
		l_int rval = 0;
		error = kern_ptrace(curp, req, pid, addr, 0, &rval);
		if (error == 0) {
			error = copyout(&rval, (caddr_t)uap->data, sizeof(l_int));
		}
		break;
	}
	case PTRACE_DETACH:
		error = kern_ptrace(curp, PT_DETACH, pid, (void *)1,
		     map_signum(uap->data), &uap->sysmsg_result);
		break;
	case PTRACE_SINGLESTEP:
	case PTRACE_CONT:
		error = kern_ptrace(curp, req, pid, (void *)1,
		     map_signum(uap->data), &uap->sysmsg_result);
		break;
	case PTRACE_ATTACH:
		error = kern_ptrace(curp, PT_ATTACH, pid, addr, uap->data,
				&uap->sysmsg_result);
		break;
	case PTRACE_GETREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = kern_ptrace(curp, PT_GETREGS, pid, &u.bsd_reg, 0,
				&uap->sysmsg_result);
		if (error == 0) {
			map_regs_to_linux(&u.bsd_reg, &r.reg);
			error = copyout(&r.reg, (caddr_t)uap->data,
			    sizeof(r.reg));
		}
		break;
	case PTRACE_SETREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = copyin((caddr_t)uap->data, &r.reg, sizeof(r.reg));
		if (error == 0) {
			map_regs_from_linux(&u.bsd_reg, &r.reg);
			error = kern_ptrace(curp, PT_SETREGS, pid, &u.bsd_reg, 0, &uap->sysmsg_result);
		}
		break;
	case PTRACE_GETFPREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = kern_ptrace(curp, PT_GETFPREGS, pid, &u.bsd_fpreg, 0,
				&uap->sysmsg_result);
		if (error == 0) {
			map_fpregs_to_linux(&u.bsd_fpreg, &r.fpreg);
			error = copyout(&r.fpreg, (caddr_t)uap->data,
			    sizeof(r.fpreg));
		}
		break;
	case PTRACE_SETFPREGS:
		/* Linux is using data where FreeBSD is using addr */
		error = copyin((caddr_t)uap->data, &r.fpreg, sizeof(r.fpreg));
		if (error == 0) {
			map_fpregs_from_linux(&u.bsd_fpreg, &r.fpreg);
			error = kern_ptrace(curp, PT_SETFPREGS, pid,
			    &u.bsd_fpreg, 0, &uap->sysmsg_result);
		}
		break;
	case PTRACE_SETFPXREGS:
#ifndef CPU_DISABLE_SSE
		error = copyin((caddr_t)uap->data, &r.fpxreg,
		    sizeof(r.fpxreg));
		if (error)
			break;
#endif
		/* FALL THROUGH */
	case PTRACE_GETFPXREGS: {
#ifndef CPU_DISABLE_SSE
		struct proc *p;

		if (sizeof(struct linux_pt_fpxreg) != sizeof(struct savexmm)) {
			static int once = 0;
			if (!once) {
				printf("linux: savexmm != linux_pt_fpxreg\n");
				once = 1;
			}
			error = EIO;
			break;
		}

		if ((p = pfind(uap->pid)) == NULL) {
			error = ESRCH;
			break;
		}

		if (!PRISON_CHECK(curp->p_ucred, p->p_ucred)) {
			error = ESRCH;
			goto fail;
		}

		/* System processes can't be debugged. */
		if ((p->p_flag & P_SYSTEM) != 0) {
			error = EINVAL;
			goto fail;
		}

		/* not being traced... */
		if ((p->p_flag & P_TRACED) == 0) {
			error = EPERM;
			goto fail;
		}

		/* not being traced by YOU */
		if (p->p_pptr != curp) {
			error = EBUSY;
			goto fail;
		}

		/* not currently stopped */
		if ((p->p_flag & (P_TRACED|P_WAITED)) == 0) {
			error = EBUSY;
			goto fail;
		}

		if (req == PTRACE_GETFPXREGS) {
			PHOLD(p);
			error = linux_proc_read_fpxregs(p, &r.fpxreg);
			PRELE(p);
			if (error == 0)
				error = copyout(&r.fpxreg, (caddr_t)uap->data,
				    sizeof(r.fpxreg));
		} else {
			/* clear dangerous bits exactly as Linux does*/
			r.fpxreg.mxcsr &= 0xffbf;
			PHOLD(p);
			error = linux_proc_write_fpxregs(p, &r.fpxreg);
			PRELE(p);
		}
		break;

	fail:
#else
		error = EIO;
#endif
		break;
	}
	case PTRACE_PEEKUSR:
	case PTRACE_POKEUSR: {
		error = EIO;

		/* check addr for alignment */
		if (uap->addr < 0 || uap->addr & (sizeof(l_int) - 1))
			break;
		/*
		 * Allow linux programs to access register values in
		 * user struct. We simulate this through PT_GET/SETREGS
		 * as necessary.
		 */
		if (uap->addr < sizeof(struct linux_pt_reg)) {
			error = kern_ptrace(curp, PT_GETREGS, pid, &u.bsd_reg, 0, &uap->sysmsg_result);
			if (error != 0)
				break;

			map_regs_to_linux(&u.bsd_reg, &r.reg);
			if (req == PTRACE_PEEKUSR) {
				error = copyout((char *)&r.reg + uap->addr,
				    (caddr_t)uap->data, sizeof(l_int));
				break;
			}

			*(l_int *)((char *)&r.reg + uap->addr) =
			    (l_int)uap->data;

			map_regs_from_linux(&u.bsd_reg, &r.reg);
			error = kern_ptrace(curp, PT_SETREGS, pid, &u.bsd_reg, 0, &uap->sysmsg_result);
		}

		/*
		 * Simulate debug registers access
		 */
		if (uap->addr >= LINUX_DBREG_OFFSET &&
		    uap->addr <= LINUX_DBREG_OFFSET + LINUX_DBREG_SIZE) {
			error = kern_ptrace(curp, PT_GETDBREGS, pid, 
				    &u.bsd_dbreg, 0, &uap->sysmsg_result);
			if (error != 0)
				break;

			uap->addr -= LINUX_DBREG_OFFSET;
			if (req == PTRACE_PEEKUSR) {
				error = copyout((char *)&u.bsd_dbreg +
				    uap->addr, (caddr_t)uap->data,
				    sizeof(l_int));
				break;
			}

			*(l_int *)((char *)&u.bsd_dbreg + uap->addr) =
			     uap->data;
			error = kern_ptrace(curp, PT_SETDBREGS, pid,
			    &u.bsd_dbreg, 0, &uap->sysmsg_result);
		}

		break;
	}
	case PTRACE_SYSCALL:
		/* fall through */
	default:
		printf("linux: ptrace(%u, ...) not implemented\n",
		    (unsigned int)uap->req);
		error = EINVAL;
		break;
	}

	return (error);
}
