# $FreeBSD: src/gnu/lib/libreadline/readline/Makefile,v 1.5.2.3 2002/07/23 10:20:20 ru Exp $
# $DragonFly: src/gnu/lib/libreadline/readline/Attic/Makefile,v 1.3 2004/10/23 16:58:53 joerg Exp $

SUBDIR=	doc

LIB=	readline
MAN=	doc/readline.3

SRCS=	readline.c vi_mode.c funmap.c keymaps.c parens.c search.c \
	rltty.c complete.c bind.c isearch.c display.c signals.c \
	util.c kill.c undo.c macro.c input.c callback.c terminal.c \
	text.c nls.c misc.c compat.c tilde.c ${HISTSRC}

HEADERS=	readline.h chardefs.h keymaps.h history.h tilde.h \
		rlstdc.h rlconf.h rltypedefs.h

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

INCSDIR=${INCLUDEDIR}/readline
INCS=	${HEADERS:S,^,${SRCDIR}/,}

.include <bsd.lib.mk>
