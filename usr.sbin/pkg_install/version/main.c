/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jeremy D. Lea.
 * 11 May 2002
 *
 * This is the version module. Based on pkg_version.pl by Bruce A. Mah.
 *
 * $FreeBSD: src/usr.sbin/pkg_install/version/main.c,v 1.4 2005/01/11 11:23:59 ru Exp $
 * $DragonFly: src/usr.sbin/pkg_install/version/Attic/main.c,v 1.2 2005/03/08 19:11:30 joerg Exp $
 */

#include "lib.h"
#include "version.h"
#include <err.h>

static char Options[] = "dhl:L:qs:XtTv";

char	*LimitChars = NULL;
char	*PreventChars = NULL;
char	*MatchName = NULL;
Boolean RegexExtended = FALSE;

static void usage(void);

int
main(int argc, char **argv)
{
    int ch, cmp = 0;

    if (argc == 4 && !strcmp(argv[1], "-t")) {
	cmp = version_cmp(argv[2], argv[3]);
	printf(cmp > 0 ? ">\n" : (cmp < 0 ? "<\n" : "=\n"));
	exit(0);
    }
    else if (argc == 4 && !strcmp(argv[1], "-T")) {
	cmp = version_match(argv[3], argv[2]);
	exit(cmp == 1 ? 0 : 1);
    }
    else while ((ch = getopt(argc, argv, Options)) != -1) {
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'l':
	    LimitChars = optarg;
	    break;

	case 'L':
	    PreventChars = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 's':
	    MatchName = optarg;
	    break;

	case 't':
	    errx(2, "Invalid -t usage.");
	    break;

	case 'T':
	    errx(2, "Invalid -T usage.");
	    break;

	case 'X':
	    RegexExtended = TRUE;
	    break;

	case 'h':
	case '?':
	default:
	    usage();
	    break;
	}
    }

    argc -= optind;
    argv += optind;

    return pkg_perform(argv);
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n",
	"usage: pkg_version [-hqv] [-l limchar] [-L limchar] [[-X] -s string] index",
	"       pkg_version -t v1 v2",
	"       pkg_version -T name pattern");
    exit(1);
}
