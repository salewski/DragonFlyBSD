# $FreeBSD: src/libexec/ypxfr/Makefile,v 1.8.2.1 2001/04/25 10:41:06 ru Exp $
# $DragonFly: src/libexec/ypxfr/Makefile,v 1.2 2003/06/17 04:27:08 dillon Exp $

PROG=	ypxfr
SRCS=	yp_dblookup.c yp_dbwrite.c yp_error.c \
	ypxfr_getmap.c ypxfr_main.c ypxfr_misc.c \
	ypxfrd_getmap.c \
	${GENSRCS}
GENSRCS=yp.h yp_clnt.c ypxfr_clnt.c

.PATH: ${.CURDIR}/../../usr.sbin/ypserv

MAN=	ypxfr.8

CFLAGS+= -I.
DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

CLEANFILES= ${GENSRCS}

RPCDIR= ${.CURDIR}/../../include/rpcsvc
RPCGEN= rpcgen -I -C

ypxfr_clnt.c: ${RPCDIR}/yp.x
	rm -f ${.TARGET}
	${RPCGEN} -DYPPUSH_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp_clnt.c: ${RPCDIR}/yp.x
	rm -f ${.TARGET}
	${RPCGEN} -DYPSERV_ONLY -l -o ${.TARGET} ${RPCDIR}/yp.x

yp.h: ${RPCDIR}/yp.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/yp.x

# ypxfrd_xdr.c: ${RPCDIR}/ypxfrd.x
#	rm -f ${.TARGET}
#	${RPCGEN} -c -o ${.TARGET} ${RPCDIR}/ypxfrd.x

ypxfrd.h: ${RPCDIR}/ypxfrd.x
	rm -f ${.TARGET}
	${RPCGEN} -h -o ${.TARGET} ${RPCDIR}/ypxfrd.x

.include <bsd.prog.mk>
