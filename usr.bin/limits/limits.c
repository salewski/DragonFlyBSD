/*-
 * Copyright (c) 1997 by
 * David L. Nugent <davidn@blaze.net.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is permitted provided this notation is included.
 * 4. Absolutely no warranty of function or purpose is made by the authors.
 * 5. Modifications may be freely made to this file providing the above
 *    conditions are met.
 *
 * Display/change(+runprogram)/eval resource limits.
 *
 * $FreeBSD: src/usr.bin/limits/limits.c,v 1.7.2.3 2003/05/22 09:26:57 sheldonh Exp $
 * $DragonFly: src/usr.bin/limits/limits.c,v 1.4 2005/01/12 01:20:26 cpressey Exp $
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <login_cap.h>
#include <sys/time.h>
#include <sys/resource.h>

enum
{
    SH_NONE,
    SH_SH,      /* sh */
    SH_CSH,     /* csh */
    SH_BASH,    /* gnu bash */
    SH_TCSH,    /* tcsh */
    SH_KSH,     /* (pd)ksh */
    SH_ZSH,     /* zsh */
    SH_RC,      /* rc or es */
    SH_NUMBER
};


/* eval emitter for popular shells.
 * Why aren't there any standards here? Most shells support either
 * the csh 'limit' or sh 'ulimit' command, but each varies just
 * enough that they aren't very compatible from one to the other.
 */
