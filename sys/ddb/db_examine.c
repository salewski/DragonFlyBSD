/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD: src/sys/ddb/db_examine.c,v 1.27 1999/08/28 00:41:07 peter Exp $
 * $DragonFly: src/sys/ddb/db_examine.c,v 1.5 2003/11/08 03:06:53 dillon Exp $
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <ddb/ddb.h>

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>

static char	db_examine_format[TOK_STRING_SIZE] = "x";

static void	db_examine (db_addr_t, char *, int);
static void	db_search (db_addr_t, int, db_expr_t, db_expr_t, u_int);

/*
 * Examine (print) data.
 */
/*ARGSUSED*/
void
db_examine_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char *		modif;
{
	if (modif[0] != '\0')
	    db_strcpy(db_examine_format, modif);

	if (count == -1)
	    count = 1;

	db_examine((db_addr_t) addr, db_examine_format, count);
}

static void
db_examine(addr, fmt, count)

	db_addr_t	addr;
	char *		fmt;	/* format string */
	int		count;	/* repeat count */
{
	int		c;
	db_expr_t	value;
	int		size;
	int		width;
	char *		fp;

	while (--count >= 0) {
	    fp = fmt;
	    size = 4;
	    width = 16;
	    while ((c = *fp++) != 0) {
		switch (c) {
		    case 'b':
			size = 1;
			width = 4;
			break;
		    case 'h':
			size = 2;
			width = 8;
			break;
		    case 'l':
			size = 4;
			width = 16;
			break;
		    case 'g':
			size = 8;
			width = 32;
			break;
		    case 'a':	/* address */
			/* always forces a new line */
			if (db_print_position() != 0)
			    db_printf("\n");
			db_prev = addr;
			db_printsym(addr, DB_STGY_ANY);
			db_printf(":\t");
			break;
		    default:
			if (db_print_position() == 0) {
			    /* Print the address. */
			    db_printsym(addr, DB_STGY_ANY);
			    db_printf(":\t");
			    db_prev = addr;
			}

			switch (c) {
			    case 'r':	/* signed, current radix */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_printf("%+-*lr", width, (long)value);
				break;
			    case 'x':	/* unsigned hex */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lx", width, (long)value);
				break;
			    case 'z':	/* signed hex */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_printf("%-*lz", width, (long)value);
				break;
			    case 'd':	/* signed decimal */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_printf("%-*ld", width, (long)value);
				break;
			    case 'u':	/* unsigned decimal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lu", width, (long)value);
				break;
			    case 'o':	/* unsigned octal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lo", width, (long)value);
				break;
			    case 'c':	/* character */
				value = db_get_value(addr, 1, FALSE);
				addr += 1;
				if (value >= ' ' && value <= '~')
				    db_printf("%c", (int)value);
				else
				    db_printf("\\%03o", (int)value);
				break;
			    case 's':	/* null-terminated string */
				for (;;) {
				    value = db_get_value(addr, 1, FALSE);
				    addr += 1;
				    if (value == 0)
					break;
				    if (value >= ' ' && value <= '~')
					db_printf("%c", (int)value);
				    else
					db_printf("\\%03o", (int)value);
				}
				break;
			    case 'i':	/* instruction */
				addr = db_disasm(addr, FALSE, NULL);
				break;
			    case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, TRUE, NULL);
				break;
			    default:
				break;
			}
			if (db_print_position() != 0)
			    db_end_line();
			break;
		}
	    }
	}
	db_next = addr;
}

/*
 * Print value.
 */
static char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	boolean_t	have_addr;
	db_expr_t	count;
	char *		modif;
{
	db_expr_t	value;

	if (modif[0] != '\0')
	    db_print_format = modif[0];

	switch (db_print_format) {
	    case 'a':
		db_printsym((db_addr_t)addr, DB_STGY_ANY);
		break;
	    case 'r':
		db_printf("%+11lr", (long)addr);
		break;
	    case 'x':
		db_printf("%8lx", (unsigned long)addr);
		break;
	    case 'z':
		db_printf("%8lz", (long)addr);
		break;
	    case 'd':
		db_printf("%11ld", (long)addr);
		break;
	    case 'u':
		db_printf("%11lu", (unsigned long)addr);
		break;
	    case 'o':
		db_printf("%16lo", (unsigned long)addr);
		break;
	    case 'c':
		value = addr & 0xFF;
		if (value >= ' ' && value <= '~')
		    db_printf("%c", (int)value);
		else
		    db_printf("\\%03o", (int)value);
		break;
	}
	db_printf("\n");
}

void
db_print_loc_and_inst(db_addr_t	loc, db_regs_t *regs)
{
	db_printsym(loc, DB_STGY_PROC);
	db_printf(":\t");
	(void) db_disasm(loc, TRUE, regs);
}

/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count]
 */
void
db_search_cmd(dummy1, dummy2, dummy3, dummy4)
	db_expr_t	dummy1;
	boolean_t	dummy2;
	db_expr_t	dummy3;
	char *		dummy4;
{
	int		t;
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	db_expr_t	count;

	t = db_read_token();
	if (t == tSLASH) {
	    t = db_read_token();
	    if (t != tIDENT) {
	      bad_modifier:
		db_printf("Bad modifier\n");
		db_flush_lex();
		return;
	    }

	    if (!strcmp(db_tok_string, "b"))
		size = 1;
	    else if (!strcmp(db_tok_string, "h"))
		size = 2;
	    else if (!strcmp(db_tok_string, "l"))
		size = 4;
	    else
		goto bad_modifier;
	} else {
	    db_unread_token(t);
	    size = 4;
	}

	if (!db_expression((db_expr_t *)&addr)) {
	    db_printf("Address missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&value)) {
	    db_printf("Value missing\n");
	    db_flush_lex();
	    return;
	}

	if (!db_expression(&mask))
	    mask = 0xffffffffUL;

	t = db_read_token();
	if (t == tCOMMA) {
	    if (!db_expression(&count)) {
		db_printf("Count missing\n");
		db_flush_lex();
		return;
	    }
	} else {
	    db_unread_token(t);
	    count = -1;		/* effectively forever */
	}
	db_skip_to_eol();

	db_search(addr, size, value, mask, count);
}

static void
db_search(addr, size, value, mask, count)
	
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	unsigned int	count;
{
	while (count-- != 0) {
		db_prev = addr;
		if ((db_get_value(addr, size, FALSE) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}
