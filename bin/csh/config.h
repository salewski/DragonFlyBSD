/* config.h.  Generated automatically by configure.  */
/*
 * config.h -- configure various defines for tcsh
 *
 * All source files should #include this FIRST.
 *
 * Edit this to match your system type.
 */

/* $FreeBSD: src/bin/csh/config.h,v 1.5.2.3 2001/10/08 21:11:00 mp Exp $ */
/* $DragonFly: src/bin/csh/config.h,v 1.3 2004/01/26 18:12:54 drhodus Exp $ */

#ifndef _h_config
#define _h_config
/****************** System dependant compilation flags ****************/
/*
 * POSIX	This system supports IEEE Std 1003.1-1988 (POSIX).
 */
#define POSIX

/*
 * POSIXJOBS	This system supports the optional IEEE Std 1003.1-1988 (POSIX)
 *		job control facilities.
 */
#define POSIXJOBS

/*
 * POSIXSIGS	Use the POSIX signal facilities to emulate BSD signals.
 */
/* #undef POSIXSIGS */

/*
 * VFORK	This machine has a vfork().  
 *		It used to be that for job control to work, this define
 *		was mandatory. This is not the case any more.
 *		If you think you still need it, but you don't have vfork, 
 *		define this anyway and then do #define vfork fork.  
 *		I do this anyway on a Sun because of yellow pages brain damage,
 *		[should not be needed under 4.1]
 *		and on the iris4d cause	SGI's fork is sufficiently "virtual" 
 *		that vfork isn't necessary.  (Besides, SGI's vfork is weird).
 *		Note that some machines eg. rs6000 have a vfork, but not
 *		with the berkeley semantics, so we cannot use it there either.
 */
#define VFORK

/*
 * BSDJOBS	You have BSD-style job control (both process groups and
 *		a tty that deals correctly
 */
#define BSDJOBS

/*
 * BSDSIGS	You have 4.2-style signals, rather than USG style.
 *		Note: POSIX systems should not define this unless they
 *		have sigvec() and friends (ie: 4.3BSD-RENO, HP-UX).
 */
#define BSDSIGS

/*
 * BSDTIMES	You have BSD-style process time stuff (like rusage)
 *		This may or may not be true.  For example, Apple Unix
 *		(OREO) has BSDJOBS and BSDSIGS but not BSDTIMES.
 */
#define BSDTIMES

/*
 * BSDLIMIT	You have BSD-style resource limit stuff (getrlimit/setrlimit)
 */
#define BSDLIMIT

/*
 * BSDNICE	Your system uses setpriority() instead of nice, to
 *		change a processes scheduling priority
 */
#define BSDNICE

/*
 * TERMIO	You have struct termio instead of struct sgttyb.
 * 		This is usually the case for SYSV systems, where
 *		BSD uses sgttyb. POSIX systems should define this
 *		anyway, even though they use struct termios.
 */
#define TERMIO

/*
 * SYSVREL	Your machine is SYSV based (HPUX, A/UX)
 *		NOTE: don't do this if you are on a Pyramid -- tcsh is
 *		built in a BSD universe.
 *		Set SYSVREL to 1, 2, 3, or 4, depending the version of System V
 *		you are running. Or set it to 0 if you are not SYSV based
 */
#define SYSVREL	0

/*
 * YPBUGS	Work around Sun YP bugs that cause expansion of ~username
 *		to send command output to /dev/null
 */
/* #undef YPBUGS */

/*
 * SIGVOID	Define this if your signal handlers return void.  On older
 *		systems, signal returns int, but on newer ones, it returns void.
 */
#define SIGVOID 

/*
 * HAVEDUP2	Define this if your system supports dup2().
 */
#define HAVEDUP2

/*
 * UTHOST	Does the utmp file have a host field?
 */
#define UTHOST

/*
 * DIRENT	Your system has <dirent.h> instead of <sys/dir.h>
 */
#define DIRENT
/****************** local defines *********************/
/****************** configurable hacks ****************/

#include <stdlib.h>

/* have been moved to config_f.h */
#include "config_f.h"

#if defined(__DragonFly__)
#define NLS_BUGS
/* we want to use the system malloc when we install as /bin/csh */
#define SYSMALLOC
#define BSD_STYLE_COLORLS
#endif

#if defined(__bsdi__)
/*
 * _PATH_TCSHELL      if you've change the installation location (vix)
 */
# if _BSDI_VERSION >= 199701
#  define _PATH_TCSHELL "/bin/tcsh"
/* #  undef SYSMALLOC */
#  define SYSMALLOC
# else
#  define _PATH_TCSHELL "/usr/contrib/bin/tcsh"
# endif

# undef NLS
# undef NLS_CATALOGS

#elif defined(__APPLE__)
# define SYSMALLOC

#else
# define NLS_CATALOGS
#endif

#define KANJI
#define DSPMBYTE

#endif /* _h_config */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */
