#	@(#)Makefile	5.2 (Berkeley) 12/28/90
# $FreeBSD: src/usr.bin/make/Makefile,v 1.13.2.1 2001/05/25 08:33:40 sobomax Exp $
# $DragonFly: src/usr.bin/make/Makefile,v 1.18 2005/01/09 18:32:28 okumoto Exp $

PROG=	make
CFLAGS+=-I${.CURDIR}
SRCS=	arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c lst.c main.c \
	make.c parse.c str.c suff.c targ.c util.c var.c var_modify.c

WARNS?=	6
NOSHARED?=	YES

CFLAGS+=-DMAKE_VERSION=\"5200408120\"

# There is no obvious performance improvement currently.
# CFLAGS+=-DUSE_KQUEUE

main.o: ${MAKEFILE}

# Set the shell which make(1) uses.  Bourne is the default, but a decent
# Korn shell works fine, and much faster.  Using the C shell for this
# will almost certainly break everything, but it's Unix tradition to
# allow you to shoot yourself in the foot if you want to :-)

MAKE_SHELL?=	sh
.if ${MAKE_SHELL} == "csh"
CFLAGS+=	-DDEFSHELL=0
.elif ${MAKE_SHELL} == "sh"
CFLAGS+=	-DDEFSHELL=1
.elif ${MAKE_SHELL} == "ksh"
CFLAGS+=	-DDEFSHELL=2
.else
.error "MAKE_SHELL must be set to one of \"csh\", \"sh\" or \"ksh\"."
.endif

.include <bsd.prog.mk>
