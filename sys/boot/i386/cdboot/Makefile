# $FreeBSD: src/sys/boot/i386/cdboot/Makefile,v 1.5 2002/09/17 01:48:55 peter Exp $
# $DragonFly: src/sys/boot/i386/cdboot/Attic/Makefile,v 1.4 2004/01/31 06:56:42 dillon Exp $

ORG=		0x7c00

PROG=		cdboot
NOMAN=
STRIP=
BINDIR?=	/boot

${PROG}: ${PROG}.o
	${LD} -N -e start -Ttext ${ORG} -o ${PROG}.out ${PROG}.o
	objcopy -S -O binary ${PROG}.out ${.TARGET}

CLEANFILES+=	${PROG}.o ${PROG}.out

.include <bsd.prog.mk>
