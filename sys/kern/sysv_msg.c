/* $FreeBSD: src/sys/kern/sysv_msg.c,v 1.23.2.5 2002/12/31 08:54:53 maxim Exp $ */
/* $DragonFly: src/sys/kern/sysv_msg.c,v 1.13 2004/05/26 14:12:34 hmp Exp $ */

/*
 * Implementation of SVID messages
 *
 * Author:  Daniel Boulet
 *
 * Copyright 1993 Daniel Boulet and RTMX Inc.
 *
 * This system call was implemented by Daniel Boulet under contract from RTMX.
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/msg.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/jail.h>

static MALLOC_DEFINE(M_MSG, "msg", "SVID compatible message queues");

static void msginit (void *);

#define MSG_DEBUG
#undef MSG_DEBUG_OK

static void msg_freehdr (struct msg *msghdr);

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *msgcalls[] = {
	(sy_call_t *)msgctl, (sy_call_t *)msgget,
	(sy_call_t *)msgsnd, (sy_call_t *)msgrcv
};

struct msg {
	struct	msg *msg_next;	/* next msg in the chain */
	long	msg_type;	/* type of this message */
    				/* >0 -> type of this message */
    				/* 0 -> free header */
	u_short	msg_ts;		/* size of this message */
	short	msg_spot;	/* location of start of msg in buffer */
};


#ifndef MSGSSZ
#define MSGSSZ	8		/* Each segment must be 2^N long */
#endif
#ifndef MSGSEG
#define MSGSEG	2048		/* must be less than 32767 */
#endif
#define MSGMAX	(MSGSSZ*MSGSEG)
#ifndef MSGMNB
#define MSGMNB	2048		/* max # of bytes in a queue */
#endif
#ifndef MSGMNI
#define MSGMNI	40
#endif
#ifndef MSGTQL
#define MSGTQL	40
#endif

/*
 * Based on the configuration parameters described in an SVR2 (yes, two)
 * config(1m) man page.
 *
 * Each message is broken up and stored in segments that are msgssz bytes
 * long.  For efficiency reasons, this should be a power of two.  Also,
 * it doesn't make sense if it is less than 8 or greater than about 256.
 * Consequently, msginit in kern/sysv_msg.c checks that msgssz is a power of
 * two between 8 and 1024 inclusive (and panic's if it isn't).
 */
struct msginfo msginfo = {
                MSGMAX,         /* max chars in a message */
                MSGMNI,         /* # of message queue identifiers */
                MSGMNB,         /* max chars in a queue */
                MSGTQL,         /* max messages in system */
                MSGSSZ,         /* size of a message segment */
                		/* (must be small power of 2 greater than 4) */
                MSGSEG          /* number of message segments */
};

/*
 * macros to convert between msqid_ds's and msqid's.
 * (specific to this implementation)
 */
#define MSQID(ix,ds)	((ix) & 0xffff | (((ds).msg_perm.seq << 16) & 0xffff0000))
#define MSQID_IX(id)	((id) & 0xffff)
#define MSQID_SEQ(id)	(((id) >> 16) & 0xffff)

/*
 * The rest of this file is specific to this particular implementation.
 */

struct msgmap {
	short	next;		/* next segment in buffer */
    				/* -1 -> available */
    				/* 0..(MSGSEG-1) -> index of next segment */
};

#define MSG_LOCKED	01000	/* Is this msqid_ds locked? */

static int nfree_msgmaps;	/* # of free map entries */
static short free_msgmaps;	/* head of linked list of free map entries */
static struct msg *free_msghdrs;/* list of free msg headers */
static char *msgpool;		/* MSGMAX byte long msg buffer pool */
static struct msgmap *msgmaps;	/* MSGSEG msgmap structures */
static struct msg *msghdrs;	/* MSGTQL msg headers */
static struct msqid_ds *msqids;	/* MSGMNI msqid_ds struct's */

