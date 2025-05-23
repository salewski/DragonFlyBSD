/*-
 * Copyright (c) 1983, 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * @(#)vmstat.c	8.2 (Berkeley) 1/12/94
 * $FreeBSD: src/usr.bin/systat/vmstat.c,v 1.38.2.4 2002/03/12 19:50:23 phantom Exp $
 * $DragonFly: src/usr.bin/systat/vmstat.c,v 1.8 2004/12/22 11:01:49 joerg Exp $
 */

/*
 * Cursed vmstat -- from Robert Elz.
 */

#define _KERNEL_STRUCTURES
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <vm/vm_param.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <kinfo.h>
#include <langinfo.h>
#include <nlist.h>
#include <paths.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <devstat.h>
#include "systat.h"
#include "extern.h"
#include "devs.h"

static struct Info {
	struct kinfo_cputime cp_time;
	struct	vmmeter Vmm;
	struct	vmtotal Total;
	struct  vmstats Vms;
	struct	nchstats nchstats;
	long	nchcount;
	long	*intrcnt;
	int	bufspace;
	int	desiredvnodes;
	long	numvnodes;
	long	freevnodes;
	long	numdirtybuffers;
} s, s1, s2, z;

struct kinfo_cputime cp_time, old_cp_time;
struct statinfo cur, last, run;

#define	vmm s.Vmm
#define	vms s.Vms
#define oldvmm s1.Vmm
#define oldvms s1.Vms
#define	total s.Total
#define	nchtotal s.nchstats
#define	oldnchtotal s1.nchstats

static	enum state { BOOT, TIME, RUN } state = TIME;

static void allocinfo(struct Info *);
static void copyinfo(struct Info *, struct Info *);
static void dinfo(int, int, struct statinfo *, struct statinfo *);
static void getinfo(struct Info *, enum state);
static void putint(int, int, int, int);
static void putfloat(double, int, int, int, int, int);
static void putlongdouble(long double, int, int, int, int, int);
static int ucount(void);

static	int ncpu;
static	int ut;
static	char buf[26];
static	time_t t;
static	double etime;
static	int nintr;
static	long *intrloc;
static	char **intrname;
static	int nextintsrow;
static  int extended_vm_stats;

struct	utmp utmp;


WINDOW *
openkre(void)
{

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0)
		error("No utmp");
	return (stdscr);
}

void
closekre(WINDOW *w)
{

	(void) close(ut);
	if (w == NULL)
		return;
	wclear(w);
	wrefresh(w);
}


static struct nlist namelist[] = {
#define	X_BUFFERSPACE	0
	{ "_bufspace" },
#define	X_NCHSTATS	1
	{ "_nchstats" },
#define	X_INTRNAMES	2
	{ "_intrnames" },
#define	X_EINTRNAMES	3
	{ "_eintrnames" },
#define	X_INTRCNT	4
	{ "_intrcnt" },
#define	X_EINTRCNT	5
	{ "_eintrcnt" },
#define	X_DESIREDVNODES	6
	{ "_desiredvnodes" },
#define	X_NUMVNODES	7
	{ "_numvnodes" },
#define	X_FREEVNODES	8
	{ "_freevnodes" },
#define X_NUMDIRTYBUFFERS 9
	{ "_numdirtybuffers" },
	{ "" },
};

/*
 * These constants define where the major pieces are laid out
 */
#define STATROW		 0	/* uses 1 row and 68 cols */
#define STATCOL		 2
#define MEMROW		 2	/* uses 4 rows and 31 cols */
#define MEMCOL		 0
#define PAGEROW		 2	/* uses 4 rows and 26 cols */
#define PAGECOL		46
#define INTSROW		 6	/* uses all rows to bottom and 17 cols */
#define INTSCOL		61
#define PROCSROW	 7	/* uses 2 rows and 20 cols */
#define PROCSCOL	 0
#define GENSTATROW	 7	/* uses 2 rows and 30 cols */
#define GENSTATCOL	20
#define VMSTATROW	 6	/* uses 17 rows and 12 cols */
#define VMSTATCOL	48
#define GRAPHROW	10	/* uses 3 rows and 51 cols */
#define GRAPHCOL	 0
#define NAMEIROW	14	/* uses 3 rows and 38 cols */
#define NAMEICOL	 0
#define DISKROW		18	/* uses 5 rows and 50 cols (for 9 drives) */
#define DISKCOL		 0

