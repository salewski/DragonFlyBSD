/*
 * Copyright 2000 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/getconf/getconf.c,v 1.6.2.1 2002/10/27 04:18:40 wollman Exp $
 * $DragonFly: src/usr.bin/getconf/getconf.c,v 1.3 2003/11/04 20:25:45 dillon Exp $
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "getconf.h"

static void	do_confstr(const char *name, int key);
static void	do_sysconf(const char *name, int key);
static void	do_pathconf(const char *name, int key, const char *path);

static void
usage(void)
{
	fprintf(stderr, "usage:\n"
		"\tgetconf [-v prog_env] system_var\n"
		"\tgetconf [-v prog_env] path_var pathname\n");
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	int c, key, valid;
	const char *name, *vflag, *alt_path;
	gc_intmax_t limitval;

	vflag = NULL;
	while ((c = getopt(argc, argv, "v:")) != -1) {
		switch (c) {
		case 'v':
			vflag = optarg;
			break;

		default:
			usage();
		}
	}

	if ((name = argv[optind]) == NULL)
		usage();

	if (vflag != NULL) {
		if ((valid = find_progenv(vflag, &alt_path)) == 0)
			errx(EX_USAGE, "invalid programming environment %s",
			     vflag);
		if (valid > 0 && alt_path != NULL) {
			if (argv[optind + 1] == NULL)
				execl(alt_path, "getconf", argv[optind],
				      (char *)NULL);
			else
				execl(alt_path, "getconf", argv[optind],
				      argv[optind + 1], (char *)NULL);

			err(EX_OSERR, "execl: %s", alt_path);
		}
		if (valid < 0)
			errx(EX_UNAVAILABLE, "environment %s is not available",
			     vflag);
	}

	if (argv[optind + 1] == NULL) { /* confstr or sysconf */
		if ((valid = find_limit(name, &limitval)) != 0) {
			if (valid > 0)
				printf("%" GC_PRIdMAX "\n", limitval);
			else
				printf("undefined\n");

			return 0;
		}
		if ((valid = find_confstr(name, &key)) != 0) {
			if (valid > 0)
				do_confstr(name, key);
			else
				printf("undefined\n");
		} else {		
			valid = find_sysconf(name, &key);
			if (valid > 0) {
				do_sysconf(name, key);
			} else if (valid < 0) {
				printf("undefined\n");
			} else 
				errx(EX_USAGE,
				     "no such configuration parameter `%s'",
				     name);
		}
	} else {
		valid = find_pathconf(name, &key);
		if (valid != 0) {
			if (valid > 0)
				do_pathconf(name, key, argv[optind + 1]);
			else
				printf("undefined\n");
		} else
			errx(EX_USAGE,
			     "no such path configuration parameter `%s'",
			     name);
	}
	return 0;
}

static void
do_confstr(const char *name, int key)
{
	char *buf;
	size_t len;

	len = confstr(key, 0, 0);
	if (len == (size_t)-1)
		err(EX_OSERR, "confstr: %s", name);
	
	if (len == 0) {
		printf("undefined\n");
	} else {
		buf = alloca(len);
		confstr(key, buf, len);
		printf("%s\n", buf);
	}
}

static void
do_sysconf(const char *name, int key)
{
	long value;

	errno = 0;
	value = sysconf(key);
	if (value == -1 && errno != 0)
		err(EX_OSERR, "sysconf: %s", name);
	else if (value == -1)
		printf("undefined\n");
	else
		printf("%ld\n", value);
}

static void
do_pathconf(const char *name, int key, const char *path)
{
	long value;

	errno = 0;
	value = pathconf(path, key);
	if (value == -1 && errno != 0)
		err(EX_OSERR, "pathconf: %s", name);
	else if (value == -1)
		printf("undefined\n");
	else
		printf("%ld\n", value);
}

