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
 * @(#)authunix_prot.c 1.15 87/08/11 Copyr 1984 Sun Micro
 * @(#)authunix_prot.c	2.1 88/07/29 4.0 RPCSRC
 * $FreeBSD: src/lib/libc/rpc/authunix_prot.c,v 1.6 1999/08/28 00:00:33 peter Exp $
 * $DragonFly: src/lib/libc/rpc/authunix_prot.c,v 1.3 2004/10/25 19:38:01 drhodus Exp $
 */

/*
 * authunix_prot.c
 * XDR for UNIX style authentication parameters for RPC
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */


#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>

/*
 * XDR for unix authentication parameters.
 */
bool_t
xdr_authunix_parms(xdrs, p)
	XDR *xdrs;
	struct authunix_parms *p;
{

	if (xdr_u_long(xdrs, &(p->aup_time))
	    && xdr_string(xdrs, &(p->aup_machname), MAX_MACHINE_NAME)
	    && xdr_int(xdrs, &(p->aup_uid))
	    && xdr_int(xdrs, &(p->aup_gid))
	    && xdr_array(xdrs, (caddr_t *)&(p->aup_gids),
		    &(p->aup_len), NGRPS, sizeof(int), xdr_int) ) {
		return (TRUE);
	}
	return (FALSE);
}

