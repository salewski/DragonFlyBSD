#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD: src/sbin/mount_ext2fs/Makefile,v 1.3.6.1 2001/04/25 10:58:34 ru Exp $
# $DragonFly: src/sbin/mount_ext2fs/Makefile,v 1.3 2005/04/03 15:46:43 joerg Exp $

PROG=	mount_ext2fs
SRCS=	mount_ext2fs.c getmntopts.c
MAN=	mount_ext2fs.8
WARNS?=	6

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
