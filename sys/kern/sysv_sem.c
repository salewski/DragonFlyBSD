/* $FreeBSD: src/sys/kern/sysv_sem.c,v 1.69 2004/03/17 09:37:13 cperciva Exp $ */
/* $DragonFly: src/sys/kern/sysv_sem.c,v 1.14 2004/05/26 14:12:34 hmp Exp $ */

/*
 * Implementation of SVID semaphores
 *
 * Author:  Daniel Boulet
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#include "opt_sysvipc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/jail.h>

static MALLOC_DEFINE(M_SEM, "sem", "SVID compatible semaphores");

static void seminit (void *);

#ifndef _SYS_SYSPROTO_H_
struct __semctl_args;
int __semctl (struct proc *p, struct __semctl_args *uap);
struct semget_args;
int semget (struct proc *p, struct semget_args *uap);
struct semop_args;
int semop (struct proc *p, struct semop_args *uap);
#endif

static struct sem_undo *semu_alloc (struct proc *p);
static int semundo_adjust (struct proc *p, struct sem_undo **supptr, 
		int semid, int semnum, int adjval);
static void semundo_clear (int semid, int semnum);

/* XXX casting to (sy_call_t *) is bogus, as usual. */
static sy_call_t *semcalls[] = {
	(sy_call_t *)__semctl, (sy_call_t *)semget,
	(sy_call_t *)semop
};

static int	semtot = 0;
static struct semid_ds *sema;	/* semaphore id pool */
static struct sem *sem;		/* semaphore pool */
static struct sem_undo *semu_list; /* list of active undo structures */
static int	*semu;		/* undo structure pool */

struct sem {
	u_short	semval;		/* semaphore value */
	pid_t	sempid;		/* pid of last operation */
	u_short	semncnt;	/* # awaiting semval > cval */
	u_short	semzcnt;	/* # awaiting semval = 0 */
};

/*
 * Undo structure (one per process)
 */
struct sem_undo {
	struct	sem_undo *un_next;	/* ptr to next active undo structure */
	struct	proc *un_proc;		/* owner of this structure */
	short	un_cnt;			/* # of active entries */
	struct undo {
		short	un_adjval;	/* adjust on exit values */
		short	un_num;		/* semaphore # */
		int	un_id;		/* semid */
	} un_ent[1];			/* undo entries */
};

/*
 * Configuration parameters
 */
#ifndef SEMMNI
#define SEMMNI	10		/* # of semaphore identifiers */
#endif
#ifndef SEMMNS
#define SEMMNS	60		/* # of semaphores in system */
#endif
#ifndef SEMUME
#define SEMUME	10		/* max # of undo entries per process */
#endif
#ifndef SEMMNU
#define SEMMNU	30		/* # of undo structures in system */
#endif

/* shouldn't need tuning */
#ifndef SEMMAP
#define SEMMAP	30		/* # of entries in semaphore map */
#endif
#ifndef SEMMSL
#define SEMMSL	SEMMNS		/* max # of semaphores per id */
#endif
#ifndef SEMOPM
#define SEMOPM	100		/* max # of operations per semop call */
#endif

#define SEMVMX	32767		/* semaphore maximum value */
#define SEMAEM	16384		/* adjust on exit max value */

/*
 * Due to the way semaphore memory is allocated, we have to ensure that
 * SEMUSZ is properly aligned.
 */

#define SEM_ALIGN(bytes) (((bytes) + (sizeof(long) - 1)) & ~(sizeof(long) - 1))

/* actual size of an undo structure */
#define SEMUSZ	SEM_ALIGN(offsetof(struct sem_undo, un_ent[SEMUME]))

/*
 * Macro to find a particular sem_undo vector
 */
#define SEMU(ix)	((struct sem_undo *)(((intptr_t)semu)+ix * seminfo.semusz))

/*
 * semaphore info struct
 */
struct seminfo seminfo = {
                SEMMAP,         /* # of entries in semaphore map */
                SEMMNI,         /* # of semaphore identifiers */
                SEMMNS,         /* # of semaphores in system */
                SEMMNU,         /* # of undo structures in system */
                SEMMSL,         /* max # of semaphores per id */
                SEMOPM,         /* max # of operations per semop call */
                SEMUME,         /* max # of undo entries per process */
                SEMUSZ,         /* size in bytes of undo structure */
                SEMVMX,         /* semaphore maximum value */
                SEMAEM          /* adjust on exit max value */
};

