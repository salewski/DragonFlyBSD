/* $FreeBSD: ports/devel/gdb6/files/kvm-fbsd.c,v 1.2 2004/06/20 22:22:02 obrien Exp $ */
/* $DragonFly: src/gnu/usr.bin/gdb/gdb/Attic/kvm-fbsd.c,v 1.5 2005/02/07 21:33:36 dillon Exp $ */

/* Kernel core dump functions below target vector, for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994, 1995
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
 * This works like "remote" but, you use it like this:
 *     target kcore /dev/mem
 * or
 *     target kcore /var/crash/host/core.0
 *
 * This way makes it easy to short-circut the whole bfd monster,
 * and direct the inferior stuff to our libkvm implementation.
 *
 */

#include <sys/param.h>
#include <sys/user.h>
#include <machine/frame.h>

#include <ctype.h>
#include <fcntl.h>
#include <kvm.h>
#include <sys/sysctl.h>
#include <paths.h>

#include "defs.h"
#include "target.h"
#include "gdb_string.h"
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "command.h"
#include "bfd.h"
#include "gdbcore.h"
#include "solist.h"
#include "regcache.h"
#include <readline/tilde.h>

#include "kvm-fbsd.h"

static void
kcore_files_info (struct target_ops *);

static void
kgdb_close (int);

static void
kgdb_get_registers (int);

static void
set_proc_cmd (char *, int);


	/*
static int
xfer_mem (CORE_ADDR, char *, int, int, struct mem_attrib *,
          struct target_ops *);
	*/

static int
xfer_umem (CORE_ADDR, char *, int, int);

static char		*core_file;
static kvm_t		*core_kd;
static struct pcb	cur_pcb;
static struct kinfo_proc *cur_proc;

static struct target_ops kgdb_ops;

int kernel_debugging;
int kernel_writablecore;

/* Read the "thing" at kernel address 'addr' into the space pointed to
   by point.  The length of the "thing" is determined by the type of p.
   Result is non-zero if transfer fails.  */

#define kvread(addr, p) \
  (target_read_memory ((CORE_ADDR) (addr), (char *) (p), sizeof (*(p))))

static CORE_ADDR
ksym_kernbase (void)
{
  static CORE_ADDR kernbase;
  struct minimal_symbol *sym;

  if (kernbase == 0)
    {
      sym = lookup_minimal_symbol ("kernbase", NULL, NULL);
      if (sym == NULL) {
	kernbase = KERNBASE;
      } else {
	kernbase = SYMBOL_VALUE_ADDRESS (sym);
      }
    }
  return kernbase;
}

#define	KERNOFF		(ksym_kernbase ())
#define	INKERNEL(x)	((x) >= KERNOFF)

CORE_ADDR
ksym_lookup(const char *name)
{
  struct minimal_symbol *sym;

  sym = lookup_minimal_symbol (name, NULL, NULL);
  if (sym == NULL)
    error ("kernel symbol `%s' not found.", name);

  return SYMBOL_VALUE_ADDRESS (sym);
}

/* Provide the address of an initial PCB to use.
   If this is a crash dump, try for "dumppcb".
   If no "dumppcb" or it's /dev/mem, use proc0.
   Return the core address of the PCB we found.  */

static CORE_ADDR
initial_pcb (void)
{
  struct minimal_symbol *sym;
  CORE_ADDR addr;
  void *val;

  /* Make sure things are open...  */
  if (!core_kd || !core_file)
    return (0);

  /* If this is NOT /dev/mem try for dumppcb.  */
  if (strncmp (core_file, _PATH_DEV, sizeof _PATH_DEV - 1))
    {
      sym = lookup_minimal_symbol ("dumppcb", NULL, NULL);
      if (sym != NULL)
	{
	  addr = SYMBOL_VALUE_ADDRESS (sym);
	  return (addr);
	}
  }

  /* OK, just use thread0's pcb.  Note that curproc might
     not exist, and if it does, it will point to gdb.
     Therefore, just use proc0 and let the user set
     some other context if they care about it.  */

  addr = ksym_lookup ("thread0");
  if (kvread (addr, &val))
    {
      error ("cannot read thread0 pointer at %lx\n", addr);
      val = 0;
    }
  else
    {
      /* Read the PCB address in thread structure.  */
      addr += offsetof (struct thread, td_pcb);
      if (kvread (addr, &val))
	{
	  error ("cannot read thread0->td_pcb pointer at %lx\n", addr);
	  val = 0;
	}
    }

  /* thread0 is wholly in the kernel and cur_proc is only used for
     reading user mem, so no point in setting this up.  */
  cur_proc = 0;

  return ((CORE_ADDR)val);
}

