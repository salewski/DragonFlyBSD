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
 * $FreeBSD: src/sys/ddb/db_trap.c,v 1.14 1999/08/28 00:41:11 peter Exp $
 * $DragonFly: src/sys/ddb/db_trap.c,v 1.3 2003/11/08 03:06:53 dillon Exp $
 */

/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Trap entry point to kernel debugger.
 */
#include <sys/param.h>

#include <ddb/ddb.h>
#include <ddb/db_command.h>

#include <setjmp.h>

extern jmp_buf	db_jmpbuf;

void
db_trap(type, code)
	int	type, code;
{
	boolean_t	bkpt;
	boolean_t	watchpt;

	bkpt = IS_BREAKPOINT_TRAP(type, code);
	watchpt = IS_WATCHPOINT_TRAP(type, code);

	if (db_stop_at_pc(&bkpt)) {
	    if (db_inst_count) {
		db_printf("After %d instructions (%d loads, %d stores),\n",
			  db_inst_count, db_load_count, db_store_count);
	    }
	    if (bkpt)
		db_printf("Breakpoint at\t");
	    else if (watchpt)
		db_printf("Watchpoint at\t");
	    else
		db_printf("Stopped at\t");
	    db_dot = PC_REGS(DDB_REGS);
	    if (setjmp(db_jmpbuf) == 0)
		db_print_loc_and_inst(db_dot, DDB_REGS);

	    db_command_loop();
	}

	db_restart_at_pc(watchpt);
}
