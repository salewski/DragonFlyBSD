/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
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
 * @(#) Copyright (c) 1989, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)ping.c	8.1 (Berkeley) 6/5/93
 * $FreeBSD: src/sbin/ping/ping.c,v 1.52.2.13 2002/10/29 10:23:21 maxim Exp $
 * $DragonFly: src/sbin/ping/ping.c,v 1.5 2005/02/13 02:11:26 swildner Exp $
 */

/*
 *			P I N G . C
 *
 * Using the Internet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/param.h>		/* NB: we rely on this for <sys/types.h> */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

#define	INADDR_LEN	((int)sizeof(in_addr_t))
#define	PHDR_LEN	((int)sizeof(struct timeval))
#define	DEFDATALEN	(64 - PHDR_LEN)	/* default data length */
#define	FLOOD_BACKOFF	20000		/* usecs to back off if F_FLOOD mode */
					/* runs out of buffer space */
#define	MAXIPLEN	(sizeof(struct ip) + MAX_IPOPTLEN)
#define	MAXICMPLEN	(ICMP_ADVLENMIN + MAX_IPOPTLEN)
#define	MINICMPLEN	ICMP_MINLEN
#define	MAXPAYLOAD	(IP_MAXPACKET - MAXIPLEN - MINICMPLEN)
#define	MAXWAIT		10		/* max seconds to wait for response */
#define	MAXALARM	(60 * 60)	/* max seconds for alarm timeout */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
int options;
#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_NUMERIC	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
#define	F_SO_DONTROUTE	0x0080
#define	F_VERBOSE	0x0100
#define	F_QUIET2	0x0200
#define	F_NOLOOP	0x0400
#define	F_MTTL		0x0800
#define	F_MIF		0x1000
#define	F_AUDIBLE	0x2000
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
#define F_POLICY	0x4000
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
#define	F_TTL		0x8000
#define	F_MISSED	0x10000

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 128)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct sockaddr_in whereto;	/* who to ping */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[MINICMPLEN + MAXPAYLOAD];
char BSPACE = '\b';		/* characters written for flood */
char BBELL = '\a';		/* characters written for MISSED and AUDIBLE */
char DOT = '.';
char *hostname;
char *shostname;
int ident;			/* process id to identify our packets */
int uid;			/* cached uid for micro-optimization */

/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
long nmissedmax;		/* max value of ntransmitted - nreceived - 1 */
int interval = 1000;		/* interval between packets, ms */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */
double tsumsq = 0.0;		/* sum of all times squared, for std. dev. */

volatile sig_atomic_t finish_up;  /* nonzero if we've been told to finish up */
int reset_kerninfo;
volatile sig_atomic_t siginfo_p;

static void fill(char *, char *);
static u_short in_cksum(u_short *, int);
static void check_status(void);
static void finish(void) __dead2;
static void pinger(void);
static char *pr_addr(struct in_addr);
static void pr_icmph(struct icmp *);
static void pr_iph(struct ip *);
static void pr_pack(char *, int, struct sockaddr_in *, struct timeval *);
static void pr_retip(struct ip *);
static void status(int);
static void stopit(int);
static void tvsub(struct timeval *, struct timeval *);
static void usage(void) __dead2;