#define	DRIVESPACE	 7	/* max # for space */

#define	MAXDRIVES	DRIVESPACE	 /* max # to display */

int
initkre(void)
{
	char *intrnamebuf, *cp;
	int i;

	if (namelist[0].n_type == 0) {
		if (kvm_nlist(kd, namelist)) {
			nlisterr(namelist);
			return(0);
		}
		if (namelist[0].n_type == 0) {
			error("No namelist");
			return(0);
		}
	}

	if ((num_devices = getnumdevs()) < 0) {
		warnx("%s", devstat_errbuf);
		return(0);
	}

	cur.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	last.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	run.dinfo = (struct devinfo *)malloc(sizeof(struct devinfo));
	bzero(cur.dinfo, sizeof(struct devinfo));
	bzero(last.dinfo, sizeof(struct devinfo));
	bzero(run.dinfo, sizeof(struct devinfo));

	if (dsinit(MAXDRIVES, &cur, &last, &run) != 1)
		return(0);

	if (nintr == 0) {
		nintr = (namelist[X_EINTRCNT].n_value -
			namelist[X_INTRCNT].n_value) / sizeof (long);
		intrloc = calloc(nintr, sizeof (long));
		intrname = calloc(nintr, sizeof (long));
		intrnamebuf = malloc(namelist[X_EINTRNAMES].n_value -
			namelist[X_INTRNAMES].n_value);
		if (intrnamebuf == 0 || intrname == 0 || intrloc == 0) {
			error("Out of memory\n");
			if (intrnamebuf)
				free(intrnamebuf);
			if (intrname)
				free(intrname);
			if (intrloc)
				free(intrloc);
			nintr = 0;
			return(0);
		}
		NREAD(X_INTRNAMES, intrnamebuf, NVAL(X_EINTRNAMES) -
			NVAL(X_INTRNAMES));
		for (cp = intrnamebuf, i = 0; i < nintr; i++) {
			intrname[i] = cp;
			cp += strlen(cp) + 1;
		}
		nextintsrow = INTSROW + 2;
		allocinfo(&s);
		allocinfo(&s1);
		allocinfo(&s2);
		allocinfo(&z);
	}
	getinfo(&s2, RUN);
	copyinfo(&s2, &s1);
	return(1);
}

void
fetchkre(void)
{
	time_t now;
	struct tm *tp;
	static int d_first = -1;

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	time(&now);
	tp = localtime(&now);
	(void) strftime(buf, sizeof(buf),
			d_first ? "%e %b %R" : "%b %e %R", tp);
	getinfo(&s, state);
}

