/*
 * Copyright (c) 1983, 1989, 1991, 1993
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
 * @(#) Copyright (c) 1983, 1989, 1991, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)route.c	8.6 (Berkeley) 4/28/95
 * $FreeBSD: src/sbin/route/route.c,v 1.40.2.11 2003/02/27 23:10:10 ru Exp $
 * $DragonFly: src/sbin/route/route.c,v 1.13 2005/03/16 08:02:05 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netatalk/at.h>
#ifdef NS
#include <netns/ns.h>
#endif
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "extern.h"
#include "keywords.h"

union	sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
#ifdef INET6
	struct	sockaddr_in6 sin6;
#endif
	struct	sockaddr_at sat;
#ifdef NS
	struct	sockaddr_ns sns;
#endif
	struct	sockaddr_dl sdl;
	struct	sockaddr_inarp sinarp;
	struct	sockaddr_storage ss; /* added to avoid memory overrun */
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp;

typedef union sockunion *sup;

int	nflag, wflag;

static struct ortentry route;
static struct rt_metrics rt_metrics;
static int	pid, rtm_addrs;
static int	s;
static int	forcehost, forcenet, af, qflag, tflag;
static int	iflag, verbose, aflen = sizeof(struct sockaddr_in);
static int	locking, lockrest, debugonly;
static u_long	rtm_inits;
static uid_t	uid;
#ifdef NS
static short	ns_bh[] = {-1,-1,-1};
#endif

static int	 atalk_aton(const char *, struct at_addr *);
static char	*atalk_ntoa(struct at_addr);
static void	 flushroutes(int, char **);
static void	 set_metric(char *, int);
static void	 newroute(int, char **);
static void	 inet_makenetandmask(u_long, struct sockaddr_in *, u_long);
static void	 interfaces(void);
static void	 monitor(void);
static void	 sockaddr(char *, struct sockaddr *);
static void	 sodump(sup, const char *);
static void	 bprintf(FILE *, int, u_char *);
static void	 print_getmsg(struct rt_msghdr *, int);
static void	 print_rtmsg(struct rt_msghdr *, int);
static void	 pmsg_common(struct rt_msghdr *);
static void	 pmsg_addrs(char *, int);
static void	 mask_addr(void);
static int	 getaddr(int, char *, struct hostent **);
static int	 rtmsg(int, int);
static int	 prefixlen(const char *);
#ifdef INET6
static int	 inet6_makenetandmask(struct sockaddr_in6 *, const char *);
#endif

void
usage(const char *cp)
{
	if (cp != NULL)
		warnx("bad keyword: %s", cp);
	fprintf(stderr, "usage: route [-dnqtv] command [[modifiers] args]\n");
	exit(EX_USAGE);
	/* NOTREACHED */
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

int
main(int argc, char **argv)
{
	int ch;

	if (argc < 2)
		usage((char *)NULL);

	while ((ch = getopt(argc, argv, "wnqdtv")) != -1)
		switch(ch) {
		case 'w':
			wflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'd':
			debugonly = 1;
			break;
		case '?':
		default:
			usage((char *)NULL);
		}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = geteuid();
	if (tflag)
		s = open(_PATH_DEVNULL, O_WRONLY, 0);
	else
		s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(EX_OSERR, "socket");
	if (*argv != NULL)
		switch (keyword(*argv)) {
		case K_GET:
			uid = 0;
			/* FALLTHROUGH */

		case K_CHANGE:
		case K_ADD:
		case K_DELETE:
			newroute(argc, argv);
			/* NOTREACHED */

		case K_SHOW:
			show(argc, argv);
			return(0);

		case K_MONITOR:
			monitor();
			/* NOTREACHED */

		case K_FLUSH:
			flushroutes(argc, argv);
			exit(0);
			/* NOTREACHED */
		}
	usage(*argv);
	/* NOTREACHED */
}

/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
static void
flushroutes(int argc, char **argv)
{
	size_t needed;
	int mib[6], rlen, seqno;
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;

	if (uid != 0) {
		errx(EX_NOPERM, "must be root to alter routing table");
	}
	shutdown(s, 0); /* Don't want to read back our messages */
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-')
		    switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
#ifdef INET6
			case K_INET6:
				af = AF_INET6;
				break;
#endif
			case K_ATALK:
				af = AF_APPLETALK;
				break;
#ifdef NS
			case K_XNS:
				af = AF_NS;
				break;
#endif
			case K_LINK:
				af = AF_LINK;
				break;
			default:
				goto bad;
		} else
bad:			usage(*argv);
	}
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(EX_OSERR, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(EX_OSERR, "malloc failed");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(EX_OSERR, "route-sysctl-get");
	lim = buf + needed;
	if (verbose)
		printf("Examining routing table from sysctl\n");
	seqno = 0;		/* ??? */
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		if ((rtm->rtm_flags & RTF_GATEWAY) == 0)
			continue;
		if (af != 0) {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);

			if (sa->sa_family != af)
				continue;
		}
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen < (int)rtm->rtm_msglen) {
			warn("write to routing socket");
			printf("got only %d for rlen\n", rlen);
			break;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose) {
			print_rtmsg(rtm, rlen);
		} else {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
			if (wflag) {
			    printf("%-20s ", rtm->rtm_flags & RTF_HOST ?
				routename(sa) : netname(sa));
			} else {
			    printf("%-20.20s ", rtm->rtm_flags & RTF_HOST ?
				routename(sa) : netname(sa));
			}
			sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) +
			    (char *)sa);
			if (wflag) {
			    printf("%-20s ", routename(sa));
			} else {
			    printf("%-20.20s ", routename(sa));
			}
			printf("done\n");
		}
	}
}

