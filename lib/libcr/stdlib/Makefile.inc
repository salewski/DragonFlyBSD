#	from @(#)Makefile.inc	8.3 (Berkeley) 2/4/95
# $FreeBSD: src/lib/libc/stdlib/Makefile.inc,v 1.19.2.4 2001/10/02 11:15:38 ru Exp $
# $DragonFly: src/lib/libcr/stdlib/Attic/Makefile.inc,v 1.7 2005/03/13 15:10:03 swildner Exp $

# machine-independent stdlib sources
.PATH: ${.CURDIR}/../libcr/${MACHINE_ARCH}/stdlib ${.CURDIR}/../libcr/stdlib

MISRCS+=abort.c abs.c atexit.c atof.c atoi.c atol.c bsearch.c calloc.c div.c \
	exit.c getenv.c getopt.c getopt_long.c getsubopt.c hcreate.c heapsort.c \
	labs.c ldiv.c malloc.c merge.c putenv.c qsort.c radixsort.c rand.c \
	random.c reallocf.c realpath.c setenv.c strtod.c strtol.c strtoll.c \
	strtoq.c strtoul.c strtoull.c strtouq.c system.c tdelete.c tfind.c \
	tsearch.c twalk.c

# machine-dependent stdlib sources
.if exists(${.CURDIR}/../libcr/${MACHINE_ARCH}/stdlib/Makefile.inc)
.include "${.CURDIR}/../libcr/${MACHINE_ARCH}/stdlib/Makefile.inc"
.endif

.if ${LIB} == "c"
MAN+=	abort.3 abs.3 alloca.3 atexit.3 atof.3 atoi.3 atol.3 bsearch.3 \
	div.3 exit.3 getenv.3 getopt.3 getopt_long.3 getsubopt.3 hcreate.3 \
	labs.3 ldiv.3 malloc.3 memory.3 qsort.3 radixsort.3 rand.3 random.3 \
	realpath.3 strtod.3 strtol.3 strtoul.3 system.3 tsearch.3

MLINKS+=getenv.3 putenv.3 getenv.3 setenv.3 getenv.3 unsetenv.3
MLINKS+=hcreate.3 hdestroy.3 hcreate.3 hsearch.3
MLINKS+=qsort.3 heapsort.3 qsort.3 mergesort.3
MLINKS+=rand.3 rand_r.3 rand.3 srand.3
MLINKS+=random.3 initstate.3 random.3 setstate.3 random.3 srandom.3 \
	random.3 srandomdev.3
MLINKS+=strtol.3 strtoll.3 strtol.3 strtoq.3
MLINKS+=strtoul.3 strtoull.3 strtoul.3 strtouq.3
MLINKS+=malloc.3 calloc.3 malloc.3 free.3 malloc.3 realloc.3 malloc.3 reallocf.3
.endif
