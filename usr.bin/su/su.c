/*
 * Copyright (c) 1988, 1993, 1994
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
 * @(#) Copyright (c) 1988, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)su.c	8.3 (Berkeley) 4/2/94
 * $FreeBSD: src/usr.bin/su/su.c,v 1.34.2.4 2002/06/16 21:04:15 nectar Exp $
 * $DragonFly: src/usr.bin/su/su.c,v 1.8 2005/03/14 11:55:33 joerg Exp $
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <libutil.h>

#ifdef LOGIN_CAP
#include <login_cap.h>
#endif

#ifdef	SKEY
#include <skey.h>
#endif

#ifdef KERBEROS5
#include <krb5.h>

static long get_su_principal(krb5_context context, const char *target_user,
    const char *current_user, char **su_principal_name,
    krb5_principal *su_principal);
static long kerberos5(krb5_context context, const char *current_user,
    const char *target_user, krb5_principal su_principal,
    const char *pass);

int use_kerberos5 = 1;
#endif

#ifdef LOGIN_CAP
#define LOGIN_CAP_ARG(x) x
#else
#define LOGIN_CAP_ARG(x)
#endif
#if defined(KERBEROS5)
#define KERBEROS_ARG(x) x
#else
#define KERBEROS_ARG(x)
#endif
#define COMMON_ARG(x) x
#define ARGSTR	"-" COMMON_ARG("flm") LOGIN_CAP_ARG("c:") KERBEROS_ARG("K")

char   *ontty(void);
int	chshell(char *);
static void usage(void);

extern char **environ;

int
main(int argc, char **argv)
{
	struct passwd *pwd;
#ifdef WHEELSU
	char *targetpass;
	int iswheelsu;
#endif /* WHEELSU */
	const char *p, *user, *shell = NULL;
	const char **nargv, **np;
	char **g, **cleanenv, *username, *entered_pass;
	struct group *gr;
	uid_t ruid;
	gid_t gid;
	int asme, ch, asthem, fastlogin, prio, i;
	enum { UNSET, YES, NO } iscsh = UNSET;
#ifdef LOGIN_CAP
	login_cap_t *lc;
	char *class=NULL;
	int setwhat;
#endif
#if defined(KERBEROS5)
	char *k;
#endif
#ifdef KERBEROS5
	char *su_principal_name, *ccname;
	krb5_context context;
	krb5_principal su_principal;
#endif
	char shellbuf[MAXPATHLEN];

#ifdef WHEELSU
	iswheelsu =
#endif /* WHEELSU */
	asme = asthem = fastlogin = 0;
	user = "root";
	while((ch = getopt(argc, argv, ARGSTR)) != -1) 
		switch((char)ch) {
#if defined(KERBEROS5)
		case 'K':
			use_kerberos5 = 0;
			break;
#endif
		case 'f':
			fastlogin = 1;
			break;
		case '-':
		case 'l':
			asme = 0;
			asthem = 1;
			break;
		case 'm':
			asme = 1;
			asthem = 0;
			break;
#ifdef LOGIN_CAP
		case 'c':
			class = optarg;
			break;
#endif
		case '?':
		default:
			usage();
		}

	if (optind < argc && strcmp(argv[optind], "-") == 0) {
		asme = 0;
		asthem = 1;
		optind++;
	}

	if (optind < argc)
		user = argv[optind++];

	if (strlen(user) > MAXLOGNAME - 1) {
		fprintf(stderr, "su: username too long.\n");
		exit(1);
	}
		
	if (user == NULL)
		usage();

	if ((nargv = malloc (sizeof (char *) * (argc + 4))) == NULL) {
	    errx(1, "malloc failure");
	}

	nargv[argc + 3] = NULL;
	for (i = argc; i >= optind; i--)
	    nargv[i + 3] = argv[i];
	np = &nargv[i + 3];

	argv += optind;

#if defined(KERBEROS5)
	k = auth_getval("auth_list");
	if (k && !strstr(k, "kerberos")) {
	    use_kerberos5 = 0;
	}
	su_principal_name = NULL;
	su_principal = NULL;
	if (krb5_init_context(&context) != 0)
		use_kerberos5 = 0;
