# $FreeBSD: src/sys/modules/syscons/dragon/Makefile,v 1.3.4.1 2003/05/11 01:17:02 murray Exp $
# $DragonFly: src/sys/dev/misc/syscons/dragon/Makefile,v 1.2 2003/06/17 04:28:47 dillon Exp $

KMOD=	dragon_saver
SRCS= 	dragon_saver.c

NOMAN=
CFLAGS+= -I${.CURDIR}/..
CWARNFLAGS= -Wall -pedantic

.include <bsd.kmod.mk>
