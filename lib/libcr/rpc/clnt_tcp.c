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
 * @(#)clnt_tcp.c 1.37 87/10/05 Copyr 1984 Sun Micro
 * @(#)clnt_tcp.c	2.2 88/08/01 4.0 RPCSRC
 * $FreeBSD: src/lib/libc/rpc/clnt_tcp.c,v 1.14 2000/01/27 23:06:36 jasone Exp $
 * $DragonFly: src/lib/libcr/rpc/Attic/clnt_tcp.c,v 1.3 2004/10/25 19:38:25 drhodus Exp $
 */

/*
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>

#define MCALL_MSG_SIZE 24

static int	readtcp();
static int	writetcp();

static enum clnt_stat	clnttcp_call();
static void		clnttcp_abort();
static void		clnttcp_geterr();
static bool_t		clnttcp_freeres();
static bool_t           clnttcp_control();
static void		clnttcp_destroy();

static struct clnt_ops tcp_ops = {
	clnttcp_call,
	clnttcp_abort,
	clnttcp_geterr,
	clnttcp_freeres,
	clnttcp_destroy,
	clnttcp_control
};

struct ct_data {
	int		ct_sock;
	bool_t		ct_closeit;
	struct timeval	ct_wait;
	bool_t          ct_waitset;       /* wait set by clnt_control? */
	struct sockaddr_in ct_addr;
	struct rpc_err	ct_error;
	char		ct_mcall[MCALL_MSG_SIZE];	/* marshalled callmsg */
	u_int		ct_mpos;			/* pos after marshal */
	XDR		ct_xdrs;
};

/*
 * Create a client handle for a tcp/ip connection.
 * If *sockp<0, *sockp is set to a newly created TCP socket and it is
 * connected to raddr.  If *sockp non-negative then
 * raddr is ignored.  The rpc/tcp package does buffering
 * similar to stdio, so the client must pick send and receive buffer sizes,];
 * 0 => use the default.
 * If raddr->sin_port is 0, then a binder on the remote machine is
 * consulted for the right port number.
 * NB: *sockp is copied into a private area.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is set null authentication.  Caller may wish to set this
 * something more useful.
 */
CLIENT *
clnttcp_create(raddr, prog, vers, sockp, sendsz, recvsz)
	struct sockaddr_in *raddr;
	u_long prog;
	u_long vers;
	int *sockp;
	u_int sendsz;
	u_int recvsz;
{
	CLIENT *h;
	struct ct_data *ct = NULL;
	struct timeval now;
	struct rpc_msg call_msg;
	static u_int32_t disrupt;

	if (disrupt == 0)
		disrupt = (u_int32_t)(long)raddr;

	h  = (CLIENT *)mem_alloc(sizeof(*h));
	if (h == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	ct = (struct ct_data *)mem_alloc(sizeof(*ct));
	if (ct == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port = pmap_getport(raddr, prog, vers, IPPROTO_TCP)) == 0) {
			mem_free((caddr_t)ct, sizeof(struct ct_data));
			mem_free((caddr_t)h, sizeof(CLIENT));
			return ((CLIENT *)NULL);
		}
		raddr->sin_port = htons(port);
	}

	/*
	 * If no socket given, open one
	 */
	if (*sockp < 0) {
		*sockp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		(void)bindresvport(*sockp, (struct sockaddr_in *)0);
		if ((*sockp < 0)
		    || (connect(*sockp, (struct sockaddr *)raddr,
		    sizeof(*raddr)) < 0)) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
			if (*sockp != -1)
				(void)_close(*sockp);
			goto fooy;
		}
		ct->ct_closeit = TRUE;
	} else {
		ct->ct_closeit = FALSE;
	}

	/*
	 * Set up private data struct
	 */
	ct->ct_sock = *sockp;
	ct->ct_wait.tv_usec = 0;
	ct->ct_waitset = FALSE;
	ct->ct_addr = *raddr;

	/*
	 * Initialize call message
	 */
	(void)gettimeofday(&now, (struct timezone *)0);
	call_msg.rm_xid = (++disrupt) ^ getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;

	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_mcall, MCALL_MSG_SIZE,
	    XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		if (ct->ct_closeit) {
			(void)_close(*sockp);
		}
		goto fooy;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz,
	    (caddr_t)ct, readtcp, writetcp);
	h->cl_ops = &tcp_ops;
	h->cl_private = (caddr_t) ct;
	h->cl_auth = authnone_create();
	return (h);