#endif
	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
	if (errno)
		prio = 0;
	setpriority(PRIO_PROCESS, 0, -2);
	openlog("su", LOG_CONS, 0);

	/* get current login name and shell */
	ruid = getuid();
	username = getlogin();
	if (username == NULL || (pwd = getpwnam(username)) == NULL ||
	    pwd->pw_uid != ruid)
		pwd = getpwuid(ruid);
	if (pwd == NULL)
		errx(1, "who are you?");
	username = strdup(pwd->pw_name);
	gid = pwd->pw_gid;
	if (username == NULL)
		err(1, NULL);
	if (asme) {
		if (pwd->pw_shell != NULL && *pwd->pw_shell != '\0') {
			/* copy: pwd memory is recycled */
			shell = strncpy(shellbuf,  pwd->pw_shell, sizeof shellbuf);
			shellbuf[sizeof shellbuf - 1] = '\0';
		} else {
			shell = _PATH_BSHELL;
			iscsh = NO;
		}
	}

	/* get target login information, default to root */
	if ((pwd = getpwnam(user)) == NULL) {
		errx(1, "unknown login: %s", user);
	}
#ifdef LOGIN_CAP
	if (class==NULL) {
		lc = login_getpwclass(pwd);
	} else {
		if (ruid)
			errx(1, "only root may use -c");
		lc = login_getclass(class);
		if (lc == NULL)
			errx(1, "unknown class: %s", class);
	}
#endif

#ifdef WHEELSU
	targetpass = strdup(pwd->pw_passwd);
#endif /* WHEELSU */

	if (ruid) {
#ifdef KERBEROS5
		if (use_kerberos5) {
			if (get_su_principal(context, user, username,
			    &su_principal_name, &su_principal) != 0 ||
			    !krb5_kuserok(context, su_principal, user)) {
				warnx("kerberos5: not in %s's ACL.", user);
				use_kerberos5 = 0;
			}
		}
#endif
		/*
		 * Only allow those with pw_gid==0 or those listed in
		 * group zero to su to root.  If group zero entry is
		 * missing or empty, then allow anyone to su to root.
		 * iswheelsu will only be set if the user is EXPLICITLY
		 * listed in group zero.
		 */
		if (pwd->pw_uid == 0 && (gr = getgrgid((gid_t)0)) &&
		    gr->gr_mem && *(gr->gr_mem)) {
			for (g = gr->gr_mem;; ++g) {
				if (!*g) {
					if (gid == 0)
						break;
					else
						errx(1,
			     "you are not in the correct group (%s) to su %s.",
						    gr->gr_name,
						    user);
				}
				if (strcmp(username, *g) == 0) {
#ifdef WHEELSU
					iswheelsu = 1;
#endif /* WHEELSU */
					break;
				}
			}
		}
		/* if target requires a password, verify it */
		if (*pwd->pw_passwd) {
#ifdef	SKEY
#ifdef WHEELSU
			if (iswheelsu) {
				pwd = getpwnam(username);
			}
#endif /* WHEELSU */
			entered_pass = skey_getpass("Password:", pwd, 1);
			if (!(!strcmp(pwd->pw_passwd, skey_crypt(entered_pass,
			    pwd->pw_passwd, pwd, 1))
#ifdef WHEELSU
			      || (iswheelsu && !strcmp(targetpass,
				  crypt(entered_pass, targetpass)))
#endif /* WHEELSU */
			      )) {
#else
			entered_pass = getpass("Password:");
			if (strcmp(pwd->pw_passwd, crypt(entered_pass,
			    pwd->pw_passwd))) {
#endif
#ifdef KERBEROS5
				if (use_kerberos5 && kerberos5(context,
				    username, user, su_principal,
				    entered_pass) == 0)
					goto authok;
#endif
				fprintf(stderr, "Sorry\n");
				syslog(LOG_AUTH|LOG_WARNING,
				    "BAD SU %s to %s%s", username, user,
				    ontty());
				exit(1);
			}
#if defined(KERBEROS5)
		authok:
			;
#endif
#ifdef WHEELSU
			if (iswheelsu) {
				pwd = getpwnam(user);
			}
#endif /* WHEELSU */
		}
		if (pwd->pw_expire && time(NULL) >= pwd->pw_expire) {
			fprintf(stderr, "Sorry - account expired\n");
			syslog(LOG_AUTH|LOG_WARNING,
				"BAD SU %s to %s%s", username,
				user, ontty());
			exit(1);
		}
	}

	if (asme) {
		/* if asme and non-standard target shell, must be root */
		if (!chshell(pwd->pw_shell) && ruid)
			errx(1, "permission denied (shell).");
	} else if (pwd->pw_shell && *pwd->pw_shell) {
		shell = pwd->pw_shell;
		iscsh = UNSET;
	} else {
		shell = _PATH_BSHELL;
		iscsh = NO;
	}

	/* if we're forking a csh, we want to slightly muck the args */
	if (iscsh == UNSET) {
		p = strrchr(shell, '/');
		if (p)
			++p;
		else
			p = shell;
		if ((iscsh = strcmp(p, "csh") ? NO : YES) == NO)
		    iscsh = strcmp(p, "tcsh") ? NO : YES;
	}

	setpriority(PRIO_PROCESS, 0, prio);

#ifdef LOGIN_CAP
	/* Set everything now except the environment & umask */
	setwhat = LOGIN_SETUSER|LOGIN_SETGROUP|LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
	/*
	 * Don't touch resource/priority settings if -m has been
	 * used or -l and -c hasn't, and we're not su'ing to root.
	 */
        if ((asme || (!asthem && class == NULL)) && pwd->pw_uid)
		setwhat &= ~(LOGIN_SETPRIORITY|LOGIN_SETRESOURCES);
	if (setusercontext(lc, pwd, pwd->pw_uid, setwhat) < 0)
		err(1, "setusercontext");
#else
	/* set permissions */
	if (setgid(pwd->pw_gid) < 0)
		err(1, "setgid");
	if (initgroups(user, pwd->pw_gid))
		errx(1, "initgroups failed");
	if (setuid(pwd->pw_uid) < 0)
		err(1, "setuid");
#endif

	if (!asme) {
		if (asthem) {
			p = getenv("TERM");
#ifdef KERBEROS5
			ccname = getenv("KRB5CCNAME");
#endif
			if ((cleanenv = calloc(20, sizeof(char*))) == NULL)
				errx(1, "calloc");
			cleanenv[0] = NULL;
			environ = cleanenv;
#ifdef LOGIN_CAP
			/* set the su'd user's environment & umask */
			setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETPATH|LOGIN_SETUMASK|LOGIN_SETENV);
#else
			setenv("PATH", _PATH_DEFPATH, 1);
#endif
			if (p)
				setenv("TERM", p, 1);
#ifdef KERBEROS5
			if (ccname)
				setenv("KRB5CCNAME", ccname, 1);
#endif
			if (chdir(pwd->pw_dir) < 0)
				errx(1, "no directory");
		}
		if (asthem || pwd->pw_uid)
			setenv("USER", pwd->pw_name, 1);
		setenv("HOME", pwd->pw_dir, 1);
		setenv("SHELL", shell, 1);
	}
	if (iscsh == YES) {
		if (fastlogin)
			*np-- = "-f";
		if (asme)
			*np-- = "-m";
	}

	/* csh strips the first character... */
	*np = asthem ? "-su" : iscsh == YES ? "_su" : "su";

	if (ruid != 0)
		syslog(LOG_NOTICE|LOG_AUTH, "%s to %s%s",
		    username, user, ontty());

