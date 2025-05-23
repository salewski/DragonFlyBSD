/*-
 * Copyright (c) 1998 Dag-Erling Co�dan Sm�rgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 * $FreeBSD: src/lib/libfetch/file.c,v 1.6.2.8 2003/02/01 18:14:32 des Exp $
 * $DragonFly: src/lib/libfetch/file.c,v 1.2 2003/06/17 04:26:49 dillon Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include "fetch.h"
#include "common.h"

FILE *
fetchXGetFile(struct url *u, struct url_stat *us, const char *flags)
{
	FILE *f;

	if (us && fetchStatFile(u, us, flags) == -1)
		return (NULL);

	f = fopen(u->doc, "r");

	if (f == NULL)
		_fetch_syserr();

	if (u->offset && fseeko(f, u->offset, SEEK_SET) == -1) {
		fclose(f);
		_fetch_syserr();
	}

	return (f);
}

FILE *
fetchGetFile(struct url *u, const char *flags)
{
	return (fetchXGetFile(u, NULL, flags));
}

FILE *
fetchPutFile(struct url *u, const char *flags)
{
	FILE *f;

	if (CHECK_FLAG('a'))
		f = fopen(u->doc, "a");
	else
		f = fopen(u->doc, "w+");

	if (f == NULL)
		_fetch_syserr();

	if (u->offset && fseeko(f, u->offset, SEEK_SET) == -1) {
		fclose(f);
		_fetch_syserr();
	}

	return (f);
}

static int
_fetch_stat_file(const char *fn, struct url_stat *us)
{
	struct stat sb;

	us->size = -1;
	us->atime = us->mtime = 0;
	if (stat(fn, &sb) == -1) {
		_fetch_syserr();
		return (-1);
	}
	us->size = sb.st_size;
	us->atime = sb.st_atime;
	us->mtime = sb.st_mtime;
	return (0);
}

int
fetchStatFile(struct url *u, struct url_stat *us, const char *flags __unused)
{
	return (_fetch_stat_file(u->doc, us));
}

struct url_ent *
fetchListFile(struct url *u, const char *flags __unused)
{
	struct dirent *de;
	struct url_stat us;
	struct url_ent *ue;
	int size, len;
	char fn[PATH_MAX], *p;
	DIR *dir;
	int l;

	if ((dir = opendir(u->doc)) == NULL) {
		_fetch_syserr();
		return (NULL);
	}

	ue = NULL;
	strncpy(fn, u->doc, sizeof(fn) - 2);
	fn[sizeof(fn) - 2] = 0;
	strcat(fn, "/");
	p = strchr(fn, 0);
	l = sizeof(fn) - strlen(fn) - 1;

	while ((de = readdir(dir)) != NULL) {
		strncpy(p, de->d_name, l - 1);
		p[l - 1] = 0;
		if (_fetch_stat_file(fn, &us) == -1)
			/* should I return a partial result, or abort? */
			break;
		_fetch_add_entry(&ue, &size, &len, de->d_name, &us);
	}

	return (ue);
}