void
labelkre(void)
{
	register int i, j;

	clear();
	mvprintw(STATROW, STATCOL + 4, "users    Load");
	mvprintw(MEMROW, MEMCOL, "Mem:KB    REAL            VIRTUAL");
	mvprintw(MEMROW + 1, MEMCOL, "        Tot   Share      Tot    Share");
	mvprintw(MEMROW + 2, MEMCOL, "Act");
	mvprintw(MEMROW + 3, MEMCOL, "All");

	mvprintw(MEMROW + 1, MEMCOL + 41, "Free");

	mvprintw(PAGEROW, PAGECOL,     "        VN PAGER  SWAP PAGER ");
	mvprintw(PAGEROW + 1, PAGECOL, "        in  out     in  out ");
	mvprintw(PAGEROW + 2, PAGECOL, "count");
	mvprintw(PAGEROW + 3, PAGECOL, "pages");

	mvprintw(INTSROW, INTSCOL + 3, " Interrupts");
	mvprintw(INTSROW + 1, INTSCOL + 9, "total");

	mvprintw(VMSTATROW + 1, VMSTATCOL + 10, "cow");
	mvprintw(VMSTATROW + 2, VMSTATCOL + 10, "wire");
	mvprintw(VMSTATROW + 3, VMSTATCOL + 10, "act");
	mvprintw(VMSTATROW + 4, VMSTATCOL + 10, "inact");
	mvprintw(VMSTATROW + 5, VMSTATCOL + 10, "cache");
	mvprintw(VMSTATROW + 6, VMSTATCOL + 10, "free");
	mvprintw(VMSTATROW + 7, VMSTATCOL + 10, "daefr");
	mvprintw(VMSTATROW + 8, VMSTATCOL + 10, "prcfr");
	mvprintw(VMSTATROW + 9, VMSTATCOL + 10, "react");
	mvprintw(VMSTATROW + 10, VMSTATCOL + 10, "pdwake");
	mvprintw(VMSTATROW + 11, VMSTATCOL + 10, "pdpgs");
	mvprintw(VMSTATROW + 12, VMSTATCOL + 10, "intrn");
	mvprintw(VMSTATROW + 13, VMSTATCOL + 10, "buf");
	mvprintw(VMSTATROW + 14, VMSTATCOL + 10, "dirtybuf");

	mvprintw(VMSTATROW + 15, VMSTATCOL + 10, "desiredvnodes");
	mvprintw(VMSTATROW + 16, VMSTATCOL + 10, "numvnodes");
	mvprintw(VMSTATROW + 17, VMSTATCOL + 10, "freevnodes");

	mvprintw(GENSTATROW, GENSTATCOL, "  Csw  Trp  Sys  Int  Sof  Flt");

	mvprintw(GRAPHROW, GRAPHCOL,
		"  . %%Sys    . %%Intr   . %%User   . %%Nice   . %%Idle");
	mvprintw(PROCSROW, PROCSCOL, "Proc:r  p  d  s  w");
	mvprintw(GRAPHROW + 1, GRAPHCOL,
		"|    |    |    |    |    |    |    |    |    |    |");

	mvprintw(NAMEIROW, NAMEICOL, "Namei         Name-cache    Dir-cache");
	mvprintw(NAMEIROW + 1, NAMEICOL,
		"    Calls     hits    %%     hits    %%");
	mvprintw(DISKROW, DISKCOL, "Disks");
	mvprintw(DISKROW + 1, DISKCOL, "KB/t");
	mvprintw(DISKROW + 2, DISKCOL, "tps");
	mvprintw(DISKROW + 3, DISKCOL, "MB/s");
	mvprintw(DISKROW + 4, DISKCOL, "%% busy");
	/*
	 * For now, we don't support a fourth disk statistic.  So there's
	 * no point in providing a label for it.  If someone can think of a
	 * fourth useful disk statistic, there is room to add it.
	 */
	/* mvprintw(DISKROW + 4, DISKCOL, " msps"); */
	j = 0;
	for (i = 0; i < num_devices && j < MAXDRIVES; i++)
		if (dev_select[i].selected) {
			char tmpstr[80];
			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			mvprintw(DISKROW, DISKCOL + 5 + 6 * j,
				" %5.5s", tmpstr);
			j++;
		}

	if (j <= 4) {
		/*
		 * room for extended VM stats
		 */
		mvprintw(VMSTATROW + 11, VMSTATCOL - 6, "zfod");
		mvprintw(VMSTATROW + 12, VMSTATCOL - 6, "ofod");
		mvprintw(VMSTATROW + 13, VMSTATCOL - 6, "%%slo-z");
		mvprintw(VMSTATROW + 14, VMSTATCOL - 6, "tfree");
		extended_vm_stats = 1;
	} else {
		extended_vm_stats = 0;
		mvprintw(VMSTATROW + 0, VMSTATCOL + 10, "zfod");
	}

	for (i = 0; i < nintr; i++) {
		if (intrloc[i] == 0)
			continue;
		mvprintw(intrloc[i], INTSCOL + 9, "%-10.10s", intrname[i]);
	}
}

