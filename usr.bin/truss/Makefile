# $FreeBSD: src/usr.bin/truss/Makefile,v 1.10.2.2 2002/07/23 09:18:30 ru Exp $
# $DragonFly: src/usr.bin/truss/Makefile,v 1.6 2004/07/21 16:13:43 drhodus Exp $

PROG=	truss
SRCS=	main.c setup.c syscalls.c syscalls.h ioctl.c
.if (${MACHINE_ARCH} == "alpha")
SRCS+=	alpha-fbsd.c
.elif (${MACHINE_ARCH} == "i386")
SRCS+=	i386-fbsd.c i386-linux.c linux_syscalls.h
.endif

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../sys -I.

CLEANFILES+=i386l-syscalls.master syscalls.master linux_syscalls.h \
	syscalls.h ioctl.c

.SUFFIXES: .master

i386l-syscalls.master:	${.CURDIR}/../../sys/emulation/linux/i386/syscalls.master
	cp ${.ALLSRC} i386l-syscalls.master

linux_syscalls.h:	i386l-syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh i386l-syscalls.master \
		${.CURDIR}/i386linux.conf

syscalls.master:	${.CURDIR}/../../sys/kern/syscalls.master
	cp ${.ALLSRC} syscalls.master

syscalls.h:	syscalls.master
	/bin/sh ${.CURDIR}/../../sys/kern/makesyscalls.sh syscalls.master \
		${.CURDIR}/i386.conf

ioctl.c: ${.CURDIR}/../kdump/mkioctls
	sh ${.CURDIR}/../kdump/mkioctls ${DESTDIR}/usr/include > ${.TARGET}

.include <bsd.prog.mk>
