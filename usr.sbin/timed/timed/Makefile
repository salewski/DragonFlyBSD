#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/timed/timed/Makefile,v 1.2.14.1 2001/04/25 12:11:05 ru Exp $
# $DragonFly: src/usr.sbin/timed/timed/Makefile,v 1.4 2004/09/05 02:20:15 dillon Exp $
#
# optional flags are: MEASURE TESTING DEBUG

PROG=	timed
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	timed.8
WARNS?= 3

.include "../../Makefile.inc"
.include <bsd.prog.mk>
