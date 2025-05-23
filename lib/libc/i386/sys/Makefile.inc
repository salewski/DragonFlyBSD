#	from: Makefile.inc,v 1.1 1993/09/03 19:04:23 jtc Exp
# $FreeBSD: src/lib/libc/i386/sys/Makefile.inc,v 1.17.2.3 2002/10/15 19:46:46 fjoe Exp $
# $DragonFly: src/lib/libc/i386/sys/Makefile.inc,v 1.2 2003/06/17 04:26:43 dillon Exp $

SRCS+=	i386_clr_watch.c i386_get_ioperm.c i386_get_ldt.c i386_set_ioperm.c \
	i386_set_ldt.c i386_set_watch.c i386_vm86.c

MDASM=	Ovfork.S brk.S cerror.S exect.S fork.S pipe.S ptrace.S reboot.S \
	rfork.S sbrk.S setlogin.S sigreturn.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	__semctl.o break.o exit.o ftruncate.o getdomainname.o getlogin.o \
	lseek.o mlockall.o mmap.o msgctl.o msgget.o msgrcv.o msgsnd.o \
	munlockall.o openbsd_poll.o pread.o pwrite.o semconfig.o semget.o \
	semop.o setdomainname.o shmat.o shmctl.o shmdt.o shmget.o sstk.o \
	thr_sleep.o thr_wakeup.o truncate.o uname.o vfork.o yield.o

PSEUDO=	_getlogin.o

# Pseudo syscalls that are renamed as __sys_{pseudo} when
# building libc_r.
PSEUDOR=	_exit.o

.if ${LIB} == "c"
MAN+=	i386_get_ioperm.2 i386_get_ldt.2 i386_vm86.2
MAN+=	i386_set_watch.3

MLINKS+=i386_get_ioperm.2 i386_set_ioperm.2
MLINKS+=i386_get_ldt.2 i386_set_ldt.2
MLINKS+=i386_set_watch.3 i386_clr_watch.3
.endif
