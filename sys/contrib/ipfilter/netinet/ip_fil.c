/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)ip_fil.c     2.41 6/5/96 (C) 1993-2000 Darren Reed
 * @(#)$Id: ip_fil.c,v 2.42.2.60 2002/08/28 12:40:39 darrenr Exp $
 * $FreeBSD: src/sys/contrib/ipfilter/netinet/ip_fil.c,v 1.25.2.7 2004/07/04  09:24:38 darrenr Exp $
 * $DragonFly: src/sys/contrib/ipfilter/netinet/ip_fil.c,v 1.16 2005/02/01 19:39:07 hrs Exp $
 */
#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#if defined(KERNEL) && !defined(_KERNEL)
# define	_KERNEL
#endif
#if defined(_KERNEL) && (defined(__DragonFly__) || (defined(__FreeBSD_version) && \
    (__FreeBSD_version >= 400000))) && !defined(KLD_MODULE)
#include "opt_inet6.h"
#endif
#include <sys/param.h>
#if defined(__NetBSD__) && (NetBSD >= 199905) && !defined(IPFILTER_LKM) && \
    defined(_KERNEL)  && !defined(_LKM)
# include "opt_ipfilter_log.h"
#endif
#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
# if !defined(_KERNEL) || defined(IPFILTER_LKM)
#  include <osreldate.h>
# endif
#endif
#if defined(__sgi) && (IRIX > 602)
# define _KMEMUSER
# include <sys/ptimers.h>
#endif
#ifndef	_KERNEL
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <ctype.h>
# include <fcntl.h>
#endif
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/file.h>
#if (defined(__DragonFly__) || __FreeBSD_version >= 220000) && defined(_KERNEL)
# include <sys/fcntl.h>
# include <sys/filio.h>
#else
# include <sys/ioctl.h>
#endif
#include <sys/time.h>
#ifdef	_KERNEL
# include <sys/systm.h>
#endif
#if !SOLARIS
# if defined(__DragonFly__) || (NetBSD > 199609) || (OpenBSD > 199603) || (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
# include <sys/mbuf.h>
#else
# include <sys/filio.h>
#endif
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#if defined(__DragonFly__) || __FreeBSD_version >= 300000
# include <net/if_var.h>
# if defined(_KERNEL) && !defined(IPFILTER_LKM)
#  include "opt_ipfilter.h"
# endif
#endif
#ifdef __sgi
#include <sys/debug.h>
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
# endif
#endif
#include <net/route.h>
#include <netinet/in.h>
#if !(defined(__sgi) && !defined(IFF_DRVRLOCK)) /* IRIX < 6 */
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#ifndef	_KERNEL
# include <unistd.h>
# include <syslog.h>
#endif
#include "ip_compat.h"
#ifdef USE_INET6
# include <netinet/icmp6.h>
# if !SOLARIS
#  include <netinet6/ip6protosw.h>
#  include <netinet6/nd6.h>
# endif
#endif
#include "ip_fil.h"
#include "ip_nat.h"
#include "ip_frag.h"
#include "ip_state.h"
#include "ip_proxy.h"
#include "ip_auth.h"
#if defined(__DragonFly__) || (defined(__FreeBSD_version) && (__FreeBSD_version >= 300000))
# include <sys/malloc.h>
#endif
#ifndef	MIN
# define	MIN(a,b)	(((a)<(b))?(a):(b))
#endif
#if !SOLARIS && defined(_KERNEL) && !defined(__sgi)
# include <sys/kernel.h>
extern	int	ip_optcopy (struct ip *, struct ip *);
#endif
#if defined(OpenBSD) && (OpenBSD >= 200211) && defined(_KERNEL)
extern	int	ip6_getpmtu(struct route_in6 *, struct route_in6 *,
			    struct ifnet *, struct in6_addr *, u_long *);
#endif

#include <sys/in_cksum.h>

static const char sccsid[] = "@(#)ip_fil.c     2.41 6/5/96 (C) 1993-2000 Darren Reed";

extern	struct	protosw	inetsw[];

#ifndef	_KERNEL
# include "ipt.h"
static	struct	ifnet **ifneta = NULL;
static	int	nifs = 0;
#else
# if	(BSD < 199306) || defined(__sgi)
extern	int	tcp_ttl;
# endif
#endif

#ifdef	ICMP_UNREACH_FILTER_PROHIB
int	ipl_unreach = ICMP_UNREACH_FILTER_PROHIB;
#else
int	ipl_unreach = ICMP_UNREACH_FILTER;
#endif
u_long	ipl_frouteok[2] = {0, 0};

static	int	frzerostats (caddr_t);
#if defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
static	int	frrequest (int, u_long, caddr_t, int);
#else
static	int	frrequest (int, int, caddr_t, int);
#endif
#ifdef	_KERNEL
static	int	(*fr_savep) (ip_t *, int, void *, int, struct mbuf **);
static	int	send_ip (ip_t *, fr_info_t *, struct mbuf **);
# ifdef	USE_INET6
static	int	ipfr_fastroute6 (struct mbuf *, struct mbuf **,
				     fr_info_t *, frdest_t *);
# endif
# ifdef	__sgi
extern	int		tcp_mtudisc;
extern  kmutex_t        ipf_rw;
extern	KRWLOCK_T	ipf_mutex;
# endif
#else
void	init_ifp (void);
# if defined(__sgi) && (IRIX < 605)
static int 	no_output (struct ifnet *, struct mbuf *,
			       struct sockaddr *);
static int	write_output (struct ifnet *, struct mbuf *,
				  struct sockaddr *);
# else
static int 	no_output (struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *);
static int	write_output (struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *);
# endif
#endif
int	fr_running = 0;

#if (defined(__DragonFly__) || __FreeBSD_version >= 300000) && defined(_KERNEL)
struct callout ipfr_slowtimer_ch;
#endif
#if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
# include <sys/callout.h>
struct callout ipfr_slowtimer_ch;
#endif
#if defined(__OpenBSD__)
# include <sys/timeout.h>
struct timeout ipfr_slowtimer_ch;
#endif
#if defined(__sgi) && defined(_KERNEL)
toid_t ipfr_slowtimer_ch;
#endif

#if defined(__NetBSD__) && (__NetBSD_Version__ >= 106080000) && \
    defined(_KERNEL)
# include <sys/conf.h>
const struct cdevsw ipl_cdevsw = {
	iplopen, iplclose, iplread, nowrite, iplioctl,
	nostop, notty, nopoll, nommap,
};
#endif

#if (_BSDI_VERSION >= 199510) && defined(_KERNEL)
# include <sys/device.h>
# include <sys/conf.h>

struct cfdriver iplcd = {
	NULL, "ipl", NULL, NULL, DV_DULL, 0
};

struct devsw iplsw = {
	&iplcd,
	iplopen, iplclose, iplread, nowrite, iplioctl, noselect, nommap,
	nostrat, nodump, nopsize, 0,
	nostop
};
#endif /* _BSDI_VERSION >= 199510  && _KERNEL */

#if defined(__NetBSD__) || defined(__OpenBSD__)  || \
    (_BSDI_VERSION >= 199701) || (defined(__DragonFly_version) && \
    (__DragonFly_version >= 100000))
# include <sys/conf.h>
# if defined(NETBSD_PF)
#  include <net/pfil.h>
/*
 * We provide the fr_checkp name just to minimize changes later.
 */
int (*fr_checkp) (ip_t *ip, int hlen, void *ifp, int out, mb_t **mp);
# endif /* NETBSD_PF */
#endif /* __NetBSD__ */


#if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 105110000) && \
    defined(_KERNEL)
# include <net/pfil.h>

static int fr_check_wrapper(void *, struct mbuf **, struct ifnet *, int );

static int fr_check_wrapper(arg, mp, ifp, dir)
void *arg;
struct mbuf **mp;
struct ifnet *ifp;
int dir;
{
	struct ip *ip = mtod(*mp, struct ip *);
	int rv, hlen = ip->ip_hl << 2;

#if defined(M_CSUM_TCPv4)
	/*
	 * If the packet is out-bound, we can't delay checksums
	 * here.  For in-bound, the checksum has already been
	 * validated.
	 */
	if (dir == PFIL_OUT) {
		if ((*mp)->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4)) {
			in_delayed_cksum(*mp);
			(*mp)->m_pkthdr.csum_flags &=
			    ~(M_CSUM_TCPv4|M_CSUM_UDPv4);
		}
	}
