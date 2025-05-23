/*
 * Copyright (c) 1994 University of Maryland
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 *
 * $FreeBSD: src/usr.sbin/crunch/crunchgen/crunched_main.c,v 1.6.6.2 2002/08/09 02:42:02 gshapiro Exp $
 * $DragonFly: src/usr.sbin/crunch/crunchgen/crunched_main.c,v 1.3 2003/11/16 14:10:44 eirikn Exp $
 */
/*
 * crunched_main.c - main program for crunched binaries, it branches to a
 * 	particular subprogram based on the value of argv[0].  Also included
 *	is a little program invoked when the crunched binary is called via
 *	its EXECNAME.  This one prints out the list of compiled-in binaries,
 *	or calls one of them based on argv[1].   This allows the testing of
 *	the crunched binary without creating all the links.
 */
#include <stdio.h>
#include <string.h>

struct stub {
    char *name;
    int (*f)();
};

extern struct stub entry_points[];

int
main(int argc, char **argv, char **envp)
{
    char *slash, *basename;
    struct stub *ep;

    if(argv[0] == NULL || *argv[0] == '\0')
	crunched_usage();

    slash = strrchr(argv[0], '/');
    basename = slash? slash+1 : argv[0];

    for(ep=entry_points; ep->name != NULL; ep++)
	if(!strcmp(basename, ep->name)) break;

    if(ep->name)
	return ep->f(argc, argv, envp);
    else {
	fprintf(stderr, "%s: %s not compiled in\n", EXECNAME, basename);
	crunched_usage();
    }
}


int
crunched_here(char *path)
{
    char *slash, *basename;
    struct stub *ep;

    slash = strrchr(path, '/');
    basename = slash? slash+1 : path;

    for(ep=entry_points; ep->name != NULL; ep++)
	if(!strcmp(basename, ep->name))
	    return 1;
    return 0;
}


int
crunched_main(int argc, char **argv, char **envp)
{
    struct stub *ep;
    int columns, len;

    if(argc <= 1)
	crunched_usage();

    return main(--argc, ++argv, envp);
}


int
crunched_usage(void)
{
    int columns, len;
    struct stub *ep;

    fprintf(stderr, "usage: %s <prog> <args> ..., where <prog> is one of:\n",
	    EXECNAME);
    columns = 0;
    for(ep=entry_points; ep->name != NULL; ep++) {
	len = strlen(ep->name) + 1;
	if(columns+len < 80)
	    columns += len;
	else {
	    fprintf(stderr, "\n");
	    columns = len;
	}
	fprintf(stderr, " %s", ep->name);
    }
    fprintf(stderr, "\n");
    exit(1);
}

/* end of crunched_main.c */

