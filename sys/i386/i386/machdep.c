/*-
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 * $FreeBSD: src/sys/i386/i386/machdep.c,v 1.385.2.30 2003/05/31 08:48:05 alc Exp $
 * $DragonFly: src/sys/i386/i386/Attic/machdep.c,v 1.72.2.1 2005/06/19 22:55:57 dillon Exp $
 */

#include "use_apm.h"
#include "use_ether.h"
#include "use_npx.h"
#include "use_isa.h"
#include "opt_atalk.h"
#include "opt_compat.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_directio.h"
#include "opt_inet.h"
#include "opt_ipx.h"
#include "opt_maxmem.h"
#include "opt_msgbuf.h"
#include "opt_perfmon.h"
#include "opt_swap.h"
#include "opt_userconfig.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <sys/upcall.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

#include <sys/thread2.h>

#include <sys/user.h>
#include <sys/exec.h>
#include <sys/cons.h>

#include <ddb/ddb.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/clock.h>
#include <machine/specialreg.h>
#include <machine/bootinfo.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/pcb_ext.h>		/* pcb.h included via sys/user.h */
#include <machine/globaldata.h>		/* CPU_prvspace */
#ifdef SMP
#include <machine/smp.h>
#endif
#ifdef PERFMON
#include <machine/perfmon.h>
#endif
#include <machine/cputypes.h>

#ifdef OLD_BUS_ARCH
#include <bus/isa/i386/isa_device.h>
#endif
#include <i386/isa/intr_machdep.h>
#include <bus/isa/rtc.h>
#include <machine/vm86.h>
#include <sys/random.h>
#include <sys/ptrace.h>
#include <machine/sigframe.h>

extern void init386 (int first);
extern void dblfault_handler (void);

extern void printcpuinfo(void);	/* XXX header file */
extern void finishidentcpu(void);
extern void panicifcpuunsupported(void);
extern void initializecpu(void);

static void cpu_startup (void *);
#ifndef CPU_DISABLE_SSE
static void set_fpregs_xmm (struct save87 *, struct savexmm *);
static void fill_fpregs_xmm (struct savexmm *, struct save87 *);
#endif /* CPU_DISABLE_SSE */
#ifdef DIRECTIO
extern void ffs_rawread_setup(void);
#endif /* DIRECTIO */
static void init_locks(void);

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

int	_udatasel, _ucodesel;
u_int	atdevbase;

#if defined(SWTCH_OPTIM_STATS)
extern int swtch_optim_stats;
SYSCTL_INT(_debug, OID_AUTO, swtch_optim_stats,
	CTLFLAG_RD, &swtch_optim_stats, 0, "");
SYSCTL_INT(_debug, OID_AUTO, tlb_flush_count,
	CTLFLAG_RD, &tlb_flush_count, 0, "");
#endif

int physmem = 0;
int cold = 1;

static int
sysctl_hw_physmem(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0, ctob(physmem), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_PHYSMEM, physmem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_physmem, "IU", "");

static int
sysctl_hw_usermem(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0,
		ctob(physmem - vmstats.v_wire_count), req);
	return (error);
}

SYSCTL_PROC(_hw, HW_USERMEM, usermem, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_usermem, "IU", "");

static int
sysctl_hw_availpages(SYSCTL_HANDLER_ARGS)
{
	int error = sysctl_handle_int(oidp, 0,
		i386_btop(avail_end - avail_start), req);
	return (error);
}

SYSCTL_PROC(_hw, OID_AUTO, availpages, CTLTYPE_INT|CTLFLAG_RD,
	0, 0, sysctl_hw_availpages, "I", "");

static int
sysctl_machdep_msgbuf(SYSCTL_HANDLER_ARGS)
{
	int error;

	/* Unwind the buffer, so that it's linear (possibly starting with
	 * some initial nulls).
	 */
	error=sysctl_handle_opaque(oidp,msgbufp->msg_ptr+msgbufp->msg_bufr,
		msgbufp->msg_size-msgbufp->msg_bufr,req);
	if(error) return(error);
	if(msgbufp->msg_bufr>0) {
		error=sysctl_handle_opaque(oidp,msgbufp->msg_ptr,
			msgbufp->msg_bufr,req);
	}
	return(error);
}

SYSCTL_PROC(_machdep, OID_AUTO, msgbuf, CTLTYPE_STRING|CTLFLAG_RD,
	0, 0, sysctl_machdep_msgbuf, "A","Contents of kernel message buffer");

static int msgbuf_clear;

static int
sysctl_machdep_msgbuf_clear(SYSCTL_HANDLER_ARGS)
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,
		req);
	if (!error && req->newptr) {
		/* Clear the buffer and reset write pointer */
		bzero(msgbufp->msg_ptr,msgbufp->msg_size);
		msgbufp->msg_bufr=msgbufp->msg_bufx=0;
		msgbuf_clear=0;
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, msgbuf_clear, CTLTYPE_INT|CTLFLAG_RW,
	&msgbuf_clear, 0, sysctl_machdep_msgbuf_clear, "I",
	"Clear kernel message buffer");

int bootverbose = 0;
vm_paddr_t Maxmem = 0;
long dumplo;

vm_paddr_t phys_avail[22];	/* 10 entries + (0,0) terminator */

/* must be 2 less so 0 0 can signal end of chunks */
#define PHYS_AVAIL_ARRAY_END ((sizeof(phys_avail) / sizeof(phys_avail[0])) - 2)

static vm_offset_t buffer_sva, buffer_eva;
vm_offset_t clean_sva, clean_eva;
static vm_offset_t pager_sva, pager_eva;
static struct trapframe proc0_tf;

static void
cpu_startup(dummy)
	void *dummy;
{
	caddr_t v;
	vm_offset_t minaddr;
	vm_offset_t maxaddr;
	vm_size_t size = 0;
	int firstaddr;

	if (boothowto & RB_VERBOSE)
		bootverbose++;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s", version);
	startrtclock();
	printcpuinfo();
	panicifcpuunsupported();
#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %llu (%lluK bytes)\n", ptoa(Maxmem), ptoa(Maxmem) / 1024);
	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size1 = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08llx - 0x%08llx, %llu bytes (%llu pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1, size1,
			    size1 / PAGE_SIZE);
		}
	}

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */

	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t)firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))

	/*
	 * The nominal buffer size (and minimum KVA allocation) is BKVASIZE.
	 * For the first 64MB of ram nominally allocate sufficient buffers to
	 * cover 1/4 of our ram.  Beyond the first 64MB allocate additional
	 * buffers to cover 1/20 of our ram over 64MB.  When auto-sizing
	 * the buffer cache we limit the eventual kva reservation to
	 * maxbcache bytes.
	 *
	 * factor represents the 1/4 x ram conversion.
	 */
	if (nbuf == 0) {
		int factor = 4 * BKVASIZE / 1024;
		int kbytes = physmem * (PAGE_SIZE / 1024);

		nbuf = 50;
		if (kbytes > 4096)
			nbuf += min((kbytes - 4096) / factor, 65536 / factor);
		if (kbytes > 65536)
			nbuf += (kbytes - 65536) * 2 / (factor * 5);
		if (maxbcache && nbuf > maxbcache / BKVASIZE)
			nbuf = maxbcache / BKVASIZE;
	}

	/*
	 * Do not allow the buffer_map to be more then 1/2 the size of the
	 * kernel_map.
	 */
	if (nbuf > (kernel_map->max_offset - kernel_map->min_offset) / 
	    (BKVASIZE * 2)) {
		nbuf = (kernel_map->max_offset - kernel_map->min_offset) / 
		    (BKVASIZE * 2);
		printf("Warning: nbufs capped at %d\n", nbuf);
	}

	nswbuf = max(min(nbuf/4, 256), 16);
#ifdef NSWBUF_MIN
	if (nswbuf < NSWBUF_MIN)
		nswbuf = NSWBUF_MIN;
#endif
#ifdef DIRECTIO
	ffs_rawread_setup();
#endif

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	v = bufhashinit(v);

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t)(v - firstaddr);
		firstaddr = (int)kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}

	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t)(v - firstaddr) != size)
		panic("startup: table size inconsistency");

	clean_map = kmem_suballoc(kernel_map, &clean_sva, &clean_eva,
			(nbuf*BKVASIZE) + (nswbuf*MAXPHYS) + pager_map_size);
	buffer_map = kmem_suballoc(clean_map, &buffer_sva, &buffer_eva,
				(nbuf*BKVASIZE));
	buffer_map->system_map = 1;
	pager_map = kmem_suballoc(clean_map, &pager_sva, &pager_eva,
				(nswbuf*MAXPHYS) + pager_map_size);
	pager_map->system_map = 1;
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				(16*(ARG_MAX+(PAGE_SIZE*3))));

#if defined(USERCONFIG)
	userconfig();
	cninit();		/* the preferred console may have changed */
#endif

	printf("avail memory = %u (%uK bytes)\n", ptoa(vmstats.v_free_count),
	    ptoa(vmstats.v_free_count) / 1024);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

#ifdef SMP
	/*
	 * OK, enough kmem_alloc/malloc state should be up, lets get on with it!
	 */
	mp_start();			/* fire up the APs and APICs */
	mp_announce();
