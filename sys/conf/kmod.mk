#	From: @(#)bsd.prog.mk	5.26 (Berkeley) 6/25/91
# $FreeBSD: src/sys/conf/kmod.mk,v 1.82.2.15 2003/02/10 13:11:50 nyan Exp $
# $DragonFly: src/sys/conf/kmod.mk,v 1.18 2005/02/18 11:41:41 corecode Exp $
#
# The include file <bsd.kmod.mk> handles installing Kernel Loadable Device
# drivers (KLD's).
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# KMOD          The name of the kernel module to build.
#
# KMODDIR	Base path for kernel modules (see kld(4)). [/modules]
#
# KMODOWN	KLD owner. [${BINOWN}]
#
# KMODGRP	KLD group. [${BINGRP}]
#
# KMODMODE	KLD mode. [${BINMODE}]
#
# KMODLOAD	Command to load a kernel module [/sbin/kldload]
#
# KMODUNLOAD	Command to unload a kernel module [/sbin/kldunload]
#
# NOMAN		KLD does not have a manual page if set.
#
# PROG          The name of the kernel module to build.
#		If not supplied, ${KMOD}.o is used.
#
# SRCS          List of source files
#
# KMODDEPS	List of modules which this one is dependant on
#
# DESTDIR	Change the tree where the module gets installed. [not set]
#
# MFILES	Optionally a list of interfaces used by the module.
#		This file contains a default list of interfaces.
#
# +++ targets +++
#
# 	install:
#               install the kernel module and its manual pages; if the Makefile
#               does not itself define the target install, the targets
#               beforeinstall and afterinstall may also be used to cause
#               actions immediately before and after the install target
#		is executed.
#
# 	load:
#		Load KLD.
#
# 	unload:
#		Unload KLD.
#
# bsd.obj.mk: clean, cleandir and obj
# bsd.dep.mk: cleandepend, depend and tags
# bsd.man.mk: maninstall
#

OBJCOPY?=	objcopy
KMODLOAD?=	/sbin/kldload
KMODUNLOAD?=	/sbin/kldunload

.include <bsd.init.mk>

.SUFFIXES: .out .o .c .cc .cxx .C .y .l .s .S

CFLAGS+=	${COPTS} -D_KERNEL ${CWARNFLAGS}
CFLAGS+=	-DKLD_MODULE

# Don't use any standard include directories.
# Since -nostdinc will annull any previous -I paths, we repeat all
# such paths after -nostdinc.  It doesn't seem to be possible to
# add to the front of `make' variable.
#
# Don't use -I- anymore, source-relative includes are desireable.
_ICFLAGS:=	${CFLAGS:M-I*}
CFLAGS+=	-nostdinc ${_ICFLAGS}

# Add -I paths for system headers.  Individual KLD makefiles don't
# need any -I paths for this.  Similar defaults for .PATH can't be
# set because there are no standard paths for non-headers.
CFLAGS+=	-I. -I@

# Add a -I path to standard headers like <stddef.h>.  Use a relative
# path to src/include if possible.  If the @ symlink hasn't been built
# yet, then we can't tell if the relative path exists.  Add both the
# potential relative path and an absolute path in that case.
.if exists(@)
.if exists(@/../include)
CFLAGS+=	-I@/../include
.else
CFLAGS+=	-I${DESTDIR}/usr/include
.endif
.else # !@
CFLAGS+=	-I@/../include -I${DESTDIR}/usr/include
.endif # @

CFLAGS+=	${DEBUG_FLAGS}

OBJS+=  ${SRCS:N*.h:N*.patch:R:S/$/.o/g}
.for _PATCH in ${SRCS:T:N*.h.patch:M*.patch}
.for _OBJ in ${_PATCH:R:R:S/$/.o/}
OBJS:=	${OBJS:N${_OBJ}} ${_OBJ}
.endfor
.endfor

.if !defined(PROG)
PROG=	${KMOD}.ko
.endif

${PROG}: ${KMOD}.kld ${KMODDEPS}
	${LD} -Bshareable ${LDFLAGS} -o ${.TARGET} ${KMOD}.kld ${KMODDEPS}

.if defined(KMODDEPS)
.for dep in ${KMODDEPS}
CLEANFILES+=	${dep} __${dep}_hack_dep.c

${dep}:
	touch __${dep}_hack_dep.c
	${CC} -shared ${CFLAGS} -o ${dep} __${dep}_hack_dep.c
.endfor
.endif

${KMOD}.kld: ${OBJS}
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${OBJS}

.if !defined(NOMAN)
.include <bsd.man.mk>
.endif

_ILINKS=@ machine
.if defined(ARCH)
_ILINKS+=${ARCH}
.endif

all: objwarn ${PROG}
.if !defined(NOMAN)
all: _manpages
.endif

beforedepend: ${_ILINKS}
# Ensure that the links exist without depending on it when it exists which
# causes all the modules to be rebuilt when the directory pointed to changes.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${OBJS}: ${_link}
.endif
.endfor

