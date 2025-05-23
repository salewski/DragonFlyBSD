# $FreeBSD: src/share/mk/bsd.kern.mk,v 1.17.2.1 2001/08/01 16:56:56 obrien Exp $
# $DragonFly: src/sys/conf/bsd.kern.mk,v 1.5 2004/02/24 18:07:11 joerg Exp $

#
# Warning flags for compiling the kernel and components of the kernel.
#
# Note that the newly added -Wcast-qual is responsible for generating 
# most of the remaining warnings.  Warnings introduced with -Wall will
# also pop up, but are easier to fix.
#
.if ${CCVER} == "gcc2"
CWARNFLAGS?=	-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual \
		-fformat-extensions -ansi
.else
CWARNFLAGS?=	-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
		-Wmissing-prototypes -Wpointer-arith -Winline -Wcast-qual \
		-ansi
.endif
#
# The following flags are next up for working on:
#	-W
#
# When working on removing warnings from code, the `-Werror' flag should be
# of material assistance.
#

#
# On the i386, do not align the stack to 16-byte boundaries.  Otherwise GCC
# 2.95 adds code to the entry and exit point of every function to align the
# stack to 16-byte boundaries -- thus wasting approximately 12 bytes of stack
# per function call.  While the 16-byte alignment may benefit micro benchmarks, 
# it is probably an overall loss as it makes the code bigger (less efficient
# use of code cache tag lines) and uses more stack (less efficient use of data
# cache tag lines)
#
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-mpreferred-stack-boundary=2
.endif

#
# On the alpha, make sure that we don't use floating-point registers and
# allow the use of EV56 instructions (only needed for low-level i/o).
#
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=	-mno-fp-regs -Wa,-mev56
.endif

# Require the proper use of 'extern' for variables.  -fno-common will
# cause duplicate declarations to generate a link error.
#
CFLAGS+=	-fno-common

# Prevent GCC 3.x from making certain libc based inline optimizations
#
CFLAGS+=	-ffreestanding
