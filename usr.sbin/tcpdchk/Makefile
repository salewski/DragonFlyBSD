#
# $FreeBSD: src/usr.sbin/tcpdchk/Makefile,v 1.3.2.2 2001/04/25 12:11:00 ru Exp $
# $DragonFly: src/usr.sbin/tcpdchk/Makefile,v 1.3 2004/01/31 06:56:46 dillon Exp $
#

PROG=	tcpdchk
MAN=	tcpdchk.8
SRCS=	tcpdchk.c fakelog.c inetcf.c scaffold.c

CFLAGS=	-DREAL_DAEMON_DIR=\"/usr/libexec\" \
	-DSEVERITY=LOG_INFO -DRFC931_TIMEOUT=10 \
	-DHOSTS_DENY=\"/etc/hosts.deny\" -DHOSTS_ALLOW=\"/etc/hosts.allow\"
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

DPADD=	${LIBWRAP}
LDADD=	-lwrap

.PATH:	${.CURDIR}/../../contrib/tcp_wrappers

.include <bsd.prog.mk>
