/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: netdb.h.in,v 1.34 2001/08/16 06:39:33 marka Exp $ */
/* $DragonFly: src/usr.sbin/named/include/lwres/netdb.h,v 1.1 2004/05/27 18:15:42 dillon Exp $ */

#ifndef LWRES_NETDB_H
#define LWRES_NETDB_H 1

#include <stddef.h>	/* Required on FreeBSD (and  others?) for size_t. */
#include <netdb.h>	/* Contractual provision. */

#include <lwres/lang.h>

/*
 * Define if <netdb.h> does not declare struct addrinfo.
 */
#undef ISC_LWRES_NEEDADDRINFO

#ifdef ISC_LWRES_NEEDADDRINFO
struct addrinfo {
	int		ai_flags;      /* AI_PASSIVE, AI_CANONNAME */
	int		ai_family;     /* PF_xxx */
	int		ai_socktype;   /* SOCK_xxx */
	int		ai_protocol;   /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
	size_t		ai_addrlen;    /* Length of ai_addr */
	char		*ai_canonname; /* Canonical name for hostname */
	struct sockaddr	*ai_addr;      /* Binary address */
	struct addrinfo	*ai_next;      /* Next structure in linked list */
};
#endif

/*
 * Undefine all #defines we are interested in as <netdb.h> may or may not have
 * defined them.
 */

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#undef	NETDB_INTERNAL
#undef	NETDB_SUCCESS
#undef	HOST_NOT_FOUND
#undef	TRY_AGAIN
#undef	NO_RECOVERY
#undef	NO_DATA
#undef	NO_ADDRESS

#define	NETDB_INTERNAL	-1	/* see errno */
#define	NETDB_SUCCESS	0	/* no problem */
#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

/*
 * Error return codes from getaddrinfo()
 */

#undef	EAI_ADDRFAMILY
#undef	EAI_AGAIN
#undef	EAI_BADFLAGS
#undef	EAI_FAIL
#undef	EAI_FAMILY
#undef	EAI_MEMORY
#undef	EAI_NODATA
#undef	EAI_NONAME
#undef	EAI_SERVICE
#undef	EAI_SOCKTYPE
#undef	EAI_SYSTEM
#undef	EAI_BADHINTS
#undef	EAI_PROTOCOL
#undef	EAI_MAX

#define	EAI_ADDRFAMILY	 1	/* address family for hostname not supported */
#define	EAI_AGAIN	 2	/* temporary failure in name resolution */
#define	EAI_BADFLAGS	 3	/* invalid value for ai_flags */
#define	EAI_FAIL	 4	/* non-recoverable failure in name resolution */
#define	EAI_FAMILY	 5	/* ai_family not supported */
#define	EAI_MEMORY	 6	/* memory allocation failure */
#define	EAI_NODATA	 7	/* no address associated with hostname */
#define	EAI_NONAME	 8	/* hostname nor servname provided, or not known */
#define	EAI_SERVICE	 9	/* servname not supported for ai_socktype */
#define	EAI_SOCKTYPE	10	/* ai_socktype not supported */
#define	EAI_SYSTEM	11	/* system error returned in errno */
#define EAI_BADHINTS	12
#define EAI_PROTOCOL	13
#define EAI_MAX		14

/*
 * Flag values for getaddrinfo()
 */
#undef	AI_PASSIVE
#undef	AI_CANONNAME
#undef	AI_NUMERICHOST

#define	AI_PASSIVE	0x00000001
#define	AI_CANONNAME	0x00000002
#define AI_NUMERICHOST	0x00000004

/*
 * Flag values for getipnodebyname()
 */
#undef AI_V4MAPPED
#undef AI_ALL
#undef AI_ADDRCONFIG
#undef AI_DEFAULT

#define AI_V4MAPPED	0x00000008
#define AI_ALL		0x00000010
#define AI_ADDRCONFIG	0x00000020
#define AI_DEFAULT	(AI_V4MAPPED|AI_ADDRCONFIG)

/*
 * Constants for lwres_getnameinfo()
 */
#undef	NI_MAXHOST
#undef	NI_MAXSERV

#define	NI_MAXHOST	1025
#define	NI_MAXSERV	32

/*
 * Flag values for lwres_getnameinfo()
 */
#undef	NI_NOFQDN
#undef	NI_NUMERICHOST
#undef	NI_NAMEREQD
#undef	NI_NUMERICSERV
#undef	NI_DGRAM
#undef	NI_NUMERICSCOPE

#define	NI_NOFQDN	0x00000001
#define	NI_NUMERICHOST	0x00000002
#define	NI_NAMEREQD	0x00000004
#define	NI_NUMERICSERV	0x00000008
#define	NI_DGRAM	0x00000010
#define	NI_NUMERICSCOPE	0x00000020	/*2553bis-00*/

/*
 * Define if <netdb.h> does not declare struct rrsetinfo.
 */
#define ISC_LWRES_NEEDRRSETINFO 1

#ifdef ISC_LWRES_NEEDRRSETINFO
/*
 * Structures for getrrsetbyname()
 */
struct rdatainfo {
	unsigned int		rdi_length;
	unsigned char		*rdi_data;
};

