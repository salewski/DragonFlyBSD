/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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
 * @(#)replace.c	8.3 (Berkeley) 4/2/94
 *
 * $DragonFly: src/usr.bin/ar/Attic/replace.c,v 1.4 2005/01/13 18:57:56 okumoto Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <ar.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "extern.h"

/*
 * replace --
 *	Replace or add named members to archive.  Entries already in the
 *	archive are swapped in place.  Others are added before or after
 *	the key entry, based on the a, b and i options.  If the u option
 *	is specified, modification dates select for replacement.
 */
int
replace(char **argv)
{
	char *file;
	int afd, curfd, errflg, exists, mods, sfd, tfd1, tfd2;
	struct stat sb;
	CF cf;
	off_t size, tsize;

	errflg = 0;
	/*
	 * If doesn't exist, simply append to the archive.  There's
	 * a race here, but it's pretty short, and not worth fixing.
	 */
	exists = !stat(archive, &sb);
	afd = open_archive(O_CREAT|O_RDWR);

	if (!exists) {
		tfd1 = -1;
		tfd2 = tmp();
		goto append;
	}

	tfd1 = tmp();			/* Files before key file. */
	tfd2 = tmp();			/* Files after key file. */

	/*
	 * Break archive into two parts -- entries before and after the key
	 * entry.  If positioning before the key, place the key at the
	 * beginning of the after key entries and if positioning after the
	 * key, place the key at the end of the before key entries.  Put it
	 * all back together at the end.
	 */
	mods = (options & (AR_A|AR_B));
	for (curfd = tfd1; get_arobj(afd);) {
		if (*argv && (file = files(argv))) {
			if ((sfd = open(file, O_RDONLY)) < 0) {
				errflg = 1;
				warn("%s", file);
				goto useold;
			}
			fstat(sfd, &sb);
			if (options & AR_U && sb.st_mtime <= chdr.date) {
				close(sfd);
				goto useold;
			}

			if (options & AR_V)
			     printf("r - %s\n", file);

			/* Read from disk, write to an archive; pad on write */
			SETCF(sfd, file, curfd, tname, WPAD);
			put_arobj(&cf, &sb);
			close(sfd);
			skip_arobj(afd);
			continue;
		}

		if (mods && compare(posname)) {
			mods = 0;
			if (options & AR_B)
				curfd = tfd2;
			/* Read and write to an archive; pad on both. */
			SETCF(afd, archive, curfd, tname, RPAD|WPAD);
			put_arobj(&cf, (struct stat *)NULL);
			if (options & AR_A)
				curfd = tfd2;
		} else {
			/* Read and write to an archive; pad on both. */
useold:			SETCF(afd, archive, curfd, tname, RPAD|WPAD);
			put_arobj(&cf, (struct stat *)NULL);
		}
	}

	if (mods) {
		warnx("%s: archive member not found", posarg);
                close_archive(afd);
                return (1);
        }

	/* Append any left-over arguments to the end of the after file. */
append:	while ( (file = *argv++) ) {
		if (options & AR_V)
			printf("a - %s\n", file);
		if ((sfd = open(file, O_RDONLY)) < 0) {
			errflg = 1;
			warn("%s", file);
			continue;
		}
		fstat(sfd, &sb);
		/* Read from disk, write to an archive; pad on write. */
		SETCF(sfd, file,
		    options & (AR_A|AR_B) ? tfd1 : tfd2, tname, WPAD);
		put_arobj(&cf, &sb);
		close(sfd);
	}

	lseek(afd, (off_t)SARMAG, SEEK_SET);

	SETCF(tfd1, tname, afd, archive, NOPAD);
	if (tfd1 != -1) {
		tsize = size = lseek(tfd1, (off_t)0, SEEK_CUR);
		lseek(tfd1, (off_t)0, SEEK_SET);
		copy_ar(&cf, size);
	} else
		tsize = 0;

	tsize += size = lseek(tfd2, (off_t)0, SEEK_CUR);
	lseek(tfd2, (off_t)0, SEEK_SET);
	cf.rfd = tfd2;
	copy_ar(&cf, size);

	ftruncate(afd, tsize + SARMAG);
	close_archive(afd);
	return (errflg);
}
