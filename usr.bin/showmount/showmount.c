/*
 * Copyright (c) 1989, 1993, 1995
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
 * @(#) Copyright (c) 1989, 1993, 1995 The Regents of the University of California.  All rights reserved.
 * @(#)showmount.c	8.3 (Berkeley) 3/29/95
 * $FreeBSD: src/usr.bin/showmount/showmount.c,v 1.8 1999/08/28 01:05:43 peter Exp $
 * $DragonFly: src/usr.bin/showmount/showmount.c,v 1.3 2003/10/04 20:36:50 hmp Exp $
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <err.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Constant defs */
#define	ALL	1
#define	DIRS	2

#define	DODUMP		0x1
#define	DOEXPORTS	0x2

struct mountlist {
	struct mountlist *ml_left;
	struct mountlist *ml_right;
	char	ml_host[RPCMNT_NAMELEN+1];
	char	ml_dirp[RPCMNT_PATHLEN+1];
};

struct grouplist {
	struct grouplist *gr_next;
	char	gr_name[RPCMNT_NAMELEN+1];
};

struct exportslist {
	struct exportslist *ex_next;
	struct grouplist *ex_groups;
	char	ex_dirp[RPCMNT_PATHLEN+1];
};

static struct mountlist *mntdump;
static struct exportslist *exports;
static int type = 0;

void print_dump(struct mountlist *);
static void usage(void);
int xdr_mntdump(XDR *, struct mountlist **);
int xdr_exports(XDR *, struct exportslist **);
int tcp_callrpc(char *host,
                int prognum, int versnum, int procnum,
                xdrproc_t inproc, char *in,
                xdrproc_t outproc, char *out);

/*
 * This command queries the NFS mount daemon for it's mount list and/or
 * it's exports list and prints them out.
 * See "NFS: Network File System Protocol Specification, RFC1094, Appendix A"
 * and the "Network File System Protocol XXX.."
 * for detailed information on the protocol.
 */