static struct {
    const char * name;	    /* Name of shell */
    const char * inf;	    /* Name used for 'unlimited' resource */
    const char * cmd;	    /* Intro text */
    const char * hard;	    /* Hard limit text */
    const char * soft;	    /* Soft limit text */
    const char * both;	    /* Hard+Soft limit text */
    struct {
	const char * pfx;
	const char * sfx;
	int divisor;
    } lprm[RLIM_NLIMITS];
} shellparm[] =
{
    { "", "infinity", "Resource limits%s%s:\n", "-max", "-cur", "",
      {
	  { "  cputime%-4s      %8s", " secs\n",  1    },
	  { "  filesize%-4s     %8s", " kb\n",    1024 },
	  { "  datasize%-4s     %8s", " kb\n",    1024 },
	  { "  stacksize%-4s    %8s", " kb\n",    1024 },
	  { "  coredumpsize%-4s %8s", " kb\n",    1024 },
	  { "  memoryuse%-4s    %8s", " kb\n",    1024 },
	  { "  memorylocked%-4s %8s", " kb\n",    1024 },
	  { "  maxprocesses%-4s %8s", "\n",       1    },
	  { "  openfiles%-4s    %8s", "\n",       1    },
	  { "  sbsize%-4s       %8s", " bytes\n", 1    },
	  { "  vmemoryuse%-4s   %8s", " kb\n",    1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "  posixlocks%-4s   %8s", "\n",	  1    },
#endif
      }
    },
    { "sh", "unlimited", "", " -H", " -S", "",
      {
	  { "ulimit%s -t %s", ";\n",  1    },
	  { "ulimit%s -f %s", ";\n",  512  },
	  { "ulimit%s -d %s", ";\n",  1024 },
	  { "ulimit%s -s %s", ";\n",  1024 },
	  { "ulimit%s -c %s", ";\n",  512  },
	  { "ulimit%s -m %s", ";\n",  1024 },
	  { "ulimit%s -l %s", ";\n",  1024 },
	  { "ulimit%s -u %s", ";\n",  1    },
	  { "ulimit%s -n %s", ";\n",  1    },
	  { "ulimit%s -b %s", ";\n",  1    },
	  { "ulimit%s -v %s", ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "ulimit%s -k %s", ";\n",  1    },
#endif
      }
    },
    { "csh", "unlimited", "", " -h", "", NULL,
      {
	  { "limit%s cputime %s",      ";\n",  1    },
	  { "limit%s filesize %s",     ";\n",  1024 },
	  { "limit%s datasize %s",     ";\n",  1024 },
	  { "limit%s stacksize %s",    ";\n",  1024 },
	  { "limit%s coredumpsize %s", ";\n",  1024 },
	  { "limit%s memoryuse %s",    ";\n",  1024 },
	  { "limit%s memorylocked %s", ";\n",  1024 },
	  { "limit%s maxproc %s",      ";\n",  1    },
	  { "limit%s openfiles %s",    ";\n",  1    },
	  { "limit%s sbsize %s",       ";\n",  1    },
	  { "limit%s vmemoryuse %s",   ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "limit%s advlocks %s",     ";\n",  1    },
#endif
      }
    },
    { "bash|bash2", "unlimited", "", " -H", " -S", "",
      {
	  { "ulimit%s -t %s", ";\n",  1    },
	  { "ulimit%s -f %s", ";\n",  1024 },
	  { "ulimit%s -d %s", ";\n",  1024 },
	  { "ulimit%s -s %s", ";\n",  1024 },
	  { "ulimit%s -c %s", ";\n",  1024 },
	  { "ulimit%s -m %s", ";\n",  1024 },
	  { "ulimit%s -l %s", ";\n",  1024 },
	  { "ulimit%s -u %s", ";\n",  1    },
	  { "ulimit%s -n %s", ";\n",  1    },
	  { "ulimit%s -b %s", ";\n",  1    },
	  { "ulimit%s -v %s", ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "ulimit%s -k %s", ";\n",  1    },
#endif
      }
    },
    { "tcsh", "unlimited", "", " -h", "", NULL,
      {
	  { "limit%s cputime %s",      ";\n",  1    },
	  { "limit%s filesize %s",     ";\n",  1024 },
	  { "limit%s datasize %s",     ";\n",  1024 },
	  { "limit%s stacksize %s",    ";\n",  1024 },
	  { "limit%s coredumpsize %s", ";\n",  1024 },
	  { "limit%s memoryuse %s",    ";\n",  1024 },
	  { "limit%s memorylocked %s", ";\n",  1024 },
	  { "limit%s maxproc %s",      ";\n",  1    },
	  { "limit%s descriptors %s",  ";\n",  1    },
	  { "limit%s sbsize %s",       ";\n",  1    },
	  { "limit%s vmemoryuse %s",   ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "limit%s advlocks %s",      ";\n",  1   },
#endif
      }
    },
    { "ksh|pdksh", "unlimited", "", " -H", " -S", "",
      {
	  { "ulimit%s -t %s", ";\n",  1    },
	  { "ulimit%s -f %s", ";\n",  512  },
	  { "ulimit%s -d %s", ";\n",  1024 },
	  { "ulimit%s -s %s", ";\n",  1024 },
	  { "ulimit%s -c %s", ";\n",  512  },
	  { "ulimit%s -m %s", ";\n",  1024 },
	  { "ulimit%s -l %s", ";\n",  1024 },
	  { "ulimit%s -p %s", ";\n",  1    },
	  { "ulimit%s -n %s", ";\n",  1    },
	  { "ulimit%s -b %s", ";\n",  1    },
	  { "ulimit%s -v %s", ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "ulimit%s -k %s", ";\n",  1    },
#endif
      }
    },
    { "zsh", "unlimited", "", " -H", " -S", "",
      {
	  { "ulimit%s -t %s", ";\n",  1    },
	  { "ulimit%s -f %s", ";\n",  512  },
	  { "ulimit%s -d %s", ";\n",  1024 },
	  { "ulimit%s -s %s", ";\n",  1024 },
	  { "ulimit%s -c %s", ";\n",  512  },
	  { "ulimit%s -m %s", ";\n",  1024 },
	  { "ulimit%s -l %s", ";\n",  1024 },
	  { "ulimit%s -u %s", ";\n",  1    },
	  { "ulimit%s -n %s", ";\n",  1    },
	  { "ulimit%s -b %s", ";\n",  1    },
	  { "ulimit%s -v %s", ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "ulimit%s -k %s", ";\n",  1    },
#endif
      }
    },
    { "rc|es", "unlimited", "", " -h", "", NULL,
      {
	  { "limit%s cputime %s",      ";\n",  1    },
	  { "limit%s filesize %s",     ";\n",  1024 },
	  { "limit%s datasize %s",     ";\n",  1024 },
	  { "limit%s stacksize %s",    ";\n",  1024 },
	  { "limit%s coredumpsize %s", ";\n",  1024 },
	  { "limit%s memoryuse %s",    ";\n",  1024 },
	  { "limit%s lockedmemory %s", ";\n",  1024 },
	  { "limit%s processes %s",    ";\n",  1    },
	  { "limit%s descriptors %s",  ";\n",  1    },
	  { "limit%s sbsize %s",       ";\n",  1    },
	  { "limit%s vmemoryuse %s",   ";\n",  1024 },
#ifdef RLIMIT_POSIXLOCKS
	  { "limit%s advlocks %s",     ";\n",  1    },
#endif
      }
    },
    { NULL, NULL, NULL, NULL, NULL, NULL, {} }
};