#endif  /* SMP */
	cpu_setregs();
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	sigset_t *mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *regs;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe sf, *sfp;
	int oonstack;

	regs = p->p_md.md_regs;
	oonstack = (p->p_sigstk.ss_flags & SS_ONSTACK) ? 1 : 0;

	/* save user context */
	bzero(&sf, sizeof(struct sigframe));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = p->p_sigstk;
	sf.sf_uc.uc_mcontext.mc_onstack = oonstack;
	sf.sf_uc.uc_mcontext.mc_gs = rgs();
	bcopy(regs, &sf.sf_uc.uc_mcontext.mc_fs, sizeof(struct trapframe));

	/* Allocate and validate space for the signal handler context. */
        if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct sigframe *)(p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - sizeof(struct sigframe));
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	}
	else
		sfp = (struct sigframe *)regs->tf_esp - 1;

	/* Translate the signal is appropriate */
	if (p->p_sysent->sv_sigtbl) {
		if (sig <= p->p_sysent->sv_sigsize)
			sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];
	}

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	if (SIGISMEMBER(p->p_sigacts->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (register_t)&sfp->sf_si;
		sf.sf_ahu.sf_action = (__siginfohandler_t *)catcher;

		/* fill siginfo structure */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = (void*)regs->tf_err;
	}
	else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = code;
		sf.sf_addr = regs->tf_err;
		sf.sf_ahu.sf_handler = catcher;
	}

	/*
	 * If we're a vm86 process, we want to save the segment registers.
	 * We also change eflags to be our emulated eflags, not the actual
	 * eflags.
	 */
	if (regs->tf_eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86 = &p->p_thread->td_pcb->pcb_ext->ext_vm86;

		sf.sf_uc.uc_mcontext.mc_gs = tf->tf_vm86_gs;
		sf.sf_uc.uc_mcontext.mc_fs = tf->tf_vm86_fs;
		sf.sf_uc.uc_mcontext.mc_es = tf->tf_vm86_es;
		sf.sf_uc.uc_mcontext.mc_ds = tf->tf_vm86_ds;

		if (vm86->vm86_has_vme == 0)
			sf.sf_uc.uc_mcontext.mc_eflags =
			    (tf->tf_eflags & ~(PSL_VIF | PSL_VIP)) |
			    (vm86->vm86_eflags & (PSL_VIF | PSL_VIP));

		/*
		 * Clear PSL_NT to inhibit T_TSSFLT faults on return from
		 * syscalls made by the signal handler.  This just avoids
		 * wasting time for our lazy fixup of such faults.  PSL_NT
		 * does nothing in vm86 mode, but vm86 programs can set it
		 * almost legitimately in probes for old cpu types.
		 */
		tf->tf_eflags &= ~(PSL_VM | PSL_NT | PSL_VIF | PSL_VIP);
	}

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(struct sigframe)) != 0) {
		/*
		 * Something is wrong with the stack pointer.
		 * ...Kill the process.
		 */
		sigexit(p, SIGILL);
	}

	regs->tf_esp = (int)sfp;
	regs->tf_eip = PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	regs->tf_eflags &= ~PSL_T;
	regs->tf_cs = _ucodesel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_ss = _udatasel;
}

/*
 * sigreturn(ucontext_t *sigcntxp)
 *
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 */
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)
#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)

int
sigreturn(struct sigreturn_args *uap)
{
	struct proc *p = curproc;
	struct trapframe *regs;
	ucontext_t *ucp;
	int cs, eflags;

	ucp = uap->sigcntxp;

	if (!useracc((caddr_t)ucp, sizeof(ucontext_t), VM_PROT_READ))
		return (EFAULT);

	regs = p->p_md.md_regs;
	eflags = ucp->uc_mcontext.mc_eflags;

	if (eflags & PSL_VM) {
		struct trapframe_vm86 *tf = (struct trapframe_vm86 *)regs;
		struct vm86_kernel *vm86;

		/*
		 * if pcb_ext == 0 or vm86_inited == 0, the user hasn't
		 * set up the vm86 area, and we can't enter vm86 mode.
		 */
		if (p->p_thread->td_pcb->pcb_ext == 0)
			return (EINVAL);
		vm86 = &p->p_thread->td_pcb->pcb_ext->ext_vm86;
		if (vm86->vm86_inited == 0)
			return (EINVAL);

		/* go back to user mode if both flags are set */
		if ((eflags & PSL_VIP) && (eflags & PSL_VIF))
			trapsignal(p, SIGBUS, 0);

		if (vm86->vm86_has_vme) {
			eflags = (tf->tf_eflags & ~VME_USERCHANGE) |
			    (eflags & VME_USERCHANGE) | PSL_VM;
		} else {
			vm86->vm86_eflags = eflags;	/* save VIF, VIP */
			eflags = (tf->tf_eflags & ~VM_USERCHANGE) |					    (eflags & VM_USERCHANGE) | PSL_VM;
		}
		bcopy(&ucp->uc_mcontext.mc_fs, tf, sizeof(struct trapframe));
		tf->tf_eflags = eflags;
		tf->tf_vm86_ds = tf->tf_ds;
		tf->tf_vm86_es = tf->tf_es;
		tf->tf_vm86_fs = tf->tf_fs;
		tf->tf_vm86_gs = ucp->uc_mcontext.mc_gs;
		tf->tf_ds = _udatasel;
		tf->tf_es = _udatasel;
		tf->tf_fs = _udatasel;
	} else {
		/*
		 * Don't allow users to change privileged or reserved flags.
		 */
		/*
		 * XXX do allow users to change the privileged flag PSL_RF.
		 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
		 * should sometimes set it there too.  tf_eflags is kept in
		 * the signal context during signal handling and there is no
		 * other place to remember it, so the PSL_RF bit may be
		 * corrupted by the signal handler without us knowing.
		 * Corruption of the PSL_RF bit at worst causes one more or
		 * one less debugger trap, so allowing it is fairly harmless.
		 */
		if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_eflags & ~PSL_RF)) {
			printf("sigreturn: eflags = 0x%x\n", eflags);
	    		return(EINVAL);
		}

		/*
		 * Don't allow users to load a valid privileged %cs.  Let the
		 * hardware check for invalid selectors, excess privilege in
		 * other selectors, invalid %eip's and invalid %esp's.
		 */
		cs = ucp->uc_mcontext.mc_cs;
		if (!CS_SECURE(cs)) {
			printf("sigreturn: cs = 0x%x\n", cs);
			trapsignal(p, SIGBUS, T_PROTFLT);
			return(EINVAL);
		}
		bcopy(&ucp->uc_mcontext.mc_fs, regs, sizeof(struct trapframe));
	}

	if (ucp->uc_mcontext.mc_onstack & 1)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;

	p->p_sigmask = ucp->uc_sigmask;
	SIG_CANTMASK(p->p_sigmask);
	return(EJUSTRETURN);
}

/*
 * Stack frame on entry to function.  %eax will contain the function vector,
 * %ecx will contain the function data.  flags, ecx, and eax will have 
 * already been pushed on the stack.
 */
struct upc_frame {
	register_t	eax;
	register_t	ecx;
	register_t	edx;
	register_t	flags;
	register_t	oldip;
};

void
sendupcall(struct vmupcall *vu, int morepending)
{
	struct proc *p = curproc;
	struct trapframe *regs;
	struct upcall upcall;
	struct upc_frame upc_frame;
	int	crit_count = 0;

	/*
	 * Get the upcall data structure
	 */
	if (copyin(p->p_upcall, &upcall, sizeof(upcall)) ||
	    copyin((char *)upcall.upc_uthread + upcall.upc_critoff, &crit_count, sizeof(int))
	) {
		vu->vu_pending = 0;
		printf("bad upcall address\n");
		return;
	}

	/*
	 * If the data structure is already marked pending or has a critical
	 * section count, mark the data structure as pending and return 
	 * without doing an upcall.  vu_pending is left set.
	 */
	if (upcall.upc_pending || crit_count >= vu->vu_pending) {
		if (upcall.upc_pending < vu->vu_pending) {
			upcall.upc_pending = vu->vu_pending;
			copyout(&upcall.upc_pending, &p->p_upcall->upc_pending,
				sizeof(upcall.upc_pending));
		}
		return;
	}

	/*
	 * We can run this upcall now, clear vu_pending.
	 *
	 * Bump our critical section count and set or clear the
	 * user pending flag depending on whether more upcalls are
	 * pending.  The user will be responsible for calling 
	 * upc_dispatch(-1) to process remaining upcalls.
	 */
	vu->vu_pending = 0;
	upcall.upc_pending = morepending;
	crit_count += TDPRI_CRIT;
	copyout(&upcall.upc_pending, &p->p_upcall->upc_pending, 
		sizeof(upcall.upc_pending));
	copyout(&crit_count, (char *)upcall.upc_uthread + upcall.upc_critoff,
		sizeof(int));

	/*
	 * Construct a stack frame and issue the upcall
	 */
	regs = p->p_md.md_regs;
	upc_frame.eax = regs->tf_eax;
	upc_frame.ecx = regs->tf_ecx;
	upc_frame.edx = regs->tf_edx;
	upc_frame.flags = regs->tf_eflags;
	upc_frame.oldip = regs->tf_eip;
	if (copyout(&upc_frame, (void *)(regs->tf_esp - sizeof(upc_frame)),
	    sizeof(upc_frame)) != 0) {
		printf("bad stack on upcall\n");
	} else {
		regs->tf_eax = (register_t)vu->vu_func;
		regs->tf_ecx = (register_t)vu->vu_data;
		regs->tf_edx = (register_t)p->p_upcall;
		regs->tf_eip = (register_t)vu->vu_ctx;
		regs->tf_esp -= sizeof(upc_frame);
	}
}

/*
 * fetchupcall occurs in the context of a system call, which means that
 * we have to return EJUSTRETURN in order to prevent eax and edx from
 * being overwritten by the syscall return value.
 *
 * if vu is not NULL we return the new context in %edx, the new data in %ecx,
 * and the function pointer in %eax.  
 */
