/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)tss.h	5.4 (Berkeley) 1/18/91
 * $FreeBSD: src/sys/i386/include/tss.h,v 1.11 1999/12/29 04:33:09 peter Exp $
 * $DragonFly: src/sys/i386/include/Attic/tss.h,v 1.3 2003/06/28 04:16:03 dillon Exp $
 */

#ifndef _MACHINE_TSS_H_
#define _MACHINE_TSS_H_ 1

/*
 * Intel 386 Context Data Type
 */

struct i386tss {
	int	tss_link;	/* actually 16 bits: top 16 bits must be zero */
	int	tss_esp0; 	/* kernel stack pointer privilege level 0 */
#define	tss_ksp	tss_esp0
	int	tss_ss0;	/* actually 16 bits: top 16 bits must be zero */
	int	tss_esp1; 	/* kernel stack pointer privilege level 1 */
	int	tss_ss1;	/* actually 16 bits: top 16 bits must be zero */
	int	tss_esp2; 	/* kernel stack pointer privilege level 2 */
	int	tss_ss2;	/* actually 16 bits: top 16 bits must be zero */
	int	tss_cr3; 	/* page table directory */
#define	tss_ptd	tss_cr3
	int	tss_eip; 	/* program counter */
#define	tss_pc	tss_eip
	int	tss_eflags; 	/* program status longword */
#define	tss_psl	tss_eflags
	int	tss_eax;
	int	tss_ecx;
	int	tss_edx;
	int	tss_ebx;
	int	tss_esp; 	/* user stack pointer */
#define	tss_usp	tss_esp
	int	tss_ebp; 	/* user frame pointer */
#define	tss_fp	tss_ebp
	int	tss_esi;
	int	tss_edi;
	int	tss_es;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_cs;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_ss;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_ds;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_fs;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_gs;		/* actually 16 bits: top 16 bits must be zero */
	int	tss_ldt;	/* actually 16 bits: top 16 bits must be zero */
	int	tss_ioopt;	/* options & io offset bitmap: currently zero */
				/* XXX unimplemented .. i/o permission bitmap */
};

#endif /* _MACHINE_TSS_H_ */

