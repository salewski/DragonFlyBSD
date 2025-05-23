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
 *
 * @(#)rpc_cout.c 1.13 89/02/22 (C) 1987 SMI
 * $FreeBSD: src/usr.bin/rpcgen/rpc_cout.c,v 1.7 1999/08/28 01:05:16 peter Exp $
 * $DragonFly: src/usr.bin/rpcgen/rpc_cout.c,v 1.4 2004/06/19 16:40:36 joerg Exp $
 */

#ident	"@(#)rpc_cout.c	1.14	93/07/05 SMI" 

/*
 * rpc_cout.c, XDR routine outputter for the RPC protocol compiler
 * Copyright (C) 1987, Sun Microsystems, Inc.
 */
#include <err.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "rpc_parse.h"
#include "rpc_util.h"

static void	print_header(definition *);
static void	print_trailer(void);
static void	print_stat(int , declaration *);
static void	emit_enum(definition *);
static void	emit_program(definition *);
static void	emit_union(definition *);
static void	emit_struct(definition *);
static void	emit_typedef(definition *);
static void	emit_inline(int, declaration *, int);
static void	emit_single_in_line(int, declaration *, int, relation);
static char	*upcase(char *str);

/*
 * Emit the C-routine for the given definition
 */
void
emit(definition *def)
{
	if (def->def_kind == DEF_CONST)
		return;
	if (def->def_kind == DEF_PROGRAM) {
		emit_program(def);
		return;
	}
	if (def->def_kind == DEF_TYPEDEF) {
		/*
		 * now we need to handle declarations like
		 * struct typedef foo foo;
		 * since we dont want this to be expanded into 2 calls to xdr_foo
		 */

		if (strcmp(def->def.ty.old_type, def->def_name) == 0)
			return;
	};
	print_header(def);
	switch (def->def_kind) {
	case DEF_UNION:
		emit_union(def);
		break;
	case DEF_ENUM:
		emit_enum(def);
		break;
	case DEF_STRUCT:
		emit_struct(def);
		break;
	case DEF_TYPEDEF:
		emit_typedef(def);
		break;
		/* DEF_CONST and DEF_PROGRAM have already been handled */
	default: 
		break;
	}
	print_trailer();
}

static int
findtype(definition *def, char *type)
{

	if (def->def_kind == DEF_PROGRAM || def->def_kind == DEF_CONST)
		return(0);
	else
		return(streq(def->def_name, type));
}

static int
undefined(char *type)
{
	definition *def;

	def = (definition *) FINDVAL(defined, type, findtype);
	return(def == NULL);
}


static void
print_generic_header(char *procname, int pointerp)
{
	f_print(fout, "\n");
	f_print(fout, "bool_t\n");
	if (Cflag) {
	    f_print(fout, "xdr_%s(", procname);
	    f_print(fout, "register XDR *xdrs, ");
	    f_print(fout, "%s ", procname);
	    if (pointerp)
		    f_print(fout, "*");
	    f_print(fout, "objp)\n{\n\n");
	} else {
	    f_print(fout, "xdr_%s(xdrs, objp)\n", procname);
	    f_print(fout, "\tregister XDR *xdrs;\n");
	    f_print(fout, "\t%s ", procname);
	    if (pointerp)
		    f_print(fout, "*");
	    f_print(fout, "objp;\n{\n\n");
	}
}

static void
print_header(definition *def)
{
	print_generic_header(def->def_name,
	    def->def_kind != DEF_TYPEDEF || !isvectordef(def->def.ty.old_type,
	    def->def.ty.rel));
	/* Now add Inline support */

	if (rpcgen_inline == 0)
		return;
	/* May cause lint to complain. but  ... */
	f_print(fout, "\tregister long *buf;\n\n");
}

static void
print_prog_header(proc_list *plist)
{
	print_generic_header(plist->args.argname, 1);
}

static void
print_trailer(void)
{
	f_print(fout, "\treturn (TRUE);\n");
	f_print(fout, "}\n");
}


static void
print_ifopen(int indent, char *name)
{
	tabify(fout, indent);
	f_print(fout, "if (!xdr_%s(xdrs", name);
}

static void
print_ifarg(char *arg)
{
	f_print(fout, ", %s", arg);
}

static void
print_ifsizeof(int indent, char *prefix, char *type)
{
	if (indent) {
		f_print(fout, ",\n");
		tabify(fout, indent);
	} else  {
		f_print(fout, ", ");
	}
	if (streq(type, "bool")) {
		f_print(fout, "sizeof (bool_t), (xdrproc_t) xdr_bool");
	} else {
		f_print(fout, "sizeof (");
		if (undefined(type) && prefix)
			f_print(fout, "%s ", prefix);
		f_print(fout, "%s), (xdrproc_t) xdr_%s", type, type);
	}
}

static void
print_ifclose(int indent)
{
	f_print(fout, "))\n");
	tabify(fout, indent);
	f_print(fout, "\treturn (FALSE);\n");
}

static void
print_ifstat(int indent, char *prefix, char *type, relation rel, char *amax,
	     char *objname, char *name)
{
	char *alt = NULL;

	switch (rel) {
	case REL_POINTER:
		print_ifopen(indent, "pointer");
		print_ifarg("(char **)");
		f_print(fout, "%s", objname);
		print_ifsizeof(0, prefix, type);
		break;
	case REL_VECTOR:
		if (streq(type, "string"))
			alt = "string";
		else if (streq(type, "opaque"))
			alt = "opaque";

		if (alt) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
		} else {
			print_ifopen(indent, "vector");
			print_ifarg("(char *)");
			f_print(fout, "%s", objname);
		}
		print_ifarg(amax);
		if (!alt)
			print_ifsizeof(indent + 1, prefix, type);
		break;
	case REL_ARRAY:
		if (streq(type, "string"))
			alt = "string";
		else if (streq(type, "opaque"))
			alt = "bytes";

		if (streq(type, "string")) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
		} else {
			if (alt)
				print_ifopen(indent, alt);
			else
				print_ifopen(indent, "array");
			print_ifarg("(char **)");
			if (*objname == '&') {
				f_print(fout, "%s.%s_val, (u_int *) %s.%s_len",
					objname, name, objname, name);
			} else {
				f_print(fout,
					"&%s->%s_val, (u_int *) &%s->%s_len",
					objname, name, objname, name);
			}
		}
		print_ifarg(amax);
		if (!alt)
			print_ifsizeof(indent + 1, prefix, type);
		break;
	case REL_ALIAS:
		print_ifopen(indent, type);
		print_ifarg(objname);
		break;
	}
	print_ifclose(indent);
}

