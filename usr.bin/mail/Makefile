#	@(#)Makefile	8.2 (Berkeley) 1/25/94
# $FreeBSD: src/usr.bin/mail/Makefile,v 1.5.6.4 2002/08/07 16:32:01 ru Exp $
# $DragonFly: src/usr.bin/mail/Makefile,v 1.2 2003/06/17 04:29:28 dillon Exp $

PROG=	mail
SRCS=	version.c aux.c cmd1.c cmd2.c cmd3.c cmdtab.c collect.c edit.c fio.c \
	getname.c head.c v7.local.c lex.c list.c main.c names.c popen.c \
	quit.c send.c strings.c temp.c tty.c vars.c
FILES=	mail.help mail.tildehelp
FILESDIR=	${SHAREDIR}/misc
EFILES=	mail.rc
LINKS=	${BINDIR}/mail ${BINDIR}/Mail ${BINDIR}/mail ${BINDIR}/mailx
MLINKS=	mail.1 Mail.1 mail.1 mailx.1

.PATH: ${.CURDIR}/misc

etc-mailrc:
	cd ${.CURDIR}/misc; ${INSTALL} -o root -g wheel \
	    -m 644 ${EFILES} ${DESTDIR}/etc

.include <bsd.prog.mk>
