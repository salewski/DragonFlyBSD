/*-
 * Copyright (c) 2003 Kip Macy
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
 * $DragonFly: src/sys/kern/kern_checkpoint.c,v 1.2 2005/02/26 20:32:36 dillon Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/nlookup.h>

#include <sys/file.h>
/* only on dragonfly */
#include <sys/file2.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <vm/vm_param.h>
#include <vm/vm.h>
#include <sys/imgact_elf.h>
#include <sys/procfs.h>

#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <sys/mman.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <machine/limits.h>
#include <i386/include/frame.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <i386/include/sigframe.h>
#include <sys/exec.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <sys/kern_syscall.h>
#include <sys/checkpoint.h>
#include <sys/mount.h>
#include <sys/ckpt.h>


static int elf_loadphdrs(struct file *fp,  Elf_Phdr *phdr, int numsegs);
static int elf_getnotes(struct proc *p, struct file *fp, size_t notesz);
static int elf_demarshalnotes(void *src, prpsinfo_t *psinfo,
		 prstatus_t *status, prfpregset_t *fpregset, int nthreads); 
static int elf_loadnotes(struct proc *, prpsinfo_t *, prstatus_t *, 
		 prfpregset_t *);
static int elf_getsigs(struct proc *p, struct file *fp); 
static int elf_getfiles(struct proc *p, struct file *fp);
static int elf_gettextvp(struct proc *p, struct file *fp);
static char *ckpt_expand_name(const char *name, uid_t uid, pid_t pid);

static int ckptgroup = 0;       /* wheel only, -1 for any group */
SYSCTL_INT(_kern, OID_AUTO, ckptgroup, CTLFLAG_RW, &ckptgroup, 0, "");

/* ref count to see how many processes that are being checkpointed */
static int chptinuse = 0;

static __inline
int
read_check(struct file *fp, void *buf, size_t nbyte)
{
	size_t nread;
	int error;

        PRINTF(("reading %d bytes\n", nbyte));
	error = fp_read(fp, buf, nbyte, &nread);
	if (error) {
                PRINTF(("read failed - %d", error));
	} else if (nread != nbyte) {
                PRINTF(("wanted to read %d - read %d\n", nbyte, nread));
		error = EINVAL;
	}
	return error;
}

static int
elf_gethdr(struct file *fp, Elf_Ehdr *ehdr) 
{
	size_t nbyte = sizeof(Elf_Ehdr);
	int error;

	if ((error = read_check(fp, ehdr, nbyte)) != 0)
		goto done;
	if (!(ehdr->e_ehsize == sizeof(Elf_Ehdr))) {
		PRINTF(("wrong elf header size: %d\n"
		       "expected size        : %d\n", 
		       ehdr->e_ehsize, sizeof(Elf_Ehdr)));
		return EINVAL;
	}
	if (!(ehdr->e_phentsize == sizeof(Elf_Phdr))) {
		PRINTF(("wrong program header size: %d\n"
		       "expected size            : %d\n",  
		       ehdr->e_phentsize, sizeof(Elf_Phdr)));
		return EINVAL;
	}

	if (!(ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
	      ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
	      ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
	      ehdr->e_ident[EI_MAG3] == ELFMAG3 &&
	      ehdr->e_ident[EI_CLASS] == ELF_CLASS &&
	      ehdr->e_ident[EI_DATA] == ELF_DATA &&
	      ehdr->e_ident[EI_VERSION] == EV_CURRENT &&
	      ehdr->e_ident[EI_OSABI] == ELFOSABI_FREEBSD &&
	      ehdr->e_ident[EI_ABIVERSION] == 0)) {
		PRINTF(("bad elf header\n there are %d segments\n",
		       ehdr->e_phnum));
		return EINVAL;

	}
	PRINTF(("Elf header size:           %d\n", ehdr->e_ehsize));
	PRINTF(("Program header size:       %d\n", ehdr->e_phentsize));
	PRINTF(("Number of Program headers: %d\n", ehdr->e_phnum));
 done:
	return error;
} 

