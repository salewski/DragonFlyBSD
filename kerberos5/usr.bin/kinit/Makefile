# $FreeBSD: src/kerberos5/usr.bin/kinit/Makefile,v 1.15 2004/02/05 18:51:51 ru Exp $
# $DragonFly: src/kerberos5/usr.bin/kinit/Makefile,v 1.1 2005/01/16 14:25:47 eirikn Exp $

PROG=	kinit
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
