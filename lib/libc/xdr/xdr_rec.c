/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 *
 * @(#)xdr_rec.c 1.21 87/08/11 Copyr 1984 Sun Micro
 * @(#)xdr_rec.c	2.2 88/08/01 4.0 RPCSRC
 * $FreeBSD: src/lib/libc/xdr/xdr_rec.c,v 1.12 2000/01/19 06:12:32 wpaul Exp $
 * $DragonFly: src/lib/libc/xdr/xdr_rec.c,v 1.4 2004/10/25 19:38:02 drhodus Exp $
 */

/*
 * xdr_rec.c, Implements TCP/IP based XDR streams with a "record marking"
 * layer above tcp (for rpc's use).
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * These routines interface XDRSTREAMS to a tcp/ip connection.
 * There is a record marking layer between the xdr stream
 * and the tcp transport level.  A record is composed on one or more
 * record fragments.  A record fragment is a thirty-two bit header followed
 * by n bytes of data, where n is contained in the header.  The header
 * is represented as a htonl(u_long).  Thegh order bit encodes
 * whether or not the fragment is the last fragment of the record
 * (1 => fragment is last, 0 => more fragments to follow.
 * The other 31 bits encode the byte length of the fragment.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netinet/in.h>

static u_int	fix_buf_size();
static bool_t	flush_out();
static bool_t	get_input_bytes();
static bool_t	set_input_fragment();
static bool_t	skip_input_bytes();

static bool_t	xdrrec_getlong();
static bool_t	xdrrec_putlong();
static bool_t	xdrrec_getbytes();
static bool_t	xdrrec_putbytes();
static u_int	xdrrec_getpos();
static bool_t	xdrrec_setpos();
static int32_t *xdrrec_inline();
static void	xdrrec_destroy();

static struct  xdr_ops xdrrec_ops = {
	xdrrec_getlong,
	xdrrec_putlong,
	xdrrec_getbytes,
	xdrrec_putbytes,
	xdrrec_getpos,
	xdrrec_setpos,
	xdrrec_inline,
	xdrrec_destroy
};

/*
 * A record is composed of one or more record fragments.
 * A record fragment is a two-byte header followed by zero to
 * 2**32-1 bytes.  The header is treated as a long unsigned and is
 * encode/decoded to the network via htonl/ntohl.  The low order 31 bits
 * are a byte count of the fragment.  The highest order bit is a boolean:
 * 1 => this fragment is the last fragment of the record,
 * 0 => this fragment is followed by more fragment(s).
 *
 * The fragment/record machinery is not general;  it is constructed to
 * meet the needs of xdr and rpc based on tcp.
 */

#define LAST_FRAG ((u_int32_t)(1 << 31))

typedef struct rec_strm {
	caddr_t tcp_handle;
	caddr_t the_buffer;
	/*
	 * out-goung bits
	 */
	int (*writeit) (caddr_t, caddr_t, int);
	caddr_t out_base;	/* output buffer (points to frag header) */
	caddr_t out_finger;	/* next output position */
	caddr_t out_boundry;	/* data cannot up to this address */
	u_int32_t *frag_header;	/* beginning of current fragment */
	bool_t frag_sent;	/* true if buffer sent in middle of record */
	/*
	 * in-coming bits
	 */
	int (*readit) (caddr_t, caddr_t, int);
	u_long in_size;	/* fixed size of the input buffer */
	caddr_t in_base;
	caddr_t in_finger;	/* location of next byte to be had */
	caddr_t in_boundry;	/* can read up to this location */
	long fbtbc;		/* fragment bytes to be consumed */
	bool_t last_frag;
	u_int sendsize;
	u_int recvsize;
} RECSTREAM;


/*
 * Create an xdr handle for xdrrec
 * xdrrec_create fills in xdrs.  Sendsize and recvsize are
 * send and recv buffer sizes (0 => use default).
 * tcp_handle is an opaque handle that is passed as the first parameter to
 * the procedures readit and writeit.  Readit and writeit are read and
 * write respectively.   They are like the system
 * calls expect that they take an opaque handle rather than an fd.
 */
void
xdrrec_create(xdrs, sendsize, recvsize, tcp_handle, readit, writeit)
	XDR *xdrs;
	u_int sendsize;
	u_int recvsize;
	caddr_t tcp_handle;
	int (*readit)();  /* like read, but pass it a tcp_handle, not sock */
	int (*writeit)();  /* like write, but pass it a tcp_handle, not sock */
{
	RECSTREAM *rstrm =
		(RECSTREAM *)mem_alloc(sizeof(RECSTREAM));

	if (rstrm == NULL) {
		(void)fprintf(stderr, "xdrrec_create: out of memory\n");
		/*
		 *  This is bad.  Should rework xdrrec_create to
		 *  return a handle, and in this case return NULL
		 */
		return;
	}
	/*
	 * adjust sizes and allocate buffer quad byte aligned
	 */
	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
	rstrm->the_buffer = mem_alloc(sendsize + recvsize + BYTES_PER_XDR_UNIT);
	if (rstrm->the_buffer == NULL) {
		(void)fprintf(stderr, "xdrrec_create: out of memory\n");
		return;
	}
	for (rstrm->out_base = rstrm->the_buffer;
		(u_long)rstrm->out_base % BYTES_PER_XDR_UNIT != 0;
		rstrm->out_base++);
	rstrm->in_base = rstrm->out_base + sendsize;
	/*
	 * now the rest ...
	 */
	xdrs->x_ops = &xdrrec_ops;
	xdrs->x_private = (caddr_t)rstrm;
	rstrm->tcp_handle = tcp_handle;
	rstrm->readit = readit;
	rstrm->writeit = writeit;
	rstrm->out_finger = rstrm->out_boundry = rstrm->out_base;
	rstrm->frag_header = (u_int32_t *)rstrm->out_base;
	rstrm->out_finger += sizeof(u_int32_t);
	rstrm->out_boundry += sendsize;
	rstrm->frag_sent = FALSE;
	rstrm->in_size = recvsize;
	rstrm->in_boundry = rstrm->in_base;
	rstrm->in_finger = (rstrm->in_boundry += recvsize);
	rstrm->fbtbc = 0;
	rstrm->last_frag = TRUE;
}


/*
 * The reoutines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */

static bool_t
xdrrec_getlong(xdrs, lp)
	XDR *xdrs;
	long *lp;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	int32_t *buflp = (int32_t *)(rstrm->in_finger);
	int32_t mylong;

	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= sizeof(int32_t)) &&
		(((long)rstrm->in_boundry - (long)buflp) >= sizeof(int32_t))) {
		*lp = (long)ntohl((u_int32_t)(*buflp));
		rstrm->fbtbc -= sizeof(int32_t);
		rstrm->in_finger += sizeof(int32_t);
	} else {
		if (! xdrrec_getbytes(xdrs, (caddr_t)&mylong, sizeof(int32_t)))
			return (FALSE);
		*lp = (long)ntohl((u_int32_t)mylong);
	}
	return (TRUE);
}