/* ARGSUSED */
static void
emit_enum(definition *def)
{
	print_ifopen(1, "enum");
	print_ifarg("(enum_t *)objp");
	print_ifclose(1);
}

static void
emit_program(definition *def)
{
	decl_list *dl;
	version_list *vlist;
	proc_list *plist;

	for (vlist = def->def.pr.versions; vlist != NULL; vlist = vlist->next)
		for (plist = vlist->procs; plist != NULL; plist = plist->next) {
			if (!newstyle || plist->arg_num < 2)
				continue; /* old style, or single argument */
			print_prog_header(plist);
			for (dl = plist->args.decls; dl != NULL;
			     dl = dl->next)
				print_stat(1, &dl->decl);
			print_trailer();
		}
}


static void
emit_union(definition *def)
{
	declaration *dflt;
	case_list *cl;
	declaration *cs;
	char *object;
	char *vecformat = "objp->%s_u.%s";
	char *format = "&objp->%s_u.%s";

	print_stat(1, &def->def.un.enum_decl);
	f_print(fout, "\tswitch (objp->%s) {\n", def->def.un.enum_decl.name);
	for (cl = def->def.un.cases; cl != NULL; cl = cl->next) {

		f_print(fout, "\tcase %s:\n", cl->case_name);
		if (cl->contflag == 1) /* a continued case statement */
			continue;
		cs = &cl->case_decl;
		if (!streq(cs->type, "void")) {
			object = alloc(strlen(def->def_name) + strlen(format) +
				       strlen(cs->name) + 1);
			if (isvectordef (cs->type, cs->rel)) {
				s_print(object, vecformat, def->def_name,
					cs->name);
			} else {
				s_print(object, format, def->def_name,
					cs->name);
			}
			print_ifstat(2, cs->prefix, cs->type, cs->rel,
				     cs->array_max, object, cs->name);
			free(object);
		}
		f_print(fout, "\t\tbreak;\n");
	}
	dflt = def->def.un.default_decl;
	if (dflt != NULL) {
		if (!streq(dflt->type, "void")) {
			f_print(fout, "\tdefault:\n");
			object = alloc(strlen(def->def_name) + strlen(format) +
				       strlen(dflt->name) + 1);
			if (isvectordef (dflt->type, dflt->rel)) {
				s_print(object, vecformat, def->def_name,
					dflt->name);
			} else {
				s_print(object, format, def->def_name,
					dflt->name);
			}

			print_ifstat(2, dflt->prefix, dflt->type, dflt->rel,
				    dflt->array_max, object, dflt->name);
			free(object);
			f_print(fout, "\t\tbreak;\n");
		} else {
			f_print(fout, "\tdefault:\n");
			f_print(fout, "\t\tbreak;\n");
		}
	} else {
		f_print(fout, "\tdefault:\n");
		f_print(fout, "\t\treturn (FALSE);\n");
	}

	f_print(fout, "\t}\n");
}

