/*
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)res_mkquery.c	8.1 (Berkeley) 6/4/93
 * $From: Id: res_mkquery.c,v 8.9 1997/04/24 22:22:36 vixie Exp $
 * $FreeBSD: src/lib/libc/net/res_mkquery.c,v 1.15.2.2 2002/09/20 10:45:35 nectar Exp $
 * $DragonFly: src/lib/libcr/net/Attic/res_mkquery.c,v 1.3 2004/10/25 19:38:25 drhodus Exp $
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include "res_config.h"

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
int
res_mkquery(op, dname, class, type, data, datalen, newrr_in, buf, buflen)
	int op;			/* opcode of query */
	const char *dname;	/* domain name */
	int class, type;	/* class and type of query */
	const u_char *data;	/* resource record data */
	int datalen;		/* length of data */
	const u_char *newrr_in;	/* new rr for modify or append */
	u_char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
{
	HEADER *hp;
	u_char *cp;
	int n;
	u_char *dnptrs[20], **dpp, **lastdnptr;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}
#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_mkquery(%d, %s, %d, %d)\n",
		       op, dname, class, type);
#endif
	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < HFIXEDSZ))
		return (-1);
	memset(buf, 0, HFIXEDSZ);
	hp = (HEADER *) buf;
	hp->id = htons(++_res.id);
	hp->opcode = op;
	hp->rd = (_res.options & RES_RECURSE) != 0;
	hp->rcode = NOERROR;
	cp = buf + HFIXEDSZ;
	buflen -= HFIXEDSZ;
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];
	/*
	 * perform opcode specific processing
	 */
	switch (op) {
	case QUERY:	/*FALLTHROUGH*/
	case NS_NOTIFY_OP:
		if ((buflen -= QFIXEDSZ) < 0)
			return (-1);
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(type, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		hp->qdcount = htons(1);
		if (op == QUERY || data == NULL)
			break;
		/*
		 * Make an additional record for completion domain.
		 */
		buflen -= RRFIXEDSZ;
		n = dn_comp((char *)data, cp, buflen, dnptrs, lastdnptr);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(T_NULL, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		__putlong(0, cp);
		cp += INT32SZ;
		__putshort(0, cp);
		cp += INT16SZ;
		hp->arcount = htons(1);
		break;

	case IQUERY:
		/*
		 * Initialize answer section
		 */
		if (buflen < 1 + RRFIXEDSZ + datalen)
			return (-1);
		*cp++ = '\0';	/* no domain name */
		__putshort(type, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		__putlong(0, cp);
		cp += INT32SZ;
		__putshort(datalen, cp);
		cp += INT16SZ;
		if (datalen) {
			memcpy(cp, data, datalen);
			cp += datalen;
		}
		hp->ancount = htons(1);
		break;

	default:
		return (-1);
	}
	return (cp - buf);
}

/*
 * Weak aliases for applications that use certain private entry points,
 * and fail to include <resolv.h>.
 */
#undef res_mkquery
__weak_reference(__res_mkquery, res_mkquery);

/* attach OPT pseudo-RR, as documented in RFC2671 (EDNS0). */
int
res_opt(n0, buf, buflen, anslen)
	int n0;
	u_char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
	int anslen;		/* answer buffer length */
{
	HEADER *hp;
	u_char *cp;

	hp = (HEADER *) buf;
	cp = buf + n0;
	buflen -= n0;

	if (buflen < 1 + RRFIXEDSZ)
		return -1;

	*cp++ = 0;	/* "." */
	buflen--;

	__putshort(T_OPT, cp);	/* TYPE */
	cp += INT16SZ;
	if (anslen > 0xffff)
		anslen = 0xffff;		/* limit to 16bit value */
	__putshort(anslen & 0xffff, cp);	/* CLASS = UDP payload size */
	cp += INT16SZ;
	*cp++ = NOERROR;	/* extended RCODE */
	*cp++ = 0;		/* EDNS version */
	__putshort(0, cp);	/* MBZ */
	cp += INT16SZ;
	__putshort(0, cp);	/* RDLEN */
	cp += INT16SZ;
	hp->arcount = htons(ntohs(hp->arcount) + 1);
	buflen -= RRFIXEDSZ;

	return cp - buf;
}
