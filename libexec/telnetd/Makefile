# $FreeBSD: src/libexec/telnetd/Makefile,v 1.13.2.2 2002/04/13 11:03:30 markm Exp $
# $DragonFly: src/libexec/telnetd/Makefile,v 1.2 2003/06/17 04:27:08 dillon Exp $

# Do not define -DKLUDGELINEMODE, as it does not interact well with many
# telnet implementations.

PROG=		telnetd
MAN=		telnetd.8

SRCS=		global.c slc.c state.c sys_term.c telnetd.c \
		termstat.c utility.c

CFLAGS+=	-DLINEMODE -DUSE_TERMIO -DDIAGNOSTICS -DOLD_ENVIRON \
		-DENV_HACK \
		-I${.CURDIR}/../../lib -DINET6

WARNS?=		2
WFORMAT?=	0

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=		${LIBUTIL} ${LIBTERMCAP} ${LIBTELNET}
LDADD=		-lutil -ltermcap ${LIBTELNET}

unifdef:
	-cd ${.CURDIR} ; \
	( for i in *.[ch] ; do \
		unifdef -UAUTHENTICATION -UENCRYPTION \
			${.CURDIR}/../../crypto/telnet/telnetd/$$i \
				> ${.CURDIR}/$$i ; \
		echo $$i ; \
	done )

.include <bsd.prog.mk>