# Search for kernel source tree in standard places.
.for _dir in ${.CURDIR}/../.. ${.CURDIR}/../../.. ${.CURDIR}/../../../.. /sys /usr/src/sys
.if !defined(SYSDIR) && exists(${_dir}/kern/)
SYSDIR=	${_dir}
.endif
.endfor
.if !defined(SYSDIR) || !exists(${SYSDIR}/kern)
.error "can't find kernel source tree"
.endif

#	path=`(cd $$path && /bin/pwd)` ; 

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		path=${SYSDIR}/${MACHINE_ARCH}/include ;; \
	@) \
		path=${SYSDIR} ;; \
	arch_*) \
		path=${.CURDIR}/${MACHINE_ARCH} ;; \
	esac ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}

CLEANFILES+= ${PROG} ${KMOD}.kld ${OBJS} ${_ILINKS} symb.tmp tmp.o

.if !target(install)

_INSTALLFLAGS:=	${INSTALLFLAGS}
.for ie in ${INSTALLFLAGS_EDIT}
_INSTALLFLAGS:=	${_INSTALLFLAGS${ie}}
.endfor

.if !target(realinstall)
realinstall: _kmodinstall
.ORDER: beforeinstall _kmodinstall
_kmodinstall:
.if defined(INSTALLSTRIPPEDMODULES)
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}
	${OBJCOPY} --strip-debug ${DESTDIR}${KMODDIR}/${PROG}
.else
	${INSTALL} -o ${KMODOWN} -g ${KMODGRP} -m ${KMODMODE} \
	    ${_INSTALLFLAGS} ${PROG} ${DESTDIR}${KMODDIR}
.endif
.endif !target(realinstall)

.include <bsd.links.mk>

.if !defined(NOMAN)
realinstall: _maninstall
.ORDER: beforeinstall _maninstall
.endif

.endif !target(install)

.if !target(load)
load:	${PROG}
	${KMODLOAD} -v ./${KMOD}.ko
.endif

.if !target(unload)
unload:
	${KMODUNLOAD} -v ${KMOD}
.endif

.for _src in ${SRCS:Mopt_*.h} ${SRCS:Muse_*.h}
CLEANFILES+=	${_src}
.if !target(${_src})
.if defined(BUILDING_WITH_KERNEL) && exists(${BUILDING_WITH_KERNEL}/${_src})
${_src}: ${BUILDING_WITH_KERNEL}/${_src}
	cp ${BUILDING_WITH_KERNEL}/${_src} ${.TARGET}
.else
${_src}:
	touch ${.TARGET}
.endif	# BUILDING_WITH_KERNEL
.endif
.endfor

MFILES?= kern/bus_if.m kern/device_if.m bus/iicbus/iicbb_if.m \
    bus/iicbus/iicbus_if.m bus/isa/isa_if.m dev/netif/mii_layer/miibus_if.m \
    bus/pccard/card_if.m bus/pccard/power_if.m bus/pci/pci_if.m \
    bus/pci/pcib_if.m \
    bus/ppbus/ppbus_if.m bus/smbus/smbus_if.m bus/usb/usb_if.m \
    dev/sound/pcm/ac97_if.m dev/sound/pcm/channel_if.m \
    dev/sound/pcm/feeder_if.m dev/sound/pcm/mixer_if.m \
    libiconv/iconv_converter_if.m dev/agp/agp_if.m opencrypto/crypto_if.m \
    bus/canbus/canbus_if.m

.for _srcsrc in ${MFILES}
.for _ext in c h
.for _src in ${SRCS:M${_srcsrc:T:R}.${_ext}}
CLEANFILES+=	${_src}
.if !target(${_src})
${_src}: @
.if exists(@)
${_src}: @/tools/makeobjops.awk @/${_srcsrc}
.endif
	awk -f @/tools/makeobjops.awk -- -${_ext} @/${_srcsrc}
.endif
.endfor # _src
.endfor # _ext
.endfor # _srcsrc

#.for _ext in c h
#.if ${SRCS:Mvnode_if.${_ext}} != ""
#CLEANFILES+=	vnode_if.${_ext}
#vnode_if.${_ext}: @
#.if exists(@)
#vnode_if.${_ext}: @/tools/vnode_if.awk @/kern/vnode_if.src
#.endif
#	awk -f @/tools/vnode_if.awk -- -${_ext} @/kern/vnode_if.src
#.endif
#.endfor

regress:

.include <bsd.dep.mk>

.if !exists(${DEPENDFILE})
${OBJS}: ${SRCS:M*.h}
.endif

.include <bsd.obj.mk>
.include "bsd.kern.mk"

# Behaves like MODULE_OVERRIDE
.if defined(KLD_DEPS)
all: _kdeps_all
_kdeps_all: @
.for _mdep in ${KLD_DEPS}
	cd ${SYSDIR}/${_mdep} && make all
.endfor
depend: _kdeps_depend
_kdeps_depend: @
.for _mdep in ${KLD_DEPS}
	cd ${SYSDIR}/${_mdep} && make depend
.endfor
install: _kdeps_install
_kdeps_install: @
.for _mdep in ${KLD_DEPS}
	cd ${SYSDIR}/${_mdep} && make install
.endfor
clean: _kdeps_clean
_kdeps_clean: @
.for _mdep in ${KLD_DEPS}
	cd ${SYSDIR}/${_mdep} && make clean
.endfor
.endif
