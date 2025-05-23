/*-
 * Copyright (c) 1989, 1990 William F. Jolitz
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
 *	from: @(#)segments.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD: src/sys/i386/include/segments.h,v 1.24 1999/12/29 04:33:07 peter Exp $
 * $DragonFly: src/sys/i386/include/Attic/segments.h,v 1.8 2005/02/21 21:40:55 dillon Exp $
 */

#ifndef _MACHINE_SEGMENTS_H_
#define	_MACHINE_SEGMENTS_H_

/*
 * 386 Segmentation Data Structures and definitions
 *	William F. Jolitz (william@ernie.berkeley.edu) 6/20/1989
 */

/*
 * Selectors
 */

#define	ISPL(s)	((s)&3)		/* what is the priority level of a selector */
#define	SEL_KPL	0		/* kernel priority level */
#define	SEL_UPL	3		/* user priority level */
#define	ISLDT(s)	((s)&SEL_LDT)	/* is it local or global */
#define	SEL_LDT	4		/* local descriptor table */
#define	IDXSEL(s)	(((s)>>3) & 0x1fff)		/* index of selector */
#define	LSEL(s,r)	(((s)<<3) | SEL_LDT | r)	/* a local selector */
#define	GSEL(s,r)	(((s)<<3) | r)			/* a global selector */

/*
 * Memory and System segment descriptors
 */
struct	segment_descriptor	{
	unsigned sd_lolimit:16 ;	/* segment extent (lsb) */
	unsigned sd_lobase:24 __attribute__ ((packed));
					/* segment base address (lsb) */
	unsigned sd_type:5 ;		/* segment type */
	unsigned sd_dpl:2 ;		/* segment descriptor priority level */
	unsigned sd_p:1 ;		/* segment descriptor present */
	unsigned sd_hilimit:4 ;		/* segment extent (msb) */
	unsigned sd_xx:2 ;		/* unused */
	unsigned sd_def32:1 ;		/* default 32 vs 16 bit size */
	unsigned sd_gran:1 ;		/* limit granularity (byte/page units)*/
	unsigned sd_hibase:8 ;		/* segment base address  (msb) */
} ;

/*
 * Gate descriptors (e.g. indirect descriptors)
 */
struct	gate_descriptor	{
	unsigned gd_looffset:16 ;	/* gate offset (lsb) */
	unsigned gd_selector:16 ;	/* gate segment selector */
	unsigned gd_stkcpy:5 ;		/* number of stack wds to cpy */
	unsigned gd_xx:3 ;		/* unused */
	unsigned gd_type:5 ;		/* segment type */
	unsigned gd_dpl:2 ;		/* segment descriptor priority level */
	unsigned gd_p:1 ;		/* segment descriptor present */
	unsigned gd_hioffset:16 ;	/* gate offset (msb) */
} ;

/*
 * Generic descriptor
 */
union	descriptor	{
	struct	segment_descriptor sd;
	struct	gate_descriptor gd;
};

	/* system segments and gate types */
#define	SDT_SYSNULL	 0	/* system null */
#define	SDT_SYS286TSS	 1	/* system 286 TSS available */
#define	SDT_SYSLDT	 2	/* system local descriptor table */
#define	SDT_SYS286BSY	 3	/* system 286 TSS busy */
#define	SDT_SYS286CGT	 4	/* system 286 call gate */
#define	SDT_SYSTASKGT	 5	/* system task gate */
#define	SDT_SYS286IGT	 6	/* system 286 interrupt gate */
#define	SDT_SYS286TGT	 7	/* system 286 trap gate */
#define	SDT_SYSNULL2	 8	/* system null again */
#define	SDT_SYS386TSS	 9	/* system 386 TSS available */
#define	SDT_SYSNULL3	10	/* system null again */
#define	SDT_SYS386BSY	11	/* system 386 TSS busy */
#define	SDT_SYS386CGT	12	/* system 386 call gate */
#define	SDT_SYSNULL4	13	/* system null again */
#define	SDT_SYS386IGT	14	/* system 386 interrupt gate */
#define	SDT_SYS386TGT	15	/* system 386 trap gate */

	/* memory segment types */
#define	SDT_MEMRO	16	/* memory read only */
#define	SDT_MEMROA	17	/* memory read only accessed */
#define	SDT_MEMRW	18	/* memory read write */
#define	SDT_MEMRWA	19	/* memory read write accessed */
#define	SDT_MEMROD	20	/* memory read only expand dwn limit */
#define	SDT_MEMRODA	21	/* memory read only expand dwn limit accessed */
#define	SDT_MEMRWD	22	/* memory read write expand dwn limit */
#define	SDT_MEMRWDA	23	/* memory read write expand dwn limit accessed */
#define	SDT_MEME	24	/* memory execute only */
#define	SDT_MEMEA	25	/* memory execute only accessed */
#define	SDT_MEMER	26	/* memory execute read */
#define	SDT_MEMERA	27	/* memory execute read accessed */
#define	SDT_MEMEC	28	/* memory execute only conforming */
#define	SDT_MEMEAC	29	/* memory execute only accessed conforming */
#define	SDT_MEMERC	30	/* memory execute read conforming */
#define	SDT_MEMERAC	31	/* memory execute read accessed conforming */

/* is memory segment descriptor pointer ? */
#define ISMEMSDP(s)	((s->d_type) >= SDT_MEMRO && (s->d_type) <= SDT_MEMERAC)

