/*	$NetBSD: xlint.c,v 1.3 1995/10/23 14:29:30 jpo Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/xlint/xlint/xlint.c,v 1.8 2000/01/14 09:25:31 sheldonh Exp $
 * $DragonFly: src/usr.bin/xlint/xlint/xlint.c,v 1.11 2005/04/05 08:19:35 joerg Exp $
 */

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <paths.h>

#include "lint.h"
#include "pathnames.h"

/* directory for temporary files */
static	const	char *tmpdir;

/* path name for cpp output */
static	char	*cppout;

/* files created by 1st pass */
static	char	**p1out;

/* input files for 2nd pass (without libraries) */
static	char	**p2in;

/* library which will be created by 2nd pass */
static	char	*p2out;

/* flags always passed to cpp */
static	char	**cppflags;

/* flags for cpp, controled by sflag/tflag */
static	char	**lcppflgs;

/* flags for lint1 */
static	char	**l1flags;

/* flags for lint2 */
static	char	**l2flags;

/* libraries for lint2 */
static	char	**l2libs;

/* default libraries */
static	char	**deflibs;

/* additional libraries */
static	char	**libs;

/* search path for libraries */
static	char	**libsrchpath;

/* flags */
static	int	iflag, oflag, Cflag, sflag, tflag, Fflag, Sflag;

/* print the commands executed to run the stages of compilation */
static	int	Vflag;

/* filename for oflag */
static	char	*outputfn;

/* reset after first .c source has been processed */
static	int	first = 1;

/*
 * name of a file which is currently written by a child and should
 * be removed after abnormal termination of the child
 */
static	const	char *currfn;


static	void	appstrg(char ***, char *);
static	void	appcstrg(char ***, const char *);
static	void	applst(char ***, char *const *);
static	void	freelst(char ***);
static	char	*concat2(const char *, const char *);
static	char	*concat3(const char *, const char *, const char *);
static	void	terminate(int);
static	const	char *basename(const char *, int);
static	void	appdef(char ***, const char *);
static	void	usage(void);
static	void	fname(const char *, int);
static	void	runchild(const char *, char *const *, const char *);
static	void	findlibs(char *const *);
static	int	rdok(const char *);
static	void	lint2(void);
static	void	cat(char *const *, const char *);

/*
 * Some functions to deal with lists of strings.
 * Take care that we get no surprises in case of asyncron signals.
 */
static void
appstrg(char ***lstp, char *s)
{
	char	**lst, **olst;
	int	i;

	olst = *lstp;
	for (i = 0; olst[i] != NULL; i++) ;
	lst = xmalloc((i + 2) * sizeof (char *));
	memcpy(lst, olst, i * sizeof (char *));
	lst[i] = s;
	lst[i + 1] = NULL;
	*lstp = lst;
}

static void
appcstrg(char ***lstp, const char *s)
{
	appstrg(lstp, xstrdup(s));
}

static void
applst(char ***destp, char *const *src)
{
	int	i, k;
	char	**dest, **odest;

	odest = *destp;
	for (i = 0; odest[i] != NULL; i++) ;
	for (k = 0; src[k] != NULL; k++) ;
	dest = xmalloc((i + k + 1) * sizeof (char *));
	memcpy(dest, odest, i * sizeof (char *));
	for (k = 0; src[k] != NULL; k++)
		dest[i + k] = xstrdup(src[k]);
	dest[i + k] = NULL;
	*destp = dest;
	free(odest);
}

static void
freelst(char ***lstp)
{
	char	*s;
	int	i;

	for (i = 0; (*lstp)[i] != NULL; i++) ;
	while (i-- > 0) {
		s = (*lstp)[i];
		(*lstp)[i] = NULL;
		free(s);
	}
}

static char *
concat2(const char *s1, const char *s2)
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + 1);
	strcpy(s, s1);
	strcat(s, s2);

	return (s);
}

static char *
concat3(const char *s1, const char *s2, const char *s3)
{
	char	*s;

	s = xmalloc(strlen(s1) + strlen(s2) + strlen(s3) + 1);
	strcpy(s, s1);
	strcat(s, s2);
	strcat(s, s3);

	return (s);
}

/*
 * Clean up after a signal.
 */
static void
terminate(int signo)
{
	int	i;

	if (cppout != NULL)
		remove(cppout);

	if (p1out != NULL) {
		for (i = 0; p1out[i] != NULL; i++)
			remove(p1out[i]);
	}

	if (p2out != NULL)
		remove(p2out);

	if (currfn != NULL)
		remove(currfn);

	exit(signo != 0 ? 1 : 0);
}

