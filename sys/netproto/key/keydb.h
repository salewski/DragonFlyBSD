/*	$FreeBSD: src/sys/netkey/keydb.h,v 1.2.2.3 2003/01/11 19:10:59 ume Exp $	*/
/*	$DragonFly: src/sys/netproto/key/keydb.h,v 1.4 2003/08/23 10:06:24 rob Exp $	*/
/*	$KAME: keydb.h,v 1.14 2000/08/02 17:58:26 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETKEY_KEYDB_H_
#define _NETKEY_KEYDB_H_

#ifdef _KERNEL

#include "key_var.h"

/* Security Assocciation Index */
/* NOTE: Ensure to be same address family */
struct secasindex {
	struct sockaddr_storage src;	/* srouce address for SA */
	struct sockaddr_storage dst;	/* destination address for SA */
	u_int16_t proto;		/* IPPROTO_ESP or IPPROTO_AH */
	u_int8_t mode;			/* mode of protocol, see ipsec.h */
	u_int32_t reqid;		/* reqid id who owned this SA */
					/* see IPSEC_MANUAL_REQID_MAX. */
};

/* Security Association Data Base */
struct secashead {
	LIST_ENTRY(secashead) chain;

	struct secasindex saidx;

	struct sadb_ident *idents;	/* source identity */
	struct sadb_ident *identd;	/* destination identity */
					/* XXX I don't know how to use them. */

	u_int8_t state;			/* MATURE or DEAD. */
	LIST_HEAD(_satree, secasvar) savtree[SADB_SASTATE_MAX+1];
					/* SA chain */
					/* The first of this list is newer SA */

	union {
		struct route sau_route;
		struct route_in6 sau_route6;
	} sa_u;
#define sa_route sa_u.sau_route
};

/* Security Association */
struct secasvar {
	LIST_ENTRY(secasvar) chain;

	int refcnt;			/* reference count */
	u_int8_t state;			/* Status of this Association */

	u_int8_t alg_auth;		/* Authentication Algorithm Identifier*/
	u_int8_t alg_enc;		/* Cipher Algorithm Identifier */
	u_int32_t spi;			/* SPI Value, network byte order */
	u_int32_t flags;		/* holder for SADB_KEY_FLAGS */

	struct sadb_key *key_auth;	/* Key for Authentication */
	struct sadb_key *key_enc;	/* Key for Encryption */
	caddr_t iv;			/* Initilization Vector */
	u_int ivlen;			/* length of IV */
	void *sched;			/* intermediate encryption key */
	size_t schedlen;

	struct secreplay *replay;	/* replay prevention */
	long created;			/* for lifetime */

	struct sadb_lifetime *lft_c;	/* CURRENT lifetime, it's constant. */
	struct sadb_lifetime *lft_h;	/* HARD lifetime */
	struct sadb_lifetime *lft_s;	/* SOFT lifetime */

	u_int32_t seq;			/* sequence number */
	pid_t pid;			/* message's pid */

	struct secashead *sah;		/* back pointer to the secashead */
};

/* replay prevention */
struct secreplay {
	u_int32_t count;
	u_int wsize;		/* window size, i.g. 4 bytes */
	u_int32_t seq;		/* used by sender */
	u_int32_t lastseq;	/* used by receiver */
	caddr_t bitmap;		/* used by receiver */
	int overflow;		/* overflow flag */
};

/* socket table due to send PF_KEY messages. */
struct secreg {
	LIST_ENTRY(secreg) chain;

	struct socket *so;
};

#ifndef IPSEC_NONBLOCK_ACQUIRE
/* acquiring list table. */
struct secacq {
	LIST_ENTRY(secacq) chain;

	struct secasindex saidx;

	u_int32_t seq;		/* sequence number */
	long created;		/* for lifetime */
	int count;		/* for lifetime */
};
#endif

/* Sensitivity Level Specification */
/* nothing */

#define SADB_KILL_INTERVAL	600	/* six seconds */

struct key_cb {
	int key_count;
	int any_count;
};

/* secpolicy */
extern struct secpolicy *keydb_newsecpolicy (void);
extern void keydb_delsecpolicy (struct secpolicy *);
/* secashead */
extern struct secashead *keydb_newsecashead (void);
extern void keydb_delsecashead (struct secashead *);
/* secasvar */
extern struct secasvar *keydb_newsecasvar (void);
extern void keydb_refsecasvar (struct secasvar *);
extern void keydb_freesecasvar (struct secasvar *);
/* secreplay */
extern struct secreplay *keydb_newsecreplay (size_t);
extern void keydb_delsecreplay (struct secreplay *);
/* secreg */
extern struct secreg *keydb_newsecreg (void);
extern void keydb_delsecreg (struct secreg *);

#endif /* _KERNEL */

#endif /* _NETKEY_KEYDB_H_ */
