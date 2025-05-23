/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/netncp/ncp_subr.c,v 1.2.2.1 2001/02/22 08:54:11 bp Exp $
 * $DragonFly: src/sys/netproto/ncp/ncp_subr.c,v 1.8 2005/02/28 16:23:00 joerg Exp $
 */
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/mbuf.h>

#include "ncp.h"
#include "ncp_conn.h"
#include "ncp_sock.h"
#include "ncp_subr.h"
#include "ncp_rq.h"
#include "ncp_ncp.h"
#include "nwerror.h"

int ncp_debuglevel = 0;

struct callout ncp_timer_handle;

static void ncp_at_exit(struct thread *td);
static void ncp_timer(void *arg);

/*
 * duplicate string from user space. It should be very-very slow.
 */
char *
ncp_str_dup(const char *s) 
{
	const char *src;
	char *dst;
	char bt;
	int len = 0;

	for (src = s; ;src++) {
		if (copyin(src, &bt, 1)) return NULL;
		len++;
		if (bt == 0) break;
	}
	dst = malloc(len, M_NCPDATA, M_WAITOK);
	copyin(s, dst, len);
	return(dst);
}


void
ncp_at_exit(struct thread *td)
{
	struct ncp_conn *ncp, *nncp;
	struct ucred *cred;

	KKASSERT(td->td_proc);
	cred = td->td_proc->p_ucred;

	if (ncp_conn_putprochandles(td) == 0) return;

	ncp_conn_locklist(LK_EXCLUSIVE, td);
	for (ncp = SLIST_FIRST(&conn_list); ncp; ncp = nncp) {
		nncp = SLIST_NEXT(ncp, nc_next);
		if (ncp->ref_cnt != 0) continue;
		if (ncp_conn_lock(ncp, td, cred, NCPM_READ|NCPM_EXECUTE|NCPM_WRITE))
			continue;
		if (ncp_disconnect(ncp) != 0)
			ncp_conn_unlock(ncp, td);
	}
	ncp_conn_unlocklist(td);
}

int
ncp_init(void) 
{
	ncp_conn_init();
	if (at_exit(ncp_at_exit)) {
		NCPFATAL("can't register at_exit handler\n");
		return ENOMEM;
	}
	callout_init(&ncp_timer_handle);
	callout_reset(&ncp_timer_handle, NCP_TIMER_TICK, ncp_timer, NULL);
	return 0;
}

void
ncp_done(void) 
{
	struct ncp_conn *ncp, *nncp;
	struct thread *td = curthread;	/* XXX */
	struct ucred *cred;

	KKASSERT(td->td_proc);
	cred = td->td_proc->p_ucred;

	callout_stop(&ncp_timer_handle);
	rm_at_exit(ncp_at_exit);
	ncp_conn_locklist(LK_EXCLUSIVE, td);
	for (ncp = SLIST_FIRST(&conn_list); ncp; ncp = nncp) {
		nncp = SLIST_NEXT(ncp, nc_next);
		ncp->ref_cnt = 0;
		if (ncp_conn_lock(ncp, td, cred,NCPM_READ|NCPM_EXECUTE|NCPM_WRITE)) {
			NCPFATAL("Can't lock connection !\n");
			continue;
		}
		if (ncp_disconnect(ncp) != 0)
			ncp_conn_unlock(ncp, td);
	}
	ncp_conn_unlocklist(td);
}


/* tick every second and check for watch dog packets and lost connections */
static void
ncp_timer(void *arg)
{
	struct ncp_conn *conn;

	if(ncp_conn_locklist(LK_SHARED | LK_NOWAIT, NULL) == 0) {
		SLIST_FOREACH(conn, &conn_list, nc_next)
			ncp_check_conn(conn);
		ncp_conn_unlocklist(NULL);
	}
	callout_reset(&ncp_timer_handle, NCP_TIMER_TICK, ncp_timer, NULL);
}

