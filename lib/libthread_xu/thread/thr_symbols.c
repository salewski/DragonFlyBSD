/*
 * Copyright (c) 2004 David Xu <davidxu@freebsd.org>
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/lib/libpthread/thread/thr_symbols.c,v 1.1 2004/08/16 03:25:07 davidxu Exp $
 * $DragonFly: src/lib/libthread_xu/thread/thr_symbols.c,v 1.2 2005/03/29 19:26:20 joerg Exp $
 */

#include <stddef.h>
#include <pthread.h>
#ifndef __DragonFly__
#include <rtld.h>
#endif

#include "thr_private.h"

/* A collection of symbols needed by debugger */

int _thread_off_tcb = offsetof(struct pthread, tcb);
int _thread_off_tid = offsetof(struct pthread, tid);
int _thread_off_next = offsetof(struct pthread, tle.tqe_next);
int _thread_off_attr_flags = offsetof(struct pthread, attr.flags);
int _thread_off_thr_locklevel = offsetof(struct pthread, locklevel);
int _thread_off_isdead = offsetof(struct pthread, terminated);
int _thread_size_key = sizeof(struct pthread_key);
int _thread_off_key_allocated = offsetof(struct pthread_key, allocated);
int _thread_off_key_destructor = offsetof(struct pthread_key, destructor);
int _thread_max_keys = PTHREAD_KEYS_MAX;
int _thread_off_state = offsetof(struct pthread, state);
int _thread_state_running = PS_RUNNING;
int _thread_state_zoombie = PS_DEAD;
#ifndef __DragonFly__
int _thread_off_linkmap = offsetof(Obj_Entry, linkmap);
int _thread_off_tlsindex = offsetof(Obj_Entry, tlsindex);
#endif