int
main(int argc, char **argv)
{
	struct in_addr ifaddr;
	struct iovec iov;
	struct msghdr msg;
	struct sigaction si_sa;
	struct sockaddr_in from, src_in;
	struct termios ts;
	struct timeval last, intvl;
	struct hostent *hp;
	struct sockaddr_in *to;
	double t;
	u_char *datap, packet[IP_MAXPACKET];
	char *ep, *source, *target;
#ifdef IPSEC_POLICY_IPSEC
	char *policy_in, *policy_out;
#endif
	u_long alarmtimeout, ultmp;
	int ch, hold, i, packlen, preload, sockerrno, almost_done = 0, ttl;
	char ctrl[CMSG_SPACE(sizeof(struct timeval))];
	char hnamebuf[MAXHOSTNAMELEN], snamebuf[MAXHOSTNAMELEN];
#ifdef IP_OPTIONS
	char rspace[MAX_IPOPTLEN];	/* record route space */
#endif
	unsigned char mttl, loop;

	source = NULL;
#ifdef IPSEC_POLICY_IPSEC
	policy_in = policy_out = NULL;
#endif

	/*
	 * Do the stuff that we need root priv's for *first*, and
	 * then drop our setuid bit.  Save error reporting for
	 * after arg parsing.
	 */
	s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	sockerrno = errno;

	setuid(getuid());
	uid = getuid();

	alarmtimeout = preload = 0;

	datap = &outpack[MINICMPLEN + PHDR_LEN];
	while ((ch = getopt(argc, argv,
		"AI:LQRS:T:c:adfi:l:m:np:qrs:t:v"
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		"P:"
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		)) != -1)
	{
		switch(ch) {
		case 'A':
			options |= F_MISSED;
			break;
		case 'a':
			options |= F_AUDIBLE;
			break;
		case 'c':
			ultmp = strtoul(optarg, &ep, 0);
			if (*ep || ep == optarg || ultmp > LONG_MAX || !ultmp)
				errx(EX_USAGE,
				    "invalid count of packets to transmit: `%s'",
				    optarg);
			npackets = ultmp;
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (uid) {
				errno = EPERM;
				err(EX_NOPERM, "-f flag");
			}
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'i':		/* wait between sending packets */
			t = strtod(optarg, &ep) * 1000.0;
			if (*ep || ep == optarg || t > (double)INT_MAX)
				errx(EX_USAGE, "invalid timing interval: `%s'",
				    optarg);
			options |= F_INTERVAL;
			interval = (int)t;
			if (uid && interval < 1000) {
				errno = EPERM;
				err(EX_NOPERM, "-i interval too short");
			}
			break;
		case 'I':		/* multicast interface */
			if (inet_aton(optarg, &ifaddr) == 0)
				errx(EX_USAGE,
				    "invalid multicast interface: `%s'",
				    optarg);
			options |= F_MIF;
			break;
		case 'l':
			ultmp = strtoul(optarg, &ep, 0);
			if (*ep || ep == optarg || ultmp > INT_MAX)
				errx(EX_USAGE,
				    "invalid preload value: `%s'", optarg);
			if (uid) {
				errno = EPERM;
				err(EX_NOPERM, "-l flag");
			}
			preload = ultmp;
			break;
		case 'L':
			options |= F_NOLOOP;
			loop = 0;
			break;
		case 'm':		/* TTL */
			ultmp = strtoul(optarg, &ep, 0);
			if (*ep || ep == optarg || ultmp > 255)
				errx(EX_USAGE, "invalid TTL: `%s'", optarg);
			ttl = ultmp;
			options |= F_TTL;
			break;
		case 'n':
			options |= F_NUMERIC;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'Q':
			options |= F_QUIET2;
			break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 'r':
			options |= F_SO_DONTROUTE;
			break;
		case 's':		/* size of packet to send */
			if (uid) {
				errno = EPERM;
				err(EX_NOPERM, "-s flag");
			}
			ultmp = strtoul(optarg, &ep, 0);
			if (ultmp > MAXPAYLOAD)
				errx(EX_USAGE,
				    "packet size too large: %lu > %u",
				    ultmp, MAXPAYLOAD);
			if (*ep || ep == optarg)
				errx(EX_USAGE, "invalid packet size: `%s'",
				    optarg);
			datalen = ultmp;
			break;
		case 'S':
			source = optarg;
			break;
		case 't':
			alarmtimeout = strtoul(optarg, &ep, 0);
			if ((alarmtimeout < 1) || (alarmtimeout == ULONG_MAX))
				errx(EX_USAGE, "invalid timeout: `%s'",
				    optarg);
			if (alarmtimeout > MAXALARM)
				errx(EX_USAGE, "invalid timeout: `%s' > %d",
				    optarg, MAXALARM);
			alarm((int)alarmtimeout);
			break;
		case 'T':		/* multicast TTL */
			ultmp = strtoul(optarg, &ep, 0);
			if (*ep || ep == optarg || ultmp > 255)
				errx(EX_USAGE, "invalid multicast TTL: `%s'",
				    optarg);
			mttl = ultmp;
			options |= F_MTTL;
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		case 'P':
			options |= F_POLICY;
			if (!strncmp("in", optarg, 2))
				policy_in = strdup(optarg);
			else if (!strncmp("out", optarg, 3))
				policy_out = strdup(optarg);
			else
				errx(1, "invalid security policy");
			break;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		default:
			usage();
		}
	}

	if (argc - optind != 1)
		usage();
	target = argv[optind];

	if (source) {
		bzero((char *)&src_in, sizeof(src_in));
		src_in.sin_family = AF_INET;
		if (inet_aton(source, &src_in.sin_addr) != 0) {
			shostname = source;
		} else {
			hp = gethostbyname2(source, AF_INET);
			if (!hp)
				errx(EX_NOHOST, "cannot resolve %s: %s",
				    source, hstrerror(h_errno));

			src_in.sin_len = sizeof src_in;
			if (hp->h_length > (int)sizeof(src_in.sin_addr) ||
			    hp->h_length < 0)
				errx(1, "gethostbyname2: illegal address");
			memcpy(&src_in.sin_addr, hp->h_addr_list[0],
			    sizeof(src_in.sin_addr));
			strncpy(snamebuf, hp->h_name,
			    sizeof(snamebuf) - 1);
			snamebuf[sizeof(snamebuf) - 1] = '\0';
			shostname = snamebuf;
		}
		if (bind(s, (struct sockaddr *)&src_in, sizeof src_in) == -1)
			err(1, "bind");
	}

	bzero(&whereto, sizeof(whereto));
	to = &whereto;
	to->sin_family = AF_INET;
	to->sin_len = sizeof *to;
	if (inet_aton(target, &to->sin_addr) != 0) {
		hostname = target;
	} else {
		hp = gethostbyname2(target, AF_INET);
		if (!hp)
			errx(EX_NOHOST, "cannot resolve %s: %s",
			    target, hstrerror(h_errno));

		if (hp->h_length > (int)sizeof(to->sin_addr))
			errx(1, "gethostbyname2 returned an illegal address");
		memcpy(&to->sin_addr, hp->h_addr_list[0], sizeof to->sin_addr);
		strncpy(hnamebuf, hp->h_name, sizeof(hnamebuf) - 1);
		hnamebuf[sizeof(hnamebuf) - 1] = '\0';
		hostname = hnamebuf;
	}

	if (options & F_FLOOD && options & F_INTERVAL)
		errx(EX_USAGE, "-f and -i: incompatible options");

	if (options & F_FLOOD && IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
		errx(EX_USAGE,
		    "-f flag cannot be used with multicast destination");
	if (options & (F_MIF | F_NOLOOP | F_MTTL)
	    && !IN_MULTICAST(ntohl(to->sin_addr.s_addr)))
		errx(EX_USAGE,
		    "-I, -L, -T flags cannot be used with unicast destination");

	if (datalen >= PHDR_LEN)	/* can we time transfer */
		timing = 1;
	packlen = MAXIPLEN + MAXICMPLEN + datalen;
	packlen = packlen > IP_MAXPACKET ? IP_MAXPACKET : packlen;

	if (!(options & F_PINGFILLED))
		for (i = PHDR_LEN; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if (s < 0) {
		errno = sockerrno;
		err(EX_OSERR, "socket");
	}
	hold = 1;
	if (options & F_SO_DEBUG)
		setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	if (options & F_SO_DONTROUTE)
		setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *)&hold,
		    sizeof(hold));
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (options & F_POLICY) {
		char *buf;
		if (policy_in != NULL) {
			buf = ipsec_set_policy(policy_in, strlen(policy_in));
			if (buf == NULL)
				errx(EX_CONFIG, "%s", ipsec_strerror());
			if (setsockopt(s, IPPROTO_IP, IP_IPSEC_POLICY,
					buf, ipsec_get_policylen(buf)) < 0)
				err(EX_CONFIG,
				    "ipsec policy cannot be configured");
			free(buf);
		}

		if (policy_out != NULL) {
			buf = ipsec_set_policy(policy_out, strlen(policy_out));
			if (buf == NULL)
				errx(EX_CONFIG, "%s", ipsec_strerror());
			if (setsockopt(s, IPPROTO_IP, IP_IPSEC_POLICY,
					buf, ipsec_get_policylen(buf)) < 0)
				err(EX_CONFIG,
				    "ipsec policy cannot be configured");
			free(buf);
		}
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

	/* record route option */
	if (options & F_RROUTE) {
#ifdef IP_OPTIONS
		bzero(rspace, sizeof(rspace));
		rspace[IPOPT_OPTVAL] = IPOPT_RR;
		rspace[IPOPT_OLEN] = sizeof(rspace) - 1;
		rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		rspace[sizeof(rspace) - 1] = IPOPT_EOL;
		if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, rspace,
		    sizeof(rspace)) < 0)
			err(EX_OSERR, "setsockopt IP_OPTIONS");
#else
		errx(EX_UNAVAILABLE,
		    "record route not available in this implementation");
#endif /* IP_OPTIONS */
	}

	if (options & F_TTL) {
		if (setsockopt(s, IPPROTO_IP, IP_TTL, &ttl,
		    sizeof(ttl)) < 0) {
			err(EX_OSERR, "setsockopt IP_TTL");
		}
	}
	if (options & F_NOLOOP) {
		if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
		    sizeof(loop)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_LOOP");
		}
	}
	if (options & F_MTTL) {
		if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &mttl,
		    sizeof(mttl)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_TTL");
		}
	}
	if (options & F_MIF) {
		if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, &ifaddr,
		    sizeof(ifaddr)) < 0) {
			err(EX_OSERR, "setsockopt IP_MULTICAST_IF");
		}
	}