static void
msginit(dummy)
	void *dummy;
{
	int i;

	msginfo.msgmax = msginfo.msgseg * msginfo.msgssz;
	msgpool = malloc(msginfo.msgmax, M_MSG, M_WAITOK);
	if (msgpool == NULL)
		panic("msgpool is NULL");
	msgmaps = malloc(sizeof(struct msgmap) * msginfo.msgseg, M_MSG, M_WAITOK);
	if (msgmaps == NULL)
		panic("msgmaps is NULL");
	msghdrs = malloc(sizeof(struct msg) * msginfo.msgtql, M_MSG, M_WAITOK);
	if (msghdrs == NULL)
		panic("msghdrs is NULL");
	msqids = malloc(sizeof(struct msqid_ds) * msginfo.msgmni, M_MSG, M_WAITOK);
	if (msqids == NULL)
		panic("msqids is NULL");

	/*
	 * msginfo.msgssz should be a power of two for efficiency reasons.
	 * It is also pretty silly if msginfo.msgssz is less than 8
	 * or greater than about 256 so ...
	 */

	i = 8;
	while (i < 1024 && i != msginfo.msgssz)
		i <<= 1;
    	if (i != msginfo.msgssz) {
		printf("msginfo.msgssz=%d (0x%x)\n", msginfo.msgssz,
		    msginfo.msgssz);
		panic("msginfo.msgssz not a small power of 2");
	}

	if (msginfo.msgseg > 32767) {
		printf("msginfo.msgseg=%d\n", msginfo.msgseg);
		panic("msginfo.msgseg > 32767");
	}

	if (msgmaps == NULL)
		panic("msgmaps is NULL");

	for (i = 0; i < msginfo.msgseg; i++) {
		if (i > 0)
			msgmaps[i-1].next = i;
		msgmaps[i].next = -1;	/* implies entry is available */
	}
	free_msgmaps = 0;
	nfree_msgmaps = msginfo.msgseg;

	if (msghdrs == NULL)
		panic("msghdrs is NULL");

	for (i = 0; i < msginfo.msgtql; i++) {
		msghdrs[i].msg_type = 0;
		if (i > 0)
			msghdrs[i-1].msg_next = &msghdrs[i];
		msghdrs[i].msg_next = NULL;
    	}
	free_msghdrs = &msghdrs[0];

	if (msqids == NULL)
		panic("msqids is NULL");

	for (i = 0; i < msginfo.msgmni; i++) {
		msqids[i].msg_qbytes = 0;	/* implies entry is available */
		msqids[i].msg_perm.seq = 0;	/* reset to a known value */
		msqids[i].msg_perm.mode = 0;
	}
}
SYSINIT(sysv_msg, SI_SUB_SYSV_MSG, SI_ORDER_FIRST, msginit, NULL)

/*
 * Entry point for all MSG calls
 *
 * msgsys_args(int which, int a2, ...) (VARARGS)
 */
int
msgsys(struct msgsys_args *uap)
{
	struct proc *p = curproc;
	unsigned int which = (unsigned int)uap->which;

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	if (which >= sizeof(msgcalls)/sizeof(msgcalls[0]))
		return (EINVAL);
	bcopy(&uap->a2, &uap->which,
	    sizeof(struct msgsys_args) - offsetof(struct msgsys_args, a2));
	return ((*msgcalls[which])(uap));
}

static void
msg_freehdr(struct msg *msghdr)
{
	while (msghdr->msg_ts > 0) {
		short next;
		if (msghdr->msg_spot < 0 || msghdr->msg_spot >= msginfo.msgseg)
			panic("msghdr->msg_spot out of range");
		next = msgmaps[msghdr->msg_spot].next;
		msgmaps[msghdr->msg_spot].next = free_msgmaps;
		free_msgmaps = msghdr->msg_spot;
		nfree_msgmaps++;
		msghdr->msg_spot = next;
		if (msghdr->msg_ts >= msginfo.msgssz)
			msghdr->msg_ts -= msginfo.msgssz;
		else
			msghdr->msg_ts = 0;
	}
	if (msghdr->msg_spot != -1)
		panic("msghdr->msg_spot != -1");
	msghdr->msg_next = free_msghdrs;
	free_msghdrs = msghdr;
}

int
msgctl(struct msgctl_args *uap)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int msqid = uap->msqid;
	int cmd = uap->cmd;
	struct msqid_ds *user_msqptr = uap->buf;
	int rval, eval;
	struct msqid_ds msqbuf;
	struct msqid_ds *msqptr;

