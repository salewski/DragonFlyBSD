#	from: @(#)Makefile    5.10 (Berkeley) 6/24/90
# $FreeBSD: src/lib/librpcsvc/Makefile,v 1.13 1999/08/28 00:05:23 peter Exp $
# $DragonFly: src/lib/librpcsvc/Makefile,v 1.2 2003/06/17 04:26:51 dillon Exp $

.PATH: ${DESTDIR}/usr/include/rpcsvc

LIB=    rpcsvc

RPCSRCS= klm_prot.x mount.x nfs_prot.x nlm_prot.x rex.x rnusers.x \
	rquota.x rstat.x rwall.x sm_inter.x spray.x yppasswd.x ypxfrd.x \
	ypupdate_prot.x

OTHERSRCS=  rnusers.c rstat.c rwall.c yp_passwd.c yp_update.c
SECRPCSRCS= secretkey.c xcrypt.c

RPCCOM = rpcgen -C

INCDIRS= -I${DESTDIR}/usr/include/rpcsvc -I${DESTDIR}/usr/include

CFLAGS+= -DYP ${INCDIRS}

GENSRCS= ${RPCSRCS:R:S/$/_xdr.c/g}
SRCS+= ${GENSRCS} ${OTHERSRCS} ${SECRPCSRCS}

CLEANFILES+= ${GENSRCS}

NOMAN= noman

.include <bsd.lib.mk>

.SUFFIXES: .x _xdr.c

.x_xdr.c:
	${RPCCOM} -c ${.IMPSRC} -o ${.TARGET}

OBJS=	${RPCSRCS:R:S/$/_xdr.o/g} ${SECRPCSRCS:R:S/$/.o/g} \
	${OTHERSRCS:R:S/$/.o/g}
