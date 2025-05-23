 /*
  * pmap_set - set portmapper table from data produced by pmap_dump
  *
  * Author: Wietse Venema (wietse@wzv.win.tue.nl), dept. of Mathematics and
  * Computing Science, Eindhoven University of Technology, The Netherlands.
  */

/*
 * @(#) pmap_set.c 1.1 92/06/11 22:53:16
 * $FreeBSD: src/usr.sbin/portmap/pmap_set/pmap_set.c,v 1.6 2000/01/15 23:08:30 brian Exp $
 * $DragonFly: src/usr.sbin/portmap/pmap_set/pmap_set.c,v 1.4 2004/03/30 02:59:00 cpressey Exp $
 */

#include <sys/types.h>
#ifdef SYSV40
#include <netinet/in.h>
#endif
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include <err.h>
#include <stdio.h>

static int parse_line(char *, u_long *, u_long *, int *, unsigned *);

int
main(int argc, char **argv)
{
    struct sockaddr_in addr;
    char buf[BUFSIZ];
    u_long prog;
    u_long vers;
    int prot;
    unsigned port;

    get_myaddress(&addr);

    while (fgets(buf, sizeof(buf), stdin)) {
	if (parse_line(buf, &prog, &vers, &prot, &port) == 0) {
	    warnx("malformed line: %s", buf);
	    return (1);
	}
	if (pmap_set(prog, vers, prot, (unsigned short)port) == 0)
	    warnx("not registered: %s", buf);
    }
    return (0);
}

/* parse_line - convert line to numbers */

static int
parse_line(char *buf, u_long *prog, u_long *vers, int *prot,
	   unsigned int *port)
{
    char proto_name[BUFSIZ];

    if (sscanf(buf, "%lu %lu %s %u", prog, vers, proto_name, port) != 4) {
	return (0);
    }
    if (strcmp(proto_name, "tcp") == 0) {
	*prot = IPPROTO_TCP;
	return (1);
    }
    if (strcmp(proto_name, "udp") == 0) {
	*prot = IPPROTO_UDP;
	return (1);
    }
    if (sscanf(proto_name, "%d", prot) == 1) {
	return (1);
    }
    return (0);
}