TUNABLE_INT("kern.ipc.semmap", &seminfo.semmap);
TUNABLE_INT("kern.ipc.semmni", &seminfo.semmni);
TUNABLE_INT("kern.ipc.semmns", &seminfo.semmns);
TUNABLE_INT("kern.ipc.semmnu", &seminfo.semmnu);
TUNABLE_INT("kern.ipc.semmsl", &seminfo.semmsl);
TUNABLE_INT("kern.ipc.semopm", &seminfo.semopm);
TUNABLE_INT("kern.ipc.semume", &seminfo.semume);
TUNABLE_INT("kern.ipc.semusz", &seminfo.semusz);
TUNABLE_INT("kern.ipc.semvmx", &seminfo.semvmx);
TUNABLE_INT("kern.ipc.semaem", &seminfo.semaem);

SYSCTL_INT(_kern_ipc, OID_AUTO, semmap, CTLFLAG_RW, &seminfo.semmap, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmni, CTLFLAG_RD, &seminfo.semmni, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmns, CTLFLAG_RD, &seminfo.semmns, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmnu, CTLFLAG_RD, &seminfo.semmnu, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semmsl, CTLFLAG_RW, &seminfo.semmsl, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semopm, CTLFLAG_RD, &seminfo.semopm, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semume, CTLFLAG_RD, &seminfo.semume, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semusz, CTLFLAG_RD, &seminfo.semusz, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semvmx, CTLFLAG_RW, &seminfo.semvmx, 0, "");
SYSCTL_INT(_kern_ipc, OID_AUTO, semaem, CTLFLAG_RW, &seminfo.semaem, 0, "");

#if 0
RO seminfo.semmap	/* SEMMAP unused */
RO seminfo.semmni
RO seminfo.semmns
RO seminfo.semmnu	/* undo entries per system */
RW seminfo.semmsl
RO seminfo.semopm	/* SEMOPM unused */
RO seminfo.semume
RO seminfo.semusz	/* param - derived from SEMUME for per-proc sizeof */
RO seminfo.semvmx	/* SEMVMX unused - user param */
RO seminfo.semaem	/* SEMAEM unused - user param */
#endif

static void
seminit(dummy)
	void *dummy;
{
	int i;

	sem = malloc(sizeof(struct sem) * seminfo.semmns, M_SEM, M_WAITOK);
	if (sem == NULL)
		panic("sem is NULL");
	sema = malloc(sizeof(struct semid_ds) * seminfo.semmni, M_SEM, M_WAITOK);
	if (sema == NULL)
		panic("sema is NULL");
	semu = malloc(seminfo.semmnu * seminfo.semusz, M_SEM, M_WAITOK);
	if (semu == NULL)
		panic("semu is NULL");

	for (i = 0; i < seminfo.semmni; i++) {
		sema[i].sem_base = 0;
		sema[i].sem_perm.mode = 0;
	}
	for (i = 0; i < seminfo.semmnu; i++) {
		struct sem_undo *suptr = SEMU(i);
		suptr->un_proc = NULL;
	}
	semu_list = NULL;
}
SYSINIT(sysv_sem, SI_SUB_SYSV_SEM, SI_ORDER_FIRST, seminit, NULL)

/*
 * Entry point for all SEM calls
 *
 * semsys_args(int which, a2, a3, ...) (VARARGS)
 */
int
semsys(struct semsys_args *uap)
{
	struct proc *p = curproc;
	unsigned int which = (unsigned int)uap->which;

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	if (which >= sizeof(semcalls)/sizeof(semcalls[0]))
		return (EINVAL);
	bcopy(&uap->a2, &uap->which,
	    sizeof(struct semsys_args) - offsetof(struct semsys_args, a2));
	return ((*semcalls[which])(uap));
}

/*
 * Allocate a new sem_undo structure for a process
 * (returns ptr to structure or NULL if no more room)
 */

static struct sem_undo *
semu_alloc(p)
	struct proc *p;
{
	int i;
	struct sem_undo *suptr;
	struct sem_undo **supptr;
	int attempt;

	/*
	 * Try twice to allocate something.
	 * (we'll purge any empty structures after the first pass so
	 * two passes are always enough)
	 */

