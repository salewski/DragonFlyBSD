# $FreeBSD: src/usr.sbin/mailwrapper/Makefile,v 1.4.2.5 2003/04/16 11:01:51 ru Exp $
# $DragonFly: src/usr.sbin/mailwrapper/Makefile,v 1.3 2005/03/02 07:36:32 cpressey Exp $

.if !defined(NO_MAILWRAPPER)
PROG=	mailwrapper
WARNS?=	6
MAN=	mailwrapper.8

DPADD=	${LIBUTIL}
LDADD=	-lutil
.endif

SYMLINKS= 	${BINDIR}/mailwrapper /usr/sbin/sendmail  \
		${BINDIR}/mailwrapper /usr/sbin/hoststat  \
		${BINDIR}/mailwrapper /usr/sbin/purgestat \
		${BINDIR}/mailwrapper /usr/bin/newaliases \
		${BINDIR}/mailwrapper /usr/bin/mailq

.if defined(NO_MAILWRAPPER) && !defined(NO_SENDMAIL)
SYMLINKS+=	/usr/libexec/sendmail/sendmail ${BINDIR}/mailwrapper
.endif

afterinstall:
.if !defined(NO_MAILWRAPPER)
.if !exists(${DESTDIR}/etc/mail/mailer.conf)
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/../../etc/mail/mailer.conf ${DESTDIR}/etc/mail
.endif
.endif

.include <bsd.prog.mk>
