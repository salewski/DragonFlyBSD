#	$NetBSD: Makefile,v 1.6 1997/05/08 21:11:11 gwr Exp $
# $FreeBSD: src/sbin/fsck_msdosfs/Makefile,v 1.2.2.1 2001/08/01 05:47:55 obrien Exp $
# $DragonFly: src/sbin/fsck_msdosfs/Makefile,v 1.2 2003/06/17 04:27:32 dillon Exp $

FSCK=	${.CURDIR}/../fsck
.PATH: ${FSCK}

PROG=	fsck_msdosfs
MAN=	fsck_msdosfs.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c

CFLAGS+= -I${FSCK}

.include <bsd.prog.mk>