fooy:
	/*
	 * Something goofed, free stuff and barf
	 */
	if (ct)
		mem_free((caddr_t)ct, sizeof(struct ct_data));
	if (h)
		mem_free((caddr_t)h, sizeof(CLIENT));
	return ((CLIENT *)NULL);
}

static enum clnt_stat
clnttcp_call(h, proc, xdr_args, args_ptr, xdr_results, results_ptr, timeout)
	CLIENT *h;
	u_long proc;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
	xdrproc_t xdr_results;
	caddr_t results_ptr;
	struct timeval timeout;
{
	struct ct_data *ct = (struct ct_data *) h->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	u_long x_id;
	u_int32_t *msg_x_id = (u_int32_t *)(ct->ct_mcall);	/* yuk */
	bool_t shipnow;
	int refreshes = 2;

	if (!ct->ct_waitset) {
		ct->ct_wait = timeout;
	}

	shipnow =
	    (xdr_results == (xdrproc_t)0 && timeout.tv_sec == 0
	    && timeout.tv_usec == 0) ? FALSE : TRUE;

call_again:
	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	x_id = ntohl(--(*msg_x_id));
	if ((! XDR_PUTBYTES(xdrs, ct->ct_mcall, ct->ct_mpos)) ||
	    (! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xdr_args)(xdrs, args_ptr))) {
		if (ct->ct_error.re_status == RPC_SUCCESS)
			ct->ct_error.re_status = RPC_CANTENCODEARGS;
		(void)xdrrec_endofrecord(xdrs, TRUE);
		return (ct->ct_error.re_status);
	}
	if (! xdrrec_endofrecord(xdrs, shipnow))
		return (ct->ct_error.re_status = RPC_CANTSEND);
	if (! shipnow)
		return (RPC_SUCCESS);
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		return(ct->ct_error.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	while (TRUE) {
		reply_msg.acpted_rply.ar_verf = _null_auth;
		reply_msg.acpted_rply.ar_results.where = NULL;
		reply_msg.acpted_rply.ar_results.proc = xdr_void;
		if (! xdrrec_skiprecord(xdrs))
			return (ct->ct_error.re_status);
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, &reply_msg)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				continue;
			return (ct->ct_error.re_status);
		}
		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	_seterr_reply(&reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &reply_msg.acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! (*xdr_results)(xdrs, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
		/* free verifier ... */
		if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(reply_msg.acpted_rply.ar_verf));
		}
	}  /* end successful completion */
	else {
		/* maybe our credentials need to be refreshed ... */
		if (refreshes-- && AUTH_REFRESH(h->cl_auth))
			goto call_again;
	}  /* end of unsuccessful completion */
	return (ct->ct_error.re_status);
}

static void
clnttcp_geterr(h, errp)
	CLIENT *h;
	struct rpc_err *errp;
{
	struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	*errp = ct->ct_error;
}