static int
elf_getphdrs(struct file *fp, Elf_Phdr *phdr, size_t nbyte) 
{
	int i;
	int error;
	int nheaders = nbyte/sizeof(Elf_Phdr); 

	PRINTF(("reading phdrs section\n"));
	if ((error = read_check(fp, phdr, nbyte)) != 0)
		goto done;
	printf("headers section:\n");
	for (i = 0; i < nheaders; i++) {
		printf("entry type:   %d\n", phdr[i].p_type);
		printf("file offset:  %d\n", phdr[i].p_offset);
		printf("virt address: %p\n", (uint32_t *)phdr[i].p_vaddr);
		printf("file size:    %d\n", phdr[i].p_filesz);
		printf("memory size:  %d\n", phdr[i].p_memsz);
		printf("\n");
	}
 done:
	return error;
}


static int
elf_getnotes(struct proc *p, struct file *fp, size_t notesz) 
{
	int error;
	int nthreads;
	char *note;
	prpsinfo_t *psinfo;
	prstatus_t *status;
	prfpregset_t *fpregset;

	nthreads = (notesz - sizeof(prpsinfo_t))/(sizeof(prstatus_t) + 
						  sizeof(prfpregset_t));
	PRINTF(("reading notes header nthreads=%d\n", nthreads));
	if (nthreads <= 0 || nthreads > CKPT_MAXTHREADS)
		return EINVAL;

	psinfo  = malloc(sizeof(prpsinfo_t), M_TEMP, M_ZERO | M_WAITOK);
	status  = malloc(nthreads*sizeof(prstatus_t), M_TEMP, M_WAITOK);
	fpregset  = malloc(nthreads*sizeof(prfpregset_t), M_TEMP, M_WAITOK);
	note = malloc(notesz, M_TEMP, M_WAITOK);

	
	PRINTF(("reading notes section\n"));
	if ((error = read_check(fp, note, notesz)) != 0)
		goto done;
	error = elf_demarshalnotes(note, psinfo, status, fpregset, nthreads);
	if (error)
		goto done;
	/* fetch register state from notes */
	error = elf_loadnotes(p, psinfo, status, fpregset);
 done:
	if (psinfo)
		free(psinfo, M_TEMP);
	if (status)
		free(status, M_TEMP);
	if (fpregset)
		free(fpregset, M_TEMP);
	if (note)
		free(note, M_TEMP);
	return error;
}

static int
ckpt_thaw_proc(struct proc *p, struct file *fp)
{

	Elf_Phdr *phdr = NULL;
	Elf_Ehdr *ehdr = NULL;
	int error;
	size_t nbyte;

	TRACE_ENTER;
	
	ehdr = malloc(sizeof(Elf_Ehdr), M_TEMP, M_ZERO | M_WAITOK);

	if ((error = elf_gethdr(fp, ehdr)) != 0)
		goto done;
	nbyte = sizeof(Elf_Phdr) * ehdr->e_phnum; 
	phdr = malloc(nbyte, M_TEMP, M_WAITOK); 

	/* fetch description of program writable mappings */
	if ((error = elf_getphdrs(fp, phdr, nbyte)) != 0)
		goto done;

	/* fetch notes section containing register state */
	if ((error = elf_getnotes(p, fp, phdr->p_filesz)) != 0)
		goto done;

	/* fetch program text vnodes */
	if ((error = elf_gettextvp(p, fp)) != 0)
		goto done;

	/* fetch signal disposition */
	if ((error = elf_getsigs(p, fp)) != 0) {
		printf("failure in recovering signals\n");
		goto done;
	}

	/* fetch open files */
	if ((error = elf_getfiles(p, fp)) != 0)
		goto done;

	/* handle mappings last in case we are reading from a socket */
	error = elf_loadphdrs(fp, phdr, ehdr->e_phnum);

	/*
	 * Set the textvp to the checkpoint file and mark the vnode so
	 * a future checkpointing of this checkpoint-restored program
	 * will copy out the contents of the mappings rather then trying
	 * to record the vnode info related to the checkpoint file, which
	 * is likely going to be destroyed when the program is re-checkpointed.
	 */
	if (error == 0 && fp->f_data && fp->f_type == DTYPE_VNODE) {
		if (p->p_textvp)
			vrele(p->p_textvp);
		p->p_textvp = (struct vnode *)fp->f_data;
		p->p_textvp->v_flag |= VCKPT;
		vref(p->p_textvp);
	}
done:
	if (ehdr)
		free(ehdr, M_TEMP);
	if (phdr)
		free(phdr, M_TEMP);
	TRACE_EXIT;
	return error;
}

