/* apps/speed.c -*- mode:C; c-file-style: "eay" -*- */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

/* most of this code has been pilfered from my libdes speed.c program */

/* $DragonFly: src/secure/usr.bin/openssl/apps/Attic/speed.c,v 1.3 2004/02/08 10:54:25 rob Exp $
*/
#ifndef OPENSSL_NO_SPEED

#undef SECONDS
#define SECONDS		3	
#define RSA_SECONDS	10
#define DSA_SECONDS	10

/* 11-Sep-92 Andrew Daviel   Support for Silicon Graphics IRIX added */
/* 06-Apr-92 Luke Brennan    Support for VMS and add extra signal calls */

#undef PROG
#define PROG speed_main

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include "apps.h"
#ifdef OPENSSL_NO_STDIO
#define APPS_WIN16
#endif
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#if !defined(OPENSSL_SYS_MSDOS)
#include OPENSSL_UNISTD
#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(OPENSSL_SYS_MACOSX) || defined (__DragonFly__)
# define USE_TOD
#elif !defined(OPENSSL_SYS_MSDOS) && !defined(OPENSSL_SYS_VXWORKS) && (!defined(OPENSSL_SYS_VMS) || defined(__DECC))
# define TIMES
#endif
#if !defined(_UNICOS) && !defined(__OpenBSD__) && !defined(sgi) && !defined(__FreeBSD__) && !(defined(__bsdi) || defined(__bsdi__)) && !defined(_AIX) && !defined(OPENSSL_SYS_MPE) && !defined(__NetBSD__) && !defined(OPENSSL_SYS_VXWORKS) && !defined (__DragonFly__) /* FIXME */
# define TIMEB
#endif

#ifndef _IRIX
# include <time.h>
#endif
#ifdef TIMES
# include <sys/types.h>
# include <sys/times.h>
#endif
#ifdef USE_TOD
# include <sys/time.h>
# include <sys/resource.h>
#endif

/* Depending on the VMS version, the tms structure is perhaps defined.
   The __TMS macro will show if it was.  If it wasn't defined, we should
   undefine TIMES, since that tells the rest of the program how things
   should be handled.				-- Richard Levitte */
#if defined(OPENSSL_SYS_VMS_DECC) && !defined(__TMS)
#undef TIMES
#endif

#ifdef TIMEB
#include <sys/timeb.h>
#endif

#if !defined(TIMES) && !defined(TIMEB) && !defined(USE_TOD) && !defined(OPENSSL_SYS_VXWORKS)
#error "It seems neither struct tms nor struct timeb is supported in this platform!"
#endif

#if defined(sun) || defined(__ultrix)
#define _POSIX_SOURCE
#include <limits.h>
#include <sys/param.h>
#endif

#ifndef OPENSSL_NO_DES
#include <openssl/des.h>
#endif
#ifndef OPENSSL_NO_AES
#include <openssl/aes.h>
#endif
#ifndef OPENSSL_NO_MD2
#include <openssl/md2.h>
#endif
#ifndef OPENSSL_NO_MDC2
#include <openssl/mdc2.h>
#endif
#ifndef OPENSSL_NO_MD4
#include <openssl/md4.h>
#endif
#ifndef OPENSSL_NO_MD5
#include <openssl/md5.h>
#endif
#ifndef OPENSSL_NO_HMAC
#include <openssl/hmac.h>
#endif
#include <openssl/evp.h>
#ifndef OPENSSL_NO_SHA
#include <openssl/sha.h>
#endif
#ifndef OPENSSL_NO_RIPEMD
#include <openssl/ripemd.h>
#endif
#ifndef OPENSSL_NO_RC4
#include <openssl/rc4.h>
#endif
#ifndef OPENSSL_NO_RC5
#include <openssl/rc5.h>
#endif
#ifndef OPENSSL_NO_RC2
#include <openssl/rc2.h>
#endif
#ifndef OPENSSL_NO_IDEA
#include <openssl/idea.h>
#endif
#ifndef OPENSSL_NO_BF
#include <openssl/blowfish.h>
#endif
#ifndef OPENSSL_NO_CAST
#include <openssl/cast.h>
#endif
#ifndef OPENSSL_NO_RSA
#include <openssl/rsa.h>
#include "./testrsa.h"
#endif
#include <openssl/x509.h>
#ifndef OPENSSL_NO_DSA
#include "./testdsa.h"
#endif

/* The following if from times(3) man page.  It may need to be changed */
#ifndef HZ
# if defined(_SC_CLK_TCK) \
     && (!defined(OPENSSL_SYS_VMS) || __CTRL_VER >= 70000000)
#  define HZ ((double)sysconf(_SC_CLK_TCK))
# else
#  ifndef CLK_TCK
#   ifndef _BSD_CLK_TCK_ /* FreeBSD hack */
#    define HZ	100.0
#   else /* _BSD_CLK_TCK_ */
#    define HZ ((double)_BSD_CLK_TCK_)
#   endif
#  else /* CLK_TCK */
#   define HZ ((double)CLK_TCK)
#  endif
# endif
#endif

#if !defined(OPENSSL_SYS_VMS) && !defined(OPENSSL_SYS_WINDOWS) && !defined(OPENSSL_SYS_MACINTOSH_CLASSIC) && !defined(OPENSSL_SYS_OS2)
# define HAVE_FORK 1
#endif

#undef BUFSIZE
#define BUFSIZE	((long)1024*8+1)
int run=0;

static char ftime_used = 0, times_used = 0, gettimeofday_used = 1, getrusage_used = 0;
static int mr=0;
static int usertime=1;

static double Time_F(int s);
static void print_message(const char *s,long num,int length);
static void pkey_print_message(char *str,char *str2,long num,int bits,int sec);
static void print_result(int alg,int run_no,int count,double time_used);
#ifdef HAVE_FORK
static int do_multi(int multi);
#endif

#define ALGOR_NUM	19
#define SIZE_NUM	5
#define RSA_NUM		4
#define DSA_NUM		3
static const char *names[ALGOR_NUM]={
  "md2","mdc2","md4","md5","hmac(md5)","sha1","rmd160","rc4",
  "des cbc","des ede3","idea cbc",
  "rc2 cbc","rc5-32/12 cbc","blowfish cbc","cast cbc",
  "aes-128 cbc","aes-192 cbc","aes-256 cbc"};
static double results[ALGOR_NUM][SIZE_NUM];
static int lengths[SIZE_NUM]={16,64,256,1024,8*1024};
static double rsa_results[RSA_NUM][2];
static double dsa_results[DSA_NUM][2];

#ifdef SIGALRM
#if defined(__STDC__) || defined(sgi) || defined(_AIX)
#define SIGRETTYPE void
#else
#define SIGRETTYPE int
#endif 

static SIGRETTYPE sig_done(int sig);
static SIGRETTYPE sig_done(int sig)
	{
	signal(SIGALRM,sig_done);
	run=0;
#ifdef LINT
	sig=sig;
#endif
	}
#endif

#define START	0
#define STOP	1

