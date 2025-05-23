/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)kern_proc.c	8.7 (Berkeley) 2/14/95
 * $FreeBSD: src/sys/kern/kern_proc.c,v 1.63.2.9 2003/05/08 07:47:16 kbyanc Exp $
 * $DragonFly: src/sys/kern/kern_proc.c,v 1.18 2005/02/01 02:25:45 joerg Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/jail.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <vm/vm.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <sys/user.h>
#include <vm/vm_zone.h>
#include <machine/smp.h>

static MALLOC_DEFINE(M_PGRP, "pgrp", "process group header");
MALLOC_DEFINE(M_SESSION, "session", "session header");
static MALLOC_DEFINE(M_PROC, "proc", "Proc structures");
MALLOC_DEFINE(M_SUBPROC, "subproc", "Proc sub-structures");

int ps_showallprocs = 1;
static int ps_showallthreads = 1;
SYSCTL_INT(_kern, OID_AUTO, ps_showallprocs, CTLFLAG_RW,
    &ps_showallprocs, 0, "");
SYSCTL_INT(_kern, OID_AUTO, ps_showallthreads, CTLFLAG_RW,
    &ps_showallthreads, 0, "");

static void pgdelete	(struct pgrp *);

static void	orphanpg (struct pgrp *pg);

/*
 * Other process lists
 */
struct pidhashhead *pidhashtbl;
u_long pidhash;
struct pgrphashhead *pgrphashtbl;
u_long pgrphash;
struct proclist allproc;
struct proclist zombproc;
vm_zone_t proc_zone;
vm_zone_t thread_zone;

/*
 * Initialize global process hashing structures.
 */
void
procinit()
{

	LIST_INIT(&allproc);
	LIST_INIT(&zombproc);
	pidhashtbl = hashinit(maxproc / 4, M_PROC, &pidhash);
	pgrphashtbl = hashinit(maxproc / 4, M_PROC, &pgrphash);
	proc_zone = zinit("PROC", sizeof (struct proc), 0, 0, 5);
	thread_zone = zinit("THREAD", sizeof (struct thread), 0, 0, 5);
	uihashinit();
}

/*
 * Is p an inferior of the current process?
 */
int
inferior(p)
	struct proc *p;
{

	for (; p != curproc; p = p->p_pptr)
		if (p->p_pid == 0)
			return (0);
	return (1);
}

/*
 * Locate a process by number
 */
struct proc *
pfind(pid)
	pid_t pid;
{
	struct proc *p;

	LIST_FOREACH(p, PIDHASH(pid), p_hash)
		if (p->p_pid == pid)
			return (p);
	return (NULL);
}

/*
 * Locate a process group by number
 */
struct pgrp *
pgfind(pgid)
	pid_t pgid;
{
	struct pgrp *pgrp;

	LIST_FOREACH(pgrp, PGRPHASH(pgid), pg_hash)
		if (pgrp->pg_id == pgid)
			return (pgrp);
	return (NULL);
}

/*
 * Move p to a new or existing process group (and session)
 */
int
enterpgrp(p, pgid, mksess)
	struct proc *p;
	pid_t pgid;
	int mksess;
{
	struct pgrp *pgrp = pgfind(pgid);

	KASSERT(pgrp == NULL || !mksess,
	    ("enterpgrp: setsid into non-empty pgrp"));
	KASSERT(!SESS_LEADER(p),
	    ("enterpgrp: session leader attempted setpgrp"));

	if (pgrp == NULL) {
		pid_t savepid = p->p_pid;
		struct proc *np;
		/*
		 * new process group
		 */
		KASSERT(p->p_pid == pgid,
		    ("enterpgrp: new pgrp and pid != pgid"));
		if ((np = pfind(savepid)) == NULL || np != p)
			return (ESRCH);
		MALLOC(pgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP,
		    M_WAITOK);
		if (mksess) {
			struct session *sess;

			/*
			 * new session
			 */
			MALLOC(sess, struct session *, sizeof(struct session),
			    M_SESSION, M_WAITOK);
			sess->s_leader = p;
			sess->s_sid = p->p_pid;
			sess->s_count = 1;
			sess->s_ttyvp = NULL;
			sess->s_ttyp = NULL;
			bcopy(p->p_session->s_login, sess->s_login,
			    sizeof(sess->s_login));
			p->p_flag &= ~P_CONTROLT;
			pgrp->pg_session = sess;
			KASSERT(p == curproc,
			    ("enterpgrp: mksession and p != curproc"));
		} else {
			pgrp->pg_session = p->p_session;
			sess_hold(pgrp->pg_session);
		}
		pgrp->pg_id = pgid;
		LIST_INIT(&pgrp->pg_members);
		LIST_INSERT_HEAD(PGRPHASH(pgid), pgrp, pg_hash);
		pgrp->pg_jobc = 0;
		SLIST_INIT(&pgrp->pg_sigiolst);
	} else if (pgrp == p->p_pgrp)
		return (0);

	/*
	 * Adjust eligibility of affected pgrps to participate in job control.
	 * Increment eligibility counts before decrementing, otherwise we
	 * could reach 0 spuriously during the first call.
	 */
	fixjobc(p, pgrp, 1);
	fixjobc(p, p->p_pgrp, 0);

	LIST_REMOVE(p, p_pglist);
	if (LIST_EMPTY(&p->p_pgrp->pg_members))
		pgdelete(p->p_pgrp);
	p->p_pgrp = pgrp;
	LIST_INSERT_HEAD(&pgrp->pg_members, p, p_pglist);
	return (0);
}