int
fetchupcall (struct vmupcall *vu, int morepending, void *rsp)
{
	struct upc_frame upc_frame;
	struct proc *p;
	struct trapframe *regs;
	int error;
	struct upcall upcall;
	int crit_count;

	p = curproc;
	regs = p->p_md.md_regs;

	error = copyout(&morepending, &p->p_upcall->upc_pending, sizeof(int));
	if (error == 0) {
	    if (vu) {
		/*
		 * This jumps us to the next ready context.
		 */
		vu->vu_pending = 0;
		error = copyin(p->p_upcall, &upcall, sizeof(upcall));
		crit_count = 0;
		if (error == 0)
			error = copyin((char *)upcall.upc_uthread + upcall.upc_critoff, &crit_count, sizeof(int));
		crit_count += TDPRI_CRIT;
		if (error == 0)
			error = copyout(&crit_count, (char *)upcall.upc_uthread + upcall.upc_critoff, sizeof(int));
		regs->tf_eax = (register_t)vu->vu_func;
		regs->tf_ecx = (register_t)vu->vu_data;
		regs->tf_edx = (register_t)p->p_upcall;
		regs->tf_eip = (register_t)vu->vu_ctx;
		regs->tf_esp = (register_t)rsp;
	    } else {
		/*
		 * This returns us to the originally interrupted code.
		 */
		error = copyin(rsp, &upc_frame, sizeof(upc_frame));
		regs->tf_eax = upc_frame.eax;
		regs->tf_ecx = upc_frame.ecx;
		regs->tf_edx = upc_frame.edx;
		regs->tf_eflags = (regs->tf_eflags & ~PSL_USERCHANGE) |
				(upc_frame.flags & PSL_USERCHANGE);
		regs->tf_eip = upc_frame.oldip;
		regs->tf_esp = (register_t)((char *)rsp + sizeof(upc_frame));
	    }
	}
	if (error == 0)
		error = EJUSTRETURN;
	return(error);
}

/*
 * Machine dependent boot() routine
 *
 * I haven't seen anything to put here yet
 * Possibly some stuff might be grafted back here from boot()
 */
void
cpu_boot(int howto)
{
}

/*
 * Shutdown the CPU as much as possible
 */
void
cpu_halt(void)
{
	for (;;)
		__asm__ ("hlt");
}

/*
 * cpu_idle() represents the idle LWKT.  You cannot return from this function
 * (unless you want to blow things up!).  Instead we look for runnable threads
 * and loop or halt as appropriate.  Giant is not held on entry to the thread.
 *
 * The main loop is entered with a critical section held, we must release
 * the critical section before doing anything else.  lwkt_switch() will
 * check for pending interrupts due to entering and exiting its own 
 * critical section.
 *
 * Note on cpu_idle_hlt:  On an SMP system we rely on a scheduler IPI
 * to wake a HLTed cpu up.  However, there are cases where the idlethread
 * will be entered with the possibility that no IPI will occur and in such
 * cases lwkt_switch() sets TDF_IDLE_NOHLT.
 */
static int	cpu_idle_hlt = 1;
static int	cpu_idle_hltcnt;
static int	cpu_idle_spincnt;
SYSCTL_INT(_machdep, OID_AUTO, cpu_idle_hlt, CTLFLAG_RW,
    &cpu_idle_hlt, 0, "Idle loop HLT enable");
SYSCTL_INT(_machdep, OID_AUTO, cpu_idle_hltcnt, CTLFLAG_RW,
    &cpu_idle_hltcnt, 0, "Idle loop entry halts");
SYSCTL_INT(_machdep, OID_AUTO, cpu_idle_spincnt, CTLFLAG_RW,
    &cpu_idle_spincnt, 0, "Idle loop entry spins");

static void
cpu_idle_default_hook(void)
{
	/*
	 * We must guarentee that hlt is exactly the instruction
	 * following the sti.
	 */
	__asm __volatile("sti; hlt");
}

/* Other subsystems (e.g., ACPI) can hook this later. */
void (*cpu_idle_hook)(void) = cpu_idle_default_hook;

void
cpu_idle(void)
{
	struct thread *td = curthread;

	crit_exit();
	KKASSERT(td->td_pri < TDPRI_CRIT);
	for (;;) {
		/*
		 * See if there are any LWKTs ready to go.
		 */
		lwkt_switch();

		/*
		 * If we are going to halt call splz unconditionally after
		 * CLIing to catch any interrupt races.  Note that we are
		 * at SPL0 and interrupts are enabled.
		 */
		if (cpu_idle_hlt && !lwkt_runnable() &&
		    (td->td_flags & TDF_IDLE_NOHLT) == 0) {
			__asm __volatile("cli");
			splz();
			if (!lwkt_runnable())
			    cpu_idle_hook();
#ifdef SMP
			else
			    __asm __volatile("pause");
#endif
			++cpu_idle_hltcnt;
		} else {
			td->td_flags &= ~TDF_IDLE_NOHLT;
			splz();
#ifdef SMP
			__asm __volatile("sti; pause");
#else
			__asm __volatile("sti");
#endif
			++cpu_idle_spincnt;
		}
	}
}

/*
 * Clear registers on exec
 */
void
setregs(p, entry, stack, ps_strings)
	struct proc *p;
	u_long entry;
	u_long stack;
	u_long ps_strings;
{
	struct trapframe *regs = p->p_md.md_regs;
	struct pcb *pcb = p->p_thread->td_pcb;

	/* Reset pc->pcb_gs and %gs before possibly invalidating it. */
	pcb->pcb_gs = _udatasel;
	load_gs(_udatasel);

	/* was i386_user_cleanup() in NetBSD */
	user_ldt_free(pcb);
  
	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_eip = entry;
	regs->tf_esp = stack;
	regs->tf_eflags = PSL_USER | (regs->tf_eflags & PSL_T);
	regs->tf_ss = _udatasel;
	regs->tf_ds = _udatasel;
	regs->tf_es = _udatasel;
	regs->tf_fs = _udatasel;
	regs->tf_cs = _ucodesel;

	/* PS_STRINGS value for BSD/OS binaries.  It is 0 for non-BSD/OS. */
	regs->tf_ebx = ps_strings;

        /*
         * Reset the hardware debug registers if they were in use.
         * They won't have any meaning for the newly exec'd process.  
         */
        if (pcb->pcb_flags & PCB_DBREGS) {
                pcb->pcb_dr0 = 0;
                pcb->pcb_dr1 = 0;
                pcb->pcb_dr2 = 0;
                pcb->pcb_dr3 = 0;
                pcb->pcb_dr6 = 0;
                pcb->pcb_dr7 = 0;
                if (pcb == curthread->td_pcb) {
		        /*
			 * Clear the debug registers on the running
			 * CPU, otherwise they will end up affecting
			 * the next process we switch to.
			 */
		        reset_dbregs();
                }
                pcb->pcb_flags &= ~PCB_DBREGS;
        }

	/*
	 * Initialize the math emulator (if any) for the current process.
	 * Actually, just clear the bit that says that the emulator has
	 * been initialized.  Initialization is delayed until the process
	 * traps to the emulator (if it is done at all) mainly because
	 * emulators don't provide an entry point for initialization.
	 */
	p->p_thread->td_pcb->pcb_flags &= ~FP_SOFTFP;

	/*
	 * note: do not set CR0_TS here.  npxinit() must do it after clearing
	 * gd_npxthread.  Otherwise a preemptive interrupt thread may panic
	 * in npxdna().
	 */
	crit_enter();
	load_cr0(rcr0() | CR0_MP);

#if NNPX > 0
	/* Initialize the npx (if any) for the current process. */
	npxinit(__INITIAL_NPXCW__);
#endif
	crit_exit();

	/*
	 * note: linux emulator needs edx to be 0x0 on entry, which is
	 * handled in execve simply by setting the 64 bit syscall
	 * return value to 0.
	 */
}

void
cpu_setregs(void)
{
	unsigned int cr0;

	cr0 = rcr0();
	cr0 |= CR0_NE;			/* Done by npxinit() */
	cr0 |= CR0_MP | CR0_TS;		/* Done at every execve() too. */
#ifdef I386_CPU
	if (cpu_class != CPUCLASS_386)
#endif
		cr0 |= CR0_WP | CR0_AM;
	load_cr0(cr0);
	load_gs(_udatasel);
}

static int
sysctl_machdep_adjkerntz(SYSCTL_HANDLER_ARGS)
{
	int error;
	error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,
		req);
	if (!error && req->newptr)
		resettodr();
	return (error);
}

SYSCTL_PROC(_machdep, CPU_ADJKERNTZ, adjkerntz, CTLTYPE_INT|CTLFLAG_RW,
	&adjkerntz, 0, sysctl_machdep_adjkerntz, "I", "");

SYSCTL_INT(_machdep, CPU_DISRTCSET, disable_rtc_set,
	CTLFLAG_RW, &disable_rtc_set, 0, "");

SYSCTL_STRUCT(_machdep, CPU_BOOTINFO, bootinfo, 
	CTLFLAG_RD, &bootinfo, bootinfo, "");

SYSCTL_INT(_machdep, CPU_WALLCLOCK, wall_cmos_clock,
	CTLFLAG_RW, &wall_cmos_clock, 0, "");

extern u_long bootdev;		/* not a dev_t - encoding is different */
SYSCTL_ULONG(_machdep, OID_AUTO, guessed_bootdev,
	CTLFLAG_RD, &bootdev, 0, "Boot device (not in dev_t format)");

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

int _default_ldt;
union descriptor gdt[NGDT * MAXCPU];	/* global descriptor table */
static struct gate_descriptor idt0[NIDT];
struct gate_descriptor *idt = &idt0[0];	/* interrupt descriptor table */
union descriptor ldt[NLDT];		/* local descriptor table */

/* table descriptors - used to load tables by cpu */
struct region_descriptor r_gdt, r_idt;

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern int has_f00f_bug;
#endif

static struct i386tss dblfault_tss;
static char dblfault_stack[PAGE_SIZE];

extern  struct user *proc0paddr;