#endif /* M_CSUM_TCPv4 */

	/*
	 * We get the packet with all fields in network byte
	 * order.  We expect ip_len and ip_off to be in host
	 * order.  We frob them, call the filter, then frob
	 * them back.
	 *
	 * Note, we don't need to update the checksum, because
	 * it has already been verified.
	 */
	ip->ip_len = ntohs(ip->ip_len);
	ip->ip_off = ntohs(ip->ip_off);

	rv = fr_check(ip, hlen, ifp, (dir == PFIL_OUT), mp);

	if (rv == 0 && *mp != NULL) {
		ip = mtod(*mp, struct ip *);
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htnos(ip->ip_off);
	}

	return (rv);
}

# ifdef USE_INET6
#  include <netinet/ip6.h>

static int fr_check_wrapper6(void *, struct mbuf **, struct ifnet *, int );

static int fr_check_wrapper6(arg, mp, ifp, dir)
void *arg;
struct mbuf **mp;
struct ifnet *ifp;
int dir;
{
	
	return (fr_check(mtod(*mp, struct ip *), sizeof(struct ip6_hdr),
	    ifp, (dir == PFIL_OUT), mp));
}
# endif
#endif /* __NetBSD_Version >= 105110000 && _KERNEL */
#if defined(__DragonFly_version) && (__DragonFly_version >= 100000) && \
    defined(_KERNEL)

static int
fr_check_wrapper(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir)
{
	struct ip *ip = mtod(*mp, struct ip *);
	return fr_check(ip, ip->ip_hl << 2, ifp, (dir == PFIL_OUT), mp);
}

# ifdef USE_INET6
#  include <netinet/ip6.h>

static int
fr_check_wrapper6(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir)
{
	return (fr_check(mtod(*mp, struct ip *), sizeof(struct ip6_hdr),
	    ifp, (dir == PFIL_OUT), mp));
}
# endif /* USE_INET6 */
#endif /* __DragonFly_version >= 100000 && _KERNEL */
#ifdef	_KERNEL
# if	defined(IPFILTER_LKM) && !defined(__sgi)
int iplidentify(s)
char *s;
{
	if (strcmp(s, "ipl") == 0)
		return 1;
	return 0;
}
# endif /* IPFILTER_LKM */


/*
 * Try to detect the case when compiling for NetBSD with pseudo-device
 */
# if defined(__NetBSD__) && defined(PFIL_HOOKS)
void
ipfilterattach(count)
int count;
{

	/*
	 * Do nothing here, really.  The filter will be enabled
	 * by the SIOCFRENB ioctl.
	 */
}
# endif


# if defined(__NetBSD__) || defined(__OpenBSD__)
int ipl_enable()
# else
int iplattach()
# endif
{
	char *defpass;
	int s;
# if defined(__sgi) || (defined(NETBSD_PF) && \
     (__NetBSD_Version__ >= 104200000)) || \
     (defined(__DragonFly_version) && (__DragonFly_version >= 100000))
	int error = 0;
# endif
#if (defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 105110000)) || \
    (defined(__DragonFly_version) && (__DragonFly_version >= 100000))
	struct pfil_head *ph_inet;
# ifdef USE_INET6
	struct pfil_head *ph_inet6;
# endif
#endif

	SPL_NET(s);
	if (fr_running || (fr_checkp == fr_check)) {
		printf("IP Filter: already initialized\n");
		SPL_X(s);
		return EBUSY;
	}

# ifdef	IPFILTER_LOG
	ipflog_init();
# endif
	if (nat_init() == -1) {
		SPL_X(s);
		return EIO;
	}
	if (fr_stateinit() == -1) {
		SPL_X(s);
		return EIO;
	}
	if (appr_init() == -1) {
		SPL_X(s);
		return EIO;
	}

# ifdef NETBSD_PF
#  if (__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011) || \
      (defined(__DragonFly_version) && (__DragonFly_version >= 100000))
#   if (__NetBSD_Version__ >= 105110000) || (__DragonFly_version >= 100000)
	ph_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
#    ifdef USE_INET6
	ph_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
#    endif
	if (ph_inet == NULL
#    ifdef USE_INET6
	    && ph_inet6 == NULL
#    endif
	   )
		return ENODEV;

	if (ph_inet != NULL)
		error = pfil_add_hook((void *)fr_check_wrapper, NULL,
				      PFIL_IN|PFIL_OUT, ph_inet);
	else
		error = 0;
#  else
	error = pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
			      &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
#  endif
	if (error) {
#   ifdef USE_INET6
		goto pfil_error;
#   else
		SPL_X(s);
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
#   endif
	}
#  else
	pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
#  endif
#  ifdef USE_INET6
#   if (__NetBSD_Version__ >= 105110000) || (__DragonFly_version >= 100000)
	if (ph_inet6 != NULL)
		error = pfil_add_hook((void *)fr_check_wrapper6, NULL,
				      PFIL_IN|PFIL_OUT, ph_inet6);
	else
		error = 0;
	if (error) {
		pfil_remove_hook((void *)fr_check_wrapper6, NULL,
				 PFIL_IN|PFIL_OUT, ph_inet6);
#   else
	error = pfil_add_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
			      &inet6sw[ip6_protox[IPPROTO_IPV6]].pr_pfh);
	if (error) {
		pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
#   endif
pfil_error:
		SPL_X(s);
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
	}
#  endif
# endif

# ifdef __sgi
	error = ipfilter_sgi_attach();
	if (error) {
		SPL_X(s);
		appr_unload();
		ip_natunload();
		fr_stateunload();
		return error;
	}
# endif

	bzero((char *)frcache, sizeof(frcache));
	fr_savep = fr_checkp;
	fr_checkp = fr_check;
	fr_running = 1;

	SPL_X(s);
	if (fr_pass & FR_PASS)
		defpass = "pass";
	else if (fr_pass & FR_BLOCK)
		defpass = "block";
	else
		defpass = "no-match -> block";

	printf("%s initialized.  Default = %s all, Logging = %s\n",
		ipfilter_version, defpass,
# ifdef	IPFILTER_LOG
		"enabled");
# else
		"disabled");
# endif
#ifdef  _KERNEL
# if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
	callout_init(&ipfr_slowtimer_ch);
	callout_reset(&ipfr_slowtimer_ch, hz / 2, ipfr_slowtimer, NULL);
# else
#  if defined(__OpenBSD__)
	timeout_set(&ipfr_slowtimer_ch, ipfr_slowtimer, NULL);
	timeout_add(&ipfr_slowtimer_ch, hz/2);
#  else
#   if (defined(__DragonFly__) || __FreeBSD_version >= 300000) || defined(__sgi)
	callout_init(&ipfr_slowtimer_ch);
	callout_reset(&ipfr_slowtimer_ch, hz / 2, ipfr_slowtimer, NULL);
#   else
	timeout(ipfr_slowtimer, NULL, hz/2);
#   endif
#  endif
# endif
#endif
	return 0;
}


/*
 * Disable the filter by removing the hooks from the IP input/output
 * stream.
 */
