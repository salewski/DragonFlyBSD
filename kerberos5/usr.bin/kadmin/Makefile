# $FreeBSD: src/kerberos5/usr.bin/kadmin/Makefile,v 1.18 2004/02/05 18:51:51 ru Exp $
# $DragonFly: src/kerberos5/usr.bin/kadmin/Makefile,v 1.1 2005/01/16 14:25:47 eirikn Exp $

PROG=	kadmin
MAN=	kadmin.8

SRCS=	ank.c \
	cpw.c \
	del.c \
	del_enctype.c \
	dump.c \
	ext.c \
	get.c \
	init.c \
	kadmin.c \
	load.c \
	mod.c \
	random_password.c \
	rename.c \
	util.c

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/sl ${LDAPCFLAGS}
DPADD=	${LIBKADM5CLNT} ${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} \
	${LIBSL} ${LIBROKEN} ${LIBVERS} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} \
	${LIBREADLINE} ${LIBNCURSES} ${LDAPDPADD}
LDADD=	-lkadm5clnt -lkadm5srv -lhdb -lkrb5 \
	${LIBSL} -lroken ${LIBVERS} -lasn1 \
	-lcrypto -lcrypt -lcom_err \
	-lreadline -lncurses ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