#define CP_UPDATE(fld)	do {	\
	uint64_t t;		\
	t=s.fld;		\
	s.fld-=s1.fld;		\
	if(state==TIME)		\
		s1.fld=t;	\
	t=fld;			\
	fld-=old_##fld;		\
	if(state==TIME)		\
		old_##fld=t;	\
	etime += s.fld;		\
} while(0)
#define X(fld)	{t=s.fld[i]; s.fld[i]-=s1.fld[i]; if(state==TIME) s1.fld[i]=t;}
#define Y(fld)	{t = s.fld; s.fld -= s1.fld; if(state == TIME) s1.fld = t;}
#define Z(fld)	{t = s.nchstats.fld; s.nchstats.fld -= s1.nchstats.fld; \
	if(state == TIME) s1.nchstats.fld = t;}
#define PUTRATE(fld, l, c, w) \
	Y(fld); \
	putint((int)((float)s.fld/etime + 0.5), l, c, w)
#define MAXFAIL 5

#define CPUSTATES 5
static	const char cpuchar[5] = { '=' , '+', '>', '-', ' ' };

static	const size_t cpuoffsets[] = {
	offsetof(struct kinfo_cputime, cp_sys),
	offsetof(struct kinfo_cputime, cp_intr),
	offsetof(struct kinfo_cputime, cp_user),
	offsetof(struct kinfo_cputime, cp_nice),
	offsetof(struct kinfo_cputime, cp_idle)
};

