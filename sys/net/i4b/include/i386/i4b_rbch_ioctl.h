/*
 * Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_rbch_ioctl.h raw B-channel driver interface ioctls
 *	------------------------------------------------------
 *
 *	$Id: i4b_rbch_ioctl.h,v 1.2 1999/12/13 21:25:28 hm Exp $
 *
 * $FreeBSD: src/sys/i386/include/i4b_rbch_ioctl.h,v 1.3.2.1 2001/08/01 17:45:01 obrien Exp $
 * $DragonFly: src/sys/net/i4b/include/i386/i4b_rbch_ioctl.h,v 1.2 2003/06/17 04:28:35 dillon Exp $
 *
 *      last edit-date: [Mon Dec 13 22:07:12 1999]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_RBCH_IOCTL_H_
#define _I4B_RBCH_IOCTL_H_

/*---------------------------------------------------------------------------*
 *	instruct the rbch device to dial the given number
 *---------------------------------------------------------------------------*/

typedef char telno_t[TELNO_MAX];

#define	I4B_RBCH_DIALOUT	_IOW('R', 1, telno_t)

/*---------------------------------------------------------------------------*
 *	request version and release info from kernel part
 *---------------------------------------------------------------------------*/

#define I4B_RBCH_VR_REQ		_IOR('R', 2, msg_vr_req_t)

#endif /* _I4B_RBCH_IOCTL_H_ */
