/*
 * Copyright (c) 1996, Nickolay Dudorov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD: src/sbin/nos-tun/nos-tun.c,v 1.6.2.2 2001/08/01 23:14:00 obrien Exp $
 * $DragonFly: src/sbin/nos-tun/nos-tun.c,v 1.5 2004/12/18 21:43:39 swildner Exp $
 */

/*
 *  'nos-tun' program configure tunN interface as a point-to-point
 *  connection with two "pseudo"-addresses between this host and
 *  'target'.
 *
 *  It uses Ip-over-Ip incapsulation ( protocol number 94 - IPIP)
 *  (known as NOS-incapsulation in CISCO-routers' terminology).
 *
 *  'nos-tun' can works with itself and CISCO-routers.
 *  (It may also work with Linux 'nos-tun's, but
 *  I have no Linux system here to test with).
 *
 *  BUGS (or features ?):
 *  - you must specify ONE of the target host's addresses
 *    ( nos-tun sends and accepts packets only to/from this
 *      address )
 *  - there can be only ONE tunnel between two hosts,
 *    more precisely - between given host and (one of)
 *    target hosts' address(es)
 *    (and why do you want more ?)
 */

/*
 * Mar. 23 1999 by Isao SEKI <iseki@gongon.com>
 * I added a new flag for ip protocol number.
 * We are using 4 as protocol number in ampr.org.
 *
 */

#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>

/* Tunnel interface configuration stuff */
static struct ifaliasreq ifra;
static struct ifreq ifrq;

/* Global descriptors */
int net;                          /* socket descriptor */
int tun;                          /* tunnel descriptor */

static void usage(void);

int Set_address(char *addr, struct sockaddr_in *sin)
{
  struct hostent *hp;

  bzero((char *)sin, sizeof(struct sockaddr));
  sin->sin_family = AF_INET;
  if((sin->sin_addr.s_addr = inet_addr(addr)) == (u_long)-1) {
    hp = gethostbyname(addr);
    if (!hp) {
      syslog(LOG_ERR,"unknown host %s", addr);
      return 1;
    }
    sin->sin_family = hp->h_addrtype;
    bcopy(hp->h_addr, (caddr_t)&sin->sin_addr, hp->h_length);
  }
  return 0;
}

int tun_open(char *devname, struct sockaddr *ouraddr, char *theiraddr)
{
  int s;
  struct sockaddr_in *sin;

  /* Open tun device */
  tun = open (devname, O_RDWR);
  if (tun < 0) {
    syslog(LOG_ERR,"can't open %s - %m",devname);
    return(1);
  }

  /*
   * At first, name the interface.
   */
  bzero((char *)&ifra, sizeof(ifra));
  bzero((char *)&ifrq, sizeof(ifrq));

  strncpy(ifrq.ifr_name, devname+5, IFNAMSIZ);
  strncpy(ifra.ifra_name, devname+5, IFNAMSIZ);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    syslog(LOG_ERR,"can't open socket - %m");
    goto tunc_return;
  }

  /*
   *  Delete (previous) addresses for interface
   *
   *  !!!!
   *  On FreeBSD this ioctl returns error
   *  when tunN have no addresses, so - log and ignore it.
   *
   */
  if (ioctl(s, SIOCDIFADDR, &ifra) < 0) {
    syslog(LOG_ERR,"SIOCDIFADDR - %m");
  }

  /*
   *  Set interface address
   */
  sin = (struct sockaddr_in *)&(ifra.ifra_addr);
  bcopy(ouraddr, sin, sizeof(struct sockaddr_in));
  sin->sin_len = sizeof(*sin);

  /*
   *  Set destination address
   */
  sin = (struct sockaddr_in *)&(ifra.ifra_broadaddr);
  if(Set_address(theiraddr,sin)) {
    syslog(LOG_ERR,"bad destination address: %s",theiraddr);
    goto stunc_return;
  }
  sin->sin_len = sizeof(*sin);

  if (ioctl(s, SIOCAIFADDR, &ifra) < 0) {
    syslog(LOG_ERR,"can't set interface address - %m");
    goto stunc_return;
  }

  /*
   *  Now, bring up the interface.
   */
  if (ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    syslog(LOG_ERR,"can't get interface flags - %m");
    goto stunc_return;
  }

  ifrq.ifr_flags |= IFF_UP;
  if (!(ioctl(s, SIOCSIFFLAGS, &ifrq) < 0)) {
    close(s);
    return(0);
  }
  syslog(LOG_ERR,"can't set interface UP - %m");
stunc_return:
  close(s);
tunc_return:
  close(tun);
  return(1);
}

