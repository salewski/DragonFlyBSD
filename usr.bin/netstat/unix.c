/*-
 * Copyright (c) 1983, 1988, 1993
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
 * @(#)unix.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/netstat/unix.c,v 1.12.2.2 2001/08/10 09:07:09 ru Exp $
 * $DragonFly: src/usr.bin/netstat/unix.c,v 1.4 2005/03/05 13:23:15 hmp Exp $
 */

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>

#include <netinet/in.h>

#include <errno.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <kvm.h>
#include "netstat.h"

static	void unixdomainpr (struct xunpcb *, struct xsocket *);

static	const char *const socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

void
unixpr(void)
{
	void	*buf;
	int	type;
	size_t	i, len;
	struct	xunpcb *xunp;
	char mibvar[sizeof "net.local.seqpacket.pcblist"];

	for (type = SOCK_STREAM; type <= SOCK_SEQPACKET; type++) {
		sprintf(mibvar, "net.local.%s.pcblist", socktype[type]);

		len = 0;
		if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: %s", mibvar);
			continue;
		}
		if ((buf = malloc(len)) == 0) {
			warn("malloc %lu bytes", (u_long)len);
			return;
		}
		if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
			warn("sysctl: %s", mibvar);
			free(buf);
			return;
		}

		xunp = (struct xunpcb *)buf;
		len /= sizeof(*xunp);
		for (i = 0; i < len; i++) {
			if (xunp[i].xu_len != sizeof(*xunp)) {
				warnx("sysctl: ABI mismatch");
				free(buf);
				return;
			}
			unixdomainpr(&xunp[i], &xunp[i].xu_socket);
		}
		free(buf);
	}
}

static void
unixdomainpr(struct xunpcb *xunp, struct xsocket *so)
{
	struct unpcb *unp;
	struct sockaddr_un *sa;
	static int first = 1;

	unp = &xunp->xu_unp;
	if (unp->unp_addr)
		sa = &xunp->xu_addr;
	else
		sa = (struct sockaddr_un *)0;

	if (first) {
		printf("Active UNIX domain sockets\n");
		printf(
"%-8.8s %-6.6s %-6.6s %-6.6s %8.8s %8.8s %8.8s %8.8s Addr\n",
		    "Address", "Type", "Recv-Q", "Send-Q",
		    "Inode", "Conn", "Refs", "Nextref");
		first = 0;
	}
	printf("%8lx %-6.6s %6ld %6ld %8lx %8lx %8lx %8lx",
	       (long)so->so_pcb, socktype[so->so_type], so->so_rcv.sb_cc,
	       so->so_snd.sb_cc,
	       (long)unp->unp_vnode, (long)unp->unp_conn,
	       (long)LIST_FIRST(&unp->unp_refs), (long)LIST_NEXT(unp, unp_reflink));
	if (sa)
		printf(" %.*s",
		    (int)(sa->sun_len - offsetof(struct sockaddr_un, sun_path)),
		    sa->sun_path);
	putchar('\n');
}
