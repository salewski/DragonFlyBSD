/*
 * This function implements the "bigcrypt" algorithm specifically for
 * Linux-PAM.
 *  
 * This algorithm is algorithm 0 (default) shipped with the C2 secure
 * implementation of Digital UNIX.
 * 
 * Disclaimer: This work is not based on the source code to Digital
 * UNIX, nor am I connected to Digital Equipment Corp, in any way
 * other than as a customer. This code is based on published
 * interfaces and reasonable guesswork.
 * 
 * Description: The cleartext is divided into blocks of SEGMENT_SIZE=8
 * characters or less. Each block is encrypted using the standard UNIX
 * libc crypt function. The result of the encryption for one block
 * provides the salt for the suceeding block.
 * 
 * Restrictions: The buffer used to hold the encrypted result is
 * statically allocated. (see MAX_PASS_LEN below).  This is necessary,
 * as the returned pointer points to "static data that are overwritten
 * by each call", (XPG3: XSI System Interface + Headers pg 109), and
 * this is a drop in replacement for crypt();
 *
 * Andy Phillips <atp@mssl.ucl.ac.uk>
 * $FreeBSD: src/contrib/libpam/modules/pam_unix/bigcrypt.c,v 1.1.1.1.2.2 2001/06/11 15:28:30 markm Exp $
 * $DragonFly: src/contrib/libpam/modules/pam_unix/Attic/bigcrypt.c,v 1.2 2003/06/17 04:24:03 dillon Exp $
 */

#include <string.h>
#include <security/_pam_macros.h>

char *crypt(const char *key, const char *salt);
char *bigcrypt(const char *key, const char *salt);

/*
 * Max cleartext password length in segments of 8 characters this
 * function can deal with (16 segments of 8 chars= max 128 character
 * password).
 */

#define MAX_PASS_LEN       16
#define SEGMENT_SIZE       8
#define SALT_SIZE          2
#define KEYBUF_SIZE        ((MAX_PASS_LEN*SEGMENT_SIZE)+SALT_SIZE)
#define ESEGMENT_SIZE      11
#define CBUF_SIZE          ((MAX_PASS_LEN*ESEGMENT_SIZE)+SALT_SIZE+1)

char *bigcrypt(const char *key, const char *salt)
{
	static char dec_c2_cryptbuf[CBUF_SIZE];		/* static storage area */

	unsigned long int keylen, n_seg, j;
	char *cipher_ptr, *plaintext_ptr, *tmp_ptr, *salt_ptr;
	char keybuf[KEYBUF_SIZE + 1];

	D(("called with key='%s', salt='%s'.", key, salt));

	/* reset arrays */
	memset(keybuf, 0, KEYBUF_SIZE + 1);
	memset(dec_c2_cryptbuf, 0, CBUF_SIZE);

	/* fill KEYBUF_SIZE with key */
	strncpy(keybuf, key, KEYBUF_SIZE);

	/* deal with case that we are doing a password check for a
	   conventially encrypted password: the salt will be
	   SALT_SIZE+ESEGMENT_SIZE long. */
	if (strlen(salt) == (SALT_SIZE + ESEGMENT_SIZE))
		keybuf[SEGMENT_SIZE] = '\0';	/* terminate password early(?) */

	keylen = strlen(keybuf);

	if (!keylen) {
		n_seg = 1;
	} else {
		/* work out how many segments */
		n_seg = 1 + ((keylen - 1) / SEGMENT_SIZE);
	}

	if (n_seg > MAX_PASS_LEN)
		n_seg = MAX_PASS_LEN;	/* truncate at max length */

	/* set up some pointers */
	cipher_ptr = dec_c2_cryptbuf;
	plaintext_ptr = keybuf;

	/* do the first block with supplied salt */
	tmp_ptr = crypt(plaintext_ptr, salt);	/* libc crypt() */

	/* and place in the static area */
	strncpy(cipher_ptr, tmp_ptr, 13);
	cipher_ptr += ESEGMENT_SIZE + SALT_SIZE;
	plaintext_ptr += SEGMENT_SIZE;	/* first block of SEGMENT_SIZE */

	/* change the salt (1st 2 chars of previous block) - this was found
	   by dowsing */

	salt_ptr = cipher_ptr - ESEGMENT_SIZE;

	/* so far this is identical to "return crypt(key, salt);", if
	   there is more than one block encrypt them... */

	if (n_seg > 1) {
		for (j = 2; j <= n_seg; j++) {

			tmp_ptr = crypt(plaintext_ptr, salt_ptr);

			/* skip the salt for seg!=0 */
			strncpy(cipher_ptr, (tmp_ptr + SALT_SIZE), ESEGMENT_SIZE);

			cipher_ptr += ESEGMENT_SIZE;
			plaintext_ptr += SEGMENT_SIZE;
			salt_ptr = cipher_ptr - ESEGMENT_SIZE;
		}
	}
	D(("key=|%s|, salt=|%s|\nbuf=|%s|\n", key, salt, dec_c2_cryptbuf));

	/* this is the <NUL> terminated encrypted password */

	return dec_c2_cryptbuf;
}
