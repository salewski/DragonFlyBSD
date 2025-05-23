#	@(#)Makefile	8.2 (Berkeley) 1/4/94
# $FreeBSD: src/include/Makefile,v 1.109.2.27 2003/01/24 05:12:29 sam Exp $
# $DragonFly: src/include/Makefile,v 1.23 2005/02/11 23:20:58 joerg Exp $
#
# Doing a make install builds /usr/include
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.

CLEANFILES= osreldate.h version vers.c
SUBDIR= arpa protocols rpc rpcsvc
INCS=	a.out.h ar.h assert.h bitstring.h complex.h cpio.h ctype.h db.h \
	dirent.h disktab.h \
	dlfcn.h elf.h elf-hints.h err.h fnmatch.h fstab.h \
	fts.h getopt.h glob.h grp.h histedit.h ieeefp.h ifaddrs.h \
	iso646.h inttypes.h \
	langinfo.h libgen.h limits.h link.h locale.h malloc.h memory.h \
	mpool.h ndbm.h netdb.h nl_types.h nlist.h objformat.h \
	paths.h pthread.h pthread_np.h pwd.h \
	ranlib.h readpassphrase.h regex.h regexp.h resolv.h re_comp.h rmd160.h \
	rune.h runetype.h search.h setjmp.h sgtty.h \
	signal.h stab.h stdarg.h stdbool.h stddef.h stdint.h stdio.h stdlib.h \
	string.h stringlist.h strings.h struct.h sysexits.h tar.h time.h \
	timers.h ttyent.h unistd.h ulimit.h utime.h utmp.h vis.h wchar.h \
	wctype.h

MHDRS=	float.h floatingpoint.h varargs.h

# posix4/aio.h conflicts with dysons and isn't installed:
PHDRS=	mqueue.h sched.h semaphore.h # aio.h

# Only for default SHARED=copies case
SHDRS=	soundcard.h joystick.h

LHDRS=	aio.h errno.h fcntl.h linker_set.h poll.h syslog.h \
	termios.h ucontext.h

# directories which also contain header files that need to be copied.
# Other directories, like 'bus' and 'netproto', are created using mtree.
#
# XXX allow these directories to not contain header files.
LDIRS=	net netgraph netinet netinet6 sys vm

# Subdirectories containing header files to copy.  In symlink mode 
# the subdirectory will be symlinked.  Care must be taken to adjust
# LSYMSUBDIRS below to remove subdirectories whos parent directories.
# If you make a mistake, part of your source tree might get overwritten
# when buildworld is run.
#
LSUBDIRS=	bus/cam bus/cam/scsi \
	emulation/posix4 \
	emulation/linux \
	vfs/msdosfs vfs/nfs vfs/ntfs vfs/nwfs \
	vfs/smbfs vfs/udf vfs/ufs \
	net/vlan net/ipfw net/ip6fw net/dummynet net/sppp net/ip_mroute \
	net/bridge net/tun net/ppp net/ppp_layer net/sl net/pf net/altq \
	netgraph/UI netgraph/async netgraph/bpf netgraph/bridge \
	netgraph/cisco netgraph/echo netgraph/eiface netgraph/etf \
	netgraph/ether netgraph/fec netgraph/frame_relay netgraph/hole \
	netgraph/iface netgraph/ksocket netgraph/l2tp netgraph/lmi \
	netgraph/mppc netgraph/one2many netgraph/ppp \
	netgraph/pppoe netgraph/pptpgre netgraph/rfc1490 netgraph/socket \
	netgraph/tee netgraph/tty netgraph/vjc \
	bus/cam bus/usb bus/pccard bus/pci bus/isa \
	netproto/atalk netproto/atm netproto/ipsec netproto/ipx \
	netproto/key netproto/natm netproto/ncp netproto/ns netproto/smb \
	netproto/atm/ipatm netproto/atm/sigpvc netproto/atm/spans \
	netproto/atm/uni netproto/802_11

LSUBDIRS3= vfs/isofs/cd9660 net/i4b/include \
	dev/misc/lpt dev/netif/wi dev/video/bktr dev/video/meteor

