#	@(#)Makefile	5.11 (Berkeley) 9/6/90
# $FreeBSD: src/lib/libc/xdr/Makefile.inc,v 1.10.2.1 2001/04/25 10:04:19 ru Exp $
# $DragonFly: src/lib/libcr/xdr/Attic/Makefile.inc,v 1.3 2003/07/24 21:41:56 dillon Exp $

.PATH: ${.CURDIR}/../libcr/xdr ${.CURDIR}/.
SRCS+=	xdr.c xdr_array.c xdr_float.c xdr_mem.c \
	xdr_rec.c xdr_reference.c xdr_stdio.c xdr_sizeof.c

.if ${LIB} == "c"
MAN+=	xdr.3

MLINKS+=	xdr.3 xdr_array.3 \
		xdr.3 xdr_bool.3 \
		xdr.3 xdr_bytes.3 \
		xdr.3 xdr_char.3 \
		xdr.3 xdr_destroy.3 \
		xdr.3 xdr_double.3 \
		xdr.3 xdr_enum.3 \
		xdr.3 xdr_float.3 \
		xdr.3 xdr_free.3 \
		xdr.3 xdr_getpos.3 \
		xdr.3 xdr_inline.3 \
		xdr.3 xdr_int.3 \
		xdr.3 xdr_long.3 \
		xdr.3 xdrmem_create.3 \
		xdr.3 xdr_opaque.3 \
		xdr.3 xdr_pointer.3 \
		xdr.3 xdrrec_create.3 \
		xdr.3 xdrrec_endofrecord.3 \
		xdr.3 xdrrec_eof.3 \
		xdr.3 xdrrec_skiprecord.3 \
		xdr.3 xdr_reference.3 \
		xdr.3 xdr_setpos.3 \
		xdr.3 xdr_short.3 \
		xdr.3 xdrstdio_create.3 \
		xdr.3 xdr_string.3 \
		xdr.3 xdr_u_char.3 \
		xdr.3 xdr_u_long.3 \
		xdr.3 xdr_u_short.3 \
		xdr.3 xdr_union.3 \
		xdr.3 xdr_vector.3 \
		xdr.3 xdr_void.3 \
		xdr.3 xdr_wrapstring.3
.endif