# if defined(__NetBSD__)
int ipl_disable()
# else
int ipldetach()
# endif
{
	int s, i;
#if defined(NETBSD_PF) && \
    ((__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011) || \
     (defined(__DragonFly_version) && (__DragonFly_version >= 100000)))
	int error = 0;
# if (__NetBSD_Version__ >= 105150000) || (__DragonFly_version >= 100000)
        struct pfil_head *ph_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
#  ifdef USE_INET6
	struct pfil_head *ph_inet6 = pfil_head_get(PFIL_TYPE_AF, AF_INET6);
#  endif
# endif
#endif

#ifdef  _KERNEL
# if defined(__NetBSD__) && (__NetBSD_Version__ >= 104230000)
	callout_stop(&ipfr_slowtimer_ch);
# else
#  if (defined(__DragonFly__) || __FreeBSD_version >= 300000)
	callout_stop(&ipfr_slowtimer_ch);
#  else
#  ifdef __sgi
	untimeout(ipfr_slowtimer_ch);
#   else
#    if defined(__OpenBSD__)
	timeout_del(&ipfr_slowtimer_ch);
#    else
	untimeout(ipfr_slowtimer, NULL);
#    endif /* OpenBSD */
#   endif /* __sgi */
#  endif /* FreeBSD */
# endif /* NetBSD */
#endif
	SPL_NET(s);
	if (!fr_running)
	{
		printf("IP Filter: not initialized\n");
		SPL_X(s);
		return 0;
	}

	printf("%s unloaded\n", ipfilter_version);

	fr_checkp = fr_savep;
	i = frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	i += frflush(IPL_LOGIPF, 0, FR_INQUE|FR_OUTQUE);
	fr_running = 0;

# ifdef NETBSD_PF
#  if ((__NetBSD_Version__ >= 104200000) || (__FreeBSD_version >= 500011) || \
       (__DragonFly_version >= 100000))
#   if (__NetBSD_Version__ >= 105110000) || (__DragonFly_version >= 100000)
	if (ph_inet != NULL)
		error = pfil_remove_hook((void *)fr_check_wrapper, NULL,
					 PFIL_IN|PFIL_OUT, ph_inet);
	else
		error = 0;
#   else
	error = pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inetsw[ip_protox[IPPROTO_IP]].pr_pfh);
#   endif
	if (error) {
		SPL_X(s);
		return error;
	}
#  else
	pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT);
#  endif
#  ifdef USE_INET6
#   if (__NetBSD_Version__ >= 105110000) || (__DragonFly_version >= 100000)
	if (ph_inet6 != NULL)
		error = pfil_remove_hook((void *)fr_check_wrapper6, NULL,
					 PFIL_IN|PFIL_OUT, ph_inet6);
	else
		error = 0;
#   else
	error = pfil_remove_hook((void *)fr_check, PFIL_IN|PFIL_OUT,
				 &inet6sw[ip6_protox[IPPROTO_IPV6]].pr_pfh);
#   endif
	if (error) {
		SPL_X(s);
		return error;
	}
#  endif
# endif

# ifdef __sgi
	ipfilter_sgi_detach();
# endif

	appr_unload();
	ipfr_unload();
	ip_natunload();
	fr_stateunload();
	fr_authunload();

	SPL_X(s);
	return 0;
}
#endif /* _KERNEL */


static	int	frzerostats(data)
caddr_t	data;
{
	friostat_t fio;
	int error;

	fr_getstat(&fio);
	error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
	if (error)
		return EFAULT;

	bzero((char *)frstats, sizeof(*frstats) * 2);

	return 0;
}


/*
 * Filter ioctl interface.
 */
#ifdef __sgi
int IPL_EXTERN(ioctl)(dev_t dev, int cmd, caddr_t data, int mode
# ifdef _KERNEL
	, cred_t *cp, int *rp
# endif
)
#else
int IPL_EXTERN(ioctl)(dev, cmd, data, mode
#if (defined(_KERNEL) && (defined(__DragonFly__) || defined(__FreeBSD__)))
, td)
struct thread *td;
# elif (defined(_KERNEL) && ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || \
       (NetBSD >= 199511) || defined(__DragonFly__) || (__FreeBSD_version >= 220000) || \
       defined(__OpenBSD__)))
, p)
struct proc *p;
# else
)
# endif
dev_t dev;
# if defined(__NetBSD__) || defined(__OpenBSD__) || \
	(_BSDI_VERSION >= 199701) || (defined(__DragonFly__) || __FreeBSD_version >= 300000)
u_long cmd;
# else
int cmd;
# endif
caddr_t data;
int mode;
#endif /* __sgi */
{
#if defined(_KERNEL) && !SOLARIS
	int s;
#endif
	int error = 0, unit = 0, tmp;

#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel >= 3) && (mode & FWRITE))
		return EPERM;
#endif
#ifdef	_KERNEL
	unit = GET_MINOR(dev);
	if ((IPL_LOGMAX < unit) || (unit < 0))
		return ENXIO;
#else
	unit = dev;
#endif

	if (fr_running == 0 && (cmd != SIOCFRENB || unit != IPL_LOGIPF))
		return ENODEV;

	SPL_NET(s);

	if (unit == IPL_LOGNAT) {
		if (fr_running)
			error = nat_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGSTATE) {
		if (fr_running)
			error = fr_state_ioctl(data, cmd, mode);
		else
			error = EIO;
		SPL_X(s);
		return error;
	}
	if (unit == IPL_LOGAUTH) {
		if (!fr_running)
			error = EIO;
		else
			if ((cmd == SIOCADAFR) || (cmd == SIOCRMAFR)) {
				if (!(mode & FWRITE))  {
					error = EPERM;
				} else {
					error = frrequest(unit, cmd, data,
							  fr_active);
				}
			} else {
				error = fr_auth_ioctl(data, mode, cmd);
			}
		SPL_X(s);
		return error;
	}

	switch (cmd) {
	case FIONREAD :
#ifdef IPFILTER_LOG
		error = IWCOPY((caddr_t)&iplused[IPL_LOGIPF], (caddr_t)data,
			       sizeof(iplused[IPL_LOGIPF]));
#endif
		break;
#if (!defined(IPFILTER_LKM) || defined(__NetBSD__)) && defined(_KERNEL)
	case SIOCFRENB :
	{
		u_int	enable;

		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&enable, sizeof(enable));
			if (error)
				break;
			if (enable)
# if defined(__NetBSD__) || defined(__OpenBSD__)
				error = ipl_enable();
# else
				error = iplattach();
# endif
			else
# if defined(__NetBSD__)
				error = ipl_disable();
# else
				error = ipldetach();
# endif
		}
		break;
	}
#endif
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = IRCOPY(data, (caddr_t)&fr_flags,
				       sizeof(fr_flags));
		break;
	case SIOCGETFF :
		error = IWCOPY((caddr_t)&fr_flags, data, sizeof(fr_flags));
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, fr_active);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, data, 1 - fr_active);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			*(u_int *)data = fr_active;
			fr_active = 1 - fr_active;
		}
		break;
	case SIOCGETFS :
	{
		friostat_t	fio;

		fr_getstat(&fio);
		error = IWCOPYPTR((caddr_t)&fio, data, sizeof(fio));
		if (error)
			error = EFAULT;
		break;
	}
	case	SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frzerostats(data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, 4, tmp);
				error = IWCOPY((caddr_t)&tmp, data,
					       sizeof(tmp));
			}
		}
		break;
#ifdef	USE_INET6
	case	SIOCIPFL6 :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, 6, tmp);
				error = IWCOPY((caddr_t)&tmp, data,
					       sizeof(tmp));
			}
		}
		break;
#endif
	case SIOCSTLCK :
		error = IRCOPY(data, (caddr_t)&tmp, sizeof(tmp));
		if (!error) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
		break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			*(int *)data = ipflog_clear(unit);
		break;
#endif /* IPFILTER_LOG */
	case SIOCGFRST :
		error = IWCOPYPTR((caddr_t)ipfr_fragstats(), data,
				  sizeof(ipfrstat_t));
		if (error)
			error = EFAULT;
		break;
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
#if defined(_KERNEL) && defined(__sgi)
			ipfsync();
#endif
			frsync();
		}
		break;
	default :
		error = EINVAL;
		break;
	}
	SPL_X(s);
	return error;
}


