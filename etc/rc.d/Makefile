# $NetBSD: Makefile,v 1.16 2001/01/14 15:37:22 minoura Exp $
# $FreeBSD: src/etc/rc.d/Makefile,v 1.20 2003/06/29 05:15:57 mtm Exp $
# $DragonFly: src/etc/rc.d/Makefile,v 1.11 2005/02/20 21:49:51 joerg Exp $

.include <bsd.own.mk>

# note: bgfsk, devd, devfs, and lomac left out (from 5.0)
#
FILES=	DAEMON LOGIN NETWORKING SERVERS abi accounting addswap adjkerntz \
	amd apm apmd archdep atm1 atm2.sh atm3.sh \
	battd bootconf.sh bootparams ccd cleanvar \
	cleartmp cron dhclient dhcpd dhcrelay diskless dmesg dumpon fsck \
	hostname inetd initdiskless initrandom ip6fw ipfilter ipfs ipfw ipmon \
	ipnat ipsec ipxrouted isdnd jail \
	kadmind kerberos keyserv kldxref kpasswdd \
	ldconfig lkm1 lkm2 lkm3 local localdaemons lpd \
	mixer motd mountall mountcritlocal mountcritremote \
	mountd moused mroute6d mrouted msgs \
	named netif netoptions network network1 network2 network3 \
	network_ipv6 nfsclient nfsd nfslocking nfsserver nisdomain ntpd \
	othermta pccard pcvt pf pflog ppp ppp-user pppoed pwcheck \
	quota random rarpd rcconf.sh resident rndcontrol root route6d routed \
	routing rpcbind rtadvd rwho sysdb savecore securelevel sendmail \
	serial sppp sshd swap1 syscons sysctl syslogd timed ttys usbd \
	vinum virecover watchdogd ypbind yppasswdd ypserv ypset ypupdated \
	ypxfrd varsym wscons
FILESDIR=	/etc/rc.d
FILESMODE=	${BINMODE}

NOPROG= noprog

.include <bsd.prog.mk>