static struct {
    const char * cap;
    rlim_t (*func)(login_cap_t *, const char *, rlim_t, rlim_t);
} resources[RLIM_NLIMITS] = {
    { "cputime",	login_getcaptime },
    { "filesize",	login_getcapsize },
    { "datasize",	login_getcapsize },
    { "stacksize",	login_getcapsize },
    { "coredumpsize",	login_getcapsize },
    { "memoryuse",	login_getcapsize },
    { "memorylocked",	login_getcapsize },
    { "maxproc",	login_getcapnum  },
    { "openfiles",	login_getcapnum  },
    { "sbsize",		login_getcapsize },
    { "vmemoryuse",	login_getcapsize },
    { "posixlocks",	login_getcapnum  },
};

/*
 * One letter for each resource levels.
 * NOTE: There is a dependancy on the corresponding
 * letter index being equal to the resource number.
 * If sys/resource.h defines are changed, this needs
 * to be modified accordingly!
 */

#define RCS_STRING  "tfdscmlunbvk"

static rlim_t resource_num(int which, int ch, const char *str);
static void usage(void);
static int getshelltype(void);
static void print_limit(rlim_t limit, unsigned divisor, const char *inf,
			const char *pfx, const char *sfx, const char *which);
extern char **environ;

static const char rcs_string[] = RCS_STRING;