/*
 * remove process from process group
 */
int
leavepgrp(p)
	struct proc *p;
{

	LIST_REMOVE(p, p_pglist);
	if (LIST_EMPTY(&p->p_pgrp->pg_members))
		pgdelete(p->p_pgrp);
	p->p_pgrp = 0;
	return (0);
}

/*
 * delete a process group
 */
static void
pgdelete(pgrp)
	struct pgrp *pgrp;
{

	/*
	 * Reset any sigio structures pointing to us as a result of
	 * F_SETOWN with our pgid.
	 */
	funsetownlst(&pgrp->pg_sigiolst);

	if (pgrp->pg_session->s_ttyp != NULL &&
	    pgrp->pg_session->s_ttyp->t_pgrp == pgrp)
		pgrp->pg_session->s_ttyp->t_pgrp = NULL;
	LIST_REMOVE(pgrp, pg_hash);
	sess_rele(pgrp->pg_session);
	free(pgrp, M_PGRP);
}

/*
 * Adjust the ref count on a session structure.  When the ref count falls to
 * zero the tty is disassociated from the session and the session structure
 * is freed.  Note that tty assocation is not itself ref-counted.
 */
void
sess_hold(struct session *sp)
{
	++sp->s_count;
}

void
sess_rele(struct session *sp)
{
	KKASSERT(sp->s_count > 0);
	if (--sp->s_count == 0) {
		if (sp->s_ttyp && sp->s_ttyp->t_session) {
#ifdef TTY_DO_FULL_CLOSE
			/* FULL CLOSE, see ttyclearsession() */
			KKASSERT(sp->s_ttyp->t_session == sp);
			sp->s_ttyp->t_session = NULL;
#else
			/* HALF CLOSE, see ttyclearsession() */
			if (sp->s_ttyp->t_session == sp)
				sp->s_ttyp->t_session = NULL;
#endif
		}
		free(sp, M_SESSION);
	}
}

/*
 * Adjust pgrp jobc counters when specified process changes process group.
 * We count the number of processes in each process group that "qualify"
 * the group for terminal job control (those with a parent in a different
 * process group of the same session).  If that count reaches zero, the
 * process group becomes orphaned.  Check both the specified process'
 * process group and that of its children.
 * entering == 0 => p is leaving specified group.
 * entering == 1 => p is entering specified group.
 */
void
fixjobc(p, pgrp, entering)
	struct proc *p;
	struct pgrp *pgrp;
	int entering;
{
	struct pgrp *hispgrp;
	struct session *mysession = pgrp->pg_session;

	/*
	 * Check p's parent to see whether p qualifies its own process
	 * group; if so, adjust count for p's process group.
	 */
	if ((hispgrp = p->p_pptr->p_pgrp) != pgrp &&
	    hispgrp->pg_session == mysession) {
		if (entering)
			pgrp->pg_jobc++;
		else if (--pgrp->pg_jobc == 0)
			orphanpg(pgrp);
	}

	/*
	 * Check this process' children to see whether they qualify
	 * their process groups; if so, adjust counts for children's
	 * process groups.
	 */
	LIST_FOREACH(p, &p->p_children, p_sibling)
		if ((hispgrp = p->p_pgrp) != pgrp &&
		    hispgrp->pg_session == mysession &&
		    p->p_stat != SZOMB) {
			if (entering)
				hispgrp->pg_jobc++;
			else if (--hispgrp->pg_jobc == 0)
				orphanpg(hispgrp);
		}
}

