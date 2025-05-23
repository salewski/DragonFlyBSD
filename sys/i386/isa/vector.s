/*
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD: src/sys/i386/isa/vector.s,v 1.32 1999/08/28 00:45:04 peter Exp $
 * $DragonFly: src/sys/i386/isa/Attic/vector.s,v 1.6 2005/02/27 12:44:43 asmodai Exp $
 */

#include "opt_auto_eoi.h"

#include <i386/isa/icu.h>
#include <bus/isa/i386/isa.h>

	.data
	ALIGN_DATA

/*
 * Interrupt counters and names for export to vmstat(8) and friends.
 *
 * XXX this doesn't really belong here; everything except the labels
 * for the endpointers is almost machine-independent.
 */
#define	NR_INTRNAMES	(1 + ICU_LEN + 2 * ICU_LEN)

	.globl	intrcnt, eintrcnt
intrcnt:
	.space	NR_INTRNAMES * 4
eintrcnt:

	.globl	intrnames, eintrnames
intrnames:
	.space	NR_INTRNAMES * 16
eintrnames:

	.text

/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 *
 * XXX - the interrupt frame is set up to look like a trap frame.  This is
 * usually a waste of time.  The only interrupt handlers that want a frame
 * are the clock handler (it wants a clock frame), the npx handler (it's
 * easier to do right all in assembler).  The interrupt return routine
 * needs a trap frame for rare AST's (it could easily convert the frame).
 * The direct costs of setting up a trap frame are two pushl's (error
 * code and trap number), an addl to get rid of these, and pushing and
 * popping the call-saved regs %esi, %edi and %ebp twice,  The indirect
 * costs are making the driver interface nonuniform so unpending of
 * interrupts is more complicated and slower (call_driver(unit) would
 * be easier than ensuring an interrupt frame for all handlers.  Finally,
 * there are some struct copies in the npx handler and maybe in the clock
 * handler that could be avoided by working more with pointers to frames
 * instead of frames.
 *
 * XXX - should we do a cld on every system entry to avoid the requirement
 * for scattered cld's?
 *
 * Coding notes for *.s:
 *
 * If possible, avoid operations that involve an operand size override.
 * Word-sized operations might be smaller, but the operand size override
 * makes them slower on on 486's and no faster on 386's unless perhaps
 * the instruction pipeline is depleted.  E.g.,
 *
 *	Use movl to seg regs instead of the equivalent but more descriptive
 *	movw - gas generates an irelevant (slower) operand size override.
 *
 *	Use movl to ordinary regs in preference to movw and especially
 *	in preference to movz[bw]l.  Use unsigned (long) variables with the
 *	top bits clear instead of unsigned short variables to provide more
 *	opportunities for movl.
 *
 * If possible, use byte-sized operations.  They are smaller and no slower.
 *
 * Use (%reg) instead of 0(%reg) - gas generates larger code for the latter.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first
 * and decide the ipending case with less overhead, e.g., by avoiding
 * loading segregs.
 */

#ifdef APIC_IO
#include "i386/isa/apic_vector.s"
#else
#include "i386/isa/icu_vector.s"
#endif  /* APIC_IO */
