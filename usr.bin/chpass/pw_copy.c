/*-
 * Copyright (c) 1990, 1993, 1994
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
 * $FreeBSD: src/usr.bin/chpass/pw_copy.c,v 1.9.2.2 2002/03/24 09:00:03 cjc Exp $
 * $DragonFly: src/usr.bin/chpass/pw_copy.c,v 1.3 2003/10/02 17:42:26 hmp Exp $
 *
 * @(#)pw_copy.c	8.4 (Berkeley) 4/2/94
 */

/*
 * This module is used to copy the master password file, replacing a single
 * record, by chpass(1) and passwd(1).
 */

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pw_scan.h>
#include <pw_util.h>

#include "pw_copy.h"

extern char *tempname;

/* for use in pw_copy(). Compare a pw entry to a pw struct. */
static int
pw_equal(char *buf, struct passwd *pw)
{
	struct passwd buf_pw;
	int len;

	len = strlen (buf);
	if (buf[len-1] == '\n')
		buf[len-1] = '\0';
	if (!pw_scan(buf, &buf_pw))
		return 0;
	return (strcmp(pw->pw_name, buf_pw.pw_name) == 0
	    && pw->pw_uid == buf_pw.pw_uid
	    && pw->pw_gid == buf_pw.pw_gid
	    && strcmp(pw->pw_class, buf_pw.pw_class) == 0
	    && (long)pw->pw_change == (long)buf_pw.pw_change
	    && (long)pw->pw_expire == (long)buf_pw.pw_expire
	    && strcmp(pw->pw_gecos, buf_pw.pw_gecos) == 0
	    && strcmp(pw->pw_dir, buf_pw.pw_dir) == 0
	    && strcmp(pw->pw_shell, buf_pw.pw_shell) == 0);
}


void
pw_copy(int ffd, int tfd, struct passwd *pw, struct passwd *old_pw)
{
	FILE *from, *to;
	int done;
	char *p, buf[8192];
	char uidstr[20];
	char gidstr[20];
	char chgstr[20];
	char expstr[20];

	snprintf(uidstr, sizeof(uidstr), "%lu", (unsigned long)pw->pw_uid);
	snprintf(gidstr, sizeof(gidstr), "%lu", (unsigned long)pw->pw_gid);
	snprintf(chgstr, sizeof(chgstr), "%ld", (long)pw->pw_change);
	snprintf(expstr, sizeof(expstr), "%ld", (long)pw->pw_expire);

	if (!(from = fdopen(ffd, "r")))
		pw_error(_PATH_MASTERPASSWD, 1, 1);
	if (!(to = fdopen(tfd, "w")))
		pw_error(tempname, 1, 1);

	for (done = 0; fgets(buf, sizeof(buf), from);) {
		if (!strchr(buf, '\n')) {
			warnx("%s: line too long", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		if (done) {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		for (p = buf; *p != '\n'; p++)
			if (*p != ' ' && *p != '\t')
				break;
		if (*p == '#' || *p == '\n') {
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		if (!(p = strchr(buf, ':'))) {
			warnx("%s: corrupted entry", _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		*p = '\0';
		if (strcmp(buf, pw->pw_name)) {
			*p = ':';
			(void)fprintf(to, "%s", buf);
			if (ferror(to))
				goto err;
			continue;
		}
		*p = ':';
		if (old_pw && !pw_equal(buf, old_pw)) {
			warnx("%s: entry inconsistent",
			      _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		}
		(void)fprintf(to, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd,
		    pw->pw_fields & _PWF_UID ? uidstr : "",
		    pw->pw_fields & _PWF_GID ? gidstr : "",
		    pw->pw_class,
		    pw->pw_fields & _PWF_CHANGE ? chgstr : "",
		    pw->pw_fields & _PWF_EXPIRE ? expstr : "",
		    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
		done = 1;
		if (ferror(to))
			goto err;
	}
	if (!done) {
#ifdef YP
	/* Ultra paranoid: shouldn't happen. */
		if (getuid())  {
			warnx("%s: not found in %s -- permission denied",
					pw->pw_name, _PATH_MASTERPASSWD);
			pw_error(NULL, 0, 1);
		} else
#endif /* YP */
		(void)fprintf(to, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n",
		    pw->pw_name, pw->pw_passwd,
		    pw->pw_fields & _PWF_UID ? uidstr : "",
		    pw->pw_fields & _PWF_GID ? gidstr : "",
		    pw->pw_class,
		    pw->pw_fields & _PWF_CHANGE ? chgstr : "",
		    pw->pw_fields & _PWF_EXPIRE ? expstr : "",
		    pw->pw_gecos, pw->pw_dir, pw->pw_shell);
	}

	if (ferror(to))
err:		pw_error(NULL, 1, 1);
	(void)fclose(to);
}
