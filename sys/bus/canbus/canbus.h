/*-
 * Copyright (c) 2000 KIYOHARA Takashi <kiyohara@kk.iij4u.or.jp>
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.FreeBSD.org>
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
 * $FreeBSD: src/sys/pc98/pc98/canbus.h,v 1.3.2.1 2003/02/10 13:11:51 nyan Exp $
 * $DragonFly: src/sys/bus/canbus/Attic/canbus.h,v 1.2 2003/06/17 04:28:55 dillon Exp $
 */

#ifndef _PC98_PC98_CANBUS_H_
#define _PC98_PC98_CANBUS_H_

u_int8_t	canbus_read(device_t, device_t, int);
void		canbus_write(device_t, device_t, int, u_int8_t);
void		canbus_write_multi(
		    device_t, device_t, int, const int, const u_int8_t *);
void		canbus_delay(device_t, device_t);

#endif	/* _PC98_PC98_CANBUS_H_ */