void fr_forgetifp(ifp)
void *ifp;
{
	frentry_t *f;

	WRITE_ENTER(&ipf_mutex);
	for (f = ipacct[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#ifdef	USE_INET6
	for (f = ipacct6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipacct6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[0][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = ipfilter6[1][fr_active]; (f != NULL); f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
#endif
	RWLOCK_EXIT(&ipf_mutex);
	ip_natsync(ifp);
}


static int frrequest(unit, req, data, set)
int unit;
#if defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__) || (__FreeBSD_version >= 300003)
u_long req;
#else
int req;
#endif
int set;
caddr_t data;
{
	frentry_t *fp, *f, **fprev;
	frentry_t **ftail;
	frgroup_t *fg = NULL;
	int error = 0, in, i;
	u_int   *p, *pp;
	frentry_t frd;
	frdest_t *fdp;
	u_int group;

	fp = &frd;
	error = IRCOPYPTR(data, (caddr_t)fp, sizeof(*fp));
	if (error)
		return EFAULT;
	fp->fr_ref = 0;
#if (BSD >= 199306) && defined(_KERNEL)
	if ((securelevel > 0) && (fp->fr_func != NULL))
		return EPERM;
#endif

	/*
	 * Check that the group number does exist and that if a head group
	 * has been specified, doesn't exist.
	 */
	if ((req != SIOCZRLST) && ((req == SIOCINAFR) || (req == SIOCINIFR) ||
	     (req == SIOCADAFR) || (req == SIOCADIFR)) && fp->fr_grhead &&
	    fr_findgroup((u_int)fp->fr_grhead, fp->fr_flags, unit, set, NULL))
		return EEXIST;
	if ((req != SIOCZRLST) && fp->fr_group &&
	    !fr_findgroup((u_int)fp->fr_group, fp->fr_flags, unit, set, NULL))
		return ESRCH;

	in = (fp->fr_flags & FR_INQUE) ? 0 : 1;

	if (unit == IPL_LOGAUTH)
		ftail = fprev = &ipauth;
	else if ((fp->fr_flags & FR_ACCOUNT) && (fp->fr_v == 4))
		ftail = fprev = &ipacct[in][set];
	else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) && (fp->fr_v == 4))
		ftail = fprev = &ipfilter[in][set];
#ifdef	USE_INET6
	else if ((fp->fr_flags & FR_ACCOUNT) && (fp->fr_v == 6))
		ftail = fprev = &ipacct6[in][set];
	else if ((fp->fr_flags & (FR_OUTQUE|FR_INQUE)) && (fp->fr_v == 6))
		ftail = fprev = &ipfilter6[in][set];
#endif
	else
		return ESRCH;

	if ((group = fp->fr_group)) {
		if (!(fg = fr_findgroup(group, fp->fr_flags, unit, set, NULL)))
			return ESRCH;
		ftail = fprev = fg->fg_start;
	}

	bzero((char *)frcache, sizeof(frcache[0]) * 2);

	for (i = 0; i < 4; i++) {
		if ((fp->fr_ifnames[i][1] == '\0') &&
		    ((fp->fr_ifnames[i][0] == '-') ||
		     (fp->fr_ifnames[i][0] == '*'))) {
			fp->fr_ifas[i] = NULL;
		} else if (*fp->fr_ifnames[i]) {
			fp->fr_ifas[i] = GETUNIT(fp->fr_ifnames[i], fp->fr_v);
			if (!fp->fr_ifas[i])
				fp->fr_ifas[i] = (void *)-1;
		}
	}

	fdp = &fp->fr_dif;
	fp->fr_flags &= ~FR_DUP;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
		else
			fp->fr_flags |= FR_DUP;
	}

	fdp = &fp->fr_tif;
	if (*fdp->fd_ifname) {
		fdp->fd_ifp = GETUNIT(fdp->fd_ifname, fp->fr_v);
		if (!fdp->fd_ifp)
			fdp->fd_ifp = (struct ifnet *)-1;
	}

	/*
	 * Look for a matching filter rule, but don't include the next or
	 * interface pointer in the comparison (fr_next, fr_ifa).
	 */
	for (fp->fr_cksum = 0, p = (u_int *)&fp->fr_ip, pp = &fp->fr_cksum;
	     p < pp; p++)
		fp->fr_cksum += *p;

	for (; (f = *ftail); ftail = &f->fr_next)
		if ((fp->fr_cksum == f->fr_cksum) &&
		    !bcmp((char *)&f->fr_ip, (char *)&fp->fr_ip, FR_CMPSIZ))
			break;

	/*
	 * If zero'ing statistics, copy current to caller and zero.
	 */
	if (req == SIOCZRLST) {
		if (!f)
			return ESRCH;
		error = IWCOPYPTR((caddr_t)f, data, sizeof(*f));
		if (error)
			return EFAULT;
		f->fr_hits = 0;
		f->fr_bytes = 0;
		return 0;
	}

	if (!f) {
		if (req != SIOCINAFR && req != SIOCINIFR)
			while ((f = *ftail))
				ftail = &f->fr_next;
		else {
			ftail = fprev;
			if (fp->fr_hits) {
				while (--fp->fr_hits && (f = *ftail))
					ftail = &f->fr_next;
			}
			f = NULL;
		}
	}

	if (req == SIOCRMAFR || req == SIOCRMIFR) {
		if (!f)
			error = ESRCH;
		else {
			/*
			 * Only return EBUSY if there is a group list, else
			 * it's probably just state information referencing
			 * the rule.
			 */
			if ((f->fr_ref > 1) && f->fr_grp)
				return EBUSY;
			if (fg && fg->fg_head)
				fg->fg_head->fr_ref--;
			if (unit == IPL_LOGAUTH) {
				return fr_preauthcmd(req, f, ftail);
			}
			if (f->fr_grhead)
				fr_delgroup((u_int)f->fr_grhead, fp->fr_flags,
					    unit, set);
			fixskip(fprev, f, -1);
			*ftail = f->fr_next;
			f->fr_next = NULL;
			f->fr_ref--;
			if (f->fr_ref == 0)
				KFREE(f);
		}
	} else {
		if (f)
			error = EEXIST;
		else {
			if (unit == IPL_LOGAUTH) {
				return fr_preauthcmd(req, fp, ftail);
			}
			KMALLOC(f, frentry_t *);
			if (f != NULL) {
				if (fg && fg->fg_head)
					fg->fg_head->fr_ref++;
				bcopy((char *)fp, (char *)f, sizeof(*f));
				f->fr_ref = 1;
				f->fr_hits = 0;
				f->fr_next = *ftail;
				*ftail = f;
				if (req == SIOCINIFR || req == SIOCINAFR)
					fixskip(fprev, f, 1);
				f->fr_grp = NULL;
				if ((group = f->fr_grhead))
					fg = fr_addgroup(group, f, unit, set);
			} else
				error = ENOMEM;
		}
	}
	return (error);
}


#ifdef	_KERNEL
/*
 * routines below for saving IP headers to buffer
 */
# ifdef __sgi
#  ifdef _KERNEL
int IPL_EXTERN(open)(dev_t *pdev, int flags, int devtype, cred_t *cp)
#  else
int IPL_EXTERN(open)(dev_t dev, int flags)
#  endif
# else
int IPL_EXTERN(open)(dev, flags
#if defined(__DragonFly__) || defined(__FreeBSD__)
, devtype, td)
int devtype;
struct thread *td;
#elif ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || (NetBSD >= 199511) || \
     (defined(__DragonFly__) || __FreeBSD_version >= 220000) || defined(__OpenBSD__)) && defined(_KERNEL)
, devtype, p)
int devtype;
struct proc *p;
#  else
)
#  endif
dev_t dev;
int flags;
# endif /* __sgi */
{
# if defined(__sgi) && defined(_KERNEL)
	u_int min = geteminor(*pdev);
# else
	u_int min = GET_MINOR(dev);
# endif

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}


# ifdef __sgi
int IPL_EXTERN(close)(dev_t dev, int flags, int devtype, cred_t *cp)
#else
int IPL_EXTERN(close)(dev, flags
#if defined(__DragonFly__) || defined(__FreeBSD__)
, devtype, td)
int devtype;
struct thread *td;
#elif ((_BSDI_VERSION >= 199510) || (BSD >= 199506) || (NetBSD >= 199511) || \
     (defined(__DragonFly__) || __FreeBSD_version >= 220000) || defined(__OpenBSD__)) && defined(_KERNEL)
, devtype, p)
int devtype;
struct proc *p;
#  else
)
#  endif
dev_t dev;
int flags;
# endif /* __sgi */
{
	u_int	min = GET_MINOR(dev);

	if (IPL_LOGMAX < min)
		min = ENXIO;
	else
		min = 0;
	return min;
}

