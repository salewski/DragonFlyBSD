# Make `routed` tools for BSD/OS
# $Revision: 1.6 $
# $FreeBSD: src/sbin/routed/rtquery/Makefile,v 1.7.2.1 2001/04/25 10:58:50 ru Exp $
# $DragonFly: src/sbin/routed/rtquery/Makefile,v 1.3 2005/01/17 07:27:45 cpressey Exp $

PROG=	rtquery
WARNS?=	6
MAN=	rtquery.8
LDADD+=	-lmd
DPADD+=	${LIBMD}
#COPTS=	-g -DDEBUG -W -Wall -Wcast-align -Wcast-qual -Winline -Wpointer-arith -Wnested-externs -Wwrite-strings -Wunused

.include <bsd.prog.mk>