char *
routename(struct sockaddr *sa)
{
	const char *cp;
	static char line[MAXHOSTNAMELEN + 1];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.'))) {
			domain[MAXHOSTNAMELEN] = '\0';
			strcpy(domain, cp + 1);
		} else
			domain[0] = 0;
	}

	if (sa->sa_len == 0)
		strcpy(line, "default");
	else switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		cp = NULL;
		if (in.s_addr == INADDR_ANY || sa->sa_len < 4)
			cp = "default";
		if (cp == NULL && !nflag) {
			hp = gethostbyaddr((char *)&in, sizeof(struct in_addr),
				AF_INET);
			if (hp != NULL) {
				char *cptr;
				cptr = strchr(hp->h_name, '.');
				if (cptr != NULL && !wflag &&
				    strcmp(cptr + 1, domain) == 0)
					*cptr = '\0';
				cp = hp->h_name;
			}
		}
		if (cp != NULL) {
			strncpy(line, cp, sizeof(line) - 1);
			line[sizeof(line) - 1] = '\0';
		} else
			sprintf(line, "%s", inet_ntoa(in));
		break;
	    }

#ifdef INET6
	case AF_INET6:
	{
		struct sockaddr_in6 sin6; /* use static var for safety */
		int niflags = 0;
#ifdef NI_WITHSCOPEID
		niflags = NI_WITHSCOPEID;
#endif

		memset(&sin6, 0, sizeof(sin6));
		memcpy(&sin6, sa, sa->sa_len);
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_family = AF_INET6;
#ifdef __KAME__
		if (sa->sa_len == sizeof(struct sockaddr_in6) &&
		    (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr)) &&
		    sin6.sin6_scope_id == 0) {
			sin6.sin6_scope_id =
			    ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (nflag)
			niflags |= NI_NUMERICHOST;
		if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
		    line, sizeof(line), NULL, 0, niflags) != 0)
			strncpy(line, "invalid", sizeof(line));

		return(line);
	}
#endif

	case AF_APPLETALK:
		snprintf(line, sizeof(line), "atalk %s",
			atalk_ntoa(((struct sockaddr_at *)sa)->sat_addr));
		break;

#ifdef NS
	case AF_NS:
		return(ns_print((struct sockaddr_ns *)sa));