#ifdef MSG_DEBUG_OK
	printf("call to msgctl(%d, %d, 0x%x)\n", msqid, cmd, user_msqptr);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	msqid = IPCID_TO_IX(msqid);

	if (msqid < 0 || msqid >= msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
		printf("msqid (%d) out of range (0<=msqid<%d)\n", msqid,
		    msginfo.msgmni);
#endif
		return(EINVAL);
	}

	msqptr = &msqids[msqid];

	if (msqptr->msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
		printf("no such msqid\n");
#endif
		return(EINVAL);
	}
	if (msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid)) {
#ifdef MSG_DEBUG_OK
		printf("wrong sequence number\n");
#endif
		return(EINVAL);
	}

	eval = 0;
	rval = 0;

	switch (cmd) {

	case IPC_RMID:
	{
		struct msg *msghdr;
		if ((eval = ipcperm(p, &msqptr->msg_perm, IPC_M)))
			return(eval);
		/* Free the message headers */
		msghdr = msqptr->msg_first;
		while (msghdr != NULL) {
			struct msg *msghdr_tmp;

			/* Free the segments of each message */
			msqptr->msg_cbytes -= msghdr->msg_ts;
			msqptr->msg_qnum--;
			msghdr_tmp = msghdr;
			msghdr = msghdr->msg_next;
			msg_freehdr(msghdr_tmp);
		}

		if (msqptr->msg_cbytes != 0)
			panic("msg_cbytes is screwed up");
		if (msqptr->msg_qnum != 0)
			panic("msg_qnum is screwed up");

		msqptr->msg_qbytes = 0;	/* Mark it as free */

		wakeup((caddr_t)msqptr);
	}

		break;

	case IPC_SET:
		if ((eval = ipcperm(p, &msqptr->msg_perm, IPC_M)))
			return(eval);
		if ((eval = copyin(user_msqptr, &msqbuf, sizeof(msqbuf))) != 0)
			return(eval);
		if (msqbuf.msg_qbytes > msqptr->msg_qbytes) {
			eval = suser(td);
			if (eval)
				return(eval);
		}
		if (msqbuf.msg_qbytes > msginfo.msgmnb) {
#ifdef MSG_DEBUG_OK
			printf("can't increase msg_qbytes beyond %d (truncating)\n",
			    msginfo.msgmnb);
#endif
			msqbuf.msg_qbytes = msginfo.msgmnb;	/* silently restrict qbytes to system limit */
		}
		if (msqbuf.msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
			printf("can't reduce msg_qbytes to 0\n");
#endif
			return(EINVAL);		/* non-standard errno! */
		}
		msqptr->msg_perm.uid = msqbuf.msg_perm.uid;	/* change the owner */
		msqptr->msg_perm.gid = msqbuf.msg_perm.gid;	/* change the owner */
		msqptr->msg_perm.mode = (msqptr->msg_perm.mode & ~0777) |
		    (msqbuf.msg_perm.mode & 0777);
		msqptr->msg_qbytes = msqbuf.msg_qbytes;
		msqptr->msg_ctime = time_second;
		break;

	case IPC_STAT:
		if ((eval = ipcperm(p, &msqptr->msg_perm, IPC_R))) {
#ifdef MSG_DEBUG_OK
			printf("requester doesn't have read access\n");
#endif
			return(eval);
		}
		eval = copyout((caddr_t)msqptr, user_msqptr,
		    sizeof(struct msqid_ds));
		break;

	default:
#ifdef MSG_DEBUG_OK
		printf("invalid command %d\n", cmd);
#endif
		return(EINVAL);
	}

	if (eval == 0)
		uap->sysmsg_result = rval;
	return(eval);
}

