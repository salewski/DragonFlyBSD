#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/recno/Makefile.inc,v 1.3 1999/08/27 23:58:25 peter Exp $
# $DragonFly: src/lib/libcr/db/recno/Attic/Makefile.inc,v 1.3 2003/07/24 21:41:48 dillon Exp $

.PATH: ${.CURDIR}/../libcr/db/recno

SRCS+=	rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
