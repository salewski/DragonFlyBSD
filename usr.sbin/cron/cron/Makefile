# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.12.2.1 2001/04/25 12:09:23 ru Exp $
# $DragonFly: src/usr.sbin/cron/cron/Makefile,v 1.2 2003/06/17 04:29:53 dillon Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -lutil
MAN=	cron.8

.include <bsd.prog.mk>