#ifdef SO_TIMESTAMP
	{ int on = 1;
	if (setsockopt(s, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)) < 0)
		err(EX_OSERR, "setsockopt SO_TIMESTAMP");
	}
#endif

	/*
	 * When pinging the broadcast address, you can get a lot of answers.
	 * Doing something so evil is useful if you are trying to stress the
	 * ethernet, or just want to fill the arp cache to get some stuff for
	 * /etc/ethers.  But beware: RFC 1122 allows hosts to ignore broadcast
	 * or multicast pings if they wish.
	 */

	/*
	 * XXX receive buffer needs undetermined space for mbuf overhead
	 * as well.
	 */
	hold = IP_MAXPACKET + 128;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold, sizeof(hold));

	if (!uid) {
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&hold,
		    sizeof(hold));
	}

	if (to->sin_family == AF_INET) {
		printf("PING %s (%s)", hostname, inet_ntoa(to->sin_addr));
		if (source)
			printf(" from %s", shostname);
		printf(": %d data bytes\n", datalen);
	} else
		printf("PING %s: %d data bytes\n", hostname, datalen);

	/*
	 * Use sigaction() instead of signal() to get unambiguous semantics,
	 * in particular with SA_RESTART not set.
	 */

	sigemptyset(&si_sa.sa_mask);
	si_sa.sa_flags = 0;

	si_sa.sa_handler = stopit;
	if (sigaction(SIGINT, &si_sa, 0) == -1) {
		err(EX_OSERR, "sigaction SIGINT");
	}

	si_sa.sa_handler = status;
	if (sigaction(SIGINFO, &si_sa, 0) == -1) {
		err(EX_OSERR, "sigaction");
	}

        if (alarmtimeout > 0) {
		si_sa.sa_handler = stopit;
		if (sigaction(SIGALRM, &si_sa, 0) == -1)
			err(EX_OSERR, "sigaction SIGALRM");
        }

	bzero(&msg, sizeof(msg));
	msg.msg_name = (caddr_t)&from;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