static int
elf_loadnotes(struct proc *p, prpsinfo_t *psinfo, prstatus_t *status, 
	   prfpregset_t *fpregset) 
{
	int error;

	/* validate status and psinfo */
	TRACE_ENTER;
	if (status->pr_version != PRSTATUS_VERSION ||
	    status->pr_statussz != sizeof(prstatus_t) ||
	    status->pr_gregsetsz != sizeof(gregset_t) ||
	    status->pr_fpregsetsz != sizeof(fpregset_t) ||
	    psinfo->pr_version != PRPSINFO_VERSION ||
	    psinfo->pr_psinfosz != sizeof(prpsinfo_t)) {
	        PRINTF(("status check failed\n"));
		error = EINVAL;
		goto done;
	}
	if ((error = set_regs(p, &status->pr_reg)) != 0)
		goto done;
	error = set_fpregs(p, fpregset);
	strlcpy(p->p_comm, psinfo->pr_fname, sizeof(p->p_comm));
	/* XXX psinfo->pr_psargs not yet implemented */
 done:	
	TRACE_EXIT;
	return error;
}

static int 
elf_getnote(void *src, size_t *off, const char *name, unsigned int type,
	    void **desc, size_t descsz) 
{
	Elf_Note note;
	int error;

	TRACE_ENTER;
	if (src == NULL) {
		error = EFAULT;
		goto done;
	}
	bcopy((char *)src + *off, &note, sizeof note);
	
	PRINTF(("at offset: %d expected note of type: %d - got: %d\n",
	       *off, type, note.n_type));
	*off += sizeof note;
	if (type != note.n_type) {
		TRACE_ERR;
		error = EINVAL;
		goto done;
	}
	if (strncmp(name, (char *) src + *off, note.n_namesz) != 0) {
		error = EINVAL;
		goto done;
	}
	*off += roundup2(note.n_namesz, sizeof(Elf_Size));
	if (note.n_descsz != descsz) {
		TRACE_ERR;
		error = EINVAL;
		goto done;
	}
	if (desc)
	        bcopy((char *)src + *off, *desc, note.n_descsz);
	*off += roundup2(note.n_descsz, sizeof(Elf_Size));
	error = 0;
 done:
	TRACE_EXIT;
	return error;
}

static int
elf_demarshalnotes(void *src, prpsinfo_t *psinfo, prstatus_t *status, 
		   prfpregset_t *fpregset, int nthreads) 
{
	int i;
	int error;
	int off = 0;

	TRACE_ENTER;
	error = elf_getnote(src, &off, "FreeBSD", NT_PRSTATUS, 
			   (void **)&status, sizeof(prstatus_t));
	if (error)
		goto done;
	error = elf_getnote(src, &off, "FreeBSD", NT_FPREGSET, 
			   (void **)&fpregset, sizeof(prfpregset_t));
	if (error)
		goto done;
	error = elf_getnote(src, &off, "FreeBSD", NT_PRPSINFO, 
			   (void **)&psinfo, sizeof(prpsinfo_t));
	if (error)
		goto done;

	/*
	 * The remaining portion needs to be an integer multiple
	 * of prstatus_t and prfpregset_t
	 */
	for (i = 0 ; i < nthreads - 1; i++) {
		status++; fpregset++;
		error = elf_getnote(src, &off, "FreeBSD", NT_PRSTATUS, 
				   (void **)&status, sizeof (prstatus_t));
		if (error)
			goto done;
		error = elf_getnote(src, &off, "FreeBSD", NT_FPREGSET, 
				   (void **)&fpregset, sizeof(prfpregset_t));
		if (error)
			goto done;
	}
	
 done:
	TRACE_EXIT;
	return error;
}


