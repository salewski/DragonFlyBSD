/*	$FreeBSD: src/sys/opencrypto/rijndael.h,v 1.1.2.1 2002/11/21 23:34:23 sam Exp $	*/
/*	$DragonFly: src/sys/opencrypto/rijndael.h,v 1.2 2003/06/17 04:28:54 dillon Exp $	*/
/*	$OpenBSD: rijndael.h,v 1.7 2001/12/19 17:42:24 markus Exp $ */

/**
 * rijndael-alg-fst.h
 *
 * @version 3.0 (December 2000)
 *
 * Optimised ANSI C code for the Rijndael cipher (now AES)
 *
 * @author Vincent Rijmen <vincent.rijmen@esat.kuleuven.ac.be>
 * @author Antoon Bosselaers <antoon.bosselaers@esat.kuleuven.ac.be>
 * @author Paulo Barreto <paulo.barreto@terra.com.br>
 *
 * This code is hereby placed in the public domain.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __RIJNDAEL_H
#define __RIJNDAEL_H

#define MAXKC	(256/32)
#define MAXKB	(256/8)
#define MAXNR	14

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;

/*  The structure for key information */
typedef struct {
	int	decrypt;
	int	Nr;			/* key-length-dependent number of rounds */
	u32	ek[4*(MAXNR + 1)];	/* encrypt key schedule */
	u32	dk[4*(MAXNR + 1)];	/* decrypt key schedule */
} rijndael_ctx;

void	 rijndael_set_key(rijndael_ctx *, u_char *, int, int);
void	 rijndael_decrypt(rijndael_ctx *, u_char *, u_char *);
void	 rijndael_encrypt(rijndael_ctx *, u_char *, u_char *);

#endif /* __RIJNDAEL_H */
