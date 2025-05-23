# From:	@(#)Makefile	8.3 (Berkeley) 4/2/94
# $FreeBSD: src/usr.bin/passwd/Makefile,v 1.40.2.3 2002/07/17 19:08:22 ru Exp $
# $DragonFly: src/usr.bin/passwd/Makefile,v 1.4 2004/11/17 19:16:05 dillon Exp $

# NOPAM is used by PicoBSD

.if defined(NOPAM)

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_scan.c pw_util.c 

GENSRCS=yp.h yp_clnt.c yppasswd.h yppasswd_clnt.c \
	yppasswd_private.h yppasswd_private_clnt.c yppasswd_private_xdr.c
CFLAGS+=-Wall

DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/pwd_mkdb \
	${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin 

CFLAGS+= -DLOGIN_CAP -DCRYPT -I. -I${.CURDIR} \
	-I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass \
	-I${.CURDIR}/../../usr.sbin/pwd_mkdb \
	-Dyp_error=warnx -DLOGGING

.else

PROG=	passwd
SRCS=	local_passwd.c passwd.c pw_copy.c pw_scan.c pw_util.c pw_yp.c \
	yp_passwd.c ypxfr_misc.c ${GENSRCS}
GENSRCS=yp.h yp_clnt.c yppasswd.h yppasswd_clnt.c \
	yppasswd_private.h yppasswd_private_clnt.c yppasswd_private_xdr.c
CFLAGS+=-Wall

DPADD=	${LIBCRYPT} ${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lcrypt -lrpcsvc -lutil
.PATH:	${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/pwd_mkdb \
	${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../libexec/ypxfr \
	${.CURDIR}/../../usr.sbin/rpc.yppasswdd

CFLAGS+= -DLOGIN_CAP -DCRYPT -DYP -I. -I${.CURDIR} \
	-I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass \
	-I${.CURDIR}/../../libexec/ypxfr \
	-I${.CURDIR}/../../usr.sbin/pwd_mkdb \
	-I${.CURDIR}/../../usr.sbin/rpc.yppasswdd \
	-Dyp_error=warnx -DLOGGING

.endif

CLEANFILES= ${GENSRCS}

RPCGEN= rpcgen -C
RPCSRC= ${DESTDIR}/usr/include/rpcsvc/yp.x
RPCSRC_PW= ${DESTDIR}/usr/include/rpcsvc/yppasswd.x
RPCSRC_PRIV= ${.CURDIR}/../../usr.sbin/rpc.yppasswdd/yppasswd_private.x

yp.h: ${RPCSRC}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC}

yp_clnt.c: ${RPCSRC} yp.h
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC}

yppasswd.h: ${RPCSRC_PW}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PW}

yppasswd_clnt.c: ${RPCSRC_PW}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PW}

yppasswd_private.h: ${RPCSRC_PRIV}
	${RPCGEN} -h -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_xdr.c: ${RPCSRC_PRIV}
	${RPCGEN} -c -o ${.TARGET} ${RPCSRC_PRIV}

yppasswd_private_clnt.c: ${RPCSRC_PRIV}
	${RPCGEN} -l -o ${.TARGET} ${RPCSRC_PRIV}

BINMODE=4555
LINKS=${BINDIR}/passwd ${BINDIR}/yppasswd
MLINKS=passwd.1 yppasswd.1

beforeinstall:
.for i in passwd yppasswd
	[ ! -e ${DESTDIR}${BINDIR}/$i ] || \
		chflags noschg ${DESTDIR}${BINDIR}/$i || true
.endfor

.if !defined(NOFSCHG)
afterinstall:
	-chflags schg ${DESTDIR}${BINDIR}/passwd
.endif

.include <bsd.prog.mk>
