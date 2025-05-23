/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 * @(#) Copyright (c) 1989, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)nfsd.c	8.9 (Berkeley) 3/29/95
 * $FreeBSD: src/sbin/nfsd/nfsd.c,v 1.15.2.1 2000/09/16 22:52:23 brian Exp $
 * $DragonFly: src/sbin/nfsd/nfsd.c,v 1.7 2004/12/18 21:43:39 swildner Exp $
 */

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include <netdb.h>
#include <arpa/inet.h>
#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s)	fprintf(stderr,(s))
int	debug = 1;
#else
int	debug = 0;
#endif

struct	nfsd_srvargs nsd;
#ifdef OLD_SETPROCTITLE
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArg = NULL;	/* end of argv */
#endif

#ifdef NFSKERB
char		lnam[ANAME_SZ];
KTEXT_ST	kt;
AUTH_DAT	kauth;
char		inst[INST_SZ];
struct nfsrpc_fullblock kin, kout;
struct nfsrpc_fullverf kverf;
NFSKERBKEY_T	kivec;
struct timeval	ktv;
NFSKERBKEYSCHED_T kerb_keysched;
#endif

void	nonfs(int);
void	reapchild(int);
void	setbindhost(struct sockaddr_in *ia, const char *bindhost);
#ifdef OLD_SETPROCTITLE
#ifdef __FreeBSD__
void	setproctitle(char *);
#endif
#endif
void	usage(void);

/*
 * Nfs server daemon mostly just a user context for nfssvc()
 *
 * 1 - do file descriptor and signal cleanup
 * 2 - fork the nfsd(s)
 * 3 - create server socket(s)
 * 4 - register socket with portmap
 *
 * For connectionless protocols, just pass the socket into the kernel via.
 * nfssvc().
 * For connection based sockets, loop doing accepts. When you get a new
 * socket from accept, pass the msgsock into the kernel via. nfssvc().
 * The arguments are:
 *	-c - support iso cltp clients
 *	-r - reregister with portmapper
 *	-t - support tcp nfs clients
 *	-u - support udp nfs clients
 * followed by "n" which is the number of nfsds' to fork off
 */
int
main(int argc, char **argv, char **envp)
{
	struct nfsd_args nfsdargs;
	struct sockaddr_in inetaddr, inetpeer;
#ifdef ISO
	struct sockaddr_iso isoaddr, isopeer;
	char *cp;
#endif
	fd_set ready, sockbits;
	int ch, cltpflag, connect_type_cnt, i, len, maxsock = -1, msgsock;
	int nfsdcnt, nfssvc_flag, on, reregister, sock, tcpflag, tcpsock = -1;
	int tp4cnt, tp4flag, tpipcnt, tpipflag, udpflag;
	int bindhostc = 0, bindanyflag;
	char **bindhost = NULL;
#ifdef notyet
	int tp4sock, tpipsock;
#endif
#ifdef NFSKERB
	struct group *grp;
	struct passwd *pwd;
	struct ucred *cr;
	struct timeval ktv;
	char **cpp;
#endif
#ifdef __FreeBSD__
	struct vfsconf vfc;
	int error;

	error = getvfsbyname("nfs", &vfc);
	if (error && vfsisloadable("nfs")) {
		if (vfsload("nfs"))
			err(1, "vfsload(nfs)");
		endvfsent();	/* flush cache */
		error = getvfsbyname("nfs", &vfc);
	}
	if (error)
		errx(1, "NFS is not available in the running kernel");
#endif

#ifdef OLD_SETPROCTITLE
	/* Save start and extent of argv for setproctitle. */
	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);
#endif

#define	MAXNFSDCNT	20
#define	DEFNFSDCNT	 4
	nfsdcnt = DEFNFSDCNT;
	cltpflag = reregister = tcpflag = tp4cnt = tp4flag = tpipcnt = 0;
	bindanyflag = tpipflag = udpflag = 0;
#ifdef ISO
#define	GETOPT	"ach:n:rtu"
#define	USAGE	"[-acrtu] [-n num_servers] [-h bindip]"
#else
#define	GETOPT	"ah:n:rtu"
#define	USAGE	"[-artu] [-n num_servers] [-h bindip]"
#endif
	while ((ch = getopt(argc, argv, GETOPT)) != -1)
		switch (ch) {
		case 'a':
			bindanyflag = 1;
			break;
		case 'n':
			nfsdcnt = atoi(optarg);
			if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
				warnx("nfsd count %d; reset to %d", nfsdcnt,
				    DEFNFSDCNT);
				nfsdcnt = DEFNFSDCNT;
			}
			break;
		case 'h':
			bindhostc++;
			bindhost = realloc(bindhost,sizeof(char *)*bindhostc);
			if (bindhost == NULL) 
				errx(1, "Out of memory");
			bindhost[bindhostc-1] = strdup(optarg);
			if (bindhost[bindhostc-1] == NULL)
				errx(1, "Out of memory");
			break;
		case 'r':
			reregister = 1;
			break;
		case 't':
			tcpflag = 1;
			break;
		case 'u':
			udpflag = 1;
			break;
#ifdef ISO
		case 'c':
			cltpflag = 1;
			break;
#ifdef notyet
		case 'i':
			tp4cnt = 1;
			break;
		case 'p':
			tpipcnt = 1;
			break;
#endif /* notyet */
#endif /* ISO */
		default:
		case '?':
			usage();
		};
	if (!tcpflag && !udpflag)
		udpflag = 1;
	argv += optind;
	argc -= optind;

	/*
	 * XXX
	 * Backward compatibility, trailing number is the count of daemons.
	 */
	if (argc > 1)
		usage();
	if (argc == 1) {
		nfsdcnt = atoi(argv[0]);
		if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
			warnx("nfsd count %d; reset to %d", nfsdcnt,
			    DEFNFSDCNT);
			nfsdcnt = DEFNFSDCNT;
		}
	}

	if (bindhostc == 0 || bindanyflag) {
		bindhostc++;
		bindhost = realloc(bindhost,sizeof(char *)*bindhostc);
		if (bindhost == NULL) 
			errx(1, "Out of memory");
		bindhost[bindhostc-1] = strdup("*");
		if (bindhost[bindhostc-1] == NULL) 
			errx(1, "Out of memory");
	}

	if (debug == 0) {
		daemon(0, 0);
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGSYS, nonfs);
		signal(SIGTERM, SIG_IGN);
	}
	signal(SIGCHLD, reapchild);

	if (reregister) {
		if (udpflag &&
		    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_UDP, NFS_PORT) ||
		     !pmap_set(RPCPROG_NFS, 3, IPPROTO_UDP, NFS_PORT)))
			err(1, "can't register with portmap for UDP");
		if (tcpflag &&
		    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_TCP, NFS_PORT) ||
		     !pmap_set(RPCPROG_NFS, 3, IPPROTO_TCP, NFS_PORT)))
			err(1, "can't register with portmap for TCP");
		exit(0);
	}
	openlog("nfsd:", LOG_PID, LOG_DAEMON);

	for (i = 0; i < nfsdcnt; i++) {
		switch (fork()) {
		case -1:
			syslog(LOG_ERR, "fork: %m");
			exit (1);
		case 0:
			break;
		default:
			continue;
		}

		setproctitle("server");
		nfssvc_flag = NFSSVC_NFSD;
		nsd.nsd_nfsd = NULL;
#ifdef NFSKERB
		if (sizeof (struct nfsrpc_fullverf) != RPCX_FULLVERF ||
		    sizeof (struct nfsrpc_fullblock) != RPCX_FULLBLOCK)
		    syslog(LOG_ERR, "Yikes NFSKERB structs not packed!");
		nsd.nsd_authstr = (u_char *)&kt;
		nsd.nsd_authlen = sizeof (kt);
		nsd.nsd_verfstr = (u_char *)&kverf;
		nsd.nsd_verflen = sizeof (kverf);
#endif
		while (nfssvc(nfssvc_flag, &nsd) < 0) {
			if (errno != ENEEDAUTH) {
				syslog(LOG_ERR, "nfssvc: %m");
				exit(1);
			}
			nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHINFAIL;
#ifdef NFSKERB
			/*
			 * Get the Kerberos ticket out of the authenticator
			 * verify it and convert the principal name to a user
			 * name. The user name is then converted to a set of
			 * user credentials via the password and group file.
			 * Finally, decrypt the timestamp and validate it.
			 * For more info see the IETF Draft "Authentication
			 * in ONC RPC".
			 */
			kt.length = ntohl(kt.length);
			if (gettimeofday(&ktv, (struct timezone *)0) == 0 &&
			    kt.length > 0 && kt.length <=
			    (RPCAUTH_MAXSIZ - 3 * NFSX_UNSIGNED)) {
			    kin.w1 = NFS_KERBW1(kt);
			    kt.mbz = 0;
			    strcpy(inst, "*");
			    if (krb_rd_req(&kt, NFS_KERBSRV,
				inst, nsd.nsd_haddr, &kauth, "") == RD_AP_OK &&
				krb_kntoln(&kauth, lnam) == KSUCCESS &&
				(pwd = getpwnam(lnam)) != NULL) {
				cr = &nsd.nsd_cr;
				cr->cr_uid = pwd->pw_uid;
				cr->cr_groups[0] = pwd->pw_gid;
				cr->cr_ngroups = 1;
				setgrent();
				while ((grp = getgrent()) != NULL) {
					if (grp->gr_gid == cr->cr_groups[0])
						continue;
					for (cpp = grp->gr_mem;
					    *cpp != NULL; ++cpp)
						if (!strcmp(*cpp, lnam))
							break;
					if (*cpp == NULL)
						continue;
					cr->cr_groups[cr->cr_ngroups++]
					    = grp->gr_gid;
					if (cr->cr_ngroups == NGROUPS)
						break;
				}
				endgrent();

				/*
				 * Get the timestamp verifier out of the
				 * authenticator and verifier strings.
				 */
				kin.t1 = kverf.t1;
				kin.t2 = kverf.t2;
				kin.w2 = kverf.w2;
				bzero((caddr_t)kivec, sizeof (kivec));
				bcopy((caddr_t)kauth.session,
				    (caddr_t)nsd.nsd_key,sizeof(kauth.session));

				/*
				 * Decrypt the timestamp verifier in CBC mode.
				 */
				XXX

				/*
				 * Validate the timestamp verifier, to
				 * check that the session key is ok.
				 */
				nsd.nsd_timestamp.tv_sec = ntohl(kout.t1);
				nsd.nsd_timestamp.tv_usec = ntohl(kout.t2);
				nsd.nsd_ttl = ntohl(kout.w1);
				if ((nsd.nsd_ttl - 1) == ntohl(kout.w2))
				    nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHIN;
			}
#endif /* NFSKERB */
		}
		exit(0);
	}

	/* If we are serving udp, set up the socket. */
	for (i = 0; udpflag && i < bindhostc; i++) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create udp socket");
			exit(1);
		}
		setbindhost(&inetaddr, bindhost[i]);
		if (bind(sock,
		    (struct sockaddr *)&inetaddr, sizeof(inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind udp addr %s: %m", bindhost[i]);
			exit(1);
		}
		if (!pmap_set(RPCPROG_NFS, 2, IPPROTO_UDP, NFS_PORT) ||
		    !pmap_set(RPCPROG_NFS, 3, IPPROTO_UDP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			exit(1);
		}
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't Add UDP socket");
			exit(1);
		}
		close(sock);
	}

#ifdef ISO
	/* If we are serving cltp, set up the socket. */
	if (cltpflag) {
		if ((sock = socket(AF_ISO, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create cltp socket");
			exit(1);
		}
		memset(&isoaddr, 0, sizeof(isoaddr));
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		cp = TSEL(&isoaddr);
		*cp++ = (NFS_PORT >> 8);
		*cp = (NFS_PORT & 0xff);
		isoaddr.siso_len = sizeof(isoaddr);
		if (bind(sock,
		    (struct sockaddr *)&isoaddr, sizeof(isoaddr)) < 0) {
			syslog(LOG_ERR, "can't bind cltp addr");
			exit(1);
		}
#ifdef notyet
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_UDP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			exit(1);
		}
#endif /* notyet */
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't add UDP socket");
			exit(1);
		}
		close(sock);
	}
#endif /* ISO */

	/* Now set up the master server socket waiting for tcp connections. */
	on = 1;
	FD_ZERO(&sockbits);
	connect_type_cnt = 0;
	for (i = 0; tcpflag && i < bindhostc; i++) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create tcp socket");
			exit(1);
		}
		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		setbindhost(&inetaddr, bindhost[i]);
		if (bind(tcpsock,
		    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tcp addr %s: %m", bindhost[i]);
			exit(1);
		}
		if (listen(tcpsock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			exit(1);
		}
		if (!pmap_set(RPCPROG_NFS, 2, IPPROTO_TCP, NFS_PORT) ||
		    !pmap_set(RPCPROG_NFS, 3, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			exit(1);
		}
		FD_SET(tcpsock, &sockbits);
		maxsock = tcpsock;
		connect_type_cnt++;
	}

#ifdef notyet
	/* Now set up the master server socket waiting for tp4 connections. */
	if (tp4flag) {
		if ((tp4sock = socket(AF_ISO, SOCK_SEQPACKET, 0)) < 0) {
			syslog(LOG_ERR, "can't create tp4 socket");
			exit(1);
		}
		if (setsockopt(tp4sock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		memset(&isoaddr, 0, sizeof(isoaddr));
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		cp = TSEL(&isoaddr);
		*cp++ = (NFS_PORT >> 8);
		*cp = (NFS_PORT & 0xff);
		isoaddr.siso_len = sizeof(isoaddr);
		if (bind(tp4sock,
		    (struct sockaddr *)&isoaddr, sizeof (isoaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tp4 addr");
			exit(1);
		}
		if (listen(tp4sock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			exit(1);
		}
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			exit(1);
		}
		FD_SET(tp4sock, &sockbits);
		maxsock = tp4sock;
		connect_type_cnt++;
	}

	/* Now set up the master server socket waiting for tpip connections. */
	for (i = 0; tpipflag && i < bindhostc; i++) {
		if ((tpipsock = socket(AF_INET, SOCK_SEQPACKET, 0)) < 0) {
			syslog(LOG_ERR, "can't create tpip socket");
			exit(1);
		}
		if (setsockopt(tpipsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		setbindhost(&inetaddr, bindhost[i]);
		if (bind(tpipsock,
		    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tcp addr %s: %m", bindhost[i]);
			exit(1);
		}
		if (listen(tpipsock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			exit(1);
		}
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			exit(1);
		}
		FD_SET(tpipsock, &sockbits);
		maxsock = tpipsock;
		connect_type_cnt++;
	}
#endif /* notyet */

	if (connect_type_cnt == 0)
		exit(0);

	setproctitle("master");

	/*
	 * Loop forever accepting connections and passing the sockets
	 * into the kernel for the mounts.
	 */
	for (;;) {
		ready = sockbits;
		if (connect_type_cnt > 1) {
			if (select(maxsock + 1,
			    &ready, NULL, NULL, NULL) < 1) {
				syslog(LOG_ERR, "select failed: %m");
				exit(1);
			}
		}
		if (tcpflag && FD_ISSET(tcpsock, &ready)) {
			len = sizeof(inetpeer);
			if ((msgsock = accept(tcpsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			memset(inetpeer.sin_zero, 0, sizeof(inetpeer.sin_zero));
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = sizeof(inetpeer);
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(msgsock);
		}
#ifdef notyet
		if (tp4flag && FD_ISSET(tp4sock, &ready)) {
			len = sizeof(isopeer);
			if ((msgsock = accept(tp4sock,
			    (struct sockaddr *)&isopeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&isopeer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(msgsock);
		}
		if (tpipflag && FD_ISSET(tpipsock, &ready)) {
			len = sizeof(inetpeer);
			if ((msgsock = accept(tpipsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR, "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(msgsock);
		}
#endif /* notyet */
	}
}

void
setbindhost(struct sockaddr_in *ia, const char *bindhost)
{
	ia->sin_family = AF_INET;
	ia->sin_port = htons(NFS_PORT);
	ia->sin_len = sizeof(*ia);
	if (bindhost == NULL || strcmp(bindhost,"*") == 0) {
		ia->sin_addr.s_addr = INADDR_ANY;
	} else {
		if (inet_aton(bindhost, &ia->sin_addr) == 0) {
			struct hostent *he;

			he = gethostbyname2(bindhost, ia->sin_family);
			if (he == NULL) {
				syslog(LOG_ERR, "gethostbyname of %s failed", bindhost);
				exit(1);
			}
			bcopy(he->h_addr, &ia->sin_addr, he->h_length);
		}
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: nfsd %s\n", USAGE);
	exit(1);
}

void
nonfs(int signo)
{
	syslog(LOG_ERR, "missing system call: NFS not available");
}

void
reapchild(int signo)
{

	while (wait3(NULL, WNOHANG, NULL) > 0);
}

#ifdef OLD_SETPROCTITLE
#ifdef __FreeBSD__
void
setproctitle(char *a)
{
	register char *cp;
	char buf[80];

	cp = Argv[0];
	snprintf(buf, sizeof(buf), "nfsd-%s", a);
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = '\0';
	Argv[1] = NULL;
}
#endif	/* __FreeBSD__ */
#endif
