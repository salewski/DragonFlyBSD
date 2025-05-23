/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)vm_meter.c	8.4 (Berkeley) 1/4/94
 * $FreeBSD: src/sys/vm/vm_meter.c,v 1.34.2.7 2002/10/10 19:28:22 dillon Exp $
 * $DragonFly: src/sys/vm/vm_meter.c,v 1.7 2003/07/19 21:14:53 dillon Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <sys/sysctl.h>

struct vmstats vmstats;

static int maxslp = MAXSLP;

SYSCTL_UINT(_vm, VM_V_FREE_MIN, v_free_min,
	CTLFLAG_RW, &vmstats.v_free_min, 0, "");
SYSCTL_UINT(_vm, VM_V_FREE_TARGET, v_free_target,
	CTLFLAG_RW, &vmstats.v_free_target, 0, "");
SYSCTL_UINT(_vm, VM_V_FREE_RESERVED, v_free_reserved,
	CTLFLAG_RW, &vmstats.v_free_reserved, 0, "");
SYSCTL_UINT(_vm, VM_V_INACTIVE_TARGET, v_inactive_target,
	CTLFLAG_RW, &vmstats.v_inactive_target, 0, "");
SYSCTL_UINT(_vm, VM_V_CACHE_MIN, v_cache_min,
	CTLFLAG_RW, &vmstats.v_cache_min, 0, "");
SYSCTL_UINT(_vm, VM_V_CACHE_MAX, v_cache_max,
	CTLFLAG_RW, &vmstats.v_cache_max, 0, "");
SYSCTL_UINT(_vm, VM_V_PAGEOUT_FREE_MIN, v_pageout_free_min,
	CTLFLAG_RW, &vmstats.v_pageout_free_min, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, v_free_severe,
	CTLFLAG_RW, &vmstats.v_free_severe, 0, "");

SYSCTL_STRUCT(_vm, VM_LOADAVG, loadavg, CTLFLAG_RD, 
    &averunnable, loadavg, "Machine loadaverage history");

static int
do_vmtotal(SYSCTL_HANDLER_ARGS)
{
	struct proc *p;
	struct vmtotal total, *totalp;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_map_t map;
	int paging;

	totalp = &total;
	bzero(totalp, sizeof *totalp);
	/*
	 * Mark all objects as inactive.
	 */
	for (object = TAILQ_FIRST(&vm_object_list);
	    object != NULL;
	    object = TAILQ_NEXT(object,object_list))
		vm_object_clear_flag(object, OBJ_ACTIVE);
	/*
	 * Calculate process statistics.
	 */
	for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
		if (p->p_flag & P_SYSTEM)
			continue;
		switch (p->p_stat) {
		case 0:
			continue;

		case SSLEEP:
		case SSTOP:
			if (p->p_flag & P_INMEM) {
				if ((p->p_flag & P_SINTR) == 0)
					totalp->t_dw++;
				else if (p->p_slptime < maxslp)
					totalp->t_sl++;
			} else if (p->p_slptime < maxslp)
				totalp->t_sw++;
			if (p->p_slptime >= maxslp)
				continue;
			break;

		case SRUN:
		case SIDL:
			if (p->p_flag & P_INMEM)
				totalp->t_rq++;
			else
				totalp->t_sw++;
			if (p->p_stat == SIDL)
				continue;
			break;
		}
		/*
		 * Note active objects.
		 */
		paging = 0;
		for (map = &p->p_vmspace->vm_map, entry = map->header.next;
		    entry != &map->header; entry = entry->next) {
			if ((entry->eflags & MAP_ENTRY_IS_SUB_MAP) ||
			    entry->object.vm_object == NULL)
				continue;
			vm_object_set_flag(entry->object.vm_object, OBJ_ACTIVE);
			paging |= entry->object.vm_object->paging_in_progress;
		}
		if (paging)
			totalp->t_pw++;
	}
	/*
	 * Calculate object memory usage statistics.
	 */
	for (object = TAILQ_FIRST(&vm_object_list);
	    object != NULL;
	    object = TAILQ_NEXT(object, object_list)) {
		/*
		 * devices, like /dev/mem, will badly skew our totals
		 */
		if (object->type == OBJT_DEVICE)
			continue;
		totalp->t_vm += object->size;
		totalp->t_rm += object->resident_page_count;
		if (object->flags & OBJ_ACTIVE) {
			totalp->t_avm += object->size;
			totalp->t_arm += object->resident_page_count;
		}
		if (object->shadow_count > 1) {
			/* shared object */
			totalp->t_vmshr += object->size;
			totalp->t_rmshr += object->resident_page_count;
			if (object->flags & OBJ_ACTIVE) {
				totalp->t_avmshr += object->size;
				totalp->t_armshr += object->resident_page_count;
			}
		}
	}
	totalp->t_free = vmstats.v_free_count + vmstats.v_cache_count;
	return (sysctl_handle_opaque(oidp, totalp, sizeof total, req));
}