/*
 * A process group has become orphaned;
 * if there are any stopped processes in the group,
 * hang-up all process in that group.
 */
static void
orphanpg(pg)
	struct pgrp *pg;
{
	struct proc *p;

	LIST_FOREACH(p, &pg->pg_members, p_pglist) {
		if (p->p_stat == SSTOP) {
			LIST_FOREACH(p, &pg->pg_members, p_pglist) {
				psignal(p, SIGHUP);
				psignal(p, SIGCONT);
			}
			return;
		}
	}
}

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(pgrpdump, pgrpdump)
{
	struct pgrp *pgrp;
	struct proc *p;
	int i;

	for (i = 0; i <= pgrphash; i++) {
		if (!LIST_EMPTY(&pgrphashtbl[i])) {
			printf("\tindx %d\n", i);
			LIST_FOREACH(pgrp, &pgrphashtbl[i], pg_hash) {
				printf(
			"\tpgrp %p, pgid %ld, sess %p, sesscnt %d, mem %p\n",
				    (void *)pgrp, (long)pgrp->pg_id,
				    (void *)pgrp->pg_session,
				    pgrp->pg_session->s_count,
				    (void *)LIST_FIRST(&pgrp->pg_members));
				LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
					printf("\t\tpid %ld addr %p pgrp %p\n", 
					    (long)p->p_pid, (void *)p,
					    (void *)p->p_pgrp);
				}
			}
		}
	}
}
#endif /* DDB */

/*
 * Fill in an eproc structure for the specified thread.
 */
void
fill_eproc_td(thread_t td, struct eproc *ep, struct proc *xp)
{
	bzero(ep, sizeof(*ep));

	ep->e_uticks = td->td_uticks;
	ep->e_sticks = td->td_sticks;
	ep->e_iticks = td->td_iticks;
	ep->e_tdev = NOUDEV;
	ep->e_cpuid = td->td_gd->gd_cpuid;
	if (td->td_wmesg) {
		strncpy(ep->e_wmesg, td->td_wmesg, WMESGLEN);
		ep->e_wmesg[WMESGLEN] = 0;
	}

	/*
	 * Fake up portions of the proc structure copied out by the sysctl
	 * to return useful information.  Note that using td_pri directly
	 * is messy because it includes critial section data so we fake
	 * up an rtprio.prio for threads.
	 */
	if (xp) {
		*xp = *initproc;
		xp->p_rtprio.type = RTP_PRIO_THREAD;
		xp->p_rtprio.prio = td->td_pri & TDPRI_MASK;
		xp->p_pid = -1;
	}
}

/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_eproc(struct proc *p, struct eproc *ep)
{
	struct tty *tp;

	fill_eproc_td(p->p_thread, ep, NULL);

	ep->e_paddr = p;
	if (p->p_ucred) {
		ep->e_ucred = *p->p_ucred;
	}
	if (p->p_procsig) {
		ep->e_procsig = *p->p_procsig;
	}
	if (p->p_stat != SIDL && p->p_stat != SZOMB && p->p_vmspace != NULL) {
		struct vmspace *vm = p->p_vmspace;
		ep->e_vm = *vm;
		ep->e_vm.vm_rssize = vmspace_resident_count(vm); /*XXX*/
	}
	if ((p->p_flag & P_INMEM) && p->p_stats)
		ep->e_stats = *p->p_stats;
	if (p->p_pptr)
		ep->e_ppid = p->p_pptr->p_pid;
	if (p->p_pgrp) {
		ep->e_pgid = p->p_pgrp->pg_id;
		ep->e_jobc = p->p_pgrp->pg_jobc;
		ep->e_sess = p->p_pgrp->pg_session;

		if (ep->e_sess) {
			bcopy(ep->e_sess->s_login, ep->e_login, sizeof(ep->e_login));
			if (ep->e_sess->s_ttyvp)
				ep->e_flag = EPROC_CTTY;
			if (p->p_session && SESS_LEADER(p))
				ep->e_flag |= EPROC_SLEADER;
		}
	}
	if ((p->p_flag & P_CONTROLT) &&
	    (ep->e_sess != NULL) &&
	    ((tp = ep->e_sess->s_ttyp) != NULL)) {
		ep->e_tdev = dev2udev(tp->t_dev);
		ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		ep->e_tsess = tp->t_session;
	} else {
		ep->e_tdev = NOUDEV;
	}
}