void Finish(int signum)
{
  int s;

  syslog(LOG_INFO,"exiting");

  close(net);

  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    syslog(LOG_ERR,"can't open socket - %m");
    goto closing_tun;
  }

  /*
   *  Shut down interface.
   */
  if (ioctl(s, SIOCGIFFLAGS, &ifrq) < 0) {
    syslog(LOG_ERR,"can't get interface flags - %m");
    goto closing_fds;
  }

  ifrq.ifr_flags &= ~(IFF_UP|IFF_RUNNING);
  if (ioctl(s, SIOCSIFFLAGS, &ifrq) < 0) {
    syslog(LOG_ERR,"can't set interface DOWN - %m");
    goto closing_fds;
  }

  /*
   *  Delete addresses for interface
   */
  bzero(&ifra.ifra_addr, sizeof(ifra.ifra_addr));
  bzero(&ifra.ifra_broadaddr, sizeof(ifra.ifra_addr));
  bzero(&ifra.ifra_mask, sizeof(ifra.ifra_addr));
  if (ioctl(s, SIOCDIFADDR, &ifra) < 0) {
    syslog(LOG_ERR,"can't delete interface's addresses - %m");
  }
closing_fds:
  close(s);
closing_tun:
  close(tun);
  closelog();
  exit(signum);
}

int main (int argc, char **argv)
{
  int  c, len, ipoff;

  char *devname = NULL;
  char *point_to = NULL;
  char *to_point = NULL;
  char *target;
  char *protocol = NULL;
  int protnum;

  struct sockaddr t_laddr;          /* Source address of tunnel */
  struct sockaddr whereto;          /* Destination of tunnel */
  struct sockaddr_in *to;

  char buf[0x2000];                 /* Packets buffer */
  struct ip *ip = (struct ip *)buf;

  fd_set rfds, wfds, efds;          /* File descriptors for select() */
  int nfds;                         /* Return from select() */


  while ((c = getopt(argc, argv, "d:s:t:p:")) != -1) {
    switch (c) {
    case 'd':
      to_point = optarg;
      break;
    case 's':
      point_to = optarg;
      break;
    case 't':
      devname = optarg;
      break;
    case 'p':
      protocol = optarg;
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (argc != 1 || (devname == NULL) ||
      (point_to == NULL) || (to_point == NULL)) {
    usage();
  }

  if(protocol == NULL)
      protnum = 94;
  else
      protnum = atoi(protocol);

  target = *argv;

  /* Establish logging through 'syslog' */
  openlog("nos-tun", LOG_PID, LOG_DAEMON);

  if(Set_address(point_to, (struct sockaddr_in *)&t_laddr)) {
    closelog();
    exit(2);
  }

  if(tun_open(devname, &t_laddr, to_point)) {
    closelog();
    exit(3);
  }

  to = (struct sockaddr_in *)&whereto;
  if(Set_address(target, to))
    Finish(4);

  if ((net = socket(AF_INET, SOCK_RAW, protnum)) < 0) {
    syslog(LOG_ERR,"can't open socket - %m");
    Finish(5);
  }

  if (connect(net,&whereto,sizeof(struct sockaddr_in)) < 0 ) {
    syslog(LOG_ERR,"can't connect to target - %m");
    close(net);
    Finish(6);
  }

  /*  Demonize it */
  daemon(0,0);

  /* Install signal handlers */
  signal(SIGHUP,Finish);
  signal(SIGINT,Finish);
  signal(SIGTERM,Finish);

  for (;;) {
    /* Set file descriptors for select() */
    FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
    FD_SET(tun,&rfds); FD_SET(net,&rfds);

    nfds = select(net+10,&rfds,&wfds,&efds,NULL);
    if(nfds < 0) {
      syslog(LOG_ERR,"interrupted select");
      close(net);
      Finish(7);
    }
    if(nfds == 0) {         /* Impossible ? */
      syslog(LOG_ERR,"timeout in select");
      close(net);
      Finish(8);
    }


    if(FD_ISSET(net,&rfds)) {
      /* Read from socket ... */
      len = read(net, buf, sizeof(buf));
      /* Check if this is "our" packet */
      if((ip->ip_src).s_addr == (to->sin_addr).s_addr) {
	/* ... skip encapsulation headers ... */
	ipoff = (ip->ip_hl << 2);
	/* ... and write to tun-device */
	write(tun,buf+ipoff,len-ipoff);
      }
    }

    if(FD_ISSET(tun,&rfds)) {
      /* Read from tun ... */
      len = read(tun, buf, sizeof(buf));
      /* ... and send to network */
      if(send(net, buf, len,0) <= 0) {
	syslog(LOG_ERR,"can't send - %m");
      }
    }
  }
}

static void
usage(void)
{
	fprintf(stderr,
"usage: nos-tun -t <tun_name> -s <source_addr> -d <dest_addr> -p <protocol_number> <target_addr>\n");
	exit(1);
}