static bool_t
xdrrec_putlong(xdrs, lp)
	XDR *xdrs;
	long *lp;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	int32_t *dest_lp = ((int32_t *)(rstrm->out_finger));

	if ((rstrm->out_finger += sizeof(int32_t)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof(int32_t);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE))
			return (FALSE);
		dest_lp = ((int32_t *)(rstrm->out_finger));
		rstrm->out_finger += sizeof(int32_t);
	}
	*dest_lp = (int32_t)htonl((u_int32_t)(*lp));
	return (TRUE);
}

static bool_t  /* must manage buffers, fragments, and records */
xdrrec_getbytes(xdrs, addr, len)
	XDR *xdrs;
	caddr_t addr;
	u_int len;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	int current;

	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return (FALSE);
			if (! set_input_fragment(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current))
			return (FALSE);
		addr += current;
		rstrm->fbtbc -= current;
		len -= current;
	}
	return (TRUE);
}

static bool_t
xdrrec_putbytes(xdrs, addr, len)
	XDR *xdrs;
	caddr_t addr;
	u_int len;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	long current;

	while (len > 0) {
		current = (u_long)rstrm->out_boundry -
			(u_long)rstrm->out_finger;
		current = (len < current) ? len : current;
		memcpy(rstrm->out_finger, addr, current);
		rstrm->out_finger += current;
		addr += current;
		len -= current;
		if (rstrm->out_finger == rstrm->out_boundry) {
			rstrm->frag_sent = TRUE;
			if (! flush_out(rstrm, FALSE))
				return (FALSE);
		}
	}
	return (TRUE);
}

