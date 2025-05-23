/*
 * Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * BASED ON:
 * -------------------------------------------------------------------------
 *
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
 *
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 */

/*
 * $FreeBSD: src/sys/net/if_tapvar.h,v 1.3.2.1 2000/07/27 13:57:05 nsayer Exp $
 * $DragonFly: src/sys/net/tap/if_tapvar.h,v 1.3 2003/06/25 03:56:02 dillon Exp $
 * $Id: if_tapvar.h,v 0.6 2000/07/11 02:16:08 max Exp $
 */

#ifndef _NET_IF_TAPVAR_H_
#define _NET_IF_TAPVAR_H_

struct tap_softc {
	struct arpcom	arpcom;			/* ethernet common data      */
#define tap_if		arpcom.ac_if
	dev_t		tap_dev;		/* device                    */

	u_short		tap_flags;		/* misc flags                */
#define	TAP_OPEN	(1 << 0)
#define	TAP_INITED	(1 << 1)
#define	TAP_RWAIT	(1 << 2)
#define	TAP_ASYNC	(1 << 3)
#define TAP_READY       (TAP_OPEN|TAP_INITED)
#define	TAP_VMNET	(1 << 4)

	u_int8_t 	ether_addr[ETHER_ADDR_LEN]; /* ether addr of the remote side */

	struct thread	 *tap_td;		/* thread of process to open  */
	struct sigio	*tap_sigio;		/* information for async I/O */
	struct selinfo	 tap_rsel;		/* read select               */
};

#endif /* !_NET_IF_TAPVAR_H_ */