#ifdef SO_TIMESTAMP
	msg.msg_control = (caddr_t)ctrl;
#endif
	iov.iov_base = packet;
	iov.iov_len = packlen;

	if (tcgetattr(STDOUT_FILENO, &ts) != -1) {
		reset_kerninfo = !(ts.c_lflag & NOKERNINFO);
		ts.c_lflag |= NOKERNINFO;
		tcsetattr(STDOUT_FILENO, TCSANOW, &ts);
	}

	if (preload == 0)
		pinger();		/* send the first ping */
	else {
		if (npackets != 0 && preload > npackets)
			preload = npackets;
		while (preload--)	/* fire off them quickies */
			pinger();
	}
	gettimeofday(&last, NULL);

	if (options & F_FLOOD) {
		intvl.tv_sec = 0;
		intvl.tv_usec = 10000;
	} else {
		intvl.tv_sec = interval / 1000;
		intvl.tv_usec = interval % 1000 * 1000;
	}

	while (!finish_up) {
		int cc;
		int n;
		struct timeval timeout, now;
		fd_set rfds;

		check_status();
		if (s >= FD_SETSIZE)
			errx(EX_OSERR, "descriptor too large");
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		gettimeofday(&now, NULL);
		timeout.tv_sec = last.tv_sec + intvl.tv_sec - now.tv_sec;
		timeout.tv_usec = last.tv_usec + intvl.tv_usec - now.tv_usec;
		while (timeout.tv_usec < 0) {
			timeout.tv_usec += 1000000;
			timeout.tv_sec--;
		}
		while (timeout.tv_usec >= 1000000) {
			timeout.tv_usec -= 1000000;
			timeout.tv_sec++;
		}
		if (timeout.tv_sec < 0)
			timeout.tv_sec = timeout.tv_usec = 0;
		n = select(s + 1, &rfds, NULL, NULL, &timeout);
		if (n < 0)
			continue;	/* Must be EINTR. */
		if (n == 1) {
			struct timeval *tv = 0;
#ifdef SO_TIMESTAMP
			struct cmsghdr *cmsg = (struct cmsghdr *)&ctrl;

			msg.msg_controllen = sizeof(ctrl);
#endif
			msg.msg_namelen = sizeof(from);
			if ((cc = recvmsg(s, &msg, 0)) < 0) {
				if (errno == EINTR)
					continue;
				warn("recvmsg");
				continue;
			}
#ifdef SO_TIMESTAMP
			if (cmsg->cmsg_level == SOL_SOCKET &&
			    cmsg->cmsg_type == SCM_TIMESTAMP &&
			    cmsg->cmsg_len == CMSG_LEN(sizeof *tv)) {
				/* Copy to avoid alignment problems: */
				memcpy(&now,CMSG_DATA(cmsg),sizeof(now));
				tv = &now;
			}
#endif
			if (tv == 0) {
				gettimeofday(&now, NULL);
				tv = &now;
			}
			pr_pack((char *)packet, cc, &from, tv);
			if (npackets && nreceived >= npackets)
				break;
		}
		if (n == 0 || options & F_FLOOD) {
			if (!npackets || ntransmitted < npackets)
				pinger();
			else {
				if (almost_done)
					break;
				almost_done = 1;
				intvl.tv_usec = 0;
				if (nreceived) {
					intvl.tv_sec = 2 * tmax / 1000;
					if (!intvl.tv_sec)
						intvl.tv_sec = 1;
				} else
					intvl.tv_sec = MAXWAIT;
			}
			gettimeofday(&last, NULL);

			if (ntransmitted - nreceived - 1 > nmissedmax) {
				nmissedmax = ntransmitted - nreceived - 1;
				if (options & F_MISSED)
					write(STDOUT_FILENO, &BBELL, 1);
			}
		}
	}
	finish();
	/* NOTREACHED */
	exit(0);	/* Make the compiler happy */
}

