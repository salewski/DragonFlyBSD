#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD: src/games/fortune/datfiles/Makefile,v 1.24.2.3 2002/02/18 15:03:32 ru Exp $
# $DragonFly: src/games/fortune/datfiles/Makefile,v 1.3 2004/01/31 06:56:36 dillon Exp $

FILES=	fortunes fortunes2 freebsd-tips murphy startrek zippy
BLDS=	fortunes.dat fortunes2.dat murphy.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat freebsd-tips.dat

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# THREE LINES AND UNCOMMENT THE FOURTH LINE.

# THE THREE LINES:
FILES+=	fortunes2-o limerick murphy-o
BLDS+=	fortunes2-o.dat limerick.dat murphy-o.dat
TYPE=	real

# THE FOURTH LINE:
#TYPE=	fake

FILES+=	${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in fortunes fortunes2 fortunes2-o freebsd-tips limerick murphy murphy-o startrek zippy
$f.dat: $f
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Crs ${.ALLSRC} ${.TARGET}
.endfor

fortunes-o.dat: fortunes-o
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Crsx ${.ALLSRC} ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	tr a-zA-Z n-za-mN-ZA-M < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