/* is 286 gate descriptor pointer ? */
#define IS286GDP(s)	(((s->d_type) >= SDT_SYS286CGT \
				 && (s->d_type) < SDT_SYS286TGT))

/* is 386 gate descriptor pointer ? */
#define IS386GDP(s)	(((s->d_type) >= SDT_SYS386CGT \
				&& (s->d_type) < SDT_SYS386TGT))

/* is gate descriptor pointer ? */
#define ISGDP(s)	(IS286GDP(s) || IS386GDP(s))

/* is segment descriptor pointer ? */
#define ISSDP(s)	(ISMEMSDP(s) || !ISGDP(s))

/* is system segment descriptor pointer ? */
#define ISSYSSDP(s)	(!ISMEMSDP(s) && !ISGDP(s))

/*
 * Software definitions are in this convenient format,
 * which are translated into inconvenient segment descriptors
 * when needed to be used by the 386 hardware
 */

struct	soft_segment_descriptor	{
	unsigned ssd_base ;		/* segment base address  */
	unsigned ssd_limit ;		/* segment extent */
	unsigned ssd_type:5 ;		/* segment type */
	unsigned ssd_dpl:2 ;		/* segment descriptor priority level */
	unsigned ssd_p:1 ;		/* segment descriptor present */
	unsigned ssd_xx:4 ;		/* unused */
	unsigned ssd_xx1:2 ;		/* unused */
	unsigned ssd_def32:1 ;		/* default 32 vs 16 bit size */
	unsigned ssd_gran:1 ;		/* limit granularity (byte/page units)*/
};

/*
 * region descriptors, used to load gdt/idt tables before segments yet exist.
 */
struct region_descriptor {
	unsigned rd_limit:16;				/* segment extent */
	unsigned rd_base:32 __attribute__ ((packed));	/* base address  */
};

/*
 * Segment Protection Exception code bits
 */

#define	SEGEX_EXT	0x01	/* recursive or externally induced */
#define	SEGEX_IDT	0x02	/* interrupt descriptor table */
#define	SEGEX_TI	0x04	/* local descriptor table */
				/* other bits are affected descriptor index */
#define SEGEX_IDX(s)	((s)>>3)&0x1fff)

/*
 * Size of IDT table
 */

#if defined(SMP) || defined(APIC_IO)
#define	NIDT	256		/* we use them all */
#else
#define	NIDT	131		/* 32 reserved, 16 h/w, 0 s/w, 0x80, 0x81, 0x82 */
#endif /* SMP || APIC_IO */
#define	NRSVIDT	32		/* reserved entries for cpu exceptions */

/*
 * Entries in the Global Descriptor Table (GDT)
 */
#define	GNULL_SEL	0	/* Null Descriptor */
#define	GCODE_SEL	1	/* Kernel Code Descriptor */
#define	GDATA_SEL	2	/* Kernel Data Descriptor */
#define	GPRIV_SEL	3	/* SMP Per-Processor Private Data */
#define	GPROC0_SEL	4	/* Task state process slot zero and up */
#define	GLDT_SEL	5	/* LDT - eventually one per process */
#define	GUSERLDT_SEL	6	/* User LDT */
#define	GTGATE_SEL	7	/* Process task switch gate */
#define	GBIOSLOWMEM_SEL	8	/* BIOS low memory access (must be entry 8) */
#define	GPANIC_SEL	9	/* Task state to consider panic from */
#define GBIOSCODE32_SEL	10	/* BIOS interface (32bit Code) */
#define GBIOSCODE16_SEL	11	/* BIOS interface (16bit Code) */
#define GBIOSDATA_SEL	12	/* BIOS interface (Data) */
#define GBIOSUTIL_SEL	13	/* BIOS interface (Utility) */
#define GBIOSARGS_SEL	14	/* BIOS interface (Arguments) */
#define	GTLS_START	15	/* Thread TLS Descriptor */
#define	GTLS_END	17	/* Thread TLS Descriptor */

#define NGTLS		(GTLS_END - GTLS_START + 1)
#ifdef BDE_DEBUGGER
#define	NGDT		21	/* some of 11-17 are reserved for debugger */
#else
#define NGDT 		18
#endif

/*
 * Entries in the Local Descriptor Table (LDT)
 */
#define	LSYS5CALLS_SEL	0	/* forced by intel BCS */
#define	LSYS5SIGR_SEL	1
#define	L43BSDCALLS_SEL	2	/* notyet */
#define	LUCODE_SEL	3
#define LSOL26CALLS_SEL	4	/* Solaris >= 2.6 system call gate */
#define	LUDATA_SEL	5
/* separate stack, es,fs,gs sels ? */
/* #define	LPOSIXCALLS_SEL	5*/	/* notyet */
#define LBSDICALLS_SEL	16	/* BSDI system call gate */
#define NLDT		(LBSDICALLS_SEL + 1)

#ifdef _KERNEL
extern int	_default_ldt;
extern union descriptor gdt[];
extern struct soft_segment_descriptor gdt_segs[];
extern struct gate_descriptor *idt;
extern union descriptor ldt[NLDT];

void	lgdt		(struct region_descriptor *rdp);
void	lidt		(struct region_descriptor *rdp);
void	lldt		(u_short sel);
void	sdtossd		(struct segment_descriptor *sdp,
			     struct soft_segment_descriptor *ssdp);
void	ssdtosd		(struct soft_segment_descriptor *ssdp,
			     struct segment_descriptor *sdp);
#endif /* _KERNEL */

#endif /* !_MACHINE_SEGMENTS_H_ */
