/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
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
 * @(#)misc.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/tail/misc.c,v 1.4.8.2 2001/04/18 09:32:08 dwmalone Exp $
 * $DragonFly: src/usr.bin/tail/misc.c,v 1.3 2003/10/04 20:36:51 hmp Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "extern.h"

void
ierr(void)
{
	warn("%s", fname);
	rval = 1;
}

void
oerr(void)
{
	err(1, "stdout");
}

/*
 * Print `len' bytes from the file associated with `mip', starting at
 * absolute file offset `startoff'. May move map window.
 */
int
mapprint(struct mapinfo *mip, off_t startoff, off_t len)
{
	int n;

	while (len > 0) {
		if (startoff < mip->mapoff || startoff >= mip->mapoff +
		    mip->maplen) {
			if (maparound(mip, startoff) != 0)
				return (1);
		}
		n = (mip->mapoff + mip->maplen) - startoff;
		if (n > len)
			n = len;
		WR(mip->start + (startoff - mip->mapoff), n);
		startoff += n;
		len -= n;
	}
	return (0);
}

/*
 * Move the map window so that it contains the byte at absolute file
 * offset `offset'. The start of the map window will be TAILMAPLEN
 * aligned.
 */
int
maparound(struct mapinfo *mip, off_t offset)
{

	if (mip->start != NULL && munmap(mip->start, mip->maplen) != 0)
		return (1);

	mip->mapoff = offset & ~((off_t)TAILMAPLEN - 1);
	mip->maplen = TAILMAPLEN;
	if (mip->maplen > mip->maxoff - mip->mapoff)
		mip->maplen = mip->maxoff - mip->mapoff;
	if (mip->maplen <= 0)
		abort();
	if ((mip->start = mmap(NULL, mip->maplen, PROT_READ, MAP_SHARED,
	     mip->fd, mip->mapoff)) == MAP_FAILED)
		return (1);

	return (0);
}