static double Time_F(int s)
	{
	double ret;

#ifdef USE_TOD
	if(usertime)
	    {
		static struct rusage tstart,tend;

		getrusage_used = 1;
		if (s == START)
			{
			getrusage(RUSAGE_SELF,&tstart);
			return(0);
			}
		else
			{
			long i;

			getrusage(RUSAGE_SELF,&tend);
			i=(long)tend.ru_utime.tv_usec-(long)tstart.ru_utime.tv_usec;
			ret=((double)(tend.ru_utime.tv_sec-tstart.ru_utime.tv_sec))
			  +((double)i)/1000000.0;
			return((ret < 0.001)?0.001:ret);
			}
		}
	else
		{
		static struct timeval tstart,tend;
		long i;

		gettimeofday_used = 1;
		if (s == START)
			{
			gettimeofday(&tstart,NULL);
			return(0);
			}
		else
			{
			gettimeofday(&tend,NULL);
			i=(long)tend.tv_usec-(long)tstart.tv_usec;
			ret=((double)(tend.tv_sec-tstart.tv_sec))+((double)i)/1000000.0;
			return((ret < 0.001)?0.001:ret);
			}
		}
#else  /* ndef USE_TOD */
		
# ifdef TIMES
	if (usertime)
		{
		static struct tms tstart,tend;

		times_used = 1;
		if (s == START)
			{
			times(&tstart);
			return(0);
			}
		else
			{
			times(&tend);
			ret=((double)(tend.tms_utime-tstart.tms_utime))/HZ;
			return((ret < 1e-3)?1e-3:ret);
			}
		}
# endif /* times() */
# if defined(TIMES) && defined(TIMEB)
	else
# endif
# ifdef OPENSSL_SYS_VXWORKS
                {
		static unsigned long tick_start, tick_end;

		if( s == START )
			{
			tick_start = tickGet();
			return 0;
			}
		else
			{
			tick_end = tickGet();
			ret = (double)(tick_end - tick_start) / (double)sysClkRateGet();
			return((ret < 0.001)?0.001:ret);
			}
                }
# elif defined(TIMEB)
		{
		static struct timeb tstart,tend;
		long i;

		ftime_used = 0;
		if (s == START)
			{
			ftime(&tstart);
			return(0);
			}
		else
			{
			ftime(&tend);
			i=(long)tend.millitm-(long)tstart.millitm;
			ret=((double)(tend.time-tstart.time))+((double)i)/1000.0;
			return((ret < 0.001)?0.001:ret);
			}
		}
# endif
#endif
	}

int MAIN(int, char **);

