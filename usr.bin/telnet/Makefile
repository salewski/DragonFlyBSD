# $FreeBSD: src/usr.bin/telnet/Makefile,v 1.14.2.2 2002/04/13 11:03:30 markm Exp $
# $DragonFly: src/usr.bin/telnet/Makefile,v 1.2 2003/06/17 04:29:32 dillon Exp $

PROG=		telnet

SRCS=		commands.c main.c network.c ring.c sys_bsd.c \
		telnet.c terminal.c utilities.c

CFLAGS+=	-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK -DOPIE \
		-I${.CURDIR}/../../lib -I${.CURDIR}/../../lib/libtelnet/

.if exists(${.OBJDIR}/../../lib/libtelnet)
LIBTELNET=	${.OBJDIR}/../../lib/libtelnet/libtelnet.a
.else
LIBTELNET=	${.CURDIR}/../../lib/libtelnet/libtelnet.a
.endif

DPADD=		${LIBTERMCAP} ${LIBTELNET}
LDADD=		-ltermcap ${LIBTELNET}

.if !defined(RELEASE_CRUNCH)
CFLAGS+=	-DINET6 -DIPSEC
DPADD+=		${LIBIPSEC}
LDADD+=		-lipsec
.else
.PATH:		${.CURDIR}/../../lib/libtelnet
# XXX This was copied from lib/libtelnet/Makefile.
SRCS+=		genget.c getent.c misc.c
CFLAGS+=	-DHAS_CGETENT
.endif

unifdef:
	-cd ${.CURDIR} ; \
	( for i in *.[ch] ; do \
		unifdef -UAUTHENTICATION -UENCRYPTION \
			${.CURDIR}/../../crypto/telnet/telnet/$$i \
				> ${.CURDIR}/$$i ; \
		echo $$i ; \
	done )

.include <bsd.prog.mk>