#ifdef LOGIN_CAP
	login_close(lc);
#endif

	execv(shell, __DECONST(char * const *, np));
	err(1, "%s", shell);
}

static void
usage(void)
{
	fprintf(stderr, "usage: su [-] [-%s] %s[login [args]]\n",
	    KERBEROS_ARG("K") COMMON_ARG("flm"),
#ifdef LOGIN_CAP
	    "[-c class] "
#else
	    ""
#endif
	    );
	exit(1);
}

int
chshell(char *sh)
{
	int  r = 0;
	char *cp;

	setusershell();
	while (!r && (cp = getusershell()) != NULL)
		r = strcmp(cp, sh) == 0;
	endusershell();
	return r;
}

char *
ontty(void)
{
	char *p;
	static char buf[MAXPATHLEN + 4];

	buf[0] = 0;
	p = ttyname(STDERR_FILENO);
	if (p)
		snprintf(buf, sizeof(buf), " on %s", p);
	return (buf);
}

#ifdef KERBEROS5
const char superuser[] = "root";

/* Authenticate using Kerberos 5.
 *   context           -- An initialized krb5_context.
 *   current_user      -- The current username.
 *   target_user       -- The target account name.
 *   su_principal      -- The target krb5_principal.
 *   pass              -- The user's password.
 * Note that a valid keytab in the default location with a host entry
 * must be available.
 * Returns 0 if authentication was successful, or a com_err error code if
 * it was not.
 */
