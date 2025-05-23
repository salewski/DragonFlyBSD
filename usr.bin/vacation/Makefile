# $FreeBSD: src/usr.bin/vacation/Makefile,v 1.6.2.3 2002/03/25 21:31:50 gshapiro Exp $
# $DragonFly: src/usr.bin/vacation/Makefile,v 1.4 2004/02/13 03:53:37 dillon Exp $

SENDMAIL_DIR=${.CURDIR}/../../contrib/sendmail
.PATH:	${SENDMAIL_DIR}/vacation

PROG=	vacation
SRCS=	vacation.c
CFLAGS+=-I${SENDMAIL_DIR}/src -I${SENDMAIL_DIR}/include -I.
CFLAGS+=-DNEWDB -DNOT_SENDMAIL
CFLAGS+=-D_FFR_LISTDB -D_FFR_DEBUG

# Fake __FreeBSD__ so sendmail uses a reasonable feature set.  osreldate.h
# will generate a __FreeBSD_version for us.
CFLAGS+=-D__FreeBSD__

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