static int
mmap_phdr(struct file *fp, Elf_Phdr *phdr) 
{
	int error;
	size_t len;
	int prot;
	void *addr;
	int flags;
	off_t pos;

	TRACE_ENTER;
	pos = phdr->p_offset;
	len = phdr->p_filesz;
	addr = (void *)phdr->p_vaddr;
	flags = MAP_FIXED | MAP_NOSYNC | MAP_PRIVATE;
	prot = 0;
	if (phdr->p_flags & PF_R)
		prot |= PROT_READ;
	if (phdr->p_flags & PF_W)
		prot |= PROT_WRITE;
	if (phdr->p_flags & PF_X)
		prot |= PROT_EXEC;	
	if ((error = fp_mmap(addr, len, prot, flags, fp, pos, &addr)) != 0) {
		PRINTF(("mmap failed: %d\n", error);	   );
	}
	PRINTF(("map @%08x-%08x fileoff %08x-%08x\n", (int)addr, (int)((char *)addr + len), (int)pos, (int)(pos + len)));
	TRACE_EXIT;
	return error;
}


static int
elf_loadphdrs(struct file *fp, Elf_Phdr *phdr, int numsegs) 
{
	int i;
	int error = 0;

	TRACE_ENTER;
	for (i = 1; i < numsegs; i++)  {
		if ((error = mmap_phdr(fp, &phdr[i])) != 0)
			break;
	}
	TRACE_EXIT;
	return error;
}

static int
elf_getsigs(struct proc *p, struct file *fp) 
{
	int error;
	struct ckpt_siginfo *csi;
	struct sigacts *tmpsigacts;

	TRACE_ENTER;
	csi = malloc(sizeof(struct ckpt_siginfo), M_TEMP, M_ZERO | M_WAITOK);
	if ((error = read_check(fp, csi, sizeof(struct ckpt_siginfo))) != 0)
		goto done;

	if (csi->csi_ckptpisz != sizeof(struct ckpt_siginfo)) {
		TRACE_ERR;
		error = EINVAL;
		goto done;
	}
	tmpsigacts = p->p_procsig->ps_sigacts;
	bcopy(&csi->csi_procsig, p->p_procsig, sizeof(struct procsig));
	p->p_procsig->ps_sigacts = tmpsigacts;
	bcopy(&csi->csi_sigacts, p->p_procsig->ps_sigacts, sizeof(struct sigacts));
	bcopy(&csi->csi_itimerval, &p->p_realtimer, sizeof(struct itimerval));
	SIG_CANTMASK(csi->csi_sigmask);
	bcopy(&csi->csi_sigmask, &p->p_sigmask, sizeof(sigset_t));
	p->p_sigparent = csi->csi_sigparent;
 done:
	if (csi)
		free(csi, M_TEMP);
	TRACE_EXIT;
	return error;
}

/*
 * Returns a locked, refd vnode
 */
static int
ckpt_fhtovp(fhandle_t *fh, struct vnode **vpp) 
{
	struct mount *mp;
	int error;

	TRACE_ENTER;
	mp = vfs_getvfs(&fh->fh_fsid);

	if (!mp) {
		TRACE_ERR;
		PRINTF(("failed to get mount - ESTALE\n"));
	        TRACE_EXIT;
		return ESTALE;
	}
	error = VFS_FHTOVP(mp, &fh->fh_fid, vpp);
	if (error) {
		PRINTF(("failed with: %d\n", error));
		TRACE_ERR;
	        TRACE_EXIT;
		return error;
	}
	TRACE_EXIT;
	return 0;
}