#endif

	case AF_LINK:
		return(link_ntoa((struct sockaddr_dl *)sa));

	default:
	    {
		/*
		 * Unknown address family; just render the raw
		 * data in sa->sa_data as hex values.
		 */
		uint8_t *sp = (uint8_t *)sa->sa_data;
		uint8_t *splim = (uint8_t *)sa + sa->sa_len;
		char *cps = line + sprintf(line, "(%d)", sa->sa_family);
		char *cpe = line + sizeof(line);

		while (sp < splim && cps < cpe)
			cps += snprintf(cps, cpe - cps, " %02x", *sp++);
		break;
	    }
	}
	return(line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(struct sockaddr *sa)
{
	const char *cp = 0;
	static char line[MAXHOSTNAMELEN + 1];
	struct netent *np = 0;
	u_long net, mask;
	u_long i;
	int subnetshift;

	switch (sa->sa_family) {

	case AF_INET:
	    {	struct in_addr in;
		in = ((struct sockaddr_in *)sa)->sin_addr;

		i = in.s_addr = ntohl(in.s_addr);
		if (in.s_addr == 0)
			cp = "default";
		else if (!nflag) {
			if (IN_CLASSA(i)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(i)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (in.s_addr &~ mask)
				mask = (long)mask >> subnetshift;
			net = in.s_addr & mask;
			while ((mask & 1) == 0)
				mask >>= 1, net >>= 1;
			np = getnetbyaddr(net, AF_INET);
			if (np != NULL)
				cp = np->n_name;
		}
#define C(x)	(unsigned)((x) & 0xff)
		if (cp != NULL)
			strncpy(line, cp, sizeof(line));
		else if ((in.s_addr & 0xffffff) == 0)
			sprintf(line, "%u", C(in.s_addr >> 24));
		else if ((in.s_addr & 0xffff) == 0)
			sprintf(line, "%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16));
		else if ((in.s_addr & 0xff) == 0)
			sprintf(line, "%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8));
		else
			sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			    C(in.s_addr >> 16), C(in.s_addr >> 8),
			    C(in.s_addr));
#undef C
		break;
	    }

#ifdef INET6
	case AF_INET6:
	{
		struct sockaddr_in6 sin6; /* use static var for safety */
		int niflags = 0;
#ifdef NI_WITHSCOPEID
		niflags = NI_WITHSCOPEID;
#endif

		memset(&sin6, 0, sizeof(sin6));
		memcpy(&sin6, sa, sa->sa_len);
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_family = AF_INET6;
#ifdef __KAME__
		if (sa->sa_len == sizeof(struct sockaddr_in6) &&
		    (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr)) &&
		    sin6.sin6_scope_id == 0) {
			sin6.sin6_scope_id =
			    ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (nflag)
			niflags |= NI_NUMERICHOST;
		if (getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
		    line, sizeof(line), NULL, 0, niflags) != 0)
			strncpy(line, "invalid", sizeof(line));

		return(line);
	}
#endif

	case AF_APPLETALK:
		snprintf(line, sizeof(line), "atalk %s",
			atalk_ntoa(((struct sockaddr_at *)sa)->sat_addr));
		break;

#ifdef NS
	case AF_NS:
		return(ns_print((struct sockaddr_ns *)sa));
		break;
#endif

	case AF_LINK:
		return(link_ntoa((struct sockaddr_dl *)sa));


	default:
	    {
		/*
		 * Unknown address family; just render the raw
		 * data in sa->sa_data as hex values.
		 */
		uint8_t *sp = (uint8_t *)sa->sa_data;
		uint8_t *splim = (uint8_t *)sa + sa->sa_len;
		char *cps = line + sprintf(line, "af %d:", sa->sa_family);
		char *cpe = line + sizeof(line);

		while (sp < splim && cps < cpe)
			cps += snprintf(cps, cpe - cps, " %02x", *sp++);
		break;
	    }
	}
	return(line);
}

static void
set_metric(char *value, int key)
{
	int flag = 0;
	u_long noval, *valp = &noval;

	switch (key) {
#define caseof(x, y, z)	case x: valp = &rt_metrics.z; flag = y; break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = atoi(value);
}

static void
newroute(int argc, char **argv)
{
	char *cmd;
	const char *err_str, *dest = "", *gateway = "";
	int ishost = 0, proxy = 0, ret, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *hp = 0;

	if (uid != 0) {
		errx(EX_NOPERM, "must be root to alter routing table");
	}
	cmd = argv[0];
	if (*cmd != 'g')
		shutdown(s, 0); /* Don't want to read back our messages */
	while (--argc > 0) {
		if (**(++argv)== '-') {
			switch (key = keyword(1 + *argv)) {
			case K_LINK:
				af = AF_LINK;
				aflen = sizeof(struct sockaddr_dl);
				break;
			case K_INET:
				af = AF_INET;
				aflen = sizeof(struct sockaddr_in);
				break;
#ifdef INET6
			case K_INET6:
				af = AF_INET6;
				aflen = sizeof(struct sockaddr_in6);
				break;
#endif
			case K_ATALK:
				af = AF_APPLETALK;
				aflen = sizeof(struct sockaddr_at);
				break;
			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof(union sockunion);
				break;
#ifdef NS
			case K_XNS:
				af = AF_NS;
				aflen = sizeof(struct sockaddr_ns);
				break;
#endif
			case K_IFACE:
			case K_INTERFACE:
				iflag++;
				break;
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LLINFO:
				flags |= RTF_LLINFO;
				break;
			case K_LOCK:
				locking = 1;
				break;
			case K_LOCKREST:
				lockrest = 1;
				break;
			case K_HOST:
				forcehost++;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
			case K_PROXY:
				proxy = 1;
				break;
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				if (--argc == 0)
					usage((char *)NULL);
				getaddr(RTA_IFA, *++argv, 0);
				break;
			case K_IFP:
				if (--argc == 0)
					usage((char *)NULL);
				getaddr(RTA_IFP, *++argv, 0);
				break;
			case K_GENMASK:
				if (--argc == 0)
					usage((char *)NULL);
				getaddr(RTA_GENMASK, *++argv, 0);
				break;
			case K_GATEWAY:
				if (--argc == 0)
					usage((char *)NULL);
				getaddr(RTA_GATEWAY, *++argv, 0);
				break;
			case K_DST:
				if (--argc == 0)
					usage((char *)NULL);
				ishost = getaddr(RTA_DST, *++argv, &hp);
				dest = *argv;
				break;
			case K_NETMASK:
				if (--argc == 0)
					usage((char *)NULL);
				getaddr(RTA_NETMASK, *++argv, 0);
				/* FALLTHROUGH */
			case K_NET:
				forcenet++;
				break;
			case K_PREFIXLEN:
				if (--argc == 0)
					usage((char *)NULL);
				if (prefixlen(*++argv) == -1) {
					forcenet = 0;
					ishost = 1;
				} else {
					forcenet = 1;
					ishost = 0;
				}
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				if (--argc == 0)
					usage((char *)NULL);
				set_metric(*++argv, key);
				break;
			default:
				usage(1+*argv);
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &hp);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				getaddr(RTA_GATEWAY, *argv, &hp);
			} else {
				getaddr(RTA_NETMASK, *argv, 0);
				forcenet = 1;
			}
		}
	}
	if (forcehost) {
		ishost = 1;
#ifdef INET6
		if (af == AF_INET6) {
			rtm_addrs &= ~RTA_NETMASK;
			memset((void *)&so_mask, 0, sizeof(so_mask));
		}
#endif
	}
	if (forcenet)
		ishost = 0;
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	if (proxy) {
		so_dst.sinarp.sin_other = SIN_PROXY;
		flags |= RTF_ANNOUNCE;
	}
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (af == AF_INET && *gateway != '\0' &&
		    hp != NULL && hp->h_addr_list[1] != NULL) {
			hp->h_addr_list++;
			memmove(&so_gate.sin.sin_addr,
				hp->h_addr_list[0],
				MIN((size_t)hp->h_length,
				    sizeof(so_gate.sin.sin_addr)));
		} else
			break;
	}
	if (*cmd == 'g')
		exit(0);
	if (!qflag) {
		oerrno = errno;
		printf("%s %s %s", cmd, ishost? "host" : "net", dest);
		if (*gateway != '\0') {
			printf(": gateway %s", gateway);
			if (attempts > 1 && ret == 0 && af == AF_INET)
			    printf(" (%s)",
				inet_ntoa(((struct sockaddr_in *)&route.rt_gateway)->sin_addr));
		}
		if (ret == 0) {
			printf("\n");
		} else {
			switch (oerrno) {
			case ESRCH:
				err_str = "not in table";
				break;
			case EBUSY:
				err_str = "entry in use";
				break;
			case ENOBUFS:
				err_str = "routing table overflow";
				break;
			case EDQUOT: /* handle recursion avoidance in
					rt_setgate() */
				err_str = "gateway uses the same route";
				break;
			default:
				err_str = strerror(oerrno);
				break;
			}
			printf(": %s\n", err_str);
		}
	}
	exit(ret != 0);
}

static void
inet_makenetandmask(u_long net, struct sockaddr_in *in, u_long bits)
{
	u_long addr, mask = 0;
	char *cp;

	rtm_addrs |= RTA_NETMASK;
	if (net == 0)
		mask = addr = 0;
	else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 16777216L) {
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else
			mask = -1;
	}
	if (bits != 0)
		mask = 0xffffffff << (32 - bits);
	in->sin_addr.s_addr = htonl(addr);
	in = &so_mask.sin;
	in->sin_addr.s_addr = htonl(mask);
	in->sin_len = 0;
	in->sin_family = 0;
	cp = (char *)(&in->sin_addr + 1);
	while (*--cp == 0 && cp > (char *)in)
		;
	in->sin_len = 1 + cp - (char *)in;
}