static u_int
xdrrec_getpos(xdrs)
	XDR *xdrs;
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	long pos;

	pos = lseek((int)(long)rstrm->tcp_handle, (off_t) 0, 1);
	if (pos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			pos += rstrm->out_finger - rstrm->out_base;
			break;

		case XDR_DECODE:
			pos -= rstrm->in_boundry - rstrm->in_finger;
			break;

		default:
			pos = -1;
			break;
		}
	return ((u_int) pos);
}

static bool_t
xdrrec_setpos(xdrs, pos)
	XDR *xdrs;
	u_int pos;
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	u_int currpos = xdrrec_getpos(xdrs);
	int delta = currpos - pos;
	caddr_t newpos;

	if ((int)currpos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			newpos = rstrm->out_finger - delta;
			if ((newpos > (caddr_t)(rstrm->frag_header)) &&
				(newpos < rstrm->out_boundry)) {
				rstrm->out_finger = newpos;
				return (TRUE);
			}
			break;

		case XDR_DECODE:
			newpos = rstrm->in_finger - delta;
			if ((delta < (int)(rstrm->fbtbc)) &&
				(newpos <= rstrm->in_boundry) &&
				(newpos >= rstrm->in_base)) {
				rstrm->in_finger = newpos;
				rstrm->fbtbc -= delta;
				return (TRUE);
			}
			break;
		}
	return (FALSE);
}

static int32_t *
xdrrec_inline(xdrs, len)
	XDR *xdrs;
	int len;
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	int32_t * buf = NULL;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		if ((rstrm->out_finger + len) <= rstrm->out_boundry) {
			buf = (int32_t *) rstrm->out_finger;
			rstrm->out_finger += len;
		}
		break;

	case XDR_DECODE:
		if ((len <= rstrm->fbtbc) &&
			((rstrm->in_finger + len) <= rstrm->in_boundry)) {
			buf = (int32_t *) rstrm->in_finger;
			rstrm->fbtbc -= len;
			rstrm->in_finger += len;
		}
		break;
	}
	return (buf);
}

static void
xdrrec_destroy(xdrs)
	XDR *xdrs;
{
	RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;

	mem_free(rstrm->the_buffer,
		rstrm->sendsize + rstrm->recvsize + BYTES_PER_XDR_UNIT);
	mem_free((caddr_t)rstrm, sizeof(RECSTREAM));
}


/*
 * Exported routines to manage xdr records
 */

/*
 * Before reading (deserializing from the stream, one should always call
 * this procedure to guarantee proper record alignment.
 */
bool_t
xdrrec_skiprecord(xdrs)
	XDR *xdrs;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (FALSE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (FALSE);
	}
	rstrm->last_frag = FALSE;
	return (TRUE);
}

/*
 * Look ahead fuction.
 * Returns TRUE iff there is no more input in the buffer
 * after consuming the rest of the current record.
 */
bool_t
xdrrec_eof(xdrs)
	XDR *xdrs;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc))
			return (TRUE);
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm)))
			return (TRUE);
	}
	if (rstrm->in_finger == rstrm->in_boundry)
		return (TRUE);
	return (FALSE);
}

/*
 * The client must tell the package when an end-of-record has occurred.
 * The second paraemters tells whether the record should be flushed to the
 * (output) tcp stream.  (This let's the package support batched or
 * pipelined procedure calls.)  TRUE => immmediate flush to tcp connection.
 */
