#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/share/man/man8/Makefile,v 1.13.2.12 2002/07/16 14:05:30 ru Exp $
# $DragonFly: src/share/man/man8/Makefile,v 1.5 2005/02/26 09:46:31 asmodai Exp $

MAN=	adding_user.8 \
	crash.8 \
	diskless.8 \
	intro.8 \
	picobsd.8 \
	rc.8 \
	rc.sendmail.8 \
	rc.subr.8 \
	sticky.8 \
	yp.8

MLINKS=	rc.8 rc.early.8 rc.8 rc.serial.8 rc.8 rc.pccard.8 rc.8 rc.network.8
MLINKS+=rc.8 rc.firewall.8 rc.8 rc.atm.8 rc.8 rc.local.8 rc.8 rc.shutdown.8
MLINKS+=yp.8 YP.8 yp.8 NIS.8 yp.8 nis.8

.if ${MACHINE_ARCH} == "i386"
MLINKS+=rc.8 ${MACHINE_ARCH}/rc.${MACHINE_ARCH}.8
SUBDIR=	man8.${MACHINE_ARCH}
.endif

.include <bsd.prog.mk>