#ifdef INET6
/*
 * XXX the function may need more improvement...
 */
static int
inet6_makenetandmask(struct sockaddr_in6 *sin6, const char *plen)
{
	struct in6_addr in6;

	if (plen == NULL) {
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
		    sin6->sin6_scope_id == 0) {
			plen = "0";
		} else if ((sin6->sin6_addr.s6_addr[0] & 0xe0) == 0x20) {
			/* aggregatable global unicast - RFC2374 */
			memset(&in6, 0, sizeof(in6));
			if (memcmp(&sin6->sin6_addr.s6_addr[8],
				   &in6.s6_addr[8], 8) == 0)
				plen = "64";
		}
	}

	if (plen == NULL || strcmp(plen, "128") == 0)
		return(1);
	rtm_addrs |= RTA_NETMASK;
	prefixlen(plen);
	return(0);
}
#endif

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
static int
getaddr(int which, char *str, struct hostent **hpp)
{
	sup su;
	struct hostent *hp;
	struct netent *np;
	u_long val;
	char *q;
	int afamily;  /* local copy of af so we can change it */

	if (af == 0) {
		af = AF_INET;
		aflen = sizeof(struct sockaddr_in);
	}
	afamily = af;
	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = &so_dst;
		break;
	case RTA_GATEWAY:
		su = &so_gate;
		if (iflag) {
			struct ifaddrs *ifap, *ifa;
			struct sockaddr_dl *sdl = NULL;

			if (getifaddrs(&ifap))
				err(1, "getifaddrs");

			for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
				if (ifa->ifa_addr->sa_family != AF_LINK)
					continue;

				if (strcmp(str, ifa->ifa_name) != 0)
					continue;

				sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			}
			/* If we found it, then use it */
			if (sdl != NULL) {
				/*
				 * Copy is safe since we have a
				 * sockaddr_storage member in sockunion{}.
				 * Note that we need to copy before calling
				 * freeifaddrs().
				 *
				 * Interface routes are routed only by their
				 * sdl_index or interface name (we provide
				 * both here), not by the ARP address of the
				 * local interface (alen, slen = 0).
				 */
				memcpy(&su->sdl, sdl, sdl->sdl_len);
				su->sdl.sdl_alen = 0;
				su->sdl.sdl_slen = 0;
			}
			freeifaddrs(ifap);
			if (sdl != NULL)
				return(1);
		}
		break;
	case RTA_NETMASK:
		su = &so_mask;
		break;
	case RTA_GENMASK:
		su = &so_genmask;
		break;
	case RTA_IFP:
		su = &so_ifp;
		afamily = AF_LINK;
		break;
	case RTA_IFA:
		su = &so_ifa;
		break;
	default:
		usage("internal error");
		/*NOTREACHED*/
	}
	su->sa.sa_len = aflen;
	su->sa.sa_family = afamily; /* cases that don't want it have left
				       already */
	if (strcmp(str, "default") == 0) {
		/*
		 * Default is net 0.0.0.0/0
		 */
		switch (which) {
		case RTA_DST:
			forcenet++;
			/* bzero(su, sizeof(*su)); *//* for readability */
			getaddr(RTA_NETMASK, str, 0);
			break;
		case RTA_NETMASK:
		case RTA_GENMASK:
			/* bzero(su, sizeof(*su)); *//* for readability */
			break;
		}
		return(0);
	}
	switch (afamily) {
#ifdef INET6
	case AF_INET6:
	{
		struct addrinfo hints, *res;

		q = NULL;
		if (which == RTA_DST && (q = strchr(str, '/')) != NULL)
			*q = '\0';
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = afamily;	/*AF_INET6*/
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
		if (getaddrinfo(str, "0", &hints, &res) != 0 ||
		    res->ai_family != AF_INET6 ||
		    res->ai_addrlen != sizeof(su->sin6)) {
			fprintf(stderr, "%s: bad value\n", str);
			exit(1);
		}
		memcpy(&su->sin6, res->ai_addr, sizeof(su->sin6));
#ifdef __KAME__
		if ((IN6_IS_ADDR_LINKLOCAL(&su->sin6.sin6_addr) ||
		     IN6_IS_ADDR_LINKLOCAL(&su->sin6.sin6_addr)) &&
		    su->sin6.sin6_scope_id) {
			*(u_int16_t *)&su->sin6.sin6_addr.s6_addr[2] =
				htons(su->sin6.sin6_scope_id);
			su->sin6.sin6_scope_id = 0;
		}
#endif
		freeaddrinfo(res);
		if (q != NULL)
			*q++ = '/';
		if (which == RTA_DST)
			return(inet6_makenetandmask(&su->sin6, q));
		return(0);
	}
#endif /* INET6 */

#ifdef NS
	case AF_NS:
		if (which == RTA_DST) {
			struct sockaddr_ns *sms = &(so_mask.sns);
			memset(sms, 0, sizeof(*sms));
			sms->sns_family = 0;
			sms->sns_len = 6;
			sms->sns_addr.x_net = *(union ns_net *)ns_bh;
			rtm_addrs |= RTA_NETMASK;
		}
		su->sns.sns_addr = ns_addr(str);
		return(!ns_nullhost(su->sns.sns_addr));
#endif


	case AF_APPLETALK:
		if (!atalk_aton(str, &su->sat.sat_addr))
			errx(EX_NOHOST, "bad address: %s", str);
		rtm_addrs |= RTA_NETMASK;
		return(forcehost || su->sat.sat_addr.s_node != 0);

	case AF_LINK:
		link_addr(str, &su->sdl);
		return(1);


	case PF_ROUTE:
		su->sa.sa_len = sizeof(*su);
		sockaddr(str, &su->sa);
		return(1);

	case AF_INET:
	default:
		break;
	}

	if (hpp == NULL)
		hpp = &hp;
	*hpp = NULL;

	q = strchr(str,'/');
	if (q != NULL && which == RTA_DST) {
		*q = '\0';
		if ((val = inet_network(str)) != INADDR_NONE) {
			inet_makenetandmask(
				val, &su->sin, strtoul(q+1, 0, 0));
			return(0);
		}
		*q = '/';
	}
	if ((which != RTA_DST || forcenet == 0) &&
	    inet_aton(str, &su->sin.sin_addr)) {
		val = su->sin.sin_addr.s_addr;
		if (which != RTA_DST ||
		    inet_lnaof(su->sin.sin_addr) != INADDR_ANY)
			return(1);
		else {
			val = ntohl(val);
			goto netdone;
		}
	}
	if (which == RTA_DST && forcehost == 0 &&
	    ((val = inet_network(str)) != INADDR_NONE ||
	    ((np = getnetbyname(str)) != NULL && (val = np->n_net) != 0))) {
netdone:
		inet_makenetandmask(val, &su->sin, 0);
		return(0);
	}
	hp = gethostbyname(str);
	if (hp != NULL) {
		*hpp = hp;
		su->sin.sin_family = hp->h_addrtype;
		memmove((char *)&su->sin.sin_addr, hp->h_addr,
		    MIN((size_t)hp->h_length, sizeof(su->sin.sin_addr)));
		return(1);
	}
	errx(EX_NOHOST, "bad address: %s", str);
}