static void
inline_struct(definition *def, int flag)
{
	decl_list *dl;
	int i, size;
	decl_list *cur, *psav;
	bas_type *ptr;
	char *sizestr, *plus;
	char ptemp[256];
	int indent = 1;

	if (flag == PUT)
		f_print(fout, "\n\tif (xdrs->x_op == XDR_ENCODE) {\n");
	else
		f_print(fout, "\t\treturn (TRUE);\n"
			"\t} else if (xdrs->x_op == XDR_DECODE) {\n");

	i = 0;
	size = 0;
	sizestr = NULL;
	for (dl = def->def.st.decls; dl != NULL; dl = dl->next) { /* xxx */
		/* now walk down the list and check for basic types */
		if ((dl->decl.prefix == NULL) &&
		    ((ptr = find_type(dl->decl.type)) != NULL) &&
		    ((dl->decl.rel == REL_ALIAS) ||
		     (dl->decl.rel == REL_VECTOR))){
			if (i == 0)
				cur = dl;
			i++;

			if (dl->decl.rel == REL_ALIAS) {
				size += ptr->length;
			} else {
				/* this code is required to handle arrays */
				if (sizestr == NULL)
					plus = "";
				else
					plus = " + ";

				if (ptr->length != 1)
					s_print(ptemp, "%s%s * %d",
						plus, dl->decl.array_max,
						ptr->length);
				else
					s_print(ptemp, "%s%s", plus,
						dl->decl.array_max);

				/* now concatenate to sizestr !!!! */
				if (sizestr == NULL) {
					sizestr = strdup(ptemp);
				} else {
					sizestr = realloc(sizestr,
							  strlen(sizestr)
							  +strlen(ptemp)+1);
					if (sizestr == NULL){
						warnx("fatal error: no memory");
						crash();
					};
					sizestr = strcat(sizestr, ptemp);
					/* build up length of array */
				}
			}
		} else {
			if (i > 0) {
				if (sizestr == NULL && size < rpcgen_inline){
					/*
					 * don't expand into inline code
					 * if size < rpcgen_inline
					 */
					while (cur != dl){
						print_stat(indent + 1,
							   &cur->decl);
						cur = cur->next;
					}
				} else {
					/* were already looking at a xdr_inlineable structure */
					tabify(fout, indent + 1);
					if (sizestr == NULL)
						f_print(fout, "buf = XDR_INLINE(xdrs, %d * BYTES_PER_XDR_UNIT);",
							size);
					else {
						if (size == 0)
							f_print(fout,
								"buf = XDR_INLINE(xdrs, (%s) * BYTES_PER_XDR_UNIT);",
								sizestr);
						else
							f_print(fout,
								"buf = XDR_INLINE(xdrs, (%d + (%s)) * BYTES_PER_XDR_UNIT);",
								size, sizestr);

					}
					f_print(fout, "\n");
					tabify(fout, indent + 1);
					f_print(fout,
						"if (buf == NULL) {\n");

					psav = cur;
					while (cur != dl) {
						print_stat(indent + 2, &cur->decl);
						cur = cur->next;
					}

					f_print(fout, "\n\t\t} else {\n");

					cur = psav;
					while (cur != dl) {
						emit_inline(indent + 2, &cur->decl, flag);
						cur = cur->next;
					}

					tabify(fout, indent + 1);
					f_print(fout, "}\n");
				}
			}
			size = 0;
			i = 0;
			sizestr = NULL;
			print_stat(indent + 1, &dl->decl);
		}
	}

	if (i > 0)
		if (sizestr == NULL && size < rpcgen_inline) {
			/* don't expand into inline code if size < rpcgen_inline */
			while (cur != dl) {
				print_stat(indent + 1, &cur->decl);
				cur = cur->next;
			}
		} else {
			/* were already looking at a xdr_inlineable structure */
			if (sizestr == NULL)
				f_print(fout, "\t\tbuf = XDR_INLINE(xdrs, %d * BYTES_PER_XDR_UNIT);",
					size);
			else {
				if (size == 0)
					f_print(fout,
						"\t\tbuf = XDR_INLINE(xdrs, (%s) * BYTES_PER_XDR_UNIT);",
						sizestr);
				else
					f_print(fout,
						"\t\tbuf = XDR_INLINE(xdrs, (%d + (%s)) * BYTES_PER_XDR_UNIT);",
						size, sizestr);
			}

			f_print(fout, "\n\t\tif (buf == NULL) {\n");
			psav = cur;
			while (cur != NULL) {
				print_stat(indent + 2, &cur->decl);
				cur = cur->next;
			}
			f_print(fout, "\t\t} else {\n");

			cur = psav;
			while (cur != dl) {
				emit_inline(indent + 2, &cur->decl, flag);
				cur = cur->next;
			}
			f_print(fout, "\t\t}\n");
		}
}