void
showkre(void)
{
	float f1, f2;
	int psiz, inttotal;
	int i, l, c;
	static int failcnt = 0;
	double total_time;

	etime = 0;
	CP_UPDATE(cp_time.cp_user);
	CP_UPDATE(cp_time.cp_nice);
	CP_UPDATE(cp_time.cp_sys);
	CP_UPDATE(cp_time.cp_intr);
	CP_UPDATE(cp_time.cp_idle);

	total_time = etime;
	if (total_time == 0.0)
		total_time = 1.0;

	if (etime < 100000.0) {	/* < 100ms ignore this trash */
		if (failcnt++ >= MAXFAIL) {
			clear();
			mvprintw(2, 10, "The alternate system clock has died!");
			mvprintw(3, 10, "Reverting to ``pigs'' display.");
			move(CMDLINE, 0);
			refresh();
			failcnt = 0;
			sleep(5);
			command("pigs");
		}
		return;
	}
	failcnt = 0;
	etime /= 1000000.0;
	etime /= ncpu;
	if (etime == 0)
		etime = 1;
	inttotal = 0;
	for (i = 0; i < nintr; i++) {
		if (s.intrcnt[i] == 0)
			continue;
		if (intrloc[i] == 0) {
			if (nextintsrow == LINES)
				continue;
			intrloc[i] = nextintsrow++;
			mvprintw(intrloc[i], INTSCOL + 9, "%-10.10s",
				intrname[i]);
		}
		X(intrcnt);
		l = (int)((float)s.intrcnt[i]/etime + 0.5);
		inttotal += l;
		putint(l, intrloc[i], INTSCOL + 2, 6);
	}
	putint(inttotal, INTSROW + 1, INTSCOL + 2, 6);
	Z(ncs_goodhits); Z(ncs_badhits); Z(ncs_miss);
	Z(ncs_long); Z(ncs_pass2); Z(ncs_2passes); Z(ncs_neghits);
	s.nchcount = nchtotal.ncs_goodhits + nchtotal.ncs_badhits +
	    nchtotal.ncs_miss + nchtotal.ncs_long + nchtotal.ncs_neghits;
	if (state == TIME)
		s1.nchcount = s.nchcount;

	psiz = 0;
	f2 = 0.0;
	for (c = 0; c < CPUSTATES; c++) {
		uint64_t val = *(uint64_t *)(((uint8_t *)&s.cp_time) +
		    cpuoffsets[c]);
		f1 = 100.0 * val / total_time;
		f2 += f1;
		l = (int) ((f2 + 1.0) / 2.0) - psiz;
		if (f1 > 99.9)
			f1 = 99.9;	/* no room to display 100.0 */
		putfloat(f1, GRAPHROW, GRAPHCOL + 10 * c, 4, 1, 0);
		move(GRAPHROW + 2, psiz);
		psiz += l;
		while (l-- > 0)
			addch(cpuchar[c]);
	}

	putint(ucount(), STATROW, STATCOL, 3);
	putfloat(avenrun[0], STATROW, STATCOL + 17, 6, 2, 0);
	putfloat(avenrun[1], STATROW, STATCOL + 23, 6, 2, 0);
	putfloat(avenrun[2], STATROW, STATCOL + 29, 6, 2, 0);
	mvaddstr(STATROW, STATCOL + 53, buf);
#define pgtokb(pg)	((pg) * vms.v_page_size / 1024)
	putint(pgtokb(total.t_arm), MEMROW + 2, MEMCOL + 3, 8);
	putint(pgtokb(total.t_armshr), MEMROW + 2, MEMCOL + 11, 8);
	putint(pgtokb(total.t_avm), MEMROW + 2, MEMCOL + 19, 9);
	putint(pgtokb(total.t_avmshr), MEMROW + 2, MEMCOL + 28, 9);
	putint(pgtokb(total.t_rm), MEMROW + 3, MEMCOL + 3, 8);
	putint(pgtokb(total.t_rmshr), MEMROW + 3, MEMCOL + 11, 8);
	putint(pgtokb(total.t_vm), MEMROW + 3, MEMCOL + 19, 9);
	putint(pgtokb(total.t_vmshr), MEMROW + 3, MEMCOL + 28, 9);
	putint(pgtokb(total.t_free), MEMROW + 2, MEMCOL + 37, 8);
	putint(total.t_rq - 1, PROCSROW + 1, PROCSCOL + 3, 3);
	putint(total.t_pw, PROCSROW + 1, PROCSCOL + 6, 3);
	putint(total.t_dw, PROCSROW + 1, PROCSCOL + 9, 3);
	putint(total.t_sl, PROCSROW + 1, PROCSCOL + 12, 3);
	putint(total.t_sw, PROCSROW + 1, PROCSCOL + 15, 3);
	if (extended_vm_stats == 0) {
		PUTRATE(Vmm.v_zfod, VMSTATROW + 0, VMSTATCOL + 4, 5);
	}
	PUTRATE(Vmm.v_cow_faults, VMSTATROW + 1, VMSTATCOL + 3, 6);
	putint(pgtokb(vms.v_wire_count), VMSTATROW + 2, VMSTATCOL, 9);
	putint(pgtokb(vms.v_active_count), VMSTATROW + 3, VMSTATCOL, 9);
	putint(pgtokb(vms.v_inactive_count), VMSTATROW + 4, VMSTATCOL, 9);
	putint(pgtokb(vms.v_cache_count), VMSTATROW + 5, VMSTATCOL, 9);
	putint(pgtokb(vms.v_free_count), VMSTATROW + 6, VMSTATCOL, 9);
	PUTRATE(Vmm.v_dfree, VMSTATROW + 7, VMSTATCOL, 9);
	PUTRATE(Vmm.v_pfree, VMSTATROW + 8, VMSTATCOL, 9);
	PUTRATE(Vmm.v_reactivated, VMSTATROW + 9, VMSTATCOL, 9);
	PUTRATE(Vmm.v_pdwakeups, VMSTATROW + 10, VMSTATCOL, 9);
	PUTRATE(Vmm.v_pdpages, VMSTATROW + 11, VMSTATCOL, 9);
	PUTRATE(Vmm.v_intrans, VMSTATROW + 12, VMSTATCOL, 9);

	if (extended_vm_stats) {
	    PUTRATE(Vmm.v_zfod, VMSTATROW + 11, VMSTATCOL - 16, 9);
	    PUTRATE(Vmm.v_ozfod, VMSTATROW + 12, VMSTATCOL - 16, 9);
	    putint(
		((s.Vmm.v_ozfod < s.Vmm.v_zfod) ?
		    s.Vmm.v_ozfod * 100 / s.Vmm.v_zfod : 
		    0
		),
		VMSTATROW + 13, 
		VMSTATCOL - 16,
		9
	    );
	    PUTRATE(Vmm.v_tfree, VMSTATROW + 14, VMSTATCOL - 16, 9);
	}

	putint(s.bufspace/1024, VMSTATROW + 13, VMSTATCOL, 9);
	putint(s.numdirtybuffers, VMSTATROW + 14, VMSTATCOL, 9);
	putint(s.desiredvnodes, VMSTATROW + 15, VMSTATCOL, 9);
	putint(s.numvnodes, VMSTATROW + 16, VMSTATCOL, 9);
	putint(s.freevnodes, VMSTATROW + 17, VMSTATCOL, 9);
	PUTRATE(Vmm.v_vnodein, PAGEROW + 2, PAGECOL + 5, 5);
	PUTRATE(Vmm.v_vnodeout, PAGEROW + 2, PAGECOL + 10, 5);
	PUTRATE(Vmm.v_swapin, PAGEROW + 2, PAGECOL + 17, 5);
	PUTRATE(Vmm.v_swapout, PAGEROW + 2, PAGECOL + 22, 5);
	PUTRATE(Vmm.v_vnodepgsin, PAGEROW + 3, PAGECOL + 5, 5);
	PUTRATE(Vmm.v_vnodepgsout, PAGEROW + 3, PAGECOL + 10, 5);
	PUTRATE(Vmm.v_swappgsin, PAGEROW + 3, PAGECOL + 17, 5);
	PUTRATE(Vmm.v_swappgsout, PAGEROW + 3, PAGECOL + 22, 5);
	PUTRATE(Vmm.v_swtch, GENSTATROW + 1, GENSTATCOL, 5);
	PUTRATE(Vmm.v_trap, GENSTATROW + 1, GENSTATCOL + 5, 5);
	PUTRATE(Vmm.v_syscall, GENSTATROW + 1, GENSTATCOL + 10, 5);
	PUTRATE(Vmm.v_intr, GENSTATROW + 1, GENSTATCOL + 15, 5);
	PUTRATE(Vmm.v_soft, GENSTATROW + 1, GENSTATCOL + 20, 5);
	PUTRATE(Vmm.v_vm_faults, GENSTATROW + 1, GENSTATCOL + 25, 5);
	mvprintw(DISKROW, DISKCOL + 5, "                              ");
	for (i = 0, c = 0; i < num_devices && c < MAXDRIVES; i++)
		if (dev_select[i].selected) {
			char tmpstr[80];
			sprintf(tmpstr, "%s%d", dev_select[i].device_name,
				dev_select[i].unit_number);
			mvprintw(DISKROW, DISKCOL + 5 + 6 * c,
				" %5.5s", tmpstr);
			switch(state) {
			case TIME:
				dinfo(i, ++c, &cur, &last);
				break;
			case RUN:
				dinfo(i, ++c, &cur, &run);
				break;
			case BOOT:
				dinfo(i, ++c, &cur, NULL);
				break;
			}
		}
	putint(s.nchcount, NAMEIROW + 2, NAMEICOL, 9);
	putint((nchtotal.ncs_goodhits + nchtotal.ncs_neghits),
	   NAMEIROW + 2, NAMEICOL + 9, 9);
#define nz(x)	((x) ? (x) : 1)
	putfloat((nchtotal.ncs_goodhits+nchtotal.ncs_neghits) *
	   100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 19, 4, 0, 1);
	putint(nchtotal.ncs_pass2, NAMEIROW + 2, NAMEICOL + 23, 9);
	putfloat(nchtotal.ncs_pass2 * 100.0 / nz(s.nchcount),
	   NAMEIROW + 2, NAMEICOL + 33, 4, 0, 1);
#undef nz
}