# For SHARED=symlinks, bus/cam and netproto/atm are symlinks, so cam/scsi
# and netproto/atm/* are taken care of
LSYMSUBDIRS=	${LSUBDIRS:Nbus/cam/scsi:Nnetproto/atm/*:Nnet/*:Nnetgraph/*}
LSYMSUBDIRS3=	${LSUBDIRS3:Nnet/*}

# For obsolete headers which need to be removed
RMHEADERS=	machine/ansi.h sys/inttypes.h

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies'').  ``symlinks'' is
# probably only useful for developers and should be avoided if you do not
# wish to tie your /usr/include and /usr/src together.
#SHARED=	symlinks
SHARED?=	copies

INCS+=	osreldate.h

osreldate.h:	${.CURDIR}/../sys/conf/newvers.sh \
		${.CURDIR}/../sys/sys/param.h
	@${ECHO} creating osreldate.h from newvers.sh
	setvar PARAMFILE ${.CURDIR}/../sys/sys/param.h; \
	. ${.CURDIR}/../sys/conf/newvers.sh;			\
	echo "$$COPYRIGHT" > osreldate.h;			\
	echo "#ifdef _KERNEL" >> osreldate.h;			\
	echo '#error "osreldate.h must not be used in the kernel, use sys/param.h"' >> osreldate.h; \
	echo "#else" >> osreldate.h;				\
	echo "#undef __DragonFly_version" >> osreldate.h;	\
	echo "#define __DragonFly_version $$RELDATE" >> osreldate.h; \
	echo "#ifdef __FreeBSD__" >> osreldate.h;		\
	echo "#undef __FreeBSD_version" >> osreldate.h;	\
	echo "#define __FreeBSD_version 480101" >> osreldate.h; \
	echo "#endif" >> osreldate.h;				\
	echo "#endif" >> osreldate.h

.for i in ${LHDRS}
INCSLINKS+=	sys/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${MHDRS}
INCSLINKS+=	machine/$i ${INCLUDEDIR}/$i
.endfor
.for i in ${PHDRS}
INCSLINKS+=	emulation/posix4/$i ${INCLUDEDIR}/$i
.endfor
INCSLINKS+=	bus/cam ${INCLUDEDIR}/cam
INCSLINKS+=	vfs/msdosfs ${INCLUDEDIR}/msdosfs
INCSLINKS+=	vfs/isofs ${INCLUDEDIR}/isofs
INCSLINKS+=	vfs/mfs ${INCLUDEDIR}/mfs
INCSLINKS+=	vfs/nfs ${INCLUDEDIR}/nfs
INCSLINKS+=	vfs/ntfs ${INCLUDEDIR}/ntfs
INCSLINKS+=	vfs/nwfs ${INCLUDEDIR}/nwfs
INCSLINKS+=	vfs ${INCLUDEDIR}/fs
# this is for <ufs/ufs/...> and <ufs/ffs/...> paths used by ports
# It is hopefully a temporary hack until we have environments working.
INCSLINKS+=	vfs ${INCLUDEDIR}/ufs
INCSLINKS+=	ufs ${INCLUDEDIR}/vfs/ffs
INCSLINKS+=	emulation ${INCLUDEDIR}/compat
INCSLINKS+=	netproto/atalk ${INCLUDEDIR}/netatalk
INCSLINKS+=	netproto/atm ${INCLUDEDIR}/netatm
INCSLINKS+=	netproto/ipsec ${INCLUDEDIR}/netipsec
INCSLINKS+=	netproto/ipx ${INCLUDEDIR}/netipx
INCSLINKS+=	netproto/key ${INCLUDEDIR}/netkey
INCSLINKS+=	netproto/natm ${INCLUDEDIR}/netnatm
INCSLINKS+=	netproto/ncp ${INCLUDEDIR}/netncp
INCSLINKS+=	netproto/ns ${INCLUDEDIR}/netns
INCSLINKS+=	netproto/smb ${INCLUDEDIR}/netsmb
INCSLINKS+=	bus/pccard ${INCLUDEDIR}/pccard
# warning, we cannot softlink through ${INCLUDEDIR}/machine/... because
# machine itself is a softlink into the source tree in buildworld's 
# temporary include hierarchy.

mtree_setup:
.for i in ${LDIRS} ${LSYMSUBDIRS} ${LSYMSUBDIRS3} machine crypto
	if [ -h ${DESTDIR}/usr/include/$i ]; then \
		rm -f ${DESTDIR}/usr/include/$i; \
	fi
.endfor
	mtree -deU -f ${.CURDIR}/../etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include

copies:		mtree_setup
.for i in ${LDIRS} ${LSUBDIRS} ${LSUBDIRS3}
	cd ${.CURDIR}/../sys; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 $i/*.h \
		${DESTDIR}/usr/include/$i
.endfor
	cd ${.CURDIR}/../sys; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 opencrypto/*.h \
		${DESTDIR}/usr/include/crypto
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/machine
.if exists(${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc)
	cd ${.CURDIR}/../sys/${MACHINE_ARCH}/include/pc; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/machine/pc
.endif
.endif
.if exists(${.CURDIR}/../sys/net/i4b/include/${MACHINE_ARCH})
	cd ${.CURDIR}/../sys/net/i4b/include/${MACHINE_ARCH}; \
		${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 *.h \
		${DESTDIR}/usr/include/i4b_machine
.endif
.for i in ${SHDRS}
	ln -sf ../sys/$i ${DESTDIR}/usr/include/machine/$i
.endfor
.for i in ${RMHEADERS}
	rm -f ${DESTDIR}/usr/include/$i
.endfor

symlinks:	mtree_setup
	@${ECHO} "Setting up symlinks to kernel source tree..."
.for i in ${LDIRS}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s ../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
	rm -rf ${DESTDIR}/usr/include/crypto
	ln -s ../../sys/opencrypto ${DESTDIR}/usr/include/crypto
.for i in ${LSYMSUBDIRS}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s ../../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
.for i in ${LSYMSUBDIRS3}
	rm -rf ${DESTDIR}/usr/include/$i
	ln -s ../../../../sys/$i ${DESTDIR}/usr/include/$i
.endfor
	rm -rf ${DESTDIR}/usr/include/machine
	ln -s ../../sys/${MACHINE_ARCH}/include ${DESTDIR}/usr/include/machine
	rm -rf ${DESTDIR}/usr/include/i4b_machine
	ln -s ../../sys/net/i4b/include/${MACHINE_ARCH} ${DESTDIR}/usr/include/i4b_machine

.include <bsd.prog.mk>

installincludes: ${SHARED}
