/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and code derived from software contributed to
 * Berkeley by William Jolitz.
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
 *	from: Utah $Hdr: mem.c 1.13 89/10/08$
 *	from: @(#)mem.c	7.2 (Berkeley) 5/9/91
 * $FreeBSD: src/sys/i386/i386/mem.c,v 1.79.2.9 2003/01/04 22:58:01 njl Exp $
 * $DragonFly: src/sys/i386/i386/Attic/mem.c,v 1.11 2004/05/19 22:52:57 dillon Exp $
 */

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <machine/frame.h>
#include <machine/psl.h>
#include <machine/specialreg.h>
#include <i386/isa/intr_machdep.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>


static	d_open_t	mmopen;
static	d_close_t	mmclose;
static	d_read_t	mmrw;
static	d_ioctl_t	mmioctl;
static	d_mmap_t	memmmap;
static	d_poll_t	mmpoll;

#define CDEV_MAJOR 2
static struct cdevsw mem_cdevsw = {
	/* name */	"mem",
	/* maj */	CDEV_MAJOR,
	/* flags */	D_MEM,
	/* port */	NULL,
	/* clone */	NULL,

	/* open */	mmopen,
	/* close */	mmclose,
	/* read */	mmrw,
	/* write */	mmrw,
	/* ioctl */	mmioctl,
	/* poll */	mmpoll,
	/* mmap */	memmmap,
	/* strategy */	nostrategy,
	/* dump */	nodump,
	/* psize */	nopsize
};

static int rand_bolt;
static caddr_t	zbuf;

MALLOC_DEFINE(M_MEMDESC, "memdesc", "memory range descriptors");
static int mem_ioctl (dev_t, u_long, caddr_t, int, struct thread *);
static int random_ioctl (dev_t, u_long, caddr_t, int, struct thread *);

struct mem_range_softc mem_range_softc;


static int
mmclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct proc *p = td->td_proc;

	switch (minor(dev)) {
	case 14:
		p->p_md.md_regs->tf_eflags &= ~PSL_IOPL;
		break;
	default:
		break;
	}
	return (0);
}

static int
mmopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	int error;
	struct proc *p = td->td_proc;

	switch (minor(dev)) {
	case 0:
	case 1:
		if ((flags & FWRITE) && securelevel > 0)
			return (EPERM);
		break;
	case 14:
		error = suser(td);
		if (error != 0)
			return (error);
		if (securelevel > 0)
			return (EPERM);
		p->p_md.md_regs->tf_eflags |= PSL_IOPL;
		break;
	default:
		break;
	}
	return (0);
}

static int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int o;
	u_int c, v;
	u_int poolsize;
	struct iovec *iov;
	int error = 0;
	caddr_t buf = NULL;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {

/* minor device 0 is physical memory */
		case 0:
			v = uio->uio_offset;
			v &= ~PAGE_MASK;
			pmap_kenter((vm_offset_t)ptvmmap, v);
			o = (int)uio->uio_offset & PAGE_MASK;
			c = (u_int)(PAGE_SIZE - ((int)iov->iov_base & PAGE_MASK));
			c = min(c, (u_int)(PAGE_SIZE - o));
			c = min(c, (u_int)iov->iov_len);
			error = uiomove((caddr_t)&ptvmmap[o], (int)c, uio);
			pmap_kremove((vm_offset_t)ptvmmap);
			continue;

/* minor device 1 is kernel memory */
		case 1: {
			vm_offset_t addr, eaddr;
			c = iov->iov_len;

			/*
			 * Make sure that all of the pages are currently resident so
			 * that we don't create any zero-fill pages.
			 */
			addr = trunc_page(uio->uio_offset);
			eaddr = round_page(uio->uio_offset + c);

			if (addr < (vm_offset_t)VADDR(PTDPTDI, 0))
				return EFAULT;
			if (eaddr >= (vm_offset_t)VADDR(APTDPTDI, 0))
				return EFAULT;
			for (; addr < eaddr; addr += PAGE_SIZE) 
				if (pmap_extract(kernel_pmap, addr) == 0)
					return EFAULT;
			
			if (!kernacc((caddr_t)(int)uio->uio_offset, c,
			    uio->uio_rw == UIO_READ ? 
			    VM_PROT_READ : VM_PROT_WRITE))
				return (EFAULT);
			error = uiomove((caddr_t)(int)uio->uio_offset, (int)c, uio);
			continue;
		}

/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_READ)
				return (0);
			c = iov->iov_len;
			break;

