# $FreeBSD: src/sys/boot/ficl/Makefile,v 1.35 2003/06/30 19:08:49 ru Exp $
# $DragonFly: src/sys/boot/ficl/Makefile,v 1.6.2.1 2005/04/08 00:45:22 dillon Exp $
#
.if ${MACHINE_ARCH} == "amd64"
.MAKEFLAGS:  MACHINE_ARCH=i386 MACHINE=i386 REALLY_AMD64=true
.endif

.PATH: ${.CURDIR}/${MACHINE_ARCH}
BASE_SRCS=	dict.c ficl.c fileaccess.c float.c loader.c math64.c \
		prefix.c search.c stack.c tools.c vm.c words.c

SRCS=		${BASE_SRCS} sysdep.c softcore.c
#OBJS+=		stack_protector.o

stack_protector.o: ../../libkern/stack_protector.c

CLEANFILES=	softcore.c testmain testmain.o
.if ${CCVER} == "gcc34"
CFLAGS+=	-fno-stack-protector
.endif
CFLAGS+=	-ffreestanding
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-mpreferred-stack-boundary=2
.endif
.if HAVE_PNP
CFLAGS+=	-DHAVE_PNP
.endif
.ifmake testmain
CFLAGS+=	-DTESTMAIN -D_TESTMAIN
SRCS+=		testmain.c
PROG=		testmain
.include <bsd.prog.mk>
.else
LIB=		ficl
INTERNALLIB=	yes
.include <bsd.lib.mk>
.endif

# Standard softwords
.PATH: ${.CURDIR}/softwords
SOFTWORDS=	softcore.fr jhlocal.fr marker.fr freebsd.fr ficllocal.fr \
		ifbrack.fr
# Optional OO extension softwords
#SOFTWORDS+=	oo.fr classes.fr

.if defined(REALLY_AMD64)
CFLAGS+=	-m32 -I.
.endif

CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/${MACHINE_ARCH} -I${.CURDIR}/../common

softcore.c: ${SOFTWORDS} softcore.awk
	(cd ${.CURDIR}/softwords; cat ${SOFTWORDS} \
	    | awk -f softcore.awk -v datestamp="`LC_ALL=C date`") > ${.TARGET}

.if defined(REALLY_AMD64)
${SRCS:M*.c:R:S/$/.o/g}: machine

beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../i386/include machine

CLEANFILES+=	machine
.endif