int
cmdkre(char *cmd, char *args)
{
	int retval;

	if (prefix(cmd, "run")) {
		retval = 1;
		copyinfo(&s2, &s1);
		switch (getdevs(&run)) {
		case -1:
			errx(1, "%s", devstat_errbuf);
			break;
		case 1:
			num_devices = run.dinfo->numdevs;
			generation = run.dinfo->generation;
			retval = dscmd("refresh", NULL, MAXDRIVES, &cur);
			if (retval == 2)
				labelkre();
			break;
		default:
			break;
		}
		state = RUN;
		return (retval);
	}
	if (prefix(cmd, "boot")) {
		state = BOOT;
		copyinfo(&z, &s1);
		return (1);
	}
	if (prefix(cmd, "time")) {
		state = TIME;
		return (1);
	}
	if (prefix(cmd, "zero")) {
		retval = 1;
		if (state == RUN) {
			getinfo(&s1, RUN);
			switch (getdevs(&run)) {
			case -1:
				errx(1, "%s", devstat_errbuf);
				break;
			case 1:
				num_devices = run.dinfo->numdevs;
				generation = run.dinfo->generation;
				retval = dscmd("refresh",NULL, MAXDRIVES, &cur);
				if (retval == 2)
					labelkre();
				break;
			default:
				break;
			}
		}
		return (retval);
	}
	retval = dscmd(cmd, args, MAXDRIVES, &cur);

	if (retval == 2)
		labelkre();

	return(retval);
}

