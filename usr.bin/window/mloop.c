/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 * @(#)mloop.c	8.1 (Berkeley) 6/6/93
 * $FreeBSD: src/usr.bin/window/mloop.c,v 1.1.1.1.14.1 2001/05/17 09:45:00 obrien Exp $
 * $DragonFly: src/usr.bin/window/mloop.c,v 1.2 2003/06/17 04:29:34 dillon Exp $
 */

#include <sys/param.h>
#include "defs.h"

mloop()
{
	while (!quit) {
		if (incmd) {
			docmd();
		} else if (wwcurwin->ww_state != WWS_HASPROC) {
			if (!wwcurwin->ww_keepopen)
				closewin(wwcurwin);
			setcmd(1);
			if (wwpeekc() == escapec)
				(void) wwgetc();
			error("Process died.");
		} else {
			register struct ww *w = wwcurwin;
			register char *p;
			register n;

			if (wwibp >= wwibq)
				wwiomux();
			for (p = wwibp; p < wwibq && wwmaskc(*p) != escapec;
			     p++)
				;
			if ((n = p - wwibp) > 0) {
				if (!w->ww_ispty && w->ww_stopped)
					startwin(w);
#if defined(sun) && !defined(BSD)
				/* workaround for SunOS pty bug */
				while (--n >= 0)
					(void) write(w->ww_pty, wwibp++, 1);
#else
				(void) write(w->ww_pty, wwibp, n);
				wwibp = p;
#endif
			}
			if (wwpeekc() == escapec) {
				(void) wwgetc();
				setcmd(1);
			}
		}
	}
}
