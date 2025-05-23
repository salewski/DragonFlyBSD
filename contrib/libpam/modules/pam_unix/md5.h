/* $FreeBSD: src/contrib/libpam/modules/pam_unix/md5.h,v 1.1.1.1.2.2 2001/06/11 15:28:30 markm Exp $ */
/* $DragonFly: src/contrib/libpam/modules/pam_unix/Attic/md5.h,v 1.2 2003/06/17 04:24:03 dillon Exp $ */

#ifndef MD5_H
#define MD5_H

typedef unsigned int uint32;

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

void GoodMD5Init(struct MD5Context *);
void GoodMD5Update(struct MD5Context *, unsigned const char *, unsigned);
void GoodMD5Final(unsigned char digest[16], struct MD5Context *);
void GoodMD5Transform(uint32 buf[4], uint32 const in[16]);
void BrokenMD5Init(struct MD5Context *);
void BrokenMD5Update(struct MD5Context *, unsigned const char *, unsigned);
void BrokenMD5Final(unsigned char digest[16], struct MD5Context *);
void BrokenMD5Transform(uint32 buf[4], uint32 const in[16]);

char *Goodcrypt_md5(const char *pw, const char *salt);
char *Brokencrypt_md5(const char *pw, const char *salt);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */

typedef struct MD5Context MD5_CTX;

#endif				/* MD5_H */