static void
emit_struct(def)
	definition *def;
{
	decl_list *dl;
	int j, size, flag;
	bas_type *ptr;
	int can_inline;

	if (rpcgen_inline == 0) {
		/* No xdr_inlining at all */
		for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
			print_stat(1, &dl->decl);
		return;
	}

	for (dl = def->def.st.decls; dl != NULL; dl = dl->next) {
		if (dl->decl.rel == REL_VECTOR){
			f_print(fout, "\tint i;\n");
			break;
		}
	}

	size = 0;
	can_inline = 0;
	/*
	 * Make a first pass and see if inling is possible.
	 */
	for (dl = def->def.st.decls; dl != NULL; dl = dl->next) {
		if ((dl->decl.prefix == NULL) &&
		    ((ptr = find_type(dl->decl.type)) != NULL) &&
		    ((dl->decl.rel == REL_ALIAS)||
		     (dl->decl.rel == REL_VECTOR))) {
			if (dl->decl.rel == REL_ALIAS)
				size += ptr->length;
			else {
				can_inline = 1;
				break; /* can be inlined */
			}
		} else {
			if (size >= rpcgen_inline) {
				can_inline = 1;
				break; /* can be inlined */
			}
			size = 0;
		}
	}
	if (size >= rpcgen_inline)
		can_inline = 1;

	if (can_inline == 0) {	/* can not inline, drop back to old mode */
		for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
			print_stat(1, &dl->decl);
		return;
	}

	flag = PUT;
	for (j = 0; j < 2; j++){
		inline_struct(def, flag);
		if (flag == PUT)
			flag = GET;
	}

	f_print(fout, "\t\treturn (TRUE);\n\t}\n\n");

	/* now take care of XDR_FREE case */

	for (dl = def->def.st.decls; dl != NULL; dl = dl->next)
		print_stat(1, &dl->decl);

}