int MAIN(int argc, char **argv)
	{
#ifndef OPENSSL_NO_ENGINE
	ENGINE *e = NULL;
#endif
	unsigned char *buf=NULL,*buf2=NULL;
	int mret=1;
	long count=0,save_count=0;
	int i,j,k;
#if !defined(OPENSSL_NO_RSA) || !defined(OPENSSL_NO_DSA)
	long rsa_count;
#endif
#ifndef OPENSSL_NO_RSA
	unsigned rsa_num;
#endif
	unsigned char md[EVP_MAX_MD_SIZE];
#ifndef OPENSSL_NO_MD2
	unsigned char md2[MD2_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_MDC2
	unsigned char mdc2[MDC2_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_MD4
	unsigned char md4[MD4_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_MD5
	unsigned char md5[MD5_DIGEST_LENGTH];
	unsigned char hmac[MD5_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_SHA
	unsigned char sha[SHA_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_RIPEMD
	unsigned char rmd160[RIPEMD160_DIGEST_LENGTH];
#endif
#ifndef OPENSSL_NO_RC4
	RC4_KEY rc4_ks;
#endif
#ifndef OPENSSL_NO_RC5
	RC5_32_KEY rc5_ks;
#endif
#ifndef OPENSSL_NO_RC2
	RC2_KEY rc2_ks;
#endif
#ifndef OPENSSL_NO_IDEA
	IDEA_KEY_SCHEDULE idea_ks;
#endif
#ifndef OPENSSL_NO_BF
	BF_KEY bf_ks;
#endif
#ifndef OPENSSL_NO_CAST
	CAST_KEY cast_ks;
#endif
	static const unsigned char key16[16]=
		{0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
		 0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12};
	static const unsigned char key24[24]=
		{0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
		 0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,
		 0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,0x34};
	static const unsigned char key32[32]=
		{0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,
		 0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,
		 0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,0x34,
		 0x78,0x9a,0xbc,0xde,0xf0,0x12,0x34,0x56};
#ifndef OPENSSL_NO_AES
#define MAX_BLOCK_SIZE 128
#else
#define MAX_BLOCK_SIZE 64
#endif
	unsigned char DES_iv[8];
	unsigned char iv[MAX_BLOCK_SIZE/8];
#ifndef OPENSSL_NO_DES
	DES_cblock *buf_as_des_cblock = NULL;
	static DES_cblock key ={0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0};
	static DES_cblock key2={0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12};
	static DES_cblock key3={0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,0x34};
	DES_key_schedule sch;
	DES_key_schedule sch2;
	DES_key_schedule sch3;
#endif
#ifndef OPENSSL_NO_AES
	AES_KEY aes_ks1, aes_ks2, aes_ks3;
#endif
#define	D_MD2		0
#define	D_MDC2		1
#define	D_MD4		2
#define	D_MD5		3
#define	D_HMAC		4
#define	D_SHA1		5
#define D_RMD160	6
#define	D_RC4		7
#define	D_CBC_DES	8
#define	D_EDE3_DES	9
#define	D_CBC_IDEA	10
#define	D_CBC_RC2	11
#define	D_CBC_RC5	12
#define	D_CBC_BF	13
#define	D_CBC_CAST	14
#define D_CBC_128_AES	15
#define D_CBC_192_AES	16
#define D_CBC_256_AES	17
#define D_EVP		18
	double d=0.0;
	long c[ALGOR_NUM][SIZE_NUM];
#define	R_DSA_512	0
#define	R_DSA_1024	1
#define	R_DSA_2048	2
#define	R_RSA_512	0
#define	R_RSA_1024	1
#define	R_RSA_2048	2
#define	R_RSA_4096	3
#ifndef OPENSSL_NO_RSA
	RSA *rsa_key[RSA_NUM];
	long rsa_c[RSA_NUM][2];
	static unsigned int rsa_bits[RSA_NUM]={512,1024,2048,4096};
	static unsigned char *rsa_data[RSA_NUM]=
		{test512,test1024,test2048,test4096};
	static int rsa_data_length[RSA_NUM]={
		sizeof(test512),sizeof(test1024),
		sizeof(test2048),sizeof(test4096)};
#endif
#ifndef OPENSSL_NO_DSA
	DSA *dsa_key[DSA_NUM];
	long dsa_c[DSA_NUM][2];
	static unsigned int dsa_bits[DSA_NUM]={512,1024,2048};
#endif
	int rsa_doit[RSA_NUM];
	int dsa_doit[DSA_NUM];
	int doit[ALGOR_NUM];
	int pr_header=0;
	const EVP_CIPHER *evp_cipher=NULL;
	const EVP_MD *evp_md=NULL;
	int decrypt=0;
#ifdef HAVE_FORK
	int multi=0;
#endif

#ifndef TIMES
	usertime=-1;
#endif

	apps_startup();
	memset(results, 0, sizeof(results));
#ifndef OPENSSL_NO_DSA
	memset(dsa_key,0,sizeof(dsa_key));
#endif

	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err,stderr,BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto end;

#ifndef OPENSSL_NO_RSA
	memset(rsa_key,0,sizeof(rsa_key));
	for (i=0; i<RSA_NUM; i++)
		rsa_key[i]=NULL;
#endif

	if ((buf=(unsigned char *)OPENSSL_malloc((int)BUFSIZE)) == NULL)
		{
		BIO_printf(bio_err,"out of memory\n");
		goto end;
		}
#ifndef OPENSSL_NO_DES
	buf_as_des_cblock = (DES_cblock *)buf;
#endif
	if ((buf2=(unsigned char *)OPENSSL_malloc((int)BUFSIZE)) == NULL)
		{
		BIO_printf(bio_err,"out of memory\n");
		goto end;
		}

	memset(c,0,sizeof(c));
	memset(DES_iv,0,sizeof(DES_iv));
	memset(iv,0,sizeof(iv));

	for (i=0; i<ALGOR_NUM; i++)
		doit[i]=0;
	for (i=0; i<RSA_NUM; i++)
		rsa_doit[i]=0;
	for (i=0; i<DSA_NUM; i++)
		dsa_doit[i]=0;
	
	j=0;
	argc--;
	argv++;
	while (argc)
		{
		if	((argc > 0) && (strcmp(*argv,"-elapsed") == 0))
			{
			usertime = 0;
			j--;	/* Otherwise, -elapsed gets confused with
				   an algorithm. */
			}
		else if	((argc > 0) && (strcmp(*argv,"-evp") == 0))
			{
			argc--;
			argv++;
			if(argc == 0)
				{
				BIO_printf(bio_err,"no EVP given\n");
				goto end;
				}
			evp_cipher=EVP_get_cipherbyname(*argv);
			if(!evp_cipher)
				{
				evp_md=EVP_get_digestbyname(*argv);
				}
			if(!evp_cipher && !evp_md)
				{
				BIO_printf(bio_err,"%s is an unknown cipher or digest\n",*argv);
				goto end;
				}
			doit[D_EVP]=1;
			}
		else if (argc > 0 && !strcmp(*argv,"-decrypt"))
			{
			decrypt=1;
			j--;	/* Otherwise, -elapsed gets confused with
				   an algorithm. */
			}
#ifndef OPENSSL_NO_ENGINE
		else if	((argc > 0) && (strcmp(*argv,"-engine") == 0))
			{
			argc--;
			argv++;
			if(argc == 0)
				{
				BIO_printf(bio_err,"no engine given\n");
				goto end;
				}
                        e = setup_engine(bio_err, *argv, 0);
			/* j will be increased again further down.  We just
			   don't want speed to confuse an engine with an
			   algorithm, especially when none is given (which
			   means all of them should be run) */
			j--;
			}
#endif
#ifdef HAVE_FORK
		else if	((argc > 0) && (strcmp(*argv,"-multi") == 0))
			{
			argc--;
			argv++;
			if(argc == 0)
				{
				BIO_printf(bio_err,"no multi count given\n");
				goto end;
				}
			multi=atoi(argv[0]);
			if(multi <= 0)
			    {
				BIO_printf(bio_err,"bad multi count\n");
				goto end;
				}				
			j--;	/* Otherwise, -mr gets confused with
				   an algorithm. */
			}
#endif
		else if (argc > 0 && !strcmp(*argv,"-mr"))
			{
			mr=1;
			j--;	/* Otherwise, -mr gets confused with
				   an algorithm. */
			}
		else
#ifndef OPENSSL_NO_MD2
		if	(strcmp(*argv,"md2") == 0) doit[D_MD2]=1;
		else
#endif
#ifndef OPENSSL_NO_MDC2
			if (strcmp(*argv,"mdc2") == 0) doit[D_MDC2]=1;
		else
#endif
#ifndef OPENSSL_NO_MD4
			if (strcmp(*argv,"md4") == 0) doit[D_MD4]=1;
		else
#endif
#ifndef OPENSSL_NO_MD5
			if (strcmp(*argv,"md5") == 0) doit[D_MD5]=1;
		else
#endif
#ifndef OPENSSL_NO_MD5
			if (strcmp(*argv,"hmac") == 0) doit[D_HMAC]=1;
		else
#endif
#ifndef OPENSSL_NO_SHA
			if (strcmp(*argv,"sha1") == 0) doit[D_SHA1]=1;
		else
			if (strcmp(*argv,"sha") == 0) doit[D_SHA1]=1;
		else
#endif
#ifndef OPENSSL_NO_RIPEMD
			if (strcmp(*argv,"ripemd") == 0) doit[D_RMD160]=1;
		else
			if (strcmp(*argv,"rmd160") == 0) doit[D_RMD160]=1;
		else
			if (strcmp(*argv,"ripemd160") == 0) doit[D_RMD160]=1;
		else
#endif
#ifndef OPENSSL_NO_RC4
			if (strcmp(*argv,"rc4") == 0) doit[D_RC4]=1;
		else 
#endif
#ifndef OPENSSL_NO_DES
			if (strcmp(*argv,"des-cbc") == 0) doit[D_CBC_DES]=1;
		else	if (strcmp(*argv,"des-ede3") == 0) doit[D_EDE3_DES]=1;
		else
#endif
#ifndef OPENSSL_NO_AES
			if (strcmp(*argv,"aes-128-cbc") == 0) doit[D_CBC_128_AES]=1;
		else	if (strcmp(*argv,"aes-192-cbc") == 0) doit[D_CBC_192_AES]=1;
		else	if (strcmp(*argv,"aes-256-cbc") == 0) doit[D_CBC_256_AES]=1;
		else
#endif
#ifndef OPENSSL_NO_RSA
#if 0 /* was: #ifdef RSAref */
			if (strcmp(*argv,"rsaref") == 0) 
			{
			RSA_set_default_openssl_method(RSA_PKCS1_RSAref());
			j--;
			}
		else
#endif
#ifndef RSA_NULL
			if (strcmp(*argv,"openssl") == 0) 
			{
			RSA_set_default_method(RSA_PKCS1_SSLeay());
			j--;
			}
		else
#endif
#endif /* !OPENSSL_NO_RSA */
		     if (strcmp(*argv,"dsa512") == 0) dsa_doit[R_DSA_512]=2;
		else if (strcmp(*argv,"dsa1024") == 0) dsa_doit[R_DSA_1024]=2;
		else if (strcmp(*argv,"dsa2048") == 0) dsa_doit[R_DSA_2048]=2;
		else if (strcmp(*argv,"rsa512") == 0) rsa_doit[R_RSA_512]=2;
		else if (strcmp(*argv,"rsa1024") == 0) rsa_doit[R_RSA_1024]=2;
		else if (strcmp(*argv,"rsa2048") == 0) rsa_doit[R_RSA_2048]=2;
		else if (strcmp(*argv,"rsa4096") == 0) rsa_doit[R_RSA_4096]=2;
		else
#ifndef OPENSSL_NO_RC2
		     if (strcmp(*argv,"rc2-cbc") == 0) doit[D_CBC_RC2]=1;
		else if (strcmp(*argv,"rc2") == 0) doit[D_CBC_RC2]=1;
		else
#endif
#ifndef OPENSSL_NO_RC5
		     if (strcmp(*argv,"rc5-cbc") == 0) doit[D_CBC_RC5]=1;
		else if (strcmp(*argv,"rc5") == 0) doit[D_CBC_RC5]=1;
		else
#endif
#ifndef OPENSSL_NO_IDEA
		     if (strcmp(*argv,"idea-cbc") == 0) doit[D_CBC_IDEA]=1;
		else if (strcmp(*argv,"idea") == 0) doit[D_CBC_IDEA]=1;
		else
#endif
#ifndef OPENSSL_NO_BF
		     if (strcmp(*argv,"bf-cbc") == 0) doit[D_CBC_BF]=1;
		else if (strcmp(*argv,"blowfish") == 0) doit[D_CBC_BF]=1;
		else if (strcmp(*argv,"bf") == 0) doit[D_CBC_BF]=1;
		else
#endif
#ifndef OPENSSL_NO_CAST
		     if (strcmp(*argv,"cast-cbc") == 0) doit[D_CBC_CAST]=1;
		else if (strcmp(*argv,"cast") == 0) doit[D_CBC_CAST]=1;
		else if (strcmp(*argv,"cast5") == 0) doit[D_CBC_CAST]=1;
		else
#endif
#ifndef OPENSSL_NO_DES
			if (strcmp(*argv,"des") == 0)
			{
			doit[D_CBC_DES]=1;
			doit[D_EDE3_DES]=1;
			}
		else
#endif
#ifndef OPENSSL_NO_AES
			if (strcmp(*argv,"aes") == 0)
			{
			doit[D_CBC_128_AES]=1;
			doit[D_CBC_192_AES]=1;
			doit[D_CBC_256_AES]=1;
			}
		else
#endif
#ifndef OPENSSL_NO_RSA
			if (strcmp(*argv,"rsa") == 0)
			{
			rsa_doit[R_RSA_512]=1;
			rsa_doit[R_RSA_1024]=1;
			rsa_doit[R_RSA_2048]=1;
			rsa_doit[R_RSA_4096]=1;
			}
		else
#endif
#ifndef OPENSSL_NO_DSA
			if (strcmp(*argv,"dsa") == 0)
			{
			dsa_doit[R_DSA_512]=1;
			dsa_doit[R_DSA_1024]=1;
			}
		else
#endif
			{
			BIO_printf(bio_err,"Error: bad option or value\n");
			BIO_printf(bio_err,"\n");
			BIO_printf(bio_err,"Available values:\n");
#ifndef OPENSSL_NO_MD2
			BIO_printf(bio_err,"md2      ");
#endif
#ifndef OPENSSL_NO_MDC2
			BIO_printf(bio_err,"mdc2     ");
#endif
#ifndef OPENSSL_NO_MD4
			BIO_printf(bio_err,"md4      ");
#endif
#ifndef OPENSSL_NO_MD5
			BIO_printf(bio_err,"md5      ");
#ifndef OPENSSL_NO_HMAC
			BIO_printf(bio_err,"hmac     ");
#endif
#endif
#ifndef OPENSSL_NO_SHA1
			BIO_printf(bio_err,"sha1     ");
#endif
#ifndef OPENSSL_NO_RIPEMD160
			BIO_printf(bio_err,"rmd160");
#endif
#if !defined(OPENSSL_NO_MD2) || !defined(OPENSSL_NO_MDC2) || \
    !defined(OPENSSL_NO_MD4) || !defined(OPENSSL_NO_MD5) || \
    !defined(OPENSSL_NO_SHA1) || !defined(OPENSSL_NO_RIPEMD160)
			BIO_printf(bio_err,"\n");
#endif

#ifndef OPENSSL_NO_IDEA
			BIO_printf(bio_err,"idea-cbc ");
#endif
#ifndef OPENSSL_NO_RC2
			BIO_printf(bio_err,"rc2-cbc  ");
#endif
#ifndef OPENSSL_NO_RC5
			BIO_printf(bio_err,"rc5-cbc  ");
#endif
#ifndef OPENSSL_NO_BF
			BIO_printf(bio_err,"bf-cbc");
#endif
#if !defined(OPENSSL_NO_IDEA) || !defined(OPENSSL_NO_RC2) || \
    !defined(OPENSSL_NO_BF) || !defined(OPENSSL_NO_RC5)
			BIO_printf(bio_err,"\n");
#endif
#ifndef OPENSSL_NO_DES
			BIO_printf(bio_err,"des-cbc  des-ede3 ");
#endif
#ifndef OPENSSL_NO_AES
			BIO_printf(bio_err,"aes-128-cbc aes-192-cbc aes-256-cbc ");
#endif
#ifndef OPENSSL_NO_RC4
			BIO_printf(bio_err,"rc4");
#endif
			BIO_printf(bio_err,"\n");

#ifndef OPENSSL_NO_RSA
			BIO_printf(bio_err,"rsa512   rsa1024  rsa2048  rsa4096\n");
#endif

#ifndef OPENSSL_NO_DSA
			BIO_printf(bio_err,"dsa512   dsa1024  dsa2048\n");
#endif

#ifndef OPENSSL_NO_IDEA
			BIO_printf(bio_err,"idea     ");
#endif
#ifndef OPENSSL_NO_RC2
			BIO_printf(bio_err,"rc2      ");
#endif
#ifndef OPENSSL_NO_DES
			BIO_printf(bio_err,"des      ");
#endif
#ifndef OPENSSL_NO_AES
			BIO_printf(bio_err,"aes      ");
#endif
#ifndef OPENSSL_NO_RSA
			BIO_printf(bio_err,"rsa      ");
#endif
#ifndef OPENSSL_NO_BF
			BIO_printf(bio_err,"blowfish");
#endif
#if !defined(OPENSSL_NO_IDEA) || !defined(OPENSSL_NO_RC2) || \
    !defined(OPENSSL_NO_DES) || !defined(OPENSSL_NO_RSA) || \
    !defined(OPENSSL_NO_BF) || !defined(OPENSSL_NO_AES)
			BIO_printf(bio_err,"\n");
#endif

			BIO_printf(bio_err,"\n");
			BIO_printf(bio_err,"Available options:\n");
#if defined(TIMES) || defined(USE_TOD)
			BIO_printf(bio_err,"-elapsed        measure time in real time instead of CPU user time.\n");
#endif
#ifndef OPENSSL_NO_ENGINE
			BIO_printf(bio_err,"-engine e       use engine e, possibly a hardware device.\n");
#endif
			BIO_printf(bio_err,"-evp e          use EVP e.\n");
			BIO_printf(bio_err,"-decrypt        time decryption instead of encryption (only EVP).\n");
			BIO_printf(bio_err,"-mr             produce machine readable output.\n");
#ifdef HAVE_FORK
			BIO_printf(bio_err,"-multi n        run n benchmarks in parallel.\n");
#endif
			goto end;
			}
		argc--;
		argv++;
		j++;
		}

#ifdef HAVE_FORK
	if(multi && do_multi(multi))
		goto show_res;
#endif

	if (j == 0)
		{
		for (i=0; i<ALGOR_NUM; i++)
			{
			if (i != D_EVP)
				doit[i]=1;
			}
		for (i=0; i<RSA_NUM; i++)
			rsa_doit[i]=1;
		for (i=0; i<DSA_NUM; i++)
			dsa_doit[i]=1;
		}
	for (i=0; i<ALGOR_NUM; i++)
		if (doit[i]) pr_header++;

	if (usertime == 0 && !mr)
		BIO_printf(bio_err,"You have chosen to measure elapsed time instead of user CPU time.\n");
	if (usertime <= 0 && !mr)
		{
		BIO_printf(bio_err,"To get the most accurate results, try to run this\n");
		BIO_printf(bio_err,"program when this computer is idle.\n");
		}

#ifndef OPENSSL_NO_RSA
	for (i=0; i<RSA_NUM; i++)
		{
		const unsigned char *p;

		p=rsa_data[i];
		rsa_key[i]=d2i_RSAPrivateKey(NULL,&p,rsa_data_length[i]);
		if (rsa_key[i] == NULL)
			{
			BIO_printf(bio_err,"internal error loading RSA key number %d\n",i);
			goto end;
			}
#if 0
		else
			{
			BIO_printf(bio_err,mr ? "+RK:%d:"
				   : "Loaded RSA key, %d bit modulus and e= 0x",
				   BN_num_bits(rsa_key[i]->n));
			BN_print(bio_err,rsa_key[i]->e);
			BIO_printf(bio_err,"\n");
			}
#endif
		}
#endif

#ifndef OPENSSL_NO_DSA
	dsa_key[0]=get_dsa512();
	dsa_key[1]=get_dsa1024();
	dsa_key[2]=get_dsa2048();
#endif

#ifndef OPENSSL_NO_DES
	DES_set_key_unchecked(&key,&sch);
	DES_set_key_unchecked(&key2,&sch2);
	DES_set_key_unchecked(&key3,&sch3);
#endif
#ifndef OPENSSL_NO_AES
	AES_set_encrypt_key(key16,128,&aes_ks1);
	AES_set_encrypt_key(key24,192,&aes_ks2);
	AES_set_encrypt_key(key32,256,&aes_ks3);
#endif
#ifndef OPENSSL_NO_IDEA
	idea_set_encrypt_key(key16,&idea_ks);
#endif
#ifndef OPENSSL_NO_RC4
	RC4_set_key(&rc4_ks,16,key16);
#endif
#ifndef OPENSSL_NO_RC2
	RC2_set_key(&rc2_ks,16,key16,128);
#endif
#ifndef OPENSSL_NO_RC5
	RC5_32_set_key(&rc5_ks,16,key16,12);
#endif
#ifndef OPENSSL_NO_BF
	BF_set_key(&bf_ks,16,key16);
#endif
#ifndef OPENSSL_NO_CAST
	CAST_set_key(&cast_ks,16,key16);
#endif
#ifndef OPENSSL_NO_RSA
	memset(rsa_c,0,sizeof(rsa_c));
#endif
#ifndef SIGALRM
#ifndef OPENSSL_NO_DES
	BIO_printf(bio_err,"First we calculate the approximate speed ...\n");
	count=10;
	do	{
		long i;
		count*=2;
		Time_F(START);
		for (i=count; i; i--)
			DES_ecb_encrypt(buf_as_des_cblock,buf_as_des_cblock,
				&sch,DES_ENCRYPT);
		d=Time_F(STOP);
		} while (d <3);
	save_count=count;
	c[D_MD2][0]=count/10;
	c[D_MDC2][0]=count/10;
	c[D_MD4][0]=count;
	c[D_MD5][0]=count;
	c[D_HMAC][0]=count;
	c[D_SHA1][0]=count;
	c[D_RMD160][0]=count;
	c[D_RC4][0]=count*5;
	c[D_CBC_DES][0]=count;
	c[D_EDE3_DES][0]=count/3;
	c[D_CBC_IDEA][0]=count;
	c[D_CBC_RC2][0]=count;
	c[D_CBC_RC5][0]=count;
	c[D_CBC_BF][0]=count;
	c[D_CBC_CAST][0]=count;

	for (i=1; i<SIZE_NUM; i++)
		{
		c[D_MD2][i]=c[D_MD2][0]*4*lengths[0]/lengths[i];
		c[D_MDC2][i]=c[D_MDC2][0]*4*lengths[0]/lengths[i];
		c[D_MD4][i]=c[D_MD4][0]*4*lengths[0]/lengths[i];
		c[D_MD5][i]=c[D_MD5][0]*4*lengths[0]/lengths[i];
		c[D_HMAC][i]=c[D_HMAC][0]*4*lengths[0]/lengths[i];
		c[D_SHA1][i]=c[D_SHA1][0]*4*lengths[0]/lengths[i];
		c[D_RMD160][i]=c[D_RMD160][0]*4*lengths[0]/lengths[i];
		}
	for (i=1; i<SIZE_NUM; i++)
		{
		long l0,l1;

		l0=(long)lengths[i-1];
		l1=(long)lengths[i];
		c[D_RC4][i]=c[D_RC4][i-1]*l0/l1;
		c[D_CBC_DES][i]=c[D_CBC_DES][i-1]*l0/l1;
		c[D_EDE3_DES][i]=c[D_EDE3_DES][i-1]*l0/l1;
		c[D_CBC_IDEA][i]=c[D_CBC_IDEA][i-1]*l0/l1;
		c[D_CBC_RC2][i]=c[D_CBC_RC2][i-1]*l0/l1;
		c[D_CBC_RC5][i]=c[D_CBC_RC5][i-1]*l0/l1;
		c[D_CBC_BF][i]=c[D_CBC_BF][i-1]*l0/l1;
		c[D_CBC_CAST][i]=c[D_CBC_CAST][i-1]*l0/l1;
		}
#ifndef OPENSSL_NO_RSA
	rsa_c[R_RSA_512][0]=count/2000;
	rsa_c[R_RSA_512][1]=count/400;
	for (i=1; i<RSA_NUM; i++)
		{
		rsa_c[i][0]=rsa_c[i-1][0]/8;
		rsa_c[i][1]=rsa_c[i-1][1]/4;
		if ((rsa_doit[i] <= 1) && (rsa_c[i][0] == 0))
			rsa_doit[i]=0;
		else
			{
			if (rsa_c[i][0] == 0)
				{
				rsa_c[i][0]=1;
				rsa_c[i][1]=20;
				}
			}				
		}
#endif

#ifndef OPENSSL_NO_DSA
	dsa_c[R_DSA_512][0]=count/1000;
	dsa_c[R_DSA_512][1]=count/1000/2;
	for (i=1; i<DSA_NUM; i++)
		{
		dsa_c[i][0]=dsa_c[i-1][0]/4;
		dsa_c[i][1]=dsa_c[i-1][1]/4;
		if ((dsa_doit[i] <= 1) && (dsa_c[i][0] == 0))
			dsa_doit[i]=0;
		else
			{
			if (dsa_c[i] == 0)
				{
				dsa_c[i][0]=1;
				dsa_c[i][1]=1;
				}
			}				
		}
#endif

#define COND(d)	(count < (d))
#define COUNT(d) (d)
#else
/* not worth fixing */
# error "You cannot disable DES on systems without SIGALRM."
#endif /* OPENSSL_NO_DES */
#else
#define COND(c)	(run)
#define COUNT(d) (count)
	signal(SIGALRM,sig_done);
#endif /* SIGALRM */

#ifndef OPENSSL_NO_MD2
	if (doit[D_MD2])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_MD2],c[D_MD2][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_MD2][j]); count++)
				EVP_Digest(buf,(unsigned long)lengths[j],&(md2[0]),NULL,EVP_md2(),NULL);
			d=Time_F(STOP);
			print_result(D_MD2,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_MDC2
	if (doit[D_MDC2])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_MDC2],c[D_MDC2][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_MDC2][j]); count++)
				EVP_Digest(buf,(unsigned long)lengths[j],&(mdc2[0]),NULL,EVP_mdc2(),NULL);
			d=Time_F(STOP);
			print_result(D_MDC2,j,count,d);
			}
		}