struct proc *
zpfind(pid_t pid)
{
	struct proc *p;

	LIST_FOREACH(p, &zombproc, p_list)
		if (p->p_pid == pid)
			return (p);
	return (NULL);
}

static int
sysctl_out_proc(struct proc *p, struct thread *td, struct sysctl_req *req, int doingzomb)
{
	struct eproc eproc;
	struct proc xproc;
	int error;
#if 0
	pid_t pid = p->p_pid;
#endif

	if (p) {
		td = p->p_thread;
		fill_eproc(p, &eproc);
		xproc = *p;

		/*
		 * Fixup p_stat from SRUN to SSLEEP if the LWKT thread is
		 * in a thread-blocked state.
		 *
		 * XXX temporary fix which might become permanent (I'd rather
		 * not pollute the thread scheduler with knowlege about 
		 * processes).
		 */
		if (p->p_stat == SRUN && td && (td->td_flags & TDF_BLOCKED)) {
			xproc.p_stat = SSLEEP;
		}
	} else if (td) {
		fill_eproc_td(td, &eproc, &xproc);
	}
	error = SYSCTL_OUT(req,(caddr_t)&xproc, sizeof(struct proc));
	if (error)
		return (error);
	error = SYSCTL_OUT(req,(caddr_t)&eproc, sizeof(eproc));
	if (error)
		return (error);
	error = SYSCTL_OUT(req,(caddr_t)td, sizeof(struct thread));
	if (error)
		return (error);
#if 0
	if (!doingzomb && pid && (pfind(pid) != p))
		return EAGAIN;
	if (doingzomb && zpfind(pid) != p)
		return EAGAIN;
#endif
	return (0);
}

static int
sysctl_kern_proc(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct thread *td;
	int doingzomb;
	int error = 0;
	int n;
	int origcpu;
	struct ucred *cr1 = curproc->p_ucred;

	if (oidp->oid_number == KERN_PROC_PID) {
		if (namelen != 1) 
			return (EINVAL);
		p = pfind((pid_t)name[0]);
		if (!p)
			return (0);
		if (!PRISON_CHECK(cr1, p->p_ucred))
			return (0);
		error = sysctl_out_proc(p, NULL, req, 0);
		return (error);
	}
	if (oidp->oid_number == KERN_PROC_ALL && !namelen)
		;
	else if (oidp->oid_number != KERN_PROC_ALL && namelen == 1)
		;
	else
		return (EINVAL);
	
	if (!req->oldptr) {
		/* overestimate by 5 procs */
		error = SYSCTL_OUT(req, 0, sizeof (struct kinfo_proc) * 5);
		if (error)
			return (error);
	}
	for (doingzomb=0 ; doingzomb < 2 ; doingzomb++) {
		if (!doingzomb)
			p = LIST_FIRST(&allproc);
		else
			p = LIST_FIRST(&zombproc);
		for (; p != 0; p = LIST_NEXT(p, p_list)) {
			/*
			 * Show a user only their processes.
			 */
			if ((!ps_showallprocs) && p_trespass(cr1, p->p_ucred))
				continue;
			/*
			 * Skip embryonic processes.
			 */
			if (p->p_stat == SIDL)
				continue;
			/*
			 * TODO - make more efficient (see notes below).
			 * do by session.
			 */
			switch (oidp->oid_number) {
			case KERN_PROC_PGRP:
				/* could do this by traversing pgrp */
				if (p->p_pgrp == NULL || 
				    p->p_pgrp->pg_id != (pid_t)name[0])
					continue;
				break;

			case KERN_PROC_TTY:
				if ((p->p_flag & P_CONTROLT) == 0 ||
				    p->p_session == NULL ||
				    p->p_session->s_ttyp == NULL ||
				    dev2udev(p->p_session->s_ttyp->t_dev) != 
					(udev_t)name[0])
					continue;
				break;

			case KERN_PROC_UID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_uid != (uid_t)name[0])
					continue;
				break;

			case KERN_PROC_RUID:
				if (p->p_ucred == NULL || 
				    p->p_ucred->cr_ruid != (uid_t)name[0])
					continue;
				break;
			}

			if (!PRISON_CHECK(cr1, p->p_ucred))
				continue;
			PHOLD(p);
			error = sysctl_out_proc(p, NULL, req, doingzomb);
			PRELE(p);
			if (error)
				return (error);
		}
	}

	/*
	 * Iterate over all active cpus and scan their thread list.  Start
	 * with the next logical cpu and end with our original cpu.  We
	 * migrate our own thread to each target cpu in order to safely scan
	 * its thread list.  In the last loop we migrate back to our original
	 * cpu.
	 */
	origcpu = mycpu->gd_cpuid;
	if (!ps_showallthreads || jailed(cr1))
		goto post_threads;
	for (n = 1; n <= ncpus; ++n) {
		globaldata_t rgd;
		int nid;

		nid = (origcpu + n) % ncpus;
		if ((smp_active_mask & (1 << nid)) == 0)
			continue;
		rgd = globaldata_find(nid);
		lwkt_setcpu_self(rgd);
		cpu_mb1();	/* CURRENT CPU HAS CHANGED */

		TAILQ_FOREACH(td, &mycpu->gd_tdallq, td_allq) {
			if (td->td_proc)
				continue;
			switch (oidp->oid_number) {
			case KERN_PROC_PGRP:
			case KERN_PROC_TTY:
			case KERN_PROC_UID:
			case KERN_PROC_RUID:
				continue;
			default:
				break;
			}
			lwkt_hold(td);
			error = sysctl_out_proc(NULL, td, req, doingzomb);
			lwkt_rele(td);
			if (error)
				return (error);
		}
	}
