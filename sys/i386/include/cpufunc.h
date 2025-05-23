/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 * $FreeBSD: src/sys/i386/include/cpufunc.h,v 1.96.2.3 2002/04/28 22:50:54 dwmalone Exp $
 * $DragonFly: src/sys/i386/include/Attic/cpufunc.h,v 1.10 2005/03/06 05:05:52 dillon Exp $
 */

/*
 * Functions to provide access to special i386 instructions.
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#include <sys/cdefs.h>

__BEGIN_DECLS
#define readb(va)	(*(volatile u_int8_t *) (va))
#define readw(va)	(*(volatile u_int16_t *) (va))
#define readl(va)	(*(volatile u_int32_t *) (va))

#define writeb(va, d)	(*(volatile u_int8_t *) (va) = (d))
#define writew(va, d)	(*(volatile u_int16_t *) (va) = (d))
#define writel(va, d)	(*(volatile u_int32_t *) (va) = (d))

#ifdef	__GNUC__

#ifdef SMP
#include "lock.h"		/* XXX */
#endif

#ifdef SWTCH_OPTIM_STATS
extern	int	tlb_flush_count;	/* XXX */
#endif

static __inline void
breakpoint(void)
{
	__asm __volatile("int $3");
}

/*
 * Find the first 1 in mask, starting with bit 0 and return the
 * bit number.  If mask is 0 the result is undefined.
 */
static __inline u_int
bsfl(u_int mask)
{
	u_int	result;

	__asm __volatile("bsfl %0,%0" : "=r" (result) : "0" (mask));
	return (result);
}

/*
 * Find the last 1 in mask, starting with bit 31 and return the
 * bit number.  If mask is 0 the result is undefined.
 */
static __inline u_int
bsrl(u_int mask)
{
	u_int	result;

	__asm __volatile("bsrl %0,%0" : "=r" (result) : "0" (mask));
	return (result);
}

/*
 * Test and set the specified bit (1 << bit) in the integer.  The
 * previous value of the bit is returned (0 or 1).
 */
static __inline int
btsl(u_int *mask, int bit)
{
	int result;

	__asm __volatile("btsl %2,%1; movl $0,%0; adcl $0,%0" :
		    "=r"(result), "=m"(*mask) : "r" (bit));
	return(result);
}

/*
 * Test and clear the specified bit (1 << bit) in the integer.  The
 * previous value of the bit is returned (0 or 1).
 */
static __inline int
btrl(u_int *mask, int bit)
{
	int result;

	__asm __volatile("btrl %2,%1; movl $0,%0; adcl $0,%0" :
		    "=r"(result), "=m"(*mask) : "r" (bit));
	return(result);
}

static __inline void
do_cpuid(u_int ax, u_int *p)
{
	__asm __volatile("cpuid"
			 : "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
			 :  "0" (ax));
}

static __inline void
cpu_disable_intr(void)
{
	__asm __volatile("cli" : : : "memory");
}

static __inline void
cpu_enable_intr(void)
{
	__asm __volatile("sti");
}

static __inline void
cpu_mb1(void)
{
	__asm __volatile("" : : : "memory");
}

static __inline void
cpu_mb2(void)
{
	__asm __volatile("cpuid" : : : "ax", "bx", "cx", "dx", "memory");
}

#define	HAVE_INLINE_FFS

static __inline int
ffs(int mask)
{
	/*
	 * Note that gcc-2's builtin ffs would be used if we didn't declare
	 * this inline or turn off the builtin.  The builtin is faster but
	 * broken in gcc-2.4.5 and slower but working in gcc-2.5 and later
	 * versions.
	 */
	 return (mask == 0 ? mask : (int)bsfl((u_int)mask) + 1);
}

#define	HAVE_INLINE_FLS

static __inline int
fls(int mask)
{
	return (mask == 0 ? mask : (int) bsrl((u_int)mask) + 1);
}

/*
 * The following complications are to get around gcc not having a
 * constraint letter for the range 0..255.  We still put "d" in the
 * constraint because "i" isn't a valid constraint when the port
 * isn't constant.  This only matters for -O0 because otherwise
 * the non-working version gets optimized away.
 * 
 * Use an expression-statement instead of a conditional expression
 * because gcc-2.6.0 would promote the operands of the conditional
 * and produce poor code for "if ((inb(var) & const1) == const2)".
 *
 * The unnecessary test `(port) < 0x10000' is to generate a warning if
 * the `port' has type u_short or smaller.  Such types are pessimal.
 * This actually only works for signed types.  The range check is
 * careful to avoid generating warnings.
 */
