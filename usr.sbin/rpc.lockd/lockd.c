/*
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
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
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/usr.sbin/rpc.lockd/lockd.c,v 1.5.2.1 2001/08/01 06:39:36 alfred Exp $
 * $DragonFly: src/usr.sbin/rpc.lockd/lockd.c,v 1.4 2004/12/18 22:48:13 swildner Exp $
 */

/* main() function for NFS lock daemon.  Most of the code in this	*/
/* file was generated by running rpcgen /usr/include/rpcsvc/nlm_prot.x	*/
/* The actual program logic is in the file procs.c			*/

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include "lockd.h"

void nlm_prog_1(struct svc_req *, SVCXPRT *);
void nlm_prog_3(struct svc_req *, SVCXPRT *);
void nlm_prog_4(struct svc_req *, SVCXPRT *);
static void usage(void);

int debug_level = 0;	/* Zero means no debugging syslog() calls	*/

int
main(int argc, char **argv)
{
  SVCXPRT *transp;

  if (argc > 1)
  {
    if (strncmp(argv[1], "-d", 2))
      usage();
    if (argc > 2) debug_level = atoi(argv[2]);
    else debug_level = atoi(argv[1] + 2);
    /* Ensure at least some debug if -d with no specified level		*/
    if (!debug_level) debug_level = 1;
  }

  pmap_unset(NLM_PROG, NLM_VERS);
  pmap_unset(NLM_PROG, NLM_VERSX);
  pmap_unset(NLM_PROG, NLM4_VERS);

  transp = svcudp_create(RPC_ANYSOCK);
  if (transp == NULL)
    errx(1, "cannot create udp service");
  if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_UDP))
    errx(1, "unable to register (NLM_PROG, NLM_VERS, udp)");
  if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_UDP))
    errx(1, "unable to register (NLM_PROG, NLM_VERSX, udp)");
  if (!svc_register(transp, NLM_PROG, NLM4_VERS, nlm_prog_4, IPPROTO_UDP))
    errx(1, "unable to register (NLM_PROG, NLM4_VERS, udp)");

  transp = svctcp_create(RPC_ANYSOCK, 0, 0);
  if (transp == NULL)
    errx(1, "cannot create tcp service");
  if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_TCP))
    errx(1, "unable to register (NLM_PROG, NLM_VERS, tcp)");
  if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_TCP))
    errx(1, "unable to register (NLM_PROG, NLM_VERSX, tcp)");
  if (!svc_register(transp, NLM_PROG, NLM4_VERS, nlm_prog_4, IPPROTO_TCP))
    errx(1, "unable to register (NLM_PROG, NLM4_VERS, tcp)");

  /* Note that it is NOT sensible to run this program from inetd - the 	*/
  /* protocol assumes that it will run immediately at boot time.	*/
  if (daemon(0,0))
    err(1, "fork");
  openlog("rpc.lockd", 0, LOG_DAEMON);
  if (debug_level) syslog(LOG_INFO, "Starting, debug level %d", debug_level);
  else syslog(LOG_INFO, "Starting");

  svc_run();	/* Should never return					*/
  exit(1);
}

static void
usage()
{
      fprintf(stderr, "usage: rpc.lockd [-d [debuglevel]]\n");
      exit(1);
}
