#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
# $FreeBSD: src/usr.sbin/bootparamd/bootparamd/Makefile,v 1.11.2.1 2001/04/25 12:09:20 ru Exp $
# $DragonFly: src/usr.sbin/bootparamd/bootparamd/Makefile,v 1.2 2003/06/17 04:29:52 dillon Exp $

PROG=	bootparamd
SRCS=	bootparamd.c main.c ${GENSRCS}
GENSRCS=bootparam_prot.h bootparam_prot_svc.c bootparam_prot_xdr.c
MAN=	bootparams.5 bootparamd.8
CFLAGS+= -DTFTP_DIR=\"/tftpboot\" -I.
CLEANFILES= ${GENSRCS}

RPCSRC=	${DESTDIR}/usr/include/rpcsvc/bootparam_prot.x

bootparam_prot_svc.c: ${RPCSRC}
	rpcgen -m -o ${.TARGET} ${RPCSRC}

bootparam_prot_xdr.c: ${RPCSRC}
	rpcgen -c -o ${.TARGET} ${RPCSRC}

bootparam_prot.h: ${RPCSRC}
	rpcgen -h -o ${.TARGET} ${RPCSRC}

.include <bsd.prog.mk>