#define	inb(port) __extension__ ({					\
	u_char	_data;							\
	if (__builtin_constant_p(port) && ((port) & 0xffff) < 0x100	\
	    && (port) < 0x10000)					\
		_data = inbc(port);					\
	else								\
		_data = inbv(port);					\
	_data; })

#define	outb(port, data) (						\
	__builtin_constant_p(port) && ((port) & 0xffff) < 0x100		\
	&& (port) < 0x10000						\
	? outbc(port, data) : outbv(port, data))

static __inline u_char
inbc(u_int port)
{
	u_char	data;

	__asm __volatile("inb %1,%0" : "=a" (data) : "id" ((u_short)(port)));
	return (data);
}

static __inline void
outbc(u_int port, u_char data)
{
	__asm __volatile("outb %0,%1" : : "a" (data), "id" ((u_short)(port)));
}

static __inline u_char
inbv(u_int port)
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

static __inline u_int
inl(u_int port)
{
	u_int	data;

	__asm __volatile("inl %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline void
insb(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insb"
			 : "=D" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port)
			 : "memory");
}

static __inline void
insw(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insw"
			 : "=D" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port)
			 : "memory");
}

static __inline void
insl(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insl"
			 : "=D" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port)
			 : "memory");
}

static __inline void
invd(void)
{
	__asm __volatile("invd");
}

#if defined(_KERNEL)

/*
 * If we are not a true-SMP box then smp_invltlb() is a NOP.  Note that this
 * will cause the invl*() functions to be equivalent to the cpu_invl*()
 * functions.
 */
#ifdef SMP
void smp_invltlb(void);
#else
#define smp_invltlb()
#endif

/*
 * Invalidate a patricular VA on this cpu only
 */
static __inline void
cpu_invlpg(void *addr)
{
	__asm __volatile("invlpg %0" : : "m" (*(char *)addr) : "memory");
}

/*
 * Invalidate the TLB on this cpu only
 */
static __inline void
cpu_invltlb(void)
{
	u_int	temp;
	/*
	 * This should be implemented as load_cr3(rcr3()) when load_cr3()
	 * is inlined.
	 */
	__asm __volatile("movl %%cr3, %0; movl %0, %%cr3" : "=r" (temp)
			 : : "memory");
#if defined(SWTCH_OPTIM_STATS)
	++tlb_flush_count;
#endif
}

#endif	/* _KERNEL */

static __inline u_short
inw(u_int port)
{
	u_short	data;

	__asm __volatile("inw %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline u_int
loadandclear(volatile u_int *addr)
{
	u_int	result;

	__asm __volatile("xorl %0,%0; xchgl %1,%0"
			 : "=&r" (result) : "m" (*addr));
	return (result);
}

static __inline void
outbv(u_int port, u_char data)
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

static __inline void
outl(u_int port, u_int data)
{
	/*
	 * outl() and outw() aren't used much so we haven't looked at
	 * possible micro-optimizations such as the unnecessary
	 * assignment for them.
	 */
	__asm __volatile("outl %0,%%dx" : : "a" (data), "d" (port));
}

static __inline void
outsb(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsb"
			 : "=S" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port));
}

static __inline void
outsw(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsw"
			 : "=S" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port));
}

static __inline void
outsl(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsl"
			 : "=S" (addr), "=c" (cnt)
			 :  "0" (addr),  "1" (cnt), "d" (port));
}

static __inline void
outw(u_int port, u_short data)
{
	__asm __volatile("outw %0,%%dx" : : "a" (data), "d" (port));
}

static __inline u_int
rcr2(void)
{
	u_int	data;

	__asm __volatile("movl %%cr2,%0" : "=r" (data));
	return (data);
}

static __inline u_int
read_eflags(void)
{
	u_int	ef;

	__asm __volatile("pushfl; popl %0" : "=r" (ef));
	return (ef);
}

static __inline u_int64_t
rdmsr(u_int msr)
{
	u_int64_t rv;

	__asm __volatile(".byte 0x0f, 0x32" : "=A" (rv) : "c" (msr));
	return (rv);
}

static __inline u_int64_t
rdpmc(u_int pmc)
{
	u_int64_t rv;

	__asm __volatile(".byte 0x0f, 0x33" : "=A" (rv) : "c" (pmc));
	return (rv);
}

static __inline u_int64_t
rdtsc(void)
{
	u_int64_t rv;

	__asm __volatile(".byte 0x0f, 0x31" : "=A" (rv));
	return (rv);
}

static __inline void
wbinvd(void)
{
	__asm __volatile("wbinvd");
}