/*
 * stopit --
 *	Set the global bit that causes the main loop to quit.
 * Do NOT call finish() from here, since finish() does far too much
 * to be called from a signal handler.
 */
void
stopit(int sig __unused)
{

	finish_up = 1;
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first PHDR_LEN
 * bytes of the data portion are used to hold a UNIX "timeval" struct in
 * host byte-order, to compute the round-trip time.
 */
static void
pinger(void)
{
	struct icmp *icp;
	int cc, i;

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = ntransmitted;
	icp->icmp_id = ident;			/* ID */

	CLR(icp->icmp_seq % mx_dup_ck);

	if (timing)
		gettimeofday((struct timeval *)&outpack[MINICMPLEN],
		    (struct timezone *)NULL);

	cc = MINICMPLEN + datalen;

	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	i = sendto(s, (char *)outpack, cc, 0, (struct sockaddr *)&whereto,
	    sizeof(whereto));

	if (i < 0 || i != cc)  {
		if (i < 0) {
			if (options & F_FLOOD && errno == ENOBUFS) {
				usleep(FLOOD_BACKOFF);
				return;
			}
			warn("sendto");
		} else {
			warn("%s: partial write: %d of %d bytes",
			     hostname, i, cc);
		}
	}
	ntransmitted++;
	if (!(options & F_QUIET) && options & F_FLOOD)
		write(STDOUT_FILENO, &DOT, 1);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
static void
pr_pack(char *buf, int cc, struct sockaddr_in *from, struct timeval *tv)
{
	struct icmp *icp;
	struct ip *ip;
	struct in_addr ina;
	struct timeval *tp;
	u_char *cp, *dp;
	double triptime;
	int dupflag, hlen, i, j;
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];

	/* Check the IP header */
	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (options & F_VERBOSE)
			warn("packet too short (%d bytes) from %s", cc,
			     inet_ntoa(from->sin_addr));
		return;
	}

	/* Now the ICMP part */
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	if (icp->icmp_type == ICMP_ECHOREPLY) {
		if (icp->icmp_id != ident)
			return;			/* 'Twas not our ECHO */
		++nreceived;
		triptime = 0.0;
		if (timing) {
			struct timeval tv1;
#ifndef icmp_data
			tp = (struct timeval *)&icp->icmp_ip;
#else
			tp = (struct timeval *)icp->icmp_data;
#endif
			/* Avoid unaligned data: */
			memcpy(&tv1,tp,sizeof(tv1));
			tvsub(tv, &tv1);
 			triptime = ((double)tv->tv_sec) * 1000.0 +
 			    ((double)tv->tv_usec) / 1000.0;
			tsum += triptime;
			tsumsq += triptime * triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(icp->icmp_seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icp->icmp_seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			write(STDOUT_FILENO, &BSPACE, 1);
		else {
			printf("%d bytes from %s: icmp_seq=%u", cc,
			   inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr),
			   icp->icmp_seq);
			printf(" ttl=%d", ip->ip_ttl);
			if (timing)
				printf(" time=%.3f ms", triptime);
			if (dupflag)
				printf(" (DUP!)");
			if (options & F_AUDIBLE)
				write(STDOUT_FILENO, &BBELL, 1);
			/* check the data */
			cp = (u_char*)&icp->icmp_data[PHDR_LEN];
			dp = &outpack[MINICMPLEN + PHDR_LEN];
			for (i = PHDR_LEN; i < datalen; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
	printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
	    i, *dp, *cp);
					printf("\ncp:");
					cp = (u_char*)&icp->icmp_data[0];
					for (i = 0; i < datalen; ++i, ++cp) {
						if ((i % 32) == 8)
							printf("\n\t");
						printf("%x ", *cp);
					}
					printf("\ndp:");
					cp = &outpack[MINICMPLEN];
					for (i = 0; i < datalen; ++i, ++cp) {
						if ((i % 32) == 8)
							printf("\n\t");
						printf("%x ", *cp);
					}
					break;
				}
			}
		}
	} else {
		/*
		 * We've got something other than an ECHOREPLY.
		 * See if it's a reply to something that we sent.
		 * We can compare IP destination, protocol,
		 * and ICMP type and ID.
		 *
		 * Only print all the error messages if we are running
		 * as root to avoid leaking information not normally
		 * available to those not running as root.
		 */
#ifndef icmp_data
		struct ip *oip = &icp->icmp_ip;
#else
		struct ip *oip = (struct ip *)icp->icmp_data;
#endif
		struct icmp *oicmp = (struct icmp *)(oip + 1);

		if (((options & F_VERBOSE) && uid == 0) ||
		    (!(options & F_QUIET2) &&
		     (oip->ip_dst.s_addr == whereto.sin_addr.s_addr) &&
		     (oip->ip_p == IPPROTO_ICMP) &&
		     (oicmp->icmp_type == ICMP_ECHO) &&
		     (oicmp->icmp_id == ident))) {
		    printf("%d bytes from %s: ", cc,
			pr_addr(from->sin_addr));
		    pr_icmph(icp);
		} else
		    return;
	}

	/* Display any IP options */
	cp = (u_char *)buf + sizeof(struct ip);

	for (; hlen > (int)sizeof(struct ip); --hlen, ++cp)
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
			printf("\nLSRR: ");
			j = cp[IPOPT_OLEN] - IPOPT_MINOFF + 1;
			hlen -= 2;
			cp += 2;
			if (j >= INADDR_LEN &&
			    j <= hlen - (int)sizeof(struct ip)) {
				for (;;) {
					bcopy(++cp, &ina.s_addr, INADDR_LEN);
					if (ina.s_addr == 0)
						printf("\t0.0.0.0");
					else
						printf("\t%s", pr_addr(ina));
					hlen -= INADDR_LEN;
					cp += INADDR_LEN - 1;
					j -= INADDR_LEN;
					if (j < INADDR_LEN)
						break;
					putchar('\n');
				}
			} else
				printf("\t(truncated route)\n");
			break;
		case IPOPT_RR:
			j = cp[IPOPT_OLEN];		/* get length */
			i = cp[IPOPT_OFFSET];		/* and pointer */
			hlen -= 2;
			cp += 2;
			if (i > j)
				i = j;
			i = i - IPOPT_MINOFF + 1;
			if (i < 0 || i > (hlen - (int)sizeof(struct ip))) {
				old_rrlen = 0;
				continue;
			}
			if (i == old_rrlen
			    && !bcmp((char *)cp, old_rr, i)
			    && !(options & F_FLOOD)) {
				printf("\t(same route)");
				hlen -= i;
				cp += i;
				break;
			}
			old_rrlen = i;
			bcopy((char *)cp, old_rr, i);

			printf("\nRR: ");

			if (i >= INADDR_LEN &&
			    i <= hlen - (int)sizeof(struct ip)) {
				for (;;) {
					bcopy(++cp, &ina.s_addr, INADDR_LEN);
					if (ina.s_addr == 0)
						printf("\t0.0.0.0");
					else
						printf("\t%s", pr_addr(ina));
					hlen -= INADDR_LEN;
					cp += INADDR_LEN - 1;
					i -= INADDR_LEN;
					if (i < INADDR_LEN)
						break;
					putchar('\n');
				}
			} else
				printf("\t(truncated route)");
			break;
		case IPOPT_NOP:
			printf("\nNOP");
			break;
		default:
			printf("\nunknown option %x", *cp);
			break;
		}
	if (!(options & F_FLOOD)) {
		putchar('\n');
		fflush(stdout);
	}
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(u_short *addr, int len)
{
	int nleft, sum;
	u_short *w;
	union {
		u_short	us;
		u_char	uc[2];
	} last;
	u_short answer;

	nleft = len;
	sum = 0;
	w = addr;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		last.uc[0] = *(u_char *)w;
		last.uc[1] = 0;
		sum += last.us;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
static void
tvsub(struct timeval *out, struct timeval *in)
{

	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * status --
 *	Print out statistics when SIGINFO is received.
 */

static void
status(int sig __unused)
{

	siginfo_p = 1;
}

static void
check_status(void)
{

	if (siginfo_p) {
		siginfo_p = 0;
		fprintf(stderr,
	"\r%ld/%ld packets received (%.0f%%) %.3f min / %.3f avg / %.3f max\n",
		    nreceived, ntransmitted,
		    ntransmitted ? nreceived * 100.0 / ntransmitted : 0.0,
		    nreceived ? tmin : 0.0,
		    nreceived + nrepeats ? tsum / (nreceived + nrepeats) : tsum,
		    tmax);
	}
}

/*
 * finish --
 *	Print out statistics, and give up.
 */
static void
finish(void)
{

	struct termios ts;

	signal(SIGINT, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	putchar('\n');
	fflush(stdout);
	printf("--- %s ping statistics ---\n", hostname);
	printf("%ld packets transmitted, ", ntransmitted);
	printf("%ld packets received, ", nreceived);
	if (nrepeats)
		printf("+%ld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			printf("-- somebody's printing up packets!");
		else
			printf("%d%% packet loss",
			    (int)(((ntransmitted - nreceived) * 100) /
			    ntransmitted));
	}
	putchar('\n');
	if (nreceived && timing) {
		double n = nreceived + nrepeats;
		double avg = tsum / n;
		double vari = tsumsq / n - avg * avg;
		printf(
		    "round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax, sqrt(vari));
	}
	if (reset_kerninfo && tcgetattr(STDOUT_FILENO, &ts) != -1) {
		ts.c_lflag &= ~NOKERNINFO;
		tcsetattr(STDOUT_FILENO, TCSANOW, &ts);
	}

	if (nreceived)
		exit(0);
	else
		exit(2);
}

#ifdef notdef
static char *ttab[] = {
	"Echo Reply",		/* ip + seq + udata */
	"Dest Unreachable",	/* net, host, proto, port, frag, sr + IP */
	"Source Quench",	/* IP */
	"Redirect",		/* redirect type, gateway, + IP  */
	"Echo",
	"Time Exceeded",	/* transit, frag reassem + IP */
	"Parameter Problem",	/* pointer + IP */
	"Timestamp",		/* id + seq + three timestamps */
	"Timestamp Reply",	/* " */
	"Info Request",		/* id + sq */
	"Info Reply"		/* " */
};
#endif

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
static void
pr_icmph(struct icmp *icp)
{

	switch(icp->icmp_type) {
	case ICMP_ECHOREPLY:
		printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_UNREACH:
		switch(icp->icmp_code) {
		case ICMP_UNREACH_NET:
			printf("Destination Net Unreachable\n");
			break;
		case ICMP_UNREACH_HOST:
			printf("Destination Host Unreachable\n");
			break;
		case ICMP_UNREACH_PROTOCOL:
			printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_UNREACH_PORT:
			printf("Destination Port Unreachable\n");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			printf("frag needed and DF set (MTU %d)\n",
					ntohs(icp->icmp_nextmtu));
			break;
		case ICMP_UNREACH_SRCFAIL:
			printf("Source Route Failed\n");
			break;
		case ICMP_UNREACH_FILTER_PROHIB:
			printf("Communication prohibited by filter\n");
			break;
		default:
			printf("Dest Unreachable, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_SOURCEQUENCH:
		printf("Source Quench\n");
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_REDIRECT:
		switch(icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			printf("Redirect Network");
			break;
		case ICMP_REDIRECT_HOST:
			printf("Redirect Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			printf("Redirect Type of Service and Host");
			break;
		default:
			printf("Redirect, Bad Code: %d", icp->icmp_code);
			break;
		}
		printf("(New addr: %s)\n", inet_ntoa(icp->icmp_gwaddr));
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_ECHO:
		printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_TIMXCEED:
		switch(icp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			printf("Time to live exceeded\n");
			break;
		case ICMP_TIMXCEED_REASS:
			printf("Frag reassembly time exceeded\n");
			break;
		default:
			printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_PARAMPROB:
		printf("Parameter problem: pointer = 0x%02x\n",
		    icp->icmp_hun.ih_pptr);
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_TSTAMP:
		printf("Timestamp\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_TSTAMPREPLY:
		printf("Timestamp Reply\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_IREQ:
		printf("Information Request\n");
		/* XXX ID + Seq */
		break;
	case ICMP_IREQREPLY:
		printf("Information Reply\n");
		/* XXX ID + Seq */
		break;
	case ICMP_MASKREQ:
		printf("Address Mask Request\n");
		break;
	case ICMP_MASKREPLY:
		printf("Address Mask Reply\n");
		break;
	case ICMP_ROUTERADVERT:
		printf("Router Advertisement\n");
		break;
	case ICMP_ROUTERSOLICIT:
		printf("Router Solicitation\n");
		break;
	default:
		printf("Bad ICMP type: %d\n", icp->icmp_type);
	}
}

/*
 * pr_iph --
 *	Print an IP header with options.
 */
static void
pr_iph(struct ip *ip)
{
	u_char *cp;
	int hlen;

	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + 20;		/* point to options */

	printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst\n");
	printf(" %1x  %1x  %02x %04x %04x",
	    ip->ip_v, ip->ip_hl, ip->ip_tos, ntohs(ip->ip_len),
	    ntohs(ip->ip_id));
	printf("   %1lx %04lx",
	    (u_long) (ntohl(ip->ip_off) & 0xe000) >> 13,
	    (u_long) ntohl(ip->ip_off) & 0x1fff);
	printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ntohs(ip->ip_sum));
	printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_src.s_addr));
	printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_dst.s_addr));
	/* dump any option bytes */
	while (hlen-- > 20) {
		printf("%02x", *cp++);
	}
	putchar('\n');
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
static char *
pr_addr(struct in_addr ina)
{
	struct hostent *hp;
	static char buf[16 + 3 + MAXHOSTNAMELEN];

	if ((options & F_NUMERIC) ||
	    !(hp = gethostbyaddr((char *)&ina, 4, AF_INET)))
		return inet_ntoa(ina);
	else
		snprintf(buf, sizeof(buf), "%s (%s)", hp->h_name,
		    inet_ntoa(ina));
	return(buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMP) IP packet.
 */
static void
pr_retip(struct ip *ip)
{
	u_char *cp;
	int hlen;

	pr_iph(ip);
	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + hlen;

	if (ip->ip_p == 6)
		printf("TCP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (ip->ip_p == 17)
		printf("UDP: from port %u, to port %u (decimal)\n",
			(*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

static void
fill(char *bp, char *patp)
{
	char *cp;
	int pat[16];
	u_int ii, jj, kk;

	for (cp = patp; *cp; cp++) {
		if (!isxdigit(*cp))
			errx(EX_USAGE,
			    "patterns must be specified as hex digits");

	}
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0; kk <= MAXPAYLOAD - (PHDR_LEN + ii); kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			printf("%02x", bp[jj] & 0xFF);
		printf("\n");
	}
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
"usage: ping [-AQRadfnqrv] [-c count] [-i wait] [-l preload] [-m ttl]",
"            [-p pattern] "
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
"[-P policy] "
#endif
#endif
"[-s packetsize] [-S src_addr] [-t timeout]",
"            [host | [-L] [-I iface] [-T ttl] mcast-group]");
	exit(EX_USAGE);
}