int
msgget(struct msgget_args *uap)
{
	struct proc *p = curproc;
	int msqid, eval;
	int key = uap->key;
	int msgflg = uap->msgflg;
	struct ucred *cred = p->p_ucred;
	struct msqid_ds *msqptr = NULL;

#ifdef MSG_DEBUG_OK
	printf("msgget(0x%x, 0%o)\n", key, msgflg);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	if (key != IPC_PRIVATE) {
		for (msqid = 0; msqid < msginfo.msgmni; msqid++) {
			msqptr = &msqids[msqid];
			if (msqptr->msg_qbytes != 0 &&
			    msqptr->msg_perm.key == key)
				break;
		}
		if (msqid < msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
			printf("found public key\n");
#endif
			if ((msgflg & IPC_CREAT) && (msgflg & IPC_EXCL)) {
#ifdef MSG_DEBUG_OK
				printf("not exclusive\n");
#endif
				return(EEXIST);
			}
			if ((eval = ipcperm(p, &msqptr->msg_perm, msgflg & 0700 ))) {
#ifdef MSG_DEBUG_OK
				printf("requester doesn't have 0%o access\n",
				    msgflg & 0700);
#endif
				return(eval);
			}
			goto found;
		}
	}

#ifdef MSG_DEBUG_OK
	printf("need to allocate the msqid_ds\n");
#endif
	if (key == IPC_PRIVATE || (msgflg & IPC_CREAT)) {
		for (msqid = 0; msqid < msginfo.msgmni; msqid++) {
			/*
			 * Look for an unallocated and unlocked msqid_ds.
			 * msqid_ds's can be locked by msgsnd or msgrcv while
			 * they are copying the message in/out.  We can't
			 * re-use the entry until they release it.
			 */
			msqptr = &msqids[msqid];
			if (msqptr->msg_qbytes == 0 &&
			    (msqptr->msg_perm.mode & MSG_LOCKED) == 0)
				break;
		}
		if (msqid == msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
			printf("no more msqid_ds's available\n");
#endif
			return(ENOSPC);
		}
#ifdef MSG_DEBUG_OK
		printf("msqid %d is available\n", msqid);
#endif
		msqptr->msg_perm.key = key;
		msqptr->msg_perm.cuid = cred->cr_uid;
		msqptr->msg_perm.uid = cred->cr_uid;
		msqptr->msg_perm.cgid = cred->cr_gid;
		msqptr->msg_perm.gid = cred->cr_gid;
		msqptr->msg_perm.mode = (msgflg & 0777);
		/* Make sure that the returned msqid is unique */
		msqptr->msg_perm.seq = (msqptr->msg_perm.seq + 1) & 0x7fff;
		msqptr->msg_first = NULL;
		msqptr->msg_last = NULL;
		msqptr->msg_cbytes = 0;
		msqptr->msg_qnum = 0;
		msqptr->msg_qbytes = msginfo.msgmnb;
		msqptr->msg_lspid = 0;
		msqptr->msg_lrpid = 0;
		msqptr->msg_stime = 0;
		msqptr->msg_rtime = 0;
		msqptr->msg_ctime = time_second;
	} else {
#ifdef MSG_DEBUG_OK
		printf("didn't find it and wasn't asked to create it\n");
#endif
		return(ENOENT);
	}

found:
	/* Construct the unique msqid */
	uap->sysmsg_result = IXSEQ_TO_IPCID(msqid, msqptr->msg_perm);
	return(0);
}

int
msgsnd(struct msgsnd_args *uap)
{
	struct proc *p = curproc;
	int msqid = uap->msqid;
	void *user_msgp = uap->msgp;
	size_t msgsz = uap->msgsz;
	int msgflg = uap->msgflg;
	int segs_needed, eval;
	struct msqid_ds *msqptr;
	struct msg *msghdr;
	short next;

#ifdef MSG_DEBUG_OK
	printf("call to msgsnd(%d, 0x%x, %d, %d)\n", msqid, user_msgp, msgsz,
	    msgflg);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	msqid = IPCID_TO_IX(msqid);

	if (msqid < 0 || msqid >= msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
		printf("msqid (%d) out of range (0<=msqid<%d)\n", msqid,
		    msginfo.msgmni);
#endif
		return(EINVAL);
	}

	msqptr = &msqids[msqid];
	if (msqptr->msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
		printf("no such message queue id\n");
#endif
		return(EINVAL);
	}
	if (msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid)) {
#ifdef MSG_DEBUG_OK
		printf("wrong sequence number\n");
#endif
		return(EINVAL);
	}

	if ((eval = ipcperm(p, &msqptr->msg_perm, IPC_W))) {
#ifdef MSG_DEBUG_OK
		printf("requester doesn't have write access\n");
#endif
		return(eval);
	}

	segs_needed = (msgsz + msginfo.msgssz - 1) / msginfo.msgssz;
