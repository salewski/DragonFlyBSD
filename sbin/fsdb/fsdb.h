/*	$NetBSD: fsdb.h,v 1.2 1995/10/08 23:18:11 thorpej Exp $	*/

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
 * $FreeBSD: src/sbin/fsdb/fsdb.h,v 1.4.2.1 2002/03/20 13:39:02 joerg Exp $
 * $DragonFly: src/sbin/fsdb/fsdb.h,v 1.3 2003/11/01 17:15:59 drhodus Exp $
 */

extern int bread(int fd, char *buf, daddr_t blk, long size);
extern void bwrite(int fd, char *buf, daddr_t blk, long size);
extern void rwerror(char *mesg, daddr_t blk);
extern int reply(char *question);

extern long dev_bsize;
extern long secsize;
extern int fsmodified;
extern int fsfd;

struct cmdtable {
	const char *cmd;
	const char *helptxt;
	unsigned int minargc;
	unsigned int maxargc;
	unsigned int flags;
#define	FL_RO	0x0000		/* for symmetry */
#define	FL_WR	0x0001		/* wants to write */
	int (*handler)(int argc, char *argv[]);
};
extern struct dinode *curinode;
extern ino_t curinum;

int argcount(struct cmdtable *cmdp, int argc, char *argv[]);
char **crack(char *line, int *argc);
void printstat(const char *cp, ino_t inum, struct dinode *dp);
int printactive(int doblocks);
int checkactive(void);
int checkactivedir(void);