int
main(int argc, char *argv[])
{
    char *p, *cls = NULL;
    char *cleanenv[1];
    struct passwd * pwd = NULL;
    int rcswhich, shelltype;
    int i, num_limits = 0;
    int ch, doeval = 0, doall = 0;
    login_cap_t * lc = NULL;
    enum { ANY=0, SOFT=1, HARD=2, BOTH=3, DISPLAYONLY=4 } type = ANY;
    enum { RCSUNKNOWN=0, RCSSET=1, RCSSEL=2 } todo = RCSUNKNOWN;
    int which_limits[RLIM_NLIMITS];
    rlim_t set_limits[RLIM_NLIMITS];
    struct rlimit limits[RLIM_NLIMITS];

    /* init resource tables */
    for (i = 0; i < RLIM_NLIMITS; i++) {
	which_limits[i] = 0; /* Don't set/display any */
	set_limits[i] = RLIM_INFINITY;
	/* Get current resource values */
	if (getrlimit(i, &limits[i]) < 0) {
		limits[i].rlim_cur = -1;
		limits[i].rlim_max = -1;
	}
    }

    optarg = NULL;
    while ((ch = getopt(argc, argv, ":EeC:U:BSHabc:d:f:k:l:m:n:s:t:u:v:")) != -1) {
	switch(ch) {
	case 'a':
	    doall = 1;
	    break;
	case 'E':
	    environ = cleanenv;
	    cleanenv[0] = NULL;
	    break;
	case 'e':
	    doeval = 1;
	    break;
	case 'C':
	    cls = optarg;
	    break;
	case 'U':
	    if ((pwd = getpwnam(optarg)) == NULL) {
		if (!isdigit(*optarg) ||
		    (pwd = getpwuid(atoi(optarg))) == NULL) {
		    warnx("invalid user `%s'", optarg);
		    usage();
		}
	    }
	    break;
	case 'H':
	    type = HARD;
	    break;
	case 'S':
	    type = SOFT;
	    break;
	case 'B':
	    type = SOFT|HARD;
	    break;
	default:
	case ':': /* Without arg */
	    if ((p = strchr(rcs_string, optopt)) != NULL) {
		rcswhich = p - rcs_string;

		/*
		 * Backwards compatibility with earlier kernel which might
		 * support fewer resources.
		 */
		if (rcswhich >= RLIM_NLIMITS) {
		    usage();
		    break;
		}
		if (optarg && *optarg == '-') { /* 'arg' is actually a switch */
		    --optind;		/* back one arg, and make arg NULL */
		    optarg = NULL;
		}
		todo = optarg == NULL ? RCSSEL : RCSSET;
		if (type == ANY)
		    type = BOTH;
		which_limits[rcswhich] = optarg ? type : DISPLAYONLY;
		set_limits[rcswhich] = resource_num(rcswhich, optopt, optarg);
		num_limits++;
		break;
	    }
	    /* FALLTHRU */
	case '?':
	    usage();
	}
	optarg = NULL;
    }

    /* If user was specified, get class from that */
    if (pwd != NULL)
	lc = login_getpwclass(pwd);
    else if (cls != NULL && *cls != '\0') {
	lc = login_getclassbyname(cls, NULL);
	if (lc == NULL || strcmp(cls, lc->lc_class) != 0)
	    fprintf(stderr, "login class '%s' non-existent, using %s\n",
		    cls, lc?lc->lc_class:"current settings");
    }

    /* If we have a login class, update resource table from that */
    if (lc != NULL) {
	for (rcswhich = 0; rcswhich < RLIM_NLIMITS; rcswhich++) {
	    char str[40];
	    rlim_t val;

	    /* current value overridden by resourcename or resourcename-cur */
	    sprintf(str, "%s-cur", resources[rcswhich].cap);
	    val = resources[rcswhich].func(lc, resources[rcswhich].cap, limits[rcswhich].rlim_cur, limits[rcswhich].rlim_cur);
	    limits[rcswhich].rlim_cur = resources[rcswhich].func(lc, str, val, val);
	    /* maximum value overridden by resourcename or resourcename-max */
	    sprintf(str, "%s-max", resources[rcswhich].cap);
	    val = resources[rcswhich].func(lc, resources[rcswhich].cap, limits[rcswhich].rlim_max, limits[rcswhich].rlim_max);
	    limits[rcswhich].rlim_max = resources[rcswhich].func(lc, str, val, val);
	}
    }

    /* now, let's determine what we wish to do with all this */

    argv += optind;

    /* If we're setting limits or doing an eval (ie. we're not just
     * displaying), then check that hard limits are not lower than
     * soft limits, and force rasing the hard limit if we need to if
     * we are raising the soft limit, or lower the soft limit if we
     * are lowering the hard limit.
     */
    if ((*argv || doeval) && getuid() == 0) {

	for (rcswhich = 0; rcswhich < RLIM_NLIMITS; rcswhich++) {
	    if (limits[rcswhich].rlim_max != RLIM_INFINITY) {
		if (limits[rcswhich].rlim_cur == RLIM_INFINITY) {
		    limits[rcswhich].rlim_max = RLIM_INFINITY;
		    which_limits[rcswhich] |= HARD;
		} else if (limits[rcswhich].rlim_cur > limits[rcswhich].rlim_max) {
		    if (which_limits[rcswhich] == SOFT) {
			limits[rcswhich].rlim_max = limits[rcswhich].rlim_cur;
			which_limits[rcswhich] |= HARD;
		    }  else if (which_limits[rcswhich] == HARD) {
			limits[rcswhich].rlim_cur = limits[rcswhich].rlim_max;
			which_limits[rcswhich] |= SOFT;
		    } else {
			/* else.. if we're specifically setting both to
			 * silly values, then let it error out.
			 */
		    }
		}
	    }
	}
    }

    /* See if we've overridden anything specific on the command line */
    if (num_limits && todo == RCSSET) {
	for (rcswhich = 0; rcswhich < RLIM_NLIMITS; rcswhich++) {
	    if (which_limits[rcswhich] & HARD)
		limits[rcswhich].rlim_max = set_limits[rcswhich];
	    if (which_limits[rcswhich] & SOFT)
		limits[rcswhich].rlim_cur = set_limits[rcswhich];
	}
    }

    /* If *argv is not NULL, then we are being asked to
     * (perhaps) set environment variables and run a program
     */
    if (*argv) {
	if (doeval) {
	    warnx("-e cannot be used with `cmd' option");
	    usage();
	}

	login_close(lc);

	/* set leading environment variables, like eval(1) */
	while (*argv && (p = strchr(*argv, '=')))
	    (void)setenv(*argv++, ++p, 1);

	/* Set limits */
	for (rcswhich = 0; rcswhich < RLIM_NLIMITS; rcswhich++) {
	    if (doall || num_limits == 0 || which_limits[rcswhich] != 0)
		if (setrlimit(rcswhich, &limits[rcswhich]) == -1)
		    err(1, "setrlimit %s", resources[rcswhich].cap);
	}

	if (*argv == NULL)
	    usage();

	execvp(*argv, argv);
	err(1, "%s", *argv);
    }

    shelltype = doeval ? getshelltype() : SH_NONE;

    if (type == ANY) /* Default to soft limits */
	type = SOFT;

    /* Display limits */
    printf(shellparm[shelltype].cmd,
	   lc ? " for class " : " (current)",
	   lc ? lc->lc_class : "");

    for (rcswhich = 0; rcswhich < RLIM_NLIMITS; rcswhich++) {
	if (doall || num_limits == 0 || which_limits[rcswhich] != 0) {
	    if (which_limits[rcswhich] == ANY || which_limits[rcswhich])
		which_limits[rcswhich] = type;
	    if (shellparm[shelltype].lprm[rcswhich].pfx) {
		if (shellparm[shelltype].both && limits[rcswhich].rlim_cur == limits[rcswhich].rlim_max) {
		    print_limit(limits[rcswhich].rlim_max,
				shellparm[shelltype].lprm[rcswhich].divisor,
				shellparm[shelltype].inf,
				shellparm[shelltype].lprm[rcswhich].pfx,
				shellparm[shelltype].lprm[rcswhich].sfx,
				shellparm[shelltype].both);
		} else {
		    if (which_limits[rcswhich] & HARD) {
			print_limit(limits[rcswhich].rlim_max,
				    shellparm[shelltype].lprm[rcswhich].divisor,
				    shellparm[shelltype].inf,
				    shellparm[shelltype].lprm[rcswhich].pfx,
				    shellparm[shelltype].lprm[rcswhich].sfx,
				    shellparm[shelltype].hard);
		    }
		    if (which_limits[rcswhich] & SOFT) {
			print_limit(limits[rcswhich].rlim_cur,
				    shellparm[shelltype].lprm[rcswhich].divisor,
				    shellparm[shelltype].inf,
				    shellparm[shelltype].lprm[rcswhich].pfx,
				    shellparm[shelltype].lprm[rcswhich].sfx,
				    shellparm[shelltype].soft);
		    }
		}
	    }
	}
    }

    login_close(lc);
    exit(EXIT_SUCCESS);
}


