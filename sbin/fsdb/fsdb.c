/*	$NetBSD: fsdb.c,v 1.2 1995/10/08 23:18:10 thorpej Exp $	*/

/*
 *  Copyright (c) 1995 John T. Kohl
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sbin/fsdb/fsdb.c,v 1.13.2.3 2002/03/20 13:39:02 joerg Exp $
 * $DragonFly: src/sbin/fsdb/fsdb.c,v 1.6 2003/11/01 17:15:59 drhodus Exp $
 */

#include <sys/param.h>
#include <sys/time.h>
#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <histedit.h>
#include <pwd.h>
#include <string.h>

#include <vfs/ufs/dinode.h>
#include <vfs/ufs/dir.h>
#include <vfs/ufs/fs.h>

#include "fsdb.h"
#include "fsck.h"

static void usage(void);
int cmdloop(void);

static void 
usage(void)
{
	fprintf(stderr, "usage: fsdb [-d] [-f] [-r] fsname\n");
	exit(1);
}

int returntosingle = 0;
char nflag = 0;

/*
 * We suck in lots of fsck code, and just pick & choose the stuff we want.
 *
 * fsreadfd is set up to read from the file system, fswritefd to write to
 * the file system.
 */
int
main(int argc, char **argv)
{
	int ch, rval;
	char *fsys = NULL;

	while (-1 != (ch = getopt(argc, argv, "fdr"))) {
		switch (ch) {
		case 'f':
			/* The -f option is left for historical
			 * reasons and has no meaning.
			 */
			break;
		case 'd':
			debug++;
			break;
		case 'r':
			nflag++; /* "no" in fsck, readonly for us */
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();
	else
		fsys = argv[0];

	if (!setup(fsys))
		errx(1, "cannot set up file system `%s'", fsys);
	printf("%s file system `%s'\nLast Mounted on %s\n",
	       nflag? "Examining": "Editing", fsys, sblock.fs_fsmnt);
	rval = cmdloop();
	if (!nflag) {
		sblock.fs_clean = 0;	/* mark it dirty */
		sbdirty();
		ckfini(0);
		printf("*** FILE SYSTEM MARKED DIRTY\n");
		printf("*** BE SURE TO RUN FSCK TO CLEAN UP ANY DAMAGE\n");
		printf("*** IF IT WAS MOUNTED, RE-MOUNT WITH -u -o reload\n");
	}
	exit(rval);
}

#define CMDFUNC(func) int func (int argc, char **argv)
#define CMDFUNCSTART(func) int func(int argc, char **argv)

CMDFUNC(helpfn);
CMDFUNC(focus);				/* focus on inode */
CMDFUNC(active);			/* print active inode */
CMDFUNC(blocks);			/* print blocks for active inode */
CMDFUNC(focusname);			/* focus by name */
CMDFUNC(zapi);				/* clear inode */
CMDFUNC(uplink);			/* incr link */
CMDFUNC(downlink);			/* decr link */
CMDFUNC(linkcount);			/* set link count */
CMDFUNC(quit);				/* quit */
CMDFUNC(ls);				/* list directory */
CMDFUNC(rm);				/* remove name */
CMDFUNC(ln);				/* add name */
CMDFUNC(newtype);			/* change type */
CMDFUNC(chmode);			/* change mode */
CMDFUNC(chlen);				/* change length */
CMDFUNC(chaflags);			/* change flags */
CMDFUNC(chgen);				/* change generation */
CMDFUNC(chowner);			/* change owner */
CMDFUNC(chgroup);			/* Change group */
CMDFUNC(back);				/* pop back to last ino */
CMDFUNC(chmtime);			/* Change mtime */
CMDFUNC(chctime);			/* Change ctime */
CMDFUNC(chatime);			/* Change atime */
CMDFUNC(chinum);			/* Change inode # of dirent */
CMDFUNC(chname);			/* Change dirname of dirent */

struct cmdtable cmds[] = {
	{ "help", "Print out help", 1, 1, FL_RO, helpfn },
	{ "?", "Print out help", 1, 1, FL_RO, helpfn },
	{ "inode", "Set active inode to INUM", 2, 2, FL_RO, focus },
	{ "clri", "Clear inode INUM", 2, 2, FL_WR, zapi },
	{ "lookup", "Set active inode by looking up NAME", 2, 2, FL_RO, focusname },
	{ "cd", "Set active inode by looking up NAME", 2, 2, FL_RO, focusname },
	{ "back", "Go to previous active inode", 1, 1, FL_RO, back },
	{ "active", "Print active inode", 1, 1, FL_RO, active },
	{ "print", "Print active inode", 1, 1, FL_RO, active },
	{ "blocks", "Print block numbers of active inode", 1, 1, FL_RO, blocks },
	{ "uplink", "Increment link count", 1, 1, FL_WR, uplink },
	{ "downlink", "Decrement link count", 1, 1, FL_WR, downlink },
	{ "linkcount", "Set link count to COUNT", 2, 2, FL_WR, linkcount },
	{ "ls", "List current inode as directory", 1, 1, FL_RO, ls },
	{ "rm", "Remove NAME from current inode directory", 2, 2, FL_WR, rm },
	{ "del", "Remove NAME from current inode directory", 2, 2, FL_WR, rm },
	{ "ln", "Hardlink INO into current inode directory as NAME", 3, 3, FL_WR, ln },
	{ "chinum", "Change dir entry number INDEX to INUM", 3, 3, FL_WR, chinum },
	{ "chname", "Change dir entry number INDEX to NAME", 3, 3, FL_WR, chname },
	{ "chtype", "Change type of current inode to TYPE", 2, 2, FL_WR, newtype },
	{ "chmod", "Change mode of current inode to MODE", 2, 2, FL_WR, chmode },
	{ "chlen", "Change length of current inode to LENGTH", 2, 2, FL_WR, chlen },
	{ "chown", "Change owner of current inode to OWNER", 2, 2, FL_WR, chowner },
	{ "chgrp", "Change group of current inode to GROUP", 2, 2, FL_WR, chgroup },
	{ "chflags", "Change flags of current inode to FLAGS", 2, 2, FL_WR, chaflags },
	{ "chgen", "Change generation number of current inode to GEN", 2, 2, FL_WR, chgen },
	{ "mtime", "Change mtime of current inode to MTIME", 2, 2, FL_WR, chmtime },
	{ "ctime", "Change ctime of current inode to CTIME", 2, 2, FL_WR, chctime },
	{ "atime", "Change atime of current inode to ATIME", 2, 2, FL_WR, chatime },
	{ "quit", "Exit", 1, 1, FL_RO, quit },
	{ "q", "Exit", 1, 1, FL_RO, quit },
	{ "exit", "Exit", 1, 1, FL_RO, quit },
	{ NULL, 0, 0, 0 },
};

int
helpfn(int argc, char **argv)
{
    register struct cmdtable *cmdtp;

    printf("Commands are:\n%-10s %5s %5s   %s\n",
	   "command", "min argc", "max argc", "what");
    
    for (cmdtp = cmds; cmdtp->cmd; cmdtp++)
	printf("%-10s %5u %5u   %s\n",
	       cmdtp->cmd, cmdtp->minargc, cmdtp->maxargc, cmdtp->helptxt);
    return 0;
}

char *
prompt(EditLine *el)
{
    static char pstring[64];
    snprintf(pstring, sizeof(pstring), "fsdb (inum: %d)> ", curinum);
    return pstring;
}


int
cmdloop(void)
{
    char *line;
    const char *elline;
    int cmd_argc, rval = 0, known;
#define scratch known
    char **cmd_argv;
    struct cmdtable *cmdp;
    History *hist;
    EditLine *elptr;

    curinode = ginode(ROOTINO);
    curinum = ROOTINO;
    printactive(0);

    hist = history_init();
    history(hist, H_EVENT, 100);	/* 100 elt history buffer */

    elptr = el_init("fsdb", stdin, stdout);
    el_set(elptr, EL_EDITOR, "emacs");
    el_set(elptr, EL_PROMPT, prompt);
    el_set(elptr, EL_HIST, history, hist);
    el_source(elptr, NULL);

    while ((elline = el_gets(elptr, &scratch)) != NULL && scratch != 0) {
	if (debug)
	    printf("command `%s'\n", elline);

	history(hist, H_ENTER, elline);

	line = strdup(elline);
	cmd_argv = crack(line, &cmd_argc);
	/*
	 * el_parse returns -1 to signal that it's not been handled
	 * internally.
	 */
	if (el_parse(elptr, cmd_argc, cmd_argv) != -1)
	    continue;
	if (cmd_argc) {
	    known = 0;
	    for (cmdp = cmds; cmdp->cmd; cmdp++) {
		if (!strcmp(cmdp->cmd, cmd_argv[0])) {
		    if ((cmdp->flags & FL_WR) == FL_WR && nflag)
			warnx("`%s' requires write access", cmd_argv[0]),
			    rval = 1;
		    else if (cmd_argc >= cmdp->minargc &&
			cmd_argc <= cmdp->maxargc)
			rval = (*cmdp->handler)(cmd_argc, cmd_argv);
		    else
			rval = argcount(cmdp, cmd_argc, cmd_argv);
		    known = 1;
		    break;
		}
	    }
	    if (!known)
		warnx("unknown command `%s'", cmd_argv[0]), rval = 1;
	} else
	    rval = 0;
	free(line);
	if (rval < 0)
	    return rval;
	if (rval)
	    warnx("rval was %d", rval);
    }
    el_end(elptr);
    history_end(hist);
    return rval;
}

struct dinode *curinode;
ino_t curinum, ocurrent;

#define GETINUM(ac,inum)    inum = strtoul(argv[ac], &cp, 0); \
    if (inum < ROOTINO || inum > maxino || cp == argv[ac] || *cp != '\0' ) { \
	printf("inode %d out of range; range is [%d,%d]\n", \
	       inum, ROOTINO, maxino); \
	return 1; \
    }

/*
 * Focus on given inode number
 */
CMDFUNCSTART(focus)
{
    ino_t inum;
    char *cp;

    GETINUM(1,inum);
    curinode = ginode(inum);
    ocurrent = curinum;
    curinum = inum;
    printactive(0);
    return 0;
}

CMDFUNCSTART(back)
{
    curinum = ocurrent;
    curinode = ginode(curinum);
    printactive(0);
    return 0;
}

CMDFUNCSTART(zapi)
{
    ino_t inum;
    struct dinode *dp;
    char *cp;

    GETINUM(1,inum);
    dp = ginode(inum);
    clearinode(dp);
    inodirty();
    if (curinode)			/* re-set after potential change */
	curinode = ginode(curinum);
    return 0;
}

CMDFUNCSTART(active)
{
    printactive(0);
    return 0;
}

CMDFUNCSTART(blocks)
{
    printactive(1);
    return 0;
}

CMDFUNCSTART(quit)
{
    return -1;
}

CMDFUNCSTART(uplink)
{
    if (!checkactive())
	return 1;
    printf("inode %d link count now %d\n", curinum, ++curinode->di_nlink);
    inodirty();
    return 0;
}

CMDFUNCSTART(downlink)
{
    if (!checkactive())
	return 1;
    printf("inode %d link count now %d\n", curinum, --curinode->di_nlink);
    inodirty();
    return 0;
}

const char *typename[] = {
    "unknown",
    "fifo",
    "char special",
    "unregistered #3",
    "directory",
    "unregistered #5",
    "blk special",
    "unregistered #7",
    "regular",
    "unregistered #9",
    "symlink",
    "unregistered #11",
    "socket",
    "unregistered #13",
    "whiteout",
};
    
int slot;

int
scannames(struct inodesc *idesc)
{
	register struct direct *dirp = idesc->id_dirp;

	printf("slot %d ino %d reclen %d: %s, `%.*s'\n",
	       slot++, dirp->d_ino, dirp->d_reclen, typename[dirp->d_type],
	       dirp->d_namlen, dirp->d_name);
	return (KEEPON);
}

CMDFUNCSTART(ls)
{
    struct inodesc idesc;
    checkactivedir();			/* let it go on anyway */

    slot = 0;
    idesc.id_number = curinum;
    idesc.id_func = scannames;
    idesc.id_type = DATA;
    idesc.id_fix = IGNORE;
    ckinode(curinode, &idesc);
    curinode = ginode(curinum);

    return 0;
}

int findino(struct inodesc *idesc); /* from fsck */
static int dolookup(char *name);

static int
dolookup(char *name)
{
    struct inodesc idesc;

    if (!checkactivedir())
	    return 0;
    idesc.id_number = curinum;
    idesc.id_func = findino;
    idesc.id_name = name;
    idesc.id_type = DATA;
    idesc.id_fix = IGNORE;
    if (ckinode(curinode, &idesc) & FOUND) {
	curinum = idesc.id_parent;
	curinode = ginode(curinum);
	printactive(0);
	return 1;
    } else {
	warnx("name `%s' not found in current inode directory", name);
	return 0;
    }
}

CMDFUNCSTART(focusname)
{
    char *p, *val;

    if (!checkactive())
	return 1;

    ocurrent = curinum;
    
    if (argv[1][0] == '/') {
	curinum = ROOTINO;
	curinode = ginode(ROOTINO);
    } else {
	if (!checkactivedir())
	    return 1;
    }
    for (p = argv[1]; p != NULL;) {
	while ((val = strsep(&p, "/")) != NULL && *val == '\0');
	if (val) {
	    printf("component `%s': ", val);
	    fflush(stdout);
	    if (!dolookup(val)) {
		curinode = ginode(curinum);
		return(1);
	    }
	}
    }
    return 0;
}

CMDFUNCSTART(ln)
{
    ino_t inum;
    int rval;
    char *cp;

    GETINUM(1,inum);

    if (!checkactivedir())
	return 1;
    rval = makeentry(curinum, inum, argv[2]);
    if (rval)
	printf("Ino %d entered as `%s'\n", inum, argv[2]);
    else
	printf("could not enter name? weird.\n");
    curinode = ginode(curinum);
    return rval;
}

CMDFUNCSTART(rm)
{
    int rval;

    if (!checkactivedir())
	return 1;
    rval = changeino(curinum, argv[1], 0);
    if (rval & ALTERED) {
	printf("Name `%s' removed\n", argv[1]);
	return 0;
    } else {
	printf("could not remove name? weird.\n");
	return 1;
    }
}

long slotcount, desired;

int
chinumfunc(struct inodesc *idesc)
{
	register struct direct *dirp = idesc->id_dirp;

	if (slotcount++ == desired) {
	    dirp->d_ino = idesc->id_parent;
	    return STOP|ALTERED|FOUND;
	}
	return KEEPON;
}

CMDFUNCSTART(chinum)
{
    char *cp;
    ino_t inum;
    struct inodesc idesc;
    
    slotcount = 0;
    if (!checkactivedir())
	return 1;
    GETINUM(2,inum);

    desired = strtol(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' || desired < 0) {
	printf("invalid slot number `%s'\n", argv[1]);
	return 1;
    }

    idesc.id_number = curinum;
    idesc.id_func = chinumfunc;
    idesc.id_fix = IGNORE;
    idesc.id_type = DATA;
    idesc.id_parent = inum;		/* XXX convenient hiding place */

    if (ckinode(curinode, &idesc) & FOUND)
	return 0;
    else {
	warnx("no %sth slot in current directory", argv[1]);
	return 1;
    }
}

int
chnamefunc(struct inodesc *idesc)
{
	register struct direct *dirp = idesc->id_dirp;
	struct direct testdir;

	if (slotcount++ == desired) {
	    /* will name fit? */
	    testdir.d_namlen = strlen(idesc->id_name);
	    if (DIRSIZ(NEWDIRFMT, &testdir) <= dirp->d_reclen) {
		dirp->d_namlen = testdir.d_namlen;
		strcpy(dirp->d_name, idesc->id_name);
		return STOP|ALTERED|FOUND;
	    } else
		return STOP|FOUND;	/* won't fit, so give up */
	}
	return KEEPON;
}

CMDFUNCSTART(chname)
{
    int rval;
    char *cp;
    struct inodesc idesc;
    
    slotcount = 0;
    if (!checkactivedir())
	return 1;

    desired = strtoul(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0') {
	printf("invalid slot number `%s'\n", argv[1]);
	return 1;
    }

    idesc.id_number = curinum;
    idesc.id_func = chnamefunc;
    idesc.id_fix = IGNORE;
    idesc.id_type = DATA;
    idesc.id_name = argv[2];

    rval = ckinode(curinode, &idesc);
    if ((rval & (FOUND|ALTERED)) == (FOUND|ALTERED))
	return 0;
    else if (rval & FOUND) {
	warnx("new name `%s' does not fit in slot %s\n", argv[2], argv[1]);
	return 1;
    } else {
	warnx("no %sth slot in current directory", argv[1]);
	return 1;
    }
}

struct typemap {
    const char *typename;
    int typebits;
} typenamemap[]  = {
    {"file", IFREG},
    {"dir", IFDIR},
    {"socket", IFSOCK},
    {"fifo", IFIFO},
};

CMDFUNCSTART(newtype)
{
    int type;
    struct typemap *tp;

    if (!checkactive())
	return 1;
    type = curinode->di_mode & IFMT;
    for (tp = typenamemap;
	 tp < &typenamemap[sizeof(typenamemap)/sizeof(*typenamemap)];
	 tp++) {
	if (!strcmp(argv[1], tp->typename)) {
	    printf("setting type to %s\n", tp->typename);
	    type = tp->typebits;
	    break;
	}
    }
    if (tp == &typenamemap[sizeof(typenamemap)/sizeof(*typenamemap)]) {
	warnx("type `%s' not known", argv[1]);
	warnx("try one of `file', `dir', `socket', `fifo'");
	return 1;
    }
    curinode->di_mode &= ~IFMT;
    curinode->di_mode |= type;
    inodirty();
    printactive(0);
    return 0;
}

CMDFUNCSTART(chlen)
{
    int rval = 1;
    long len;
    char *cp;

    if (!checkactive())
	return 1;

    len = strtol(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' || len < 0) { 
	warnx("bad length `%s'", argv[1]);
	return 1;
    }
    
    curinode->di_size = len;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(chmode)
{
    int rval = 1;
    long modebits;
    char *cp;

    if (!checkactive())
	return 1;

    modebits = strtol(argv[1], &cp, 8);
    if (cp == argv[1] || *cp != '\0' || (modebits & ~07777)) { 
	warnx("bad modebits `%s'", argv[1]);
	return 1;
    }
    
    curinode->di_mode &= ~07777;
    curinode->di_mode |= modebits;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(chaflags)
{
    int rval = 1;
    u_long flags;
    char *cp;

    if (!checkactive())
	return 1;

    flags = strtoul(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' ) { 
	warnx("bad flags `%s'", argv[1]);
	return 1;
    }
    
    if (flags > UINT_MAX) {
	warnx("flags set beyond 32-bit range of field (%lx)\n", flags);
	return(1);
    }
    curinode->di_flags = flags;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(chgen)
{
    int rval = 1;
    long gen;
    char *cp;

    if (!checkactive())
	return 1;

    gen = strtol(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' ) { 
	warnx("bad gen `%s'", argv[1]);
	return 1;
    }
    
    if (gen > INT_MAX || gen < INT_MIN) {
	warnx("gen set beyond 32-bit range of field (%lx)\n", gen);
	return(1);
    }
    curinode->di_gen = gen;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(linkcount)
{
    int rval = 1;
    int lcnt;
    char *cp;

    if (!checkactive())
	return 1;

    lcnt = strtol(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' ) { 
	warnx("bad link count `%s'", argv[1]);
	return 1;
    }
    if (lcnt > USHRT_MAX || lcnt < 0) {
	warnx("max link count is %d\n", USHRT_MAX);
	return 1;
    }
    
    curinode->di_nlink = lcnt;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(chowner)
{
    int rval = 1;
    unsigned long uid;
    char *cp;
    struct passwd *pwd;

    if (!checkactive())
	return 1;

    uid = strtoul(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' ) { 
	/* try looking up name */
	if ((pwd = getpwnam(argv[1]))) {
	    uid = pwd->pw_uid;
	} else {
	    warnx("bad uid `%s'", argv[1]);
	    return 1;
	}
    }
    
    curinode->di_uid = uid;
    inodirty();
    printactive(0);
    return rval;
}

CMDFUNCSTART(chgroup)
{
    int rval = 1;
    unsigned long gid;
    char *cp;
    struct group *grp;

    if (!checkactive())
	return 1;

    gid = strtoul(argv[1], &cp, 0);
    if (cp == argv[1] || *cp != '\0' ) { 
	if ((grp = getgrnam(argv[1]))) {
	    gid = grp->gr_gid;
	} else {
	    warnx("bad gid `%s'", argv[1]);
	    return 1;
	}
    }
    
    curinode->di_gid = gid;
    inodirty();
    printactive(0);
    return rval;
}

int
dotime(char *name, struct timespec *rts)
{
    char *p, *val;
    struct tm t;
    int32_t sec;
    int32_t nsec;
    p = strchr(name, '.');
    if (p) {
	*p = '\0';
	nsec = strtoul(++p, &val, 0);
	if (val == p || *val != '\0' || nsec >= 1000000000 || nsec < 0) {
		warnx("invalid nanoseconds");
		goto badformat;
	}
    } else
	nsec = 0;
    if (strlen(name) != 14) {
badformat:
	warnx("date format: YYYYMMDDHHMMSS[.nsec]");
	return 1;
    }

    for (p = name; *p; p++)
	if (*p < '0' || *p > '9')
	    goto badformat;
    
    p = name;
#define VAL() ((*p++) - '0')
    t.tm_year = VAL();
    t.tm_year = VAL() + t.tm_year * 10;
    t.tm_year = VAL() + t.tm_year * 10;
    t.tm_year = VAL() + t.tm_year * 10 - 1900;
    t.tm_mon = VAL();
    t.tm_mon = VAL() + t.tm_mon * 10 - 1;
    t.tm_mday = VAL();
    t.tm_mday = VAL() + t.tm_mday * 10;
    t.tm_hour = VAL();
    t.tm_hour = VAL() + t.tm_hour * 10;
    t.tm_min = VAL();
    t.tm_min = VAL() + t.tm_min * 10;
    t.tm_sec = VAL();
    t.tm_sec = VAL() + t.tm_sec * 10;
    t.tm_isdst = -1;

    sec = mktime(&t);
    if (sec == -1) {
	warnx("date/time out of range");
	return 1;
    }
    rts->tv_sec = sec;
    rts->tv_nsec = nsec;
    return 0;
}

CMDFUNCSTART(chmtime)
{
    if (dotime(argv[1], &curinode->di_mtime))
	return 1;
    inodirty();
    printactive(0);
    return 0;
}

CMDFUNCSTART(chatime)
{
    if (dotime(argv[1], &curinode->di_atime))
	return 1;
    inodirty();
    printactive(0);
    return 0;
}

CMDFUNCSTART(chctime)
{
    if (dotime(argv[1], &curinode->di_ctime))
	return 1;
    inodirty();
    printactive(0);
    return 0;
}