#ifdef MSG_DEBUG_OK
	printf("msgsz=%d, msgssz=%d, segs_needed=%d\n", msgsz, msginfo.msgssz,
	    segs_needed);
#endif
	for (;;) {
		int need_more_resources = 0;

		/*
		 * check msgsz
		 * (inside this loop in case msg_qbytes changes while we sleep)
		 */

		if (msgsz > msqptr->msg_qbytes) {
#ifdef MSG_DEBUG_OK
			printf("msgsz > msqptr->msg_qbytes\n");
#endif
			return(EINVAL);
		}

		if (msqptr->msg_perm.mode & MSG_LOCKED) {
#ifdef MSG_DEBUG_OK
			printf("msqid is locked\n");
#endif
			need_more_resources = 1;
		}
		if (msgsz + msqptr->msg_cbytes > msqptr->msg_qbytes) {
#ifdef MSG_DEBUG_OK
			printf("msgsz + msg_cbytes > msg_qbytes\n");
#endif
			need_more_resources = 1;
		}
		if (segs_needed > nfree_msgmaps) {
#ifdef MSG_DEBUG_OK
			printf("segs_needed > nfree_msgmaps\n");
#endif
			need_more_resources = 1;
		}
		if (free_msghdrs == NULL) {
#ifdef MSG_DEBUG_OK
			printf("no more msghdrs\n");
#endif
			need_more_resources = 1;
		}

		if (need_more_resources) {
			int we_own_it;

			if ((msgflg & IPC_NOWAIT) != 0) {
#ifdef MSG_DEBUG_OK
				printf("need more resources but caller doesn't want to wait\n");
#endif
				return(EAGAIN);
			}

			if ((msqptr->msg_perm.mode & MSG_LOCKED) != 0) {
#ifdef MSG_DEBUG_OK
				printf("we don't own the msqid_ds\n");
#endif
				we_own_it = 0;
			} else {
				/* Force later arrivals to wait for our
				   request */
#ifdef MSG_DEBUG_OK
				printf("we own the msqid_ds\n");
#endif
				msqptr->msg_perm.mode |= MSG_LOCKED;
				we_own_it = 1;
			}
#ifdef MSG_DEBUG_OK
			printf("goodnight\n");
#endif
			eval = tsleep((caddr_t)msqptr, PCATCH, "msgwait", 0);
#ifdef MSG_DEBUG_OK
			printf("good morning, eval=%d\n", eval);
#endif
			if (we_own_it)
				msqptr->msg_perm.mode &= ~MSG_LOCKED;
			if (eval != 0) {
#ifdef MSG_DEBUG_OK
				printf("msgsnd:  interrupted system call\n");
#endif
				return(EINTR);
			}

			/*
			 * Make sure that the msq queue still exists
			 */

			if (msqptr->msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
				printf("msqid deleted\n");
#endif
				return(EIDRM);
			}

		} else {
#ifdef MSG_DEBUG_OK
			printf("got all the resources that we need\n");
#endif
			break;
		}
	}

	/*
	 * We have the resources that we need.
	 * Make sure!
	 */

	if (msqptr->msg_perm.mode & MSG_LOCKED)
		panic("msg_perm.mode & MSG_LOCKED");
	if (segs_needed > nfree_msgmaps)
		panic("segs_needed > nfree_msgmaps");
	if (msgsz + msqptr->msg_cbytes > msqptr->msg_qbytes)
		panic("msgsz + msg_cbytes > msg_qbytes");
	if (free_msghdrs == NULL)
		panic("no more msghdrs");

	/*
	 * Re-lock the msqid_ds in case we page-fault when copying in the
	 * message
	 */

	if ((msqptr->msg_perm.mode & MSG_LOCKED) != 0)
		panic("msqid_ds is already locked");
	msqptr->msg_perm.mode |= MSG_LOCKED;

	/*
	 * Allocate a message header
	 */

	msghdr = free_msghdrs;
	free_msghdrs = msghdr->msg_next;
	msghdr->msg_spot = -1;
	msghdr->msg_ts = msgsz;

	/*
	 * Allocate space for the message
	 */

	while (segs_needed > 0) {
		if (nfree_msgmaps <= 0)
			panic("not enough msgmaps");
		if (free_msgmaps == -1)
			panic("nil free_msgmaps");
		next = free_msgmaps;
		if (next <= -1)
			panic("next too low #1");
		if (next >= msginfo.msgseg)
			panic("next out of range #1");
#ifdef MSG_DEBUG_OK
		printf("allocating segment %d to message\n", next);
#endif
		free_msgmaps = msgmaps[next].next;
		nfree_msgmaps--;
		msgmaps[next].next = msghdr->msg_spot;
		msghdr->msg_spot = next;
		segs_needed--;
	}

	/*
	 * Copy in the message type
	 */

	if ((eval = copyin(user_msgp, &msghdr->msg_type,
	    sizeof(msghdr->msg_type))) != 0) {
#ifdef MSG_DEBUG_OK
		printf("error %d copying the message type\n", eval);
#endif
		msg_freehdr(msghdr);
		msqptr->msg_perm.mode &= ~MSG_LOCKED;
		wakeup((caddr_t)msqptr);
		return(eval);
	}
	user_msgp = (char *)user_msgp + sizeof(msghdr->msg_type);

	/*
	 * Validate the message type
	 */

	if (msghdr->msg_type < 1) {
		msg_freehdr(msghdr);
		msqptr->msg_perm.mode &= ~MSG_LOCKED;
		wakeup((caddr_t)msqptr);
#ifdef MSG_DEBUG_OK
		printf("mtype (%d) < 1\n", msghdr->msg_type);
#endif
		return(EINVAL);
	}

	/*
	 * Copy in the message body
	 */

	next = msghdr->msg_spot;
	while (msgsz > 0) {
		size_t tlen;
		if (msgsz > msginfo.msgssz)
			tlen = msginfo.msgssz;
		else
			tlen = msgsz;
		if (next <= -1)
			panic("next too low #2");
		if (next >= msginfo.msgseg)
			panic("next out of range #2");
		if ((eval = copyin(user_msgp, &msgpool[next * msginfo.msgssz],
		    tlen)) != 0) {
#ifdef MSG_DEBUG_OK
			printf("error %d copying in message segment\n", eval);
#endif
			msg_freehdr(msghdr);
			msqptr->msg_perm.mode &= ~MSG_LOCKED;
			wakeup((caddr_t)msqptr);
			return(eval);
		}
		msgsz -= tlen;
		user_msgp = (char *)user_msgp + tlen;
		next = msgmaps[next].next;
	}
	if (next != -1)
		panic("didn't use all the msg segments");

	/*
	 * We've got the message.  Unlock the msqid_ds.
	 */

	msqptr->msg_perm.mode &= ~MSG_LOCKED;

	/*
	 * Make sure that the msqid_ds is still allocated.
	 */

	if (msqptr->msg_qbytes == 0) {
		msg_freehdr(msghdr);
		wakeup((caddr_t)msqptr);
		return(EIDRM);
	}

	/*
	 * Put the message into the queue
	 */

	if (msqptr->msg_first == NULL) {
		msqptr->msg_first = msghdr;
		msqptr->msg_last = msghdr;
	} else {
		msqptr->msg_last->msg_next = msghdr;
		msqptr->msg_last = msghdr;
	}
	msqptr->msg_last->msg_next = NULL;

	msqptr->msg_cbytes += msghdr->msg_ts;
	msqptr->msg_qnum++;
	msqptr->msg_lspid = p->p_pid;
	msqptr->msg_stime = time_second;

	wakeup((caddr_t)msqptr);
	uap->sysmsg_result = 0;
	return(0);
}

