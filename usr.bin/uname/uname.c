/*-
 * Copyright (c) 2002 Juli Mallett.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1993 The Regents of the University of California.  All rights reserved.
 * @(#)uname.c	8.2 (Berkeley) 5/4/95
 * $FreeBSD: src/usr.bin/uname/uname.c,v 1.4.6.2 2002/10/17 07:47:29 jmallett Exp $
 * $DragonFly: src/usr.bin/uname/uname.c,v 1.3 2003/10/24 17:19:16 dillon Exp $
 */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	MFLAG	0x01
#define	NFLAG	0x02
#define	PFLAG	0x04
#define	RFLAG	0x08
#define	SFLAG	0x10
#define	VFLAG	0x20
#define	IFLAG   0x40

typedef void (*get_t)(void);
get_t get_ident, get_platform, get_hostname, get_arch, get_release, get_sysname, get_version;
  
void native_ident(void);
void native_platform(void);
void native_hostname(void);
void native_arch(void);
void native_release(void);
void native_sysname(void);
void native_version(void);
void print_uname(u_int);
void setup_get(void);
void usage(void);

char *ident, *platform, *hostname, *arch, *release, *sysname, *version;
int space;

int
main(int argc, char *argv[])
{
	u_int flags;
	int ch;

	setup_get();
	flags = 0;

	while ((ch = getopt(argc, argv, "aimnprsv")) != -1)
		switch(ch) {
		case 'a':
			flags |= (MFLAG | NFLAG | RFLAG | SFLAG | VFLAG);
			break;
		case 'i':
			flags |= IFLAG;
			break;
		case 'm':
			flags |= MFLAG;
			break;
		case 'n':
			flags |= NFLAG;
			break;
		case 'p':
			flags |= PFLAG;
			break;
		case 'r':
			flags |= RFLAG;
			break;
		case 's':
			flags |= SFLAG;
			break;
		case 'v':
			flags |= VFLAG;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (!flags)
		flags |= SFLAG;

	print_uname(flags);
	exit(0);
}

#define	CHECK_ENV(opt,var)				\
do {							\
	if ((var = getenv("UNAME_" opt)) == NULL) {	\
		get_##var = native_##var;		\
	} else {					\
		get_##var = (get_t)NULL;		\
	}						\
} while (0)

void
setup_get(void)
{
	CHECK_ENV("s", sysname);
	CHECK_ENV("n", hostname);
	CHECK_ENV("r", release);
	CHECK_ENV("v", version);
	CHECK_ENV("m", platform);
	CHECK_ENV("p", arch);
	CHECK_ENV("i", ident);
}

#define	PRINT_FLAG(flags,flag,var)		\
	if ((flags & flag) == flag) {		\
		if (space)			\
			printf(" ");		\
		else				\
			space++;		\
		if (get_##var != NULL)		\
			(*get_##var)();		\
		printf("%s", var);		\
	}

void
print_uname(u_int flags)
{
	PRINT_FLAG(flags, SFLAG, sysname);
	PRINT_FLAG(flags, NFLAG, hostname);
	PRINT_FLAG(flags, RFLAG, release);
	PRINT_FLAG(flags, VFLAG, version);
	PRINT_FLAG(flags, MFLAG, platform);
	PRINT_FLAG(flags, PFLAG, arch);
	PRINT_FLAG(flags, IFLAG, ident);
	printf("\n");
}

#define	NATIVE_SYSCTL2_GET(var,mib0,mib1)	\
void						\
native_##var(void)				\
{						\
	int mib[] = { (mib0), (mib1) };		\
	size_t len;				\
	static char buf[1024];			\
	char **varp = &(var);			\
						\
	len = sizeof buf;			\
	if (sysctl(mib, sizeof mib / sizeof mib[0],	\
	   &buf, &len, NULL, 0) == -1)		\
		err(1, "sysctl");

#define	NATIVE_SYSCTLNAME_GET(var,name)		\
void						\
native_##var(void)				\
{						\
	size_t len;				\
	static char buf[1024];			\
	char **varp = &(var);			\
						\
	len = sizeof buf;			\
	if (sysctlbyname(name, &buf, &len, NULL,\
	    0) == -1)				\
		err(1, "sysctlbyname");

#define	NATIVE_SET				\
	*varp = buf;				\
	return;					\
}	struct __hack

#define	NATIVE_BUFFER	(buf)
#define	NATIVE_LENGTH	(len)

NATIVE_SYSCTL2_GET(sysname, CTL_KERN, KERN_OSTYPE) {
} NATIVE_SET;

NATIVE_SYSCTL2_GET(hostname, CTL_KERN, KERN_HOSTNAME) {
} NATIVE_SET;

NATIVE_SYSCTL2_GET(release, CTL_KERN, KERN_OSRELEASE) {
} NATIVE_SET;

NATIVE_SYSCTL2_GET(version, CTL_KERN, KERN_VERSION) {
	size_t n;
	char *p;

	p = NATIVE_BUFFER;
	n = NATIVE_LENGTH;
	for (; n--; ++p)
		if (*p == '\n' || *p == '\t')
			*p = ' ';
} NATIVE_SET;

NATIVE_SYSCTL2_GET(platform, CTL_HW, HW_MACHINE) {
} NATIVE_SET;

NATIVE_SYSCTL2_GET(arch, CTL_HW, HW_MACHINE_ARCH) {
} NATIVE_SET;

NATIVE_SYSCTLNAME_GET(ident, "kern.ident") {
} NATIVE_SET;

void
usage(void)
{
	fprintf(stderr, "usage: uname [-aimnprsv]\n");
	exit(1);
}