static void
usage(void)
{
    (void)fprintf(stderr,
"usage: limits [-C class|-U user] [-eaSHBE] [-bcdflmnstuvk [val]] [[name=val ...] cmd]\n");
    exit(EXIT_FAILURE);
}

static void
print_limit(rlim_t limit, unsigned divisor, const char * inf, const char * pfx, const char * sfx, const char * which)
{
    char numbr[64];

    if (limit == RLIM_INFINITY)
	strcpy(numbr, inf);
    else
	sprintf(numbr, "%qd", (quad_t)((limit + divisor/2) / divisor));
    printf(pfx, which, numbr);
    printf(sfx, which);

}


static rlim_t
resource_num(int which, int ch, const char *str)
{
    rlim_t res = RLIM_INFINITY;

    if (str != NULL &&
	!(strcasecmp(str, "inf") == 0 ||
	  strcasecmp(str, "infinity") == 0 ||
	  strcasecmp(str, "unlimit") == 0 ||
	  strcasecmp(str, "unlimited") == 0)) {
	const char * s = str;
	char *e;

	switch (which) {
	case RLIMIT_CPU:	/* time values */
	    errno = 0;
	    res = 0;
	    while (*s) {
		rlim_t tim = strtoq(s, &e, 0);
		if (e == NULL || e == s || errno)
		    break;
		switch (*e++) {
		case 0:		   	/* end of string */
		    e--;
		default:
		case 's': case 'S':	/* seconds */
		    break;
		case 'm': case 'M':	/* minutes */
		    tim *= 60L;
		    break;
		case 'h': case 'H':	/* hours */
		    tim *= (60L * 60L);
		    break;
		case 'd': case 'D':	/* days */
		    tim *= (60L * 60L * 24L);
		    break;
		case 'w': case 'W':	/* weeks */
		    tim *= (60L * 60L * 24L * 7L);
		case 'y': case 'Y':	/* Years */
		    tim *= (60L * 60L * 24L * 365L);
		}
		s = e;
		res += tim;
	    }
	    break;
	case RLIMIT_FSIZE: /* Size values */
	case RLIMIT_DATA:
	case RLIMIT_STACK:
	case RLIMIT_CORE:
	case RLIMIT_RSS:
	case RLIMIT_MEMLOCK:
	case RLIMIT_VMEM:
	    errno = 0;
	    res = 0;
	    while (*s) {
		rlim_t mult, tim = strtoq(s, &e, 0);
		if (e == NULL || e == s || errno)
		    break;
		switch (*e++) {
		case 0:	/* end of string */
		    e--;
		default:
		    mult = 1;
		    break;
		case 'b': case 'B':	/* 512-byte blocks */
		    mult = 512;
		    break;
		case 'k': case 'K':	/* 1024-byte Kilobytes */
		    mult = 1024;
		    break;
		case 'm': case 'M':	/* 1024-k kbytes */
		    mult = 1024 * 1024;
		    break;
		case 'g': case 'G':	/* 1Gbyte */
		    mult = 1024 * 1024 * 1024;
		    break;
		case 't': case 'T':	/* 1TBte */
		    mult = 1024LL * 1024LL * 1024LL * 1024LL;
		    break;
		}
		s = e;
		res += (tim * mult);
	    }
	    break;
	case RLIMIT_NPROC:
	case RLIMIT_NOFILE:
	    res = strtoq(s, &e, 0);
	    s = e;
	    break;
	}
	if (*s) {
	    warnx("invalid value -%c `%s'", ch, str);
	    usage();
	}
    }
    return res;
}


