/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/svr4/svr4_filio.h,v 1.3 1999/08/28 00:51:16 peter Exp $
 * $DragonFly: src/sys/emulation/svr4/Attic/svr4_filio.h,v 1.2 2003/06/17 04:28:57 dillon Exp $
 */

#ifndef	_SVR4_FILIO_H_
#define	_SVR4_FILIO_H_

#define SVR4_FIOC	('f' << 8)

#define	SVR4_FIOCLEX	SVR4_IO('f', 1)
#define	SVR4_FIONCLEX	SVR4_IO('f', 2)

#define	SVR4_FIOGETOWN	SVR4_IOR('f', 123, int)
#define	SVR4_FIOSETOWN	SVR4_IOW('f', 124, int)
#define	SVR4_FIOASYNC	SVR4_IOW('f', 125, int)
#define	SVR4_FIONBIO	SVR4_IOW('f', 126, int)
#define	SVR4_FIONREAD	SVR4_IOR('f', 127, int)

#endif /* !_SVR4_FILIO_H_ */