post_threads:
	return (0);
}

/*
 * This sysctl allows a process to retrieve the argument list or process
 * title for another process without groping around in the address space
 * of the other process.  It also allow a process to set its own "process 
 * title to a string of its own choice.
 */
static int
sysctl_kern_proc_args(SYSCTL_HANDLER_ARGS)
{
	int *name = (int*) arg1;
	u_int namelen = arg2;
	struct proc *p;
	struct pargs *pa;
	int error = 0;
	struct ucred *cr1 = curproc->p_ucred;

	if (namelen != 1) 
		return (EINVAL);

	p = pfind((pid_t)name[0]);
	if (!p)
		return (0);

	if ((!ps_argsopen) && p_trespass(cr1, p->p_ucred))
		return (0);

	if (req->newptr && curproc != p)
		return (EPERM);

	if (req->oldptr && p->p_args != NULL)
		error = SYSCTL_OUT(req, p->p_args->ar_args, p->p_args->ar_length);
	if (req->newptr == NULL)
		return (error);

	if (p->p_args && --p->p_args->ar_ref == 0) 
		FREE(p->p_args, M_PARGS);
	p->p_args = NULL;

	if (req->newlen + sizeof(struct pargs) > ps_arg_cache_limit)
		return (error);

	MALLOC(pa, struct pargs *, sizeof(struct pargs) + req->newlen, 
	    M_PARGS, M_WAITOK);
	pa->ar_ref = 1;
	pa->ar_length = req->newlen;
	error = SYSCTL_IN(req, pa->ar_args, req->newlen);
	if (!error)
		p->p_args = pa;
	else
		FREE(pa, M_PARGS);
	return (error);
}

SYSCTL_NODE(_kern, KERN_PROC, proc, CTLFLAG_RD,  0, "Process table");

SYSCTL_PROC(_kern_proc, KERN_PROC_ALL, all, CTLFLAG_RD|CTLTYPE_STRUCT,
	0, 0, sysctl_kern_proc, "S,proc", "Return entire process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PGRP, pgrp, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_TTY, tty, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_UID, uid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_RUID, ruid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_PID, pid, CTLFLAG_RD, 
	sysctl_kern_proc, "Process table");

SYSCTL_NODE(_kern_proc, KERN_PROC_ARGS, args, CTLFLAG_RW | CTLFLAG_ANYBODY,
	sysctl_kern_proc_args, "Process argument list");
