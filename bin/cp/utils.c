/*-
 * Copyright (c) 1991, 1993, 1994
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
 * @(#)utils.c	8.3 (Berkeley) 4/1/94
 * $FreeBSD: src/bin/cp/utils.c,v 1.45 2005/02/09 17:37:37 ru Exp $
 * $DragonFly: src/bin/cp/utils.c,v 1.8 2005/02/28 23:15:35 corecode Exp $
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
#include <sys/mman.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "extern.h"
#define	cp_pct(x,y)	(int)(100.0 * (double)(x) / (double)(y))

#define YESNO "(y/n [n]) "

int
copy_file(const FTSENT *entp, int dne)
{
	static char buf[MAXBSIZE];
	struct stat *fs;
	int ch, checkch, from_fd, rcount, rval, to_fd, wcount, wresid, wtotal;
	char *bufp;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	char *p;
#endif

	if ((from_fd = open(entp->fts_path, O_RDONLY, 0)) == -1) {
		warn("%s", entp->fts_path);
		return (1);
	}

	fs = entp->fts_statp;

	/*
	 * If the file exists and we're interactive, verify with the user.
	 * If the file DNE, set the mode to be the from file, minus setuid
	 * bits, modified by the umask; arguably wrong, but it makes copying
	 * executables work right and it's been that way forever.  (The
	 * other choice is 666 or'ed with the execute bits on the from file
	 * modified by the umask.)
	 */
	if (!dne) {
		if (nflag) {
			if (vflag)
				printf("%s not overwritten\n", to.p_path);
			close(from_fd);
			return (0);
		} else if (iflag) {
			fprintf(stderr, "overwrite %s? %s", 
					to.p_path, YESNO);
			checkch = ch = getchar();
			while (ch != '\n' && ch != EOF)
				ch = getchar();
			if (checkch != 'y' && checkch != 'Y') {
				close(from_fd);
				fprintf(stderr, "not overwritten\n");
				return (1);
			}
		}
		
		if (fflag) {
		    /* remove existing destination file name, 
		     * create a new file  */
		    unlink(to.p_path);
		    to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
				 fs->st_mode & ~(S_ISUID | S_ISGID));
		} else 
		    /* overwrite existing destination file name */
		    to_fd = open(to.p_path, O_WRONLY | O_TRUNC, 0);
	} else
		to_fd = open(to.p_path, O_WRONLY | O_TRUNC | O_CREAT,
		    fs->st_mode & ~(S_ISUID | S_ISGID));

	if (to_fd == -1) {
		warn("%s", to.p_path);
		close(from_fd);
		return (1);;
	}

	rval = 0;

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	if (S_ISREG(fs->st_mode) && fs->st_size > 0 &&
	    fs->st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
		    MAP_SHARED, from_fd, (off_t)0)) == MAP_FAILED) {
			warn("%s", entp->fts_path);
			rval = 1;
		} else {
			wtotal = 0;
			for (bufp = p, wresid = fs->st_size; ;
			    bufp += wcount, wresid -= wcount) {
				wcount = write(to_fd, bufp, wresid);
				wtotal += wcount;
				if (info) {
					info = 0;
					fprintf(stderr,
					    "%s -> %s %3d%%\n",
					    entp->fts_path, to.p_path,
					    cp_pct(wtotal, fs->st_size));
				}
				if (wcount >= wresid || wcount <= 0)
					break;
			}
			if (wcount != wresid) {
				warn("%s", to.p_path);
				rval = 1;
			}
			/* Some systems don't unmap on close(2). */
			if (munmap(p, fs->st_size) < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		}
	} else
#endif
	{
		wtotal = 0;
		while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
			for (bufp = buf, wresid = rcount; ;
			    bufp += wcount, wresid -= wcount) {
				wcount = write(to_fd, bufp, wresid);
				wtotal += wcount;
				if (info) {
					info = 0;
					fprintf(stderr,
					    "%s -> %s %3d%%\n",
					    entp->fts_path, to.p_path,
					    cp_pct(wtotal, fs->st_size));
				}
				if (wcount >= wresid || wcount <= 0)
					break;
			}
			if (wcount != wresid) {
				warn("%s", to.p_path);
				rval = 1;
				break;
			}
		}
		if (rcount < 0) {
			warn("%s", entp->fts_path);
			rval = 1;
		}
	}

	/*
	 * Don't remove the target even after an error.  The target might
	 * not be a regular file, or its attributes might be important,
	 * or its contents might be irreplaceable.  It would only be safe
	 * to remove it if we created it and its length is 0.
	 */

	if (pflag && setfile(fs, to_fd))
		rval = 1;
	close(from_fd);
	if (close(to_fd)) {
		warn("%s", to.p_path);
		rval = 1;
	}
	return (rval);
}

