/* $FreeBSD: src/contrib/gcc/ginclude/stddef.h,v 1.3 1999/10/13 15:55:31 obrien Exp $ */
/* $DragonFly: src/contrib/gcc/ginclude/Attic/stddef.h,v 1.3 2003/11/09 02:34:01 dillon Exp $ */

#if (!defined(_STDDEF_H) && !defined(_STDDEF_H_) && !defined(_ANSI_STDDEF_H) \
     && !defined(__STDDEF_H__)) \
    || defined(__need_wchar_t) || defined(__need_size_t) \
    || defined(__need_ptrdiff_t) || defined(__need_NULL) \
    || defined(__need_wint_t)

/* Any one of these symbols __need_* means that GNU libc
   wants us just to define one data type.  So don't define
   the symbols that indicate this file's entire job has been done.  */
#if (!defined(__need_wchar_t) && !defined(__need_size_t)	\
     && !defined(__need_ptrdiff_t) && !defined(__need_NULL)	\
     && !defined(__need_wint_t))
#define _STDDEF_H
#define _STDDEF_H_
/* snaroff@next.com says the NeXT needs this.  */
#define _ANSI_STDDEF_H
/* Irix 5.1 needs this.  */
#define __STDDEF_H__
#endif

#ifndef __sys_stdtypes_h
/* This avoids lossage on SunOS but only if stdtypes.h comes first.
   There's no way to win with the other order!  Sun lossage.  */

/* On 4.3bsd-net2, make sure stdint.h is included, so we have
   one less case to deal with in the following.  */
#if defined (__BSD_NET2__) || defined (____386BSD____) || defined (__FreeBSD__) || defined(__NetBSD__)
#include <machine/stdint.h>
#endif

/* In 4.3bsd-net2, machine/ansi.h defines these symbols, which are
   defined if the corresponding type is *not* defined.
   FreeBSD-2.1 defines _MACHINE_ANSI_H_ instead of _ANSI_H_ */
#if defined(_ANSI_H_) || defined(_MACHINE_ANSI_H_)
#if !defined(_SIZE_T_) && !defined(_BSD_SIZE_T_)
#define _SIZE_T
#endif
#if !defined(_PTRDIFF_T_) && !defined(_BSD_PTRDIFF_T_)
#define _PTRDIFF_T
#endif
/* On BSD/386 1.1, at least, machine/stdint.h defines _BSD_WCHAR_T_
   instead of _WCHAR_T_. */
#if !defined(_WCHAR_T_) && !defined(_BSD_WCHAR_T_)
#ifndef _BSD_WCHAR_T_
#define _WCHAR_T
#endif
#endif
/* Undef _FOO_T_ if we are supposed to define foo_t.  */
#if defined (__need_ptrdiff_t) || defined (_STDDEF_H_)
#undef _PTRDIFF_T_
#undef _BSD_PTRDIFF_T_
#endif
#if defined (__need_size_t) || defined (_STDDEF_H_)
#undef _SIZE_T_
#undef _BSD_SIZE_T_
#endif
#if defined (__need_wchar_t) || defined (_STDDEF_H_)
#undef _WCHAR_T_
#undef _BSD_WCHAR_T_
#endif
#endif /* defined(_ANSI_H_) || defined(_MACHINE_ANSI_H_) */

/* Sequent's header files use _PTRDIFF_T_ in some conflicting way.
   Just ignore it.  */
#if defined (__sequent__) && defined (_PTRDIFF_T_)
#undef _PTRDIFF_T_
#endif

/* On VxWorks, <type/vxTypesBase.h> may have defined macros like
   _TYPE_size_t which will typedef size_t.  fixincludes patched the
   vxTypesBase.h so that this macro is only defined if _GCC_SIZE_T is
   not defined, and so that defining this macro defines _GCC_SIZE_T.
   If we find that the macros are still defined at this point, we must
   invoke them so that the type is defined as expected.  */
#if defined (_TYPE_ptrdiff_t) && (defined (__need_ptrdiff_t) || defined (_STDDEF_H_))
_TYPE_ptrdiff_t;
#undef _TYPE_ptrdiff_t
#endif
#if defined (_TYPE_size_t) && (defined (__need_size_t) || defined (_STDDEF_H_))
_TYPE_size_t;
#undef _TYPE_size_t
#endif
#if defined (_TYPE_wchar_t) && (defined (__need_wchar_t) || defined (_STDDEF_H_))
_TYPE_wchar_t;
#undef _TYPE_wchar_t
#endif