/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
# ifdef __sgi
int IPL_EXTERN(read)(dev_t dev, uio_t *uio, cred_t *crp)
# else
#  if BSD >= 199306
int IPL_EXTERN(read)(dev, uio, ioflag)
int ioflag;
#  else
int IPL_EXTERN(read)(dev, uio)
#  endif
dev_t dev;
struct uio *uio;
# endif /* __sgi */
{
# ifdef IPFILTER_LOG
	return ipflog_read(GET_MINOR(dev), uio);
# else
	return ENXIO;
# endif
}


/*
 * send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int send_reset(oip, fin)
struct ip *oip;
fr_info_t *fin;
{
	struct tcphdr *tcp, *tcp2;
	int tlen = 0, hlen;
	struct mbuf *m;
#ifdef	USE_INET6
	ip6_t *ip6, *oip6 = (ip6_t *)oip;
#endif
	ip_t *ip;

	tcp = (struct tcphdr *)fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;		/* feedback loop */
# if	(BSD < 199306) || defined(__sgi)
	m = m_get(M_DONTWAIT, MT_HEADER);
# elif defined(__DragonFly__)
	m = m_get(MB_DONTWAIT, MT_HEADER);
# else
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
# endif
	if (m == NULL)
		return ENOBUFS;
	if (m == NULL)
		return -1;

	tlen = fin->fin_dlen - (tcp->th_off << 2) +
			((tcp->th_flags & TH_SYN) ? 1 : 0) +
			((tcp->th_flags & TH_FIN) ? 1 : 0);

#ifdef	USE_INET6
	hlen = (fin->fin_v == 6) ? sizeof(ip6_t) : sizeof(ip_t);
#else
	hlen = sizeof(ip_t);
#endif
	m->m_len = sizeof(*tcp2) + hlen;
# if	BSD >= 199306
	m->m_data += max_linkhdr;
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
# endif
	ip = mtod(m, struct ip *);
# ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
# endif
	bzero((char *)ip, sizeof(*tcp2) + hlen);
	tcp2 = (struct tcphdr *)((char *)ip + hlen);

	tcp2->th_sport = tcp->th_dport;
	tcp2->th_dport = tcp->th_sport;
	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST;
	} else {
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST|TH_ACK;
	}
	tcp2->th_off = sizeof(*tcp2) >> 2;
# ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6->ip6_plen = htons(sizeof(struct tcphdr));
		ip6->ip6_nxt = IPPROTO_TCP;
		ip6->ip6_src = oip6->ip6_dst;
		ip6->ip6_dst = oip6->ip6_src;
		tcp2->th_sum = in6_cksum(m, IPPROTO_TCP,
					 sizeof(*ip6), sizeof(*tcp2));
		return send_ip(oip, fin, &m);
	}
# endif
	ip->ip_p = IPPROTO_TCP;
	ip->ip_len = htons(sizeof(struct tcphdr));
	ip->ip_src.s_addr = oip->ip_dst.s_addr;
	ip->ip_dst.s_addr = oip->ip_src.s_addr;
	tcp2->th_sum = in_cksum(m, hlen + sizeof(*tcp2));
	ip->ip_len = hlen + sizeof(*tcp2);
	return send_ip(oip, fin, &m);
}


/*
 * Send an IP(v4/v6) datagram out into the network
 */
static int send_ip(oip, fin, mp)
ip_t *oip;
fr_info_t *fin;
struct mbuf **mp;
{
	struct mbuf *m = *mp;
	int error, hlen;
	fr_info_t frn;
	ip_t *ip;

	bzero((char *)&frn, sizeof(frn));
	frn.fin_ifp = fin->fin_ifp;
	frn.fin_v = fin->fin_v;
	frn.fin_out = fin->fin_out;
	frn.fin_mp = mp;

	ip = mtod(m, ip_t *);
	hlen = sizeof(*ip);

	ip->ip_v = fin->fin_v;
	if (ip->ip_v == 4) {
		ip->ip_hl = (sizeof(*oip) >> 2);
		ip->ip_v = IPVERSION;
		ip->ip_tos = oip->ip_tos;
		ip->ip_id = oip->ip_id;

# if defined(__NetBSD__) || \
     (defined(__OpenBSD__) && (OpenBSD >= 200012))
		if (ip_mtudisc != 0)
			ip->ip_off = IP_DF;
# else
#  if defined(__sgi)
		if (ip->ip_p == IPPROTO_TCP && tcp_mtudisc != 0)
			ip->ip_off = IP_DF;
#  endif
# endif

# if (BSD < 199306) || defined(__sgi)
		ip->ip_ttl = tcp_ttl;
# else
		ip->ip_ttl = ip_defttl;
# endif
		ip->ip_sum = 0;
		frn.fin_dp = (char *)(ip + 1);
	}
# ifdef	USE_INET6
	else if (ip->ip_v == 6) {
		ip6_t *ip6 = (ip6_t *)ip;

		hlen = sizeof(*ip6);
		ip6->ip6_hlim = 127;
		frn.fin_dp = (char *)(ip6 + 1);
	}
# endif
# ifdef	IPSEC
	m->m_pkthdr.rcvif = NULL;
# endif

	if (fr_makefrip(hlen, ip, &frn) == 0)
		error = ipfr_fastroute(m, mp, &frn, NULL);
	else
		error = EINVAL;
	return error;
}


int send_icmp_err(oip, type, fin, dst)
ip_t *oip;
int type;
fr_info_t *fin;
int dst;
{
	int err, hlen = 0, xtra = 0, iclen, ohlen = 0, avail, code;
	u_short shlen, slen = 0, soff = 0;
	struct in_addr dst4;
	struct icmp *icmp;
	struct mbuf *m;
	void *ifp;
#ifdef USE_INET6
	ip6_t *ip6, *oip6 = (ip6_t *)oip;
	struct in6_addr dst6;
#endif
	ip_t *ip;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

	avail = 0;
	m = NULL;
	ifp = fin->fin_ifp;
	if (fin->fin_v == 4) {
		if ((oip->ip_p == IPPROTO_ICMP) &&
		    !(fin->fin_fi.fi_fl & FI_SHORT))
			switch (ntohs(fin->fin_data[0]) >> 8)
			{
			case ICMP_ECHO :
			case ICMP_TSTAMP :
			case ICMP_IREQ :
			case ICMP_MASKREQ :
				break;
			default :
				return 0;
			}

# if	(BSD < 199306) || defined(__sgi)
		avail = MLEN;
		m = m_get(M_DONTWAIT, MT_HEADER);
# elif defined(__DragonFly__)
		avail = MHLEN;
		m = m_gethdr(MB_DONTWAIT, MT_HEADER);
# else
		avail = MHLEN;
		m = m_gethdr(M_DONTWAIT, MT_HEADER);
# endif
		if (m == NULL)
			return ENOBUFS;

		if (dst == 0) {
			if (fr_ifpaddr(4, ifp, &dst4) == -1)
				return -1;
		} else
			dst4.s_addr = oip->ip_dst.s_addr;

		hlen = sizeof(ip_t);
		ohlen = oip->ip_hl << 2;
		xtra = 8;
	}

#ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		hlen = sizeof(ip6_t);
		ohlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];

#ifdef __DragonFly__
		MGETHDR(m, MB_DONTWAIT, MT_HEADER);
#else
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
#endif
		if (!m)
			return ENOBUFS;

#ifdef __DragonFly__
		MCLGET(m, MB_DONTWAIT);
#else
		MCLGET(m, M_DONTWAIT);
#endif
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return ENOBUFS;
		}
# ifdef	M_TRAILINGSPACE
		m->m_len = 0;
		avail = M_TRAILINGSPACE(m);
# else
		avail = (m->m_flags & M_EXT) ? MCLBYTES : MHLEN;
# endif
		xtra = MIN(ntohs(oip6->ip6_plen) + sizeof(ip6_t),
			   avail - hlen - sizeof(*icmp) - max_linkhdr);
		if (dst == 0) {
			if (fr_ifpaddr(6, ifp, (struct in_addr *)&dst6) == -1)
				return -1;
		} else
			dst6 = oip6->ip6_dst;
	}
