/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#pragma ident   "@(#)rpc_util.h 1.16     94/05/15 SMI"

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*	PROPRIETARY NOTICE (Combined)
*
* This source code is unpublished proprietary information
* constituting, or derived under license from AT&T's UNIX(r) System V.
* In addition, portions of such source code were derived from Berkeley
* 4.3 BSD under license from the Regents of the University of
* California.
*
*
*
*	Copyright Notice 
*
* Notice of copyright on this source code product does not indicate 
*  publication.
*
*	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
*	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
*          All rights reserved.
*/ 

/*      @(#)rpc_util.h  1.5  90/08/29  (C) 1987 SMI   */
/* $DragonFly: src/usr.bin/rpcgen/rpc_util.h,v 1.2 2004/06/19 16:40:36 joerg Exp $ */

/*
 * rpc_util.h, Useful definitions for the RPC protocol compiler 
 */
#include <sys/types.h>
#include <stdlib.h>

#include "rpc_scan.h"

#define	alloc(size)		malloc((unsigned)(size))
#define	ALLOC(object)   (object *) malloc(sizeof(object))

#define	s_print	(void) sprintf
#define	f_print (void) fprintf

struct list {
	definition *val;
	struct list *next;
};
typedef struct list list;

struct xdrfunc {
	char *name;
	int pointerp;
	struct xdrfunc *next;
};
typedef struct xdrfunc xdrfunc;

struct commandline {
	int cflag;		/* xdr C routines */
	int hflag;		/* header file */
	int lflag;		/* client side stubs */
	int mflag;		/* server side stubs */
	int nflag;		/* netid flag */
	int sflag;		/* server stubs for the given transport */
	int tflag;		/* dispatch Table file */
	int Ssflag;		/* produce server sample code */
	int Scflag;		/* produce client sample code */
	int makefileflag;       /* Generate a template Makefile */
	char *infile;		/* input module name */
	char *outfile;		/* output module name */
};

#define	PUT 1
#define	GET 2

/*
 * Global variables 
 */
#define	MAXLINESIZE 1024
extern char curline[MAXLINESIZE];
extern char *where;
extern int linenum;

extern char *infilename;
extern FILE *fout;
extern FILE *fin;

extern list *defined;


extern bas_type *typ_list_h;
extern bas_type *typ_list_t;
extern xdrfunc *xdrfunc_head, *xdrfunc_tail;

/*
 * All the option flags
 */
extern int inetdflag;
extern int pmflag;   
extern int tblflag;
extern int logflag;
extern int newstyle;
extern int Cflag;     /* ANSI-C/C++ flag */
extern int CCflag;     /* C++ flag */
extern int tirpcflag; /* flag for generating tirpc code */
extern int rpcgen_inline; /* if this is 0, then do not generate inline code */
extern int mtflag;

/*
 * Other flags related with inetd jumpstart.
 */
extern int indefinitewait;
extern int exitnow;
extern int timerflag;

extern int nonfatalerrors;

extern pid_t childpid;

/*
 * rpc_util routines 
 */
void		reinitialize(void);
void		crash(void );
void		add_type(int len, char *type);

void		storeval(list **, definition *);

#define	STOREVAL(list,item)	\
	storeval(list,item)

definition	*findval(list *, char *, int (*)(definition *, char *));

#define	FINDVAL(list,item,finder) \
	findval(list, item, finder)

char 		*fixtype(char *);
char		*stringfix(char *);
char		*locase(char *);
void		pvname_svc(char *, char *);
void		pvname(char *, char *);
void		ptype(char *, char *, int);
int		isvectordef(char *, relation);
int		streq(char *, char *);
void		error(char *);
void		expected1(tok_kind);
void		expected2(tok_kind, tok_kind);
void		expected3(tok_kind, tok_kind, tok_kind);
void		tabify(FILE *, int);
void		record_open(char *);
bas_type	*find_type(char *);
/*
 * rpc_cout routines 
 */
void		cprint(void);
void		emit(definition *);

/*
 * rpc_hout routines 
 */
void		print_datadef(definition *);
void		print_funcdef(definition *);
void		print_xdr_func_def(char *, int, int);

/*
 * rpc_svcout routines 
 */
void		write_most(char *, int, int);
void		write_rest(void);
void		write_programs(char *);
void		write_svc_aux(int);
void		write_inetd_register(char *);
void		write_netid_register(char *);
void		write_nettype_register(char *);
/*
 * rpc_clntout routines
 */
void		write_stubs(void);

/*
 * rpc_tblout routines
 */
void		write_tables(void);
