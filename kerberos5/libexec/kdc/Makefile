# $FreeBSD: src/kerberos5/libexec/kdc/Makefile,v 1.17 2004/02/05 18:51:51 ru Exp $
# $DragonFly: src/kerberos5/libexec/kdc/Makefile,v 1.5 2005/01/16 14:25:46 eirikn Exp $

PROG=	kdc
MAN=	kdc.8

SRCS=	524.c \
	config.c \
	connect.c \
	kerberos4.c \
	kerberos5.c \
	log.c \
	main.c \
	misc.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	${LDAPCFLAGS}
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
