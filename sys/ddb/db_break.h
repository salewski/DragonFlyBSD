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
 * $FreeBSD: src/sys/ddb/db_break.h,v 1.12 1999/08/28 00:41:06 peter Exp $
 * $DragonFly: src/sys/ddb/db_break.h,v 1.3 2003/08/27 10:47:13 rob Exp $
 */

/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#ifndef _DDB_DB_BREAK_H_
#define	_DDB_DB_BREAK_H_

/*
 * Breakpoint.
 */

struct db_breakpoint {
	vm_map_t map;			/* in this map */
	db_addr_t address;		/* set here */
	int	init_count;		/* number of times to skip bkpt */
	int	count;			/* current count */
	int	flags;			/* flags: */
#define	BKPT_SINGLE_STEP	0x2	    /* to simulate single step */
#define	BKPT_TEMP		0x4	    /* temporary */
	int	bkpt_inst;		/* saved instruction at bkpt */
	struct db_breakpoint *link;	/* link in in-use or free chain */
};
typedef struct db_breakpoint *db_breakpoint_t;

void		db_clear_breakpoints (void);
#ifdef SOFTWARE_SSTEP
void		db_delete_temp_breakpoint (db_breakpoint_t);
#endif
db_breakpoint_t	db_find_breakpoint_here (db_addr_t addr);
void		db_set_breakpoints (void);
#ifdef SOFTWARE_SSTEP
db_breakpoint_t	db_set_temp_breakpoint (db_addr_t);
#endif

#endif /* !_DDB_DB_BREAK_H_ */