struct rrsetinfo {
	unsigned int		rri_flags;
	int			rri_rdclass;
	int			rri_rdtype;
	unsigned int		rri_ttl;
	unsigned int		rri_nrdatas;
	unsigned int		rri_nsigs;
	char			*rri_name;
	struct rdatainfo	*rri_rdatas;
	struct rdatainfo	*rri_sigs;
};

/*
 * Flags for getrrsetbyname()
 */
#define RRSET_VALIDATED		0x00000001
	/* Set was dnssec validated */

/*
 * Return codes for getrrsetbyname()
 */
#define ERRSET_SUCCESS		0
#define ERRSET_NOMEMORY		1
#define ERRSET_FAIL		2
#define ERRSET_INVAL		3
#define	ERRSET_NONAME	 	4
#define	ERRSET_NODATA	 	5
#endif

/*
 * Define to map into lwres_ namespace.
 */

#define LWRES_NAMESPACE

#ifdef LWRES_NAMESPACE

/*
 * Use our versions not the ones from the C library.
 */

#ifdef getnameinfo
#undef getnameinfo
#endif
#define getnameinfo lwres_getnameinfo

#ifdef getaddrinfo
#undef getaddrinfo
#endif
#define getaddrinfo lwres_getaddrinfo

#ifdef freeaddrinfo
#undef freeaddrinfo
#endif
#define freeaddrinfo lwres_freeaddrinfo

#ifdef gai_strerror
#undef gai_strerror
#endif
#define gai_strerror lwres_gai_strerror

#ifdef herror
#undef herror
#endif
#define herror lwres_herror

#ifdef hstrerror
#undef hstrerror
#endif
#define hstrerror lwres_hstrerror

#ifdef getipnodebyname
#undef getipnodebyname
#endif
#define getipnodebyname lwres_getipnodebyname

#ifdef getipnodebyaddr
#undef getipnodebyaddr
#endif
#define getipnodebyaddr lwres_getipnodebyaddr

#ifdef freehostent
#undef freehostent
#endif
#define freehostent lwres_freehostent

#ifdef gethostbyname
#undef gethostbyname
#endif
#define gethostbyname lwres_gethostbyname

#ifdef gethostbyname2
#undef gethostbyname2
#endif
#define gethostbyname2 lwres_gethostbyname2

#ifdef gethostbyaddr
#undef gethostbyaddr
#endif
#define gethostbyaddr lwres_gethostbyaddr

#ifdef gethostent
#undef gethostent
#endif
#define gethostent lwres_gethostent

#ifdef sethostent
#undef sethostent
#endif
#define sethostent lwres_sethostent

#ifdef endhostent
#undef endhostent
#endif
#define endhostent lwres_endhostent

/* #define sethostfile lwres_sethostfile */

#ifdef gethostbyname_r
#undef gethostbyname_r
#endif
#define gethostbyname_r lwres_gethostbyname_r

#ifdef gethostbyaddr_r
#undef gethostbyaddr_r
#endif
#define gethostbyaddr_r lwres_gethostbyaddr_r

#ifdef gethostent_r
#undef gethostent_r
#endif
#define gethostent_r lwres_gethostent_r

#ifdef sethostent_r
#undef sethostent_r
#endif
#define sethostent_r lwres_sethostent_r

#ifdef endhostent_r
#undef endhostent_r
#endif
#define endhostent_r lwres_endhostent_r

#ifdef getrrsetbyname
#undef getrrsetbyname
#endif
#define getrrsetbyname lwres_getrrsetbyname

#ifdef freerrset
#undef freerrset
#endif
#define freerrset lwres_freerrset

#ifdef notyet
#define getservbyname lwres_getservbyname
#define getservbyport lwres_getservbyport
#define getservent lwres_getservent
#define setservent lwres_setservent
#define endservent lwres_endservent

#define getservbyname_r lwres_getservbyname_r
#define getservbyport_r lwres_getservbyport_r
#define getservent_r lwres_getservent_r
#define setservent_r lwres_setservent_r
#define endservent_r lwres_endservent_r

#define getprotobyname lwres_getprotobyname
#define getprotobynumber lwres_getprotobynumber
#define getprotoent lwres_getprotoent
#define setprotoent lwres_setprotoent
#define endprotoent lwres_endprotoent

#define getprotobyname_r lwres_getprotobyname_r
#define getprotobynumber_r lwres_getprotobynumber_r
#define getprotoent_r lwres_getprotoent_r
#define setprotoent_r lwres_setprotoent_r
#define endprotoent_r lwres_endprotoent_r

#ifdef getnetbyname
#undef getnetbyname
#endif
#define getnetbyname lwres_getnetbyname

#ifdef getnetbyaddr
#undef getnetbyaddr
#endif
#define getnetbyaddr lwres_getnetbyaddr

#ifdef getnetent
#undef getnetent
#endif
#define getnetent lwres_getnetent

#ifdef setnetent
#undef setnetent
#endif
#define setnetent lwres_setnetent

#ifdef endnetent
#undef endnetent
#endif
#define endnetent lwres_endnetent