int
msgrcv(struct msgrcv_args *uap)
{
	struct proc *p = curproc;
	int msqid = uap->msqid;
	void *user_msgp = uap->msgp;
	size_t msgsz = uap->msgsz;
	long msgtyp = uap->msgtyp;
	int msgflg = uap->msgflg;
	size_t len;
	struct msqid_ds *msqptr;
	struct msg *msghdr;
	int eval;
	short next;

#ifdef MSG_DEBUG_OK
	printf("call to msgrcv(%d, 0x%x, %d, %ld, %d)\n", msqid, user_msgp,
	    msgsz, msgtyp, msgflg);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	msqid = IPCID_TO_IX(msqid);

	if (msqid < 0 || msqid >= msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
		printf("msqid (%d) out of range (0<=msqid<%d)\n", msqid,
		    msginfo.msgmni);
#endif
		return(EINVAL);
	}

	msqptr = &msqids[msqid];
	if (msqptr->msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
		printf("no such message queue id\n");
#endif
		return(EINVAL);
	}
	if (msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid)) {
#ifdef MSG_DEBUG_OK
		printf("wrong sequence number\n");
#endif
		return(EINVAL);
	}

	if ((eval = ipcperm(p, &msqptr->msg_perm, IPC_R))) {
#ifdef MSG_DEBUG_OK
		printf("requester doesn't have read access\n");
#endif
		return(eval);
	}

	msghdr = NULL;
	while (msghdr == NULL) {
		if (msgtyp == 0) {
			msghdr = msqptr->msg_first;
			if (msghdr != NULL) {
				if (msgsz < msghdr->msg_ts &&
				    (msgflg & MSG_NOERROR) == 0) {
#ifdef MSG_DEBUG_OK
					printf("first message on the queue is too big (want %d, got %d)\n",
					    msgsz, msghdr->msg_ts);
#endif
					return(E2BIG);
				}
				if (msqptr->msg_first == msqptr->msg_last) {
					msqptr->msg_first = NULL;
					msqptr->msg_last = NULL;
				} else {
					msqptr->msg_first = msghdr->msg_next;
					if (msqptr->msg_first == NULL)
						panic("msg_first/last screwed up #1");
				}
			}
		} else {
			struct msg *previous;
			struct msg **prev;

			previous = NULL;
			prev = &(msqptr->msg_first);
			while ((msghdr = *prev) != NULL) {
				/*
				 * Is this message's type an exact match or is
				 * this message's type less than or equal to
				 * the absolute value of a negative msgtyp?
				 * Note that the second half of this test can
				 * NEVER be true if msgtyp is positive since
				 * msg_type is always positive!
				 */

				if (msgtyp == msghdr->msg_type ||
				    msghdr->msg_type <= -msgtyp) {
#ifdef MSG_DEBUG_OK
					printf("found message type %d, requested %d\n",
					    msghdr->msg_type, msgtyp);
#endif
					if (msgsz < msghdr->msg_ts &&
					    (msgflg & MSG_NOERROR) == 0) {
#ifdef MSG_DEBUG_OK
						printf("requested message on the queue is too big (want %d, got %d)\n",
						    msgsz, msghdr->msg_ts);
#endif
						return(E2BIG);
					}
					*prev = msghdr->msg_next;
					if (msghdr == msqptr->msg_last) {
						if (previous == NULL) {
							if (prev !=
							    &msqptr->msg_first)
								panic("msg_first/last screwed up #2");
							msqptr->msg_first =
							    NULL;
							msqptr->msg_last =
							    NULL;
						} else {
							if (prev ==
							    &msqptr->msg_first)
								panic("msg_first/last screwed up #3");
							msqptr->msg_last =
							    previous;
						}
					}
					break;
				}
				previous = msghdr;
				prev = &(msghdr->msg_next);
			}
		}

		/*
		 * We've either extracted the msghdr for the appropriate
		 * message or there isn't one.
		 * If there is one then bail out of this loop.
		 */

		if (msghdr != NULL)
			break;

		/*
		 * Hmph!  No message found.  Does the user want to wait?
		 */

		if ((msgflg & IPC_NOWAIT) != 0) {
#ifdef MSG_DEBUG_OK
			printf("no appropriate message found (msgtyp=%d)\n",
			    msgtyp);
#endif
			/* The SVID says to return ENOMSG. */
#ifdef ENOMSG
			return(ENOMSG);
#else
			/* Unfortunately, BSD doesn't define that code yet! */
			return(EAGAIN);
#endif
		}

		/*
		 * Wait for something to happen
		 */

#ifdef MSG_DEBUG_OK
		printf("msgrcv:  goodnight\n");
#endif
		eval = tsleep((caddr_t)msqptr, PCATCH, "msgwait", 0);
#ifdef MSG_DEBUG_OK
		printf("msgrcv:  good morning (eval=%d)\n", eval);
#endif

		if (eval != 0) {
#ifdef MSG_DEBUG_OK
			printf("msgsnd:  interrupted system call\n");
#endif
			return(EINTR);
		}

		/*
		 * Make sure that the msq queue still exists
		 */

		if (msqptr->msg_qbytes == 0 ||
		    msqptr->msg_perm.seq != IPCID_TO_SEQ(uap->msqid)) {
#ifdef MSG_DEBUG_OK
			printf("msqid deleted\n");
#endif
			return(EIDRM);
		}
	}

	/*
	 * Return the message to the user.
	 *
	 * First, do the bookkeeping (before we risk being interrupted).
	 */

	msqptr->msg_cbytes -= msghdr->msg_ts;
	msqptr->msg_qnum--;
	msqptr->msg_lrpid = p->p_pid;
	msqptr->msg_rtime = time_second;

	/*
	 * Make msgsz the actual amount that we'll be returning.
	 * Note that this effectively truncates the message if it is too long
	 * (since msgsz is never increased).
	 */