static void
emit_typedef(definition *def)
{
	char *prefix = def->def.ty.old_prefix;
	char *type = def->def.ty.old_type;
	char *amax = def->def.ty.array_max;
	relation rel = def->def.ty.rel;

	print_ifstat(1, prefix, type, rel, amax, "objp", def->def_name);
}

static void
print_stat(int indent, declaration *dec)
{
	char *prefix = dec->prefix;
	char *type = dec->type;
	char *amax = dec->array_max;
	relation rel = dec->rel;
	char name[256];

	if (isvectordef(type, rel))
		s_print(name, "objp->%s", dec->name);
	else
		s_print(name, "&objp->%s", dec->name);
	print_ifstat(indent, prefix, type, rel, amax, name, dec->name);
}



static void
emit_inline(int indent, declaration *decl, int flag)
{
	switch (decl->rel) {
	case REL_ALIAS :
		emit_single_in_line(indent, decl, flag, REL_ALIAS);
		break;
	case REL_VECTOR :
		tabify(fout, indent);
		f_print(fout, "{\n");
		tabify(fout, indent + 1);
		f_print(fout, "register %s *genp;\n\n", decl->type);
		tabify(fout, indent + 1);
		f_print(fout, "for (i = 0, genp = objp->%s;\n", decl->name);
		tabify(fout, indent + 2);
		f_print(fout, "i < %s; i++) {\n", decl->array_max);
		emit_single_in_line(indent + 2, decl, flag, REL_VECTOR);
		tabify(fout, indent + 1);
		f_print(fout, "}\n");
		tabify(fout, indent);
		f_print(fout, "}\n");
	default:
		break;
	}
}

static void
emit_single_in_line(int indent, declaration *decl, int flag, relation rel)
{
	char *upp_case;
	int freed = 0;

	tabify(fout, indent);
	if (flag == PUT)
		f_print(fout, "IXDR_PUT_");
	else if (rel == REL_ALIAS)
		f_print(fout, "objp->%s = IXDR_GET_", decl->name);
	else
		f_print(fout, "*genp++ = IXDR_GET_");

	upp_case = upcase(decl->type);

	/* hack	 - XX */
	if (strcmp(upp_case, "INT") == 0)
	{
		free(upp_case);
		freed = 1;
		upp_case = "LONG";
	}

	if (strcmp(upp_case, "U_INT") == 0)
	{
		free(upp_case);
		freed = 1;
		upp_case = "U_LONG";
	}
	if (flag == PUT) {
		if (rel == REL_ALIAS)
			f_print(fout, "%s(buf, objp->%s);\n", upp_case,
				decl->name);
		else
			f_print(fout, "%s(buf, *genp++);\n", upp_case);
	} else {
		f_print(fout, "%s(buf);\n", upp_case);
	}
	if (!freed)
		free(upp_case);
}

static char *
upcase(char *str)
{
	char *ptr, *hptr;

	ptr =  malloc(strlen(str)+1);
	if (ptr == NULL)
		errx(1, "malloc failed");

	hptr = ptr;
	while (*str != '\0')
		*ptr++ = toupper(*str++);

	*ptr = '\0';
	return(hptr);
}
