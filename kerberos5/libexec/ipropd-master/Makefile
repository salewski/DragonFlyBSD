# $FreeBSD: src/kerberos5/libexec/ipropd-master/Makefile,v 1.17 2004/12/21 08:47:01 ru Exp $
# $DragonFly: src/kerberos5/libexec/ipropd-master/Makefile,v 1.5 2005/01/16 14:25:46 eirikn Exp $

PROG=	ipropd-master
NOMAN=
SRCS=	ipropd_master.c kadm5_err.h
CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I. ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/kadm5
