/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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
 * $FreeBSD: src/sys/i386/i386/globals.s,v 1.13.2.1 2000/05/16 06:58:06 dillon Exp $
 * $DragonFly: src/sys/i386/i386/Attic/globals.s,v 1.22 2004/05/05 19:26:38 dillon Exp $
 */

#include <machine/asmacros.h>
#include <machine/pmap.h>

#include "assym.s"

	/*
	 * Define the layout of the per-cpu address space.  This is
	 * "constructed" in locore.s on the BSP and in mp_machdep.c for
	 * each AP.  DO NOT REORDER THESE WITHOUT UPDATING THE REST!
	 *
	 * On UP the per-cpu address space is simply placed in the data
	 * segment.
	 */
	.data
	.globl	CPU_prvspace, lapic
	.set	CPU_prvspace,(MPPTDI << PDRSHIFT)
	.set	lapic,CPU_prvspace + (NPTEPG-1) * PAGE_SIZE

	.globl  gd_idlestack,gd_idlestack_top
	.set    gd_idlestack,PS_IDLESTACK
	.set    gd_idlestack_top,PS_IDLESTACK_TOP

	.globl	globaldata
	.set	globaldata,0

	/*
	 * Define layout of the global data.  On SMP this lives in
	 * the per-cpu address space, otherwise it's in the data segment.
	 */
	.globl	gd_curthread, gd_npxthread, gd_reqflags, gd_common_tss
	.set	gd_curthread,globaldata + GD_CURTHREAD
	.set	gd_npxthread,globaldata + GD_NPXTHREAD
	.set	gd_reqflags,globaldata + GD_REQFLAGS
	.set	gd_common_tss,globaldata + GD_COMMON_TSS

	.globl	gd_common_tssd, gd_tss_gdt
	.set	gd_common_tssd,globaldata + GD_COMMON_TSSD
	.set	gd_tss_gdt,globaldata + GD_TSS_GDT

	.globl	gd_currentldt
	.set	gd_currentldt,globaldata + GD_CURRENTLDT

	.globl	gd_fpu_lock, gd_savefpu
	.set	gd_fpu_lock, globaldata + GD_FPU_LOCK
	.set	gd_savefpu, globaldata + GD_SAVEFPU

	/*
	 * The BSP version of these get setup in locore.s and pmap.c, while
	 * the AP versions are setup in mp_machdep.c.
	 */
	.globl  gd_cpuid, gd_other_cpus
	.globl	gd_ss_eflags, gd_intr_nesting_level
	.globl  gd_CMAP1, gd_CMAP2, gd_CMAP3, gd_PMAP1
	.globl  gd_CADDR1, gd_CADDR2, gd_CADDR3, gd_PADDR1
	.globl  gd_ipending, gd_fpending, gd_cnt, gd_private_tss

	.set    gd_cpuid,globaldata + GD_CPUID
	.set    gd_private_tss,globaldata + GD_PRIVATE_TSS
	.set    gd_other_cpus,globaldata + GD_OTHER_CPUS
	.set    gd_ss_eflags,globaldata + GD_SS_EFLAGS
	.set    gd_intr_nesting_level,globaldata + GD_INTR_NESTING_LEVEL
	.set    gd_CMAP1,globaldata + GD_PRV_CMAP1
	.set    gd_CMAP2,globaldata + GD_PRV_CMAP2
	.set    gd_CMAP3,globaldata + GD_PRV_CMAP3
	.set    gd_PMAP1,globaldata + GD_PRV_PMAP1
	.set    gd_CADDR1,globaldata + GD_PRV_CADDR1
	.set    gd_CADDR2,globaldata + GD_PRV_CADDR2
	.set    gd_CADDR3,globaldata + GD_PRV_CADDR3
	.set    gd_PADDR1,globaldata + GD_PRV_PADDR1
	.set	gd_fpending,globaldata + GD_FPENDING
	.set	gd_ipending,globaldata + GD_IPENDING
	.set	gd_cnt,globaldata + GD_CNT

