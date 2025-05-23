/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: mdtest.c,v 1.16 2002/05/07 15:49:28 joda Exp $");
#endif

#include <stdio.h>
#include <string.h>
#include <md4.h>
#include <md5.h>
#include <sha.h>

#define ONE_MILLION_A "one million a's"

struct hash_foo {
    const char *name;
    size_t psize;
    size_t hsize;
    void (*init)(void*);
    void (*update)(void*, const void*, size_t);
    void (*final)(void*, void*);
} md4 = {
    "MD4",
    sizeof(MD4_CTX),
    16,
    (void (*)(void*))MD4_Init,
    (void (*)(void*,const void*, size_t))MD4_Update,
    (void (*)(void*, void*))MD4_Final
}, md5 = {
    "MD5",
    sizeof(MD5_CTX),
    16,
    (void (*)(void*))MD5_Init,
    (void (*)(void*,const void*, size_t))MD5_Update,
    (void (*)(void*, void*))MD5_Final
}, sha1 = {
    "SHA-1",
    sizeof(struct sha),
    20,
    (void (*)(void*))SHA1_Init,
    (void (*)(void*,const void*, size_t))SHA1_Update,
    (void (*)(void*, void*))SHA1_Final
};
#ifdef HAVE_SHA256
struct hash_foo sha256 = {
    "SHA-256",
    sizeof(struct sha256),
    32,
    (void (*)(void*))SHA256_Init,
    (void (*)(void*,const void*, size_t))SHA256_Update,
    (void (*)(void*, void*))SHA256_Final
};
#endif
#ifdef HAVE_SHA384
struct hash_foo sha384 = {
    "SHA-384",
    sizeof(struct sha512),
    48,
    (void (*)(void*))SHA384_Init,
    (void (*)(void*,const void*, size_t))SHA384_Update,
    (void (*)(void*, void*))SHA384_Final
};
#endif
#ifdef HAVE_SHA512
struct hash_foo sha512 = {
    "SHA-512",
    sizeof(struct sha512),
    64,
    (void (*)(void*))SHA512_Init,
    (void (*)(void*,const void*, size_t))SHA512_Update,
    (void (*)(void*, void*))SHA512_Final
};
#endif

struct test {
    char *str;
    unsigned char hash[64];
};

struct test md4_tests[] = {
    {"", 
     {0x31, 0xd6, 0xcf, 0xe0, 0xd1, 0x6a, 0xe9, 0x31, 0xb7, 0x3c, 0x59, 
      0xd7, 0xe0, 0xc0, 0x89, 0xc0}},
    {"a",
     {0xbd, 0xe5, 0x2c, 0xb3, 0x1d, 0xe3, 0x3e, 0x46, 0x24, 0x5e, 0x05,
      0xfb, 0xdb, 0xd6, 0xfb, 0x24}},
    {"abc",
     {0xa4, 0x48, 0x01, 0x7a, 0xaf, 0x21, 0xd8, 0x52, 0x5f, 0xc1, 0x0a, 0xe8, 0x7a, 0xa6, 0x72, 0x9d}},
    {"message digest",
     {0xd9, 0x13, 0x0a, 0x81, 0x64, 0x54, 0x9f, 0xe8, 0x18, 0x87, 0x48, 0x06, 0xe1, 0xc7, 0x01, 0x4b}},
    {"abcdefghijklmnopqrstuvwxyz", {0xd7, 0x9e, 0x1c, 0x30, 0x8a, 0xa5, 0xbb, 0xcd, 0xee, 0xa8, 0xed, 0x63, 0xdf, 0x41, 0x2d, 0xa9, }},
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
     {0x04, 0x3f, 0x85, 0x82, 0xf2, 0x41, 0xdb, 0x35, 0x1c, 0xe6, 0x27, 0xe1, 0x53, 0xe7, 0xf0, 0xe4}},
    {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
     {0xe3, 0x3b, 0x4d, 0xdc, 0x9c, 0x38, 0xf2, 0x19, 0x9c, 0x3e, 0x7b, 0x16, 0x4f, 0xcc, 0x05, 0x36, }},
    {NULL, { 0x0 }}};

