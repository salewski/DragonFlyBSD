#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/Makefile.inc,v 1.10.2.2 2002/02/18 16:02:39 ru Exp $
# $DragonFly: src/games/Makefile.inc,v 1.2 2003/06/17 04:25:21 dillon Exp $

BINDIR?=	/usr/games
FILESDIR?=	${SHAREDIR}/games

.if defined(HIDEGAME)
ORIGBINDIR:=	${BINDIR}
BINDIR:=	${BINDIR}/hide
ORIGBINGRP:=	${BINGRP}
BINGRP=		games
BINMODE=	550
.endif
