#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_umap/Makefile,v 1.6.6.1 2001/04/25 10:58:40 ru Exp $
# $DragonFly: src/sbin/mount_umap/Attic/Makefile,v 1.2 2003/06/17 04:27:33 dillon Exp $

PROG=	mount_umap
SRCS=	mount_umap.c getmntopts.c
MAN=	mount_umap.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