#ifdef MSG_DEBUG_OK
	printf("found a message, msgsz=%d, msg_ts=%d\n", msgsz,
	    msghdr->msg_ts);
#endif
	if (msgsz > msghdr->msg_ts)
		msgsz = msghdr->msg_ts;

	/*
	 * Return the type to the user.
	 */

	eval = copyout((caddr_t)&(msghdr->msg_type), user_msgp,
	    sizeof(msghdr->msg_type));
	if (eval != 0) {
#ifdef MSG_DEBUG_OK
		printf("error (%d) copying out message type\n", eval);
#endif
		msg_freehdr(msghdr);
		wakeup((caddr_t)msqptr);
		return(eval);
	}
	user_msgp = (char *)user_msgp + sizeof(msghdr->msg_type);

	/*
	 * Return the segments to the user
	 */

	next = msghdr->msg_spot;
	for (len = 0; len < msgsz; len += msginfo.msgssz) {
		size_t tlen;

		if (msgsz - len > msginfo.msgssz)
			tlen = msginfo.msgssz;
		else
			tlen = msgsz - len;
		if (next <= -1)
			panic("next too low #3");
		if (next >= msginfo.msgseg)
			panic("next out of range #3");
		eval = copyout((caddr_t)&msgpool[next * msginfo.msgssz],
		    user_msgp, tlen);
		if (eval != 0) {
#ifdef MSG_DEBUG_OK
			printf("error (%d) copying out message segment\n",
			    eval);
#endif
			msg_freehdr(msghdr);
			wakeup((caddr_t)msqptr);
			return(eval);
		}
		user_msgp = (char *)user_msgp + tlen;
		next = msgmaps[next].next;
	}

	/*
	 * Done, return the actual number of bytes copied out.
	 */

	msg_freehdr(msghdr);
	wakeup((caddr_t)msqptr);
	uap->sysmsg_result = msgsz;
	return(0);
}

static int
sysctl_msqids(SYSCTL_HANDLER_ARGS)
{

	return (SYSCTL_OUT(req, msqids,
	    sizeof(struct msqid_ds) * msginfo.msgmni));
}

TUNABLE_INT("kern.ipc.msgseg", &msginfo.msgseg);
TUNABLE_INT("kern.ipc.msgssz", &msginfo.msgssz);
TUNABLE_INT("kern.ipc.msgmni", &msginfo.msgmni);

SYSCTL_INT(_kern_ipc, OID_AUTO, msgmax, CTLFLAG_RD, &msginfo.msgmax, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgmni, CTLFLAG_RD, &msginfo.msgmni, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgmnb, CTLFLAG_RD, &msginfo.msgmnb, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgtql, CTLFLAG_RD, &msginfo.msgtql, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgssz, CTLFLAG_RD, &msginfo.msgssz, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, msgseg, CTLFLAG_RD, &msginfo.msgseg, 0, "");
SYSCTL_PROC(_kern_ipc, OID_AUTO, msqids, CTLFLAG_RD,
    NULL, 0, sysctl_msqids, "", "Message queue IDs");
