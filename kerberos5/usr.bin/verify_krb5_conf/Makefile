# $FreeBSD: src/kerberos5/usr.bin/verify_krb5_conf/Makefile,v 1.3 2004/02/05 18:51:52 ru Exp $
# $DragonFly: src/kerberos5/usr.bin/verify_krb5_conf/Makefile,v 1.1 2005/01/16 14:25:47 eirikn Exp $

PROG=	verify_krb5_conf
MAN=	verify_krb5_conf.8
CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/krb5
