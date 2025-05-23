/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 * 
 * $FreeBSD: src/usr.sbin/jail/jail.c,v 1.5.2.2 2003/05/08 13:04:24 maxim Exp $
 * $DragonFly: src/usr.sbin/jail/jail.c,v 1.4 2004/12/18 22:48:03 swildner Exp $
 * 
 */

#include <sys/param.h>
#include <sys/jail.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <grp.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	usage(void);

int
main(int argc, char **argv)
{
	login_cap_t *lcap = NULL;
	struct jail j;
	struct passwd *pwd = NULL;
	struct in_addr in;
	gid_t groups[NGROUPS];
	int ch, ngroups;
	char *username;

	username = NULL;

	while ((ch = getopt(argc, argv, "u:")) != -1)
		switch (ch) {
		case 'u':
			username = optarg;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;
	if (argc < 4)
		usage();

	if (username != NULL) {
		pwd = getpwnam(username);
		if (pwd == NULL)
			err(1, "getpwnam: %s", username);
		lcap = login_getpwclass(pwd);
		if (lcap == NULL)
			err(1, "getpwclass: %s", username);
		ngroups = NGROUPS;
		if (getgrouplist(username, pwd->pw_gid, groups, &ngroups) != 0)
			err(1, "getgrouplist: %s", username);
	}
	if (chdir(argv[0]) != 0)
		err(1, "chdir: %s", argv[0]);
	memset(&j, 0, sizeof(j));
	j.version = 0;
	j.path = argv[0];
	j.hostname = argv[1];
	if (inet_aton(argv[2], &in) == 0)
		errx(1, "Could not make sense of ip-number: %s", argv[2]);
	j.ip_number = ntohl(in.s_addr);
	if (jail(&j) != 0)
		err(1, "jail");
	if (username != NULL) {
		if (setgroups(ngroups, groups) != 0)
			err(1, "setgroups");
		if (setgid(pwd->pw_gid) != 0)
			err(1, "setgid");
		if (setusercontext(lcap, pwd, pwd->pw_uid,
		    LOGIN_SETALL & ~LOGIN_SETGROUP) != 0)
			err(1, "setusercontext");
		login_close(lcap);
	}
	if (execv(argv[3], argv + 3) != 0)
		err(1, "execv: %s", argv[3]);
	exit (0);
}

static void
usage(void)
{

	fprintf(stderr, "%s\n",
	    "Usage: jail [-u username] path hostname ip-number command ...");
	exit(1);
}