#endif

	iclen = hlen + sizeof(*icmp);
# if	BSD >= 199306
	avail -= (max_linkhdr + iclen);
	m->m_data += max_linkhdr;
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
	m->m_pkthdr.len = iclen;
#else
	avail -= (m->m_off + iclen);
	if (xtra > avail)
		xtra = avail;
	iclen += xtra;
#endif
	m->m_len = iclen;
	ip = mtod(m, ip_t *);
	icmp = (struct icmp *)((char *)ip + hlen);
	bzero((char *)ip, iclen);

	icmp->icmp_type = type;
	icmp->icmp_code = fin->fin_icode;
	icmp->icmp_cksum = 0;
#ifdef	icmp_nextmtu
	if (type == ICMP_UNREACH &&
	    fin->fin_icode == ICMP_UNREACH_NEEDFRAG && ifp)
		icmp->icmp_nextmtu = htons(((struct ifnet *) ifp)->if_mtu);
#endif

	if (avail) {
		slen = oip->ip_len;
		oip->ip_len = htons(oip->ip_len);
		soff = oip->ip_off;
		oip->ip_off = htons(oip->ip_off);
		bcopy((char *)oip, (char *)&icmp->icmp_ip, MIN(ohlen, avail));
		oip->ip_len = slen;
		oip->ip_off = soff;
		avail -= MIN(ohlen, avail);
	}

#ifdef	USE_INET6
	ip6 = (ip6_t *)ip;
	if (fin->fin_v == 6) {
		ip6->ip6_flow = 0;
		ip6->ip6_plen = htons(iclen - hlen);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_hlim = 0;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = oip6->ip6_src;
		if (avail)
			bcopy((char *)oip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, avail);
		icmp->icmp_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					     sizeof(*ip6), iclen - hlen);
	} else
#endif
	{

		ip->ip_src.s_addr = dst4.s_addr;
		ip->ip_dst.s_addr = oip->ip_src.s_addr;

		if (avail > 8)
			avail = 8;
		if (avail)
			bcopy((char *)oip + ohlen,
			      (char *)&icmp->icmp_ip + ohlen, avail);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sizeof(*icmp) + 8);
		ip->ip_len = iclen;
		ip->ip_p = IPPROTO_ICMP;
	}

	shlen = fin->fin_hlen;
	fin->fin_hlen = hlen;
	err = send_ip(oip, fin, &m);
	fin->fin_hlen = shlen;

	return err;
}


# if !defined(IPFILTER_LKM) && !defined(__sgi) && \
     (!defined(__DragonFly__) || !defined(__FreeBSD_version) || (__FreeBSD_version < 300000))
#  if	(BSD < 199306)
int iplinit (void);

int
#  else
void iplinit (void);

void
#  endif
iplinit()
{

#  if defined(__NetBSD__) || defined(__OpenBSD__)
	if (ipl_enable() != 0)
#  else
	if (iplattach() != 0)
#  endif
	{
		printf("IP Filter failed to attach\n");
	}
	ip_init();
}
# endif /* ! __NetBSD__ */


/*
 * Return the length of the entire mbuf.
 */
size_t mbufchainlen(m0)
struct mbuf *m0;
{
#if BSD >= 199306
	return m0->m_pkthdr.len;
#else
	size_t len = 0;

	for (; m0; m0 = m0->m_next)
		len += m0->m_len;
	return len;
#endif
}


int ipfr_fastroute(m0, mpp, fin, fdp)
struct mbuf *m0, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
	struct ip *ip, *mhip;
	struct mbuf *m = m0;
	struct route *ro;
	int len, off, error = 0, hlen, code, sout;
	struct ifnet *ifp, *sifp;
	struct sockaddr_in *dst;
	struct route iproute;
	frentry_t *fr;

	ip = NULL;
	ro = NULL;
	ifp = NULL;
	ro = &iproute;
	ro->ro_rt = NULL;

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		error = ipfr_fastroute6(m0, mpp, fin, fdp);
		if (error != 0)
			goto bad;
		goto done;
	}
#else
	if (fin->fin_v == 6)
		goto bad;
#endif

#ifdef	M_WRITABLE
	/*
	 * HOT FIX/KLUDGE:
	 *
	 * If the mbuf we're about to send is not writable (because of
	 * a cluster reference, for example) we'll need to make a copy
	 * of it since this routine modifies the contents.
	 *
	 * If you have non-crappy network hardware that can transmit data
	 * from the mbuf, rather than making a copy, this is gonna be a
	 * problem.
	 */
	if (M_WRITABLE(m) == 0) {
#ifdef __DragonFly__
		if ((m0 = m_dup(m, MB_DONTWAIT)) != NULL) {
#else
		if ((m0 = m_dup(m, M_DONTWAIT)) != NULL) {
#endif
			m_freem(*mpp);
			*mpp = m0;
			m = m0;
		} else {
			error = ENOBUFS;
			m_freem(*mpp);
			goto done;
		}
	}
#endif

	hlen = fin->fin_hlen;
	ip = mtod(m0, struct ip *);

#if defined(__NetBSD__) && defined(M_CSUM_IPv4)
	/*
	 * Clear any in-bound checksum flags for this packet.
	 */
# if (__NetBSD_Version__ > 105009999)
	m0->m_pkthdr.csum_flags = 0;
# else
	m0->m_pkthdr.csuminfo = 0;
# endif
#endif /* __NetBSD__ && M_CSUM_IPv4 */

	/*
	 * Route packet.
	 */
#if (defined(IRIX) && (IRIX >= 605))
	ROUTE_RDLOCK();
#endif
	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_addr = ip->ip_dst;

	fr = fin->fin_fr;
	if (fdp != NULL)
		ifp = fdp->fd_ifp;
	else
		ifp = fin->fin_ifp;

	/*
	 * In case we're here due to "to <if>" being used with "keep state",
	 * check that we're going in the correct direction.
	 */
	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif)) {
# if (defined(IRIX) && (IRIX >= 605))
			ROUTE_UNLOCK();
# endif
			return 0;
		}
	} else if (fdp != NULL) {
		if (fdp->fd_ip.s_addr != 0)
			dst->sin_addr = fdp->fd_ip;
	}

# if BSD >= 199306
	dst->sin_len = sizeof(*dst);
# endif
# if	(BSD >= 199306) && !defined(__NetBSD__) && !defined(__bsdi__) && \
	!defined(__OpenBSD__)
#  ifdef	RTF_CLONING
	rtalloc_ign(ro, RTF_CLONING);
#  else
	rtalloc_ign(ro, RTF_PRCLONING);
#  endif
# else
	rtalloc(ro);
# endif

	if (!ifp) {
		if (!fr || !(fr->fr_flags & FR_FASTROUTE)) {
			error = -2;
# if (defined(IRIX) && (IRIX >= 605))
			ROUTE_UNLOCK();
# endif
			goto bad;
		}
	}

	if ((ifp == NULL) && (ro->ro_rt != NULL))
		ifp = ro->ro_rt->rt_ifp;

	if ((ro->ro_rt == NULL) || (ifp == NULL)) {
		if (in_localaddr(ip->ip_dst))
			error = EHOSTUNREACH;
		else
			error = ENETUNREACH;
# if (defined(IRIX) && (IRIX >= 605))
			ROUTE_UNLOCK();
# endif
		goto bad;
	}

	if (ro->ro_rt->rt_flags & RTF_GATEWAY) {
#if (BSD >= 199306) || (defined(IRIX) && (IRIX >= 605))
		dst = (struct sockaddr_in *)ro->ro_rt->rt_gateway;
#else
		dst = (struct sockaddr_in *)&ro->ro_rt->rt_gateway;
#endif
	}
	ro->ro_rt->rt_use++;

#if (defined(IRIX) && (IRIX > 602))
	ROUTE_UNLOCK();