static int
prefixlen(const char *len_str)
{
	int len = atoi(len_str), q, r;
	int max;
	char *p;

	rtm_addrs |= RTA_NETMASK;	
	switch (af) {
#ifdef INET6
	case AF_INET6:
		max = 128;
		p = (char *)&so_mask.sin6.sin6_addr;
		break;
#endif
	case AF_INET:
		max = 32;
		p = (char *)&so_mask.sin.sin_addr;
		break;
	default:
		fprintf(stderr, "prefixlen not supported in this af\n");
		exit(1);
		/*NOTREACHED*/
	}

	if (len < 0 || max < len) {
		fprintf(stderr, "%s: bad value\n", len_str);
		exit(1);
	}
	
	q = len >> 3;
	r = len & 7;
	so_mask.sa.sa_family = af;
	so_mask.sa.sa_len = aflen;
	memset((void *)p, 0, max / 8);
	if (q > 0)
		memset((void *)p, 0xff, q);
	if (r > 0)
		*((u_char *)p + q) = (0xff00 >> r) & 0xff;
	if (len == max)
		return(-1);
	else
		return(len);
}

#ifdef NS
short ns_nullh[] = {0,0,0};

char *
ns_print(struct sockaddr_ns *sns)
{
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	u_short port;
	static char mybuf[50+MAXHOSTNAMELEN], cport[10], chost[25];
	const char *host = "";
	char *p;
	u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (port == 0)
			strncpy(mybuf, "*.*", sizeof(mybuf));
		else
			sprintf(mybuf, "*.%XH", port);
		return(mybuf);
	}

	if (memcmp(ns_bh, work.x_host.c_host, 6) == 0)
		host = "any";
	else if (memcmp(ns_nullh, work.x_host.c_host, 6) == 0)
		host = "*";
	else {
		q = work.x_host.c_host;
		sprintf(chost, "%02X%02X%02X%02X%02X%02XH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			/* void */;
		host = p;
	}
	if (port != 0)
		sprintf(cport, ".%XH", htons(port));
	else
		*cport = 0;

	snprintf(mybuf, sizeof(mybuf), "%lxH.%s%s",
			(unsigned long)ntohl(net.long_e),
			host, cport);
	return(mybuf);
}
#endif

