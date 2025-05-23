# $FreeBSD: src/share/examples/Makefile,v 1.28.2.6 2002/08/08 09:03:47 ru Exp $
# $DragonFly: src/share/examples/Makefile,v 1.2 2003/06/17 04:36:57 dillon Exp $
#
# Doing a make install builds /usr/share/examples

DIRS!=	for i in *; do \
	    if test -d $$i -a $$i != CVS -a \
		$$i != ipfilter -a $$i != smbfs; then \
		echo $$i; \
            fi; \
	done

DDIR=	${DESTDIR}/usr/share/examples

NOOBJ=	noobj

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

all clean cleandir depend lint tags:

beforeinstall: etc-examples ${SHARED}

.for dir in ${DIRS}
FILES!=	find -L ${dir} \( -name CVS -prune \) -o -type f -print
.for file in ${FILES}
copies::
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m 644 ${file} ${DDIR}/${file}
.endfor
.endfor

.for dir in ${DIRS}
symlinks::
	rm -rf ${DDIR}/${dir}; ln -s ${.CURDIR}/${dir} ${DDIR}
.endfor

etc-examples:
.if ${SHARED} != "symlinks"
	(cd ${.CURDIR}/../../etc; ${MAKE} etc-examples)
.endif

.if ${SHARED} != "symlinks"
SUBDIR=	smbfs
.if !defined(NO_IPFILTER)
SUBDIR+=ipfilter
.endif
.endif

.include <bsd.subdir.mk>