static int
mmap_vp(struct vn_hdr *vnh) 
{
	struct vnode *vp;
	Elf_Phdr *phdr;
	struct file *fp;
	int error;
	TRACE_ENTER;

	phdr = &vnh->vnh_phdr;

	if ((error = ckpt_fhtovp(&vnh->vnh_fh, &vp)) != 0)
		return error;
	/*
	 * XXX O_RDONLY -> or O_RDWR if file is PROT_WRITE, MAP_SHARED
	 */
	if ((error = fp_vpopen(vp, O_RDONLY, &fp)) != 0) {
		vput(vp);
		return error;
	}
	error = mmap_phdr(fp, phdr);
	fp_close(fp);
	TRACE_EXIT;
	return error;
}


static int
elf_gettextvp(struct proc *p, struct file *fp)
{
	int i;
	int error;
	int vpcount;
	struct ckpt_vminfo vminfo;
	struct vn_hdr *vnh = NULL;

	TRACE_ENTER;
	if ((error = read_check(fp, &vminfo, sizeof(vminfo))) != 0)
		goto done;
	if (vminfo.cvm_dsize < 0 || 
	    vminfo.cvm_dsize > p->p_rlimit[RLIMIT_DATA].rlim_cur ||
	    vminfo.cvm_tsize < 0 ||
	    (u_quad_t)vminfo.cvm_tsize > maxtsiz ||
	    vminfo.cvm_daddr >= (caddr_t)VM_MAXUSER_ADDRESS ||
	    vminfo.cvm_taddr >= (caddr_t)VM_MAXUSER_ADDRESS
	) {
	    error = ERANGE;
	    goto done;
	}

	vmspace_exec(p, NULL);
	p->p_vmspace->vm_daddr = vminfo.cvm_daddr;
	p->p_vmspace->vm_dsize = vminfo.cvm_dsize;
	p->p_vmspace->vm_taddr = vminfo.cvm_taddr;
	p->p_vmspace->vm_tsize = vminfo.cvm_tsize;
	if ((error = read_check(fp, &vpcount, sizeof(int))) != 0)
		goto done;
	vnh = malloc(sizeof(struct vn_hdr) * vpcount, M_TEMP, M_WAITOK);
	if ((error = read_check(fp, vnh, sizeof(struct vn_hdr)*vpcount)) != 0)
		goto done;
	for (i = 0; i < vpcount; i++) {
		if ((error = mmap_vp(&vnh[i])) != 0)
			goto done;
	}
	
 done:
	if (vnh)
		free(vnh, M_TEMP);
	TRACE_EXIT;
	return error;
}



