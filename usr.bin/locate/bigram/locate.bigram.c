/*
 * Copyright (c) 1995 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods.
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
 * $FreeBSD: src/usr.bin/locate/bigram/locate.bigram.c,v 1.11 1999/08/28 01:02:54 peter Exp $
 * $DragonFly: src/usr.bin/locate/bigram/locate.bigram.c,v 1.2 2003/06/17 04:29:28 dillon Exp $
 *
 * @(#) Copyright (c) 1989, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)locate.bigram.c	8.1 (Berkeley) 6/6/93
 */

/*
 *  bigram < sorted_file_names | sort -nr | 
 *  	awk 'NR <= 128 { printf $2 }' > bigrams
 *
 * List bigrams for 'updatedb' script.
 * Use 'code' to encode a file using this output.
 */

#include <stdio.h>
#include <sys/param.h>			/* for MAXPATHLEN */
#include "locate.h"

u_char buf1[MAXPATHLEN] = " ";
u_char buf2[MAXPATHLEN];
u_int bigram[UCHAR_MAX + 1][UCHAR_MAX + 1];

int
main(void)
{
  	register u_char *cp;
	register u_char *oldpath = buf1, *path = buf2;
	register u_int i, j;

     	while (fgets(path, sizeof(buf2), stdin) != NULL) {

		/* 
		 * We don't need remove newline character '\n'.
		 * '\n' is less than ASCII_MIN and will be later
		 * ignored at output.
		 */


		/* skip longest common prefix */
		for (cp = path; *cp == *oldpath; cp++, oldpath++)
			if (*cp == '\0')
				break;

		while (*cp != '\0' && *(cp + 1) != '\0') {
			bigram[(u_char)*cp][(u_char)*(cp + 1)]++;
			cp += 2;
		}

		/* swap pointers */
		if (path == buf1) { 
			path = buf2;
			oldpath = buf1;
		} else {
			path = buf1;
			oldpath = buf2;
		}
   	}

	/* output, boundary check */
	for (i = ASCII_MIN; i <= ASCII_MAX; i++)
		for (j = ASCII_MIN; j <= ASCII_MAX; j++)
			if (bigram[i][j] != 0)
				(void)printf("%4u %c%c\n", bigram[i][j], i, j);

	exit(0);
}