int
ncp_get_bindery_object_id(struct ncp_conn *conn, 
		u_int16_t object_type, char *object_name, 
		struct ncp_bindery_object *target,
		struct thread *td,struct ucred *cred)
{
	int error;
	DECLARE_RQ;

	NCP_RQ_HEAD_S(23,53,td,cred);
	ncp_rq_word_hl(rqp, object_type);
	ncp_rq_pstring(rqp, object_name);
	checkbad(ncp_request(conn,rqp));
	if (rqp->rpsize < 54) {
		printf("ncp_rp_size %d < 54\n", rqp->rpsize);
		error = EINVAL;
		goto bad;
	}
	target->object_id = ncp_rp_dword_hl(rqp);
	target->object_type = ncp_rp_word_hl(rqp);
	ncp_rp_mem(rqp,(caddr_t)target->object_name, 48);
	NCP_RQ_EXIT;
	return error;
}

int
ncp_read(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred) {
	int error = 0, len = 0, retlen=0, tsiz, burstio;
	DECLARE_RQ;

	tsiz = uiop->uio_resid;
#ifdef NCPBURST
	burstio = (ncp_burst_enabled && tsiz > conn->buffer_size);
#else
	burstio = 0;
#endif

	while (tsiz > 0) {
		if (!burstio) {
			len = min(4096 - (uiop->uio_offset % 4096), tsiz);
			len = min(len, conn->buffer_size);
			NCP_RQ_HEAD(72,uiop->uio_td,cred);
			ncp_rq_byte(rqp, 0);
			ncp_rq_mem(rqp, (caddr_t)file, 6);
			ncp_rq_dword(rqp, htonl(uiop->uio_offset));
			ncp_rq_word(rqp, htons(len));
			checkbad(ncp_request(conn,rqp));
			retlen = ncp_rp_word_hl(rqp);
			if (uiop->uio_offset & 1)
				ncp_rp_byte(rqp);
			error = nwfs_mbuftouio(&rqp->mrp,uiop,retlen,&rqp->bpos);
			NCP_RQ_EXIT;
		} else {
#ifdef NCPBURST
			error = ncp_burst_read(conn, file, tsiz, &len, &retlen, uiop, cred);
#endif
		}
		if (error) break;
		tsiz -= retlen;
		if (retlen < len)
			break;
	}
	return (error);
}

int
ncp_write(struct ncp_conn *conn, ncp_fh *file, struct uio *uiop, struct ucred *cred)
{
	int error = 0, len, tsiz, backup;
	DECLARE_RQ;

	if (uiop->uio_iovcnt != 1) {
		printf("%s: can't handle iovcnt>1 !!!\n", __func__);
		return EIO;
	}
	tsiz = uiop->uio_resid;
	while (tsiz > 0) {
		len = min(4096 - (uiop->uio_offset % 4096), tsiz);
		len = min(len, conn->buffer_size);
		if (len == 0) {
			printf("gotcha!\n");
		}
		/* rq head */
		NCP_RQ_HEAD(73,uiop->uio_td,cred);
		ncp_rq_byte(rqp, 0);
		ncp_rq_mem(rqp, (caddr_t)file, 6);
		ncp_rq_dword(rqp, htonl(uiop->uio_offset));
		ncp_rq_word_hl(rqp, len);
		nwfs_uiotombuf(uiop,&rqp->mrq,len,&rqp->bpos);
		checkbad(ncp_request(conn,rqp));
		if (len == 0)
			break;
		NCP_RQ_EXIT;
		if (error) {
			backup = len;
			uiop->uio_iov->iov_base -= backup;
			uiop->uio_iov->iov_len += backup;
			uiop->uio_offset -= backup;
			uiop->uio_resid += backup;
			break;
		}
		tsiz -= len;
	}
	if (error)
		uiop->uio_resid = tsiz;
	switch (error) {
	    case NWE_INSUFFICIENT_SPACE:
		error = ENOSPC;
		break;
	}
	return (error);
}
