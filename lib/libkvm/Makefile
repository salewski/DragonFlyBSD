#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libkvm/Makefile,v 1.9.2.1 2001/04/25 10:04:32 ru Exp $
# $DragonFly: src/lib/libkvm/Makefile,v 1.3 2004/04/02 05:46:02 hmp Exp $

LIB=	kvm
CFLAGS+=-DLIBC_SCCS -I${.CURDIR}/../../sys
SRCS=	kvm.c kvm_${MACHINE_ARCH}.c kvm_file.c kvm_getloadavg.c \
	kvm_getswapinfo.c kvm_proc.c kvm_util.c
INCS=	kvm.h

MAN=	kvm.3 kvm_geterr.3 kvm_getfiles.3 kvm_getloadavg.3 kvm_getprocs.3 \
	kvm_getswapinfo.3 kvm_nlist.3 kvm_open.3 kvm_read.3

MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
MLINKS+=kvm_open.3 kvm_close.3 kvm_open.3 kvm_openfiles.3
MLINKS+=kvm_read.3 kvm_write.3

.include <bsd.lib.mk>
