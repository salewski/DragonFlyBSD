/*	$NetBSD: extern.h,v 1.5 1998/05/06 13:16:57 mycroft Exp $	*/
/* $FreeBSD: src/usr.sbin/crunch/crunchide/extern.h,v 1.1.6.1 2002/07/25 09:33:17 ru Exp $ */
/* $DragonFly: src/usr.sbin/crunch/crunchide/extern.h,v 1.2 2003/06/17 04:29:53 dillon Exp $ */

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#ifdef arch_alpha
#define	NLIST_ECOFF
#define	NLIST_ELF64
#else
#ifdef arch_mips
#define NLIST_ELF32
#else
#ifdef arch_powerpc
#define	NLIST_ELF32
#else
#define	NLIST_AOUT
/* #define	NLIST_ECOFF */
#define	NLIST_ELF32
/* #define	NLIST_ELF64 */
#endif
#endif
#endif

#ifdef NLIST_AOUT
int	check_aout(int, const char *);
int	hide_aout(int, const char *);
#endif
#ifdef NLIST_ECOFF
int	check_ecoff(int, const char *);
int	hide_ecoff(int, const char *);
#endif
#ifdef NLIST_ELF32
int	check_elf32(int, const char *);
int	hide_elf32(int, const char *);
#endif
#ifdef NLIST_ELF64
int	check_elf64(int, const char *);
int	hide_elf64(int, const char *);
#endif

int	in_keep_list(const char *symbol);