#endif

#ifndef OPENSSL_NO_MD4
	if (doit[D_MD4])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_MD4],c[D_MD4][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_MD4][j]); count++)
				EVP_Digest(&(buf[0]),(unsigned long)lengths[j],&(md4[0]),NULL,EVP_md4(),NULL);
			d=Time_F(STOP);
			print_result(D_MD4,j,count,d);
			}
		}
#endif

#ifndef OPENSSL_NO_MD5
	if (doit[D_MD5])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_MD5],c[D_MD5][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_MD5][j]); count++)
				EVP_Digest(&(buf[0]),(unsigned long)lengths[j],&(md5[0]),NULL,EVP_get_digestbyname("md5"),NULL);
			d=Time_F(STOP);
			print_result(D_MD5,j,count,d);
			}
		}
#endif

#if !defined(OPENSSL_NO_MD5) && !defined(OPENSSL_NO_HMAC)
	if (doit[D_HMAC])
		{
		HMAC_CTX hctx;

		HMAC_CTX_init(&hctx);
		HMAC_Init_ex(&hctx,(unsigned char *)"This is a key...",
			16,EVP_md5(), NULL);

		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_HMAC],c[D_HMAC][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_HMAC][j]); count++)
				{
				HMAC_Init_ex(&hctx,NULL,0,NULL,NULL);
				HMAC_Update(&hctx,buf,lengths[j]);
				HMAC_Final(&hctx,&(hmac[0]),NULL);
				}
			d=Time_F(STOP);
			print_result(D_HMAC,j,count,d);
			}
		HMAC_CTX_cleanup(&hctx);
		}
