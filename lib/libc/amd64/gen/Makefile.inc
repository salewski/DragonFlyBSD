#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/amd64/gen/Makefile.inc,v 1.24 2003/10/13 20:32:33 alc Exp $
# $DragonFly: src/lib/libc/amd64/gen/Makefile.inc,v 1.1 2004/02/02 05:43:14 dillon Exp $

SRCS+=	_setjmp.S rfork_thread.S setjmp.S sigsetjmp.S \
	fabs.S modf.S \
	frexp.c infinity.c isinf.c ldexp.c \
	makecontext.c signalcontext.c \
	fpgetmask.c fpsetmask.c fpgetprec.c fpsetprec.c \
	fpgetround.c fpsetround.c fpgetsticky.c fpsetsticky.c
