/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	@(#)var.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: src/bin/sh/var.h,v 1.8.2.1 2002/07/19 04:38:52 tjr Exp $
 * $DragonFly: src/bin/sh/var.h,v 1.4 2004/10/01 20:30:25 dillon Exp $
 */

/*
 * Shell variables.
 */

/* flags */
#define VEXPORT		0x01	/* variable is exported */
#define VREADONLY	0x02	/* variable cannot be modified */
#define VSTRFIXED	0x04	/* variable struct is staticly allocated */
#define VTEXTFIXED	0x08	/* text is staticly allocated */
#define VSTACK		0x10	/* text is allocated on the stack */
#define VUNSET		0x20	/* the variable is not set */
#define VNOFUNC		0x40	/* don't call the callback function */


struct var {
	struct var *next;		/* next entry in hash list */
	int flags;			/* flags are defined above */
	char *text;			/* name=value */
	void (*func)(const char *);
					/* function to be called when  */
					/* the variable gets set/unset */
};


struct localvar {
	struct localvar *next;		/* next local variable in list */
	struct var *vp;			/* the variable that was made local */
	int flags;			/* saved flags */
	char *text;			/* saved text */
};


struct localvar *localvars;

extern struct var vifs;
extern struct var vmail;
extern struct var vmpath;
extern struct var vpath;
extern struct var vppid;
extern struct var vps1;
extern struct var vps2;
#ifndef NO_HISTORY
extern struct var vhistsize;
#endif

/*
 * The following macros access the values of the above variables.
 * They have to skip over the name.  They return the null string
 * for unset variables.
 */

#define ifsval()	(vifs.text + 4)
#define ifsset()	((vifs.flags & VUNSET) == 0)
#define mailval()	(vmail.text + 5)
#define mpathval()	(vmpath.text + 9)
#define pathval()	(vpath.text + 5)
#define ps1val()	(vps1.text + 4)
#define ps2val()	(vps2.text + 4)
#define optindval()	(voptind.text + 7)
#ifndef NO_HISTORY
#define histsizeval()	(vhistsize.text + 9)
#endif

#define mpathset()	((vmpath.flags & VUNSET) == 0)

void initvar(void);
void setvar(const char *, const char *, int);
void setvareq(char *, int);
struct strlist;
void listsetvar(struct strlist *);
char *lookupvar(const char *);
char *bltinlookup(const char *, int);
char **environment(void);
void shprocvar(void);
int showvarscmd(int, char **);
int exportcmd(int, char **);
int localcmd(int, char **);
void mklocal(char *);
void poplocalvars(void);
int setvarcmd(int, char **);
int unsetcmd(int, char **);
int unsetvar(const char *);
int setvarsafe(const char *, const char *, int);