/* calculate number of users on the system */
static int
ucount(void)
{
	register int nusers = 0;

	if (ut < 0)
		return (0);
	while (read(ut, &utmp, sizeof(utmp)))
		if (utmp.ut_name[0] != '\0')
			nusers++;

	lseek(ut, 0L, L_SET);
	return (nusers);
}

static void
putint(int n, int l, int c, int w)
{
	char b[128];

	move(l, c);
	if (n == 0) {
		while (w-- > 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof(b), "%*d", w, n);
	if (strlen(b) > w) {
		while (w-- > 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putfloat(double f, int l, int c, int w, int d, int nz)
{
	char b[128];

	move(l, c);
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	snprintf(b, sizeof(b), "%*.*f", w, d, f);
	if (strlen(b) > w)
		snprintf(b, sizeof(b), "%*.0f", w, f);
	if (strlen(b) > w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
putlongdouble(long double f, int l, int c, int w, int d, int nz)
{
	char b[128];

	move(l, c);
	if (nz && f == 0.0) {
		while (--w >= 0)
			addch(' ');
		return;
	}
	sprintf(b, "%*.*Lf", w, d, f);
	if (strlen(b) > w)
		sprintf(b, "%*.0Lf", w, f);
	if (strlen(b) > w) {
		while (--w >= 0)
			addch('*');
		return;
	}
	addstr(b);
}

static void
getinfo(struct Info *s, enum state st)
{
	struct devinfo *tmp_dinfo;
	struct nchstats *nch_tmp;
	size_t size;
	int vms_size = sizeof(s->Vms);
	int vmm_size = sizeof(s->Vmm);
	size_t nch_size = sizeof(s->nchstats) * SMP_MAXCPU;

        if (sysctlbyname("vm.vmstats", &s->Vms, &vms_size, NULL, 0)) {
                perror("sysctlbyname: vm.vmstats");
                exit(1);
        }
        if (sysctlbyname("vm.vmmeter", &s->Vmm, &vmm_size, NULL, 0)) {
                perror("sysctlbyname: vm.vmstats");
                exit(1);
        }

	if (kinfo_get_sched_cputime(&s->cp_time))
		err(1, "kinfo_get_sched_cputime");
	if (kinfo_get_sched_cputime(&cp_time))
		err(1, "kinfo_get_sched_cputime");
	NREAD(X_BUFFERSPACE, &s->bufspace, sizeof(s->bufspace));
	NREAD(X_DESIREDVNODES, &s->desiredvnodes, sizeof(s->desiredvnodes));
	NREAD(X_NUMVNODES, &s->numvnodes, LONG);
	NREAD(X_FREEVNODES, &s->freevnodes, LONG);
	NREAD(X_INTRCNT, s->intrcnt, nintr * LONG);
	NREAD(X_NUMDIRTYBUFFERS, &s->numdirtybuffers, sizeof(s->numdirtybuffers));
	size = sizeof(s->Total);
	if (sysctlbyname("vm.vmtotal", &s->Total, &size, NULL, 0) < 0) {
		error("Can't get kernel info: %s\n", strerror(errno));
		bzero(&s->Total, sizeof(s->Total));
	}
	
	if ((nch_tmp = malloc(nch_size)) == NULL) {
		perror("malloc");
		exit(1);
	} else {
		if (sysctlbyname("vfs.cache.nchstats", nch_tmp, &nch_size, NULL, 0)) {
			perror("sysctlbyname vfs.cache.nchstats");
			free(nch_tmp);
			exit(1);
		} else {
			if ((nch_tmp = realloc(nch_tmp, nch_size)) == NULL) {
				perror("realloc");
				exit(1);
			}
		}
	}

	if (kinfo_get_cpus(&ncpu))
		err(1, "kinfo_get_cpus");
	kvm_nch_cpuagg(nch_tmp, &s->nchstats, ncpu);
	free(nch_tmp);

	tmp_dinfo = last.dinfo;
	last.dinfo = cur.dinfo;
	cur.dinfo = tmp_dinfo;

	last.busy_time = cur.busy_time;
	switch (getdevs(&cur)) {
	case -1:
		errx(1, "%s", devstat_errbuf);
		break;
	case 1:
		num_devices = cur.dinfo->numdevs;
		generation = cur.dinfo->generation;
		cmdkre("refresh", NULL);
		break;
	default:
		break;
	}
}

static void
allocinfo(struct Info *s)
{

	s->intrcnt = (long *) calloc(nintr, sizeof(long));
	if (s->intrcnt == NULL)
		errx(2, "out of memory");
}

static void
copyinfo(register struct Info *from, register struct Info *to)
{
	long *intrcnt;

	/*
	 * time, wds, seek, and xfer are malloc'd so we have to
	 * save the pointers before the structure copy and then
	 * copy by hand.
	 */
	intrcnt = to->intrcnt;
	*to = *from;

	bcopy(from->intrcnt, to->intrcnt = intrcnt, nintr * sizeof (int));
}

static void
dinfo(int dn, int c, struct statinfo *now, struct statinfo *then)
{
	long double transfers_per_second;
	long double kb_per_transfer, mb_per_second;
	long double elapsed_time, device_busy;
	int di;

	di = dev_select[dn].position;

	elapsed_time = compute_etime(now->busy_time, then ?
				     then->busy_time :
				     now->dinfo->devices[di].dev_creation_time);

	device_busy =  compute_etime(now->dinfo->devices[di].busy_time, then ?
				     then->dinfo->devices[di].busy_time :
				     now->dinfo->devices[di].dev_creation_time);

	if (compute_stats(&now->dinfo->devices[di], then ?
			  &then->dinfo->devices[di] : NULL, elapsed_time,
			  NULL, NULL, NULL,
			  &kb_per_transfer, &transfers_per_second,
			  &mb_per_second, NULL, NULL) != 0)
		errx(1, "%s", devstat_errbuf);

	if ((device_busy == 0) && (transfers_per_second > 5))
		/* the device has been 100% busy, fake it because
		 * as long as the device is 100% busy the busy_time
		 * field in the devstat struct is not updated */
		device_busy = elapsed_time;
	if (device_busy > elapsed_time)
		/* this normally happens after one or more periods
		 * where the device has been 100% busy, correct it */
		device_busy = elapsed_time;

	c = DISKCOL + c * 6;
	putlongdouble(kb_per_transfer, DISKROW + 1, c, 5, 2, 0);
	putlongdouble(transfers_per_second, DISKROW + 2, c, 5, 0, 0);
	putlongdouble(mb_per_second, DISKROW + 3, c, 5, 2, 0);
	putlongdouble(device_busy * 100 / elapsed_time, DISKROW + 4, c, 5, 0, 0);
}