static void
interfaces(void)
{
	size_t needed;
	int mib[6];
	char *buf, *lim, *next;
	struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(EX_OSERR, "route-sysctl-estimate");
	if ((buf = malloc(needed)) == NULL)
		errx(EX_OSERR, "malloc failed");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(EX_OSERR, "actual retrieval of interface table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		print_rtmsg(rtm, rtm->rtm_msglen);
	}
}

static void
monitor(void)
{
	int n;
	char msg[2048];

	verbose = 1;
	if (debugonly) {
		interfaces();
		exit(0);
	}
	for (;;) {
		time_t now;
		n = read(s, msg, 2048);
		now = time(NULL);
		printf("\ngot message of size %d on %s", n, ctime(&now));
		print_rtmsg((struct rt_msghdr *)msg, n);
	}
}

struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

static int
rtmsg(int cmd, int flags)
{
	static int seq;
	int rlen;
	char *cp = m_rtmsg.m_space;
	int l;

#define NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = ROUNDUP(u.sa.sa_len); memmove(cp, &(u), l); cp += l;\
	    if (verbose) sodump(&(u),"u");\
	}

	errno = 0;
	memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	if (cmd == 'a')
		cmd = RTM_ADD;
	else if (cmd == 'c')
		cmd = RTM_CHANGE;
	else if (cmd == 'g') {
		cmd = RTM_GET;
		if (so_ifp.sa.sa_family == 0) {
			so_ifp.sa.sa_family = AF_LINK;
			so_ifp.sa.sa_len = sizeof(struct sockaddr_dl);
			rtm_addrs |= RTA_IFP;
		}
	} else
		cmd = RTM_DELETE;