struct test md5_tests[] = {
    {"", {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e}}, 
    {"a", {0x0c, 0xc1, 0x75, 0xb9, 0xc0, 0xf1, 0xb6, 0xa8, 0x31, 0xc3, 0x99, 0xe2, 0x69, 0x77, 0x26, 0x61}}, 
    {"abc", {0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0, 0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72}}, 
    {"message digest", {0xf9, 0x6b, 0x69, 0x7d, 0x7c, 0xb7, 0x93, 0x8d, 0x52, 0x5a, 0x2f, 0x31, 0xaa, 0xf1, 0x61, 0xd0}}, 
    {"abcdefghijklmnopqrstuvwxyz", {0xc3, 0xfc, 0xd3, 0xd7, 0x61, 0x92, 0xe4, 0x00, 0x7d, 0xfb, 0x49, 0x6c, 0xca, 0x67, 0xe1, 0x3b}}, 
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", {0xd1, 0x74, 0xab, 0x98, 0xd2, 0x77, 0xd9, 0xf5, 0xa5, 0x61, 0x1c, 0x2c, 0x9f, 0x41, 0x9d, 0x9f}}, 
    {"12345678901234567890123456789012345678901234567890123456789012345678901234567890", {0x57, 0xed, 0xf4, 0xa2, 0x2b, 0xe3, 0xc9, 0x55, 0xac, 0x49, 0xda, 0x2e, 0x21, 0x07, 0xb6, 0x7a}}, 
    {NULL, { 0x0 }}};

struct test sha1_tests[] = {
    { "abc", 
      {0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A,
       0xBA, 0x3E, 0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C,
       0x9C, 0xD0, 0xD8, 0x9D}},
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      {0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E,
       0xBA, 0xAE, 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5,
       0xE5, 0x46, 0x70, 0xF1}},
    { ONE_MILLION_A,
      {0x34, 0xaa, 0x97, 0x3c, 0xd4, 0xc4, 0xda, 0xa4, 
       0xf6, 0x1e, 0xeb, 0x2b, 0xdb, 0xad, 0x27, 0x31, 
       0x65, 0x34, 0x01, 0x6f}},
    { NULL }
};

#ifdef HAVE_SHA256
struct test sha256_tests[] = {
    { "abc", 
      { 0xba, 0x78, 0x16, 0xbf,  0x8f, 0x01, 0xcf, 0xea, 
	0x41, 0x41, 0x40, 0xde,  0x5d, 0xae, 0x22, 0x23, 
	0xb0, 0x03, 0x61, 0xa3,  0x96, 0x17, 0x7a, 0x9c,
	0xb4, 0x10, 0xff, 0x61,  0xf2, 0x00, 0x15, 0xad }},
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      { 0x24, 0x8d, 0x6a, 0x61,  0xd2, 0x06, 0x38, 0xb8,
	0xe5, 0xc0, 0x26, 0x93,  0x0c, 0x3e, 0x60, 0x39,
	0xa3, 0x3c, 0xe4, 0x59,  0x64, 0xff, 0x21, 0x67,
	0xf6, 0xec, 0xed, 0xd4,  0x19, 0xdb, 0x06, 0xc1 }},
    { ONE_MILLION_A,
      {0xcd,0xc7,0x6e,0x5c, 0x99,0x14,0xfb,0x92,
       0x81,0xa1,0xc7,0xe2, 0x84,0xd7,0x3e,0x67,
       0xf1,0x80,0x9a,0x48, 0xa4,0x97,0x20,0x0e,
       0x04,0x6d,0x39,0xcc, 0xc7,0x11,0x2c,0xd0 }},
    { NULL }
};
#endif
#ifdef HAVE_SHA384
struct test sha384_tests[] = {
    { "abc", 
      { 0xcb,0x00,0x75,0x3f,0x45,0xa3,0x5e,0x8b,
	0xb5,0xa0,0x3d,0x69,0x9a,0xc6,0x50,0x07,
	0x27,0x2c,0x32,0xab,0x0e,0xde,0xd1,0x63,
	0x1a,0x8b,0x60,0x5a,0x43,0xff,0x5b,0xed,
	0x80,0x86,0x07,0x2b,0xa1,0xe7,0xcc,0x23, 
	0x58,0xba,0xec,0xa1,0x34,0xc8,0x25,0xa7}},
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
      "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
      { 0x09,0x33,0x0c,0x33,0xf7,0x11,0x47,0xe8,
	0x3d,0x19,0x2f,0xc7,0x82,0xcd,0x1b,0x47,
	0x53,0x11,0x1b,0x17,0x3b,0x3b,0x05,0xd2,
	0x2f,0xa0,0x80,0x86,0xe3,0xb0,0xf7,0x12,
	0xfc,0xc7,0xc7,0x1a,0x55,0x7e,0x2d,0xb9, 
	0x66,0xc3,0xe9,0xfa,0x91,0x74,0x60,0x39}},
    { ONE_MILLION_A,
      { 0x9d,0x0e,0x18,0x09,0x71,0x64,0x74,0xcb, 
	0x08,0x6e,0x83,0x4e,0x31,0x0a,0x4a,0x1c, 
	0xed,0x14,0x9e,0x9c,0x00,0xf2,0x48,0x52, 
	0x79,0x72,0xce,0xc5,0x70,0x4c,0x2a,0x5b,
	0x07,0xb8,0xb3,0xdc,0x38,0xec,0xc4,0xeb,
	0xae,0x97,0xdd,0xd8,0x7f,0x3d,0x89,0x85}},
    {NULL} 
};
#endif
#ifdef HAVE_SHA512
struct test sha512_tests[] = {
    { "abc", 
      { 0xdd,0xaf,0x35,0xa1,0x93,0x61,0x7a,0xba,
	0xcc,0x41,0x73,0x49,0xae,0x20,0x41,0x31,
	0x12,0xe6,0xfa,0x4e,0x89,0xa9,0x7e,0xa2,
	0x0a,0x9e,0xee,0xe6,0x4b,0x55,0xd3,0x9a,
	0x21,0x92,0x99,0x2a,0x27,0x4f,0xc1,0xa8, 
	0x36,0xba,0x3c,0x23,0xa3,0xfe,0xeb,0xbd, 
	0x45,0x4d,0x44,0x23,0x64,0x3c,0xe8,0x0e, 
	0x2a,0x9a,0xc9,0x4f,0xa5,0x4c,0xa4,0x9f }},
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
      "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
      { 0x8e,0x95,0x9b,0x75,0xda,0xe3,0x13,0xda,
	0x8c,0xf4,0xf7,0x28,0x14,0xfc,0x14,0x3f,
	0x8f,0x77,0x79,0xc6,0xeb,0x9f,0x7f,0xa1,
	0x72,0x99,0xae,0xad,0xb6,0x88,0x90,0x18,
	0x50,0x1d,0x28,0x9e,0x49,0x00,0xf7,0xe4, 
	0x33,0x1b,0x99,0xde,0xc4,0xb5,0x43,0x3a, 
	0xc7,0xd3,0x29,0xee,0xb6,0xdd,0x26,0x54, 
	0x5e,0x96,0xe5,0x5b,0x87,0x4b,0xe9,0x09 }},
    { ONE_MILLION_A,
      { 0xe7,0x18,0x48,0x3d,0x0c,0xe7,0x69,0x64,
	0x4e,0x2e,0x42,0xc7,0xbc,0x15,0xb4,0x63, 
	0x8e,0x1f,0x98,0xb1,0x3b,0x20,0x44,0x28, 
	0x56,0x32,0xa8,0x03,0xaf,0xa9,0x73,0xeb,
	0xde,0x0f,0xf2,0x44,0x87,0x7e,0xa6,0x0a,
	0x4c,0xb0,0x43,0x2c,0xe5,0x77,0xc3,0x1b, 
	0xeb,0x00,0x9c,0x5c,0x2c,0x49,0xaa,0x2e, 
	0x4e,0xad,0xb2,0x17,0xad,0x8c,0xc0,0x9b }},
    { NULL }
};
#endif