int
main(int argc, char **argv)
{
	register struct exportslist *exp;
	register struct grouplist *grp;
	register int rpcs = 0, mntvers = 1;
	char ch;
	char *host;
	int estat;

	while ((ch = getopt(argc, argv, "ade3")) != -1)
		switch((char)ch) {
		case 'a':
			if (type == 0) {
				type = ALL;
				rpcs |= DODUMP;
			} else
				usage();
			break;
		case 'd':
			if (type == 0) {
				type = DIRS;
				rpcs |= DODUMP;
			} else
				usage();
			break;
		case 'e':
			rpcs |= DOEXPORTS;
			break;
		case '3':
			mntvers = 3;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		host = *argv;
	else
		host = "localhost";

	if (rpcs == 0)
		rpcs = DODUMP;

	if (rpcs & DODUMP)
		if ((estat = tcp_callrpc(host, RPCPROG_MNT, mntvers,
			RPCMNT_DUMP, xdr_void, (char *)0,
			xdr_mntdump, (char *)&mntdump)) != 0) {
			clnt_perrno(estat);
			errx(1, "can't do mountdump rpc");
		}
	if (rpcs & DOEXPORTS)
		if ((estat = tcp_callrpc(host, RPCPROG_MNT, mntvers,
			RPCMNT_EXPORT, xdr_void, (char *)0,
			xdr_exports, (char *)&exports)) != 0) {
			clnt_perrno(estat);
			errx(1, "can't do exports rpc");
		}

	/* Now just print out the results */
	if (rpcs & DODUMP) {
		switch (type) {
		case ALL:
			printf("All mount points on %s:\n", host);
			break;
		case DIRS:
			printf("Directories on %s:\n", host);
			break;
		default:
			printf("Hosts on %s:\n", host);
			break;
		};
		print_dump(mntdump);
	}
	if (rpcs & DOEXPORTS) {
		printf("Exports list on %s:\n", host);
		exp = exports;
		while (exp) {
			printf("%-35s", exp->ex_dirp);
			grp = exp->ex_groups;
			if (grp == NULL) {
				printf("Everyone\n");
			} else {
				while (grp) {
					printf("%s ", grp->gr_name);
					grp = grp->gr_next;
				}
				printf("\n");
			}
			exp = exp->ex_next;
		}
	}
	exit(0);
}

/*
 * tcp_callrpc has the same interface as callrpc, but tries to
 * use tcp as transport method in order to handle large replies.
 */

int 
tcp_callrpc(char *host, int prognum, int versnum, int procnum, xdrproc_t inproc,
            char *in, xdrproc_t outproc, char *out)
{
	struct hostent *hp;
	struct sockaddr_in server_addr;
	CLIENT *client;
	int sock;	
	struct timeval timeout;
	int rval;
	
	hp = gethostbyname(host);

	if (!hp)
		return ((int) RPC_UNKNOWNHOST);

	memset(&server_addr,0,sizeof(server_addr));
	memcpy((char *) &server_addr.sin_addr,
	       hp->h_addr,
	       hp->h_length);
	server_addr.sin_len = sizeof(struct sockaddr_in);
	server_addr.sin_family =AF_INET;
	server_addr.sin_port = 0;
			
	sock = RPC_ANYSOCK;
			
	client = clnttcp_create(&server_addr,
				(u_long) prognum,
				(u_long) versnum, &sock, 0, 0);
	if (!client) {
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		server_addr.sin_port = 0;
		sock = RPC_ANYSOCK;
		client = clntudp_create(&server_addr,
					(u_long) prognum,
					(u_long) versnum,
					timeout,
					&sock);
	}
	if (!client)
		return ((int) rpc_createerr.cf_stat);

	timeout.tv_sec = 25;
	timeout.tv_usec = 0;
	rval = (int) clnt_call(client, procnum, 
			       inproc, in,
			       outproc, out,
			       timeout);
	clnt_destroy(client);
 	return rval;
}

/*
 * Xdr routine for retrieving the mount dump list
 */
int
xdr_mntdump(XDR *xdrsp, struct mountlist **mlp)
{
	register struct mountlist *mp;
	register struct mountlist *tp;
	register struct mountlist **otp;
	int val, val2;
	int bool;
	char *strp;

	*mlp = (struct mountlist *)0;
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		mp = (struct mountlist *)malloc(sizeof(struct mountlist));
		if (mp == NULL)
			return (0);
		mp->ml_left = mp->ml_right = (struct mountlist *)0;
		strp = mp->ml_host;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = mp->ml_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);

		/*
		 * Build a binary tree on sorted order of either host or dirp.
		 * Drop any duplications.
		 */
		if (*mlp == NULL) {
			*mlp = mp;
		} else {
			tp = *mlp;
			while (tp) {
				val = strcmp(mp->ml_host, tp->ml_host);
				val2 = strcmp(mp->ml_dirp, tp->ml_dirp);
				switch (type) {
				case ALL:
					if (val == 0) {
						if (val2 == 0) {
							free((caddr_t)mp);
							goto next;
						}
						val = val2;
					}
					break;
				case DIRS:
					if (val2 == 0) {
						free((caddr_t)mp);
						goto next;
					}
					val = val2;
					break;
				default:
					if (val == 0) {
						free((caddr_t)mp);
						goto next;
					}
					break;
				};
				if (val < 0) {
					otp = &tp->ml_left;
					tp = tp->ml_left;
				} else {
					otp = &tp->ml_right;
					tp = tp->ml_right;
				}
			}
			*otp = mp;
		}
next:
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

/*
 * Xdr routine to retrieve exports list
 */
int
xdr_exports(XDR *xdrsp, struct exportslist **exp)
{
	register struct exportslist *ep;
	register struct grouplist *gp;
	int bool, grpbool;
	char *strp;

	*exp = (struct exportslist *)0;
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		ep = (struct exportslist *)malloc(sizeof(struct exportslist));
		if (ep == NULL)
			return (0);
		ep->ex_groups = (struct grouplist *)0;
		strp = ep->ex_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		if (!xdr_bool(xdrsp, &grpbool))
			return (0);
		while (grpbool) {
			gp = (struct grouplist *)malloc(sizeof(struct grouplist));
			if (gp == NULL)
				return (0);
			strp = gp->gr_name;
			if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
				return (0);
			gp->gr_next = ep->ex_groups;
			ep->ex_groups = gp;
			if (!xdr_bool(xdrsp, &grpbool))
				return (0);
		}
		ep->ex_next = *exp;
		*exp = ep;
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: showmount [-ade3] host\n");
	exit(1);
}

/*
 * Print the binary tree in inorder so that output is sorted.
 */
void
print_dump(struct mountlist *mp)
{

	if (mp == NULL)
		return;
	if (mp->ml_left)
		print_dump(mp->ml_left);
	switch (type) {
	case ALL:
		printf("%s:%s\n", mp->ml_host, mp->ml_dirp);
		break;
	case DIRS:
		printf("%s\n", mp->ml_dirp);
		break;
	default:
		printf("%s\n", mp->ml_host);
		break;
	};
	if (mp->ml_right)
		print_dump(mp->ml_right);
}
