#	@(#)Makefile	8.2 (Berkeley) 9/21/96
# $FreeBSD: src/usr.sbin/praliases/Makefile,v 1.1.6.6 2002/03/25 21:31:46 gshapiro Exp $
# $DragonFly: src/usr.sbin/praliases/Makefile,v 1.3 2004/01/31 06:56:46 dillon Exp $

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/praliases

PROG=	praliases
SRCS=	praliases.c
MAN=	praliases.8
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNOT_SENDMAIL

.if exists(${.OBJDIR}/../../lib/libsm)
LIBSMDIR:=	${.OBJDIR}/../../lib/libsm
.else
LIBSMDIR!=	cd ${.CURDIR}/../../lib/libsm; make -V .OBJDIR
.endif
LIBSM:=		${LIBSMDIR}/libsm.a

.if exists(${.OBJDIR}/../../lib/libsmdb)
LIBSMDBDIR:=	${.OBJDIR}/../../lib/libsmdb
.else
LIBSMDBDIR!=	cd ${.CURDIR}/../../lib/libsmdb; make -V .OBJDIR
.endif
LIBSMDB:=	${LIBSMDBDIR}/libsmdb.a

.if exists(${.OBJDIR}/../../lib/libsmutil)
LIBSMUTILDIR:=	${.OBJDIR}/../../lib/libsmutil
.else
LIBSMUTILDIR!=	cd ${.CURDIR}/../../lib/libsmutil; make -V .OBJDIR
.endif
LIBSMUTIL:=	${LIBSMUTILDIR}/libsmutil.a

DPADD=	${LIBSMDB} ${LIBSMUTIL} ${LIBSM}
LDADD=	${LIBSMDB} ${LIBSMUTIL} ${LIBSM}

SRCS+=	sm_os.h
CLEANFILES+=sm_os.h

# User customizations to the sendmail build environment
CFLAGS+=${SENDMAIL_CFLAGS}
DPADD+=${SENDMAIL_DPADD}
LDADD+=${SENDMAIL_LDADD}
LDFLAGS+=${SENDMAIL_LDFLAGS}

sm_os.h:
	ln -sf ${SENDMAIL_DIR}/include/sm/os/sm_os_freebsd.h sm_os.h

.include <bsd.prog.mk>
