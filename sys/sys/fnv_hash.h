/*
 * Fowler / Noll / Vo Hash (FNV Hash)
 * http://www.isthe.com/chongo/tech/comp/fnv/
 *
 * This is an implementation of the algorithms posted above.
 * This file is placed in the public domain by Peter Wemm.
 *
 * $FreeBSD: src/sys/sys/fnv_hash.h,v 1.2.2.1 2001/03/21 10:50:59 peter Exp $
 * $DragonFly: src/sys/sys/fnv_hash.h,v 1.2 2003/06/17 04:28:58 dillon Exp $
 */

typedef u_int32_t Fnv32_t;
typedef u_int64_t Fnv64_t;

#define FNV1_32_INIT ((Fnv32_t) 33554467UL)
#define FNV1_64_INIT ((Fnv64_t) 0xcbf29ce484222325ULL)

#define FNV_32_PRIME ((Fnv32_t) 0x01000193UL)
#define FNV_64_PRIME ((Fnv64_t) 0x100000001b3ULL)

static __inline Fnv32_t
fnv_32_buf(const void *buf, size_t len, Fnv32_t hval)
{
	const u_int8_t *s = (const u_int8_t *)buf;

	while (len-- != 0) {
		hval *= FNV_32_PRIME;
		hval ^= *s++;
	}
	return hval;
}

static __inline Fnv32_t
fnv_32_str(const char *str, Fnv32_t hval)
{
	const u_int8_t *s = (const u_int8_t *)str;
	Fnv32_t c;

	while ((c = *s++) != 0) {
		hval *= FNV_32_PRIME;
		hval ^= c;
	}
	return hval;
}

static __inline Fnv64_t
fnv_64_buf(const void *buf, size_t len, Fnv64_t hval)
{
	const u_int8_t *s = (const u_int8_t *)buf;

	while (len-- != 0) {
		hval *= FNV_64_PRIME;
		hval ^= *s++;
	}
	return hval;
}

static __inline Fnv64_t
fnv_64_str(const char *str, Fnv64_t hval)
{
	const u_int8_t *s = (const u_int8_t *)str;
	u_register_t c;		 /* 32 bit on i386, 64 bit on alpha,ia64 */

	while ((c = *s++) != 0) {
		hval *= FNV_64_PRIME;
		hval ^= c;
	}
	return hval;
}