static int
do_vmstats(SYSCTL_HANDLER_ARGS)
{
	struct vmstats vms = vmstats;
	return (sysctl_handle_opaque(oidp, &vms, sizeof(vms), req));
}

static int
do_vmmeter(SYSCTL_HANDLER_ARGS)
{
	int boffset = offsetof(struct vmmeter, vmmeter_uint_begin);
	int eoffset = offsetof(struct vmmeter, vmmeter_uint_end);
	struct vmmeter vmm;
	int i;

	bzero(&vmm, sizeof(vmm));
	for (i = 0; i < ncpus; ++i) {
		int off;
		struct globaldata *gd = globaldata_find(i);

		for (off = boffset; off <= eoffset; off += sizeof(u_int)) {
			*(u_int *)((char *)&vmm + off) +=
				*(u_int *)((char *)&gd->gd_cnt + off);
		}
		
	}
	return (sysctl_handle_opaque(oidp, &vmm, sizeof(vmm), req));
}

/*
 * vcnt() -	accumulate statistics from the cnt structure for each cpu
 *
 *	The vmmeter structure is now per-cpu as well as global.  Those
 *	statistics which can be kept on a per-cpu basis (to avoid cache
 *	stalls between cpus) can be moved to the per-cpu vmmeter.  Remaining
 *	statistics, such as v_free_reserved, are left in the global
 *	structure.
 *
 * (sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
 */
static int
vcnt(SYSCTL_HANDLER_ARGS)
{
	int i;
	int count = 0;
	int offset = arg2;

	for (i = 0; i < ncpus; ++i) {
		struct globaldata *gd = globaldata_find(i);
		count += *(int *)((char *)&gd->gd_cnt + offset);
	}
	return(SYSCTL_OUT(req, &count, sizeof(int)));
}

#define VMMETEROFF(var)	offsetof(struct vmmeter, var)

SYSCTL_PROC(_vm, OID_AUTO, vmtotal, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, sizeof(struct vmtotal), do_vmtotal, "S,vmtotal", 
    "System virtual memory aggregate");
SYSCTL_PROC(_vm, OID_AUTO, vmstats, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, sizeof(struct vmstats), do_vmstats, "S,vmstats", 
    "System virtual memory statistics");
SYSCTL_PROC(_vm, OID_AUTO, vmmeter, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, sizeof(struct vmmeter), do_vmmeter, "S,vmmeter", 
    "System per-cpu statistics");
SYSCTL_NODE(_vm, OID_AUTO, stats, CTLFLAG_RW, 0, "VM meter stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, sys, CTLFLAG_RW, 0, "VM meter sys stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, vm, CTLFLAG_RW, 0, "VM meter vm stats");
SYSCTL_NODE(_vm_stats, OID_AUTO, misc, CTLFLAG_RW, 0, "VM meter misc stats");

SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_swtch, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_swtch), vcnt, "IU", "Context switches");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_intrans_coll, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_intrans_coll), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_intrans_wait, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_intrans_wait), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_forwarded_ints, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_forwarded_ints), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_forwarded_hits, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_forwarded_hits), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_forwarded_misses, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_forwarded_misses), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_trap, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_trap), vcnt, "IU", "Traps");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_syscall, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_syscall), vcnt, "IU", "Syscalls");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_intr, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_intr), vcnt, "IU", "Hardware interrupts");
SYSCTL_PROC(_vm_stats_sys, OID_AUTO, v_soft, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_soft), vcnt, "IU", "Software interrupts");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vm_faults, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vm_faults), vcnt, "IU", "VM faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cow_faults, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_cow_faults), vcnt, "IU", "COW faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_cow_optim, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_cow_optim), vcnt, "IU", "Optimized COW faults");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_zfod, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_zfod), vcnt, "IU", "Zero fill");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_ozfod, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_ozfod), vcnt, "IU", "Optimized zero fill");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swapin, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_swapin), vcnt, "IU", "Swapin operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swapout, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_swapout), vcnt, "IU", "Swapout operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swappgsin, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_swappgsin), vcnt, "IU", "Swapin pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_swappgsout, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_swappgsout), vcnt, "IU", "Swapout pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodein, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vnodein), vcnt, "IU", "Vnodein operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodeout, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vnodeout), vcnt, "IU", "Vnodeout operations");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodepgsin, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vnodepgsin), vcnt, "IU", "Vnodein pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vnodepgsout, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vnodepgsout), vcnt, "IU", "Vnodeout pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_intrans, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_intrans), vcnt, "IU", "In transit page blocking");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_reactivated, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_reactivated), vcnt, "IU", "Reactivated pages");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pdwakeups, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_pdwakeups), vcnt, "IU", "Pagedaemon wakeups");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pdpages, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_pdpages), vcnt, "IU", "Pagedaemon page scans");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_dfree, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_dfree), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_pfree, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_pfree), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_tfree, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_tfree), vcnt, "IU", "");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_forks, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_forks), vcnt, "IU", "Number of fork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vforks, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vforks), vcnt, "IU", "Number of vfork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_rforks, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_rforks), vcnt, "IU", "Number of rfork() calls");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_kthreads, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_kthreads), vcnt, "IU", "Number of fork() calls by kernel");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_forkpages, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_forkpages), vcnt, "IU", "VM pages affected by fork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_vforkpages, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_vforkpages), vcnt, "IU", "VM pages affected by vfork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_rforkpages, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_rforkpages), vcnt, "IU", "VM pages affected by rfork()");
SYSCTL_PROC(_vm_stats_vm, OID_AUTO, v_kthreadpages, CTLTYPE_UINT|CTLFLAG_RD,
	0, VMMETEROFF(v_kthreadpages), vcnt, "IU", "VM pages affected by fork() by kernel");

SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_page_size, CTLFLAG_RD, &vmstats.v_page_size, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_page_count, CTLFLAG_RD, &vmstats.v_page_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_reserved, CTLFLAG_RD, &vmstats.v_free_reserved, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_target, CTLFLAG_RD, &vmstats.v_free_target, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_min, CTLFLAG_RD, &vmstats.v_free_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_free_count, CTLFLAG_RD, &vmstats.v_free_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_wire_count, CTLFLAG_RD, &vmstats.v_wire_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_active_count, CTLFLAG_RD, &vmstats.v_active_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_inactive_target, CTLFLAG_RD, &vmstats.v_inactive_target, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_inactive_count, CTLFLAG_RD, &vmstats.v_inactive_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_count, CTLFLAG_RD, &vmstats.v_cache_count, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_min, CTLFLAG_RD, &vmstats.v_cache_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_cache_max, CTLFLAG_RD, &vmstats.v_cache_max, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_pageout_free_min, CTLFLAG_RD, &vmstats.v_pageout_free_min, 0, "");
SYSCTL_UINT(_vm_stats_vm, OID_AUTO,
	v_interrupt_free_min, CTLFLAG_RD, &vmstats.v_interrupt_free_min, 0, "");
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	zero_page_count, CTLFLAG_RD, &vm_page_zero_count, 0, "");