#endif
#ifndef OPENSSL_NO_SHA
	if (doit[D_SHA1])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_SHA1],c[D_SHA1][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_SHA1][j]); count++)
				EVP_Digest(buf,(unsigned long)lengths[j],&(sha[0]),NULL,EVP_sha1(),NULL);
			d=Time_F(STOP);
			print_result(D_SHA1,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_RIPEMD
	if (doit[D_RMD160])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_RMD160],c[D_RMD160][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_RMD160][j]); count++)
				EVP_Digest(buf,(unsigned long)lengths[j],&(rmd160[0]),NULL,EVP_ripemd160(),NULL);
			d=Time_F(STOP);
			print_result(D_RMD160,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_RC4
	if (doit[D_RC4])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_RC4],c[D_RC4][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_RC4][j]); count++)
				RC4(&rc4_ks,(unsigned int)lengths[j],
					buf,buf);
			d=Time_F(STOP);
			print_result(D_RC4,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_DES
	if (doit[D_CBC_DES])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_DES],c[D_CBC_DES][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_DES][j]); count++)
				DES_ncbc_encrypt(buf,buf,lengths[j],&sch,
						 &DES_iv,DES_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_DES,j,count,d);
			}
		}

	if (doit[D_EDE3_DES])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_EDE3_DES],c[D_EDE3_DES][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_EDE3_DES][j]); count++)
				DES_ede3_cbc_encrypt(buf,buf,lengths[j],
						     &sch,&sch2,&sch3,
						     &DES_iv,DES_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_EDE3_DES,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_AES
	if (doit[D_CBC_128_AES])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_128_AES],c[D_CBC_128_AES][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_128_AES][j]); count++)
				AES_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&aes_ks1,
					iv,AES_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_128_AES,j,count,d);
			}
		}
	if (doit[D_CBC_192_AES])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_192_AES],c[D_CBC_192_AES][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_192_AES][j]); count++)
				AES_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&aes_ks2,
					iv,AES_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_192_AES,j,count,d);
			}
		}
	if (doit[D_CBC_256_AES])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_256_AES],c[D_CBC_256_AES][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_256_AES][j]); count++)
				AES_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&aes_ks3,
					iv,AES_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_256_AES,j,count,d);
			}
		}