#define rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

	if (rtm_addrs & RTA_NETMASK)
		mask_addr();
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, l);
	if (debugonly)
		return(0);
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		warn("writing to routing socket");
		return(-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0)
			warn("read from routing socket");
		else
			print_getmsg(&rtm, l);
	}
#undef rtm
	return(0);
}

static void
mask_addr(void)
{
	int olen = so_mask.sa.sa_len;
	char *cp1 = olen + (char *)&so_mask, *cp2;

	for (so_mask.sa.sa_len = 0; cp1 > (char *)&so_mask; )
		if (*--cp1 != 0) {
			so_mask.sa.sa_len = 1 + cp1 - (char *)&so_mask;
			break;
		}
	if ((rtm_addrs & RTA_DST) == 0)
		return;
	switch (so_dst.sa.sa_family) {
#ifdef NS
	case AF_NS:
#endif
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
	case AF_APPLETALK:
	case 0:
		return;
	}
	cp1 = so_mask.sa.sa_len + 1 + (char *)&so_dst;
	cp2 = so_dst.sa.sa_len + 1 + (char *)&so_dst;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = so_mask.sa.sa_len + 1 + (char *)&so_mask;
	while (cp1 > so_dst.sa.sa_data)
		*--cp1 &= *--cp2;
}

const char *msgtypes[] = {
	"",
	"RTM_ADD: Add Route",
	"RTM_DELETE: Delete Route",
	"RTM_CHANGE: Change Metrics or flags",
	"RTM_GET: Report Metrics",
	"RTM_LOSING: Kernel Suspects Partitioning",
	"RTM_REDIRECT: Told to use different route",
	"RTM_MISS: Lookup failed on this address",
	"RTM_LOCK: fix specified metrics",
	"RTM_OLDADD: caused by SIOCADDRT",
	"RTM_OLDDEL: caused by SIOCDELRT",
	"RTM_RESOLVE: Route created by cloning",
	"RTM_NEWADDR: address being added to iface",
	"RTM_DELADDR: address being removed from iface",
	"RTM_IFINFO: iface status change",
	"RTM_NEWMADDR: new multicast group membership on iface",
	"RTM_DELMADDR: multicast group membership removed from iface",
	"RTM_IFANNOUNCE: interface arrival/departure",
	0,
};

char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount"
"\1mtu";
char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT"
"\011CLONING\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE\016b016"
"\017PROTO2\020PROTO1\021PRCLONING\022WASCLONED\023PROTO3\024CHAINDELETE"
"\025PINNED\026LOCAL\027BROADCAST\030MULTICAST";
char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6b6\7RUNNING\010NOARP"
"\011PPROMISC\012ALLMULTI\013OACTIVE\014SIMPLEX\015LINK0\016LINK1"
"\017LINK2\020MULTICAST";
char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD";

static void
print_rtmsg(struct rt_msghdr *rtm, int msglen __unused)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
#ifdef RTM_NEWMADDR
	struct ifma_msghdr *ifmam;