/*
 * Returns a pointer to the last component of strg after delim.
 * Returns strg if the string does not contain delim.
 */
static const char *
basename(const char *strg, int delim)
{
	const	char *cp, *cp1, *cp2;

	cp = cp1 = cp2 = strg;
	while (*cp != '\0') {
		if (*cp++ == delim) {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

static void
appdef(char ***lstp, const char *def)
{
	appstrg(lstp, concat2("-D__", def));
	appstrg(lstp, concat3("-D__", def, "__"));
}

static void
usage(void)
{
	printf("lint [-abceghprvxzHFS] [-s|-t] [-i|-nu] [-Dname[=def]] [-Uname]\n");
	printf("     [-Idirectory] [-Ldirectory] [-llibrary] [-ooutputfile] file ...\n");
	printf("\n");
	printf("lint [-abceghprvzHFS] [-s|-t] -Clibrary [-Dname[=def]]\n");
	printf("     [-Idirectory] [-Uname] file ...\n");
	terminate(-1);
}

int
main(int argc, char **argv)
{
	int	c, fd;
	char	flgbuf[3], *tmp, *s;
	size_t	len;
	struct	utsname un;

	if ((tmp = getenv("TMPDIR")) == NULL || (len = strlen(tmp)) == 0) {
		tmpdir = xstrdup(_PATH_TMP);
	} else {
		s = xmalloc(len + 2);
		sprintf(s, "%s%s", tmp, tmp[len - 1] == '/' ? "" : "/");
		tmpdir = s;
	}

	cppout = xmalloc(strlen(tmpdir) + sizeof ("lint0.XXXXXX"));
	sprintf(cppout, "%slint0.XXXXXX", tmpdir);

	fd = mkstemp(cppout);
	if (fd == -1) {
		err(1, "could not make a temporary file");
		close(fd);
		terminate(-1);
	}
	close (fd);

	p1out = xcalloc(1, sizeof (char *));
	p2in = xcalloc(1, sizeof (char *));
	cppflags = xcalloc(1, sizeof (char *));
	lcppflgs = xcalloc(1, sizeof (char *));
	l1flags = xcalloc(1, sizeof (char *));
	l2flags = xcalloc(1, sizeof (char *));
	l2libs = xcalloc(1, sizeof (char *));
	deflibs = xcalloc(1, sizeof (char *));
	libs = xcalloc(1, sizeof (char *));
	libsrchpath = xcalloc(1, sizeof (char *));

	appcstrg(&cppflags, "-lang-c");
	appcstrg(&cppflags, "-$");
	appcstrg(&cppflags, "-C");
	appcstrg(&cppflags, "-Wcomment");
#ifdef __DragonFly__
	appcstrg(&cppflags, "-D__DragonFly__=" __XSTRING(__DragonFly__));
#else
#	error "This ain't NetBSD.  You lose!"
	appcstrg(&cppflags, "-D__NetBSD__");
#endif
	appcstrg(&cppflags, "-Dlint");		/* XXX don't def. with -s */
	appdef(&cppflags, "lint");
	appdef(&cppflags, "unix");

	appcstrg(&lcppflgs, "-Wtraditional");

	if (uname(&un) == -1)
		err(1, "uname");
	appdef(&cppflags, un.machine);
	appstrg(&lcppflgs, concat2("-D", un.machine));

#ifdef MACHINE_ARCH
	if (strcmp(un.machine, MACHINE_ARCH) != 0) {
		appdef(&cppflags, MACHINE_ARCH);
		appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
	}
#endif

	appcstrg(&deflibs, "c");

	if (signal(SIGHUP, terminate) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	signal(SIGINT, terminate);
	signal(SIGQUIT, terminate);
	signal(SIGTERM, terminate);

	while (argc > optind) {

		c = getopt(argc, argv, "abceghil:no:prstuvxzC:D:FHI:L:SU:V");

		switch (c) {

		case 'a':
		case 'b':
		case 'c':
		case 'e':
		case 'g':
		case 'r':
		case 'v':
		case 'z':
			sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			break;

		case 'F':
			Fflag = 1;
			/* FALLTHROUGH */
		case 'u':
		case 'h':
			sprintf(flgbuf, "-%c", c);
			appcstrg(&l1flags, flgbuf);
			appcstrg(&l2flags, flgbuf);
			break;

		case 'i':
			if (Cflag)
				usage();
			iflag = 1;
			break;

		case 'n':
			freelst(&deflibs);
			break;

		case 'p':
			appcstrg(&l1flags, "-p");
			appcstrg(&l2flags, "-p");
			if (*deflibs != NULL) {
				freelst(&deflibs);
				appcstrg(&deflibs, "c");
			}
			break;

		case 's':
			if (tflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-trigraphs");
			appcstrg(&lcppflgs, "-Wtrigraphs");
			appcstrg(&lcppflgs, "-pedantic");
			appcstrg(&lcppflgs, "-D__STRICT_ANSI__");
			appcstrg(&l1flags, "-s");
			appcstrg(&l2flags, "-s");
			sflag = 1;
			break;

		case 'S':
			if (tflag)
				usage();
			appcstrg(&l1flags, "-S");
			Sflag = 1;

		case 't':
			if (sflag)
				usage();
			freelst(&lcppflgs);
			appcstrg(&lcppflgs, "-traditional");
			appstrg(&lcppflgs, concat2("-D", MACHINE));
#ifdef MACHINE_ARCH
			appstrg(&lcppflgs, concat2("-D", MACHINE_ARCH));
#endif
			appcstrg(&l1flags, "-t");
			appcstrg(&l2flags, "-t");
			tflag = 1;
			break;

		case 'x':
			appcstrg(&l2flags, "-x");
			break;

		case 'C':
			if (Cflag || oflag || iflag)
				usage();
			Cflag = 1;
			appstrg(&l2flags, concat2("-C", optarg));
			p2out = xmalloc(sizeof ("llib-l.ln") + strlen(optarg));
			sprintf(p2out, "llib-l%s.ln", optarg);
			freelst(&deflibs);
			break;

		case 'D':
		case 'I':
		case 'U':
			sprintf(flgbuf, "-%c", c);
			appstrg(&cppflags, concat2(flgbuf, optarg));
			break;

		case 'l':
			appcstrg(&libs, optarg);
			break;

		case 'o':
			if (Cflag || oflag)
				usage();
			oflag = 1;
			outputfn = xstrdup(optarg);
			break;

		case 'L':
			appcstrg(&libsrchpath, optarg);
			break;

		case 'H':
			appcstrg(&l2flags, "-H");
			break;

		case 'V':
			Vflag = 1;
			break;

		case '?':
			usage();
			/* NOTREACHED */

		case -1:
			/* filename */
			fname(argv[optind], argc == optind + 1);
			first = 0;

			argc -= optind;
			argv += optind;
			optreset = optind = 1;
		}
	}

	if (first)
		usage();

	if (iflag)
		terminate(0);

	if (!oflag) {
		if ((s = getenv("LIBDIR")) == NULL || strlen(s) == 0)
			s = PATH_LINTLIB;
		appcstrg(&libsrchpath, s);
		findlibs(libs);
		findlibs(deflibs);
	}

	printf("Lint pass2:\n");
	lint2();

	if (oflag)
		cat(p2in, outputfn);

	if (Cflag)
		p2out = NULL;

	terminate(0);
	/* NOTREACHED */
	return 0;
}

/*
 * Read a file name from the command line
 * and pass it through lint1 if it is a C source.
 */
static void
fname(const char *name, int last)
{
	const	char *bn, *suff;
	char	**args, *ofn, *path;
	size_t	len;
	int fd;

	bn = basename(name, '/');
	suff = basename(bn, '.');

	if (strcmp(suff, "ln") == 0) {
		/* only for lint2 */
		if (!iflag)
			appcstrg(&p2in, name);
		return;
	}

	if (strcmp(suff, "c") != 0 &&
	    (strncmp(bn, "llib-l", 6) != 0 || bn != suff)) {
		warnx("unknown file type: %s\n", name);
		return;
	}

	if (!iflag || !first || !last)
		printf("%s:\n", Fflag ? name : bn);

	/* build the name of the output file of lint1 */
	if (oflag) {
		ofn = outputfn;
		outputfn = NULL;
		oflag = 0;
	} else if (iflag) {
		ofn = xmalloc(strlen(bn) + (bn == suff ? 4 : 2));
		len = bn == suff ? strlen(bn) : (suff - 1) - bn;
		sprintf(ofn, "%.*s", (int)len, bn);
		strcat(ofn, ".ln");
	} else {
		ofn = xmalloc(strlen(tmpdir) + sizeof ("lint1.XXXXXX"));
		sprintf(ofn, "%slint1.XXXXXX", tmpdir);
		fd = mkstemp(ofn);
		if (fd == -1) {
			err(1, "could not make a temporary file");
			close(fd);
			terminate(-1);
        	}
        	close (fd);
	}
	if (!iflag)
		appcstrg(&p1out, ofn);

	args = xcalloc(1, sizeof (char *));

	/* run cpp */

	path = xmalloc(strlen(PATH_USRBIN) + sizeof ("/cpp"));
	sprintf(path, "%s/cpp", PATH_USRBIN);

	appcstrg(&args, path);
	applst(&args, cppflags);
	applst(&args, lcppflgs);
	appcstrg(&args, name);
	appcstrg(&args, cppout);

	runchild(path, args, cppout);
	free(path);
	freelst(&args);

	/* run lint1 */

	path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint1"));
	sprintf(path, "%s/lint1", PATH_LIBEXEC);

	appcstrg(&args, path);
	applst(&args, l1flags);
	appcstrg(&args, cppout);
	appcstrg(&args, ofn);

	runchild(path, args, ofn);
	free(path);
	freelst(&args);

	appcstrg(&p2in, ofn);
	free(ofn);

	free(args);
}

static void
runchild(const char *path, char *const *args, const char *crfn)
{
	int	status, rv, signo, i;

	if (Vflag) {
		for (i = 0; args[i] != NULL; i++)
			printf("%s ", args[i]);
		printf("\n");
	}

	currfn = crfn;

	fflush(stdout);

	switch (fork()) {
	case -1:
		warn("cannot fork");
		terminate(-1);
		/* NOTREACHED */
	default:
		/* parent */
		break;
	case 0:
		/* child */
		execv(path, args);
		warn("cannot exec %s", path);
		exit(1);
		/* NOTREACHED */
	}

	while ((rv = wait(&status)) == -1 && errno == EINTR) ;
	if (rv == -1) {
		warn("wait");
		terminate(-1);
	}
	if (WIFSIGNALED(status)) {
		signo = WTERMSIG(status);
		warnx("%s got SIG%s", path, sys_signame[signo]);
		terminate(-1);
	}
	if (WEXITSTATUS(status) != 0)
		terminate(-1);
	currfn = NULL;
}

static void
findlibs(char *const *liblst)
{
	int	i, k;
	const	char *lib, *path;
	char	*lfn;
	size_t	len;

	lfn = NULL;

	for (i = 0; (lib = liblst[i]) != NULL; i++) {
		for (k = 0; (path = libsrchpath[k]) != NULL; k++) {
			len = strlen(path) + strlen(lib);
			lfn = xrealloc(lfn, len + sizeof ("/llib-l.ln"));
			sprintf(lfn, "%s/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
			lfn = xrealloc(lfn, len + sizeof ("/lint/llib-l.ln"));
			sprintf(lfn, "%s/lint/llib-l%s.ln", path, lib);
			if (rdok(lfn))
				break;
		}
		if (path != NULL) {
			appstrg(&l2libs, concat2("-l", lfn));
		} else {
			warnx("cannot find llib-l%s.ln", lib);
		}
	}

	free(lfn);
}

static int
rdok(const char *path)
{
	struct	stat sbuf;

	if (stat(path, &sbuf) == -1)
		return (0);
	if ((sbuf.st_mode & S_IFMT) != S_IFREG)
		return (0);
	if (access(path, R_OK) == -1)
		return (0);
	return (1);
}

static void
lint2(void)
{
	char	*path, **args;

	args = xcalloc(1, sizeof (char *));

	path = xmalloc(strlen(PATH_LIBEXEC) + sizeof ("/lint2"));
	sprintf(path, "%s/lint2", PATH_LIBEXEC);

	appcstrg(&args, path);
	applst(&args, l2flags);
	applst(&args, l2libs);
	applst(&args, p2in);

	runchild(path, args, p2out);
	free(path);
	freelst(&args);
	free(args);
}

static void
cat(char *const *srcs, const char *dest)
{
	int	ifd, ofd, i;
	char	*src, *buf;
	ssize_t	rlen;

	if ((ofd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		warn("cannot open %s", dest);
		terminate(-1);
	}

	buf = xmalloc(MBLKSIZ);

	for (i = 0; (src = srcs[i]) != NULL; i++) {
		if ((ifd = open(src, O_RDONLY)) == -1) {
			free(buf);
			warn("cannot open %s", src);
			terminate(-1);
		}
		do {
			if ((rlen = read(ifd, buf, MBLKSIZ)) == -1) {
				free(buf);
				warn("read error on %s", src);
				terminate(-1);
			}
			if (write(ofd, buf, (size_t)rlen) == -1) {
				free(buf);
				warn("write error on %s", dest);
				terminate(-1);
			}
		} while (rlen == MBLKSIZ);
		close(ifd);
	}
	close(ofd);
	free(buf);
}