int
copy_link(const FTSENT *p, int exists)
{
	int len;
	char linkname[PATH_MAX];

	if ((len = readlink(p->fts_path, linkname, sizeof(linkname) - 1)) == -1) {
		warn("readlink: %s", p->fts_path);
		return (1);
	}
	linkname[len] = '\0';
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (symlink(linkname, to.p_path)) {
		warn("symlink: %s", linkname);
		return (1);
	}
	return (pflag ? setfile(p->fts_statp, -1) : 0);
}

int
copy_fifo(struct stat *from_stat, int exists)
{
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mkfifo(to.p_path, from_stat->st_mode)) {
		warn("mkfifo: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int
copy_special(struct stat *from_stat, int exists)
{
	if (exists && unlink(to.p_path)) {
		warn("unlink: %s", to.p_path);
		return (1);
	}
	if (mknod(to.p_path, from_stat->st_mode, from_stat->st_rdev)) {
		warn("mknod: %s", to.p_path);
		return (1);
	}
	return (pflag ? setfile(from_stat, -1) : 0);
}

int
setfile(struct stat *fs, int fd)
{
	static struct timeval tv[2];
	struct stat ts;
	int rval, gotstat, islink, fdval;

	rval = 0;
	fdval = fd != -1;
	islink = !fdval && S_ISLNK(fs->st_mode);
	fs->st_mode &= S_ISUID | S_ISGID | S_ISVTX |
		       S_IRWXU | S_IRWXG | S_IRWXO;

	TIMESPEC_TO_TIMEVAL(&tv[0], &fs->st_atimespec);
	TIMESPEC_TO_TIMEVAL(&tv[1], &fs->st_mtimespec);
	if (islink ? lutimes(to.p_path, tv) : utimes(to.p_path, tv)) {
		warn("%sutimes: %s", islink ? "l" : "", to.p_path);
		rval = 1;
	}
	if (fdval ? fstat(fd, &ts) :
	    (islink ? lstat(to.p_path, &ts) : stat(to.p_path, &ts))) {
		gotstat = 0;
	} else {
		gotstat = 1;
		ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX |
			      S_IRWXU | S_IRWXG | S_IRWXO;
	}
	/*
	 * Changing the ownership probably won't succeed, unless we're root
	 * or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	 * the mode; current BSD behavior is to remove all setuid bits on
	 * chown.  If chown fails, lose setuid/setgid bits.
	 */
	if (!gotstat || fs->st_uid != ts.st_uid || fs->st_gid != ts.st_gid)
		if (fdval ? fchown(fd, fs->st_uid, fs->st_gid) :
		    (islink ? lchown(to.p_path, fs->st_uid, fs->st_gid) :
		    chown(to.p_path, fs->st_uid, fs->st_gid))) {
			if (errno != EPERM) {
				warn("chown: %s", to.p_path);
				rval = 1;
			}
			fs->st_mode &= ~(S_ISUID | S_ISGID);
		}

	if (!gotstat || fs->st_mode != ts.st_mode)
		if (fdval ? fchmod(fd, fs->st_mode) :
		    (islink ? lchmod(to.p_path, fs->st_mode) :
		    chmod(to.p_path, fs->st_mode))) {
			warn("chmod: %s", to.p_path);
			rval = 1;
		}

	if (!gotstat || fs->st_flags != ts.st_flags)
		if (fdval ?
		    fchflags(fd, fs->st_flags) :
		    (islink ? (errno = ENOSYS) :
		    chflags(to.p_path, fs->st_flags))) {
			warn("chflags: %s", to.p_path);
			rval = 1;
		}

	return (rval);
}

void
usage(void)
{
	fprintf(stderr, "%s\n%s\n",
"usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-pv] source_file target_file",
"       cp [-R [-H | -L | -P]] [-f | -i | -n] [-pv] source_file ... "
"target_directory");
	exit(EX_USAGE);
}
