#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/find/Makefile,v 1.17 2002/04/15 19:27:41 obrien Exp $
# $DragonFly: src/usr.bin/find/Makefile,v 1.4 2005/02/14 00:39:04 cpressey Exp $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
WARNS?=	6
CFLAGS+=-DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