	for (attempt = 0; attempt < 2; attempt++) {
		/*
		 * Look for a free structure.
		 * Fill it in and return it if we find one.
		 */

		for (i = 0; i < seminfo.semmnu; i++) {
			suptr = SEMU(i);
			if (suptr->un_proc == NULL) {
				suptr->un_next = semu_list;
				semu_list = suptr;
				suptr->un_cnt = 0;
				suptr->un_proc = p;
				return(suptr);
			}
		}

		/*
		 * We didn't find a free one, if this is the first attempt
		 * then try to free some structures.
		 */

		if (attempt == 0) {
			/* All the structures are in use - try to free some */
			int did_something = 0;

			supptr = &semu_list;
			while ((suptr = *supptr) != NULL) {
				if (suptr->un_cnt == 0)  {
					suptr->un_proc = NULL;
					*supptr = suptr->un_next;
					did_something = 1;
				} else
					supptr = &(suptr->un_next);
			}

			/* If we didn't free anything then just give-up */
			if (!did_something)
				return(NULL);
		} else {
			/*
			 * The second pass failed even though we freed
			 * something after the first pass!
			 * This is IMPOSSIBLE!
			 */
			panic("semu_alloc - second attempt failed");
		}
	}
	return (NULL);
}

/*
 * Adjust a particular entry for a particular proc
 */

static int
semundo_adjust(p, supptr, semid, semnum, adjval)
	struct proc *p;
	struct sem_undo **supptr;
	int semid, semnum;
	int adjval;
{
	struct sem_undo *suptr;
	struct undo *sunptr;
	int i;

	/* Look for and remember the sem_undo if the caller doesn't provide
	   it */

	suptr = *supptr;
	if (suptr == NULL) {
		for (suptr = semu_list; suptr != NULL;
		    suptr = suptr->un_next) {
			if (suptr->un_proc == p) {
				*supptr = suptr;
				break;
			}
		}
		if (suptr == NULL) {
			if (adjval == 0)
				return(0);
			suptr = semu_alloc(p);
			if (suptr == NULL)
				return(ENOSPC);
			*supptr = suptr;
		}
	}

	/*
	 * Look for the requested entry and adjust it (delete if adjval becomes
	 * 0).
	 */
	sunptr = &suptr->un_ent[0];
	for (i = 0; i < suptr->un_cnt; i++, sunptr++) {
		if (sunptr->un_id != semid || sunptr->un_num != semnum)
			continue;
		if (adjval == 0)
			sunptr->un_adjval = 0;
		else
			sunptr->un_adjval += adjval;
		if (sunptr->un_adjval == 0) {
			suptr->un_cnt--;
			if (i < suptr->un_cnt)
				suptr->un_ent[i] =
				    suptr->un_ent[suptr->un_cnt];
		}
		return(0);
	}

	/* Didn't find the right entry - create it */
	if (adjval == 0)
		return(0);
	if (suptr->un_cnt != seminfo.semume) {
		sunptr = &suptr->un_ent[suptr->un_cnt];
		suptr->un_cnt++;
		sunptr->un_adjval = adjval;
		sunptr->un_id = semid; sunptr->un_num = semnum;
	} else
		return(EINVAL);
	return(0);
}

static void
semundo_clear(semid, semnum)
	int semid, semnum;
{
	struct sem_undo *suptr;

	for (suptr = semu_list; suptr != NULL; suptr = suptr->un_next) {
		struct undo *sunptr = &suptr->un_ent[0];
		int i = 0;

		while (i < suptr->un_cnt) {
			if (sunptr->un_id == semid) {
				if (semnum == -1 || sunptr->un_num == semnum) {
					suptr->un_cnt--;
					if (i < suptr->un_cnt) {
						suptr->un_ent[i] =
						  suptr->un_ent[suptr->un_cnt];
						continue;
					}
				}
				if (semnum != -1)
					break;
			}
			i++, sunptr++;
		}
	}
}

/*
 * Note that the user-mode half of this passes a union, not a pointer
 */

