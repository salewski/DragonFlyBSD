# $FreeBSD: src/sys/modules/ibcs2/Makefile,v 1.26.2.3 2002/08/07 16:31:56 ru Exp $
# $DragonFly: src/sys/emulation/ibcs2/Attic/Makefile,v 1.4 2004/08/13 17:51:08 dillon Exp $

.PATH:	${.CURDIR}/../../emulation/ibcs2/i386
KMOD=	ibcs2
SRCS=	ibcs2_errno.c ibcs2_ipc.c ibcs2_stat.c ibcs2_misc.c \
	ibcs2_fcntl.c ibcs2_signal.c ibcs2_sysent.c ibcs2_ioctl.c \
	ibcs2_socksys.c ibcs2_util.c ibcs2_xenix.c ibcs2_xenix_sysent.c \
	ibcs2_isc.c ibcs2_isc_sysent.c \
	ibcs2_msg.c ibcs2_other.c ibcs2_sysi86.c ibcs2_sysvec.c \
	opt_spx_hack.h
MAN=	ibcs2.8

CFLAGS+= -DCOMPAT_IBCS2
EXPORT_SYMS= _ibcs2_mod _ibcs2_emul_path _ibcs2_svr3_sysvec

afterinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/ibcs2.sh ${DESTDIR}/usr/sbin/ibcs2

.include <bsd.kmod.mk>
