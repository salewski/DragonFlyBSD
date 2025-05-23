/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1997 Todd Vierling
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Stolen from NetBSD /sys/compat/svr4/svr4_net.c.  Pseudo-device driver
 * skeleton produced from /usr/share/examples/drivers/make_pseudo_driver.sh
 * in 3.0-980524-SNAP then hacked a bit (but probably not enough :-).
 *
 * $FreeBSD: src/sys/dev/streams/streams.c,v 1.16.2.1 2001/02/26 04:23:07 jlemon Exp $
 * $DragonFly: src/sys/dev/misc/streams/Attic/streams.c,v 1.14 2004/11/12 00:09:07 dillon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>		/* SYSINIT stuff */
#include <sys/conf.h>		/* cdevsw stuff */
#include <sys/malloc.h>		/* malloc region definitions */
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/domain.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <sys/sysproto.h>

#include <emulation/svr4/svr4_types.h>
#include <emulation/svr4/svr4_util.h>
#include <emulation/svr4/svr4_signal.h>
#include <emulation/svr4/svr4_ioctl.h>
#include <emulation/svr4/svr4_stropts.h>
#include <emulation/svr4/svr4_socket.h>

static int svr4_soo_close (struct file *, struct thread *);
static int svr4_ptm_alloc (struct thread *);
static  d_open_t	streamsopen;

struct svr4_sockcache_entry {
	struct thread *td;	/* Thread for the socket		*/
	void *cookie;		/* Internal cookie used for matching	*/
	struct sockaddr_un sock;/* Pathname for the socket		*/
	dev_t dev;		/* Device where the socket lives on	*/
	ino_t ino;		/* Inode where the socket lives on	*/
	TAILQ_ENTRY(svr4_sockcache_entry) entries;
};

TAILQ_HEAD(svr4_sockcache_head, svr4_sockcache_entry) svr4_head;

/* Initialization flag (set/queried by svr4_mod LKM) */
int svr4_str_initialized = 0;

/*
 * Device minor numbers
 */
enum {
	dev_ptm			= 10,
	dev_arp			= 26,
	dev_icmp		= 27,
	dev_ip			= 28,
	dev_tcp			= 35,
	dev_udp			= 36,
	dev_rawip		= 37,
	dev_unix_dgram		= 38,
	dev_unix_stream		= 39,
	dev_unix_ord_stream	= 40
};

static struct fileops svr4_netops = {
	NULL,	/* port */
	NULL,	/* clone */
	soo_read, soo_write, soo_ioctl, soo_poll, sokqfilter,
	soo_stat, svr4_soo_close
};
 
#define CDEV_MAJOR 103
static struct cdevsw streams_cdevsw = {
	/* name */	"streams",
	/* maj */	CDEV_MAJOR,
	/* flags */	0,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */	streamsopen,
	/* close */	noclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};
 
struct streams_softc {
	struct isa_device *dev;
} ;

#define UNIT(dev) minor(dev)	/* assume one minor number per unit */

typedef	struct streams_softc *sc_p;

static	int
streams_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		/* XXX should make sure it isn't already loaded first */
		cdevsw_add(&streams_cdevsw, 0, 0);
		make_dev(&streams_cdevsw, dev_ptm, 0, 0, 0666, "ptm");
		make_dev(&streams_cdevsw, dev_arp, 0, 0, 0666, "arp");
		make_dev(&streams_cdevsw, dev_icmp, 0, 0, 0666, "icmp");
		make_dev(&streams_cdevsw, dev_ip, 0, 0, 0666, "ip");
		make_dev(&streams_cdevsw, dev_tcp, 0, 0, 0666, "tcp");
		make_dev(&streams_cdevsw, dev_udp, 0, 0, 0666, "udp");
		make_dev(&streams_cdevsw, dev_rawip, 0, 0, 0666, "rawip");
		make_dev(&streams_cdevsw, dev_unix_dgram, 0, 0, 0666, "ticlts");
		make_dev(&streams_cdevsw, dev_unix_stream, 
			0, 0, 0666, "ticots");
		make_dev(&streams_cdevsw, dev_unix_ord_stream, 
			0, 0, 0666, "ticotsord");
		return 0;
	case MOD_UNLOAD:
	  	/* XXX should check to see if it's busy first */
		cdevsw_remove(&streams_cdevsw, 0, 0);

		return 0;
	default:
		break;
	}
	return 0;
}

static moduledata_t streams_mod = {
	"streams",
	streams_modevent,
	0
};
DECLARE_MODULE(streams, streams_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);

/*
 * We only need open() and close() routines.  open() calls socreate()
 * to allocate a "real" object behind the stream and mallocs some state
 * info for use by the svr4 emulator;  close() deallocates the state
 * information and passes the underlying object to the normal socket close
 * routine.
 */