/* place holder */
static int
elf_getfiles(struct proc *p, struct file *fp)
{
	int error;
	int i;
	int filecount;
	int fd;
	struct ckpt_filehdr filehdr;
	struct ckpt_fileinfo *cfi_base = NULL;
	struct vnode *vp;
	struct file *tempfp;
	struct file *ofp;

	TRACE_ENTER;
	if ((error = read_check(fp, &filehdr, sizeof(filehdr))) != 0)
		goto done;
	filecount = filehdr.cfh_nfiles;
	cfi_base = malloc(filecount*sizeof(struct ckpt_fileinfo), M_TEMP, M_WAITOK);
	error = read_check(fp, cfi_base, filecount*sizeof(struct ckpt_fileinfo));
	if (error)
		goto done;

	/*
	 * Close all file descriptors >= 3.  These descriptors are from the
	 * checkpt(1) program itself and should not be retained.
	 *
	 * XXX we need a flag so a checkpoint restore can opt to supply the
	 * descriptors, or the non-regular-file descripors.
	 */
	for (i = 3; i < p->p_fd->fd_nfiles; ++i)
		kern_close(i);

	/*
	 * Scan files to load
	 */
	for (i = 0; i < filecount; i++) {
		struct ckpt_fileinfo *cfi= &cfi_base[i];
		/*
		 * Ignore placeholder entries where cfi_index is less then
		 * zero.  This will occur if the elf core dump code thinks
		 * it can save a vnode but winds up not being able to.
		 */
		if (cfi->cfi_index < 0)
			continue;

		if ((error = ckpt_fhtovp(&cfi->cfi_fh, &vp)) != 0)
			break;
		if ((error = fp_vpopen(vp, OFLAGS(cfi->cfi_flags), &tempfp)) != 0) {
			vput(vp);
			break;
		}
		tempfp->f_offset = cfi->cfi_offset;

		/*
		 * If overwriting a descriptor close the old descriptor.  This
		 * only occurs if the saved core saved descriptors that we 
		 * have not already closed.
		 */
		if (cfi->cfi_index < p->p_fd->fd_nfiles &&
		    (ofp = p->p_fd->fd_ofiles[cfi->cfi_index]) != NULL) {
			kern_close(cfi->cfi_index);
		}

		/*
		 * Allocate the descriptor we want.
		 */
		if (fdalloc(p, cfi->cfi_index, &fd) != 0) {
			PRINTF(("can't currently restore fd: %d\n",
			       cfi->cfi_index));
			fp_close(fp);
			goto done;
		}
		KKASSERT(fd == cfi->cfi_index);
		p->p_fd->fd_ofiles[cfi->cfi_index] = tempfp;		
		cfi++;
		PRINTF(("restoring %d\n", cfi->cfi_index));
	}

 done:
	if (cfi_base)
		free(cfi_base, M_TEMP);
	TRACE_EXIT;
	return error;
}

static int
ckpt_freeze_proc (struct proc *p, struct file *fp)
{
	rlim_t limit;
	int error;

        PRINTF(("calling generic_elf_coredump\n"));
	limit = p->p_rlimit[RLIMIT_CORE].rlim_cur;
	if (limit) {
		error = generic_elf_coredump(p, fp, limit);
	} else {
		error = ERANGE;
	}
	return error;
}

int 
sys_checkpoint(struct sys_checkpoint_args *uap)
{
        int error = 0;
	struct proc *p = curthread->td_proc;
	struct file *fp;

	/*
	 * Only certain groups (to reduce our security exposure).  -1
	 * allows any group.
	 */
	if (ckptgroup >= 0 && groupmember(ckptgroup, p->p_ucred) == 0)
		return (EPERM);

	/*
	 * For now we can only checkpoint the current process
	 */
	if (uap->pid != -1 && uap->pid != p->p_pid)
		return (EINVAL);

	switch (uap->type) {
	case CKPT_FREEZE:
		fp = NULL;
		if (uap->fd == -1 && uap->pid == (pid_t)-1)
			error = checkpoint_signal_handler(p);
		else if ((fp = holdfp(p->p_fd, uap->fd, FWRITE)) == NULL)
			error = EBADF;
		else
			error = ckpt_freeze_proc(p, fp);
		if (fp)
			fdrop(fp, curthread);
		break;
	case CKPT_THAW:
		if (uap->pid != -1)
			return EINVAL;
		if ((fp = holdfp(p->p_fd, uap->fd, FREAD)) == NULL)
			return EBADF;
		uap->sysmsg_result = uap->retval;
	        error = ckpt_thaw_proc(p, fp);
		fdrop(fp, curthread);
		break;
	default:
	        error = EOPNOTSUPP;
		break;
	}
	return error;
}