#endif
#ifndef OPENSSL_NO_IDEA
	if (doit[D_CBC_IDEA])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_IDEA],c[D_CBC_IDEA][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_IDEA][j]); count++)
				idea_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&idea_ks,
					iv,IDEA_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_IDEA,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_RC2
	if (doit[D_CBC_RC2])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_RC2],c[D_CBC_RC2][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_RC2][j]); count++)
				RC2_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&rc2_ks,
					iv,RC2_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_RC2,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_RC5
	if (doit[D_CBC_RC5])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_RC5],c[D_CBC_RC5][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_RC5][j]); count++)
				RC5_32_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&rc5_ks,
					iv,RC5_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_RC5,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_BF
	if (doit[D_CBC_BF])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_BF],c[D_CBC_BF][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_BF][j]); count++)
				BF_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&bf_ks,
					iv,BF_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_BF,j,count,d);
			}
		}
#endif
#ifndef OPENSSL_NO_CAST
	if (doit[D_CBC_CAST])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			print_message(names[D_CBC_CAST],c[D_CBC_CAST][j],lengths[j]);
			Time_F(START);
			for (count=0,run=1; COND(c[D_CBC_CAST][j]); count++)
				CAST_cbc_encrypt(buf,buf,
					(unsigned long)lengths[j],&cast_ks,
					iv,CAST_ENCRYPT);
			d=Time_F(STOP);
			print_result(D_CBC_CAST,j,count,d);
			}
		}
#endif

	if (doit[D_EVP])
		{
		for (j=0; j<SIZE_NUM; j++)
			{
			if (evp_cipher)
				{
				EVP_CIPHER_CTX ctx;
				int outl;

				names[D_EVP]=OBJ_nid2ln(evp_cipher->nid);
				/* -O3 -fschedule-insns messes up an
				 * optimization here!  names[D_EVP]
				 * somehow becomes NULL */
				print_message(names[D_EVP],save_count,
					lengths[j]);

				EVP_CIPHER_CTX_init(&ctx);
				if(decrypt)
					EVP_DecryptInit_ex(&ctx,evp_cipher,NULL,key16,iv);
				else
					EVP_EncryptInit_ex(&ctx,evp_cipher,NULL,key16,iv);

				Time_F(START);
				if(decrypt)
					for (count=0,run=1; COND(save_count*4*lengths[0]/lengths[j]); count++)
						EVP_DecryptUpdate(&ctx,buf,&outl,buf,lengths[j]);
				else
					for (count=0,run=1; COND(save_count*4*lengths[0]/lengths[j]); count++)
						EVP_EncryptUpdate(&ctx,buf,&outl,buf,lengths[j]);
				if(decrypt)
					EVP_DecryptFinal_ex(&ctx,buf,&outl);
				else
					EVP_EncryptFinal_ex(&ctx,buf,&outl);
				d=Time_F(STOP);
				EVP_CIPHER_CTX_cleanup(&ctx);
				}
			if (evp_md)
				{
				names[D_EVP]=OBJ_nid2ln(evp_md->type);
				print_message(names[D_EVP],save_count,
					lengths[j]);

				Time_F(START);
				for (count=0,run=1; COND(save_count*4*lengths[0]/lengths[j]); count++)
					EVP_Digest(buf,lengths[j],&(md[0]),NULL,evp_md,NULL);

				d=Time_F(STOP);
				}
			print_result(D_EVP,j,count,d);
			}
		}

	RAND_pseudo_bytes(buf,36);
