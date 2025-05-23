/* $FreeBSD: src/contrib/ipfilter/ipsend/sock.c,v 1.3.2.4 2003/03/01 03:55:53 darrenr Exp $ */
/* $DragonFly: src/contrib/ipfilter/ipsend/sock.c,v 1.2 2003/06/17 04:24:02 dillon Exp $ */
/*
 * sock.c (C) 1995-1998 Darren Reed
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if defined(__sgi) && (IRIX > 602)
# include <sys/ptimers.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifndef	ultrix
#include <fcntl.h>
#endif
#if (__FreeBSD_version >= 300000)
# include <sys/dirent.h>
#else
# include <sys/dir.h>
#endif
#define _KERNEL
#define	KERNEL
#ifdef	ultrix
# undef	LOCORE
# include <sys/smp_lock.h>
#endif
#include <sys/file.h>
#undef  _KERNEL
#undef  KERNEL
#include <nlist.h>
#include <sys/user.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#if !defined(ultrix) && !defined(hpux)
# include <kvm.h>
#endif
#ifdef sun
#include <sys/systm.h>
#include <sys/session.h>
#endif
#if BSD >= 199103
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <paths.h>
#endif
#include <math.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include "ipsend.h"

#if !defined(lint)
static const char sccsid[] = "@(#)sock.c	1.2 1/11/96 (C)1995 Darren Reed";
static const char rcsid[] = "@(#)$Id: sock.c,v 2.1.4.6 2002/12/06 11:40:36 darrenr Exp $";
#endif


int	nproc;
struct	proc	*proc;

#ifndef	KMEM
# ifdef	_PATH_KMEM
#  define	KMEM	_PATH_KMEM
# endif
#endif
#ifndef	KERNEL
# ifdef	_PATH_UNIX
#  define	KERNEL	_PATH_UNIX
# endif
#endif
#ifndef	KMEM
# define	KMEM	"/dev/kmem"
#endif
#ifndef	KERNEL
# define	KERNEL	"/vmunix"
#endif


#if BSD < 199103
static	struct	proc	*getproc __P((void));
#else
static	struct	kinfo_proc	*getproc __P((void));
#endif


int	kmemcpy(buf, pos, n)
char	*buf;
void	*pos;
int	n;
{
	static	int	kfd = -1;
	off_t	offset = (u_long)pos;

	if (kfd == -1)
		kfd = open(KMEM, O_RDONLY);

	if (lseek(kfd, offset, SEEK_SET) == -1)
	    {
		perror("lseek");
		return -1;
	    }
	if (read(kfd, buf, n) == -1)
	    {
		perror("read");
		return -1;
	    }
	return n;
}

struct	nlist	names[4] = {
	{ "_proc" },
	{ "_nproc" },
#ifdef	ultrix
	{ "_u" },
#else
	{ NULL },
#endif
	{ NULL }
	};

#if BSD < 199103
static struct proc *getproc()
{
	struct	proc	*p;
	pid_t	pid = getpid();
	int	siz, n;

	n = nlist(KERNEL, names);
	if (n != 0)
	    {
		fprintf(stderr, "nlist(%#x) == %d\n", names, n);
		return NULL;
	    }
	if (KMCPY(&nproc, names[1].n_value, sizeof(nproc)) == -1)
	    {
		fprintf(stderr, "read nproc (%#x)\n", names[1].n_value);
		return NULL;
	    }
	siz = nproc * sizeof(struct proc);
	if (KMCPY(&p, names[0].n_value, sizeof(p)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) proc\n",
			names[0].n_value, &p, sizeof(p));
		return NULL;
	    }
	proc = (struct proc *)malloc(siz);
	if (KMCPY(proc, p, siz) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) proc\n",
			p, proc, siz);
		return NULL;
	    }

	p = proc;

	for (n = nproc; n; n--, p++)
		if (p->p_pid == pid)
			break;
	if (!n)
		return NULL;

	return p;
}


struct	tcpcb	*find_tcp(fd, ti)
int	fd;
struct	tcpiphdr *ti;
{
	struct	tcpcb	*t;
	struct	inpcb	*i;
	struct	socket	*s;
	struct	user	*up;
	struct	proc	*p;
	struct	file	*f, **o;

	if (!(p = getproc()))
		return NULL;
	up = (struct user *)malloc(sizeof(*up));
#ifndef	ultrix
	if (KMCPY(up, p->p_uarea, sizeof(*up)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x) failed\n", p, p->p_uarea);
		return NULL;
	    }
#else
	if (KMCPY(up, names[2].n_value, sizeof(*up)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x) failed\n", p, names[2].n_value);
		return NULL;
	    }
#endif

	o = (struct file **)calloc(1, sizeof(*o) * (up->u_lastfile + 1));
	if (KMCPY(o, up->u_ofile, (up->u_lastfile + 1) * sizeof(*o)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) - u_ofile - failed\n",
			up->u_ofile, o, sizeof(*o));
		return NULL;
	    }
	f = (struct file *)calloc(1, sizeof(*f));
	if (KMCPY(f, o[fd], sizeof(*f)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) - o[fd] - failed\n",
			up->u_ofile[fd], f, sizeof(*f));
		return NULL;
	    }

	s = (struct socket *)calloc(1, sizeof(*s));
	if (KMCPY(s, f->f_data, sizeof(*s)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) - f_data - failed\n",
			o[fd], s, sizeof(*s));
		return NULL;
	    }

	i = (struct inpcb *)calloc(1, sizeof(*i));
	if (KMCPY(i, s->so_pcb, sizeof(*i)) == -1)
	    {
		fprintf(stderr, "kvm_read(%#x,%#x,%d) - so_pcb - failed\n",
			s->so_pcb, i, sizeof(*i));
		return NULL;
	    }

	t = (struct tcpcb *)calloc(1, sizeof(*t));
	if (KMCPY(t, i->inp_ppcb, sizeof(*t)) == -1)
	    {
		fprintf(stderr, "read(%#x,%#x,%d) - inp_ppcb - failed\n",
			i->inp_ppcb, t, sizeof(*t));
		return NULL;
	    }
	return (struct tcpcb *)i->inp_ppcb;
}
#else
static struct kinfo_proc *getproc()
{
	static	struct	kinfo_proc kp;
	pid_t	pid = getpid();
	int	mib[4];
	size_t	n;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	n = sizeof(kp);
	if (sysctl(mib, 4, &kp, &n, NULL, 0) == -1)
	    {
		perror("sysctl");
		return NULL;
	    }
	return &kp;
}


struct	tcpcb	*find_tcp(tfd, ti)
int	tfd;
struct	tcpiphdr *ti;
{
	struct	tcpcb	*t;
	struct	inpcb	*i;
	struct	socket	*s;
	struct	filedesc	*fd;
	struct	kinfo_proc	*p;
	struct	file	*f, **o;

	if (!(p = getproc()))
		return NULL;

	fd = (struct filedesc *)malloc(sizeof(*fd));
#if defined( __FreeBSD_version) && __FreeBSD_version >= 500013
	if (KMCPY(fd, p->ki_fd, sizeof(*fd)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx) failed\n",
			(u_long)p, (u_long)p->ki_fd);
		return NULL;
	    }
#else
	if (KMCPY(fd, p->kp_proc.p_fd, sizeof(*fd)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx) failed\n",
			(u_long)p, (u_long)p->kp_proc.p_fd);
		return NULL;
	    }
#endif

	o = (struct file **)calloc(1, sizeof(*o) * (fd->fd_lastfile + 1));
	if (KMCPY(o, fd->fd_ofiles, (fd->fd_lastfile + 1) * sizeof(*o)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - u_ofile - failed\n",
			(u_long)fd->fd_ofiles, (u_long)o, (u_long)sizeof(*o));
		return NULL;
	    }
	f = (struct file *)calloc(1, sizeof(*f));
	if (KMCPY(f, o[tfd], sizeof(*f)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - o[tfd] - failed\n",
			(u_long)o[tfd], (u_long)f, (u_long)sizeof(*f));
		return NULL;
	    }

	s = (struct socket *)calloc(1, sizeof(*s));
	if (KMCPY(s, f->f_data, sizeof(*s)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - f_data - failed\n",
			(u_long)f->f_data, (u_long)s, (u_long)sizeof(*s));
		return NULL;
	    }

	i = (struct inpcb *)calloc(1, sizeof(*i));
	if (KMCPY(i, s->so_pcb, sizeof(*i)) == -1)
	    {
		fprintf(stderr, "kvm_read(%#lx,%#lx,%lu) - so_pcb - failed\n",
			(u_long)s->so_pcb, (u_long)i, (u_long)sizeof(*i));
		return NULL;
	    }

	t = (struct tcpcb *)calloc(1, sizeof(*t));
	if (KMCPY(t, i->inp_ppcb, sizeof(*t)) == -1)
	    {
		fprintf(stderr, "read(%#lx,%#lx,%lu) - inp_ppcb - failed\n",
			(u_long)i->inp_ppcb, (u_long)t, (u_long)sizeof(*t));
		return NULL;
	    }
	return (struct tcpcb *)i->inp_ppcb;
}
#endif /* BSD < 199301 */