#ifdef getnetbyname_r
#undef getnetbyname_r
#endif
#define getnetbyname_r lwres_getnetbyname_r

#ifdef getnetbyaddr_r
#undef getnetbyaddr_r
#endif
#define getnetbyaddr_r lwres_getnetbyaddr_r

#ifdef getnetent_r
#undef getnetent_r
#endif
#define getnetent_r lwres_getnetent_r

#ifdef setnetent_r
#undef setnetent_r
#endif
#define setnetent_r lwres_setnetent_r

#ifdef endnetent_r
#undef endnetent_r
#endif
#define endnetent_r lwres_endnetent_r
#endif	/* notyet */

#ifdef h_errno
#undef h_errno
#endif
#define h_errno lwres_h_errno

#endif	/* LWRES_NAMESPACE */

LWRES_LANG_BEGINDECLS

extern int lwres_h_errno;

int		lwres_getaddrinfo(const char *, const char *,
				 const struct addrinfo *, struct addrinfo **);
int		lwres_getnameinfo(const struct sockaddr *, size_t, char *,
				 size_t, char *, size_t, int);
void		lwres_freeaddrinfo(struct addrinfo *);
char		*lwres_gai_strerror(int);

struct hostent	*lwres_gethostbyaddr(const char *, int, int);
struct hostent	*lwres_gethostbyname(const char *);
struct hostent	*lwres_gethostbyname2(const char *, int);
struct hostent	*lwres_gethostent(void);
struct hostent	*lwres_getipnodebyname(const char *, int, int, int *);
struct hostent	*lwres_getipnodebyaddr(const void *, size_t, int, int *);
void		lwres_endhostent(void);
void		lwres_sethostent(int);
/* void		lwres_sethostfile(const char *); */
void		lwres_freehostent(struct hostent *);

int		lwres_getrrsetbyname(const char *, unsigned int, unsigned int,
				     unsigned int, struct rrsetinfo **);
void		lwres_freerrset(struct rrsetinfo *);

#ifdef notyet
struct netent	*lwres_getnetbyaddr(unsigned long, int);
struct netent	*lwres_getnetbyname(const char *);
struct netent	*lwres_getnetent(void);
void		lwres_endnetent(void);
void		lwres_setnetent(int);

struct protoent	*lwres_getprotobyname(const char *);
struct protoent	*lwres_getprotobynumber(int);
struct protoent	*lwres_getprotoent(void);
void		lwres_endprotoent(void);
void		lwres_setprotoent(int);

struct servent	*lwres_getservbyname(const char *, const char *);
struct servent	*lwres_getservbyport(int, const char *);
struct servent	*lwres_getservent(void);
void		lwres_endservent(void);
void		lwres_setservent(int);
#endif /* notyet */

void		lwres_herror(const char *);
const char	*lwres_hstrerror(int);


struct hostent	*lwres_gethostbyaddr_r(const char *, int, int, struct hostent *,
					char *, int, int *);
struct hostent	*lwres_gethostbyname_r(const char *, struct hostent *,
					char *, int, int *);
struct hostent	*lwres_gethostent_r(struct hostent *, char *, int, int *);
void		lwres_sethostent_r(int);
void		lwres_endhostent_r(void);

#ifdef notyet
struct netent	*lwres_getnetbyname_r(const char *, struct netent *,
					char *, int);
struct netent	*lwres_getnetbyaddr_r(long, int, struct netent *,
					char *, int);
struct netent	*lwres_getnetent_r(struct netent *, char *, int);
void		lwres_setnetent_r(int);
void		lwres_endnetent_r(void);

struct protoent	*lwres_getprotobyname_r(const char *,
				struct protoent *, char *, int);
struct protoent	*lwres_getprotobynumber_r(int,
				struct protoent *, char *, int);
struct protoent	*lwres_getprotoent_r(struct protoent *, char *, int);
void		lwres_setprotoent_r(int);
void		lwres_endprotoent_r(void);

struct servent	*lwres_getservbyname_r(const char *name, const char *,
					struct servent *, char *, int);
struct servent	*lwres_getservbyport_r(int port, const char *,
					struct servent *, char *, int);
struct servent	*lwres_getservent_r(struct servent *, char *, int);
void		lwres_setservent_r(int);
void		lwres_endservent_r(void);
#endif	/* notyet */

LWRES_LANG_ENDDECLS

#ifdef notyet
/* This is nec'y to make this include file properly replace the sun version. */
#ifdef sun
#ifdef __GNU_LIBRARY__
#include <rpc/netdb.h>		/* Required. */
#else /* !__GNU_LIBRARY__ */
struct rpcent {
	char	*r_name;	/* name of server for this rpc program */
	char	**r_aliases;	/* alias list */
	int	r_number;	/* rpc program number */
};
struct rpcent	*lwres_getrpcbyname();
struct rpcent	*lwres_getrpcbynumber(),
struct rpcent	*lwres_getrpcent();
#endif /* __GNU_LIBRARY__ */
#endif /* sun */
#endif /* notyet */

/*
 * Tell Emacs to use C mode on this file.
 * Local variables:
 * mode: c
 * End:
 */

#endif /* LWRES_NETDB_H */