#endif
	struct if_announcemsghdr *ifan;

	if (verbose == 0)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (msgtypes[rtm->rtm_type] != NULL)
		printf("%s: ", msgtypes[rtm->rtm_type]);
	else
		printf("#%d: ", rtm->rtm_type);
	printf("len %d, ", rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		printf("if# %d, flags:", ifm->ifm_index);
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		printf("metric %d, flags:", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
#ifdef RTM_NEWMADDR
	case RTM_NEWMADDR:
	case RTM_DELMADDR:
		ifmam = (struct ifma_msghdr *)rtm;
		pmsg_addrs((char *)(ifmam + 1), ifmam->ifmam_addrs);
		break;
#endif
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		printf("if# %d, what: ", ifan->ifan_index);
		switch (ifan->ifan_what) {
		case IFAN_ARRIVAL:
			printf("arrival");
			break;
		case IFAN_DEPARTURE:
			printf("departure");
			break;
		default:
			printf("#%d", ifan->ifan_what);
			break;
		}
		printf("\n");
		break;

	default:
		printf("pid: %ld, seq %d, errno %d, flags:",
			(long)rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

static void
print_getmsg(struct rt_msghdr *rtm, int msglen)
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL;
	struct sockaddr_dl *ifp = NULL;
	struct sockaddr *sa;
	char *cp;
	int i;

	printf("   route to: %s\n", routename(&so_dst.sa));
	if (rtm->rtm_version != RTM_VERSION) {
		warnx("routing message version %d not understood",
		     rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > msglen) {
		warnx("message length mismatch, in packet %d, returned %d",
		      rtm->rtm_msglen, msglen);
	}
	if (rtm->rtm_errno != 0)  {
		errno = rtm->rtm_errno;
		warn("message indicates error %d", errno);
		return;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs)
		for (i = 1; i != 0; i <<= 1)
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					   ((struct sockaddr_dl *)sa)->sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
				}
				ADVANCE(cp, sa);
			}
	if (dst != NULL && mask != NULL)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst != NULL)
		printf("destination: %s\n", routename(dst));
	if (mask != NULL) {
		int savenflag = nflag;

		nflag = 1;
		printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (gate != NULL && rtm->rtm_flags & RTF_GATEWAY)
		printf("    gateway: %s\n", routename(gate));
	if (ifp != NULL)
		printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
	printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);

#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
#define msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	printf("\n%s\n", "\
 recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire");
	printf("%8ld%c ", rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
	printf("%8ld%c ", rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
	printf("%8ld%c ", rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
	printf("%8ld%c ", msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
	printf("%8ld%c ", msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
	printf("%8ld%c ", rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
	printf("%8ld%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire != 0)
		rtm->rtm_rmx.rmx_expire -= time(0);
	printf("%8ld%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
#undef	RTA_IGN
}

static void
pmsg_common(struct rt_msghdr *rtm)
{
	printf("\nlocks: ");
	bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
	printf(" inits: ");
	bprintf(stdout, rtm->rtm_inits, metricnames);
	pmsg_addrs(((char *)(rtm + 1)), rtm->rtm_addrs);
}

static void
pmsg_addrs(char *cp, int addrs)
{
	struct sockaddr *sa;
	int i;

	if (addrs == 0) {
		putchar('\n');
		return;
	}
	printf("\nsockaddrs: ");
	bprintf(stdout, addrs, addrnames);
	putchar('\n');
	for (i = 1; i != 0; i <<= 1)
		if (i & addrs) {
			sa = (struct sockaddr *)cp;
			printf(" %s", routename(sa));
			ADVANCE(cp, sa);
		}
	putchar('\n');
	fflush(stdout);
}

static void
bprintf(FILE *fp, int b, u_char *str)
{
	int i;
	int gotsome = 0;

	if (b == 0)
		return;
	while ((i = *str++) != 0) {
		if (b & (1 << (i-1))) {
			if (!gotsome)
				i = '<';
			else
				i = ',';
			putc(i, fp);
			gotsome = 1;
			for (; (i = *str) > 32; str++)
				putc(i, fp);
		} else
			while (*str > 32)
				str++;
	}
	if (gotsome)
		putc('>', fp);
}

int
keyword(const char *cp)
{
	struct keytab *kt = keywords;

	while (kt->kt_cp != NULL && strcmp(kt->kt_cp, cp) != 0)
		kt++;
	return(kt->kt_i);
}

static void
sodump(sup su, const char *which)
{
	switch (su->sa.sa_family) {
	case AF_LINK:
		printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
	case AF_INET:
		printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
	case AF_APPLETALK:
		printf("%s: atalk %s; ",
		    which, atalk_ntoa(su->sat.sat_addr));
		break;
#ifdef NS
	case AF_NS:
		printf("%s: xns %s; ",
		    which, ns_ntoa(su->sns.sns_addr));
		break;
#endif
	}
	fflush(stdout);
}

/* States*/
#define VIRGIN	0
#define GOTONE	1
#define GOTTWO	2
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)

static void
sockaddr(char *addr, struct sockaddr *sa)
{
	char *cp = (char *)sa;
	int size = sa->sa_len;
	char *cplim = cp + size;
	int byte = 0, state = VIRGIN, new = 0 /* foil gcc */;

	memset(cp, 0, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == '\0')
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
	sa->sa_len = cp - (char *)sa;
}

static int
atalk_aton(const char *text, struct at_addr *addr)
{
	u_int net, node;

	if (sscanf(text, "%u.%u", &net, &node) != 2
	    || net > 0xffff || node > 0xff)
		return(0);
	addr->s_net = htons(net);
	addr->s_node = node;
	return(1);
}

static char *
atalk_ntoa(struct at_addr at)
{
	static char buf[20];

	snprintf(buf, sizeof(buf), "%u.%u", ntohs(at.s_net), at.s_node);
	return(buf);
}
