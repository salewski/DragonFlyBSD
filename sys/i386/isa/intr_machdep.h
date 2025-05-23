/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * $FreeBSD: src/sys/i386/isa/intr_machdep.h,v 1.19.2.2 2001/10/14 20:05:50 luigi Exp $
 * $DragonFly: src/sys/i386/isa/Attic/intr_machdep.h,v 1.12 2005/02/01 22:41:25 dillon Exp $
 */

#ifndef _I386_ISA_INTR_MACHDEP_H_
#define	_I386_ISA_INTR_MACHDEP_H_

#ifndef _SYS_INTERRUPT_H_
#ifndef LOCORE
#include <sys/interrupt.h>
#endif
#endif

/*
 * Low level interrupt code.
 */ 

#ifdef _KERNEL

#if defined(SMP) || defined(APIC_IO)
/*
 * XXX FIXME: rethink location for all IPI vectors.
 */

/*
    APIC TPR priority vector levels:

	0xff (255) +-------------+
		   |             | 15 (IPIs: Xspuriousint)
	0xf0 (240) +-------------+
		   |             | 14
	0xe0 (224) +-------------+
		   |             | 13
	0xd0 (208) +-------------+
		   |             | 12
	0xc0 (192) +-------------+
		   |             | 11
	0xb0 (176) +-------------+
		   |             | 10 (IPIs: Xcpustop)
	0xa0 (160) +-------------+
		   |             |  9 (IPIs: Xinvltlb)
	0x90 (144) +-------------+
		   |             |  8 (linux/BSD syscall, IGNORE FAST HW INTS)
	0x80 (128) +-------------+
		   |             |  7 (FAST_INTR 16-23)
	0x70 (112) +-------------+
		   |             |  6 (FAST_INTR 0-15)
	0x60 (96)  +-------------+
		   |             |  5 (IGNORE HW INTS)
	0x50 (80)  +-------------+
		   |             |  4 (2nd IO APIC)
	0x40 (64)  +------+------+
		   |      |      |  3 (upper APIC hardware INTs: PCI)
	0x30 (48)  +------+------+
		   |             |  2 (start of hardware INTs: ISA)
	0x20 (32)  +-------------+
		   |             |  1 (exceptions, traps, etc.)
	0x10 (16)  +-------------+
		   |             |  0 (exceptions, traps, etc.)
	0x00 (0)   +-------------+
 */

/* IDT vector base for regular (aka. slow) and fast interrupts */
#define TPR_SLOW_INTS		0x20
#define TPR_FAST_INTS		0x60

/* blocking values for local APIC Task Priority Register */
#define TPR_BLOCK_HWI		0x4f		/* hardware INTs */
#define TPR_IGNORE_HWI		0x5f		/* ignore INTs */
#define TPR_BLOCK_FHWI		0x7f		/* hardware FAST INTs */
#define TPR_IGNORE_FHWI		0x8f		/* ignore FAST INTs */
#define TPR_IPI_ONLY		0x8f		/* ignore FAST INTs */
#define TPR_BLOCK_XINVLTLB	0x9f		/*  */
#define TPR_BLOCK_XCPUSTOP	0xaf		/*  */
#define TPR_BLOCK_ALL		0xff		/* all INTs */


#ifdef TEST_TEST1
/* put a 'fake' HWI in top of APIC prio 0x3x, 32 + 31 = 63 = 0x3f */
#define XTEST1_OFFSET		(ICU_OFFSET + 31)
#endif /** TEST_TEST1 */

/* TLB shootdowns */
#define XINVLTLB_OFFSET		(ICU_OFFSET + 112)

/* unused/open (was inter-cpu clock handling) */
#define XUNUSED113_OFFSET	(ICU_OFFSET + 113)

/* inter-CPU rendezvous */
#define XRENDEZVOUS_OFFSET	(ICU_OFFSET + 114)

/* IPIQ rendezvous */
#define XIPIQ_OFFSET		(ICU_OFFSET + 115)

/* IPI to signal CPUs to stop and wait for another CPU to restart them */
#define XCPUSTOP_OFFSET		(ICU_OFFSET + 128)

/*
 * Note: this vector MUST be xxxx1111, 32 + 223 = 255 = 0xff:
 */
#define XSPURIOUSINT_OFFSET	(ICU_OFFSET + 223)