/* minor device 3 (/dev/random) is source of filth on read, rathole on write */
		case 3:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (buf == NULL)
				buf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
			c = min(iov->iov_len, PAGE_SIZE);
			poolsize = read_random(buf, c);
			if (poolsize == 0) {
				if (buf)
					free(buf, M_TEMP);
				if ((flags & IO_NDELAY) != 0)
					return (EWOULDBLOCK);
				return (0);
			}
			c = min(c, poolsize);
			error = uiomove(buf, (int)c, uio);
			continue;

/* minor device 4 (/dev/urandom) is source of muck on read, rathole on write */
		case 4:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (CURSIG(curproc) != 0) {
				/*
				 * Use tsleep() to get the error code right.
				 * It should return immediately.
				 */
				error = tsleep(&rand_bolt, PCATCH, "urand", 1);
				if (error != 0 && error != EWOULDBLOCK)
					continue;
			}
			if (buf == NULL)
				buf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
			c = min(iov->iov_len, PAGE_SIZE);
			poolsize = read_random_unlimited(buf, c);
			c = min(c, poolsize);
			error = uiomove(buf, (int)c, uio);
			continue;

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (zbuf == NULL) {
				zbuf = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				bzero(zbuf, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(zbuf, (int)c, uio);
			continue;

		default:
			return (ENODEV);
		}
		if (error)
			break;
		iov->iov_base += c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}
	if (buf)
		free(buf, M_TEMP);
	return (error);
}




/*******************************************************\
* allow user processes to MMAP some memory sections	*
* instead of going through read/write			*
\*******************************************************/
static int
memmmap(dev_t dev, vm_offset_t offset, int nprot)
{
	switch (minor(dev))
	{

/* minor device 0 is physical memory */
	case 0:
        	return i386_btop(offset);

/* minor device 1 is kernel memory */
	case 1:
        	return i386_btop(vtophys(offset));

	default:
		return -1;
	}
}

static int
mmioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{

	switch (minor(dev)) {
	case 0:
		return mem_ioctl(dev, cmd, data, flags, td);
	case 3:
	case 4:
		return random_ioctl(dev, cmd, data, flags, td);
	}
	return (ENODEV);
}

/*
 * Operations for changing memory attributes.
 *
 * This is basically just an ioctl shim for mem_range_attr_get
 * and mem_range_attr_set.
 */
static int 
mem_ioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	int nd, error = 0;
	struct mem_range_op *mo = (struct mem_range_op *)data;
	struct mem_range_desc *md;
	
	/* is this for us? */
	if ((cmd != MEMRANGE_GET) &&
	    (cmd != MEMRANGE_SET))
		return (ENOTTY);

	/* any chance we can handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	/* do we have any descriptors? */
	if (mem_range_softc.mr_ndesc == 0)
		return (ENXIO);

	switch (cmd) {
	case MEMRANGE_GET:
		nd = imin(mo->mo_arg[0], mem_range_softc.mr_ndesc);
		if (nd > 0) {
			md = (struct mem_range_desc *)
				malloc(nd * sizeof(struct mem_range_desc),
				       M_MEMDESC, M_WAITOK);
			error = mem_range_attr_get(md, &nd);
			if (!error)
				error = copyout(md, mo->mo_desc, 
					nd * sizeof(struct mem_range_desc));
			free(md, M_MEMDESC);
		} else {
			nd = mem_range_softc.mr_ndesc;
		}
		mo->mo_arg[0] = nd;
		break;
		
	case MEMRANGE_SET:
		md = (struct mem_range_desc *)malloc(sizeof(struct mem_range_desc),
						    M_MEMDESC, M_WAITOK);
		error = copyin(mo->mo_desc, md, sizeof(struct mem_range_desc));
		/* clamp description string */
		md->mr_owner[sizeof(md->mr_owner) - 1] = 0;
		if (error == 0)
			error = mem_range_attr_set(md, &mo->mo_arg[0]);
		free(md, M_MEMDESC);
		break;
	}
	return (error);
}

/*
 * Implementation-neutral, kernel-callable functions for manipulating
 * memory range attributes.
 */
