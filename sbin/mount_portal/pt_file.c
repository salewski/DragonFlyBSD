/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)pt_file.c	8.3 (Berkeley) 7/3/94
 *
 * $FreeBSD: src/sbin/mount_portal/pt_file.c,v 1.8 1999/08/28 00:13:38 peter Exp $
 * $DragonFly: src/sbin/mount_portal/pt_file.c,v 1.4 2004/12/18 21:43:39 swildner Exp $
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>

#include "portald.h"

int portal_file(struct portal_cred *pcr, char *key, char **v, int so, int *fdp)
{
	int fd;
	char pbuf[MAXPATHLEN];
	int error;
	gid_t gidset[NGROUPS];
	int i;

	pbuf[0] = '/';
	strcpy(pbuf+1, key + (v[1] ? strlen(v[1]) : 0));

#ifdef DEBUG
	printf("path = %s, uid = %d, gid = %d\n", pbuf, pcr->pcr_uid, pcr->pcr_groups[0]);
	printf ("fflag = %x, oflag = %x\n", pcr->pcr_flag, (pcr->pcr_flag)-1);
#endif

	for (i = 0; i < pcr->pcr_ngroups; i++)
		gidset[i] = pcr->pcr_groups[i];

	if (setgroups(pcr->pcr_ngroups, gidset) < 0)
		return (errno);

	if (seteuid(pcr->pcr_uid) < 0)
		return (errno);

	/* dmb convert kernel flags to oflags, see <fcntl.h> */
	fd = open(pbuf, (pcr->pcr_flag)-1, 0777);
	if (fd < 0)
		error = errno;
	else
		error = 0;

	if (seteuid((uid_t) 0) < 0) {	/* XXX - should reset gidset too */
		error = errno;
		syslog(LOG_ERR, "setcred: %s", strerror(error));
		if (fd >= 0) {
			close(fd);
			fd = -1;
		}
	}

	if (error == 0)
		*fdp = fd;

#ifdef DEBUG
	fprintf(stderr, "pt_file returns *fdp = %d, error = %d\n", *fdp, error);
#endif

	return (error);
}