int
__semctl(struct __semctl_args *uap)
{
	struct proc *p = curproc;
	int semid = uap->semid;
	int semnum = uap->semnum;
	int cmd = uap->cmd;
	union semun *arg = uap->arg;
	union semun real_arg;
	struct ucred *cred = p->p_ucred;
	int i, rval, eval;
	struct semid_ds sbuf;
	struct semid_ds *semaptr;

#ifdef SEM_DEBUG
	printf("call to semctl(%d, %d, %d, 0x%x)\n", semid, semnum, cmd, arg);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	semid = IPCID_TO_IX(semid);
	if (semid < 0 || semid >= seminfo.semmni)
		return(EINVAL);

	semaptr = &sema[semid];
	if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0 ||
	    semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid))
		return(EINVAL);

	eval = 0;
	rval = 0;

	switch (cmd) {
	case IPC_RMID:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_M)))
			return(eval);
		semaptr->sem_perm.cuid = cred->cr_uid;
		semaptr->sem_perm.uid = cred->cr_uid;
		semtot -= semaptr->sem_nsems;
		for (i = semaptr->sem_base - sem; i < semtot; i++)
			sem[i] = sem[i + semaptr->sem_nsems];
		for (i = 0; i < seminfo.semmni; i++) {
			if ((sema[i].sem_perm.mode & SEM_ALLOC) &&
			    sema[i].sem_base > semaptr->sem_base)
				sema[i].sem_base -= semaptr->sem_nsems;
		}
		semaptr->sem_perm.mode = 0;
		semundo_clear(semid, -1);
		wakeup((caddr_t)semaptr);
		break;

	case IPC_SET:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_M)))
			return(eval);
		if ((eval = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(eval);
		if ((eval = copyin(real_arg.buf, (caddr_t)&sbuf,
		    sizeof(sbuf))) != 0)
			return(eval);
		semaptr->sem_perm.uid = sbuf.sem_perm.uid;
		semaptr->sem_perm.gid = sbuf.sem_perm.gid;
		semaptr->sem_perm.mode = (semaptr->sem_perm.mode & ~0777) |
		    (sbuf.sem_perm.mode & 0777);
		semaptr->sem_ctime = time_second;
		break;

	case IPC_STAT:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if ((eval = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(eval);
		eval = copyout((caddr_t)semaptr, real_arg.buf,
		    sizeof(struct semid_ds));
		break;

	case GETNCNT:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semncnt;
		break;

	case GETPID:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].sempid;
		break;

	case GETVAL:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semval;
		break;

	case GETALL:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if ((eval = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(eval);
		for (i = 0; i < semaptr->sem_nsems; i++) {
			eval = copyout((caddr_t)&semaptr->sem_base[i].semval,
			    &real_arg.array[i], sizeof(real_arg.array[0]));
			if (eval != 0)
				break;
		}
		break;

	case GETZCNT:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_R)))
			return(eval);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semzcnt;
		break;

	case SETVAL:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_W)))
			return(eval);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		if ((eval = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(eval);
		semaptr->sem_base[semnum].semval = real_arg.val;
		semundo_clear(semid, semnum);
		wakeup((caddr_t)semaptr);
		break;

	case SETALL:
		if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_W)))
			return(eval);
		if ((eval = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(eval);
		for (i = 0; i < semaptr->sem_nsems; i++) {
			eval = copyin(&real_arg.array[i],
			    (caddr_t)&semaptr->sem_base[i].semval,
			    sizeof(real_arg.array[0]));
			if (eval != 0)
				break;
		}
		semundo_clear(semid, -1);
		wakeup((caddr_t)semaptr);
		break;

	default:
		return(EINVAL);
	}

	if (eval == 0)
		uap->sysmsg_result = rval;
	return(eval);
}

int
semget(struct semget_args *uap)
{
	struct proc *p = curproc;
	int semid, eval;
	int key = uap->key;
	int nsems = uap->nsems;
	int semflg = uap->semflg;
	struct ucred *cred = p->p_ucred;

#ifdef SEM_DEBUG
	printf("semget(0x%x, %d, 0%o)\n", key, nsems, semflg);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	if (key != IPC_PRIVATE) {
		for (semid = 0; semid < seminfo.semmni; semid++) {
			if ((sema[semid].sem_perm.mode & SEM_ALLOC) &&
			    sema[semid].sem_perm.key == key)
				break;
		}
		if (semid < seminfo.semmni) {
#ifdef SEM_DEBUG
			printf("found public key\n");
#endif
			if ((eval = ipcperm(p, &sema[semid].sem_perm,
			    semflg & 0700)))
				return(eval);
			if (nsems > 0 && sema[semid].sem_nsems < nsems) {
#ifdef SEM_DEBUG
				printf("too small\n");
#endif
				return(EINVAL);
			}
			if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
#ifdef SEM_DEBUG
				printf("not exclusive\n");
#endif
				return(EEXIST);
			}
			goto found;
		}
	}

