#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.3.2.1 2001/10/02 11:15:38 ru Exp $
# $DragonFly: src/lib/libcr/db/hash/Attic/Makefile.inc,v 1.3 2003/07/24 21:41:47 dillon Exp $

.PATH: ${.CURDIR}/../libcr/db/hash

SRCS+=	hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
