# $FreeBSD: src/gnu/usr.bin/cc/cc_tools/Makefile,v 1.48.2.6 2002/06/20 23:13:31 obrien Exp $
# $DragonFly: src/gnu/usr.bin/cc/cc_tools/Attic/Makefile,v 1.4 2004/03/22 20:57:48 dillon Exp $

#
# This could probably be merged with ../cc_int/Makefile, but bsd.lib.mk
# is such a !@#!*#% nightmare because of how it reprograms the dependencies,
# suffix rules, SRCS, etc.  It's easiest to cheat by using bsd.prog.mk and
# SRCS to get dependencies.
#

# ../Makefile.inc will put an absolute path to our objdir in CFLAGS.
# Prevent mkdep from using it, so that we don't have to give rules for
# aliases of generated headers.

CFLAGS+=	-I.
CFLAGS+=	-static

.include "../Makefile.inc"

.PATH: ${GCCDIR} ${GCCDIR}/cp

#-----------------------------------------------------------------------
# insn-* gunk

.for F in attr codes config flags
insn-$F.h: gen$F ${MD_FILE}
	./gen$F ${MD_FILE} > insn-$F.h
GENSRCS+=	insn-$F.h
.endfor

GENSRCS+=	gen-time-stamp
gen-time-stamp: genattrtab genemit genextract genopinit genoutput genpeep genrecog
	touch ${.TARGET}

.for F in attr codes config emit extract flags opinit output peep recog
build-tools: gen$F

gen$F: gen$F.no rtl.no print-rtl.no obstack.no bitmap.no
	${NXCC} ${NXCFLAGS} ${NXLDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F gen$F.no
.endfor

.for F in attrtab
build-tools: gen$F

gen$F: gen$F.no rtl.no rtlanal.no print-rtl.no obstack.no bitmap.no
	${NXCC} ${NXCFLAGS} ${NXLDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F gen$F.no
.endfor

SRCS+=		bitmap.c obstack.c print-rtl.c rtl.c rtlanal.c
CLEANFILES+=	bitmap.no obstack.no print-rtl.no rtl.no rtlanal.no

.for F in check genrtl
build-tools: gen$F

gen$F: gen$F.no
	${NXCC} ${NXCFLAGS} ${NXLDFLAGS} -o ${.TARGET} ${.ALLSRC}

GENSRCS+=	gen$F.c
CLEANFILES+=	gen$F gen$F.no
.endfor

.ORDER: genrtl.c genrtl.h
genrtl.c genrtl.h: gengenrtl
	./gengenrtl genrtl.h genrtl.c

GENSRCS+=	genrtl.c genrtl.h

#-----------------------------------------------------------------------
# C hash codes
c-gperf.h: c-parse.gperf
	gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$$ \
	    ${GCCDIR}/c-parse.gperf > ${.TARGET}
GENSRCS+=	c-gperf.h

#-----------------------------------------------------------------------
# C++ hash codes
gxx-hash.h: gxx.gperf
	gperf -p -j1 -g -o -t -N is_reserved_word '-k1,4,7,$$' \
	    ${GCCDIR}/cp/gxx.gperf >gxx-hash.h
GENSRCS+=	gxx-hash.h

# make a link the the Cygnus used name for now -- hopes are they will change it
hash.h: gxx-hash.h
	ln -sf ${.ALLSRC} ${.TARGET}
GENSRCS+=	hash.h

#-----------------------------------------------------------------------
# Common parser stuff.

tree-check.h: gencheck
	./gencheck > ${.TARGET}
GENSRCS+=	tree-check.h

#-----------------------------------------------------------------------
# the host/target compiler config.

COMMONHDRS=	config.h hconfig.h multilib.h options.h specs.h tconfig.h tm.h
GENSRCS+=	${COMMONHDRS} gencheck.h

config.h hconfig.h:
	echo '#include "auto-host.h"'				> ${.TARGET}
	echo '#include "gansidecl.h"'				>> ${.TARGET}
	echo '#include "${TARGET_ARCH}/xm-${TARGET_ARCH}.h"'	>> ${.TARGET}
	echo '#include "hwint.h"'				>> ${.TARGET}

gencheck.h:
	echo '#include "cp/cp-tree.def"'		> ${.TARGET}
	echo '#include "objc/objc-tree.def"'		>> ${.TARGET}

multilib.h:
	echo 'static char *multilib_raw[] = { \
	    "aout maout;", "elf !maout;", NULL };'	> ${.TARGET}
	echo 'static char *multilib_matches_raw[] = { \
	    "maout maout;", "melf melf;", NULL };'	>> ${.TARGET}
	echo 'static char *multilib_extra = "";'	>> ${.TARGET}

options.h:
	echo '#include "cp/lang-options.h"'		> ${.TARGET}
	echo '#include "f/lang-options.h"'		>> ${.TARGET}

specs.h:
	echo '#include "cp/lang-specs.h"'		> ${.TARGET}
	echo '#include "f/lang-specs.h"'		>> ${.TARGET}
	echo '#include "objc/lang-specs.h"'		>> ${.TARGET}

tconfig.h:
	echo '#include "gansidecl.h"'				> ${.TARGET}
	echo '#include "${TARGET_ARCH}/xm-${TARGET_ARCH}.h"'	>> ${.TARGET}

# KEEP THIS IN SYNC with src/gcc/lib/libgcc/Makefile !!
tm.h:
	echo '#include "${TARGET_ARCH}/${TARGET_ARCH}.h"'	> ${.TARGET}
.if ${TARGET_ARCH} == "i386"
	echo '#include "${TARGET_ARCH}/att.h"'			>> ${.TARGET}
.endif
	echo '#include <dragonfly.h>'				>> ${.TARGET}
	echo '#include "dbxelf.h"'				>> ${.TARGET}
.if exists(${GCCDIR}/config/${TARGET_ARCH}/elf.h)
	echo '#include "${TARGET_ARCH}/elf.h"'			>> ${.TARGET}
.endif
	echo '#include "${TARGET_ARCH}/dragonfly.h"'		>> ${.TARGET}
.if ${TARGET_ARCH} == "i386"
	echo '#include "${TARGET_ARCH}/perform.h"'		>> ${.TARGET}
.endif
	echo '#include <dragonfly-native.h>'			>> ${.TARGET}

#-----------------------------------------------------------------------
# General things.

SRCS+=		${GENSRCS}
CLEANFILES+=	${GENSRCS}

all: ${SRCS}

build-tools: depend

.include <bsd.prog.mk>

#-----------------------------------------------------------------------
# Fixups.

# Set OBJS the same as bsd.prog.mk would do if we defined PROG.  We can't
# define PROG because we have multiple programs.
#
OBJS+=		${SRCS:N*.h:R:S/$/.o/g}

# Setup dependancies so helper programs will compile -jn
#
${CLEANFILES:M*.no:Ngencheck.no:Ngengenrtl.no}: ${SRCS:M*.h:Ninsn-*.h}

genextract.no genattrtab.no: insn-config.h

gencheck.no: gencheck.h ${SRCS:M*.h:Ngenrtl.h:Ntree-check.h:Ninsn-*.h}

gengenrtl.no: ${SRCS:M*.h:Ngenrtl.h:Ninsn-*.h}

