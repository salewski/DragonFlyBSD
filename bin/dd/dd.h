/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
 *	@(#)dd.h	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/bin/dd/dd.h,v 1.15.2.1 2000/08/07 08:30:17 ps Exp $
 * $DragonFly: src/bin/dd/dd.h,v 1.2 2003/06/17 04:22:49 dillon Exp $
 */

/* Input/output stream state. */
typedef struct {
	u_char		*db;		/* buffer address */
	u_char		*dbp;		/* current buffer I/O address */
	/* XXX ssize_t? */
	size_t		dbcnt;		/* current buffer byte count */
	size_t		dbrcnt;		/* last read byte count */
	size_t		dbsz;		/* buffer size */

#define	ISCHR		0x01		/* character device (warn on short) */
#define	ISPIPE		0x02		/* pipe-like (see position.c) */
#define	ISTAPE		0x04		/* tape */
#define	ISSEEK		0x08		/* valid to seek on */
#define	NOREAD		0x10		/* not readable */
#define	ISTRUNC		0x20		/* valid to ftruncate() */
	u_int		flags;

	const char 	*name;		/* name */
	int		fd;		/* file descriptor */
	off_t		offset;		/* # of blocks to skip */

	u_quad_t	f_stats;	/* # of full blocks processed */
	u_quad_t	p_stats;	/* # of partial blocks processed */
	u_quad_t	s_stats;	/* # of odd swab blocks */
	u_quad_t	t_stats;	/* # of truncations */
} IO;

typedef struct {
	u_quad_t	in_full;	/* # of full input blocks */
	u_quad_t	in_part;	/* # of partial input blocks */
	u_quad_t	out_full;	/* # of full output blocks */
	u_quad_t	out_part;	/* # of partial output blocks */
	u_quad_t	trunc;		/* # of truncated records */
	u_quad_t	swab;		/* # of odd-length swab blocks */
	u_quad_t	bytes;		/* # of bytes written */
	double		start; 			/* start time of dd */
} STAT;

/* Flags (in ddflags). */
#define	C_ASCII		0x00001
#define	C_BLOCK		0x00002
#define	C_BS		0x00004
#define	C_CBS		0x00008
#define	C_COUNT		0x00010
#define	C_EBCDIC	0x00020
#define	C_FILES		0x00040
#define	C_IBS		0x00080
#define	C_IF		0x00100
#define	C_LCASE		0x00200
#define	C_NOERROR	0x00400
#define	C_NOTRUNC	0x00800
#define	C_OBS		0x01000
#define	C_OF		0x02000
#define	C_SEEK		0x04000
#define	C_SKIP		0x08000
#define	C_SWAB		0x10000
#define	C_SYNC		0x20000
#define	C_UCASE		0x40000
#define	C_UNBLOCK	0x80000
#define	C_OSYNC		0x100000
#define	C_SPARSE	0x200000
