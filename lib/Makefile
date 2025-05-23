#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/Makefile,v 1.107.2.16 2002/10/10 19:24:35 kbyanc Exp $
# $DragonFly: src/lib/Makefile,v 1.17 2005/03/13 15:10:02 swildner Exp $

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libcom_err must be built before libkrb and libpam.
# libcrypt must be built before libkrb, libpam and libskey.
# msun must be built before libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, libskey, and
# libtacplus.
# libncurses must be built before libdialog, libedit and libreadline.
# libradius must be built before libpam.
# libskey must be built before libpam.
# libtacplus must be built before libpam.
# libutil must be built before libpam.
# libsbuf must be built before libcam.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} csu/common libarchive libcom_err libcrypt msun libmd \
	libncurses libradius libskey libtacplus libutil libsbuf ${_compat} \
	libalias libatm ${_libbind} libbz2 libc ${_libc_r} libcalendar \
	libcam libcompat libdevstat libdisk libedit libfetch libftpio \
	libipsec libipx libisc libkcore libkinfo libkvm libmagic \
	${_libmilter} ${_libncp} libnetgraph libopie libpam \
	libpcap libposix1e librpcsvc ${_libsm} ${_libsmb} ${_libsmdb} \
	${_libsmutil} libstand libtelnet libthread_xu libusbhid ${_libvgl} \
	libwrap libxpg4 liby libz

_csu=csu/${MACHINE_ARCH}

.if ${MACHINE_ARCH} == "i386"
.if !defined(NOLIBC_R)
_libc_r=	libc_r
.endif
.endif

.if !defined(NO_BIND)
_libbind=	libbind
.endif

.if !defined(NO_SENDMAIL)
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if ${MACHINE_ARCH} == "i386"
_compat=	compat
_libncp=	libncp
_libsmb=	libsmb
_libvgl=	libvgl
.endif

.include <bsd.subdir.mk>