#endif

	/*
	 * For input packets which are being "fastrouted", they won't
	 * go back through output filtering and miss their chance to get
	 * NAT'd and counted.
	 */
	if (fin->fin_out == 0) {
		sifp = fin->fin_ifp;
		sout = fin->fin_out;
		fin->fin_ifp = ifp;
		fin->fin_out = 1;
		if ((fin->fin_fr = ipacct[1][fr_active]) &&
		    (fr_scanlist(FR_NOMATCH, ip, fin, m) & FR_ACCOUNT)) {
			ATOMIC_INCL(frstats[1].fr_acct);
		}
		fin->fin_fr = NULL;
		if (!fr || !(fr->fr_flags & FR_RETMASK))
			(void) fr_checkstate(ip, fin);

		switch (ip_natout(ip, fin))
		{
		case 0 :
			break;
		case 1 :
			ip->ip_sum = 0;
			break;
		case -1 :
			error = EINVAL;
			goto done;
			break;
		}

		fin->fin_ifp = sifp;
		fin->fin_out = sout;
	} else
		ip->ip_sum = 0;

	/*
	 * If small enough for interface, can just send directly.
	 */
	if (ip->ip_len <= ifp->if_mtu) {
# ifndef sparc
#  if (!defined(__DragonFly__) && !defined(__FreeBSD__) && !(_BSDI_VERSION >= 199510)) && \
      !(__NetBSD_Version__ >= 105110000)
		ip->ip_id = htons(ip->ip_id);
#  endif
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
# endif
# if defined(__NetBSD__) && defined(M_CSUM_IPv4)
#  if (__NetBSD_Version__ > 105009999)
		if (ifp->if_csum_flags_tx & IFCAP_CSUM_IPv4)
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4;
		else if (ip->ip_sum == 0)
			ip->ip_sum = in_cksum(m, hlen);
#  else
		if (ifp->if_capabilities & IFCAP_CSUM_IPv4)
			m->m_pkthdr.csuminfo |= M_CSUM_IPv4;
		else if (ip->ip_sum == 0)
			ip->ip_sum = in_cksum(m, hlen);
#  endif
# else
		if (!ip->ip_sum)
			ip->ip_sum = in_cksum(m, hlen);
# endif /* __NetBSD__ && M_CSUM_IPv4 */
# if	(BSD >= 199306) || (defined(IRIX) && (IRIX >= 605))
#  ifdef IRIX
		IFNET_UPPERLOCK(ifp);
#  endif
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst,
					  ro->ro_rt);
#  ifdef IRIX
		IFNET_UPPERUNLOCK(ifp);
#  endif
# else
		error = (*ifp->if_output)(ifp, m, (struct sockaddr *)dst);
# endif
		goto done;
	}

	/*
	 * Too large for interface; fragment if possible.
	 * Must be able to put at least 8 bytes per fragment.
	 */
	if (ip->ip_off & IP_DF) {
		error = EMSGSIZE;
		goto bad;
	}
	len = (ifp->if_mtu - hlen) &~ 7;
	if (len < 8) {
		error = EMSGSIZE;
		goto bad;
	}

    {
	int mhlen, firstlen = len;
	struct mbuf **mnext = &m->m_act;

	/*
	 * Loop through length of segment after first fragment,
	 * make new header and copy data of each part and link onto chain.
	 */
	m0 = m;
	mhlen = sizeof (struct ip);
	for (off = hlen + len; off < ip->ip_len; off += len) {
# ifdef	__DragonFly__
		MGETHDR(m, MB_DONTWAIT, MT_HEADER);
# elif	defined(MGETHDR)
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
# else
		MGET(m, M_DONTWAIT, MT_HEADER);
# endif
		if (m == 0) {
			error = ENOBUFS;
			goto bad;
		}
# if BSD >= 199306
		m->m_data += max_linkhdr;
# else
		m->m_off = MMAXOFF - hlen;
# endif
		mhip = mtod(m, struct ip *);
		bcopy((char *)ip, (char *)mhip, sizeof(*ip));
		if (hlen > sizeof (struct ip)) {
			mhlen = ip_optcopy(ip, mhip) + sizeof (struct ip);
			mhip->ip_hl = mhlen >> 2;
		}
		m->m_len = mhlen;
		mhip->ip_off = ((off - hlen) >> 3) + (ip->ip_off & ~IP_MF);
		if (ip->ip_off & IP_MF)
			mhip->ip_off |= IP_MF;
		if (off + len >= ip->ip_len)
			len = ip->ip_len - off;
		else
			mhip->ip_off |= IP_MF;
		mhip->ip_len = htons((u_short)(len + mhlen));
		m->m_next = m_copy(m0, off, len);
		if (m->m_next == 0) {
			error = ENOBUFS;	/* ??? */
			goto sendorfree;
		}
# if BSD >= 199306
		m->m_pkthdr.len = mhlen + len;
		m->m_pkthdr.rcvif = NULL;
# endif
		mhip->ip_off = htons((u_short)mhip->ip_off);
		mhip->ip_sum = 0;
		mhip->ip_sum = in_cksum(m, mhlen);
		*mnext = m;
		mnext = &m->m_act;
	}
	/*
	 * Update first fragment by trimming what's been copied out
	 * and updating header, then send each fragment (in order).
	 */
	m_adj(m0, hlen + firstlen - ip->ip_len);
	ip->ip_len = htons((u_short)(hlen + firstlen));
	ip->ip_off = htons((u_short)(ip->ip_off | IP_MF));
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m0, hlen);
sendorfree:
	for (m = m0; m; m = m0) {
		m0 = m->m_act;
		m->m_act = 0;
		if (error == 0)
# if (BSD >= 199306) || (defined(IRIX) && (IRIX >= 605))
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst, ro->ro_rt);
# else
			error = (*ifp->if_output)(ifp, m,
			    (struct sockaddr *)dst);
# endif
		else
			m_freem(m);
	}
    }	
done:
	if (!error)
		ipl_frouteok[0]++;
	else
		ipl_frouteok[1]++;

	if (ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
	}
	*mpp = NULL;
	return error;
bad:
	if ((error == EMSGSIZE) && (fin->fin_v == 4)) {
		sifp = fin->fin_ifp;
		code = fin->fin_icode;
		fin->fin_icode = ICMP_UNREACH_NEEDFRAG;
		fin->fin_ifp = ifp;
		(void) send_icmp_err(ip, ICMP_UNREACH, fin, 1);
		fin->fin_ifp = sifp;
		fin->fin_icode = code;
	}
	m_freem(m);
	goto done;
}


/*
 * Return true or false depending on whether the route to the
 * given IP address uses the same interface as the one passed.
 */
int fr_verifysrc(ipa, ifp)
struct in_addr ipa;
void *ifp;
{
	struct sockaddr_in *dst;
	struct route iproute;

	bzero((char *)&iproute, sizeof(iproute));
	dst = (struct sockaddr_in *)&iproute.ro_dst;
# if    (BSD >= 199306)
	dst->sin_len = sizeof(*dst);
# endif
	dst->sin_family = AF_INET;
	dst->sin_addr = ipa;
# if    (BSD >= 199306) && !defined(__NetBSD__) && !defined(__bsdi__) && \
	!defined(__OpenBSD__)
#  ifdef        RTF_CLONING
	rtalloc_ign(&iproute, RTF_CLONING);
#  else
	rtalloc_ign(&iproute, RTF_PRCLONING);
#  endif
# else
	rtalloc(&iproute);
# endif
	if (iproute.ro_rt == NULL)
		return 0;
	return (ifp == iproute.ro_rt->rt_ifp);
}


# ifdef	USE_GETIFNAME
char *
get_ifname(ifp)
struct ifnet *ifp;
{
	static char workbuf[64];

	sprintf(workbuf, "%s%d", ifp->if_name, ifp->if_unit);
	return workbuf;
}
# endif


# if defined(USE_INET6)
/*
 * This is the IPv6 specific fastroute code.  It doesn't clean up the mbuf's
 * or ensure that it is an IPv6 packet that is being forwarded, those are
 * expected to be done by the called (ipfr_fastroute).
 */
static int ipfr_fastroute6(m0, mpp, fin, fdp)
struct mbuf *m0, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
	struct route_in6 ip6route;
	struct sockaddr_in6 *dst6;
	struct route_in6 *ro;
	struct ifnet *ifp;
	frentry_t *fr;