/*
 * Set the current context to that of the PCB struct at the system address
 * passed.  If a (local copy of the) thread is passed and the thread does not
 * represent a process, we have to fake up a pcb because pure kernel threads
 * do not use pcb's.  Note however that in the case of a crash dump the 
 * kernel core will have a pcb structure that works whether or not it was
 * a process or a pure thread that crashed, and thr will be NULL in that case.
 */
static int
set_context (CORE_ADDR addr, struct thread *thr)
{
  CORE_ADDR procaddr = 0;
  int rval;

  if (thr && thr->td_proc == NULL) {
     /*
      * Pure threads do not have PCBs (unless this is the crash point in
      * which case the kernel saves the crash point state as a pcb).
      *
      * on thread stack: [low_level_restore_eip][eflags][edi][esi][ebx][ebp]
      *			 [switch_restore_eip]
      */
     rval = kvread(thr->td_sp + 2*4, &cur_pcb.pcb_edi);
     rval += kvread(thr->td_sp + 3*4, &cur_pcb.pcb_esi);
     rval += kvread(thr->td_sp + 4*4, &cur_pcb.pcb_ebx);
     rval += kvread(thr->td_sp + 5*4, &cur_pcb.pcb_ebp);
     rval += kvread(thr->td_sp + 6*4, &cur_pcb.pcb_eip);
     cur_pcb.pcb_esp = (register_t)thr->td_sp + 7*4;
     if (rval)
	    error ("cannot figure out fake pcb for pure thread");
  } else {
      if (kvread (addr, &cur_pcb))
	    error ("cannot read pcb at %#lx", addr);
  }

  /* Fetch all registers from core file.  */
  target_fetch_registers (-1);

  /* Now, set up the frame cache, and print the top of stack.  */
  flush_cached_frames ();
/*DEO XXX
  set_current_frame (create_new_frame (read_fp (), read_pc ()));
  set_current_frame (create_new_frame (deprecated_read_fp (), read_pc ()));
  select_frame (get_current_frame ());
*/
  print_stack_frame (get_selected_frame (), -1, 1);
  return (0);
}

/* Discard all vestiges of any previous core file and mark data and stack
   spaces as empty.  */

/* ARGSUSED */
static void
kgdb_close (int quitting)
{

  inferior_ptid = null_ptid;	/* Avoid confusion from thread stuff.  */

  if (core_kd)
    {
      kvm_close (core_kd);
      free (core_file);
      core_file = NULL;
      core_kd = NULL;
    }
}

/* This routine opens and sets up the core file bfd.  */

static void
kgdb_open (char *filename /* the core file */, int from_tty)
{
  kvm_t *kd;
  const char *p;
  struct cleanup *old_chain;
  char buf[256], *cp;
  int ontop;
  CORE_ADDR addr;

  target_preopen (from_tty);

  /* The exec file is required for symbols.  */
  if (exec_bfd == NULL)
    error ("No kernel exec file specified");

  if (core_kd)
    {
      error ("No core file specified."
	     "  (Use `detach' to stop debugging a core file.)");
      return;
    }

  if (!filename)
    {
      error ("No core file specified.");
      return;
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
      cp = concat (current_directory, "/", filename, NULL);
      free (filename);
      filename = cp;
    }

  old_chain = make_cleanup (free, filename);

  kd = kvm_open (bfd_get_filename(exec_bfd), filename, NULL,
		 kernel_writablecore ? O_RDWR: O_RDONLY, 0);
  if (kd == NULL)
    {
      perror_with_name (filename);
      return;
    }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain);		/* Don't free filename any more.  */
  core_file = filename;
  unpush_target (&kgdb_ops);
  ontop = !push_target (&kgdb_ops);

  /* Note unpush_target (above) calls kgdb_close.  */
  core_kd = kd;

  /* Print out the panic string if there is one.  */
  if (kvread (ksym_lookup ("panicstr"), &addr) == 0 &&
      addr != 0 &&
      target_read_memory (addr, buf, sizeof(buf)) == 0)
    {

      for (cp = buf; cp < &buf[sizeof(buf)] && *cp; cp++)
	if (!isascii (*cp) || (!isprint (*cp) && !isspace (*cp)))
	  *cp = '?';
      *cp = '\0';
      if (buf[0] != '\0')
	printf_filtered ("panic: %s\n", buf);
    }

  /* Print all the panic messages if possible.  */
  if (symfile_objfile != NULL)
    {
      printf ("panic messages:\n---\n");
      snprintf (buf, sizeof buf,
                "/sbin/dmesg -N %s -M %s | \
                 /usr/bin/awk '/^(panic:|Fatal trap) / { printing = 1 } \
                               { if (printing) print $0 }'",
                symfile_objfile->name, filename);
      fflush (stdout);
      system (buf);
      printf ("---\n");
    }

  if (!ontop)
    {
      warning ("you won't be able to access this core file until you terminate\n"
		"your %s; do ``info files''", target_longname);
      return;
    }

  /* Now, set up process context, and print the top of stack.  */
  (void)set_context (initial_pcb(), NULL);
  print_stack_frame (get_selected_frame (),
		     frame_relative_level (get_selected_frame ()), 1);
}

