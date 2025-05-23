/*-
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * $FreeBSD: src/sys/compat/linux/linux_ipc.h,v 1.2.2.4 2001/11/05 19:08:22 marcel Exp $
 * $DragonFly: src/sys/emulation/linux/linux_ipc.h,v 1.7 2003/11/20 06:05:29 dillon Exp $
 */

#ifndef _LINUX_IPC_H_
#define _LINUX_IPC_H_

#ifdef __i386__

struct linux_msgctl_args 
{
	struct sysmsg	sysmsg;
	l_int		msqid;
	l_int		cmd;
	struct l_msqid_ds *buf;
};

struct linux_msgget_args
{
	struct sysmsg	sysmsg;
	l_key_t		key;
	l_int		msgflg;
};

struct linux_msgrcv_args
{
	struct sysmsg	sysmsg;
	l_int		msqid;
	struct l_msgbuf *msgp;
	l_size_t	msgsz;
	l_long		msgtyp;
	l_int		msgflg;
};

struct linux_msgsnd_args
{
	struct sysmsg	sysmsg;
	l_int		msqid;
	struct l_msgbuf *msgp;
	l_size_t	msgsz;
	l_int		msgflg;
};

struct linux_semctl_args
{
	struct sysmsg	sysmsg;
	l_int		semid;
	l_int		semnum;
	l_int		cmd;
	union l_semun	arg;
};

struct linux_semget_args
{
	struct sysmsg	sysmsg;
	l_key_t		key;
	l_int		nsems;
	l_int		semflg;
};

struct linux_semop_args
{
	struct sysmsg	sysmsg;
	l_int		semid;
	struct l_sembuf *tsops;
	l_uint		nsops;
};

struct linux_shmat_args
{
	struct sysmsg	sysmsg;
	l_int		shmid;
	char		*shmaddr;
	l_int		shmflg;
	l_ulong		*raddr;
};

struct linux_shmctl_args
{
	struct sysmsg	sysmsg;
	l_int		shmid;
	l_int		cmd;
	struct l_shmid_ds *buf;
};

struct linux_shmdt_args
{
	struct sysmsg	sysmsg;
	char *shmaddr;
};

struct linux_shmget_args
{
	struct sysmsg	sysmsg;
	l_key_t		key;
	l_size_t	size;
	l_int		shmflg;
};

int linux_msgctl (struct linux_msgctl_args *);
int linux_msgget (struct linux_msgget_args *);
int linux_msgrcv (struct linux_msgrcv_args *);
int linux_msgsnd (struct linux_msgsnd_args *);

int linux_semctl (struct linux_semctl_args *);
int linux_semget (struct linux_semget_args *);
int linux_semop  (struct linux_semop_args *);

int linux_shmat  (struct linux_shmat_args *);
int linux_shmctl (struct linux_shmctl_args *);
int linux_shmdt  (struct linux_shmdt_args *);
int linux_shmget (struct linux_shmget_args *);

#endif	/* __i386__ */

#endif /* _LINUX_IPC_H_ */