int
mem_range_attr_get(mrd, arg)
	struct mem_range_desc *mrd;
	int *arg;
{
	/* can we handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	if (*arg == 0) {
		*arg = mem_range_softc.mr_ndesc;
	} else {
		bcopy(mem_range_softc.mr_desc, mrd, (*arg) * sizeof(struct mem_range_desc));
	}
	return (0);
}

int
mem_range_attr_set(mrd, arg)
	struct mem_range_desc *mrd;
	int *arg;
{
	/* can we handle this? */
	if (mem_range_softc.mr_op == NULL)
		return (EOPNOTSUPP);

	return (mem_range_softc.mr_op->set(&mem_range_softc, mrd, arg));
}

#ifdef SMP
void
mem_range_AP_init(void)
{
	if (mem_range_softc.mr_op && mem_range_softc.mr_op->initAP)
		return (mem_range_softc.mr_op->initAP(&mem_range_softc));
}
#endif

static int 
random_ioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	static intrmask_t interrupt_allowed;
	intrmask_t interrupt_mask;
	int error, intr;
	
	/*
	 * We're the random or urandom device.  The only ioctls are for
	 * selecting and inspecting which interrupts are used in the muck
	 * gathering business and the fcntl() stuff.
	 */
	if (cmd != MEM_SETIRQ && cmd != MEM_CLEARIRQ && cmd != MEM_RETURNIRQ
		&& cmd != FIONBIO && cmd != FIOASYNC)
		return (ENOTTY);

	/*
	 * XXX the data is 16-bit due to a historical botch, so we use
	 * magic 16's instead of ICU_LEN and can't support 24 interrupts
	 * under SMP.
	 * Even inspecting the state is privileged, since it gives a hint
	 * about how easily the randomness might be guessed.
	 */
	intr = *(int16_t *)data;
	interrupt_mask = 1 << intr;
	switch (cmd) {
	/* Really handled in upper layer */
	case FIOASYNC:
	case FIONBIO:
		break;
	case MEM_SETIRQ:
		error = suser(td);
		if (error != 0)
			return (error);
		if (intr < 0 || intr >= 16)
			return (EINVAL);
		if (interrupt_allowed & interrupt_mask)
			break;
		interrupt_allowed |= interrupt_mask;
		register_randintr(intr);
		break;
	case MEM_CLEARIRQ:
		error = suser(td);
		if (error != 0)
			return (error);
		if (intr < 0 || intr >= 16)
			return (EINVAL);
		if (!(interrupt_allowed & interrupt_mask))
			break;
		interrupt_allowed &= ~interrupt_mask;
		unregister_randintr(intr);
		break;
	case MEM_RETURNIRQ:
		error = suser(td);
		if (error != 0)
			return (error);
		*(u_int16_t *)data = interrupt_allowed;
		break;
	}
	return (0);
}

int
mmpoll(dev_t dev, int events, struct thread *td)
{
	switch (minor(dev)) {
	case 3:		/* /dev/random */
		return random_poll(dev, events, td);
	case 4:		/* /dev/urandom */
	default:
		return seltrue(dev, events, td);
	}
}

int
iszerodev(dev)
	dev_t dev;
{
	return ((major(dev) == mem_cdevsw.d_maj)
	  && minor(dev) == 12);
}

static void
mem_drvinit(void *unused)
{

	/* Initialise memory range handling */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->init(&mem_range_softc);

	cdevsw_add(&mem_cdevsw, 0xf0, 0);
	make_dev(&mem_cdevsw, 0, UID_ROOT, GID_KMEM, 0640, "mem");
	make_dev(&mem_cdevsw, 1, UID_ROOT, GID_KMEM, 0640, "kmem");
	make_dev(&mem_cdevsw, 2, UID_ROOT, GID_WHEEL, 0666, "null");
	make_dev(&mem_cdevsw, 3, UID_ROOT, GID_WHEEL, 0644, "random");
	make_dev(&mem_cdevsw, 4, UID_ROOT, GID_WHEEL, 0644, "urandom");
	make_dev(&mem_cdevsw, 12, UID_ROOT, GID_WHEEL, 0666, "zero");
	make_dev(&mem_cdevsw, 14, UID_ROOT, GID_WHEEL, 0600, "io");
}

SYSINIT(memdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,mem_drvinit,NULL)

