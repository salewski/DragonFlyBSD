/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
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
 *	@(#)cdefs.h	8.8 (Berkeley) 1/9/95
 * $FreeBSD: src/sys/sys/cdefs.h,v 1.28.2.8 2002/09/18 04:05:13 mikeh Exp $
 * $DragonFly: src/sys/sys/cdefs.h,v 1.15 2005/02/16 21:50:00 joerg Exp $
 */

#ifndef	_SYS_CDEFS_H_
#define	_SYS_CDEFS_H_

#if defined(__cplusplus)
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif

/*
 * Macro to test if we are using a specific version of gcc or later.
 */
#ifdef __GNUC__
#define __GNUC_PREREQ__(ma, mi)		\
        (__GNUC__ > (ma) || __GNUC__ == (ma) && __GNUC_MINOR__ >= (mi))
#else
#define __GNUC_PREREQ__(ma, mi) 	\
	0
#endif

/*
 * The __VM_CACHELINE_SIZE macro defines the common cache line alignment
 * size that can be found across most recent and somewhat latest Intel
 * hardware, i.e. L1 cache sizes etc.
 *
 * If needed, this value can be TUNED.  Suitable values for this macro
 * are 32, 64 and 128 bytes.  The unit of measurement for this macro is
 * bytes.
 * 
 * XXX: This macro and related macros will eventually move to a MD
 * header, but currently, we do need such a hierarchy.
 */
#define	__VM_CACHELINE_SIZE	32

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky to use if it must work in non-ANSI
 * mode -- there must be no spaces between its arguments, and for nested
 * __CONCAT's, all the __CONCAT's must be at the left.  __CONCAT can also
 * concatenate double-quoted strings produced by the __STRING macro, but
 * this only works with ANSI C.
 *
 * __XSTRING is like __STRING, but it expands any macros in its argument
 * first.  It is only available with ANSI C.
 */
#if defined(__STDC__) || defined(__cplusplus)
#define	__P(protos)	protos		/* full-blown ANSI C */
#define	__CONCAT1(x,y)	x ## y
#define	__CONCAT(x,y)	__CONCAT1(x,y)
#define	__STRING(x)	#x		/* stringify without expanding x */
#define	__XSTRING(x)	__STRING(x)	/* expand x, then stringify */

#define	__const		const		/* define reserved names to standard */
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus)
#define	__inline	inline		/* convert to C++ keyword */
#else
#ifndef __GNUC__
#define	__inline			/* delete GCC keyword */
#endif /* !__GNUC__ */
#endif /* !__cplusplus */

#else	/* !(__STDC__ || __cplusplus) */
#define	__P(protos)	()		/* traditional C preprocessor */
#define	__CONCAT(x,y)	x/**/y
#define	__STRING(x)	"x"

#ifndef __GNUC__
#define	__const				/* delete pseudo-ANSI C keywords */
#define	__inline
#define	__signed
#define	__volatile
/*
 * In non-ANSI C environments, new programs will want ANSI-only C keywords
 * deleted from the program and old programs will want them left alone.
 * When using a compiler other than gcc, programs using the ANSI C keywords
 * const, inline etc. as normal identifiers should define -DNO_ANSI_KEYWORDS.
 * When using "gcc -traditional", we assume that this is the intent; if
 * __GNUC__ is defined but __STDC__ is not, we leave the new keywords alone.
 */
#ifndef	NO_ANSI_KEYWORDS
#define	const				/* delete ANSI C keywords */
#define	inline
#define	signed
#define	volatile
#endif	/* !NO_ANSI_KEYWORDS */
#endif	/* !__GNUC__ */
#endif	/* !(__STDC__ || __cplusplus) */

/*
 * Compiler-dependent macros to help declare dead (non-returning) and
 * pure (no side effects) functions, and unused variables.  They are
 * null except for versions of gcc that are known to support the features
 * properly (old versions of gcc-2 supported the dead and pure features
 * in a different (wrong) way).
 */
#ifdef lint

#define __dead2
#define	__pure
#define __pure2
#define __unused
#define __packed
#define __aligned(x)
#define __section(x)
#define __always_inline
#define __nonnull(x)

#else

#if !__GNUC_PREREQ__(2, 7)
#define __dead2
#define __pure2
#define __unused
#endif

#if __GNUC_PREREQ__(2, 7)
#define __dead2		__attribute__((__noreturn__))
#define __pure2		__attribute__((__const__))
#define __unused	__attribute__((__unused__))
#define __packed        __attribute__((__packed__))
#define __aligned(x)    __attribute__((__aligned__(x)))
#define __section(x)    __attribute__((__section__(x)))
#endif

#if __GNUC_PREREQ__(3, 0)
#define	__pure		__attribute__((__pure__))
#else
#define	__pure		__pure2
#endif

