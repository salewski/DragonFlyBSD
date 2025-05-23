/*
 * Copyright (c) 1998 Mark Newton
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
 * $FreeBSD: src/sys/svr4/svr4.h,v 1.6 2000/01/09 12:29:45 bde Exp $
 * $DragonFly: src/sys/emulation/svr4/Attic/svr4.h,v 1.2 2003/06/17 04:28:57 dillon Exp $
 */

#include "opt_svr4.h"

#if !defined(_SVR4_H)
#define _SVR4_H

extern struct sysentvec svr4_sysvec;

#define memset(x,y,z) bzero(x,z)

#define COMPAT_SVR4_SOLARIS2
#define KTRACE

/* These are currently unimplemented (see svr4_ipc.c) */
#if defined(SYSVMSG)
# undef SYSVMSG
#endif
#if defined(SYSVSHM)
# undef SYSVSHM
#endif
#if defined(SYSVSEM)
# undef SYSVSEM
#endif

#endif
