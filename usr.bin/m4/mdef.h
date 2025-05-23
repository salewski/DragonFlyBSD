/*	$OpenBSD: mdef.h,v 1.21 2001/09/27 11:40:33 espie Exp $	*/
/*	$NetBSD: mdef.h,v 1.7 1996/01/13 23:25:27 pk Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
 *	@(#)mdef.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/m4/mdef.h,v 1.3.12.2 2002/07/15 02:06:15 jmallett Exp $
 * $DragonFly: src/usr.bin/m4/mdef.h,v 1.2 2003/06/17 04:29:28 dillon Exp $
 */

#define MACRTYPE        1
#define DEFITYPE        2
#define EXPRTYPE        3
#define SUBSTYPE        4
#define IFELTYPE        5
#define LENGTYPE        6
#define CHNQTYPE        7
#define SYSCTYPE        8
#define UNDFTYPE        9
#define INCLTYPE        10
#define SINCTYPE        11
#define PASTTYPE        12
#define SPASTYPE        13
#define INCRTYPE        14
#define IFDFTYPE        15
#define PUSDTYPE        16
#define POPDTYPE        17
#define SHIFTYPE        18
#define DECRTYPE        19
#define DIVRTYPE        20
#define UNDVTYPE        21
#define DIVNTYPE        22
#define MKTMTYPE        23
#define ERRPTYPE        24
#define M4WRTYPE        25
#define TRNLTYPE        26
#define DNLNTYPE        27
#define DUMPTYPE        28
#define CHNCTYPE        29
#define INDXTYPE        30
#define SYSVTYPE        31
#define EXITTYPE        32
#define DEFNTYPE        33
#define SELFTYPE	34
#define INDIRTYPE	35
#define BUILTINTYPE	36
#define PATSTYPE	37
#define FILENAMETYPE	38
#define LINETYPE	39
#define REGEXPTYPE	40
#define ESYSCMDTYPE	41
#define TRACEONTYPE	42
#define TRACEOFFTYPE	43

 
#define TYPEMASK	63	/* Keep bits really corresponding to a type. */
#define RECDEF		256	/* Pure recursive def, don't expand it */
#define NOARGS		512	/* builtin needs no args */
#define NEEDARGS	1024	/* mark builtin that need args with this */

/*
 * m4 special characters
 */
 
#define ARGFLAG         '$'
#define LPAREN          '('
#define RPAREN          ')'
#define LQUOTE          '`'
#define RQUOTE          '\''
#define COMMA           ','
#define SCOMMT          '#'
#define ECOMMT          '\n'

#ifdef msdos
#define system(str)	(-1)
#endif

/*
 * other important constants
 */

#define EOS             '\0'
#define MAXINP          10              /* maximum include files   	    */
#define MAXOUT          10              /* maximum # of diversions 	    */
#define BUFSIZE         4096            /* starting size of pushback buffer */
#define INITSTACKMAX    4096           	/* starting size of call stack      */
#define STRSPMAX        4096            /* starting size of string space    */
#define MAXTOK          512          	/* maximum chars in a tokn 	    */
#define HASHSIZE        199             /* maximum size of hashtab 	    */
#define MAXCCHARS	5		/* max size of comment/quote delim  */
 
#define ALL             1
#define TOP             0
 
#define TRUE            1
#define FALSE           0
#define cycle           for(;;)

/*
 * m4 data structures
 */
 
typedef struct ndblock *ndptr;
 
struct ndblock {		/* hastable structure         */
	char		*name;	/* entry name..               */
	char		*defn;	/* definition..               */
	unsigned int	type;	/* type of the entry..        */
	unsigned int 	hv;	/* hash function value..      */
	ndptr		nxtptr;	/* link to next entry..       */
};
 
#define nil     ((ndptr) 0)
 
struct keyblk {
        const char    *knam;    /* keyword name */
        int     ktyp;           /* keyword type */
};

typedef union {			/* stack structure */
	int	sfra;		/* frame entry  */
	char 	*sstr;		/* string entry */
} stae;

struct input_file {
	FILE 		*file;
	char 		*name;
	unsigned long 	lineno;
	int 		c;
};

#define CURRENT_NAME	(infile[ilevel].name)
#define CURRENT_LINE	(infile[ilevel].lineno)
/*
 * macros for readibility and/or speed
 *
 *      pushf() - push a call frame entry onto stack
 *      pushs() - push a string pointer onto stack
 */
#define pushf(x) 					\
	do {						\
		if ((uintptr_t)++sp == STACKMAX) 	\
			enlarge_stack();		\
		mstack[sp].sfra = (x);			\
		sstack[sp] = 0;				\
	} while (0)

#define pushs(x) 					\
	do {						\
		if ((uintptr_t)++sp == STACKMAX) 	\
			enlarge_stack();		\
		mstack[sp].sstr = (x);			\
		sstack[sp] = 1;				\
	} while (0)

#define pushs1(x) 					\
	do {						\
		if ((uintptr_t)++sp == STACKMAX) 	\
			enlarge_stack();		\
		mstack[sp].sstr = (x);			\
		sstack[sp] = 0;				\
	} while (0)

/*
 *	    .				   .
 *	|   .	|  <-- sp		|  .  |
 *	+-------+			+-----+
 *	| arg 3 ----------------------->| str |
 *	+-------+			|  .  |
 *	| arg 2 ---PREVEP-----+ 	   .
 *	+-------+	      |
 *	    .		      |		|     |
 *	+-------+	      | 	+-----+
 *	| plev	|  PARLEV     +-------->| str |
 *	+-------+			|  .  |
 *	| type	|  CALTYP		   .
 *	+-------+
 *	| prcf	---PREVFP--+
 *	+-------+  	   |
 *	|   .	|  PREVSP  |
 *	    .	   	   |
 *	+-------+	   |
 *	|	<----------+
 *	+-------+
 *
 */
#define PARLEV  (mstack[fp].sfra)
#define CALTYP  (mstack[fp-1].sfra)
#define PREVEP	(mstack[fp+3].sstr)
#define PREVSP	(fp-3)
#define PREVFP	(mstack[fp-2].sfra)