#if __GNUC_PREREQ__(3, 1)
#define __always_inline __attribute__((__always_inline__))
#else
#define __always_inline
#endif

#if __GNUC_PREREQ__(3, 3)
#define __nonnull(x)    __attribute__((__nonnull__(x)))
#define	__used		__attribute__((__used__))
#else
#define __nonnull(x)
#define __used		__unused
#endif

#endif	/* LINT */

#if !__GNUC_PREREQ__(2, 7) && __STDC_VERSION < 199901
#define __func__        NULL
#endif

#if (__GNUC_PREREQ__(2, 0) && !defined(__STRICT_ANSI)) || \
    __STDC_VERSION__ >= 199901
#define	__LONG_LONG_SUPPORTED
#endif

/*
 * GCC 2.95 and later provides `__restrict' as an extention to C90 to support
 * the C99-specific `restrict' type qualifier.  We happen to use `__restrict'
 * as a way to define the `restrict' type qualifier without disturbing older
 * software that is unaware of C99 keywords.
 */
#if !__GNUC_PREREQ__(2, 95)
#if __STDC_VERSION__ < 199901
#define	__restrict
#else
#define	__restrict	restrict
#endif
#endif

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2
 * didn't permit keeping the keywords out of the application namespace).
 *
 * The printf0like macro for GCC 2 uses DragonFly specific compiler extensions.
 */
#if !__GNUC_PREREQ__(2, 7)
#define	__printflike(fmtarg, firstvararg)
#define	__scanflike(fmtarg, firstvararg)
#define	__printf0like(fmtarg, firstvararg)
#elif __GNUC_PREREQ__(3, 0)
#define	__printflike(fmtarg, firstvararg) \
            __attribute__((__nonnull__(fmtarg), \
			  __format__ (__printf__, fmtarg, firstvararg)))
#define	__printf0like(fmtarg, firstvararg) \
            __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define	__scanflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))

#else
#define	__printflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define	__printf0like(fmtarg, firstvararg) \
	    __attribute__((__format__ (__printf0__, fmtarg, firstvararg)))
#define	__scanflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#endif

#if !__GNUC_PREREQ__(3, 0)
#define __ARRAY_ZERO	0
#else
#define __ARRAY_ZERO
#endif

/*
 * Handy GCC based macros:
 *
 * 	__cachealign:
 * 	
 * 	The __cachealign macro can be used for cache line aligning structures
 * 	of small to medium size.  It aligns the particular structure or
 * 	storage type to a system default cache line alignment, thus giving us
 * 	a much more better cache utilization by making the hardware work at
 * 	its best burst speeds.
 *
 * 	__usereg:
 * 	
 * 	The __usereg macro can/should be used when a function contains
 * 	arguments not more than 3.  It can be very useful to us due to the
 * 	message-passing nature of the kernel.
 *
 * !!NOTE - USAGE INFORMATION!!
 *
 * The __cachealign macro should not be used for data structures that are
 * as big struct proc, struct vnode, struct thread, and other structs which
 * are as big as them; simply because it will be useless in that case.
 *
 * The __usereg macro should be used whenever possible, i.e., when a function
 * does not exceed more than 3 arguments, and should not be used for vararg
 * type functions.
 *
 * In other words, AVOID MISUSE OF THESE MACROS. :-)
 */
#ifdef __GNUC__
#define	__cachealign	__attribute__((__aligned__(__VM_CACHELINE_SIZE)))
#define	__usereg     	__attribute__((__regparm__(3)))
#else
#define	__cachealign
#define	__usereg
#endif