/* In case nobody has defined these types, but we aren't running under
   GCC 2.00, make sure that __PTRDIFF_TYPE__, __SIZE__TYPE__, and
   __WCHAR_TYPE__ have reasonable values.  This can happen if the
   parts of GCC is compiled by an older compiler, that actually
   include gstddef.h, such as collect2.  */

/* Signed type of difference of two pointers.  */

/* Define this type if we are doing the whole job,
   or if we want this type in particular.  */
#if defined (_STDDEF_H) || defined (__need_ptrdiff_t)
#ifndef _PTRDIFF_T	/* in case <sys/types.h> has defined it. */
#ifndef _T_PTRDIFF_
#ifndef _T_PTRDIFF
#ifndef __PTRDIFF_T
#ifndef _PTRDIFF_T_
#ifndef _BSD_PTRDIFF_T_
#ifndef ___int_ptrdiff_t_h
#ifndef _GCC_PTRDIFF_T
#define _PTRDIFF_T
#define _T_PTRDIFF_
#define _T_PTRDIFF
#define __PTRDIFF_T
#define _PTRDIFF_T_
#define _BSD_PTRDIFF_T_
#define ___int_ptrdiff_t_h
#define _GCC_PTRDIFF_T
#ifndef __PTRDIFF_TYPE__
#define __PTRDIFF_TYPE__ long int
#endif
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#endif /* _GCC_PTRDIFF_T */
#endif /* ___int_ptrdiff_t_h */
#endif /* _BSD_PTRDIFF_T_ */
#endif /* _PTRDIFF_T_ */
#endif /* __PTRDIFF_T */
#endif /* _T_PTRDIFF */
#endif /* _T_PTRDIFF_ */
#endif /* _PTRDIFF_T */

/* If this symbol has done its job, get rid of it.  */
#undef	__need_ptrdiff_t

#endif /* _STDDEF_H or __need_ptrdiff_t.  */

/* Unsigned type of `sizeof' something.  */

/* Define this type if we are doing the whole job,
   or if we want this type in particular.  */
#if defined (_STDDEF_H) || defined (__need_size_t)
#ifndef __size_t__	/* BeOS */
#ifndef _SIZE_T	/* in case <sys/types.h> has defined it. */
#ifndef _SYS_SIZE_T_H
#ifndef _T_SIZE_
#ifndef _T_SIZE
#ifndef __SIZE_T
#ifndef _SIZE_T_
#ifndef _BSD_SIZE_T_
#ifndef _SIZE_T_DEFINED_
#ifndef _SIZE_T_DEFINED
#ifndef ___int_size_t_h
#ifndef _GCC_SIZE_T
#ifndef _SIZET_
#ifndef __size_t
#define __size_t__	/* BeOS */
#define _SIZE_T
#define _SYS_SIZE_T_H
#define _T_SIZE_
#define _T_SIZE
#define __SIZE_T
#define _SIZE_T_
#define _BSD_SIZE_T_
#define _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
#define ___int_size_t_h
#define _GCC_SIZE_T
#define _SIZET_
#define __size_t
#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ long unsigned int
#endif
#if !(defined (__GNUG__) && defined (size_t))
typedef __SIZE_TYPE__ size_t;
#ifdef __BEOS__
typedef long ssize_t;
#endif /* __BEOS__ */
#endif /* !(defined (__GNUG__) && defined (size_t)) */
#endif /* __size_t */
#endif /* _SIZET_ */
#endif /* _GCC_SIZE_T */
#endif /* ___int_size_t_h */
#endif /* _SIZE_T_DEFINED */
#endif /* _SIZE_T_DEFINED_ */
#endif /* _BSD_SIZE_T_ */
#endif /* _SIZE_T_ */
#endif /* __SIZE_T */
#endif /* _T_SIZE */
#endif /* _T_SIZE_ */
#endif /* _SYS_SIZE_T_H */
#endif /* _SIZE_T */
#endif /* __size_t__ */
#undef	__need_size_t
#endif /* _STDDEF_H or __need_size_t.  */


/* Wide character type.
   Locale-writers should change this as necessary to
   be big enough to hold unique values not between 0 and 127,
   and not (wchar_t) -1, for each defined multibyte character.  */

/* Define this type if we are doing the whole job,
   or if we want this type in particular.  */