static __inline void
write_eflags(u_int ef)
{
	__asm __volatile("pushl %0; popfl" : : "r" (ef));
}

static __inline void
wrmsr(u_int msr, u_int64_t newval)
{
	__asm __volatile(".byte 0x0f, 0x30" : : "A" (newval), "c" (msr));
}

static __inline u_int
rfs(void)
{
	u_int sel;
	__asm __volatile("movl %%fs,%0" : "=rm" (sel));
	return (sel);
}

static __inline u_int
rgs(void)
{
	u_int sel;
	__asm __volatile("movl %%gs,%0" : "=rm" (sel));
	return (sel);
}

static __inline void
load_fs(u_int sel)
{
	__asm __volatile("movl %0,%%fs" : : "rm" (sel));
}

static __inline void
load_gs(u_int sel)
{
	__asm __volatile("movl %0,%%gs" : : "rm" (sel));
}

static __inline u_int
rdr0(void)
{
	u_int	data;
	__asm __volatile("movl %%dr0,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr0(u_int sel)
{
	__asm __volatile("movl %0,%%dr0" : : "r" (sel));
}

static __inline u_int
rdr1(void)
{
	u_int	data;
	__asm __volatile("movl %%dr1,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr1(u_int sel)
{
	__asm __volatile("movl %0,%%dr1" : : "r" (sel));
}

static __inline u_int
rdr2(void)
{
	u_int	data;
	__asm __volatile("movl %%dr2,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr2(u_int sel)
{
	__asm __volatile("movl %0,%%dr2" : : "r" (sel));
}

static __inline u_int
rdr3(void)
{
	u_int	data;
	__asm __volatile("movl %%dr3,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr3(u_int sel)
{
	__asm __volatile("movl %0,%%dr3" : : "r" (sel));
}

static __inline u_int
rdr4(void)
{
	u_int	data;
	__asm __volatile("movl %%dr4,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr4(u_int sel)
{
	__asm __volatile("movl %0,%%dr4" : : "r" (sel));
}

static __inline u_int
rdr5(void)
{
	u_int	data;
	__asm __volatile("movl %%dr5,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr5(u_int sel)
{
	__asm __volatile("movl %0,%%dr5" : : "r" (sel));
}

static __inline u_int
rdr6(void)
{
	u_int	data;
	__asm __volatile("movl %%dr6,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr6(u_int sel)
{
	__asm __volatile("movl %0,%%dr6" : : "r" (sel));
}

static __inline u_int
rdr7(void)
{
	u_int	data;
	__asm __volatile("movl %%dr7,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr7(u_int sel)
{
	__asm __volatile("movl %0,%%dr7" : : "r" (sel));
}

#else /* !__GNUC__ */

int	breakpoint	(void);
u_int	bsfl		(u_int mask);
u_int	bsrl		(u_int mask);
void	cpu_disable_intr (void);
void	do_cpuid	(u_int ax, u_int *p);
void	cpu_enable_intr	(void);
u_char	inb		(u_int port);
u_int	inl		(u_int port);
void	insb		(u_int port, void *addr, size_t cnt);
void	insl		(u_int port, void *addr, size_t cnt);
void	insw		(u_int port, void *addr, size_t cnt);
void	invd		(void);
u_short	inw		(u_int port);
u_int	loadandclear	(u_int *addr);
void	outb		(u_int port, u_char data);
void	outl		(u_int port, u_int data);
void	outsb		(u_int port, void *addr, size_t cnt);
void	outsl		(u_int port, void *addr, size_t cnt);
void	outsw		(u_int port, void *addr, size_t cnt);
void	outw		(u_int port, u_short data);
u_int	rcr2		(void);
u_int64_t rdmsr		(u_int msr);
u_int64_t rdpmc		(u_int pmc);
u_int64_t rdtsc		(void);
u_int	read_eflags	(void);
void	wbinvd		(void);
void	write_eflags	(u_int ef);
void	wrmsr		(u_int msr, u_int64_t newval);
u_int	rfs		(void);
u_int	rgs		(void);
void	load_fs		(u_int sel);
void	load_gs		(u_int sel);

#endif	/* __GNUC__ */

void	load_cr0	(u_int cr0);
void	load_cr3	(u_int cr3);
void	load_cr4	(u_int cr4);
void	ltr		(u_short sel);
u_int	rcr0		(void);
u_int	rcr3		(void);
u_int	rcr4		(void);
void    reset_dbregs    (void);
__END_DECLS

#endif /* !_MACHINE_CPUFUNC_H_ */