static int
hash_test (struct hash_foo *hash, struct test *tests)
{
    struct test *t;
    void *ctx = malloc(hash->psize);
    unsigned char *res = malloc(hash->hsize);

    printf ("%s... ", hash->name);
    for (t = tests; t->str; ++t) {
	char buf[1000];

	(*hash->init)(ctx);
	if(strcmp(t->str, ONE_MILLION_A) == 0) {
	    int i;
	    memset(buf, 'a', sizeof(buf));
	    for(i = 0; i < 1000; i++)
		(*hash->update)(ctx, buf, sizeof(buf));
	} else
	    (*hash->update)(ctx, (unsigned char *)t->str, strlen(t->str));
	(*hash->final) (res, ctx);
	if (memcmp (res, t->hash, hash->hsize) != 0) {
	    int i;

	    printf ("%s(\"%s\") failed\n", hash->name, t->str);
	    printf("should be:  ");
	    for(i = 0; i < hash->hsize; ++i) {
		if(i > 0 && (i % 16) == 0)
		    printf("\n            ");
		printf("%02x ", t->hash[i]);
	    }
	    printf("\nresult was: ");
	    for(i = 0; i < hash->hsize; ++i) {
		if(i > 0 && (i % 16) == 0)
		    printf("\n            ");
		printf("%02x ", res[i]);
	    }
	    printf("\n");
	    return 1;
	}
    }
    printf ("success\n");
    return 0;
}

int
main (void)
{
    return hash_test(&md4, md4_tests) +
	hash_test(&md5, md5_tests) +
	hash_test(&sha1, sha1_tests)
#ifdef HAVE_SHA256
	+ hash_test(&sha256, sha256_tests)
#endif
#ifdef HAVE_SHA384
	+ hash_test(&sha384, sha384_tests)
#endif
#ifdef HAVE_SHA512
	+ hash_test(&sha512, sha512_tests)
#endif
	;
}