#if defined (_STDDEF_H) || defined (__need_wchar_t)
#ifndef __wchar_t__	/* BeOS */
#ifndef _WCHAR_T
#ifndef _T_WCHAR_
#ifndef _T_WCHAR
#ifndef __WCHAR_T
#ifndef _WCHAR_T_
#ifndef _BSD_WCHAR_T_
#ifndef _WCHAR_T_DEFINED_
#ifndef _WCHAR_T_DEFINED
#ifndef _WCHAR_T_H
#ifndef ___int_wchar_t_h
#ifndef __INT_WCHAR_T_H
#ifndef _GCC_WCHAR_T
#define __wchar_t__	/* BeOS */
#define _WCHAR_T
#define _T_WCHAR_
#define _T_WCHAR
#define __WCHAR_T
#define _WCHAR_T_
#define _BSD_WCHAR_T_
#define _WCHAR_T_DEFINED_
#define _WCHAR_T_DEFINED
#define _WCHAR_T_H
#define ___int_wchar_t_h
#define __INT_WCHAR_T_H
#define _GCC_WCHAR_T

/* On BSD/386 1.1, at least, machine/stdint.h defines _BSD_WCHAR_T_
   instead of _WCHAR_T_, and _BSD_RUNE_T_ (which, unlike the other
   symbols in the _FOO_T_ family, stays defined even after its
   corresponding type is defined).  If we define wchar_t, then we
   must undef _WCHAR_T_; for BSD/386 1.1 (and perhaps others), if
   we undef _WCHAR_T_, then we must also define rune_t, since 
   headers like runetype.h assume that if machine/stdint.h is included,
   and _BSD_WCHAR_T_ is not defined, then rune_t is available.
   machine/stdint.h says, "Note that _WCHAR_T_ and _RUNE_T_ must be of
   the same type." */
#ifdef _BSD_WCHAR_T_
#undef _BSD_WCHAR_T_
#ifdef _BSD_RUNE_T_
#if !defined (_ANSI_SOURCE) && !defined (_POSIX_SOURCE)
typedef _BSD_RUNE_T_ rune_t;
#endif
#endif
#endif

#ifndef __WCHAR_TYPE__
#ifdef __BEOS__
#define __WCHAR_TYPE__ unsigned char
#else
#define __WCHAR_TYPE__ int
#endif
#endif
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif /* __wchar_t__ */
#undef	__need_wchar_t
#endif /* _STDDEF_H or __need_wchar_t.  */

#if defined (_STDDEF_H) || defined (__need_wint_t)
#ifndef _WINT_T
#define _WINT_T

#ifndef __WINT_TYPE__
#define __WINT_TYPE__ unsigned int
#endif
typedef __WINT_TYPE__ wint_t;
#endif
#undef __need_wint_t
#endif

/*  In 4.3bsd-net2, leave these undefined to indicate that size_t, etc.
    are already defined.  */
/*  BSD/OS 3.1 requires the MACHINE_ANSI_H check here.  FreeBSD 2.x apparently
    does not, even though there is a check for MACHINE_ANSI_H above.  */
#if defined(_ANSI_H_) || (defined(__bsdi__) && defined(_MACHINE_ANSI_H_))
/*  The references to _GCC_PTRDIFF_T_, _GCC_SIZE_T_, and _GCC_WCHAR_T_
    are probably typos and should be removed before 2.8 is released.  */
#ifdef _GCC_PTRDIFF_T_
#undef _PTRDIFF_T_
#undef _BSD_PTRDIFF_T_
#endif
#ifdef _GCC_SIZE_T_
#undef _SIZE_T_
#undef _BSD_SIZE_T_
#endif
#ifdef _GCC_WCHAR_T_
#undef _WCHAR_T_
#undef _BSD_WCHAR_T_
#endif
/*  The following ones are the real ones.  */
#ifdef _GCC_PTRDIFF_T
#undef _PTRDIFF_T_
#undef _BSD_PTRDIFF_T_
#endif
#ifdef _GCC_SIZE_T
#undef _SIZE_T_
#undef _BSD_SIZE_T_
#endif
#ifdef _GCC_WCHAR_T
#undef _WCHAR_T_
#undef _BSD_WCHAR_T_
#endif
#endif /* _ANSI_H_ || ( __bsdi__ && _MACHINE_ANSI_H_ ) */

#endif /* __sys_stdtypes_h */

/* A null pointer constant.  */

#if defined (_STDDEF_H) || defined (__need_NULL)
#undef NULL		/* in case <stdio.h> has defined it. */
#ifdef __GNUG__
#define NULL __null
#else   /* G++ */
#define NULL ((void *)0)
#endif  /* G++ */
#endif	/* NULL not defined and <stddef.h> or need NULL.  */
#undef	__need_NULL

#ifdef _STDDEF_H

/* Offset of member MEMBER in a struct of type TYPE.  */

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#endif /* _STDDEF_H was defined this time */

#endif /* !_STDDEF_H && !_STDDEF_H_ && !_ANSI_STDDEF_H && !__STDDEF_H__
	  || __need_XXX was not defined before */
