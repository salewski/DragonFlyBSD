/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the create module.
 *
 * $FreeBSD: src/usr.sbin/pkg_install/create/main.c,v 1.35 2004/06/29 18:56:59 eik Exp $
 * $DragonFly: src/usr.sbin/pkg_install/create/Attic/main.c,v 1.4 2004/07/30 04:46:12 dillon Exp $
 */

#include <err.h>
#include "lib.h"
#include "create.h"

static char Options[] = "YNOhjvyzf:p:P:C:c:d:i:I:k:K:r:t:X:D:m:s:S:o:b:";

char	*Prefix		= NULL;
char	*Comment        = NULL;
char	*Desc		= NULL;
char	*SrcDir		= NULL;
char	*BaseDir	= NULL;
char	*Display	= NULL;
char	*Install	= NULL;
char	*PostInstall	= NULL;
char	*DeInstall	= NULL;
char	*PostDeInstall	= NULL;
char	*Contents	= NULL;
char	*Require	= NULL;
char	*ExcludeFrom	= NULL;
char	*Mtree		= NULL;
char	*Pkgdeps	= NULL;
char	*Conflicts	= NULL;
char	*Origin		= NULL;
char	*InstalledPkg	= NULL;
char	PlayPen[FILENAME_MAX];
int	Dereference	= FALSE;
int	PlistOnly	= FALSE;
enum zipper	Zipper	= GZIP;

static void usage(void);

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start, *tmp;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'N':
	    AutoAnswer = NO;
	    break;

	case 'Y':
	    AutoAnswer = YES;
	    break;

	case 'O':
	    PlistOnly = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 's':
	    SrcDir = optarg;
	    break;

	case 'S':
	    BaseDir = optarg;
	    break;

	case 'f':
	    Contents = optarg;
	    break;

	case 'C':
	    Conflicts = optarg;
	    break;

	case 'c':
	    Comment = optarg;
	    break;

	case 'd':
	    Desc = optarg;
	    break;

	case 'i':
	    Install = optarg;
	    break;

	case 'I':
	    PostInstall = optarg;
	    break;

	case 'k':
	    DeInstall = optarg;
	    break;

	case 'K':
	    PostDeInstall = optarg;
	    break;

	case 'r':
	    Require = optarg;
	    break;

	case 't':
	    strlcpy(PlayPen, optarg, sizeof(PlayPen));
	    break;

	case 'X':
	    ExcludeFrom = optarg;
	    break;

	case 'h':
	    Dereference = TRUE;
	    break;

	case 'D':
	    Display = optarg;
	    break;

	case 'm':
	    Mtree = optarg;
	    break;

	case 'P':
	    Pkgdeps = optarg;
	    break;

	case 'o':
	    Origin = optarg;
	    break;

	case 'y':
	case 'j':
	    Zipper = BZIP2;
	    break;

	case 'z':
	    Zipper = GZIP;
	    break;

	case 'b':
	    InstalledPkg = optarg;
	    while ((tmp = strrchr(optarg, (int)'/')) != NULL) {
		*tmp++ = '\0';
		/*
		 * If character after the '/' is alphanumeric, then we've
		 * found the package name.  Otherwise we've come across
		 * a trailing '/' and need to continue our quest.
		 */
		if (isalpha(*tmp)) {
		    InstalledPkg = tmp;
		    break;
		}
	    }
	    break;

	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if ((pkgs == start) && (InstalledPkg == NULL))
	warnx("missing package name"), usage();
    *pkgs = NULL;
    if ((start[0] != NULL) && (start[1] != NULL)) {
	warnx("only one package name allowed ('%s' extraneous)", start[1]);
	usage();
    }
    if (start[0] == NULL)
	start[0] = InstalledPkg;
    if (!pkg_perform(start)) {
	if (Verbose)
	    warnx("package creation failed");
	return 1;
    }
    else
	return 0;
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
"usage: pkg_create [-YNOhvyz] [-P pkgs] [-C conflicts] [-p prefix] ",
"                  [-i iscript] [-I piscript] [-k dscript] [-K pdscript] ",
"                  [-r rscript] [-t template] [-X excludefile] ",
"                  [-D displayfile] [-m mtreefile] [-o origin] ",
"                  [-s srcdir] [-S basedir] ",
"                  -c comment -d description -f packlist pkg-filename",
"       pkg_create [-YNhvyz] -b pkg-name [pkg-filename]");
    exit(1);
}
