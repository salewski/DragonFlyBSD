/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/svr4/svr4_ioctl.c,v 1.6 1999/12/08 12:00:48 newton Exp $
 * $DragonFly: src/sys/emulation/svr4/Attic/svr4_ioctl.c,v 1.11 2003/09/12 00:43:30 daver Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <emulation/43bsd/43bsd_socket.h>

#include "svr4.h"
#include "svr4_types.h"
#include "svr4_util.h"
#include "svr4_signal.h"
#include "svr4_proto.h"
#include "svr4_stropts.h"
#include "svr4_ioctl.h"
#include "svr4_termios.h"
#include "svr4_ttold.h"
#include "svr4_filio.h"
#include "svr4_sockio.h"

#ifdef DEBUG_SVR4
static void svr4_decode_cmd (u_long, char *, char *, int *, int *);
/*
 * Decode an ioctl command symbolically
 */
static void
svr4_decode_cmd(cmd, dir, c, num, argsiz)
	u_long		  cmd;
	char		 *dir, *c;
	int		 *num, *argsiz;
{
	if (cmd & SVR4_IOC_VOID)
		*dir++ = 'V';
	if (cmd & SVR4_IOC_IN)
		*dir++ = 'R';
	if (cmd & SVR4_IOC_OUT)
		*dir++ = 'W';
	*dir = '\0';
	if (cmd & SVR4_IOC_INOUT)
		*argsiz = (cmd >> 16) & 0xff;
	else
		*argsiz = -1;

	*c = (cmd >> 8) & 0xff;
	*num = cmd & 0xff;
}
#endif

int
svr4_sys_ioctl(struct svr4_sys_ioctl_args *uap)
{
	struct thread	*td = curthread;
	struct proc 	*p = td->td_proc;
	int             *retval;
	struct file	*fp;
	struct filedesc	*fdp;
	u_long		 cmd;
	int (*fun) (struct file *, struct thread *, register_t *,
			int, u_long, caddr_t);
#ifdef DEBUG_SVR4
	char		 dir[4];
	char		 c;
	int		 num;
	int		 argsiz;

	KKASSERT(p);

	svr4_decode_cmd(SCARG(uap, com), dir, &c, &num, &argsiz);

	DPRINTF(("svr4_ioctl[%lx](%d, _IO%s(%c, %d, %d), %p);\n",
	    SCARG(uap, com), SCARG(uap, fd),
	    dir, c, num, argsiz, SCARG(uap, data)));
#endif
	retval = &uap->sysmsg_result;
	fdp = p->p_fd;
	cmd = SCARG(uap, com);

	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return EBADF;

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return EBADF;

#if defined(DEBUG_SVR4)
	if (fp->f_type == DTYPE_SOCKET) {
	        struct socket *so = (struct socket *)fp->f_data;
		DPRINTF(("<<< IN: so_state = 0x%x\n", so->so_state));
	}
#endif

	switch (cmd & 0xff00) {
	case SVR4_tIOC:
	        DPRINTF(("ttold\n"));
		fun = svr4_ttold_ioctl;
		break;

	case SVR4_TIOC:
	        DPRINTF(("term\n"));
		fun = svr4_term_ioctl;
		break;

	case SVR4_STR:
	        DPRINTF(("stream\n"));
		fun = svr4_stream_ioctl;
		break;

	case SVR4_FIOC:
                DPRINTF(("file\n"));
		fun = svr4_fil_ioctl;
		break;

	case SVR4_SIOC:
	        DPRINTF(("socket\n"));
		fun = svr4_sock_ioctl;
		break;

	case SVR4_XIOC:
		/* We do not support those */
		return EINVAL;

	default:
		DPRINTF(("Unimplemented ioctl %lx\n", cmd));
		return 0;	/* XXX: really ENOSYS */
	}
#if defined(DEBUG_SVR4)
	if (fp->f_type == DTYPE_SOCKET) {
	        struct socket *so = (struct socket *)fp->f_data;
		DPRINTF((">>> OUT: so_state = 0x%x\n", so->so_state));
	}
#endif
	return (*fun)(fp, td, retval, SCARG(uap, fd), cmd, SCARG(uap, data));
}