#ifndef OPENSSL_NO_RSA
	for (j=0; j<RSA_NUM; j++)
		{
		int ret;
		if (!rsa_doit[j]) continue;
		ret=RSA_sign(NID_md5_sha1, buf,36, buf2, &rsa_num, rsa_key[j]);
		if (ret == 0)
			{
			BIO_printf(bio_err,"RSA sign failure.  No RSA sign will be done.\n");
			ERR_print_errors(bio_err);
			rsa_count=1;
			}
		else
			{
			pkey_print_message("private","rsa",
				rsa_c[j][0],rsa_bits[j],
				RSA_SECONDS);
/*			RSA_blinding_on(rsa_key[j],NULL); */
			Time_F(START);
			for (count=0,run=1; COND(rsa_c[j][0]); count++)
				{
				ret=RSA_sign(NID_md5_sha1, buf,36, buf2,
					&rsa_num, rsa_key[j]);
				if (ret == 0)
					{
					BIO_printf(bio_err,
						"RSA sign failure\n");
					ERR_print_errors(bio_err);
					count=1;
					break;
					}
				}
			d=Time_F(STOP);
			BIO_printf(bio_err,mr ? "+R1:%ld:%d:%.2f\n"
				   : "%ld %d bit private RSA's in %.2fs\n",
				   count,rsa_bits[j],d);
			rsa_results[j][0]=d/(double)count;
			rsa_count=count;
			}

#if 1
		ret=RSA_verify(NID_md5_sha1, buf,36, buf2, rsa_num, rsa_key[j]);
		if (ret <= 0)
			{
			BIO_printf(bio_err,"RSA verify failure.  No RSA verify will be done.\n");
			ERR_print_errors(bio_err);
			rsa_doit[j] = 0;
			}
		else
			{
			pkey_print_message("public","rsa",
				rsa_c[j][1],rsa_bits[j],
				RSA_SECONDS);
			Time_F(START);
			for (count=0,run=1; COND(rsa_c[j][1]); count++)
				{
				ret=RSA_verify(NID_md5_sha1, buf,36, buf2,
					rsa_num, rsa_key[j]);
				if (ret == 0)
					{
					BIO_printf(bio_err,
						"RSA verify failure\n");
					ERR_print_errors(bio_err);
					count=1;
					break;
					}
				}
			d=Time_F(STOP);
			BIO_printf(bio_err,mr ? "+R2:%ld:%d:%.2f\n"
				   : "%ld %d bit public RSA's in %.2fs\n",
				   count,rsa_bits[j],d);
			rsa_results[j][1]=d/(double)count;
			}
#endif

		if (rsa_count <= 1)
			{
			/* if longer than 10s, don't do any more */
			for (j++; j<RSA_NUM; j++)
				rsa_doit[j]=0;
			}
		}
#endif

	RAND_pseudo_bytes(buf,20);
#ifndef OPENSSL_NO_DSA
	if (RAND_status() != 1)
		{
		RAND_seed(rnd_seed, sizeof rnd_seed);
		rnd_fake = 1;
		}
	for (j=0; j<DSA_NUM; j++)
		{
		unsigned int kk;
		int ret;

		if (!dsa_doit[j]) continue;
/*		DSA_generate_key(dsa_key[j]); */
/*		DSA_sign_setup(dsa_key[j],NULL); */
		ret=DSA_sign(EVP_PKEY_DSA,buf,20,buf2,
			&kk,dsa_key[j]);
		if (ret == 0)
			{
			BIO_printf(bio_err,"DSA sign failure.  No DSA sign will be done.\n");
			ERR_print_errors(bio_err);
			rsa_count=1;
			}
		else
			{
			pkey_print_message("sign","dsa",
				dsa_c[j][0],dsa_bits[j],
				DSA_SECONDS);
			Time_F(START);
			for (count=0,run=1; COND(dsa_c[j][0]); count++)
				{
				ret=DSA_sign(EVP_PKEY_DSA,buf,20,buf2,
					&kk,dsa_key[j]);
				if (ret == 0)
					{
					BIO_printf(bio_err,
						"DSA sign failure\n");
					ERR_print_errors(bio_err);
					count=1;
					break;
					}
				}
			d=Time_F(STOP);
			BIO_printf(bio_err,mr ? "+R3:%ld:%d:%.2f\n"
				   : "%ld %d bit DSA signs in %.2fs\n",
				   count,dsa_bits[j],d);
			dsa_results[j][0]=d/(double)count;
			rsa_count=count;
			}

		ret=DSA_verify(EVP_PKEY_DSA,buf,20,buf2,
			kk,dsa_key[j]);
		if (ret <= 0)
			{
			BIO_printf(bio_err,"DSA verify failure.  No DSA verify will be done.\n");
			ERR_print_errors(bio_err);
			dsa_doit[j] = 0;
			}
		else
			{
			pkey_print_message("verify","dsa",
				dsa_c[j][1],dsa_bits[j],
				DSA_SECONDS);
			Time_F(START);
			for (count=0,run=1; COND(dsa_c[j][1]); count++)
				{
				ret=DSA_verify(EVP_PKEY_DSA,buf,20,buf2,
					kk,dsa_key[j]);
				if (ret <= 0)
					{
					BIO_printf(bio_err,
						"DSA verify failure\n");
					ERR_print_errors(bio_err);
					count=1;
					break;
					}
				}
			d=Time_F(STOP);
			BIO_printf(bio_err,mr ? "+R4:%ld:%d:%.2f\n"
				   : "%ld %d bit DSA verify in %.2fs\n",
				   count,dsa_bits[j],d);
			dsa_results[j][1]=d/(double)count;
			}

		if (rsa_count <= 1)
			{
			/* if longer than 10s, don't do any more */
			for (j++; j<DSA_NUM; j++)
				dsa_doit[j]=0;
			}
		}
	if (rnd_fake) RAND_cleanup();
#endif
#ifdef HAVE_FORK
show_res:
#endif
	if(!mr)
		{
		fprintf(stdout,"%s\n",SSLeay_version(SSLEAY_VERSION));
        fprintf(stdout,"%s\n",SSLeay_version(SSLEAY_BUILT_ON));
		printf("options:");
		printf("%s ",BN_options());
#ifndef OPENSSL_NO_MD2
		printf("%s ",MD2_options());
#endif
#ifndef OPENSSL_NO_RC4
		printf("%s ",RC4_options());
#endif
#ifndef OPENSSL_NO_DES
		printf("%s ",DES_options());
#endif
#ifndef OPENSSL_NO_AES
		printf("%s ",AES_options());
#endif
#ifndef OPENSSL_NO_IDEA
		printf("%s ",idea_options());
#endif
#ifndef OPENSSL_NO_BF
		printf("%s ",BF_options());
#endif
		fprintf(stdout,"\n%s\n",SSLeay_version(SSLEAY_CFLAGS));
		printf("available timing options: ");
#ifdef TIMES
		printf("TIMES ");
#endif
#ifdef TIMEB
		printf("TIMEB ");
#endif
#ifdef USE_TOD
		printf("USE_TOD ");
#endif
#ifdef HZ
#define as_string(s) (#s)
		printf("HZ=%g", (double)HZ);
# ifdef _SC_CLK_TCK
		printf(" [sysconf value]");
# endif
#endif
		printf("\n");
		printf("timing function used: %s%s%s%s%s%s%s\n",
		       (ftime_used ? "ftime" : ""),
		       (ftime_used + times_used > 1 ? "," : ""),
		       (times_used ? "times" : ""),
		       (ftime_used + times_used + gettimeofday_used > 1 ? "," : ""),
		       (gettimeofday_used ? "gettimeofday" : ""),
		       (ftime_used + times_used + gettimeofday_used + getrusage_used > 1 ? "," : ""),
		       (getrusage_used ? "getrusage" : ""));
		}

	if (pr_header)
		{
		if(mr)
			fprintf(stdout,"+H");
		else
			{
			fprintf(stdout,"The 'numbers' are in 1000s of bytes per second processed.\n"); 
			fprintf(stdout,"type        ");
			}
		for (j=0;  j<SIZE_NUM; j++)
			fprintf(stdout,mr ? ":%d" : "%7d bytes",lengths[j]);
		fprintf(stdout,"\n");
		}

	for (k=0; k<ALGOR_NUM; k++)
		{
		if (!doit[k]) continue;
		if(mr)
			fprintf(stdout,"+F:%d:%s",k,names[k]);
		else
			fprintf(stdout,"%-13s",names[k]);
		for (j=0; j<SIZE_NUM; j++)
			{
			if (results[k][j] > 10000 && !mr)
				fprintf(stdout," %11.2fk",results[k][j]/1e3);
			else
				fprintf(stdout,mr ? ":%.2f" : " %11.2f ",results[k][j]);
			}
		fprintf(stdout,"\n");
		}