bool_t
xdrrec_endofrecord(xdrs, sendnow)
	XDR *xdrs;
	bool_t sendnow;
{
	RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	u_long len;  /* fragment length */

	if (sendnow || rstrm->frag_sent ||
		((u_long)rstrm->out_finger + sizeof(u_int32_t) >=
		(u_long)rstrm->out_boundry)) {
		rstrm->frag_sent = FALSE;
		return (flush_out(rstrm, TRUE));
	}
	len = (u_long)(rstrm->out_finger) - (u_long)(rstrm->frag_header) -
	   sizeof(u_int32_t);
	*(rstrm->frag_header) = htonl((u_long)len | LAST_FRAG);
	rstrm->frag_header = (u_int32_t *)rstrm->out_finger;
	rstrm->out_finger += sizeof(u_int32_t);
	return (TRUE);
}


/*
 * Internal useful routines
 */
static bool_t
flush_out(rstrm, eor)
	RECSTREAM *rstrm;
	bool_t eor;
{
	u_long eormask = (eor == TRUE) ? LAST_FRAG : 0;
	u_int32_t len = (u_long)(rstrm->out_finger) -
		(u_long)(rstrm->frag_header) - sizeof(u_int32_t);

	*(rstrm->frag_header) = htonl(len | eormask);
	len = (u_long)(rstrm->out_finger) - (u_long)(rstrm->out_base);
	if ((*(rstrm->writeit))(rstrm->tcp_handle, rstrm->out_base, (int)len)
		!= (int)len)
		return (FALSE);
	rstrm->frag_header = (u_int32_t *)rstrm->out_base;
	rstrm->out_finger = (caddr_t)rstrm->out_base + sizeof(u_int32_t);
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
fill_input_buf(rstrm)
	RECSTREAM *rstrm;
{
	caddr_t where;
	u_long i;
	long len;

	where = rstrm->in_base;
	i = (u_long)rstrm->in_boundry % BYTES_PER_XDR_UNIT;
	where += i;
	len = rstrm->in_size - i;
	if ((len = (*(rstrm->readit))(rstrm->tcp_handle, where, len)) == -1)
		return (FALSE);
	rstrm->in_finger = where;
	where += len;
	rstrm->in_boundry = where;
	return (TRUE);
}

static bool_t  /* knows nothing about records!  Only about input buffers */
get_input_bytes(rstrm, addr, len)
	RECSTREAM *rstrm;
	caddr_t addr;
	int len;
{
	long current;

	while (len > 0) {
		current = (long)rstrm->in_boundry - (long)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (len < current) ? len : current;
		memcpy(addr, rstrm->in_finger, current);
		rstrm->in_finger += current;
		addr += current;
		len -= current;
	}
	return (TRUE);
}

static bool_t  /* next two bytes of the input stream are treated as a header */
set_input_fragment(rstrm)
	RECSTREAM *rstrm;
{
	u_int32_t header;

	if (! get_input_bytes(rstrm, (caddr_t)&header, sizeof(header)))
		return (FALSE);
	header = (long)ntohl(header);
	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
	/*
	 * Sanity check. Try not to accept wildly incorrect
	 * record sizes. Unfortunately, the only record size
	 * we can positively identify as being 'wildly incorrect'
	 * is zero. Ridiculously large record sizes may look wrong,
	 * but we don't have any way to be certain that they aren't
	 * what the client actually intended to send us.
	 */
	if (header == 0)
		return(FALSE);
	rstrm->fbtbc = header & (~LAST_FRAG);
	return (TRUE);
}

static bool_t  /* consumes input bytes; knows nothing about records! */
skip_input_bytes(rstrm, cnt)
	RECSTREAM *rstrm;
	long cnt;
{
	long current;

	while (cnt > 0) {
		current = (long)rstrm->in_boundry - (long)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm))
				return (FALSE);
			continue;
		}
		current = (cnt < current) ? cnt : current;
		rstrm->in_finger += current;
		cnt -= current;
	}
	return (TRUE);
}

static u_int
fix_buf_size(s)
	u_int s;
{

	if (s < 100)
		s = 4000;
	return (RNDUP(s));
}