static  int
streamsopen(dev_t dev, int oflags, int devtype, d_thread_t *td)
{
	struct proc *p = td->td_proc;
	int type, protocol;
	int fd;
	struct file *fp;
	struct socket *so;
	int error;
	int family;

	KKASSERT(p != NULL);
	
	if (p->p_dupfd >= 0)
	  return ENODEV;

	switch (minor(dev)) {
	case dev_udp:
	  family = AF_INET;
	  type = SOCK_DGRAM;
	  protocol = IPPROTO_UDP;
	  break;

	case dev_tcp:
	  family = AF_INET;
	  type = SOCK_STREAM;
	  protocol = IPPROTO_TCP;
	  break;

	case dev_ip:
	case dev_rawip:
	  family = AF_INET;
	  type = SOCK_RAW;
	  protocol = IPPROTO_IP;
	  break;

	case dev_icmp:
	  family = AF_INET;
	  type = SOCK_RAW;
	  protocol = IPPROTO_ICMP;
	  break;

	case dev_unix_dgram:
	  family = AF_LOCAL;
	  type = SOCK_DGRAM;
	  protocol = 0;
	  break;

	case dev_unix_stream:
	case dev_unix_ord_stream:
	  family = AF_LOCAL;
	  type = SOCK_STREAM;
	  protocol = 0;
	  break;

	case dev_ptm:
	  return svr4_ptm_alloc(td);

	default:
	  return EOPNOTSUPP;
	}

	if ((error = falloc(p, &fp, &fd)) != 0)
	  return error;

	if ((error = socreate(family, &so, type, protocol, td)) != 0) {
	  if (p->p_fd->fd_ofiles[fd] == fp) {
	      p->p_fd->fd_ofiles[fd] = NULL;
	      fdrop(fp, td);
	  }
	  fdrop(fp, td);
	  return error;
	}

	fp->f_data = (caddr_t)so;
	fp->f_flag = FREAD|FWRITE;
	fp->f_ops = &svr4_netops;
	fp->f_type = DTYPE_SOCKET;
	(void)svr4_stream_get(fp);
	fdrop(fp, td);
	p->p_dupfd = fd;
	return ENXIO;
}

static int
svr4_ptm_alloc(struct thread *td)
{
	/*
	 * XXX this is very, very ugly.  But I can't find a better
	 * way that won't duplicate a big amount of code from
	 * sys_open().  Ho hum...
	 *
	 * Fortunately for us, Solaris (at least 2.5.1) makes the
	 * /dev/ptmx open automatically just open a pty, that (after
	 * STREAMS I_PUSHes), is just a plain pty.  fstat() is used
	 * to get the minor device number to map to a tty.
	 * 
	 * Cycle through the names. If sys_open() returns ENOENT (or
	 * ENXIO), short circuit the cycle and exit.
	 */
	static char ptyname[] = "/dev/ptyXX";
	static char ttyletters[] = "pqrstuwxyzPQRST";
	static char ttynumbers[] = "0123456789abcdef";
	caddr_t sg = stackgap_init();
	char *path = stackgap_alloc(&sg, sizeof(ptyname));
	struct open_args oa;
	int l = 0, n = 0;
	register_t fd = -1;
	int error;
	struct proc *p = td->td_proc;

	KKASSERT(p);

	SCARG(&oa, path) = path;
	SCARG(&oa, flags) = O_RDWR;
	SCARG(&oa, mode) = 0;

	while (fd == -1) {
		ptyname[8] = ttyletters[l];
		ptyname[9] = ttynumbers[n];

		if ((error = copyout(ptyname, path, sizeof(ptyname))) != 0)
			return error;

		switch (error = open(&oa)) {
		case ENOENT:
		case ENXIO:
			return error;
		case 0:
			p->p_dupfd = oa.sysmsg_result;
			return ENXIO;
		default:
			if (ttynumbers[++n] == '\0') {
				if (ttyletters[++l] == '\0')
					break;
				n = 0;
			}
		}
	}
	return ENOENT;
}


struct svr4_strm *
svr4_stream_get(fp)
	struct file *fp;
{
	struct socket *so;
	struct svr4_strm *st;

	if (fp == NULL || fp->f_type != DTYPE_SOCKET)
		return NULL;

	so = (struct socket *) fp->f_data;

       	if (so->so_emuldata)
		return so->so_emuldata;

	/* Allocate a new one. */
	st = malloc(sizeof(struct svr4_strm), M_TEMP, M_WAITOK);
	st->s_family = so->so_proto->pr_domain->dom_family;
	st->s_cmd = ~0;
	st->s_afd = -1;
	st->s_eventmask = 0;
	so->so_emuldata = st;
	fp->f_ops = &svr4_netops;

	return st;
}

void
svr4_delete_socket(struct thread *td, struct file *fp)
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *) fp->f_data)->so_emuldata;

	if (!svr4_str_initialized) {
		TAILQ_INIT(&svr4_head);
		svr4_str_initialized = 1;
		return;
	}

	TAILQ_FOREACH(e, &svr4_head, entries) {
		if (e->td == td && e->cookie == cookie) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			DPRINTF(("svr4_delete_socket: %s [%p,%d,%d]\n",
				 e->sock.sun_path, td, (int)e->dev,
				 (int)e->ino));
			free(e, M_TEMP);
			return;
		}
	}
}

static int
svr4_soo_close(struct file *fp, struct thread *td)
{
        struct socket *so = (struct socket *)fp->f_data;
	
	/*	CHECKUNIT_DIAG(ENXIO);*/

	svr4_delete_socket(td, fp);
	free(so->so_emuldata, M_TEMP);
	return soo_close(fp, td);
	return (0);
}
