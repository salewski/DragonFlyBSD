/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)unpcb.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD: src/sys/sys/unpcb.h,v 1.9.2.1 2002/03/09 05:22:23 dd Exp $
 * $DragonFly: src/sys/sys/unpcb.h,v 1.3 2004/12/20 11:03:16 joerg Exp $
 */

#ifndef _SYS_UNPCB_H_
#define _SYS_UNPCB_H_

#include <sys/queue.h>
#include <sys/ucred.h>

/*
 * Protocol control block for an active
 * instance of a UNIX internal protocol.
 *
 * A socket may be associated with an vnode in the
 * file system.  If so, the unp_vnode pointer holds
 * a reference count to this vnode, which should be irele'd
 * when the socket goes away.
 *
 * A socket may be connected to another socket, in which
 * case the control block of the socket to which it is connected
 * is given by unp_conn.
 *
 * A socket may be referenced by a number of sockets (e.g. several
 * sockets may be connected to a datagram socket.)  These sockets
 * are in a linked list starting with unp_refs, linked through
 * unp_nextref and null-terminated.  Note that a socket may be referenced
 * by a number of other sockets and may also reference a socket (not
 * necessarily one which is referencing it).  This generates
 * the need for unp_refs and unp_nextref to be separate fields.
 *
 * Stream sockets keep copies of receive sockbuf sb_cc and sb_mbcnt
 * so that changes in the sockbuf may be computed to modify
 * back pressure on the sender accordingly.
 */
typedef	u_quad_t	unp_gen_t;
LIST_HEAD(unp_head, unpcb);

struct	unpcb {
	LIST_ENTRY(unpcb) unp_link; 	/* glue on list of all PCBs */
	struct	socket *unp_socket;	/* pointer back to socket */
	struct	vnode *unp_vnode;	/* if associated with file */
	struct	vnode *unp_rvnode;	/* root vp for creating process */
	ino_t	unp_ino;		/* fake inode number */
	struct	unpcb *unp_conn;	/* control block of connected socket */
	struct	unp_head unp_refs;	/* referencing socket linked list */
	LIST_ENTRY(unpcb) unp_reflink;	/* link in unp_refs list */
	struct	sockaddr_un *unp_addr;	/* bound address of socket */
	int	unp_cc;			/* copy of rcv.sb_cc */
	int	unp_mbcnt;		/* copy of rcv.sb_mbcnt */
	unp_gen_t unp_gencnt;		/* generation count of this instance */
	int	unp_flags;		/* flags */
	struct	xucred unp_peercred;	/* peer credentials, if applicable */
};

/*
 * Flags in unp_flags.
 *
 * UNP_HAVEPC - indicates that the unp_peercred member is filled in
 * and is really the credentials of the connected peer.  This is used
 * to determine whether the contents should be sent to the user or
 * not.
 *
 * UNP_HAVEPCCACHED - indicates that the unp_peercred member is filled
 * in, but does *not* contain the credentials of the connected peer
 * (there may not even be a peer).  This is set in unp_listen() when
 * it fills in unp_peercred for later consumption by unp_connect().
 */
#define UNP_HAVEPC			0x001
#define UNP_HAVEPCCACHED		0x002

#define	sotounpcb(so)	((struct unpcb *)((so)->so_pcb))

/* Hack alert -- this structure depends on <sys/socketvar.h>. */
#ifdef	_SYS_SOCKETVAR_H_
struct	xunpcb {
	size_t	xu_len;			/* length of this structure */
	struct	unpcb *xu_unpp;		/* to help netstat, fstat */
	struct	unpcb xu_unp;		/* our information */
	union {
		struct	sockaddr_un xuu_addr;	/* our bound address */
		char	xu_dummy1[256];
	} xu_au;
#define	xu_addr	xu_au.xuu_addr
	union {
		struct	sockaddr_un xuu_caddr; /* their bound address */
		char	xu_dummy2[256];
	} xu_cau;
#define	xu_caddr xu_cau.xuu_caddr
	struct	xsocket	xu_socket;
	u_quad_t	xu_alignment_hack;
};
#endif /* _SYS_SOCKETVAR_H_ */

#endif /* _SYS_UNPCB_H_ */
