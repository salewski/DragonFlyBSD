/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/jls/jls.c,v 1.3 2003/04/22 13:24:56 mike Exp $
 * $DragonFly: src/usr.sbin/jls/jls.c,v 1.1 2005/01/31 22:29:59 joerg Exp $
 */

#include <sys/param.h>
#include <sys/kinfo.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <arpa/inet.h>

int
main(void)
{ 
	struct kinfo_prison *sxp, *xp;
	struct in_addr in;
	size_t i, len;

	if (sysctlbyname("jail.list", NULL, &len, NULL, 0) == -1)
		err(1, "sysctlbyname(): jail.list");
retry:
	if (len == 0)
		exit(0);	

	sxp = xp = malloc(len);
	if (sxp == NULL)
		err(1, "malloc failed");

	if (sysctlbyname("jail.list", xp, &len, NULL, 0) == -1) {
		if (errno == ENOMEM) {
			free(sxp);
			goto retry;
		}
		err(1, "sysctlbyname(): jail.list");
	}
	if (len < sizeof(*xp) || len % sizeof(*xp) ||
	    xp->pr_version != KINFO_PRISON_VERSION)
		errx(1, "Kernel and userland out of sync");

	len /= sizeof(*xp);
	printf("   JID  IP Address      Hostname                      Path\n");
	for (i = 0; i < len; i++) {
		in.s_addr = ntohl(xp->pr_ip);
		printf("%6d  %-15.15s %-29.29s %.74s\n",
		    xp->pr_id, inet_ntoa(in), xp->pr_host, xp->pr_path);
		xp++;
	}
	free(sxp);
	exit(0);
}
