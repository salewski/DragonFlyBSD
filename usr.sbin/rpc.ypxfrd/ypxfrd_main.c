/*
 * Copyright (c) 1995, 1996
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/rpc.ypxfrd/ypxfrd_main.c,v 1.8.2.3 2002/02/15 00:46:58 des Exp $
 * $DragonFly: src/usr.sbin/rpc.ypxfrd/ypxfrd_main.c,v 1.4 2004/12/18 22:48:14 swildner Exp $
 */

#include "ypxfrd.h"
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <unistd.h>
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */
#include <signal.h>
#include <sys/ttycom.h> /* TIOCNOTTY */
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include "ypxfrd_extern.h"
#include <sys/wait.h>
#include <errno.h>

#ifndef SIG_PF
#define	SIG_PF void(*)(int)
#endif

#ifdef DEBUG
#define	RPC_SVC_FG
#endif

#define	_RPCSVC_CLOSEDOWN 120
int _rpcpmstart;		/* Started by a port monitor ? */
static int _rpcfdtype;
		 /* Whether Stream or Datagram ? */
	/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1
#define	_SERVING 2

extern int _rpcsvcstate;	 /* Set when a request is serviced */

char *progname = "rpc.ypxfrd";
char *yp_dir = "/var/yp/";

static
void _msgout(char* msg)
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, "%s", msg);
	else
		warnx("%s", msg);
#else
	syslog(LOG_ERR, "%s", msg);
#endif
}

static void
closedown(int sig)
{
	if (_rpcsvcstate == _IDLE) {
		extern fd_set svc_fdset;
		static int size;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM)
			exit(0);
		if (size == 0) {
			size = getdtablesize();
		}
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1)
			exit(0);
	}
	if (_rpcsvcstate == _SERVED)
		_rpcsvcstate = _IDLE;

	signal(SIGALRM, (SIG_PF) closedown);
	alarm(_RPCSVC_CLOSEDOWN/2);
}


static void
ypxfrd_svc_run()
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else
	int readfds;
#endif /* def FD_SETSIZE */
	extern int forked;
	int pid;
	int fd_setsize = _rpc_dtablesize();

	/* Establish the identity of the parent ypserv process. */
	pid = getpid();

	for (;;) {
#ifdef FD_SETSIZE
		readfds = svc_fdset;
#else
		readfds = svc_fds;
#endif /* def FD_SETSIZE */
		switch (select(fd_setsize, &readfds, NULL, NULL,
			       (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			warn("svc_run: - select failed");
			return;
		case 0:
			continue;
		default:
			svc_getreqset(&readfds);
			if (forked && pid != getpid())
				exit(0);
		}
	}
}

static void reaper(sig)
	int sig;
{
	int			status;
	int			saved_errno;

	saved_errno = errno;

	if (sig == SIGHUP) {
		load_securenets();
		errno = saved_errno;
		return;
	}

	if (sig == SIGCHLD) {
		while (wait3(&status, WNOHANG, NULL) > 0)
			children--;
	} else {
		pmap_unset(YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS);
		exit(0);
	}

	errno = saved_errno;
	return;
}

void usage()
{
	fprintf(stderr, "usage: rpc.ypxfrd [-p path]\n");
	exit(0);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	SVCXPRT *transp = NULL;
	int sock;
	int proto = 0;
	struct sockaddr_in saddr;
	int asize = sizeof (saddr);
	int ch;

	while ((ch = getopt(argc, argv, "p:h")) != -1) {
		switch (ch) {
		case 'p':
			yp_dir = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	load_securenets();

	if (getsockname(0, (struct sockaddr *)&saddr, &asize) == 0) {
		int ssize = sizeof (int);

		if (saddr.sin_family != AF_INET)
			exit(1);
		if (getsockopt(0, SOL_SOCKET, SO_TYPE,
				(char *)&_rpcfdtype, &ssize) == -1)
			exit(1);
		sock = 0;
		_rpcpmstart = 1;
		proto = 0;
		openlog("rpc.ypxfrd", LOG_PID, LOG_DAEMON);
	} else {
#ifndef RPC_SVC_FG
		int size;
		int pid, i;

		pid = fork();
		if (pid < 0)
			err(1, "fork");
		if (pid)
			exit(0);
		size = getdtablesize();
		for (i = 0; i < size; i++)
			close(i);
		i = open(_PATH_CONSOLE, 2);
		dup2(i, 1);
		dup2(i, 2);
		i = open(_PATH_TTY, 2);
		if (i >= 0) {
			ioctl(i, TIOCNOTTY, (char *)NULL);
			close(i);
		}
		openlog("rpc.ypxfrd", LOG_PID, LOG_DAEMON);
#endif
		sock = RPC_ANYSOCK;
		pmap_unset(YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS);
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			_msgout("cannot create udp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (!svc_register(transp, YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS, ypxfrd_freebsd_prog_1, proto)) {
			_msgout("unable to register (YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS, udp).");
			exit(1);
		}
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			_msgout("cannot create tcp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (!svc_register(transp, YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS, ypxfrd_freebsd_prog_1, proto)) {
			_msgout("unable to register (YPXFRD_FREEBSD_PROG, YPXFRD_FREEBSD_VERS, tcp).");
			exit(1);
		}
	}

	if (transp == (SVCXPRT *)NULL) {
		_msgout("could not create a handle");
		exit(1);
	}
	if (_rpcpmstart) {
		signal(SIGALRM, (SIG_PF) closedown);
		alarm(_RPCSVC_CLOSEDOWN/2);
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, (SIG_PF) reaper);
	signal(SIGTERM, (SIG_PF) reaper);
	signal(SIGINT, (SIG_PF) reaper);
	signal(SIGHUP, (SIG_PF) reaper);

	ypxfrd_svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}