#ifdef SEM_DEBUG
	printf("need to allocate the semid_ds\n");
#endif
	if (key == IPC_PRIVATE || (semflg & IPC_CREAT)) {
		if (nsems <= 0 || nsems > seminfo.semmsl) {
#ifdef SEM_DEBUG
			printf("nsems out of range (0<%d<=%d)\n", nsems,
			    seminfo.semmsl);
#endif
			return(EINVAL);
		}
		if (nsems > seminfo.semmns - semtot) {
#ifdef SEM_DEBUG
			printf("not enough semaphores left (need %d, got %d)\n",
			    nsems, seminfo.semmns - semtot);
#endif
			return(ENOSPC);
		}
		for (semid = 0; semid < seminfo.semmni; semid++) {
			if ((sema[semid].sem_perm.mode & SEM_ALLOC) == 0)
				break;
		}
		if (semid == seminfo.semmni) {
#ifdef SEM_DEBUG
			printf("no more semid_ds's available\n");
#endif
			return(ENOSPC);
		}
#ifdef SEM_DEBUG
		printf("semid %d is available\n", semid);
#endif
		sema[semid].sem_perm.key = key;
		sema[semid].sem_perm.cuid = cred->cr_uid;
		sema[semid].sem_perm.uid = cred->cr_uid;
		sema[semid].sem_perm.cgid = cred->cr_gid;
		sema[semid].sem_perm.gid = cred->cr_gid;
		sema[semid].sem_perm.mode = (semflg & 0777) | SEM_ALLOC;
		sema[semid].sem_perm.seq =
		    (sema[semid].sem_perm.seq + 1) & 0x7fff;
		sema[semid].sem_nsems = nsems;
		sema[semid].sem_otime = 0;
		sema[semid].sem_ctime = time_second;
		sema[semid].sem_base = &sem[semtot];
		semtot += nsems;
		bzero(sema[semid].sem_base,
		    sizeof(sema[semid].sem_base[0])*nsems);
#ifdef SEM_DEBUG
		printf("sembase = 0x%x, next = 0x%x\n", sema[semid].sem_base,
		    &sem[semtot]);
#endif
	} else {
#ifdef SEM_DEBUG
		printf("didn't find it and wasn't asked to create it\n");
#endif
		return(ENOENT);
	}

found:
	uap->sysmsg_result = IXSEQ_TO_IPCID(semid, sema[semid].sem_perm);
	return(0);
}