#ifndef OPENSSL_NO_RSA
	j=1;
	for (k=0; k<RSA_NUM; k++)
		{
		if (!rsa_doit[k]) continue;
		if (j && !mr)
			{
			printf("%18ssign    verify    sign/s verify/s\n"," ");
			j=0;
			}
		if(mr)
			fprintf(stdout,"+F2:%u:%u:%f:%f\n",
				k,rsa_bits[k],rsa_results[k][0],
				rsa_results[k][1]);
		else
			fprintf(stdout,"rsa %4u bits %8.4fs %8.4fs %8.1f %8.1f\n",
				rsa_bits[k],rsa_results[k][0],rsa_results[k][1],
				1.0/rsa_results[k][0],1.0/rsa_results[k][1]);
		}
#endif
#ifndef OPENSSL_NO_DSA
	j=1;
	for (k=0; k<DSA_NUM; k++)
		{
		if (!dsa_doit[k]) continue;
		if (j && !mr)
			{
			printf("%18ssign    verify    sign/s verify/s\n"," ");
			j=0;
			}
		if(mr)
			fprintf(stdout,"+F3:%u:%u:%f:%f\n",
				k,dsa_bits[k],dsa_results[k][0],dsa_results[k][1]);
		else
			fprintf(stdout,"dsa %4u bits %8.4fs %8.4fs %8.1f %8.1f\n",
				dsa_bits[k],dsa_results[k][0],dsa_results[k][1],
				1.0/dsa_results[k][0],1.0/dsa_results[k][1]);
		}
#endif
	mret=0;
end:
	ERR_print_errors(bio_err);
	if (buf != NULL) OPENSSL_free(buf);
	if (buf2 != NULL) OPENSSL_free(buf2);
#ifndef OPENSSL_NO_RSA
	for (i=0; i<RSA_NUM; i++)
		if (rsa_key[i] != NULL)
			RSA_free(rsa_key[i]);
#endif
#ifndef OPENSSL_NO_DSA
	for (i=0; i<DSA_NUM; i++)
		if (dsa_key[i] != NULL)
			DSA_free(dsa_key[i]);
#endif
	apps_shutdown();
	OPENSSL_EXIT(mret);
	}

static void print_message(const char *s, long num, int length)
	{
#ifdef SIGALRM
	BIO_printf(bio_err,mr ? "+DT:%s:%d:%d\n"
		   : "Doing %s for %ds on %d size blocks: ",s,SECONDS,length);
	(void)BIO_flush(bio_err);
	alarm(SECONDS);
#else
	BIO_printf(bio_err,mr ? "+DN:%s:%ld:%d\n"
		   : "Doing %s %ld times on %d size blocks: ",s,num,length);
	(void)BIO_flush(bio_err);
#endif
#ifdef LINT
	num=num;
#endif
	}

static void pkey_print_message(char *str, char *str2, long num, int bits,
	     int tm)
	{
#ifdef SIGALRM
	BIO_printf(bio_err,mr ? "+DTP:%d:%s:%s:%d\n"
			   : "Doing %d bit %s %s's for %ds: ",bits,str,str2,tm);
	(void)BIO_flush(bio_err);
	alarm(RSA_SECONDS);
#else
	BIO_printf(bio_err,mr ? "+DNP:%ld:%d:%s:%s\n"
			   : "Doing %ld %d bit %s %s's: ",num,bits,str,str2);
	(void)BIO_flush(bio_err);
#endif
#ifdef LINT
	num=num;
#endif
	}

static void print_result(int alg,int run_no,int count,double time_used)
	{
	BIO_printf(bio_err,mr ? "+R:%ld:%s:%f\n"
		   : "%ld %s's in %.2fs\n",count,names[alg],time_used);
	results[alg][run_no]=((double)count)/time_used*lengths[run_no];
	}

static char *sstrsep(char **string, const char *delim)
    {
    char isdelim[256];
    char *token = *string;

    if (**string == 0)
        return NULL;

    memset(isdelim, 0, sizeof isdelim);
    isdelim[0] = 1;

    while (*delim)
        {
        isdelim[(unsigned char)(*delim)] = 1;
        delim++;
        }

    while (!isdelim[(unsigned char)(**string)])
        {
        (*string)++;
        }

    if (**string)
        {
        **string = 0;
        (*string)++;
        }

    return token;
    }

#ifdef HAVE_FORK
static int do_multi(int multi)
	{
	int n;
	int fd[2];
	int *fds;
	static char sep[]=":";

	fds=malloc(multi*sizeof *fds);
	for(n=0 ; n < multi ; ++n)
		{
		pipe(fd);
		if(fork())
			{
			close(fd[1]);
			fds[n]=fd[0];
			}
		else
			{
			close(fd[0]);
			close(1);
			dup(fd[1]);
			close(fd[1]);
			mr=1;
			usertime=0;
			return 0;
			}
		printf("Forked child %d\n",n);
		}

	/* for now, assume the pipe is long enough to take all the output */
	for(n=0 ; n < multi ; ++n)
		{
		FILE *f;
		char buf[1024];
		char *p;

		f=fdopen(fds[n],"r");
		while(fgets(buf,sizeof buf,f))
			{
			p=strchr(buf,'\n');
			if(p)
				*p='\0';
			if(buf[0] != '+')
				{
				fprintf(stderr,"Don't understand line '%s' from child %d\n",
						buf,n);
				continue;
				}
			printf("Got: %s from %d\n",buf,n);
			if(!strncmp(buf,"+F:",3))
				{
				int alg;
				int j;

				p=buf+3;
				alg=atoi(sstrsep(&p,sep));
				sstrsep(&p,sep);
				for(j=0 ; j < SIZE_NUM ; ++j)
					results[alg][j]+=atof(sstrsep(&p,sep));
				}
			else if(!strncmp(buf,"+F2:",4))
				{
				int k;
				double d;
				
				p=buf+4;
				k=atoi(sstrsep(&p,sep));
				sstrsep(&p,sep);

				d=atof(sstrsep(&p,sep));
				if(n)
					rsa_results[k][0]=1/(1/rsa_results[k][0]+1/d);
				else
					rsa_results[k][0]=d;

				d=atof(sstrsep(&p,sep));
				if(n)
					rsa_results[k][1]=1/(1/rsa_results[k][1]+1/d);
				else
					rsa_results[k][1]=d;
				}
			else if(!strncmp(buf,"+F2:",4))
				{
				int k;
				double d;
				
				p=buf+4;
				k=atoi(sstrsep(&p,sep));
				sstrsep(&p,sep);

				d=atof(sstrsep(&p,sep));
				if(n)
					rsa_results[k][0]=1/(1/rsa_results[k][0]+1/d);
				else
					rsa_results[k][0]=d;

				d=atof(sstrsep(&p,sep));
				if(n)
					rsa_results[k][1]=1/(1/rsa_results[k][1]+1/d);
				else
					rsa_results[k][1]=d;
				}
			else if(!strncmp(buf,"+F3:",4))
				{
				int k;
				double d;
				
				p=buf+4;
				k=atoi(sstrsep(&p,sep));
				sstrsep(&p,sep);

				d=atof(sstrsep(&p,sep));
				if(n)
					dsa_results[k][0]=1/(1/dsa_results[k][0]+1/d);
				else
					dsa_results[k][0]=d;

				d=atof(sstrsep(&p,sep));
				if(n)
					dsa_results[k][1]=1/(1/dsa_results[k][1]+1/d);
				else
					dsa_results[k][1]=d;
				}
			else if(!strncmp(buf,"+H:",3))
				{
				}
			else
				fprintf(stderr,"Unknown type '%s' from child %d\n",buf,n);
			}
		}
	return 1;
	}
#endif
#endif
