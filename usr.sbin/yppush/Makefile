# $FreeBSD: src/usr.sbin/yppush/Makefile,v 1.6.2.1 2001/04/25 12:11:14 ru Exp $
# $DragonFly: src/usr.sbin/yppush/Makefile,v 1.2 2003/06/17 04:30:04 dillon Exp $

PROG=	yppush
SRCS=	ypxfr_getmap.c yp_dblookup.c yp_error.c ypxfr_misc.c yppush_main.c \
	${GENSRCS}
GENSRCS=yp.h yp_clnt.c yppush_svc.c

MAN=	yppush.8

CFLAGS+=-I. -I${.CURDIR}/../../libexec/ypxfr

RPCDIR=	${.CURDIR}/../../include/rpcsvc
RPCGEN= rpcgen -C

.PATH:	${RPCDIR} ${.CURDIR}/../../usr.sbin/ypserv \
	${.CURDIR}/../../libexec/ypxfr

CLEANFILES= ${GENSRCS}

yppush_svc.c: yp.x
	${RPCGEN} -DYPPUSH_ONLY -m -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: yp.x
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: yp.x
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

.include <bsd.prog.mk>
