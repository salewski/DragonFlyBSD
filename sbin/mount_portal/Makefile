#	From: @(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_portal/Makefile,v 1.10.2.1 2001/04/25 10:58:39 ru Exp $
# $DragonFly: src/sbin/mount_portal/Makefile,v 1.2 2003/06/17 04:27:33 dillon Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c pt_tcplisten.c
MAN=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