#ifdef __GNUC__
#define __strong_reference(sym,aliassym)	\
	extern __typeof (sym) aliassym __attribute__ ((__alias__ (#sym)));
#define	__weak_reference(sym,alias)	\
	__asm__(".weak " #alias);	\
	__asm__(".equ "  #alias ", " #sym)
#define	__warn_references(sym,msg)	\
	__asm__(".section .gnu.warning." #sym);	\
	__asm__(".asciz \"" msg "\"");	\
	__asm__(".previous")
#endif	/* __GNUC__ */

#if defined(__GNUC__)
#define	__IDSTRING(name,string)	__asm__(".ident\t\"" string "\"")
#endif

#ifndef	__RCSID
#define	__RCSID(s)	__IDSTRING(rcsid,s)
#endif

#ifndef	__RCSID_SOURCE
#define	__RCSID_SOURCE(s) __IDSTRING(rcsid_source,s)
#endif

#ifndef	__COPYRIGHT
#define	__COPYRIGHT(s)	__IDSTRING(copyright,s)
#endif

#ifndef	__DECONST
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif

#ifndef	__DEVOLATILE
#define	__DEVOLATILE(type, var)	((type)(uintptr_t)(volatile void *)(var))
#endif

#ifndef	__DEQUALIFY
#define	__DEQUALIFY(type, var)	((type)(uintptr_t)(const volatile void *)(var))
#endif

/*-
 * The following definitions are an extension of the behavior originally
 * implemented in <sys/_posix.h>, but with a different level of granularity.
 * POSIX.1 requires that the macros we test be defined before any standard
 * header file is included.
 *
 * Here's a quick run-down of the versions:
 *  defined(_POSIX_SOURCE)		1003.1-1988
 *  _POSIX_C_SOURCE == 1		1003.1-1990
 *  _POSIX_C_SOURCE == 2		1003.2-1992 C Language Binding Option
 *  _POSIX_C_SOURCE == 199309		1003.1b-1993
 *  _POSIX_C_SOURCE == 199506		1003.1c-1995, 1003.1i-1995,
 *					and the omnibus ISO/IEC 9945-1: 1996
 *  _POSIX_C_SOURCE == 200112		1003.1-2001
 *
 * In addition, the X/Open Portability Guide, which is now the Single UNIX
 * Specification, defines a feature-test macro which indicates the version of
 * that specification, and which subsumes _POSIX_C_SOURCE.
 *
 * Our macros begin with two underscores to avoid namespace screwage.
 */

#if defined(_POSIX_C_SOURCE)

/* Deal with IEEE Std. 1003.1-1990, in which _POSIX_C_SOURCE == 1. */
#if _POSIX_C_SOURCE == 1
#undef _POSIX_C_SOURCE		/* Probably illegal, but beyond caring now. */
#define	_POSIX_C_SOURCE		199009
#endif

/* Deal with IEEE Std. 1003.2-1992, in which _POSIX_C_SOURCE == 2. */
#if _POSIX_C_SOURCE == 2
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		199209
#endif

#endif	/* _POSIX_C_SOURCE */

/* Deal with various X/Open Portability Guides and Single UNIX Spec. */
#ifdef _XOPEN_SOURCE
#if _XOPEN_SOURCE - 0 >= 600
#define	__XSI_VISIBLE		600
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		200112
#elif _XOPEN_SOURCE - 0 >= 500
#define	__XSI_VISIBLE		500
#undef _POSIX_C_SOURCE
#define	_POSIX_C_SOURCE		199506
#endif
#endif

/*
 * Deal with all versions of POSIX.  The ordering relative to the tests above is
 * important.
 */
#if defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)
#define	_POSIX_C_SOURCE		198808
#endif
#ifdef _POSIX_C_SOURCE
#if _POSIX_C_SOURCE >= 200112
#define	__POSIX_VISIBLE		200112
#define	__ISO_C_VISIBLE		1999
#elif _POSIX_C_SOURCE >= 199506
#define	__POSIX_VISIBLE		199506
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199309
#define	__POSIX_VISIBLE		199309
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199209
#define	__POSIX_VISIBLE		199209
#define	__ISO_C_VISIBLE		1990
#elif _POSIX_C_SOURCE >= 199009
#define	__POSIX_VISIBLE		199009
#define	__ISO_C_VISIBLE		1990
#else
#define	__POSIX_VISIBLE		198808
#define	__ISO_C_VISIBLE		0
#endif /* _POSIX_C_SOURCE */
#else
/*-
 * Deal with _ANSI_SOURCE:
 * If it is defined, and no other compilation environment is explicitly
 * requested, then define our internal feature-test macros to zero.  This
 * makes no difference to the preprocessor (undefined symbols in preprocessing
 * expressions are defined to have value zero), but makes it more convenient for
 * a test program to print out the values.
 *
 * If a program mistakenly defines _ANSI_SOURCE and some other macro such as
 * _POSIX_C_SOURCE, we will assume that it wants the broader compilation
 * environment (and in fact we will never get here).
 */
#ifdef _ANSI_SOURCE		/* Hide almost everything. */
#define	__POSIX_VISIBLE		0
#define	__XSI_VISIBLE		0
#define	__BSD_VISIBLE		0
#define	__ISO_C_VISIBLE		1990
#elif defined(_C99_SOURCE)	/* Localism to specify strict C99 env. */
#define	__POSIX_VISIBLE		0
#define	__XSI_VISIBLE		0
#define	__BSD_VISIBLE		0
#define	__ISO_C_VISIBLE		1999
#else				/* Default environment: show everything. */
#define	__POSIX_VISIBLE		200112
#define	__XSI_VISIBLE		600
#define	__BSD_VISIBLE		1
#define	__ISO_C_VISIBLE		1999
#endif
#endif

#endif /* !_SYS_CDEFS_H_ */