int
semop(struct semop_args *uap)
{
	struct proc *p = curproc;
	int semid = uap->semid;
	u_int nsops = uap->nsops;
	struct sembuf sops[MAX_SOPS];
	struct semid_ds *semaptr;
	struct sembuf *sopptr;
	struct sem *semptr;
	struct sem_undo *suptr = NULL;
	int i, j, eval;
	int do_wakeup, do_undos;

#ifdef SEM_DEBUG
	printf("call to semop(%d, 0x%x, %u)\n", semid, sops, nsops);
#endif

	if (!jail_sysvipc_allowed && p->p_ucred->cr_prison != NULL)
		return (ENOSYS);

	semid = IPCID_TO_IX(semid);	/* Convert back to zero origin */

	if (semid < 0 || semid >= seminfo.semmni)
		return(EINVAL);

	semaptr = &sema[semid];
	if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0)
		return(EINVAL);
	if (semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid))
		return(EINVAL);

	if ((eval = ipcperm(p, &semaptr->sem_perm, IPC_W))) {
#ifdef SEM_DEBUG
		printf("eval = %d from ipaccess\n", eval);
#endif
		return(eval);
	}

	if (nsops > MAX_SOPS) {
#ifdef SEM_DEBUG
		printf("too many sops (max=%d, nsops=%u)\n", MAX_SOPS, nsops);
#endif
		return(E2BIG);
	}

	if ((eval = copyin(uap->sops, &sops, nsops * sizeof(sops[0]))) != 0) {
#ifdef SEM_DEBUG
		printf("eval = %d from copyin(%08x, %08x, %u)\n", eval,
		    uap->sops, &sops, nsops * sizeof(sops[0]));
#endif
		return(eval);
	}

	/*
	 * Loop trying to satisfy the vector of requests.
	 * If we reach a point where we must wait, any requests already
	 * performed are rolled back and we go to sleep until some other
	 * process wakes us up.  At this point, we start all over again.
	 *
	 * This ensures that from the perspective of other tasks, a set
	 * of requests is atomic (never partially satisfied).
	 */
	do_undos = 0;

	for (;;) {
		do_wakeup = 0;

		for (i = 0; i < nsops; i++) {
			sopptr = &sops[i];

			if (sopptr->sem_num >= semaptr->sem_nsems)
				return(EFBIG);

			semptr = &semaptr->sem_base[sopptr->sem_num];

#ifdef SEM_DEBUG
			printf("semop:  semaptr=%x, sem_base=%x, semptr=%x, sem[%d]=%d : op=%d, flag=%s\n",
			    semaptr, semaptr->sem_base, semptr,
			    sopptr->sem_num, semptr->semval, sopptr->sem_op,
			    (sopptr->sem_flg & IPC_NOWAIT) ? "nowait" : "wait");
#endif

			if (sopptr->sem_op < 0) {
				if (semptr->semval + sopptr->sem_op < 0) {
#ifdef SEM_DEBUG
					printf("semop:  can't do it now\n");
#endif
					break;
				} else {
					semptr->semval += sopptr->sem_op;
					if (semptr->semval == 0 &&
					    semptr->semzcnt > 0)
						do_wakeup = 1;
				}
				if (sopptr->sem_flg & SEM_UNDO)
					do_undos = 1;
			} else if (sopptr->sem_op == 0) {
				if (semptr->semval > 0) {
#ifdef SEM_DEBUG
					printf("semop:  not zero now\n");
#endif
					break;
				}
			} else {
				if (semptr->semncnt > 0)
					do_wakeup = 1;
				semptr->semval += sopptr->sem_op;
				if (sopptr->sem_flg & SEM_UNDO)
					do_undos = 1;
			}
		}

		/*
		 * Did we get through the entire vector?
		 */
		if (i >= nsops)
			goto done;

		/*
		 * No ... rollback anything that we've already done
		 */
#ifdef SEM_DEBUG
		printf("semop:  rollback 0 through %d\n", i-1);
#endif
		for (j = 0; j < i; j++)
			semaptr->sem_base[sops[j].sem_num].semval -=
			    sops[j].sem_op;

		/*
		 * If the request that we couldn't satisfy has the
		 * NOWAIT flag set then return with EAGAIN.
		 */
		if (sopptr->sem_flg & IPC_NOWAIT)
			return(EAGAIN);

		if (sopptr->sem_op == 0)
			semptr->semzcnt++;
		else
			semptr->semncnt++;

#ifdef SEM_DEBUG
		printf("semop:  good night!\n");
#endif
		eval = tsleep((caddr_t)semaptr, PCATCH, "semwait", 0);
#ifdef SEM_DEBUG
		printf("semop:  good morning (eval=%d)!\n", eval);
#endif

		suptr = NULL;	/* sem_undo may have been reallocated */

		/* return code is checked below, after sem[nz]cnt-- */

		/*
		 * Make sure that the semaphore still exists
		 */
		if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0 ||
		    semaptr->sem_perm.seq != IPCID_TO_SEQ(uap->semid))
			return(EIDRM);

		/*
		 * The semaphore is still alive.  Readjust the count of
		 * waiting processes.
		 */
		if (sopptr->sem_op == 0)
			semptr->semzcnt--;
		else
			semptr->semncnt--;

		/*
		 * Is it really morning, or was our sleep interrupted?
		 * (Delayed check of msleep() return code because we
		 * need to decrement sem[nz]cnt either way.)
		 */
		if (eval != 0)
			return(EINTR);
#ifdef SEM_DEBUG
		printf("semop:  good morning!\n");
#endif
	}