int	do_socket(dev, mtu, ti, gwip)
char	*dev;
int	mtu;
struct	tcpiphdr *ti;
struct	in_addr	gwip;
{
	struct	sockaddr_in	rsin, lsin;
	struct	tcpcb	*t, tcb;
	int	fd, nfd, len;

	printf("Dest. Port: %d\n", ti->ti_dport);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	    {
		perror("socket");
		return -1;
	    }

	if (fcntl(fd, F_SETFL, FNDELAY) == -1)
	    {
		perror("fcntl");
		return -1;
	    }

	bzero((char *)&lsin, sizeof(lsin));
	lsin.sin_family = AF_INET;
	bcopy((char *)&ti->ti_src, (char *)&lsin.sin_addr,
	      sizeof(struct in_addr));
	if (bind(fd, (struct sockaddr *)&lsin, sizeof(lsin)) == -1)
	    {
		perror("bind");
		return -1;
	    }
	len = sizeof(lsin);
	(void) getsockname(fd, (struct sockaddr *)&lsin, &len);
	ti->ti_sport = lsin.sin_port;
	printf("sport %d\n", ntohs(lsin.sin_port));
	nfd = initdevice(dev, ntohs(lsin.sin_port), 1);

	if (!(t = find_tcp(fd, ti)))
		return -1;

	bzero((char *)&rsin, sizeof(rsin));
	rsin.sin_family = AF_INET;
	bcopy((char *)&ti->ti_dst, (char *)&rsin.sin_addr,
	      sizeof(struct in_addr));
	rsin.sin_port = ti->ti_dport;
	if (connect(fd, (struct sockaddr *)&rsin, sizeof(rsin)) == -1 &&
	    errno != EINPROGRESS)
	    {
		perror("connect");
		return -1;
	    }
	KMCPY(&tcb, t, sizeof(tcb));
	ti->ti_win = tcb.rcv_adv;
	ti->ti_seq = tcb.snd_nxt - 1;
	ti->ti_ack = tcb.rcv_nxt;

	if (send_tcp(nfd, mtu, (ip_t *)ti, gwip) == -1)
		return -1;
	(void)write(fd, "Hello World\n", 12);
	sleep(2);
	close(fd);
	return 0;
}