#if defined(OpenBSD) && (OpenBSD >= 200211)
	struct route_in6 *ro_pmtu = NULL;
	struct in6_addr finaldst;
	ip6_t *ip6;
#endif
	u_long mtu;
	int error;

	ro = &ip6route;
	fr = fin->fin_fr;
	bzero((caddr_t)ro, sizeof(*ro));
	dst6 = (struct sockaddr_in6 *)&ro->ro_dst;
	dst6->sin6_family = AF_INET6;
	dst6->sin6_len = sizeof(struct sockaddr_in6);
	dst6->sin6_addr = fin->fin_fi.fi_dst.in6;

	if (fdp != NULL)
		ifp = fdp->fd_ifp;
	else
		ifp = fin->fin_ifp;

	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif))
			return 0;
	} else if (fdp != NULL) {
		if (IP6_NOTZERO(&fdp->fd_ip6))
			dst6->sin6_addr = fdp->fd_ip6.in6;
	}
	if (ifp == NULL)
		return -2;

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__)
	/* KAME */
	if (IN6_IS_ADDR_LINKLOCAL(&dst6->sin6_addr))
		dst6->sin6_addr.s6_addr16[1] = htons(ifp->if_index);
#endif
	rtalloc((struct route *)ro);

	if ((ifp == NULL) && (ro->ro_rt != NULL))
		ifp = ro->ro_rt->rt_ifp;

	if ((ro->ro_rt == NULL) || (ifp == NULL) ||
	    (ifp != ro->ro_rt->rt_ifp)) {
		error = EHOSTUNREACH;
	} else {
		if (ro->ro_rt->rt_flags & RTF_GATEWAY)
			dst6 = (struct sockaddr_in6 *)ro->ro_rt->rt_gateway;
		ro->ro_rt->rt_use++;

#if defined(OpenBSD) && (OpenBSD >= 200211)
		ip6 = mtod(m0, ip6_t *);
		ro_pmtu = ro;
		finaldst = ip6->ip6_dst;
		error = ip6_getpmtu(ro_pmtu, ro, ifp, &finaldst, &mtu);
		if (error == 0) {
#else
#ifdef ND_IFINFO
			mtu = ND_IFINFO(ifp)->linkmtu;
#else
			mtu = nd_ifinfo[ifp->if_index].linkmtu;
#endif
#endif
			if (m0->m_pkthdr.len <= mtu)
				error = nd6_output(ifp, fin->fin_ifp, m0,
						   dst6, ro->ro_rt);
			else
				error = EMSGSIZE;
#if defined(OpenBSD) && (OpenBSD >= 200211)
		}
#endif
	}

	if (ro->ro_rt != NULL) {
		RTFREE(ro->ro_rt);
	}
	return error;
}
# endif
#else /* #ifdef _KERNEL */


# if defined(__sgi) && (IRIX < 605)
static int no_output (struct ifnet *ifp, struct mbuf *m,
			   struct sockaddr *s)
# else
static int no_output (struct ifnet *ifp, struct mbuf *m,
			   struct sockaddr *s, struct rtentry *rt)
# endif
{
	return 0;
}


# ifdef __STDC__
#  if defined(__sgi) && (IRIX < 605)
static int write_output (struct ifnet *ifp, struct mbuf *m,
			     struct sockaddr *s)
#  else
static int write_output (struct ifnet *ifp, struct mbuf *m,
			     struct sockaddr *s, struct rtentry *rt)
#  endif
{
	ip_t *ip = (ip_t *)m;
# else
static int write_output(ifp, ip)
struct ifnet *ifp;
ip_t *ip;
{
# endif
	char fname[32];
	int fd;

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603)) || \
	(defined(__DragonFly__))
	sprintf(fname, "%s", ifp->if_xname);
# else
	sprintf(fname, "%s%d", ifp->if_name, ifp->if_unit);
# endif
	fd = open(fname, O_WRONLY|O_APPEND);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	write(fd, (char *)ip, ntohs(ip->ip_len));
	close(fd);
	return 0;
}


char *get_ifname(ifp)
struct ifnet *ifp;
{
# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
     (defined(OpenBSD) && (OpenBSD >= 199603)) || \
     (defined(__DragonFly__))
	return ifp->if_xname;
# else
	static char fullifname[LIFNAMSIZ];

	sprintf(fullifname, "%s%d", ifp->if_name, ifp->if_unit);
	return fullifname;
# endif
}


struct ifnet *get_unit(ifname, v)
char *ifname;
int v;
{
	struct ifnet *ifp, **ifa, **old_ifneta;

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
     (defined(OpenBSD) && (OpenBSD >= 199603)) || \
     (defined(__DragonFly__))
		if (!strncmp(ifname, ifp->if_xname, sizeof(ifp->if_xname)))
# else
		char fullname[LIFNAMSIZ];

		sprintf(fullname, "%s%d", ifp->if_name, ifp->if_unit);
		if (!strcmp(ifname, fullname))
# endif
			return ifp;
	}

	if (!ifneta) {
		ifneta = (struct ifnet **)malloc(sizeof(ifp) * 2);
		if (!ifneta)
			return NULL;
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		if (!ifneta[0]) {
			free(ifneta);
			return NULL;
		}
		nifs = 1;
	} else {
		old_ifneta = ifneta;
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(*ifa));
		if (!ifneta) {
			free(old_ifneta);
			nifs = 0;
			return NULL;
		}
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
		if (!ifneta[nifs - 1]) {
			nifs--;
			return NULL;
		}
	}
	ifp = ifneta[nifs - 1];

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
     (defined(OpenBSD) && (OpenBSD >= 199603)) || \
     (defined(__DragonFly__))
	strncpy(ifp->if_xname, ifname, sizeof(ifp->if_xname));
# else
	ifp->if_name = strdup(ifname);

	ifname = ifp->if_name;
	while (*ifname && !isdigit(*ifname))
		ifname++;
	if (*ifname && isdigit(*ifname)) {
		ifp->if_unit = atoi(ifname);
		*ifname = '\0';
	} else
		ifp->if_unit = -1;
# endif
	ifp->if_output = no_output;
	return ifp;
}



void init_ifp()
{
	struct ifnet *ifp, **ifa;
	char fname[32];
	int fd;

# if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
	(defined(OpenBSD) && (OpenBSD >= 199603)) || \
	(defined(__DragonFly__))
	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s", ifp->if_xname);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
# else

	for (ifa = ifneta; ifa && (ifp = *ifa); ifa++) {
		ifp->if_output = write_output;
		sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
# endif
}


int send_reset(ip, fin)
ip_t *ip;
fr_info_t *fin;
{
	verbose("- TCP RST sent\n");
	return 0;
}


int send_icmp_err(ip, code, fin, dst)
ip_t *ip;
int code;
fr_info_t *fin;
int dst;
{
	verbose("- ICMP UNREACHABLE sent\n");
	return 0;
}


void frsync()
{
	return;
}

void m_copydata(m, off, len, cp)
mb_t *m;
int off, len;
caddr_t cp;
{
	bcopy((char *)m + off, cp, len);
}


int ipfuiomove(buf, len, rwflag, uio)
caddr_t buf;
int len, rwflag;
struct uio *uio;
{
	int left, ioc, num, offset;
	struct iovec *io;
	char *start;

	if (rwflag == UIO_READ) {
		left = len;
		ioc = 0;

		offset = uio->uio_offset;

		while ((left > 0) && (ioc < uio->uio_iovcnt)) {
			io = uio->uio_iov + ioc;
			num = io->iov_len;
			if (num > left)
				num = left;
			start = (char *)io->iov_base + offset;
			if (start > (char *)io->iov_base + io->iov_len) {
				offset -= io->iov_len;
				ioc++;
				continue;
			}
			bcopy(buf, start, num);
			uio->uio_resid -= num;
			uio->uio_offset += num;
			left -= num;
			if (left > 0)
				ioc++;
		}
		if (left > 0)
			return EFAULT;
	}
	return 0;
}
#endif /* _KERNEL */