done:
	/*
	 * Process any SEM_UNDO requests.
	 */
	if (do_undos) {
		for (i = 0; i < nsops; i++) {
			/*
			 * We only need to deal with SEM_UNDO's for non-zero
			 * op's.
			 */
			int adjval;

			if ((sops[i].sem_flg & SEM_UNDO) == 0)
				continue;
			adjval = sops[i].sem_op;
			if (adjval == 0)
				continue;
			eval = semundo_adjust(p, &suptr, semid,
			    sops[i].sem_num, -adjval);
			if (eval == 0)
				continue;

			/*
			 * Oh-Oh!  We ran out of either sem_undo's or undo's.
			 * Rollback the adjustments to this point and then
			 * rollback the semaphore ups and down so we can return
			 * with an error with all structures restored.  We
			 * rollback the undo's in the exact reverse order that
			 * we applied them.  This guarantees that we won't run
			 * out of space as we roll things back out.
			 */
			for (j = i - 1; j >= 0; j--) {
				if ((sops[j].sem_flg & SEM_UNDO) == 0)
					continue;
				adjval = sops[j].sem_op;
				if (adjval == 0)
					continue;
				if (semundo_adjust(p, &suptr, semid,
				    sops[j].sem_num, adjval) != 0)
					panic("semop - can't undo undos");
			}

			for (j = 0; j < nsops; j++)
				semaptr->sem_base[sops[j].sem_num].semval -=
				    sops[j].sem_op;

#ifdef SEM_DEBUG
			printf("eval = %d from semundo_adjust\n", eval);
#endif
			return(eval);
		} /* loop through the sops */
	} /* if (do_undos) */

	/* We're definitely done - set the sempid's */
	for (i = 0; i < nsops; i++) {
		sopptr = &sops[i];
		semptr = &semaptr->sem_base[sopptr->sem_num];
		semptr->sempid = p->p_pid;
	}

	/* Do a wakeup if any semaphore was up'd. */
	if (do_wakeup) {
#ifdef SEM_DEBUG
		printf("semop:  doing wakeup\n");
#endif
		wakeup((caddr_t)semaptr);
#ifdef SEM_DEBUG
		printf("semop:  back from wakeup\n");
#endif
	}
#ifdef SEM_DEBUG
	printf("semop:  done\n");
#endif
	uap->sysmsg_result = 0;
	return(0);
}

/*
 * Go through the undo structures for this process and apply the adjustments to
 * semaphores.
 */
void
semexit(p)
	struct proc *p;
{
	struct sem_undo *suptr;
	struct sem_undo **supptr;
	int did_something;

	did_something = 0;

	/*
	 * Go through the chain of undo vectors looking for one
	 * associated with this process.
	 */

	for (supptr = &semu_list; (suptr = *supptr) != NULL;
	    supptr = &suptr->un_next) {
		if (suptr->un_proc == p)
			break;
	}

	if (suptr == NULL)
		return;

#ifdef SEM_DEBUG
	printf("proc @%08x has undo structure with %d entries\n", p,
	    suptr->un_cnt);
#endif

	/*
	 * If there are any active undo elements then process them.
	 */
	if (suptr->un_cnt > 0) {
		int ix;

		for (ix = 0; ix < suptr->un_cnt; ix++) {
			int semid = suptr->un_ent[ix].un_id;
			int semnum = suptr->un_ent[ix].un_num;
			int adjval = suptr->un_ent[ix].un_adjval;
			struct semid_ds *semaptr;

			semaptr = &sema[semid];
			if ((semaptr->sem_perm.mode & SEM_ALLOC) == 0)
				panic("semexit - semid not allocated");
			if (semnum >= semaptr->sem_nsems)
				panic("semexit - semnum out of range");

#ifdef SEM_DEBUG
			printf("semexit:  %08x id=%d num=%d(adj=%d) ; sem=%d\n",
			    suptr->un_proc, suptr->un_ent[ix].un_id,
			    suptr->un_ent[ix].un_num,
			    suptr->un_ent[ix].un_adjval,
			    semaptr->sem_base[semnum].semval);
#endif

			if (adjval < 0) {
				if (semaptr->sem_base[semnum].semval < -adjval)
					semaptr->sem_base[semnum].semval = 0;
				else
					semaptr->sem_base[semnum].semval +=
					    adjval;
			} else
				semaptr->sem_base[semnum].semval += adjval;

			wakeup((caddr_t)semaptr);
#ifdef SEM_DEBUG
			printf("semexit:  back from wakeup\n");
#endif
		}
	}

	/*
	 * Deallocate the undo vector.
	 */
#ifdef SEM_DEBUG
	printf("removing vector\n");
#endif
	suptr->un_proc = NULL;
	*supptr = suptr->un_next;
}
