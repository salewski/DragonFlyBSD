/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie          <paul@vix.com>          uunet!decwrl!vixie!paul
 *
 * $FreeBSD: src/usr.sbin/cron/lib/compat.c,v 1.6.2.1 2000/12/11 01:03:31 obrien Exp $
 * $DragonFly: src/usr.sbin/cron/lib/compat.c,v 1.4 2004/12/18 22:48:03 swildner Exp $
 */

/* vix 30dec93 [broke this out of misc.c - see RCS log for history]
 * vix 15jan87 [added TIOCNOTTY, thanks csg@pyramid]
 */


#include "cron.h"
#ifdef NEED_GETDTABLESIZE
# include <limits.h>
#endif
#if defined(NEED_SETSID) && defined(BSD)
# include <sys/ioctl.h>
#endif
#include <errno.h>
#include <paths.h>


/* the code does not depend on any of vfork's
 * side-effects; it just uses it as a quick
 * fork-and-exec.
 */
#ifdef NEED_VFORK
PID_T
vfork(void)
{
	return (fork());
}
#endif


#ifdef NEED_STRDUP
char *
strdup(char *str)
{
	char	*temp;

	if ((temp = malloc(strlen(str) + 1)) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	strcpy(temp, str);
	return temp;
}
#endif


#ifdef NEED_STRERROR
char *
strerror(int error)
{
	extern char *sys_errlist[];
	extern int sys_nerr;
	static char buf[32];

	if ((error <= sys_nerr) && (error > 0)) {
		return sys_errlist[error];
	}

	sprintf(buf, "Unknown error: %d", error);
	return buf;
}
#endif


#ifdef NEED_STRCASECMP
int
strcasecmp(char *left, char *right)
{
	while (*left && (MkLower(*left) == MkLower(*right))) {
		left++;
		right++;
	}
	return MkLower(*left) - MkLower(*right);
}
#endif


#ifdef NEED_SETSID
int
setsid(void)
{
	int	newpgrp;
# if defined(BSD)
	int	fd;
#  if defined(POSIX)
	newpgrp = setpgid((pid_t)0, getpid());
#  else
	newpgrp = setpgrp(0, getpid());
#  endif
	if ((fd = open(_PATH_TTY, 2)) >= 0)
	{
		ioctl(fd, TIOCNOTTY, (char*)0);
		close(fd);
	}
# else /*BSD*/
	newpgrp = setpgrp();

	close(STDIN);	open(_PATH_DEVNULL, 0);
	close(STDOUT);	open(_PATH_DEVNULL, 1);
	close(STDERR);	open(_PATH_DEVNULL, 2);
# endif /*BSD*/
	return newpgrp;
}
#endif /*NEED_SETSID*/


#ifdef NEED_GETDTABLESIZE
int
getdtablesize(void)
{
#ifdef _SC_OPEN_MAX
	return sysconf(_SC_OPEN_MAX);
#else
	return _POSIX_OPEN_MAX;
#endif
}
#endif


#ifdef NEED_FLOCK
/* The following flock() emulation snarfed intact *) from the HP-UX
 * "BSD to HP-UX porting tricks" maintained by
 * system@alchemy.chem.utoronto.ca (System Admin (Mike Peterson))
 * from the version "last updated: 11-Jan-1993"
 * Snarfage done by Jarkko Hietaniemi <Jarkko.Hietaniemi@hut.fi>
 * *) well, almost, had to K&R the function entry, HPUX "cc"
 * does not grok ANSI function prototypes */
 
/*
 * flock (fd, operation)
 *
 * This routine performs some file locking like the BSD 'flock'
 * on the object described by the int file descriptor 'fd',
 * which must already be open.
 *
 * The operations that are available are:
 *
 * LOCK_SH  -  get a shared lock.
 * LOCK_EX  -  get an exclusive lock.
 * LOCK_NB  -  don't block (must be ORed with LOCK_SH or LOCK_EX).
 * LOCK_UN  -  release a lock.
 *
 * Return value: 0 if lock successful, -1 if failed.
 *
 * Note that whether the locks are enforced or advisory is
 * controlled by the presence or absence of the SETGID bit on
 * the executable.
 *
 * Note that there is no difference between shared and exclusive
 * locks, since the 'lockf' system call in SYSV doesn't make any
 * distinction.
 *
 * The file "<sys/file.h>" should be modified to contain the definitions
 * of the available operations, which must be added manually (see below
 * for the values).
 */

/* this code has been reformatted by vixie */

int
flock(int fd, int operation)
{
	int i;

	switch (operation) {
	case LOCK_SH:		/* get a shared lock */
	case LOCK_EX:		/* get an exclusive lock */
		i = lockf (fd, F_LOCK, 0);
		break;

	case LOCK_SH|LOCK_NB:	/* get a non-blocking shared lock */
	case LOCK_EX|LOCK_NB:	/* get a non-blocking exclusive lock */
		i = lockf (fd, F_TLOCK, 0);
		if (i == -1)
			if ((errno == EAGAIN) || (errno == EACCES))
				errno = EWOULDBLOCK;
		break;

	case LOCK_UN:		/* unlock */
		i = lockf (fd, F_ULOCK, 0);
		break;
 
	default:		/* can't decipher operation */
		i = -1;
		errno = EINVAL;
		break;
	}
 
	return (i);
}
#endif /*NEED_FLOCK*/


#ifdef NEED_SETENV
int
setenv(char *name, char *value, int overwrite)
{
	char *tmp;

	if (overwrite && getenv(name))
		return -1;

	if (!(tmp = malloc(strlen(name) + strlen(value) + 2))) {
		errno = ENOMEM;
		return -1;
	}

	sprintf(tmp, "%s=%s", name, value);
	return putenv(tmp);	/* intentionally orphan 'tmp' storage */
}
#endif