#endif /* SMP || APIC_IO */

#ifndef	LOCORE

/*
 * Type of the first (asm) part of an interrupt handler.
 */
typedef void inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);
typedef void unpendhand_t(void);

#define	IDTVEC(name)	__CONCAT(X,name)

extern u_long *intr_countp[];	/* pointers into intrcnt[] */
extern inthand2_t *intr_handler[];	/* C entry points for FAST ints */
extern u_int intr_mask[];	/* sets of intrs masked during handling of 1 */
extern void *intr_unit[];	/* cookies to pass to intr handlers */

inthand_t
	IDTVEC(fastintr0), IDTVEC(fastintr1),
	IDTVEC(fastintr2), IDTVEC(fastintr3),
	IDTVEC(fastintr4), IDTVEC(fastintr5),
	IDTVEC(fastintr6), IDTVEC(fastintr7),
	IDTVEC(fastintr8), IDTVEC(fastintr9),
	IDTVEC(fastintr10), IDTVEC(fastintr11),
	IDTVEC(fastintr12), IDTVEC(fastintr13),
	IDTVEC(fastintr14), IDTVEC(fastintr15);
inthand_t
	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);

unpendhand_t
	IDTVEC(fastunpend0), IDTVEC(fastunpend1),
	IDTVEC(fastunpend2), IDTVEC(fastunpend3),
	IDTVEC(fastunpend4), IDTVEC(fastunpend5),
	IDTVEC(fastunpend6), IDTVEC(fastunpend7),
	IDTVEC(fastunpend8), IDTVEC(fastunpend9),
	IDTVEC(fastunpend10), IDTVEC(fastunpend11),
	IDTVEC(fastunpend12), IDTVEC(fastunpend13),
	IDTVEC(fastunpend14), IDTVEC(fastunpend15);

#if defined(APIC_IO)
inthand_t
	IDTVEC(fastintr16), IDTVEC(fastintr17),
	IDTVEC(fastintr18), IDTVEC(fastintr19),
	IDTVEC(fastintr20), IDTVEC(fastintr21),
	IDTVEC(fastintr22), IDTVEC(fastintr23);
inthand_t
	IDTVEC(intr16), IDTVEC(intr17), IDTVEC(intr18), IDTVEC(intr19),
	IDTVEC(intr20), IDTVEC(intr21), IDTVEC(intr22), IDTVEC(intr23);
unpendhand_t
	IDTVEC(fastunpend16), IDTVEC(fastunpend17),
	IDTVEC(fastunpend18), IDTVEC(fastunpend19),
	IDTVEC(fastunpend20), IDTVEC(fastunpend21),
	IDTVEC(fastunpend22), IDTVEC(fastunpend23);
#endif

#if defined(SMP)
inthand_t
	Xinvltlb,	/* TLB shootdowns */
	Xcpuast,	/* Additional software trap on other cpu */ 
	Xforward_irq,	/* Forward irq to cpu holding ISR lock */
	Xcpustop,	/* CPU stops & waits for another CPU to restart it */
	Xspuriousint,	/* handle APIC "spurious INTs" */
	Xipiq,		/* handle lwkt_send_ipiq() requests */
	Xrendezvous;	/* handle CPU rendezvous */

#ifdef TEST_TEST1
inthand_t
	Xtest1;		/* 'fake' HWI at top of APIC prio 0x3x, 32+31 = 0x3f */
#endif /** TEST_TEST1 */
#endif /* SMP */

void	call_fast_unpend(int irq);
void	isa_defaultirq (void);
int	isa_nmi (int cd);
int	icu_setup (int intr, inthand2_t *func, void *arg, 
		       u_int *maskptr, int flags);
int	icu_unset (int intr, inthand2_t *handler);
void	icu_reinit (void);
int	update_intr_masks (void);

/*
 * WARNING: These are internal functions and not to be used by device drivers!
 * They are subject to change without notice. 
 */
struct intrec *inthand_add(const char *name, int irq, inthand2_t handler,
			   void *arg, intrmask_t *maskptr, int flags);

int inthand_remove(struct intrec *idesc);
void forward_fastint_remote(void *arg);

#endif /* LOCORE */

#endif /* _KERNEL */

#endif /* !_I386_ISA_INTR_MACHDEP_H_ */
