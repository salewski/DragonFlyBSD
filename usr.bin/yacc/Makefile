#	@(#)Makefile	5.3 (Berkeley) 5/12/90
# $FreeBSD: src/usr.bin/yacc/Makefile,v 1.8.2.2 2001/04/25 14:04:46 ru Exp $
# $DragonFly: src/usr.bin/yacc/Makefile,v 1.4 2005/01/05 15:26:05 joerg Exp $

PROG=	yacc
SRCS=	closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	skeleton.c symtab.c verbose.c warshall.c
SCRIPTS=yyfix.sh
MAN=	yacc.1 yyfix.1
LINKS=	${BINDIR}/yacc ${BINDIR}/byacc
MLINKS=	yacc.1 byacc.1 \
	yacc.1 liby.3
WARNS?=	6

.include <bsd.prog.mk>