/* software prototypes -- in more palatable form */
struct soft_segment_descriptor gdt_segs[] = {
/* GNULL_SEL	0 Null Descriptor */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GCODE_SEL	1 Code Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GDATA_SEL	2 Data Descriptor for kernel */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GPRIV_SEL	3 SMP Per-Processor Private Data Descriptor */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GPROC0_SEL	4 Proc 0 Tss Descriptor */
{
	0x0,			/* segment base address */
	sizeof(struct i386tss)-1,/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GLDT_SEL	5 LDT Descriptor */
{	(int) ldt,		/* segment base address  */
	sizeof(ldt)-1,		/* length - all address space */
	SDT_SYSLDT,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GUSERLDT_SEL	6 User LDT Descriptor per process */
{	(int) ldt,		/* segment base address  */
	(512 * sizeof(union descriptor)-1),		/* length */
	SDT_SYSLDT,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GTGATE_SEL	7 Null Descriptor - Placeholder */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GBIOSLOWMEM_SEL 8 BIOS access to realmode segment 0x40, must be #8 in GDT */
{	0x400,			/* segment base address */
	0xfffff,		/* length */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GPANIC_SEL	9 Panic Tss Descriptor */
{	(int) &dblfault_tss,	/* segment base address  */
	sizeof(struct i386tss)-1,/* length - all address space */
	SDT_SYS386TSS,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* unused - default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GBIOSCODE32_SEL 10 BIOS 32-bit interface (32bit Code) */
{	0,			/* segment base address (overwritten)  */
	0xfffff,		/* length */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GBIOSCODE16_SEL 11 BIOS 32-bit interface (16bit Code) */
{	0,			/* segment base address (overwritten)  */
	0xfffff,		/* length */
	SDT_MEMERA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GBIOSDATA_SEL 12 BIOS 32-bit interface (Data) */
{	0,			/* segment base address (overwritten) */
	0xfffff,		/* length */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GBIOSUTIL_SEL 13 BIOS 16-bit interface (Utility) */
{	0,			/* segment base address (overwritten) */
	0xfffff,		/* length */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GBIOSARGS_SEL 14 BIOS 16-bit interface (Arguments) */
{	0,			/* segment base address (overwritten) */
	0xfffff,		/* length */
	SDT_MEMRWA,		/* segment type */
	0,			/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
/* GTLS_START 15 TLS */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GTLS_START+1 16 TLS */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
/* GTLS_END 17 TLS */
{	0x0,			/* segment base address  */
	0x0,			/* length */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
};

static struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Code Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMERA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
	/* Null Descriptor - overwritten by call gate */
{	0x0,			/* segment base address  */
	0x0,			/* length - all address space */
	0,			/* segment type */
	0,			/* segment descriptor priority level */
	0,			/* segment descriptor present */
	0, 0,
	0,			/* default 32 vs 16 bit size */
	0  			/* limit granularity (byte/page units)*/ },
	/* Data Descriptor for user */
{	0x0,			/* segment base address  */
	0xfffff,		/* length - all address space */
	SDT_MEMRWA,		/* segment type */
	SEL_UPL,		/* segment descriptor priority level */
	1,			/* segment descriptor present */
	0, 0,
	1,			/* default 32 vs 16 bit size */
	1  			/* limit granularity (byte/page units)*/ },
};

void
setidt(idx, func, typ, dpl, selec)
	int idx;
	inthand_t *func;
	int typ;
	int dpl;
	int selec;
{
	struct gate_descriptor *ip;

	ip = idt + idx;
	ip->gd_looffset = (int)func;
	ip->gd_selector = selec;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((int)func)>>16 ;
}

#define	IDTVEC(name)	__CONCAT(X,name)

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm), IDTVEC(syscall),
	IDTVEC(rsvd0);
extern inthand_t
	IDTVEC(int0x80_syscall), IDTVEC(int0x81_syscall),
	IDTVEC(int0x82_syscall);

#ifdef DEBUG_INTERRUPTS
extern inthand_t *Xrsvdary[256];
#endif

void
sdtossd(sd, ssd)
	struct segment_descriptor *sd;
	struct soft_segment_descriptor *ssd;
{
	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

#define PHYSMAP_SIZE	(2 * 10)

/*
 * Populate the (physmap) array with base/bound pairs describing the
 * available physical memory in the system, then test this memory and
 * build the phys_avail array describing the actually-available memory.
 *
 * If we cannot accurately determine the physical memory map, then use
 * value from the 0xE801 call, and failing that, the RTC.
 *
 * Total memory size may be set by the kernel environment variable
 * hw.physmem or the compile-time define MAXMEM.
 */
static void
getmemsize(int first)
{
	int i, physmap_idx, pa_indx;
	int hasbrokenint12;
	u_int basemem, extmem;
	struct vm86frame vmf;
	struct vm86context vmc;
	vm_offset_t pa, physmap[PHYSMAP_SIZE];
	pt_entry_t *pte;
	const char *cp;
	struct {
		u_int64_t base;
		u_int64_t length;
		u_int32_t type;
	} *smap;
	quad_t dcons_addr, dcons_size;

	hasbrokenint12 = 0;
	TUNABLE_INT_FETCH("hw.hasbrokenint12", &hasbrokenint12);
	bzero(&vmf, sizeof(struct vm86frame));
	bzero(physmap, sizeof(physmap));
	basemem = 0;

	/*
	 * Some newer BIOSes has broken INT 12H implementation which cause
	 * kernel panic immediately. In this case, we need to scan SMAP
	 * with INT 15:E820 first, then determine base memory size.
	 */
	if (hasbrokenint12) {
		goto int15e820;
	}

	/*
	 * Perform "base memory" related probes & setup.  If we get a crazy
	 * value give the bios some scribble space just in case.
	 */
	vm86_intcall(0x12, &vmf);
	basemem = vmf.vmf_ax;
	if (basemem > 640) {
		printf("Preposterous BIOS basemem of %uK, "
			"truncating to < 640K\n", basemem);
		basemem = 636;
	}

	/*
	 * XXX if biosbasemem is now < 640, there is a `hole'
	 * between the end of base memory and the start of
	 * ISA memory.  The hole may be empty or it may
	 * contain BIOS code or data.  Map it read/write so
	 * that the BIOS can write to it.  (Memory from 0 to
	 * the physical end of the kernel is mapped read-only
	 * to begin with and then parts of it are remapped.
	 * The parts that aren't remapped form holes that
	 * remain read-only and are unused by the kernel.
	 * The base memory area is below the physical end of
	 * the kernel and right now forms a read-only hole.
	 * The part of it from PAGE_SIZE to
	 * (trunc_page(biosbasemem * 1024) - 1) will be
	 * remapped and used by the kernel later.)
	 *
	 * This code is similar to the code used in
	 * pmap_mapdev, but since no memory needs to be
	 * allocated we simply change the mapping.
	 */
	for (pa = trunc_page(basemem * 1024);
	     pa < ISA_HOLE_START; pa += PAGE_SIZE) {
		pte = vtopte(pa + KERNBASE);
		*pte = pa | PG_RW | PG_V;
	}

	/*
	 * if basemem != 640, map pages r/w into vm86 page table so 
	 * that the bios can scribble on it.
	 */
	pte = vm86paddr;
	for (i = basemem / 4; i < 160; i++)
		pte[i] = (i << PAGE_SHIFT) | PG_V | PG_RW | PG_U;

int15e820:
	/*
	 * map page 1 R/W into the kernel page table so we can use it
	 * as a buffer.  The kernel will unmap this page later.
	 */
	pte = vtopte(KERNBASE + (1 << PAGE_SHIFT));
	*pte = (1 << PAGE_SHIFT) | PG_RW | PG_V;

	/*
	 * get memory map with INT 15:E820
	 */
#define SMAPSIZ 	sizeof(*smap)
#define SMAP_SIG	0x534D4150			/* 'SMAP' */

	vmc.npages = 0;
	smap = (void *)vm86_addpage(&vmc, 1, KERNBASE + (1 << PAGE_SHIFT));
	vm86_getptr(&vmc, (vm_offset_t)smap, &vmf.vmf_es, &vmf.vmf_di);

	physmap_idx = 0;
	vmf.vmf_ebx = 0;
	do {
		vmf.vmf_eax = 0xE820;
		vmf.vmf_edx = SMAP_SIG;
		vmf.vmf_ecx = SMAPSIZ;
		i = vm86_datacall(0x15, &vmf, &vmc);
		if (i || vmf.vmf_eax != SMAP_SIG)
			break;
		if (boothowto & RB_VERBOSE)
			printf("SMAP type=%02x base=%08x %08x len=%08x %08x\n",
				smap->type,
				*(u_int32_t *)((char *)&smap->base + 4),
				(u_int32_t)smap->base,
				*(u_int32_t *)((char *)&smap->length + 4),
				(u_int32_t)smap->length);

		if (smap->type != 0x01)
			goto next_run;

		if (smap->length == 0)
			goto next_run;

		if (smap->base >= 0xffffffff) {
			printf("%uK of memory above 4GB ignored\n",
			    (u_int)(smap->length / 1024));
			goto next_run;
		}

		for (i = 0; i <= physmap_idx; i += 2) {
			if (smap->base < physmap[i + 1]) {
				if (boothowto & RB_VERBOSE)
					printf(
	"Overlapping or non-montonic memory region, ignoring second region\n");
				goto next_run;
			}
		}

		if (smap->base == physmap[physmap_idx + 1]) {
			physmap[physmap_idx + 1] += smap->length;
			goto next_run;
		}

		physmap_idx += 2;
		if (physmap_idx == PHYSMAP_SIZE) {
			printf(
		"Too many segments in the physical address map, giving up\n");
			break;
		}
		physmap[physmap_idx] = smap->base;
		physmap[physmap_idx + 1] = smap->base + smap->length;
next_run:
		; /* fix GCC3.x warning */
	} while (vmf.vmf_ebx != 0);

	/*
	 * Perform "base memory" related probes & setup based on SMAP
	 */
	if (basemem == 0) {
		for (i = 0; i <= physmap_idx; i += 2) {
			if (physmap[i] == 0x00000000) {
				basemem = physmap[i + 1] / 1024;
				break;
			}
		}

		if (basemem == 0) {
			basemem = 640;
		}

		if (basemem > 640) {
			printf("Preposterous BIOS basemem of %uK, truncating to 640K\n",
				basemem);
			basemem = 640;
		}

		for (pa = trunc_page(basemem * 1024);
		     pa < ISA_HOLE_START; pa += PAGE_SIZE) {
			pte = vtopte(pa + KERNBASE);
			*pte = pa | PG_RW | PG_V;
		}

		pte = vm86paddr;
		for (i = basemem / 4; i < 160; i++)
			pte[i] = (i << PAGE_SHIFT) | PG_V | PG_RW | PG_U;
	}

	if (physmap[1] != 0)
		goto physmap_done;

	/*
	 * If we failed above, try memory map with INT 15:E801
	 */
	vmf.vmf_ax = 0xE801;
	if (vm86_intcall(0x15, &vmf) == 0) {
		extmem = vmf.vmf_cx + vmf.vmf_dx * 64;
	} else {
#if 0
		vmf.vmf_ah = 0x88;
		vm86_intcall(0x15, &vmf);
		extmem = vmf.vmf_ax;
#else
		/*
		 * Prefer the RTC value for extended memory.
		 */
		extmem = rtcin(RTC_EXTLO) + (rtcin(RTC_EXTHI) << 8);
#endif
	}

	/*
	 * Special hack for chipsets that still remap the 384k hole when
	 * there's 16MB of memory - this really confuses people that
	 * are trying to use bus mastering ISA controllers with the
	 * "16MB limit"; they only have 16MB, but the remapping puts
	 * them beyond the limit.
	 *
	 * If extended memory is between 15-16MB (16-17MB phys address range),
	 *	chop it to 15MB.
	 */
	if ((extmem > 15 * 1024) && (extmem < 16 * 1024))
		extmem = 15 * 1024;

	physmap[0] = 0;
	physmap[1] = basemem * 1024;
	physmap_idx = 2;
	physmap[physmap_idx] = 0x100000;
	physmap[physmap_idx + 1] = physmap[physmap_idx] + extmem * 1024;

physmap_done:
	/*
	 * Now, physmap contains a map of physical memory.
	 */

#ifdef SMP
	/* make hole for AP bootstrap code YYY */
	physmap[1] = mp_bootaddress(physmap[1] / 1024);

	/* look for the MP hardware - needed for apic addresses */
	mp_probe();
#endif

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".  We may adjust this 
	 * based on ``hw.physmem'' and the results of the memory test.
	 */
	Maxmem = atop(physmap[physmap_idx + 1]);

#ifdef MAXMEM
	Maxmem = MAXMEM / 4;
#endif

	/*
	 * hw.physmem is a size in bytes; we also allow k, m, and g suffixes
	 * for the appropriate modifiers.  This overrides MAXMEM.
	 */
	if ((cp = getenv("hw.physmem")) != NULL) {
		u_int64_t AllowMem, sanity;
		char *ep;

		sanity = AllowMem = strtouq(cp, &ep, 0);
		if ((ep != cp) && (*ep != 0)) {
			switch(*ep) {
			case 'g':
			case 'G':
				AllowMem <<= 10;
			case 'm':
			case 'M':
				AllowMem <<= 10;
			case 'k':
			case 'K':
				AllowMem <<= 10;
				break;
			default:
				AllowMem = sanity = 0;
			}
			if (AllowMem < sanity)
				AllowMem = 0;
		}
		if (AllowMem == 0)
			printf("Ignoring invalid memory size of '%s'\n", cp);
		else
			Maxmem = atop(AllowMem);
	}

	if (atop(physmap[physmap_idx + 1]) != Maxmem &&
	    (boothowto & RB_VERBOSE))
		printf("Physical memory use set to %lluK\n", Maxmem * 4);

	/*
	 * If Maxmem has been increased beyond what the system has detected,
	 * extend the last memory segment to the new limit.
	 */ 
	if (atop(physmap[physmap_idx + 1]) < Maxmem)
		physmap[physmap_idx + 1] = ptoa(Maxmem);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap(first, 0);

	/*
	 * Size up each available chunk of physical memory.
	 */
	physmap[0] = PAGE_SIZE;		/* mask off page 0 */
	pa_indx = 0;
	phys_avail[pa_indx++] = physmap[0];
	phys_avail[pa_indx] = physmap[0];
	pte = CMAP1;

	/*
	 * Get dcons buffer address
	 */
	if (getenv_quad("dcons.addr", &dcons_addr) == 0 ||
	    getenv_quad("dcons.size", &dcons_size) == 0)
		dcons_addr = 0;

	/*
	 * physmap is in bytes, so when converting to page boundaries,
	 * round up the start address and round down the end address.
	 */
	for (i = 0; i <= physmap_idx; i += 2) {
		vm_offset_t end;

		end = ptoa(Maxmem);
		if (physmap[i + 1] < end)
			end = trunc_page(physmap[i + 1]);
		for (pa = round_page(physmap[i]); pa < end; pa += PAGE_SIZE) {
			int tmp, page_bad;
#if 0
			int *ptr = 0;
#else
			int *ptr = (int *)CADDR1;
#endif

			/*
			 * block out kernel memory as not available.
			 */
			if (pa >= 0x100000 && pa < first)
				continue;
	
			/*
			 * block out dcons buffer
			 */
			if (dcons_addr > 0
			    && pa >= trunc_page(dcons_addr)
			    && pa < dcons_addr + dcons_size)
				continue;

			page_bad = FALSE;

			/*
			 * map page into kernel: valid, read/write,non-cacheable
			 */
			*pte = pa | PG_V | PG_RW | PG_N;
			cpu_invltlb();

			tmp = *(int *)ptr;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)ptr = 0xaaaaaaaa;
			if (*(volatile int *)ptr != 0xaaaaaaaa) {
				page_bad = TRUE;
			}
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)ptr = 0x55555555;
			if (*(volatile int *)ptr != 0x55555555) {
			page_bad = TRUE;
			}
			/*
			 * Test for all 1's
			 */
			*(volatile int *)ptr = 0xffffffff;
			if (*(volatile int *)ptr != 0xffffffff) {
				page_bad = TRUE;
			}
			/*
			 * Test for all 0's
			 */
			*(volatile int *)ptr = 0x0;
			if (*(volatile int *)ptr != 0x0) {
				page_bad = TRUE;
			}
			/*
			 * Restore original value.
			 */
			*(int *)ptr = tmp;

			/*
			 * Adjust array of valid/good pages.
			 */
			if (page_bad == TRUE) {
				continue;
			}
			/*
			 * If this good page is a continuation of the
			 * previous set of good pages, then just increase
			 * the end pointer. Otherwise start a new chunk.
			 * Note that "end" points one higher than end,
			 * making the range >= start and < end.
			 * If we're also doing a speculative memory
			 * test and we at or past the end, bump up Maxmem
			 * so that we keep going. The first bad page
			 * will terminate the loop.
			 */
			if (phys_avail[pa_indx] == pa) {
				phys_avail[pa_indx] += PAGE_SIZE;
			} else {
				pa_indx++;
				if (pa_indx >= PHYS_AVAIL_ARRAY_END) {
					printf("Too many holes in the physical address space, giving up\n");
					pa_indx--;
					break;
				}
				phys_avail[pa_indx++] = pa;	/* start */
				phys_avail[pa_indx] = pa + PAGE_SIZE;	/* end */
			}
			physmem++;
		}
	}
	*pte = 0;
	cpu_invltlb();

	/*
	 * XXX
	 * The last chunk must contain at least one page plus the message
	 * buffer to avoid complicating other code (message buffer address
	 * calculation, etc.).
	 */
	while (phys_avail[pa_indx - 1] + PAGE_SIZE +
	    round_page(MSGBUF_SIZE) >= phys_avail[pa_indx]) {
		physmem -= atop(phys_avail[pa_indx] - phys_avail[pa_indx - 1]);
		phys_avail[pa_indx--] = 0;
		phys_avail[pa_indx--] = 0;
	}

	Maxmem = atop(phys_avail[pa_indx]);

	/* Trim off space for the message buffer. */
	phys_avail[pa_indx] -= round_page(MSGBUF_SIZE);

	avail_end = phys_avail[pa_indx];
}

/*
 * IDT VECTORS:
 *	0	Divide by zero
 *	1	Debug
 *	2	NMI
 *	3	BreakPoint
 *	4	OverFlow
 *	5	Bound-Range
 *	6	Invalid OpCode
 *	7	Device Not Available (x87)
 *	8	Double-Fault
 *	9	Coprocessor Segment overrun (unsupported, reserved)
 *	10	Invalid-TSS
 *	11	Segment not present
 *	12	Stack
 *	13	General Protection
 *	14	Page Fault
 *	15	Reserved
 *	16	x87 FP Exception pending
 *	17	Alignment Check
 *	18	Machine Check
 *	19	SIMD floating point
 *	20-31	reserved
 *	32-255	INTn/external sources
 */
void
init386(int first)
{
	struct gate_descriptor *gdp;
	int gsel_tss, metadata_missing, off, x;
	struct mdglobaldata *gd;

	/*
	 * Prevent lowering of the ipl if we call tsleep() early.
	 */
	gd = &CPU_prvspace[0].mdglobaldata;
	bzero(gd, sizeof(*gd));

	gd->mi.gd_curthread = &thread0;

	atdevbase = ISA_HOLE_START + KERNBASE;

	metadata_missing = 0;
	if (bootinfo.bi_modulep) {
		preload_metadata = (caddr_t)bootinfo.bi_modulep + KERNBASE;
		preload_bootstrap_relocate(KERNBASE);
	} else {
		metadata_missing = 1;
	}
	if (bootinfo.bi_envp)
		kern_envp = (caddr_t)bootinfo.bi_envp + KERNBASE;

	/*
	 * start with one cpu.  Note: ncpus2_shift and ncpus2_mask are left
	 * at 0.
	 */
	ncpus = 1;
	ncpus2 = 1;
	/* Init basic tunables, hz etc */
	init_param1();

	/*
	 * make gdt memory segments, the code segment goes up to end of the
	 * page with etext in it, the data segment goes to the end of
	 * the address space
	 */
	/*
	 * XXX text protection is temporarily (?) disabled.  The limit was
	 * i386_btop(round_page(etext)) - 1.
	 */
	gdt_segs[GCODE_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GDATA_SEL].ssd_limit = atop(0 - 1);

	gdt_segs[GPRIV_SEL].ssd_limit =
		atop(sizeof(struct privatespace) - 1);
	gdt_segs[GPRIV_SEL].ssd_base = (int) &CPU_prvspace[0];
	gdt_segs[GPROC0_SEL].ssd_base =
		(int) &CPU_prvspace[0].mdglobaldata.gd_common_tss;

	gd->mi.gd_prvspace = &CPU_prvspace[0];

	/*
	 * Note: on both UP and SMP curthread must be set non-NULL
	 * early in the boot sequence because the system assumes
	 * that 'curthread' is never NULL.
	 */

	for (x = 0; x < NGDT; x++) {
#ifdef BDE_DEBUGGER
		/* avoid overwriting db entries with APM ones */
		if (x >= GAPMCODE32_SEL && x <= GAPMDATA_SEL)
			continue;
#endif
		ssdtosd(&gdt_segs[x], &gdt[x].sd);
	}

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base =  (int) gdt;
	lgdt(&r_gdt);

	mi_gdinit(&gd->mi, 0);
	cpu_gdinit(gd, 0);
	lwkt_init_thread(&thread0, proc0paddr, LWKT_THREAD_STACK, 0, &gd->mi);
	lwkt_set_comm(&thread0, "thread0");
	proc0.p_addr = (void *)thread0.td_kstack;
	proc0.p_thread = &thread0;
	varsymset_init(&proc0.p_varsymset, NULL);
	thread0.td_flags |= TDF_RUNNING;
	thread0.td_proc = &proc0;
	thread0.td_switch = cpu_heavy_switch;	/* YYY eventually LWKT */
	safepri = thread0.td_cpl = SWI_MASK | HWI_MASK;

	/* make ldt memory segments */
	/*
	 * XXX - VM_MAXUSER_ADDRESS is an end address, not a max.  And it
	 * should be spelled ...MAX_USER...
	 */
	ldt_segs[LUCODE_SEL].ssd_limit = atop(VM_MAXUSER_ADDRESS - 1);
	ldt_segs[LUDATA_SEL].ssd_limit = atop(VM_MAXUSER_ADDRESS - 1);
	for (x = 0; x < sizeof ldt_segs / sizeof ldt_segs[0]; x++)
		ssdtosd(&ldt_segs[x], &ldt[x].sd);

	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	gd->gd_currentldt = _default_ldt;
	/* spinlocks and the BGL */
	init_locks();

	/* exceptions */
	for (x = 0; x < NIDT; x++) {
#ifdef DEBUG_INTERRUPTS
		setidt(x, Xrsvdary[x], SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#else
		setidt(x, &IDTVEC(rsvd0), SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
	}
	setidt(0, &IDTVEC(div),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(1, &IDTVEC(dbg),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(2, &IDTVEC(nmi),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(3, &IDTVEC(bpt),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(4, &IDTVEC(ofl),  SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(5, &IDTVEC(bnd),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(6, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(7, &IDTVEC(dna),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(8, 0,  SDT_SYSTASKGT, SEL_KPL, GSEL(GPANIC_SEL, SEL_KPL));
	setidt(9, &IDTVEC(fpusegm),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(10, &IDTVEC(tss),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(11, &IDTVEC(missing),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(12, &IDTVEC(stk),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(13, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(14, &IDTVEC(page),  SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(15, &IDTVEC(rsvd0),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(16, &IDTVEC(fpu),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(17, &IDTVEC(align), SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(18, &IDTVEC(mchk),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(19, &IDTVEC(xmm), SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(0x80, &IDTVEC(int0x80_syscall),
			SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
 	setidt(0x81, &IDTVEC(int0x81_syscall),
			SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(0x82, &IDTVEC(int0x82_syscall),
 			SDT_SYS386TGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));

	r_idt.rd_limit = sizeof(idt0) - 1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);

	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

	if (metadata_missing)
		printf("WARNING: loader(8) metadata is missing!\n");

#if	NISA >0
	isa_defaultirq();
#endif
	rand_initialize();

#ifdef DDB
	kdb_init();
	if (boothowto & RB_KDB)
		Debugger("Boot flags requested debugger");
#endif

	finishidentcpu();	/* Final stage of CPU initialization */
	setidt(6, &IDTVEC(ill),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(13, &IDTVEC(prot),  SDT_SYS386TGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	initializecpu();	/* Initialize CPU registers */

	/*
	 * make an initial tss so cpu can get interrupt stack on syscall!
	 * The 16 bytes is to save room for a VM86 context.
	 */
	gd->gd_common_tss.tss_esp0 = (int) thread0.td_pcb - 16;
	gd->gd_common_tss.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL) ;
	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	gd->gd_tss_gdt = &gdt[GPROC0_SEL].sd;
	gd->gd_common_tssd = *gd->gd_tss_gdt;
	gd->gd_common_tss.tss_ioopt = (sizeof gd->gd_common_tss) << 16;
	ltr(gsel_tss);

	dblfault_tss.tss_esp = dblfault_tss.tss_esp0 = dblfault_tss.tss_esp1 =
	    dblfault_tss.tss_esp2 = (int) &dblfault_stack[sizeof(dblfault_stack)];
	dblfault_tss.tss_ss = dblfault_tss.tss_ss0 = dblfault_tss.tss_ss1 =
	    dblfault_tss.tss_ss2 = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_cr3 = (int)IdlePTD;
	dblfault_tss.tss_eip = (int) dblfault_handler;
	dblfault_tss.tss_eflags = PSL_KERNEL;
	dblfault_tss.tss_ds = dblfault_tss.tss_es =
	    dblfault_tss.tss_gs = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss.tss_fs = GSEL(GPRIV_SEL, SEL_KPL);
	dblfault_tss.tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	dblfault_tss.tss_ldt = GSEL(GLDT_SEL, SEL_KPL);

	vm86_initialize();
	getmemsize(first);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

	/* Map the message buffer. */
	for (off = 0; off < round_page(MSGBUF_SIZE); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, avail_end + off);

	msgbufinit(msgbufp, MSGBUF_SIZE);

	/* make a call gate to reenter kernel with */
	gdp = &ldt[LSYS5CALLS_SEL].gd;

	x = (int) &IDTVEC(syscall);
	gdp->gd_looffset = x++;
	gdp->gd_selector = GSEL(GCODE_SEL,SEL_KPL);
	gdp->gd_stkcpy = 1;
	gdp->gd_type = SDT_SYS386CGT;
	gdp->gd_dpl = SEL_UPL;
	gdp->gd_p = 1;
	gdp->gd_hioffset = ((int) &IDTVEC(syscall)) >>16;

	/* XXX does this work? */
	ldt[LBSDICALLS_SEL] = ldt[LSYS5CALLS_SEL];
	ldt[LSOL26CALLS_SEL] = ldt[LSYS5CALLS_SEL];

	/* transfer to user mode */

	_ucodesel = LSEL(LUCODE_SEL, SEL_UPL);
	_udatasel = LSEL(LUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_pcb->pcb_cr3 = (int)IdlePTD;	/* should already be setup */
	thread0.td_pcb->pcb_ext = 0;
	proc0.p_md.md_regs = &proc0_tf;
}

/*
 * Initialize machine-dependant portions of the global data structure.
 * Note that the global data area and cpu0's idlestack in the private
 * data space were allocated in locore.
 *
 * Note: the idlethread's cpl is 0
 *
 * WARNING!  Called from early boot, 'mycpu' may not work yet.
 */
void
cpu_gdinit(struct mdglobaldata *gd, int cpu)
{
	if (cpu)
		gd->mi.gd_curthread = &gd->mi.gd_idlethread;

	lwkt_init_thread(&gd->mi.gd_idlethread, 
			gd->mi.gd_prvspace->idlestack, 
			sizeof(gd->mi.gd_prvspace->idlestack), 0, &gd->mi);
	lwkt_set_comm(&gd->mi.gd_idlethread, "idle_%d", cpu);
	gd->mi.gd_idlethread.td_switch = cpu_lwkt_switch;
	gd->mi.gd_idlethread.td_sp -= sizeof(void *);
	*(void **)gd->mi.gd_idlethread.td_sp = cpu_idle_restore;
}

int
is_globaldata_space(vm_offset_t saddr, vm_offset_t eaddr)
{
	if (saddr >= (vm_offset_t)&CPU_prvspace[0] &&
	    eaddr <= (vm_offset_t)&CPU_prvspace[MAXCPU]) {
		return (TRUE);
	}
	return (FALSE);
}

struct globaldata *
globaldata_find(int cpu)
{
	KKASSERT(cpu >= 0 && cpu < ncpus);
	return(&CPU_prvspace[cpu].mdglobaldata.mi);
}

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
static void f00f_hack(void *unused);
SYSINIT(f00f_hack, SI_SUB_INTRINSIC, SI_ORDER_FIRST, f00f_hack, NULL);

static void
f00f_hack(void *unused) 
{
	struct gate_descriptor *new_idt;
	vm_offset_t tmp;

	if (!has_f00f_bug)
		return;

	printf("Intel Pentium detected, installing workaround for F00F bug\n");

	r_idt.rd_limit = sizeof(idt0) - 1;

	tmp = kmem_alloc(kernel_map, PAGE_SIZE * 2);
	if (tmp == 0)
		panic("kmem_alloc returned 0");
	if (((unsigned int)tmp & (PAGE_SIZE-1)) != 0)
		panic("kmem_alloc returned non-page-aligned memory");
	/* Put the first seven entries in the lower page */
	new_idt = (struct gate_descriptor*)(tmp + PAGE_SIZE - (7*8));
	bcopy(idt, new_idt, sizeof(idt0));
	r_idt.rd_base = (int)new_idt;
	lidt(&r_idt);
	idt = new_idt;
	if (vm_map_protect(kernel_map, tmp, tmp + PAGE_SIZE,
			   VM_PROT_READ, FALSE) != KERN_SUCCESS)
		panic("vm_map_protect failed");
	return;
}
#endif /* defined(I586_CPU) && !NO_F00F_HACK */

int
ptrace_set_pc(p, addr)
	struct proc *p;
	unsigned long addr;
{
	p->p_md.md_regs->tf_eip = addr;
	return (0);
}

int
ptrace_single_step(p)
	struct proc *p;
{
	p->p_md.md_regs->tf_eflags |= PSL_T;
	return (0);
}

int ptrace_read_u_check(p, addr, len)
	struct proc *p;
	vm_offset_t addr;
	size_t len;
{
	vm_offset_t gap;

	if ((vm_offset_t) (addr + len) < addr)
		return EPERM;
	if ((vm_offset_t) (addr + len) <= sizeof(struct user))
		return 0;

	gap = (char *) p->p_md.md_regs - (char *) p->p_addr;
	
	if ((vm_offset_t) addr < gap)
		return EPERM;
	if ((vm_offset_t) (addr + len) <= 
	    (vm_offset_t) (gap + sizeof(struct trapframe)))
		return 0;
	return EPERM;
}

int ptrace_write_u(p, off, data)
	struct proc *p;
	vm_offset_t off;
	long data;
{
	struct trapframe frame_copy;
	vm_offset_t min;
	struct trapframe *tp;

	/*
	 * Privileged kernel state is scattered all over the user area.
	 * Only allow write access to parts of regs and to fpregs.
	 */
	min = (char *)p->p_md.md_regs - (char *)p->p_addr;
	if (off >= min && off <= min + sizeof(struct trapframe) - sizeof(int)) {
		tp = p->p_md.md_regs;
		frame_copy = *tp;
		*(int *)((char *)&frame_copy + (off - min)) = data;
		if (!EFL_SECURE(frame_copy.tf_eflags, tp->tf_eflags) ||
		    !CS_SECURE(frame_copy.tf_cs))
			return (EINVAL);
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}

	/*
	 * The PCB is at the end of the user area YYY
	 */
	min = (char *)p->p_thread->td_pcb - (char *)p->p_addr;
	min += offsetof(struct pcb, pcb_save);
	if (off >= min && off <= min + sizeof(union savefpu) - sizeof(int)) {
		*(int*)((char *)p->p_addr + off) = data;
		return (0);
	}
	return (EFAULT);
}

int
fill_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = p->p_md.md_regs;
	regs->r_fs = tp->tf_fs;
	regs->r_es = tp->tf_es;
	regs->r_ds = tp->tf_ds;
	regs->r_edi = tp->tf_edi;
	regs->r_esi = tp->tf_esi;
	regs->r_ebp = tp->tf_ebp;
	regs->r_ebx = tp->tf_ebx;
	regs->r_edx = tp->tf_edx;
	regs->r_ecx = tp->tf_ecx;
	regs->r_eax = tp->tf_eax;
	regs->r_eip = tp->tf_eip;
	regs->r_cs = tp->tf_cs;
	regs->r_eflags = tp->tf_eflags;
	regs->r_esp = tp->tf_esp;
	regs->r_ss = tp->tf_ss;
	pcb = p->p_thread->td_pcb;
	regs->r_gs = pcb->pcb_gs;
	return (0);
}

int
set_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = p->p_md.md_regs;
	if (!EFL_SECURE(regs->r_eflags, tp->tf_eflags) ||
	    !CS_SECURE(regs->r_cs))
		return (EINVAL);
	tp->tf_fs = regs->r_fs;
	tp->tf_es = regs->r_es;
	tp->tf_ds = regs->r_ds;
	tp->tf_edi = regs->r_edi;
	tp->tf_esi = regs->r_esi;
	tp->tf_ebp = regs->r_ebp;
	tp->tf_ebx = regs->r_ebx;
	tp->tf_edx = regs->r_edx;
	tp->tf_ecx = regs->r_ecx;
	tp->tf_eax = regs->r_eax;
	tp->tf_eip = regs->r_eip;
	tp->tf_cs = regs->r_cs;
	tp->tf_eflags = regs->r_eflags;
	tp->tf_esp = regs->r_esp;
	tp->tf_ss = regs->r_ss;
	pcb = p->p_thread->td_pcb;
	pcb->pcb_gs = regs->r_gs;
	return (0);
}

#ifndef CPU_DISABLE_SSE
static void
fill_fpregs_xmm(sv_xmm, sv_87)
	struct savexmm *sv_xmm;
	struct save87 *sv_87;
{
	struct env87 *penv_87 = &sv_87->sv_env;
	struct envxmm *penv_xmm = &sv_xmm->sv_env;
	int i;

	/* FPU control/status */
	penv_87->en_cw = penv_xmm->en_cw;
	penv_87->en_sw = penv_xmm->en_sw;
	penv_87->en_tw = penv_xmm->en_tw;
	penv_87->en_fip = penv_xmm->en_fip;
	penv_87->en_fcs = penv_xmm->en_fcs;
	penv_87->en_opcode = penv_xmm->en_opcode;
	penv_87->en_foo = penv_xmm->en_foo;
	penv_87->en_fos = penv_xmm->en_fos;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_87->sv_ac[i] = sv_xmm->sv_fp[i].fp_acc;

	sv_87->sv_ex_sw = sv_xmm->sv_ex_sw;
}

static void
set_fpregs_xmm(sv_87, sv_xmm)
	struct save87 *sv_87;
	struct savexmm *sv_xmm;
{
	struct env87 *penv_87 = &sv_87->sv_env;
	struct envxmm *penv_xmm = &sv_xmm->sv_env;
	int i;

	/* FPU control/status */
	penv_xmm->en_cw = penv_87->en_cw;
	penv_xmm->en_sw = penv_87->en_sw;
	penv_xmm->en_tw = penv_87->en_tw;
	penv_xmm->en_fip = penv_87->en_fip;
	penv_xmm->en_fcs = penv_87->en_fcs;
	penv_xmm->en_opcode = penv_87->en_opcode;
	penv_xmm->en_foo = penv_87->en_foo;
	penv_xmm->en_fos = penv_87->en_fos;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_xmm->sv_fp[i].fp_acc = sv_87->sv_ac[i];

	sv_xmm->sv_ex_sw = sv_87->sv_ex_sw;
}
#endif /* CPU_DISABLE_SSE */

int
fill_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
#ifndef CPU_DISABLE_SSE
	if (cpu_fxsr) {
		fill_fpregs_xmm(&p->p_thread->td_pcb->pcb_save.sv_xmm,
						(struct save87 *)fpregs);
		return (0);
	}
#endif /* CPU_DISABLE_SSE */
	bcopy(&p->p_thread->td_pcb->pcb_save.sv_87, fpregs, sizeof *fpregs);
	return (0);
}

int
set_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
#ifndef CPU_DISABLE_SSE
	if (cpu_fxsr) {
		set_fpregs_xmm((struct save87 *)fpregs,
				       &p->p_thread->td_pcb->pcb_save.sv_xmm);
		return (0);
	}
#endif /* CPU_DISABLE_SSE */
	bcopy(fpregs, &p->p_thread->td_pcb->pcb_save.sv_87, sizeof *fpregs);
	return (0);
}

int
fill_dbregs(p, dbregs)
	struct proc *p;
	struct dbreg *dbregs;
{
	struct pcb *pcb;

        if (p == NULL) {
                dbregs->dr0 = rdr0();
                dbregs->dr1 = rdr1();
                dbregs->dr2 = rdr2();
                dbregs->dr3 = rdr3();
                dbregs->dr4 = rdr4();
                dbregs->dr5 = rdr5();
                dbregs->dr6 = rdr6();
                dbregs->dr7 = rdr7();
        }
        else {
                pcb = p->p_thread->td_pcb;
                dbregs->dr0 = pcb->pcb_dr0;
                dbregs->dr1 = pcb->pcb_dr1;
                dbregs->dr2 = pcb->pcb_dr2;
                dbregs->dr3 = pcb->pcb_dr3;
                dbregs->dr4 = 0;
                dbregs->dr5 = 0;
                dbregs->dr6 = pcb->pcb_dr6;
                dbregs->dr7 = pcb->pcb_dr7;
        }
	return (0);
}

int
set_dbregs(p, dbregs)
	struct proc *p;
	struct dbreg *dbregs;
{
	struct pcb *pcb;
	int i;
	u_int32_t mask1, mask2;

	if (p == NULL) {
		load_dr0(dbregs->dr0);
		load_dr1(dbregs->dr1);
		load_dr2(dbregs->dr2);
		load_dr3(dbregs->dr3);
		load_dr4(dbregs->dr4);
		load_dr5(dbregs->dr5);
		load_dr6(dbregs->dr6);
		load_dr7(dbregs->dr7);
	}
	else {
		/*
		 * Don't let an illegal value for dr7 get set.	Specifically,
		 * check for undefined settings.  Setting these bit patterns
		 * result in undefined behaviour and can lead to an unexpected
		 * TRCTRAP.
		 */
		for (i = 0, mask1 = 0x3<<16, mask2 = 0x2<<16; i < 8; 
		     i++, mask1 <<= 2, mask2 <<= 2)
			if ((dbregs->dr7 & mask1) == mask2)
				return (EINVAL);
		
		pcb = p->p_thread->td_pcb;
		
		/*
		 * Don't let a process set a breakpoint that is not within the
		 * process's address space.  If a process could do this, it
		 * could halt the system by setting a breakpoint in the kernel
		 * (if ddb was enabled).  Thus, we need to check to make sure
		 * that no breakpoints are being enabled for addresses outside
		 * process's address space, unless, perhaps, we were called by
		 * uid 0.
		 *
		 * XXX - what about when the watched area of the user's
		 * address space is written into from within the kernel
		 * ... wouldn't that still cause a breakpoint to be generated
		 * from within kernel mode?
		 */
		
		if (suser_cred(p->p_ucred, 0) != 0) {
			if (dbregs->dr7 & 0x3) {
				/* dr0 is enabled */
				if (dbregs->dr0 >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			
			if (dbregs->dr7 & (0x3<<2)) {
				/* dr1 is enabled */
				if (dbregs->dr1 >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			
			if (dbregs->dr7 & (0x3<<4)) {
				/* dr2 is enabled */
				if (dbregs->dr2 >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
			
			if (dbregs->dr7 & (0x3<<6)) {
				/* dr3 is enabled */
				if (dbregs->dr3 >= VM_MAXUSER_ADDRESS)
					return (EINVAL);
			}
		}
		
		pcb->pcb_dr0 = dbregs->dr0;
		pcb->pcb_dr1 = dbregs->dr1;
		pcb->pcb_dr2 = dbregs->dr2;
		pcb->pcb_dr3 = dbregs->dr3;
		pcb->pcb_dr6 = dbregs->dr6;
		pcb->pcb_dr7 = dbregs->dr7;
		
		pcb->pcb_flags |= PCB_DBREGS;
	}

	return (0);
}

/*
 * Return > 0 if a hardware breakpoint has been hit, and the
 * breakpoint was in user space.  Return 0, otherwise.
 */
int
user_dbreg_trap(void)
{
        u_int32_t dr7, dr6; /* debug registers dr6 and dr7 */
        u_int32_t bp;       /* breakpoint bits extracted from dr6 */
        int nbp;            /* number of breakpoints that triggered */
        caddr_t addr[4];    /* breakpoint addresses */
        int i;
        
        dr7 = rdr7();
        if ((dr7 & 0x000000ff) == 0) {
                /*
                 * all GE and LE bits in the dr7 register are zero,
                 * thus the trap couldn't have been caused by the
                 * hardware debug registers
                 */
                return 0;
        }

        nbp = 0;
        dr6 = rdr6();
        bp = dr6 & 0x0000000f;

        if (!bp) {
                /*
                 * None of the breakpoint bits are set meaning this
                 * trap was not caused by any of the debug registers
                 */
                return 0;
        }

        /*
         * at least one of the breakpoints were hit, check to see
         * which ones and if any of them are user space addresses
         */

        if (bp & 0x01) {
                addr[nbp++] = (caddr_t)rdr0();
        }
        if (bp & 0x02) {
                addr[nbp++] = (caddr_t)rdr1();
        }
        if (bp & 0x04) {
                addr[nbp++] = (caddr_t)rdr2();
        }
        if (bp & 0x08) {
                addr[nbp++] = (caddr_t)rdr3();
        }

        for (i=0; i<nbp; i++) {
                if (addr[i] <
                    (caddr_t)VM_MAXUSER_ADDRESS) {
                        /*
                         * addr[i] is in user space
                         */
                        return nbp;
                }
        }

        /*
         * None of the breakpoints are in user space.
         */
        return 0;
}


#ifndef DDB
void
Debugger(const char *msg)
{
	printf("Debugger(\"%s\") called.\n", msg);
}
#endif /* no DDB */

#include <machine/apicvar.h>

/*
 * Provide stub functions so that the MADT APIC enumerator in the acpi
 * kernel module will link against a kernel without 'option APIC_IO'.
 *
 * XXX - This is a gross hack.
 */
void
apic_register_enumerator(struct apic_enumerator *enumerator)
{
}

void *
ioapic_create(uintptr_t addr, int32_t id, int intbase)
{
	return (NULL);
}

int
ioapic_disable_pin(void *cookie, u_int pin)
{
	return (ENXIO);
}

void
ioapic_enable_mixed_mode(void)
{
}

int
ioapic_get_vector(void *cookie, u_int pin)
{
	return (-1);
}

void
ioapic_register(void *cookie)
{
}

int
ioapic_remap_vector(void *cookie, u_int pin, int vector)
{
	return (ENXIO);
}

int
ioapic_set_extint(void *cookie, u_int pin)
{
	return (ENXIO);
}

int
ioapic_set_nmi(void *cookie, u_int pin)
{
	return (ENXIO);
}

int
ioapic_set_polarity(void *cookie, u_int pin, char activehi)
{
	return (ENXIO);
}

int
ioapic_set_triggermode(void *cookie, u_int pin, char edgetrigger)
{
	return (ENXIO);
}

void
lapic_create(u_int apic_id, int boot_cpu)
{
}

void
lapic_init(uintptr_t addr)
{
}

int
lapic_set_lvt_mode(u_int apic_id, u_int lvt, u_int32_t mode)
{
	return (ENXIO);
}

int
lapic_set_lvt_polarity(u_int apic_id, u_int lvt, u_char activehi)
{
	return (ENXIO);
}

int
lapic_set_lvt_triggermode(u_int apic_id, u_int lvt, u_char edgetrigger)
{
	return (ENXIO);
}

#include <sys/disklabel.h>

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(struct buf *bp, struct disklabel *lp, int wlabel)
{
        struct partition *p = lp->d_partitions + dkpart(bp->b_dev);
        int labelsect = lp->d_partitions[0].p_offset;
        int maxsz = p->p_size,
                sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

        /* overwriting disk label ? */
        /* XXX should also protect bootstrap in first 8K */
        if (bp->b_blkno + p->p_offset <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
            bp->b_blkno + p->p_offset + sz > LABELSECTOR + labelsect &&
#endif
            (bp->b_flags & B_READ) == 0 && wlabel == 0) {
                bp->b_error = EROFS;
                goto bad;
        }

#if     defined(DOSBBSECTOR) && defined(notyet)
        /* overwriting master boot record? */
        if (bp->b_blkno + p->p_offset <= DOSBBSECTOR &&
            (bp->b_flags & B_READ) == 0 && wlabel == 0) {
                bp->b_error = EROFS;
                goto bad;
        }
#endif

        /* beyond partition? */
        if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
                /* if exactly at end of disk, return an EOF */
                if (bp->b_blkno == maxsz) {
                        bp->b_resid = bp->b_bcount;
                        return(0);
                }
                /* or truncate if part of it fits */
                sz = maxsz - bp->b_blkno;
                if (sz <= 0) {
                        bp->b_error = EINVAL;
                        goto bad;
                }
                bp->b_bcount = sz << DEV_BSHIFT;
        }

        bp->b_pblkno = bp->b_blkno + p->p_offset;
        return(1);

bad:
        bp->b_flags |= B_ERROR;
        return(-1);
}

#ifdef DDB

/*
 * Provide inb() and outb() as functions.  They are normally only
 * available as macros calling inlined functions, thus cannot be
 * called inside DDB.
 *
 * The actual code is stolen from <machine/cpufunc.h>, and de-inlined.
 */

#undef inb
#undef outb

/* silence compiler warnings */
u_char inb(u_int);
void outb(u_int, u_char);

u_char
inb(u_int port)
{
	u_char	data;
	/*
	 * We use %%dx and not %1 here because i/o is done at %dx and not at
	 * %edx, while gcc generates inferior code (movw instead of movl)
	 * if we tell it to load (u_short) port.
	 */
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

void
outb(u_int port, u_char data)
{
	u_char	al;
	/*
	 * Use an unnecessary assignment to help gcc's register allocator.
	 * This make a large difference for gcc-1.40 and a tiny difference
	 * for gcc-2.6.0.  For gcc-1.40, al had to be ``asm("ax")'' for
	 * best results.  gcc-2.6.0 can't handle this.
	 */
	al = data;
	__asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

#endif /* DDB */



#include "opt_cpu.h"


/*
 * initialize all the SMP locks
 */

/* critical region around IO APIC, apic_imen */
struct spinlock	imen_spinlock;

/* Make FAST_INTR() routines sequential */
struct spinlock	fast_intr_spinlock;

/* critical region for old style disable_intr/enable_intr */
struct spinlock	mpintr_spinlock;

/* critical region around INTR() routines */
struct spinlock	intr_spinlock;

/* lock region used by kernel profiling */
struct spinlock	mcount_spinlock;

/* locks com (tty) data/hardware accesses: a FASTINTR() */
struct spinlock	com_spinlock;

/* locks kernel printfs */
struct spinlock	cons_spinlock;

/* lock regions around the clock hardware */
struct spinlock	clock_spinlock;

/* lock around the MP rendezvous */
struct spinlock smp_rv_spinlock;

static void
init_locks(void)
{
	/*
	 * mp_lock = 0;	BSP already owns the MP lock 
	 */
	/*
	 * Get the initial mp_lock with a count of 1 for the BSP.
	 * This uses a LOGICAL cpu ID, ie BSP == 0.
	 */
#ifdef SMP
	cpu_get_initial_mplock();
#endif
	/* DEPRECATED */
	spin_lock_init(&mcount_spinlock);
	spin_lock_init(&fast_intr_spinlock);
	spin_lock_init(&intr_spinlock);
	spin_lock_init(&mpintr_spinlock);
	spin_lock_init(&imen_spinlock);
	spin_lock_init(&smp_rv_spinlock);
	spin_lock_init(&com_spinlock);
	spin_lock_init(&clock_spinlock);
	spin_lock_init(&cons_spinlock);

	/* our token pool needs to work early */
	lwkt_token_pool_init();
}