static bool_t
clnttcp_freeres(cl, xdr_res, res_ptr)
	CLIENT *cl;
	xdrproc_t xdr_res;
	caddr_t res_ptr;
{
	struct ct_data *ct = (struct ct_data *)cl->cl_private;
	XDR *xdrs = &(ct->ct_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

static void
clnttcp_abort()
{
}


static bool_t
clnttcp_control(cl, request, info)
	CLIENT *cl;
	int request;
	char *info;
{
	struct ct_data *ct = (struct ct_data *)cl->cl_private;
	struct timeval *tv;
	int len;

	switch (request) {
	case CLSET_FD_CLOSE:
		ct->ct_closeit = TRUE;
		break;
	case CLSET_FD_NCLOSE:
		ct->ct_closeit = FALSE;
		break;
	case CLSET_TIMEOUT:
		if (info == NULL)
			return(FALSE);
		tv = (struct timeval *)info;
		ct->ct_wait.tv_sec = tv->tv_sec;
		ct->ct_wait.tv_usec = tv->tv_usec;
		ct->ct_waitset = TRUE;
		break;
	case CLGET_TIMEOUT:
		if (info == NULL)
			return(FALSE);
		*(struct timeval *)info = ct->ct_wait;
		break;
	case CLGET_SERVER_ADDR:
		if (info == NULL)
			return(FALSE);
		*(struct sockaddr_in *)info = ct->ct_addr;
		break;
	case CLGET_FD:
		if (info == NULL)
			return(FALSE);
		*(int *)info = ct->ct_sock;
		break;
	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure *.
		 * This will get the xid of the PREVIOUS call
		 */
		if (info == NULL)
			return(FALSE);
		*(u_long *)info = ntohl(*(u_long *)ct->ct_mcall);
		break;
	case CLSET_XID:
		/* This will set the xid of the NEXT call */
		if (info == NULL)
			return(FALSE);
		*(u_long *)ct->ct_mcall =  htonl(*(u_long *)info - 1);
		/* decrement by 1 as clnttcp_call() increments once */
	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		if (info == NULL)
			return(FALSE);
		*(u_long *)info = ntohl(*(u_long *)(ct->ct_mcall +
						4 * BYTES_PER_XDR_UNIT));
		break;
	case CLSET_VERS:
		if (info == NULL)
			return(FALSE);
		*(u_long *)(ct->ct_mcall + 4 * BYTES_PER_XDR_UNIT)
				= htonl(*(u_long *)info);
		break;
	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the  field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		if (info == NULL)
			return(FALSE);
		*(u_long *)info = ntohl(*(u_long *)(ct->ct_mcall +
						3 * BYTES_PER_XDR_UNIT));
		break;
	case CLSET_PROG:
		if (info == NULL)
			return(FALSE);
		*(u_long *)(ct->ct_mcall + 3 * BYTES_PER_XDR_UNIT)
				= htonl(*(u_long *)info);
		break;
	case CLGET_LOCAL_ADDR:
		len = sizeof(struct sockaddr);
		if (getsockname(ct->ct_sock, (struct sockaddr *)info, &len) <0)
			return(FALSE);
		break;
	case CLGET_RETRY_TIMEOUT:
	case CLSET_RETRY_TIMEOUT:
	case CLGET_SVC_ADDR:
	case CLSET_SVC_ADDR:
	case CLSET_PUSH_TIMOD:
	case CLSET_POP_TIMOD:
	default:
		return (FALSE);
	}
	return (TRUE);
}


static void
clnttcp_destroy(h)
	CLIENT *h;
{
	struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	if (ct->ct_closeit) {
		(void)_close(ct->ct_sock);
	}
	XDR_DESTROY(&(ct->ct_xdrs));
	mem_free((caddr_t)ct, sizeof(struct ct_data));
	mem_free((caddr_t)h, sizeof(CLIENT));
}

/*
 * Interface between xdr serializer and tcp connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
readtcp(ct, buf, len)
	struct ct_data *ct;
	caddr_t buf;
	int len;
{
	fd_set *fds, readfds;
	struct timeval start, after, duration, delta, tmp, tv;
	int r, save_errno;

	if (len == 0)
		return (0);

	if (ct->ct_sock + 1 > FD_SETSIZE) {
		int bytes = howmany(ct->ct_sock + 1, NFDBITS) * sizeof(fd_mask);
		fds = (fd_set *)malloc(bytes);
		if (fds == NULL)
			return (-1);
		memset(fds, 0, bytes);
	} else {
		fds = &readfds;
		FD_ZERO(fds);
	}

	gettimeofday(&start, NULL);
	delta = ct->ct_wait;
	while (TRUE) {
		/* XXX we know the other bits are still clear */
		FD_SET(ct->ct_sock, fds);
		tv = delta;	/* in case select writes back */
		r = select(ct->ct_sock+1, fds, NULL, NULL, &tv);
		save_errno = errno;

		gettimeofday(&after, NULL);
		timersub(&start, &after, &duration);
		timersub(&ct->ct_wait, &duration, &tmp);
		delta = tmp;
		if (delta.tv_sec < 0 || !timerisset(&delta))
			r = 0;

		switch (r) {
		case 0:
			if (fds != &readfds)
				free(fds);
			ct->ct_error.re_status = RPC_TIMEDOUT;
			return (-1);

		case -1:
			if (errno == EINTR)
				continue;
			if (fds != &readfds)
				free(fds);
			ct->ct_error.re_status = RPC_CANTRECV;
			ct->ct_error.re_errno = save_errno;
			return (-1);
		}
		break;
	}
	switch (len = _read(ct->ct_sock, buf, len)) {

	case 0:
		/* premature eof */
		ct->ct_error.re_errno = ECONNRESET;
		ct->ct_error.re_status = RPC_CANTRECV;
		len = -1;  /* it's really an error */
		break;

	case -1:
		ct->ct_error.re_errno = errno;
		ct->ct_error.re_status = RPC_CANTRECV;
		break;
	}
	return (len);
}

static int
writetcp(ct, buf, len)
	struct ct_data *ct;
	caddr_t buf;
	int len;
{
	int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = _write(ct->ct_sock, buf, cnt)) == -1) {
			ct->ct_error.re_errno = errno;
			ct->ct_error.re_status = RPC_CANTSEND;
			return (-1);
		}
	}
	return (len);
}
