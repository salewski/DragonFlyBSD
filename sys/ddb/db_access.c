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
 * $FreeBSD: src/sys/ddb/db_access.c,v 1.15 1999/08/28 00:41:04 peter Exp $
 * $DragonFly: src/sys/ddb/db_access.c,v 1.3 2003/07/26 14:18:51 rob Exp $
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#include <sys/param.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>

/*
 * Access unaligned data items on aligned (longword)
 * boundaries.
 */

static unsigned db_extend[] = {	/* table for sign-extending */
	0,
	0xFFFFFF80U,
	0xFFFF8000U,
	0xFF800000U
};

db_expr_t
db_get_value(addr, size, is_signed)
	db_addr_t	addr;
	int		size;
	boolean_t	is_signed;
{
	char		data[sizeof(int)];
	db_expr_t 	value;
	int	i;

	db_read_bytes(addr, size, data);

	value = 0;
#if	BYTE_MSF
	for (i = 0; i < size; i++)
#else	/* BYTE_LSF */
	for (i = size - 1; i >= 0; i--)
#endif
	{
	    value = (value << 8) + (data[i] & 0xFF);
	}

	if (size < 4) {
	    if (is_signed && (value & db_extend[size]) != 0)
		value |= db_extend[size];
	}
	return (value);
}

void
db_put_value(addr, size, value)
	db_addr_t	addr;
	int		size;
	db_expr_t value;
{
	char		data[sizeof(int)];
	int		i;

#if	BYTE_MSF
	for (i = size - 1; i >= 0; i--)
#else	/* BYTE_LSF */
	for (i = 0; i < size; i++)
#endif
	{
	    data[i] = value & 0xFF;
	    value >>= 8;
	}

	db_write_bytes(addr, size, data);
}

