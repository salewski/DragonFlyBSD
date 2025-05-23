# Makefile for syscall tables
#
# $FreeBSD: src/sys/svr4/Makefile,v 1.3 1999/09/01 00:32:18 newton Exp $
# $DragonFly: src/sys/emulation/svr4/Attic/Makefile,v 1.9 2004/08/13 17:51:09 dillon Exp $

# This builds the module
#
#
.PATH:	${.CURDIR}/../../emulation/svr4/${MACHINE_ARCH} ${.CURDIR}/../../emulation/svr4
ARCH=	arch_svr4
KMOD=	svr4
SRCS=	svr4_sysent.c svr4_sysvec.c opt_compat.h opt_svr4.h opt_vmpage.h \
	imgact_svr4.c svr4_signal.c svr4_fcntl.c svr4_misc.c \
	svr4_ioctl.c svr4_stat.c svr4_filio.c svr4_ttold.c svr4_termios.c \
	svr4_stream.c svr4_socket.c svr4_sockio.c svr4_machdep.c \
	svr4_resource.c svr4_ipc.c
OBJS=	svr4_locore.o 
MAN=	svr4.8

EXPORT_SYMS=_svr4_mod
CLEANFILES= svr4_assym.h svr4_genassym.o opt_svr4.h opt_compat.h

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

opt_svr4.h:
	touch opt_svr4.h
.if defined(DEBUG)
	echo "#define DEBUG_SVR4 1" >> opt_svr4.h
.endif

.include <bsd.kmod.mk>

svr4_assym.h:	svr4_genassym.o
.if exists(@)
svr4_assym.h:	@/kern/genassym.sh
.endif
	sh @/kern/genassym.sh svr4_genassym.o > ${.TARGET}

svr4_locore.o:	svr4_locore.s svr4_assym.h
	${CC} -c -x assembler-with-cpp -DLOCORE ${CFLAGS} \
		${.IMPSRC} -o ${.TARGET}

svr4_genassym.o:	svr4_genassym.c svr4.h @ machine ${ARCH}
	${CC} -c ${CFLAGS:N-fno-common} ${.IMPSRC}

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/svr4.sh ${DESTDIR}/usr/sbin/svr4

sysent:
	cd ${.CURDIR} && ${MAKE} -f Makefile.misc sysent