static long
kerberos5(krb5_context context, const char *current_user,
    const char *target_user, krb5_principal su_principal,
    const char *pass)
{
	krb5_creds	 creds;
	krb5_get_init_creds_opt gic_opt;
	krb5_verify_init_creds_opt vic_opt;
	long		 rv;

	krb5_get_init_creds_opt_init(&gic_opt);
	krb5_verify_init_creds_opt_init(&vic_opt);
	rv = krb5_get_init_creds_password(context, &creds, su_principal,
	    pass, NULL, NULL, 0, NULL, &gic_opt);
	if (rv != 0) {
		syslog(LOG_NOTICE|LOG_AUTH, "BAD Kerberos5 SU: %s to %s%s: %s",
		    current_user, target_user, ontty(),
		    krb5_get_err_text(context, rv));
		return (rv);
	}
	krb5_verify_init_creds_opt_set_ap_req_nofail(&vic_opt, 1);
	rv = krb5_verify_init_creds(context, &creds, NULL, NULL, NULL,
	    &vic_opt);
	krb5_free_cred_contents(context, &creds);
	if (rv != 0) {
		syslog(LOG_NOTICE|LOG_AUTH, "BAD Kerberos5 SU: %s to %s%s: %s",
		    current_user, target_user, ontty(),
		    krb5_get_err_text(context, rv));
		return (rv);
	}
	return (0);
}

/* Determine the target principal given the current user and the target user.
 *   context           -- An initialized krb5_context.
 *   target_user       -- The target username.
 *   current_user      -- The current username.
 *   su_principal_name -- (out) The target principal name.
 *   su_principal      -- (out) The target krb5_principal.
 *
 * When target_user is `root', the su_principal will be a `root
 * instance', e.g. `luser/root@REA.LM'.  Otherwise, the su_principal
 * will simply be the current user's default principal name.  Note that
 * in any case, if KRB5CCNAME is set and a credentials cache exists, the
 * principal name found there will be the `starting point', rather than
 * the current_user parameter.
 *
 * Returns 0 for success, or a com_err error code on failure.
 */
static long
get_su_principal(krb5_context context, const char *target_user,
    const char *current_user, char **su_principal_name,
    krb5_principal *su_principal)
{
	krb5_principal	 default_principal;
	krb5_ccache	 ccache;
	char		*principal_name, *ccname, *p;
	long		 rv;
	uid_t		 euid, ruid;

	*su_principal = NULL;
	default_principal = NULL;
	/* Lower privs while messing about with the credentials
	 * cache.
	 */
	ruid = getuid();
	euid = geteuid();
	rv = seteuid(getuid());
	if (rv != 0)
		return (errno);
	p = getenv("KRB5CCNAME");
	if (p != NULL)
		ccname = strdup(p);
	else {
		asprintf(&ccname, "%s%lu", KRB5_DEFAULT_CCROOT,
		    (unsigned long)ruid);
	}
	if (ccname == NULL)
		return (errno);
	rv = krb5_cc_resolve(context, ccname, &ccache);
	free(ccname);
	if (rv == 0) {
		rv = krb5_cc_get_principal(context, ccache,
		    &default_principal);
		krb5_cc_close(context, ccache);
		if (rv != 0)
			default_principal = NULL; /* just to be safe */
	}
	rv = seteuid(euid);
	if (rv != 0)
		return (errno);
	if (default_principal == NULL) {
		rv = krb5_make_principal(context, &default_principal, NULL,
		    current_user, NULL);
		if (rv != 0) {
			warnx("Could not determine default principal name.");
			return (rv);
		}
	}
	/* Now that we have some principal, if the target account is
	 * `root', then transform it into a `root' instance, e.g.
	 * `user@REA.LM' -> `user/root@REA.LM'.
	 */
	rv = krb5_unparse_name(context, default_principal, &principal_name);
	krb5_free_principal(context, default_principal);
	if (rv != 0) {
		warnx("krb5_unparse_name: %s", krb5_get_err_text(context, rv));
		return (rv);
	}
	if (strcmp(target_user, superuser) == 0) {
		p = strrchr(principal_name, '@');
		if (p == NULL) {
			warnx("malformed principal name `%s'", principal_name);
			free(principal_name);
			return (rv);
		}
		*p++ = '\0';
		asprintf(su_principal_name, "%s/%s@%s", principal_name,
		    superuser, p);
		free(principal_name);
	} else 
		*su_principal_name = principal_name;
	if (*su_principal_name == NULL)
		return errno;
	rv = krb5_parse_name(context, *su_principal_name, &default_principal);
	if (rv != 0) {
		warnx("krb5_parse_name `%s': %s", *su_principal_name,
		    krb5_get_err_text(context, rv));
		free(*su_principal_name);
		return (rv);
	}
	*su_principal = default_principal;
	return 0;
}

#endif
