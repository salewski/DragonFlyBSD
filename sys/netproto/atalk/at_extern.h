/*
 * $DragonFly: src/sys/netproto/atalk/at_extern.h,v 1.7 2004/06/03 15:04:51 joerg Exp $
 */
struct mbuf;
struct sockaddr_at;

#ifdef _NET_IF_ARP_H_
extern timeout_t	aarpprobe;
extern int	aarpresolve	(struct arpcom *,
					struct mbuf *,
					struct sockaddr_at *,
					u_char *);
extern int	at_broadcast	(struct sockaddr_at  *);
#endif

#ifdef _NETATALK_AARP_H_
extern void	aarptfree	(struct aarptab *);
#endif

struct ifnet;
struct netmsg;
struct proc;
struct socket;

extern int	aarpintr	(struct netmsg *);
extern int	at1intr		(struct netmsg *);
extern int	at2intr		(struct netmsg *);
extern void	aarp_clean	(void);
extern int	at_control	( struct socket *so,
					u_long cmd,
					caddr_t data,
					struct ifnet *ifp,
					struct thread *td );
extern u_short	at_cksum	( struct mbuf *m, int skip);
extern void	ddp_init	(void );
extern struct at_ifaddr *at_ifawithnet	(struct sockaddr_at *);
#ifdef	_NETATALK_DDP_VAR_H_
extern int	ddp_output(struct mbuf *m, struct socket *so, ...); 

#endif
#if	defined (_NETATALK_DDP_VAR_H_) && defined(_NETATALK_AT_VAR_H_)
extern struct ddpcb  *ddp_search (struct sockaddr_at *,
                                		struct sockaddr_at *,
						struct at_ifaddr *);
#endif
#ifdef _NET_ROUTE_H_
int     ddp_route( struct mbuf *m, struct route *ro);
#endif
