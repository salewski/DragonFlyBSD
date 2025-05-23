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
 *	@(#)unistd.h	8.12 (Berkeley) 4/27/95
 * $FreeBSD: src/include/unistd.h,v 1.35.2.10 2002/04/15 12:52:28 nectar Exp $
 * $DragonFly: src/include/unistd.h,v 1.11 2005/01/14 04:21:16 dillon Exp $
 */

#ifndef _UNISTD_H_
#define	_UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/unistd.h>

#define	STDIN_FILENO	0	/* standard input file descriptor */
#define	STDOUT_FILENO	1	/* standard output file descriptor */
#define	STDERR_FILENO	2	/* standard error file descriptor */

#ifndef NULL
#define	NULL		0	/* null pointer constant */
#endif

#ifndef _POSIX_SOURCE
#define	F_ULOCK		0	/* unlock locked section */
#define	F_LOCK		1	/* lock a section for exclusive use */
#define	F_TLOCK		2	/* test and lock a section for exclusive use */
#define	F_TEST		3	/* test a section for locks by other procs */
#endif

__BEGIN_DECLS
void	 _exit(int) __dead2;
int	 access(const char *, int);
unsigned int	 alarm(unsigned int);
int	 chdir(const char *);
int	 chown(const char *, uid_t, gid_t);
int	 close(int);
int	 dup(int);
int	 dup2(int, int);
int	 execl(const char *, const char *, ...);
int	 execle(const char *, const char *, ...);
int	 execlp(const char *, const char *, ...);
int	 execv(const char *, char * const *);
int	 execve(const char *, char * const *, char * const *);
int	 execvp(const char *, char * const *);
pid_t	 fork(void);
long	 fpathconf(int, int);
char	*getcwd(char *, size_t);
gid_t	 getegid(void);
uid_t	 geteuid(void);
gid_t	 getgid(void);
int	 getgroups(int, gid_t []);
char	*getlogin(void);
pid_t	 getpgrp(void);
pid_t	 getpid(void);
pid_t	 getppid(void);
uid_t	 getuid(void);
int	 isatty(int);
int	 link(const char *, const char *);
#ifndef _LSEEK_DECLARED
#define	_LSEEK_DECLARED
off_t	 lseek(int, off_t, int);
#endif
long	 pathconf(const char *, int);
int	 pause(void);
int	 pipe(int *);
ssize_t	 read(int, void *, size_t);
int	 rmdir(const char *);
int	 setgid(gid_t);
int	 setpgid(pid_t, pid_t);
void	 setproctitle(const char *, ...) __printf0like(1, 2);
pid_t	 setsid(void);
int	 setuid(uid_t);
unsigned int	 sleep(unsigned int);
long	 sysconf(int);
pid_t	 tcgetpgrp(int);
int	 tcsetpgrp(int, pid_t);
char	*ttyname(int);
int	 unlink(const char *);
ssize_t	 write(int, const void *, size_t);

extern char *optarg;			/* getopt(3) external variables */
extern int optind, opterr, optopt;
int	 getopt(int, char * const [], const char *);

#ifndef	_POSIX_SOURCE
#ifdef	__STDC__
struct timeval;				/* select(2) */
#endif
int	 acct(const char *);
int	 async_daemon (void);
int	 brk(const void *);
int	 chroot(const char *);
size_t	 confstr(int, char *, size_t);
char	*crypt(const char *, const char *);
const char *crypt_get_format(void);
int	 crypt_set_format(const char *);
int	 des_cipher(const char *, char *, long, int);
int	 des_setkey(const char *);
int	 encrypt(char *, int);
void	 endusershell(void);
int	 exect(const char *, char * const *, char * const *);
int	 fchdir(int);
int	 fchown(int, uid_t, gid_t);
char	*fflagstostr(u_long);
int	 fsync(int);
#ifndef _FTRUNCATE_DECLARED
#define	_FTRUNCATE_DECLARED
int	 ftruncate(int, off_t);
#endif
int	 getdomainname(char *, int);
int	 getdtablesize(void);
int	 getgrouplist(const char *, gid_t, gid_t *, int *);
long	 gethostid(void);
int	 gethostname(char *, int);
int	 getlogin_r(char *, int);
mode_t	 getmode(const void *, mode_t);
int	 getpagesize(void) __pure2;
char	*getpass(const char *);
int	 getpeereid(int, uid_t *, gid_t *);
int	 getpgid(pid_t _pid);
int	 getresgid(gid_t *, gid_t *, gid_t *);
int	 getresuid(uid_t *, uid_t *, uid_t *);
int	 getsid(pid_t _pid);
char	*getusershell(void);
char	*getwd(char *);				/* obsoleted by getcwd() */
int	 initgroups(const char *, int);
int	 iruserok(unsigned long, int, const char *, const char *);
int	 iruserok_sa(const void *, int, int, const char *, const char *);
int	 issetugid(void);
int	 lchown(const char *, uid_t, gid_t);
int	 lockf(int, int, off_t);
char	*mkdtemp(char *);
int	 mknod(const char *, mode_t, dev_t);
int	 mkstemp(char *);
int	 mkstemps(char *, int);
char	*mktemp(char *);
int	 nfssvc(int, void *);
int	 nice(int);
ssize_t	 pread(int, void *, size_t, off_t);
int	 profil(char *, size_t, vm_offset_t, int);
ssize_t	 pwrite(int, const void *, size_t, off_t);
int	 rcmd(char **, int, const char *, const char *, const char *, int *);
int	 rcmd_af(char **, int, const char *, const char *, const char *, int *,
		 int);
int	 rcmdsh(char **, int, const char *, const char *, const char *,
		const char *);
char	*re_comp(const char *);
int	 re_exec(const char *);
int	 readlink(const char *, char *, int);
int	 reboot(int);
int	 revoke(const char *);
pid_t	 rfork(int);
pid_t	 rfork_thread(int, void *, int (*) (void *), void *);
int	 rresvport(int *);
int	 rresvport_af(int *, int);
int	 ruserok(const char *, int, const char *, const char *);
void	*sbrk(intptr_t);
int	 select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int	 setdomainname(const char *, int);
int	 setegid(gid_t);
int	 seteuid(uid_t);
int	 setgroups(int, const gid_t *);
void	 sethostid(long);
int	 sethostname(const char *, int);
int	 setkey(const char *);
int	 setlogin(const char *);
void	*setmode(const char *);
int	 setpgrp(pid_t _pid, pid_t _pgrp); /* obsoleted by setpgid() */
int	 setregid(gid_t, gid_t);
int	 setresgid(gid_t, gid_t, gid_t);
int	 setresuid(uid_t, uid_t, uid_t);
int	 setreuid(uid_t, uid_t);
int	 setrgid(gid_t);
int	 setruid(uid_t);
void	 setusershell(void);
int	 strtofflags(char **, u_long *, u_long *);
int	 swapon(const char *);
int	 symlink(const char *, const char *);
void	 sync(void);
int	 syscall(int, ...);
off_t	 __syscall(quad_t, ...);
#ifndef _TRUNCATE_DECLARED
#define	_TRUNCATE_DECLARED
int	 truncate(const char *, off_t);
#endif
int	 ttyslot(void);
unsigned int	 ualarm(unsigned int, unsigned int);
int	 umtx_sleep(volatile const int *, int , int);
int	 umtx_wakeup(volatile const int *, int);
int	 undelete(const char *);
int	 unwhiteout(const char *);
int	 usleep(unsigned int);
void	*valloc(size_t);			/* obsoleted by malloc() */
pid_t	 vfork(void);
int	 varsym_set(int, const char *, const char *);
int	 varsym_get(int, const char *, char *, int);
int	 varsym_list(int, char *, int, int *);

extern char *suboptarg;			/* getsubopt(3) external variable */
int	 getsubopt(char **, char * const *, char **);
#endif /* !_POSIX_SOURCE */
extern int optreset;			/* getopt(3) external variable */
__END_DECLS

#endif /* !_UNISTD_H_ */