static void
kgdb_detach (char *args, int from_tty)
{
  if (args)
    error ("Too many arguments");
  unpush_target (&kgdb_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No kernel core file now.\n");
}

/* Get the registers out of a core file.  This is the machine-
   independent part.  Fetch_core_registers is the machine-dependent
   part, typically implemented in the xm-file for each architecture.  */

/* We just get all the registers, so we don't use regno.  */

/* ARGSUSED */
static void
kgdb_gdb_registers (int regno)
{

  /* XXX - Only read the pcb when set_context() is called.
     When looking at a live kernel this may be a problem,
     but the user can do another "proc" or "pcb" command to
     grab a new copy of the pcb...  */

  /* Zero out register set then fill in the ones we know about.  */
  kgdb_fetch_registers (&cur_pcb);
}

static void
kgdb_files_info (t)
  struct target_ops *t;
{
  printf_filtered ("\t`%s'\n", core_file);
}

/* If mourn is being called in all the right places, this could be say
   `gdb internal error' (since generic_mourn calls breakpoint_init_inferior). */

	/*
static int
ignore (CORE_ADDR addr, char *contents)
{
  return 0;
}
	*/

static int
xfer_kmem (CORE_ADDR memaddr, char *myaddr, int len, int write,
	   struct mem_attrib *attrib, struct target_ops *target)
{
  int n;


  if (!INKERNEL (memaddr))
    return xfer_umem (memaddr, myaddr, len, write);

  if (core_kd == NULL)
    return 0;

  if (write)
    n = kvm_write (core_kd, memaddr, myaddr, len);
  else
    n = kvm_read (core_kd, memaddr, myaddr, len) ;
  if (n < 0) {
    fprintf_unfiltered (gdb_stderr, "can not access 0x%lx, %s\n",
			memaddr, kvm_geterr (core_kd));
    n = 0;
  }

  return n;
}


static int
xfer_umem (CORE_ADDR memaddr, char *myaddr, int len, int write /* ignored */)
{
  int n = 0;

  if (cur_proc == 0)
    {
      error ("---Can't read userspace from dump, or kernel process---\n");
      return 0;
    }

  if (write)
    error ("kvm_uwrite unimplemented\n");
  else
    n = kvm_uread (core_kd, &cur_proc->kp_proc, memaddr, myaddr, len) ;

  if (n < 0)
    return 0;

  return n;
}

void
_initialize_kgdblow (void)
{
  kgdb_ops.to_shortname = "kgdb";
  kgdb_ops.to_longname = "Kernel core dump file";
  kgdb_ops.to_doc =
    "Use a core file as a target.  Specify the filename of the core file.";
  kgdb_ops.to_open = kgdb_open;
  kgdb_ops.to_close = kgdb_close;
  kgdb_ops.to_attach = find_default_attach;
  kgdb_ops.to_detach = kgdb_detach;
  kgdb_ops.to_fetch_registers = kgdb_gdb_registers;
  kgdb_ops.to_xfer_memory = xfer_kmem;
  kgdb_ops.to_files_info = kgdb_files_info;
  kgdb_ops.to_create_inferior = find_default_create_inferior;
  kgdb_ops.to_stratum = kgdb_stratum;
  kgdb_ops.to_has_memory = 1;
  kgdb_ops.to_has_stack = 1;
  kgdb_ops.to_has_registers = 1;
  kgdb_ops.to_magic = OPS_MAGIC;

  add_target (&kgdb_ops);
  add_com ("proc", class_obscure, set_proc_cmd, "Set current process context");
}

static void
set_proc_cmd (char *arg, int from_tty)
{
    CORE_ADDR paddr, val;
    struct kinfo_proc *kp;
    struct thread thr;
    int cnt = 0;

    if (!arg)
	error_no_arg ("proc address for new current process");
    if (!kernel_debugging)
	error ("not debugging kernel");

    paddr = (CORE_ADDR)parse_and_eval_address (arg);
    /* assume it's a thread pointer if it's in the kernel */
    if (INKERNEL(paddr)) {
	if (kvread(paddr, &thr)) {
	    error("invalid thread address");
	    val = 0;
	}
        paddr += offsetof (struct thread, td_pcb);
        if (kvread (paddr, &val)) {
	    error("invalid thread address");
	    val = 0;
	}
    } else {
	kp = kvm_getprocs(core_kd, KERN_PROC_PID, paddr, &cnt);
	if (!cnt)
	    error("invalid pid");
	thr = kp->kp_thread;
	val = (CORE_ADDR)kp->kp_thread.td_pcb;
    }
	
    if (set_context(val, &thr))
	error("invalid proc address");
}