int
checkpoint_signal_handler(struct proc *p) 
{
	char *buf;
	struct file *fp;
	struct nlookupdata nd;
	int error;

	chptinuse++;

	/*
	 * Being able to checkpoint an suid or sgid program is not a good
	 * idea.
	 */
	if (sugid_coredump == 0 && (p->p_flag & P_SUGID)) {
		chptinuse--;
		return (EPERM);
	}

	buf = ckpt_expand_name(p->p_comm, p->p_ucred->cr_uid, p->p_pid);
	if (buf == NULL) {
		chptinuse--;
		return (ENOMEM);
	}

	log(LOG_INFO, "pid %d (%s), uid %d: checkpointing to %s\n",
		p->p_pid, p->p_comm, 
		(p->p_ucred ? p->p_ucred->cr_uid : -1),
		buf);

	PRINTF(("ckpt handler called, using '%s'\n", buf));

	/*
	 * Use the same safety flags that the coredump code uses.  Remove
	 * any previous checkpoint file before writing out the new one in
	 * case we are re-checkpointing a program that had been checkpt
	 * restored.  Otherwise we will corrupt the program space (which is
	 * made up of mmap()ings of the previous checkpoint file) while we
	 * write out the new one.
	 */
	error = nlookup_init(&nd, buf, UIO_SYSSPACE, 0);
	if (error == 0)
		error = kern_unlink(&nd);
	nlookup_done(&nd);
	error = fp_open(buf, O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW, 0600, &fp);
	if (error == 0) {
		error = ckpt_freeze_proc(p, fp);
		fp_close(fp);
	} else {
		printf("checkpoint failed with open - error: %d\n", error);
	}
	free(buf, M_TEMP);
	chptinuse--;
	return (error);
}

static char ckptfilename[MAXPATHLEN] = {"%N.ckpt"};
SYSCTL_STRING(_kern, OID_AUTO, ckptfile, CTLFLAG_RW, ckptfilename,
	      sizeof(ckptfilename), "process checkpoint name format string");

/*
 * expand_name(name, uid, pid)
 * Expand the name described in corefilename, using name, uid, and pid.
 * corefilename is a printf-like string, with three format specifiers:
 *	%N	name of process ("name")
 *	%P	process id (pid)
 *	%U	user id (uid)
 * For example, "%N.core" is the default; they can be disabled completely
 * by using "/dev/null", or all core files can be stored in "/cores/%U/%N-%P".
 * This is controlled by the sysctl variable kern.corefile (see above).
 *
 * -- taken from the coredump code
 */

static
char *
ckpt_expand_name(const char *name, uid_t uid, pid_t pid)
{
	char *temp;
	char *bp;
	char buf[11];		/* Buffer for pid/uid -- max 4B */
	int error;
	int i;
	int n;
	char *format = ckptfilename;
	size_t namelen;

	temp = malloc(MAXPATHLEN + 1, M_TEMP, M_NOWAIT);
	if (temp == NULL)
		return NULL;
	namelen = strlen(name);
	n = 0;
	if (ckptfilename[0] != '/') {
		if ((bp = kern_getcwd(temp, MAXPATHLEN - 1, &error)) == NULL) {
			free(temp, M_TEMP);
			return NULL;
		}
		n = strlen(bp);
		bcopy(bp, temp, n + 1);	/* normalize location of the path */
		temp[n++] = '/';
		temp[n] = '\0';
	}
	for (i= 0; n < MAXPATHLEN && format[i]; i++) {
		int l;
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				temp[n++] = '%';
				break;
			case 'N':	/* process name */
				if ((n + namelen) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, name, namelen);
				n += namelen;
				break;
			case 'P':	/* process id */
				l = sprintf(buf, "%u", pid);
				if ((n + l) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, buf, l);
				n += l;
				break;
			case 'U':	/* user id */
				l = sprintf(buf, "%u", uid);
				if ((n + l) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, buf, l);
				n += l;
				break;
			default:
			  	log(LOG_ERR, "Unknown format character %c in `%s'\n", format[i], format);
			}
			break;
		default:
			temp[n++] = format[i];
		}
	}
	temp[n] = '\0';
	return temp;
}

