#	$NetBSD: Makefile,v 1.1.1.1 1995/10/08 23:08:36 thorpej Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/fsdb/Makefile,v 1.9.2.1 2001/04/25 10:58:17 ru Exp $
# $DragonFly: src/sbin/fsdb/Makefile,v 1.3 2003/08/08 04:18:38 dillon Exp $

PROG=	fsdb
MAN=	fsdb.8
SRCS=	fsdb.c fsdbutil.c \
	dir.c inode.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ffs_subr.c ffs_tables.c
CFLAGS+= -I${.CURDIR}/../fsck
LDADD+= -ledit -ltermcap
DPADD+=	${LIBEDIT} ${LIBTERMCAP}
.PATH:	${.CURDIR}/../fsck ${.CURDIR}/../../sys/vfs/ufs

.include <bsd.prog.mk>
