# $FreeBSD: src/kerberos5/libexec/kadmind/Makefile,v 1.18 2004/02/05 18:51:50 ru Exp $
# $DragonFly: src/kerberos5/libexec/kadmind/Makefile,v 1.1 2005/01/16 14:25:46 eirikn Exp $

PROG=	kadmind
MAN=	kadmind.8
SRCS=	kadm_conn.c kadmind.c server.c
CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