static int
getshellbyname(const char * shell)
{
    int i;
    const char * q;
    const char * p = strrchr(shell, '/');

    p = p ? ++p : shell;
    for (i = 0; (q = shellparm[i].name) != NULL; i++) {
	while (*q) {
	    int j = strcspn(q, "|");

	    if (j == 0)
		break;
	    if (strncmp(p, q, j) == 0)
		return i;
	    if (*(q += j))
		++q;
	}
    }
    return SH_SH;
}


/*
 * Determine the type of shell our parent process is
 * This is quite tricky, not 100% reliable and probably
 * not nearly as thorough as it should be. Basically, this
 * is a "best guess" only, but hopefully will work in
 * most cases.
 */

static int
getshelltype(void)
{
    pid_t ppid = getppid();

    if (ppid != 1) {
	FILE * fp;
	struct stat st;
	char procdir[MAXPATHLEN], buf[128];
	int l = sprintf(procdir, "/proc/%ld/", (long)ppid);
	char * shell = getenv("SHELL");

	if (shell != NULL && stat(shell, &st) != -1) {
	    struct stat st1;

	    strcpy(procdir+l, "file");
	    /* $SHELL is actual shell? */
	    if (stat(procdir, &st1) != -1 && memcmp(&st, &st1, sizeof st) == 0)
		return getshellbyname(shell);
	}
	strcpy(procdir+l, "status");
	if (stat(procdir, &st) == 0 && (fp = fopen(procdir, "r")) != NULL) {
	    char * p = fgets(buf, sizeof buf, fp)==NULL ? NULL : strtok(buf, " \t");
	    fclose(fp);
	    if (p != NULL)
		return getshellbyname(p);
	}
    }
    return SH_SH;
}

