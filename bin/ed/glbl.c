/* glob.c: This file contains the global command routines for the ed line
   editor */
/*-
 * Copyright (c) 1993 Andrew Moore, Talke Studio.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)glob.c,v 1.1 1994/02/01 00:34:40 alm Exp
 * $FreeBSD: src/bin/ed/glbl.c,v 1.9.2.1 2001/01/23 14:38:31 asmodai Exp $
 * $DragonFly: src/bin/ed/glbl.c,v 1.3 2003/09/28 14:39:14 hmp Exp $
 */

#include <sys/types.h>

#include <sys/ioctl.h>
#include <sys/wait.h>

#include "ed.h"


/* build_active_list:  add line matching a pattern to the global-active list */
int
build_active_list(int isgcmd)
{
	pattern_t *pat;
	line_t *lp;
	long n;
	char *s;
	char delimiter;

	if ((delimiter = *ibufp) == ' ' || delimiter == '\n') {
		sprintf(errmsg, "invalid pattern delimiter");
		return ERR;
	} else if ((pat = get_compiled_pattern()) == NULL)
		return ERR;
	else if (*ibufp == delimiter)
		ibufp++;
	clear_active_list();
	lp = get_addressed_line_node(first_addr);
	for (n = first_addr; n <= second_addr; n++, lp = lp->q_forw) {
		if ((s = get_sbuf_line(lp)) == NULL)
			return ERR;
		if (isbinary)
			NUL_TO_NEWLINE(s, lp->len);
		if (!regexec(pat, s, 0, NULL, 0) == isgcmd &&
		    set_active_node(lp) < 0)
			return ERR;
	}
	return 0;
}


/* exec_global: apply command list in the command buffer to the active
   lines in a range; return command status */
long
exec_global(int interact, int gflag)
{
	static char *ocmd = NULL;
	static int ocmdsz = 0;

	line_t *lp = NULL;
	int status;
	int n;
	char *cmd = NULL;

#ifdef BACKWARDS
	if (!interact)
		if (!strcmp(ibufp, "\n"))
			cmd = "p\n";		/* null cmd-list == `p' */
		else if ((cmd = get_extended_line(&n, 0)) == NULL)
			return ERR;
#else
	if (!interact && (cmd = get_extended_line(&n, 0)) == NULL)
		return ERR;
#endif
	clear_undo_stack();
	while ((lp = next_active_node()) != NULL) {
		if ((current_addr = get_line_node_addr(lp)) < 0)
			return ERR;
		if (interact) {
			/* print current_addr; get a command in global syntax */
			if (display_lines(current_addr, current_addr, gflag) < 0)
				return ERR;
			while ((n = get_tty_line()) > 0 &&
			    ibuf[n - 1] != '\n')
				clearerr(stdin);
			if (n < 0)
				return ERR;
			else if (n == 0) {
				sprintf(errmsg, "unexpected end-of-file");
				return ERR;
			} else if (n == 1 && !strcmp(ibuf, "\n"))
				continue;
			else if (n == 2 && !strcmp(ibuf, "&\n")) {
				if (cmd == NULL) {
					sprintf(errmsg, "no previous command");
					return ERR;
				} else cmd = ocmd;
			} else if ((cmd = get_extended_line(&n, 0)) == NULL)
				return ERR;
			else {
				REALLOC(ocmd, ocmdsz, n + 1, ERR);
				memcpy(ocmd, cmd, n + 1);
				cmd = ocmd;
			}

		}
		ibufp = cmd;
		for (; *ibufp;)
			if ((status = extract_addr_range()) < 0 ||
			    (status = exec_command()) < 0 ||
			    (status > 0 && (status = display_lines(
			    current_addr, current_addr, status)) < 0))
				return status;
	}
	return 0;
}


line_t **active_list;		/* list of lines active in a global command */
long active_last;		/* index of last active line in active_list */
long active_size;		/* size of active_list */
long active_ptr;		/* active_list index (non-decreasing) */
long active_ndx;		/* active_list index (modulo active_last) */

/* set_active_node: add a line node to the global-active list */
int
set_active_node(line_t *lp)
{
	if (active_last + 1 > active_size) {
		int ti = active_size;
		line_t **ts;
		SPL1();
#if defined(sun) || defined(NO_REALLOC_NULL)
		if (active_list != NULL) {
#endif
			if ((ts = (line_t **) realloc(active_list,
			    (ti += MINBUFSZ) * sizeof(line_t **))) == NULL) {
				fprintf(stderr, "%s\n", strerror(errno));
				sprintf(errmsg, "out of memory");
				SPL0();
				return ERR;
			}
#if defined(sun) || defined(NO_REALLOC_NULL)
		} else {
			if ((ts = (line_t **) malloc((ti += MINBUFSZ) *
			    sizeof(line_t **))) == NULL) {
				fprintf(stderr, "%s\n", strerror(errno));
				sprintf(errmsg, "out of memory");
				SPL0();
				return ERR;
			}
		}
#endif
		active_size = ti;
		active_list = ts;
		SPL0();
	}
	active_list[active_last++] = lp;
	return 0;
}


/* unset_active_nodes: remove a range of lines from the global-active list */
void
unset_active_nodes(line_t *np, line_t *mp)
{
	line_t *lp;
	long i;

	for (lp = np; lp != mp; lp = lp->q_forw)
		for (i = 0; i < active_last; i++)
			if (active_list[active_ndx] == lp) {
				active_list[active_ndx] = NULL;
				active_ndx = INC_MOD(active_ndx, active_last - 1);
				break;
			} else	active_ndx = INC_MOD(active_ndx, active_last - 1);
}


/* next_active_node: return the next global-active line node */
line_t *
next_active_node(void)
{
	while (active_ptr < active_last && active_list[active_ptr] == NULL)
		active_ptr++;
	return (active_ptr < active_last) ? active_list[active_ptr++] : NULL;
}


/* clear_active_list: clear the global-active list */
void
clear_active_list(void)
{
	SPL1();
	active_size = active_last = active_ptr = active_ndx = 0;
	free(active_list);
	active_list = NULL;
	SPL0();
}
