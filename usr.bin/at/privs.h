/* 
 *  privs.h - header for privileged operations 
 *  Copyright (C) 1993  Thomas Koenig
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/at/privs.h,v 1.7 1999/12/05 19:57:14 charnier Exp $
 * $DragonFly: src/usr.bin/at/privs.h,v 1.3 2004/09/20 13:11:54 joerg Exp $
 */

#ifndef _PRIVS_H
#define _PRIVS_H

#ifndef _USE_BSD
#define _USE_BSD 1
#include <unistd.h>
#undef _USE_BSD
#else
#include <unistd.h>
#endif

/* Relinquish privileges temporarily for a setuid or setgid program
 * with the option of getting them back later.  This is done by swapping
 * the real and effective userid BSD style.  Call RELINQUISH_PRIVS once
 * at the beginning of the main program.  This will cause all operations
 * to be executed with the real userid.  When you need the privileges
 * of the setuid/setgid invocation, call PRIV_START; when you no longer
 * need it, call PRIV_END.  Note that it is an error to call PRIV_START
 * and not PRIV_END within the same function.
 *
 * Use RELINQUISH_PRIVS_ROOT(a,b) if your program started out running
 * as root, and you want to drop back the effective userid to a
 * and the effective group id to b, with the option to get them back
 * later.
 *
 * If you no longer need root privileges, but those of some other
 * userid/groupid, you can call REDUCE_PRIV(a,b) when your effective
 * is the user's.
 *
 * Problems: Do not use return between PRIV_START and PRIV_END; this
 * will cause the program to continue running in an unprivileged
 * state.
 *
 * It is NOT safe to call exec(), system() or popen() with a user-
 * supplied program (i.e. without carefully checking PATH and any
 * library load paths) with relinquished privileges; the called program
 * can acquire them just as easily.  Set both effective and real userid
 * to the real userid before calling any of them.
 */

extern uid_t real_uid, effective_uid;
extern gid_t real_gid, effective_gid;

#define RELINQUISH_PRIVS { \
			      real_uid = getuid(); \
			      effective_uid = geteuid(); \
			      real_gid = getgid(); \
			      effective_gid = getegid(); \
			      setreuid(effective_uid, real_uid); \
			      setregid(effective_gid, real_gid); \
		          }

#define RELINQUISH_PRIVS_ROOT(a,b) { \
			      real_uid = (a); \
			      effective_uid = geteuid(); \
			      real_gid = (b); \
			      effective_gid = getegid(); \
			      setregid(effective_gid, real_gid); \
			      setreuid(effective_uid, real_uid); \
		          }

#define PRIV_START {\
		    setreuid(real_uid, effective_uid); \
		    setregid(real_gid, effective_gid);

#define PRIV_END \
		    setregid(effective_gid, real_gid); \
		    setreuid(effective_uid, real_uid); \
		    }

#define REDUCE_PRIV(a,b) {\
			setreuid(real_uid, effective_uid); \
			setregid(real_gid, effective_gid); \
			effective_uid = (a); \
			effective_gid = (b); \
			setregid(effective_gid, real_gid); \
			setreuid(effective_uid, real_uid); \
		    }
#endif
