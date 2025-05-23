/*	$NetBSD: bf_enc.S,v 1.1 2001/09/09 11:01:01 tls Exp $	*/
/*	$FreeBSD: src/sys/crypto/blowfish/arch/i386/bf_enc.S,v 1.1.2.1 2002/03/26 10:12:24 ume Exp $	*/
/*	$DragonFly: src/sys/crypto/blowfish/arch/i386/bf_enc.S,v 1.2 2003/06/17 04:28:20 dillon Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@zembu.com> and Thor Lancelot Simon
 * <tls@netbsd.org>.  Public domain.
 */

/*
 * XXX Should use CPP symbols defined as a result of
 * XXX `cc -mcpu=pentiumpro'.
 */
#if defined(I386_CPU) || defined(I486_CPU) || defined(I586_CPU)
#include "bf_enc_586.S"
#else
#include "bf_enc_686.S"
#endif
