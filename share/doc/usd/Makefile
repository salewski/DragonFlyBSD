#	From: @(#)Makefile	8.2 (Berkeley) 4/20/94
# $FreeBSD: src/share/doc/usd/Makefile,v 1.8.2.2 2002/07/24 12:48:41 ru Exp $
# $DragonFly: src/share/doc/usd/Makefile,v 1.3 2004/09/20 22:03:16 dillon Exp $

# The following modules are encumbered:
# 01.begin 02.learn 03.shell 09.edtut 10.edadv 15.sed 16.awk 17.msmacros
# 21.troff 22.trofftut 23.eqn 24.eqnguide 25.tbl 26.refer 27.invert
# 28.bib 29.diction

# The following modules are not provided:
# 14.jove

SUBDIR=	title contents 04.csh 05.dc 06.bc 07.mail 10.exref 11.vitut 12.vi \
	13.viref 18.msdiffs 19.memacros 20.meref

.if exists(${.CURDIR}/../../../games) && !defined(NOGAMES)
SUBDIR+=30.rogue 31.trek
.endif

.include <bsd.subdir.mk>
