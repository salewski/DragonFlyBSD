/*
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * @(#)pass4.c	8.4 (Berkeley) 4/28/95
 * $FreeBSD: src/sbin/fsck/pass4.c,v 1.7.2.1 2001/01/23 23:11:07 iedowse Exp $
 * $DragonFly: src/sbin/fsck/pass4.c,v 1.4 2003/09/28 14:39:17 hmp Exp $
 */

#include <sys/param.h>

#include <vfs/ufs/dinode.h>
#include <vfs/ufs/fs.h>

#include <err.h>
#include <string.h>

#include "fsck.h"

void
pass4(void)
{
	register ino_t inumber;
	register struct zlncnt *zlnp;
	struct dinode *dp;
	struct inodesc idesc;
	int i, n, cg;

	memset(&idesc, 0, sizeof(struct inodesc));
	idesc.id_type = ADDR;
	idesc.id_func = pass4check;
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		if (got_siginfo) {
			printf("%s: phase 4: cyl group %d of %d (%d%%)\n",
			    cdevname, cg, sblock.fs_ncg,
			    cg * 100 / sblock.fs_ncg);
			got_siginfo = 0;
		}
		inumber = cg * sblock.fs_ipg;
		for (i = 0; i < inostathead[cg].il_numalloced; i++, inumber++) {
			if (inumber < ROOTINO)
				continue;
			idesc.id_number = inumber;
			switch (inoinfo(inumber)->ino_state) {

			case FSTATE:
			case DFOUND:
				n = inoinfo(inumber)->ino_linkcnt;
				if (n) {
					adjust(&idesc, (short)n);
					break;
				}
				for (zlnp = zlnhead; zlnp; zlnp = zlnp->next) {
					if (zlnp->zlncnt == inumber) {
						zlnp->zlncnt = zlnhead->zlncnt;
						zlnp = zlnhead;
						zlnhead = zlnhead->next;
						free((char *)zlnp);
						clri(&idesc, "UNREF", 1);
						break;
					}
				}
				break;

			case DSTATE:
				clri(&idesc, "UNREF", 1);
				break;

			case DCLEAR:
				dp = ginode(inumber);
				if (dp->di_size == 0) {
					clri(&idesc, "ZERO LENGTH", 1);
					break;
				}
				/* fall through */
			case FCLEAR:
				clri(&idesc, "BAD/DUP", 1);
				break;

			case USTATE:
				break;

			default:
				errx(EEXIT, "BAD STATE %d FOR INODE I=%d",
				    inoinfo(inumber)->ino_state, inumber);
			}
		}
	}
}

int
pass4check(register struct inodesc *idesc)
{
	register struct dups *dlp;
	int nfrags, res = KEEPON;
	ufs_daddr_t blkno = idesc->id_blkno;

	for (nfrags = idesc->id_numfrags; nfrags > 0; blkno++, nfrags--) {
		if (chkrange(blkno, 1)) {
			res = SKIP;
		} else if (testbmap(blkno)) {
			for (dlp = duplist; dlp; dlp = dlp->next) {
				if (dlp->dup != blkno)
					continue;
				dlp->dup = duplist->dup;
				dlp = duplist;
				duplist = duplist->next;
				free((char *)dlp);
				break;
			}
			if (dlp == 0) {
				clrbmap(blkno);
				n_blks--;
			}
		}
	}
	return (res);
}
