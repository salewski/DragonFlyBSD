# $DragonFly: src/gnu/usr.bin/binutils215/libbfd/Attic/Makefile,v 1.1 2004/12/20 13:14:45 asmodai Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/bfd ${SRCDIR}/opcodes

LIB=	bfd
SRCS+=	archive.c archive64.c archures.c bfd.c bfdio.c binary.c cache.c \
	coffgen.c corefile.c elf.c elf-eh-frame.c elf-strtab.c format.c \
	hash.c ihex.c init.c libbfd.c linker.c merge.c opncls.c reloc.c \
	section.c simple.c srec.c stab-syms.c stabs.c syms.c \
	targets.c tekhex.c \
	targmatch.h dwarf1.c dwarf2.c config.h bfd.h bfdver.h
.if (${TARGET_ARCH} == "alpha" || ${TARGET_ARCH} == "sparc64")
WARNS=	2
.endif
CFLAGS+= -I${SRCDIR}/bfd
INTERNALLIB=	true
CLEANFILES+=	bfd.h bfdver.h config.h elf32-i386-fbsd.c targmatch.h

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH: ${BINUTILSDISTDIR}/bfd ${BINUTILSDISTDIR}/opcodes
CFLAGS+= -I${BINUTILSDISTDIR}/bfd -I${BINUTILSDISTDIR}/include -I.
.endif

SELARCH=
.if ${BINUTIL_ARCH} == "amd64"
SELARCH= &bfd_i386_arch
.else
.for _a in ${ARCHS}
.if ${SELARCH} == ""
SELARCH+= &bfd_${_a}_arch
.else
SELARCH+= ,&bfd_${_a}_arch
.endif
.endfor
.endif
CFLAGS+= -DSELECT_ARCHITECTURES="${SELARCH}"

SELVEC=
.for _v in ${VECS}
.if ${SELVEC} == ""
SELVEC+= &${_v}
.else
SELVEC+= ,&${_v}
.endif
.endfor
CFLAGS+= -DSELECT_VECS="${SELVEC}"

# Used to do keyword replacement in bfd-X.h
#
.if ${MACHINE_ARCH} == "i386"
BFD_DEFAULT_TARGET_SIZE?=32
BFD_ARCH_SIZE?=32
BFD_DEFAULT_TARGET_SIZE?=32
BFD_HOST_64BIT_LONG?=0
BFD_HOST_64_BIT?=long long
BFD_HOST_U_64_BIT?=unsigned long long
.else
BFD_DEFAULT_TARGET_SIZE?=64
BFD_ARCH_SIZE?=64
BFD_DEFAULT_TARGET_SIZE?=64
BFD_HOST_64BIT_LONG?=1
BFD_HOST_64_BIT?=long long
BFD_HOST_U_64_BIT?=unsigned long long
.endif
BFD_HOST_64_BIT_DEFINED?=1
BFD_HOST_LONG_LONG?=1

bfd.h: 	${SRCDIR}/bfd/bfd-in2.h
	sed -e "s/@wordsize@/${BFD_ARCH_SIZE}/g" \
	    -e "s/@bfd_default_target_size@/${BFD_DEFAULT_TARGET_SIZE}/g" \
	    -e "s/@BFD_HOST_64BIT_LONG@/${BFD_HOST_64BIT_LONG}/g" \
	    -e "s/@BFD_HOST_64_BIT@/${BFD_HOST_64_BIT}/g" \
	    -e "s/@BFD_HOST_U_64_BIT@/${BFD_HOST_U_64_BIT}/g" \
	    -e "s/@BFD_HOST_64_BIT_DEFINED@/${BFD_HOST_64_BIT_DEFINED}/g" \
	    -e "s/@BFD_HOST_LONG_LONG@/${BFD_HOST_LONG_LONG}/g" \
	    -e "s/@bfd_file_ptr@/${BFD_HOST_64_BIT}/g" \
	    < ${SRCDIR}/bfd/bfd-in2.h > bfd.h.new
	mv -f bfd.h.new bfd.h

bfdver.h: ${SRCDIR}/bfd/version.h
	sed -e "s/@bfd_version_string@/\\\"`echo -n ${VERSION}`\\\"/g" \
	    -e "s/@bfd_version@/${VERSION_BFD}/g" \
	    < ${SRCDIR}/bfd/version.h > bfdver.h.new
	mv -f bfdver.h.new bfdver.h

targmatch.h: targmatch.sed config.bfd dfly_targmatch.h
	sed -f ${.ALLSRC:M*.sed} ${.ALLSRC:M*.bfd} > ${.TARGET}.new
	cat ${.CURDIR}/dfly_targmatch.h >> ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}
	
config.h: config.h.template
.if ${BINUTIL_ARCH} == "i386"
	sed -e 's,!!TRAD_HEADER!!,"hosts/i386bsd.h",g' ${.ALLSRC} > ${.TARGET}
.else
	sed -e 's,!!TRAD_HEADER!!,,g' ${.ALLSRC} > ${.TARGET}
.endif

.include <bsd.lib.mk>
