# $FreeBSD: src/sys/modules/crypto/Makefile,v 1.1.2.1 2002/11/21 23:38:46 sam Exp $
# $DragonFly: src/sys/crypto/Makefile,v 1.4 2004/07/23 11:54:14 joerg Exp $

.PATH:	${.CURDIR}/../opencrypto
.PATH:	${.CURDIR}/../crypto
.PATH:	${.CURDIR}/../crypto/blowfish
.PATH:	${.CURDIR}/../crypto/des
.PATH:	${.CURDIR}/../crypto/rc4
.PATH:	${.CURDIR}/../crypto/sha2
.PATH:	${.CURDIR}/../net

KMOD	= crypto
SRCS	= crypto.c
SRCS	+= criov.c crmbuf.c cryptosoft.c xform.c
SRCS	+= cast.c deflate.c rmd160.c rijndael.c skipjack.c
SRCS	+= bf_enc.c bf_skey.c
SRCS	+= des_ecb.c des_enc.c des_setkey.c
SRCS	+= rc4.c
SRCS	+= sha1.c sha2.c
SRCS	+= zlib.c
SRCS	+= opt_param.h

.include <bsd.kmod.mk>