#if defined(APIC_IO)
	.globl	lapic_eoi, lapic_svr, lapic_tpr, lapic_irr1, lapic_ver
	.globl	lapic_icr_lo,lapic_icr_hi,lapic_isr1
/*
 * Do not clutter our namespace with these unless we need them in other
 * assembler code.  The C code uses different definitions.
 */
#if 0
	.globl	lapic_id,lapic_ver,lapic_tpr,lapic_apr,lapic_ppr,lapic_eoi
	.globl	lapic_ldr,lapic_dfr,lapic_svr,lapic_isr,lapic_isr0
	.globl	lapic_isr2,lapic_isr3,lapic_isr4,lapic_isr5,lapic_isr6
	.globl	lapic_isr7,lapic_tmr,lapic_tmr0,lapic_tmr1,lapic_tmr2
	.globl	lapic_tmr3,lapic_tmr4,lapic_tmr5,lapic_tmr6,lapic_tmr7
	.globl	lapic_irr,lapic_irr0,lapic_irr1,lapic_irr2,lapic_irr3
	.globl	lapic_irr4,lapic_irr5,lapic_irr6,lapic_irr7,lapic_esr
	.globl	lapic_lvtt,lapic_pcint,lapic_lvt1
	.globl	lapic_lvt2,lapic_lvt3,lapic_ticr,lapic_tccr,lapic_tdcr
#endif
	.set	lapic_id,	lapic + 0x020
	.set	lapic_ver,	lapic + 0x030
	.set	lapic_tpr,	lapic + 0x080
	.set	lapic_apr,	lapic + 0x090
	.set	lapic_ppr,	lapic + 0x0a0
	.set	lapic_eoi,	lapic + 0x0b0
	.set	lapic_ldr,	lapic + 0x0d0
	.set	lapic_dfr,	lapic + 0x0e0
	.set	lapic_svr,	lapic + 0x0f0
	.set	lapic_isr,	lapic + 0x100
	.set	lapic_isr0,	lapic + 0x100
	.set	lapic_isr1,	lapic + 0x110
	.set	lapic_isr2,	lapic + 0x120
	.set	lapic_isr3,	lapic + 0x130
	.set	lapic_isr4,	lapic + 0x140
	.set	lapic_isr5,	lapic + 0x150
	.set	lapic_isr6,	lapic + 0x160
	.set	lapic_isr7,	lapic + 0x170
	.set	lapic_tmr,	lapic + 0x180
	.set	lapic_tmr0,	lapic + 0x180
	.set	lapic_tmr1,	lapic + 0x190
	.set	lapic_tmr2,	lapic + 0x1a0
	.set	lapic_tmr3,	lapic + 0x1b0
	.set	lapic_tmr4,	lapic + 0x1c0
	.set	lapic_tmr5,	lapic + 0x1d0
	.set	lapic_tmr6,	lapic + 0x1e0
	.set	lapic_tmr7,	lapic + 0x1f0
	.set	lapic_irr,	lapic + 0x200
	.set	lapic_irr0,	lapic + 0x200
	.set	lapic_irr1,	lapic + 0x210
	.set	lapic_irr2,	lapic + 0x220
	.set	lapic_irr3,	lapic + 0x230
	.set	lapic_irr4,	lapic + 0x240
	.set	lapic_irr5,	lapic + 0x250
	.set	lapic_irr6,	lapic + 0x260
	.set	lapic_irr7,	lapic + 0x270
	.set	lapic_esr,	lapic + 0x280
	.set	lapic_icr_lo,	lapic + 0x300
	.set	lapic_icr_hi,	lapic + 0x310
	.set	lapic_lvtt,	lapic + 0x320
	.set	lapic_pcint,	lapic + 0x340
	.set	lapic_lvt1,	lapic + 0x350
	.set	lapic_lvt2,	lapic + 0x360
	.set	lapic_lvt3,	lapic + 0x370
	.set	lapic_ticr,	lapic + 0x380
	.set	lapic_tccr,	lapic + 0x390
	.set	lapic_tdcr,	lapic + 0x3e0
#endif

