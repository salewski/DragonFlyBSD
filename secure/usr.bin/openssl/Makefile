# $FreeBSD: src/secure/usr.bin/openssl/Makefile,v 1.11.2.6 2003/02/14 22:38:22 nectar Exp $
# $DragonFly: src/secure/usr.bin/openssl/Makefile,v 1.5 2004/12/18 20:53:19 asmodai Exp $

PROG=	xopenssl
PROGNAME=	openssl

DPADD=	${LIBSSL} ${LIBCRYPTO}
LDADD=	-lssl -lcrypto

NOLINT=		true

.if exists(Makefile.man)
.include "Makefile.man"
.endif
.include "../../lib/libcrypto/Makefile.inc"

CFLAGS+= -D__FreeBSD__
CFLAGS+= -DMONOLITH -I${.CURDIR}

SRCS+=	app_rand.c apps.c asn1pars.c ca.c ciphers.c crl.c crl2p7.c \
	dgst.c dh.c dhparam.c dsa.c dsaparam.c enc.c engine.c errstr.c \
	gendh.c gendsa.c genrsa.c nseq.c ocsp.c openssl.c passwd.c \
	pkcs12.c pkcs7.c pkcs8.c prime.c rand.c req.c rsa.c rsautl.c s_cb.c \
	s_client.c s_server.c s_socket.c s_time.c sess_id.c smime.c \
	speed.c spkac.c verify.c version.c x509.c

.include <bsd.prog.mk>

.PATH:	${LCRYPTO_SRC}/apps \
	${.CURDIR}/man
