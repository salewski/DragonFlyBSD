/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: src/sys/netatm/atm_proto.c,v 1.3 1999/08/28 00:48:36 peter Exp $
 *	@(#) $DragonFly: src/sys/netproto/atm/atm_proto.c,v 1.10 2005/03/04 02:21:49 hsu Exp $
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM socket protocol family support definitions
 *
 */

#include "kern_include.h"

struct protosw atmsw[] = {
{	SOCK_DGRAM,				/* ioctl()-only */
	&atmdomain,
	0,
	0,
	0,			/* pr_input */
	0,			/* pr_output */
	0,			/* pr_ctlinput */
	0,			/* pr_ctloutput */
	cpu0_soport,		/* pr_soport */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	0,			/* pr_drain */
	&atm_dgram_usrreqs,	/* pr_usrreqs */
},

{	SOCK_SEQPACKET,				/* AAL-5 */
	&atmdomain,
	ATM_PROTO_AAL5,
	PR_ATOMIC|PR_CONNREQUIRED,
	0,			/* pr_input */
	0,			/* pr_output */
	0,			/* pr_ctlinput */
	atm_aal5_ctloutput,	/* pr_ctloutput */
	cpu0_soport,		/* pr_mport */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	0,			/* pr_drain */
	&atm_aal5_usrreqs,	/* pr_usrreqs */
},

#ifdef XXX
{	SOCK_SEQPACKET,				/* SSCOP */
	&atmdomain,
	ATM_PROTO_SSCOP,
	PR_ATOMIC|PR_CONNREQUIRED|PR_WANTRCVD,
	x,			/* pr_input */
	x,			/* pr_output */
	x,			/* pr_ctlinput */
	x,			/* pr_ctloutput */
	0,			/* pr_mport */
	0,			/* pr_init */
	0,			/* pr_fasttimo */
	0,			/* pr_slowtimo */
	x,			/* pr_drain */
	x,			/* pr_usrreqs */
},
#endif
};

struct domain atmdomain = {
	AF_ATM, "atm", atm_initialize, NULL, NULL,
	atmsw, &atmsw[sizeof(atmsw) / sizeof(atmsw[0])],
};

DOMAIN_SET(atm);

/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp1(struct socket *so)
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to protocol address
 *	p	pointer to process
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp2(struct socket *so, struct sockaddr *addr, thread_t td)
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	addr	pointer to pointer to protocol address
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp3(so, addr)
	struct socket	*so;
	struct sockaddr	**addr;
{
	return (EOPNOTSUPP);
}


/*
 * Protocol request not supported
 *
 * Arguments:
 *	so	pointer to socket
 *	i	integer
 *	m	pointer to kernel buffer
 *	addr	pointer to protocol address
 *	m2	pointer to kernel buffer
 *	p	pointer to process
 *
 * Returns:
 *	errno	error - operation not supported
 *
 */
int
atm_proto_notsupp4(
	struct socket	*so,
	int		i,
	KBuffer		*m,
	struct sockaddr	*addr,
	KBuffer		*m2,
	struct thread	*td
) {
	return (EOPNOTSUPP);
}
