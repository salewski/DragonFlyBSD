/*-
 * Copyright (c) 1994
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
 * @(#)uname.c	8.1 (Berkeley) 1/4/94
 * $FreeBSD: src/lib/libc/gen/uname.c,v 1.7 1999/08/27 23:59:06 peter Exp $
 * $DragonFly: src/lib/libc/gen/uname.c,v 1.2 2003/06/17 04:26:42 dillon Exp $
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <errno.h>

int
uname(name)
	struct utsname *name;
{
	int mib[2], rval;
	size_t len;
	char *p;
	int oerrno;

	rval = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_OSTYPE;
	len = sizeof(name->sysname);
	oerrno = errno;
	if (sysctl(mib, 2, &name->sysname, &len, NULL, 0) == -1) {
		if(errno == ENOMEM)
			errno = oerrno;
		else
			rval = -1;
	}
	name->sysname[sizeof(name->sysname) - 1] = '\0';

	mib[0] = CTL_KERN;
	mib[1] = KERN_HOSTNAME;
	len = sizeof(name->nodename);
	oerrno = errno;
	if (sysctl(mib, 2, &name->nodename, &len, NULL, 0) == -1) {
		if(errno == ENOMEM)
			errno = oerrno;
		else
			rval = -1;
	}
	name->nodename[sizeof(name->nodename) - 1] = '\0';

	mib[0] = CTL_KERN;
	mib[1] = KERN_OSRELEASE;
	len = sizeof(name->release);
	oerrno = errno;
	if (sysctl(mib, 2, &name->release, &len, NULL, 0) == -1) {
		if(errno == ENOMEM)
			errno = oerrno;
		else
			rval = -1;
	}
	name->release[sizeof(name->release) - 1] = '\0';

	/* The version may have newlines in it, turn them into spaces. */
	mib[0] = CTL_KERN;
	mib[1] = KERN_VERSION;
	len = sizeof(name->version);
	oerrno = errno;
	if (sysctl(mib, 2, &name->version, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			errno = oerrno;
		else
			rval = -1;
	}
	name->version[sizeof(name->version) - 1] = '\0';
	for (p = name->version; len--; ++p) {
		if (*p == '\n' || *p == '\t') {
			if (len > 1)
				*p = ' ';
			else
				*p = '\0';
		}
	}

	mib[0] = CTL_HW;
	mib[1] = HW_MACHINE;
	len = sizeof(name->machine);
	oerrno = errno;
	if (sysctl(mib, 2, &name->machine, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			errno = oerrno;
		else
			rval = -1;
	}
	name->machine[sizeof(name->machine) - 1] = '\0';
	return (rval);
}
