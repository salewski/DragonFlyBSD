/*

This code is not copyright, and is placed in the public domain. Feel free to
use and modify. Please send modifications and/or suggestions + bug fixes to

        Klas Heggemann <klas@nada.kth.se>

*/

/*
 * $FreeBSD: src/usr.sbin/bootparamd/bootparamd/main.c,v 1.9 1999/08/28 01:15:39 peter Exp $
 * $DragonFly: src/usr.sbin/bootparamd/bootparamd/main.c,v 1.5 2004/12/18 22:48:02 swildner Exp $
 */
#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "bootparam_prot.h"

int debug = 0;
int dolog = 0;
unsigned long route_addr = -1;
struct sockaddr_in my_addr;
char *bootpfile = "/etc/bootparams";

extern  void bootparamprog_1();
static void usage(void);

int
main(int argc, char **argv)
{
	SVCXPRT *transp;
	int i;
	struct hostent *he;
	struct stat buf;
	char c;

	while ((c = getopt(argc, argv,"dsr:f:")) != -1)
	  switch (c) {
	  case 'd':
	    debug = 1;
	    break;
	  case 'r':
	      if ( isdigit( *optarg)) {
		route_addr = inet_addr(optarg);
		break;
	      } else {
		he = gethostbyname(optarg);
		if (he) {
		   bcopy(he->h_addr, (char *)&route_addr, sizeof(route_addr));
		   break;
		} else {
		   errx(1, "no such host %s", argv[i]);
		}
	      }
	  case 'f':
	    bootpfile = optarg;
	    break;
	  case 's':
	    dolog = 1;
#ifndef LOG_DAEMON
	    openlog("bootparamd", 0 , 0);
#else
	    openlog("bootparamd", 0 , LOG_DAEMON);
	    setlogmask(LOG_UPTO(LOG_NOTICE));
#endif
	    break;
	  default:
	    usage();
	  }

	if ( stat(bootpfile, &buf ) )
	  err(1, "%s", bootpfile);

	if (route_addr == -1) {
	  get_myaddress(&my_addr);
	  bcopy(&my_addr.sin_addr.s_addr, &route_addr, sizeof (route_addr));
	}

	if (!debug) {
	  if (daemon(0,0))
	    err(1, "fork");
	}


	pmap_unset(BOOTPARAMPROG, BOOTPARAMVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL)
		errx(1, "cannot create udp service");
	if (!svc_register(transp, BOOTPARAMPROG, BOOTPARAMVERS, bootparamprog_1, IPPROTO_UDP))
		errx(1, "unable to register (BOOTPARAMPROG, BOOTPARAMVERS, udp)");

	svc_run();
	errx(1, "svc_run returned");
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: bootparamd [-d] [-s] [-r router] [-f bootparmsfile]\n");
	exit(1);
}
