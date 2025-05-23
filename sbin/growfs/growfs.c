/*
 * Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz
 * Copyright (c) 1980, 1989, 1993 The Regents of the University of California.
 * All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by
 * Christoph Herrmann and Thomas-Henning von Kamptz, Munich and Frankfurt.
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
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors, as well as Christoph
 *      Herrmann and Thomas-Henning von Kamptz.
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
 * $TSHeader: src/sbin/growfs/growfs.c,v 1.5 2000/12/12 19:31:00 tomsoft Exp $
 *
 * @(#) Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz Copyright (c) 1980, 1989, 1993 The Regents of the University of California. All rights reserved.
 * $FreeBSD: src/sbin/growfs/growfs.c,v 1.4.2.2 2001/08/14 12:45:11 chm Exp $
 * $DragonFly: src/sbin/growfs/growfs.c,v 1.3 2003/08/08 04:18:38 dillon Exp $
 */

/* ********************************************************** INCLUDES ***** */
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <stdio.h>
#include <paths.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vfs/ufs/dinode.h>
#include <vfs/ufs/fs.h>

#include "debug.h"

/* *************************************************** GLOBALS & TYPES ***** */
#ifdef FS_DEBUG
int	_dbg_lvl_ = (DL_INFO);	/* DL_TRC */
#endif /* FS_DEBUG */

static union {
	struct fs	fs;
	char	pad[SBSIZE];
} fsun1, fsun2;
#define	sblock	fsun1.fs	/* the new superblock */
#define	osblock	fsun2.fs	/* the old superblock */

static union {
	struct cg	cg;
	char	pad[MAXBSIZE];
} cgun1, cgun2;
#define	acg	cgun1.cg	/* a cylinder cgroup (new) */
#define	aocg	cgun2.cg	/* an old cylinder group */

static char	ablk[MAXBSIZE];		/* a block */
static char	i1blk[MAXBSIZE];	/* some indirect blocks */
static char	i2blk[MAXBSIZE];
static char	i3blk[MAXBSIZE];

	/* where to write back updated blocks */
static daddr_t	in_src, i1_src, i2_src, i3_src;

	/* what object contains the reference */
enum pointer_source {
	GFS_PS_INODE,
	GFS_PS_IND_BLK_LVL1,
	GFS_PS_IND_BLK_LVL2,
	GFS_PS_IND_BLK_LVL3
};

static struct csum	*fscs;		/* cylinder summary */

static struct dinode	zino[MAXBSIZE/sizeof(struct dinode)]; /* some inodes */

/*
 * An  array of elements of type struct gfs_bpp describes all blocks  to
 * be relocated in order to free the space needed for the cylinder group
 * summary for all cylinder groups located in the first cylinder group.
 */
struct gfs_bpp {
	daddr_t	old;		/* old block number */
	daddr_t	new;		/* new block number */
#define GFS_FL_FIRST	1
#define GFS_FL_LAST	2
	unsigned int	flags;	/* special handling required */
	int	found;		/* how many references were updated */
};

/* ******************************************************** PROTOTYPES ***** */
static void	growfs(int, int, unsigned int);
static void	rdfs(daddr_t, size_t, void *, int);
static void	wtfs(daddr_t, size_t, void *, int, unsigned int);
static daddr_t	alloc(void);
static int	charsperline(void);
static void	usage(void);
static int	isblock(struct fs *, unsigned char *, int);
static void	clrblock(struct fs *, unsigned char *, int);
static void	setblock(struct fs *, unsigned char *, int);
static void	initcg(int, time_t, int, unsigned int);
static void	updjcg(int, time_t, int, int, unsigned int);
static void	updcsloc(time_t, int, int, unsigned int);
static struct disklabel	*get_disklabel(int);
static void	return_disklabel(int, struct disklabel *, unsigned int);
static struct dinode	*ginode(ino_t, int, int);
static void	frag_adjust(daddr_t, int);
static void	cond_bl_upd(ufs_daddr_t *, struct gfs_bpp *,
    enum pointer_source, int, unsigned int);
static void	updclst(int);
static void	updrefs(int, ino_t, struct gfs_bpp *, int, int, unsigned int);

/* ************************************************************ growfs ***** */
/*
 * Here  we actually start growing the filesystem. We basically  read  the
 * cylinder  summary  from the first cylinder group as we want  to  update
 * this  on  the fly during our various operations. First  we  handle  the
 * changes in the former last cylinder group. Afterwards we create all new
 * cylinder  groups.  Now  we handle the  cylinder  group  containing  the
 * cylinder  summary  which  might result in a  relocation  of  the  whole
 * structure.  In the end we write back the updated cylinder summary,  the
 * new superblock, and slightly patched versions of the super block
 * copies.
 */
static void
growfs(int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("growfs")
	int	i;
	int	cylno, j;
	time_t	utime;
	int	width;
	char	tmpbuf[100];
#ifdef FSIRAND
	static int	randinit=0;

	DBG_ENTER;

	if (!randinit) {
		randinit = 1;
		srandomdev();
	}
#else /* not FSIRAND */

	DBG_ENTER;

#endif /* FSIRAND */
	time(&utime);

	/*
	 * Get the cylinder summary into the memory.
	 */
	fscs = (struct csum *)calloc((size_t)1, (size_t)sblock.fs_cssize);
	if(fscs == NULL) {
		errx(1, "calloc failed");
	}
	for (i = 0; i < osblock.fs_cssize; i += osblock.fs_bsize) {
		rdfs(fsbtodb(&osblock, osblock.fs_csaddr +
		    numfrags(&osblock, i)), (size_t)MIN(osblock.fs_cssize - i,
		    osblock.fs_bsize), (void *)(((char *)fscs)+i), fsi);
	}

#ifdef FS_DEBUG
{
	struct csum	*dbg_csp;
	int	dbg_csc;
	char	dbg_line[80];

	dbg_csp=fscs;
	for(dbg_csc=0; dbg_csc<osblock.fs_ncg; dbg_csc++) {
		snprintf(dbg_line, sizeof(dbg_line),
		    "%d. old csum in old location", dbg_csc);
		DBG_DUMP_CSUM(&osblock,
		    dbg_line,
		    dbg_csp++);
	}
}
#endif /* FS_DEBUG */
	DBG_PRINT0("fscs read\n");

	/*
	 * Do all needed changes in the former last cylinder group.
	 */
	updjcg(osblock.fs_ncg-1, utime, fsi, fso, Nflag);

	/*
	 * Dump out summary information about file system.
	 */
	printf("growfs:\t%d sectors in %d %s of %d tracks, %d sectors\n",
	    sblock.fs_size * NSPF(&sblock), sblock.fs_ncyl,
	    "cylinders", sblock.fs_ntrak, sblock.fs_nsect);
#define B2MBFACTOR (1 / (1024.0 * 1024.0))
	printf("\t%.1fMB in %d cyl groups (%d c/g, %.2fMB/g, %d i/g)\n",
	    (float)sblock.fs_size * sblock.fs_fsize * B2MBFACTOR,
	    sblock.fs_ncg, sblock.fs_cpg,
	    (float)sblock.fs_fpg * sblock.fs_fsize * B2MBFACTOR,
	    sblock.fs_ipg);
#undef B2MBFACTOR

	/*
	 * Now build the cylinders group blocks and
	 * then print out indices of cylinder groups.
	 */
	printf("super-block backups (for fsck -b #) at:\n");
	i = 0;
	width = charsperline();

	/*
	 * Iterate for only the new cylinder groups.
	 */
	for (cylno = osblock.fs_ncg; cylno < sblock.fs_ncg; cylno++) {
		initcg(cylno, utime, fso, Nflag);
		j = sprintf(tmpbuf, " %d%s",
		    (int)fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    cylno < (sblock.fs_ncg-1) ? "," : "" );
		if (i + j >= width) {
			printf("\n");
			i = 0;
		}
		i += j;
		printf("%s", tmpbuf);
		fflush(stdout);
	}
	printf("\n");

	/*
	 * Do all needed changes in the first cylinder group.
	 * allocate blocks in new location
	 */
	updcsloc(utime, fsi, fso, Nflag);

	/*
	 * Now write the cylinder summary back to disk.
	 */
	for (i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize) {
		wtfs(fsbtodb(&sblock, sblock.fs_csaddr + numfrags(&sblock, i)),
		    (size_t)MIN(sblock.fs_cssize - i, sblock.fs_bsize),
		    (void *)(((char *)fscs) + i), fso, Nflag);
	}
	DBG_PRINT0("fscs written\n");

#ifdef FS_DEBUG
{
	struct csum	*dbg_csp;
	int	dbg_csc;
	char	dbg_line[80];

	dbg_csp=fscs;
	for(dbg_csc=0; dbg_csc<sblock.fs_ncg; dbg_csc++) {
		snprintf(dbg_line, sizeof(dbg_line),
		    "%d. new csum in new location", dbg_csc);
		DBG_DUMP_CSUM(&sblock,
		    dbg_line,
		    dbg_csp++);
	}
}
#endif /* FS_DEBUG */

	/*
	 * Now write the new superblock back to disk.
	 */
	sblock.fs_time = utime;
	wtfs((daddr_t)(SBOFF / DEV_BSIZE), (size_t)SBSIZE, (void *)&sblock,
	    fso, Nflag);
	DBG_PRINT0("sblock written\n");
	DBG_DUMP_FS(&sblock,
	    "new initial sblock");

	/*
	 * Clean up the dynamic fields in our superblock copies.
	 */
	sblock.fs_fmod = 0;
	sblock.fs_clean = 1;
	sblock.fs_ronly = 0;
	sblock.fs_cgrotor = 0;
	sblock.fs_state = 0;
	memset((void *)&sblock.fs_fsmnt, 0, sizeof(sblock.fs_fsmnt));
	sblock.fs_flags &= FS_DOSOFTDEP;

	/*
	 * XXX
	 * The following fields are currently distributed from the  superblock
	 * to the copies:
	 *     fs_minfree
	 *     fs_rotdelay
	 *     fs_maxcontig
	 *     fs_maxbpg
	 *     fs_minfree,
	 *     fs_optim
	 *     fs_flags regarding SOFTPDATES
	 *
	 * We probably should rather change the summary for the cylinder group
	 * statistics here to the value of what would be in there, if the file
	 * system were created initially with the new size. Therefor we  still
	 * need to find an easy way of calculating that.
	 * Possibly we can try to read the first superblock copy and apply the
	 * "diffed" stats between the old and new superblock by still  copying
	 * certain parameters onto that.
	 */

	/*
	 * Write out the duplicate super blocks.
	 */
	for (cylno = 0; cylno < sblock.fs_ncg; cylno++) {
		wtfs(fsbtodb(&sblock, cgsblock(&sblock, cylno)),
		    (size_t)SBSIZE, (void *)&sblock, fso, Nflag);
	}
	DBG_PRINT0("sblock copies written\n");
	DBG_DUMP_FS(&sblock,
	    "new other sblocks");

	DBG_LEAVE;
	return;
}

/* ************************************************************ initcg ***** */
/*
 * This creates a new cylinder group structure, for more details please  see
 * the  source of newfs(8), as this function is taken over almost unchanged.
 * As  this  is  never called for the  first  cylinder  group,  the  special
 * provisions for that case are removed here.
 */
static void
initcg(int cylno, time_t utime, int fso, unsigned int Nflag)
{
	DBG_FUNC("initcg")
	daddr_t cbase, d, dlower, dupper, dmax, blkno;
	int i;
	register struct csum *cs;
#ifdef FSIRAND
	int j;
#endif

	DBG_ENTER;

	/*
	 * Determine block bounds for cylinder group.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size) {
		dmax = sblock.fs_size;
	}
	dlower = cgsblock(&sblock, cylno) - cbase;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0) { /* XXX fscs may be relocated */
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	}
	cs = fscs + cylno;
	memset(&acg, 0, (size_t)sblock.fs_cgsize);
	acg.cg_time = utime;
	acg.cg_magic = CG_MAGIC;
	acg.cg_cgx = cylno;
	if (cylno == sblock.fs_ncg - 1) {
		acg.cg_ncyl = sblock.fs_ncyl % sblock.fs_cpg;
	} else {
		acg.cg_ncyl = sblock.fs_cpg;
	}
	acg.cg_niblk = sblock.fs_ipg;
	acg.cg_ndblk = dmax - cbase;
	if (sblock.fs_contigsumsize > 0) {
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	}
	acg.cg_btotoff = &acg.cg_space[0] - (u_char *)(&acg.cg_firstfield);
	acg.cg_boff = acg.cg_btotoff + sblock.fs_cpg * sizeof(int32_t);
	acg.cg_iusedoff = acg.cg_boff +
	    sblock.fs_cpg * sblock.fs_nrpos * sizeof(u_int16_t);
	acg.cg_freeoff = acg.cg_iusedoff + howmany(sblock.fs_ipg, NBBY);
	if (sblock.fs_contigsumsize <= 0) {
		acg.cg_nextfreeoff = acg.cg_freeoff +
		    howmany(sblock.fs_cpg* sblock.fs_spc/ NSPF(&sblock), NBBY);
	} else {
		acg.cg_clustersumoff = acg.cg_freeoff + howmany
		    (sblock.fs_cpg * sblock.fs_spc / NSPF(&sblock), NBBY) -
		    sizeof(u_int32_t);
		acg.cg_clustersumoff =
		    roundup(acg.cg_clustersumoff, sizeof(u_int32_t));
		acg.cg_clusteroff = acg.cg_clustersumoff +
		    (sblock.fs_contigsumsize + 1) * sizeof(u_int32_t);
		acg.cg_nextfreeoff = acg.cg_clusteroff + howmany
		    (sblock.fs_cpg * sblock.fs_spc / NSPB(&sblock), NBBY);
	}
	if (acg.cg_nextfreeoff-(int)(&acg.cg_firstfield) > sblock.fs_cgsize) {
		/*
		 * XXX This should never happen as we would have had that panic
		 *     already on filesystem creation
		 */
		errx(37, "panic: cylinder group too big");
	}
	acg.cg_cs.cs_nifree += sblock.fs_ipg;
	if (cylno == 0)
		for (i = 0; (size_t)i < ROOTINO; i++) {
			setbit(cg_inosused(&acg), i);
			acg.cg_cs.cs_nifree--;
		}
	for (i = 0; i < sblock.fs_ipg / INOPF(&sblock); i += sblock.fs_frag) {
#ifdef FSIRAND
		for (j = 0; j < sblock.fs_bsize / sizeof(struct dinode); j++) {
			zino[j].di_gen = random();
		}
#endif
		wtfs(fsbtodb(&sblock, cgimin(&sblock, cylno) + i),
		    (size_t)sblock.fs_bsize, (void *)zino, fso, Nflag);
	}
	for (d = 0; d < dlower; d += sblock.fs_frag) {
		blkno = d / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), blkno);
		if (sblock.fs_contigsumsize > 0) {
			setbit(cg_clustersfree(&acg), blkno);
		}
		acg.cg_cs.cs_nbfree++;
		cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
		cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
		    [cbtorpos(&sblock, d)]++;
	}
	sblock.fs_dsize += dlower;
	sblock.fs_dsize += acg.cg_ndblk - dupper;
	if ((i = dupper % sblock.fs_frag)) {
		acg.cg_frsum[sblock.fs_frag - i]++;
		for (d = dupper + sblock.fs_frag - i; dupper < d; dupper++) {
			setbit(cg_blksfree(&acg), dupper);
			acg.cg_cs.cs_nffree++;
		}
	}
	for (d = dupper; d + sblock.fs_frag <= dmax - cbase; ) {
		blkno = d / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), blkno);
		if (sblock.fs_contigsumsize > 0) {
			setbit(cg_clustersfree(&acg), blkno);
		}
		acg.cg_cs.cs_nbfree++;
		cg_blktot(&acg)[cbtocylno(&sblock, d)]++;
		cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
		    [cbtorpos(&sblock, d)]++;
		d += sblock.fs_frag;
	}
	if (d < dmax - cbase) {
		acg.cg_frsum[dmax - cbase - d]++;
		for (; d < dmax - cbase; d++) {
			setbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree++;
		}
	}
	if (sblock.fs_contigsumsize > 0) {
		int32_t	*sump = cg_clustersum(&acg);
		u_char	*mapp = cg_clustersfree(&acg);
		int	map = *mapp++;
		int	bit = 1;
		int	run = 0;

		for (i = 0; i < acg.cg_nclusterblks; i++) {
			if ((map & bit) != 0) {
				run++;
			} else if (run != 0) {
				if (run > sblock.fs_contigsumsize) {
					run = sblock.fs_contigsumsize;
				}
				sump[run]++;
				run = 0;
			}
			if ((i & (NBBY - 1)) != (NBBY - 1)) {
				bit <<= 1;
			} else {
				map = *mapp++;
				bit = 1;
			}
		}
		if (run != 0) {
			if (run > sblock.fs_contigsumsize) {
				run = sblock.fs_contigsumsize;
			}
			sump[run]++;
		}
	}
	sblock.fs_cstotal.cs_ndir += acg.cg_cs.cs_ndir;
	sblock.fs_cstotal.cs_nffree += acg.cg_cs.cs_nffree;
	sblock.fs_cstotal.cs_nbfree += acg.cg_cs.cs_nbfree;
	sblock.fs_cstotal.cs_nifree += acg.cg_cs.cs_nifree;
	*cs = acg.cg_cs;
	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
	    (size_t)sblock.fs_bsize, (void *)&acg, fso, Nflag);
	DBG_DUMP_CG(&sblock,
	    "new cg",
	    &acg);

	DBG_LEAVE;
	return;
}

/* ******************************************************* frag_adjust ***** */
/*
 * Here  we add or subtract (sign +1/-1) the available fragments in  a  given
 * block to or from the fragment statistics. By subtracting before and adding
 * after  an operation on the free frag map we can easy update  the  fragment
 * statistic, which seems to be otherwise an rather complex operation.
 */
static void
frag_adjust(daddr_t frag, int sign)
{
	DBG_FUNC("frag_adjust")
	int fragsize;
	int f;

	DBG_ENTER;

	fragsize=0;
	/*
	 * Here frag only needs to point to any fragment in the block we want
	 * to examine.
	 */
	for(f=rounddown(frag, sblock.fs_frag); 
	    f<roundup(frag+1, sblock.fs_frag);
	    f++) {
		/*
		 * Count contiguos free fragments.
		 */
		if(isset(cg_blksfree(&acg), f)) {
			fragsize++;
		} else {
			if(fragsize && fragsize<sblock.fs_frag) {
				/*
				 * We found something in between.
				 */
				acg.cg_frsum[fragsize]+=sign;
				DBG_PRINT2("frag_adjust [%d]+=%d\n",
				    fragsize,
				    sign);
			}
			fragsize=0;
		}
	}
	if(fragsize && fragsize<sblock.fs_frag) {
		/*
		 * We found something.
		 */
		acg.cg_frsum[fragsize]+=sign;
		DBG_PRINT2("frag_adjust [%d]+=%d\n",
		    fragsize,
		    sign);
	}
	DBG_PRINT2("frag_adjust [[%d]]+=%d\n",
	    fragsize,
	    sign);

	DBG_LEAVE;
	return;
}

/* ******************************************************* cond_bl_upd ***** */
/*
 * Here we conditionally update a pointer to a fragment. We check for all
 * relocated blocks if any of it's fragments is referenced by the current
 * field,  and update the pointer to the respective fragment in  our  new
 * block.  If  we find a reference we write back the  block  immediately,
 * as there is no easy way for our general block reading engine to figure
 * out if a write back operation is needed.
 */
static void
cond_bl_upd(ufs_daddr_t *block, struct gfs_bpp *field,
    enum pointer_source source, int fso, unsigned int Nflag)
{
	DBG_FUNC("cond_bl_upd")
	struct gfs_bpp	*f;
	char *src;
	daddr_t dst=0;

	DBG_ENTER;

	f=field;
	while(f->old) { /* for all old blocks */
		if(*block/sblock.fs_frag == f->old) {
			/*
			 * The fragment is part of the block, so update.
			 */
			*block=(f->new*sblock.fs_frag+(*block%sblock.fs_frag));
			f->found++;
			DBG_PRINT3("scg (%d->%d)[%d] reference updated\n",
			    f->old,
			    f->new,
			    *block%sblock.fs_frag);

			/* Write the block back to disk immediately */
			switch (source) {
			case GFS_PS_INODE:
				src=ablk;
				dst=in_src;
				break;
			case GFS_PS_IND_BLK_LVL1:
				src=i1blk;
				dst=i1_src;
				break;
			case GFS_PS_IND_BLK_LVL2:
				src=i2blk;
				dst=i2_src;
				break;
			case GFS_PS_IND_BLK_LVL3:
				src=i3blk;
				dst=i3_src;
				break;
			default:	/* error */
				src=NULL;
				break;
			}
			if(src) {
				/*
				 * XXX	If src is not of type inode we have to
				 *	implement  copy on write here in  case
				 *	of active snapshots.
				 */
				wtfs(dst, (size_t)sblock.fs_bsize, (void *)src,
				    fso, Nflag);
			}

			/*
			 * The same block can't be found again in this loop.
			 */
			break;
		}
		f++;
	}

	DBG_LEAVE;
	return;
}

/* ************************************************************ updjcg ***** */
/*
 * Here we do all needed work for the former last cylinder group. It has to be
 * changed  in  any case, even if the filesystem ended exactly on the  end  of
 * this  group, as there is some slightly inconsistent handling of the  number
 * of cylinders in the cylinder group. We start again by reading the  cylinder
 * group from disk. If the last block was not fully available, we first handle
 * the  missing  fragments, then we handle all new full blocks  in  that  file
 * system  and  finally we handle the new last fragmented block  in  the  file
 * system.  We again have to handle the fragment statistics rotational  layout
 * tables and cluster summary during all those operations.
 */
static void
updjcg(int cylno, time_t utime, int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("updjcg")
	daddr_t	cbase, dmax, dupper;
	struct csum	*cs;
	int	i,k;
	int	j=0;

	DBG_ENTER;

	/*
	 * Read the former last (joining) cylinder group from disk, and make
	 * a copy.
	 */
	rdfs(fsbtodb(&osblock, cgtod(&osblock, cylno)),
	    (size_t)osblock.fs_cgsize, (void *)&aocg, fsi);
	DBG_PRINT0("jcg read\n");
	DBG_DUMP_CG(&sblock,
	    "old joining cg",
	    &aocg);

	memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

	/*
	 * If  the  cylinder  group had already it's  new  final  size  almost
	 * nothing is to be done ... except:
	 * For some reason the value of cg_ncyl in the last cylinder group has
	 * to  be  zero instead of fs_cpg. As this is now no longer  the  last
	 * cylinder group we have to change that value now to fs_cpg.
	 */ 

	if(cgbase(&osblock, cylno+1) == osblock.fs_size) {
		acg.cg_ncyl=sblock.fs_cpg;

		wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)),
		    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
		DBG_PRINT0("jcg written\n");
		DBG_DUMP_CG(&sblock,
		    "new joining cg",
		    &acg);

		DBG_LEAVE;
		return;
	}

	/*
	 * Set up some variables needed later.
	 */
	cbase = cgbase(&sblock, cylno);
	dmax = cbase + sblock.fs_fpg;
	if (dmax > sblock.fs_size)
		dmax = sblock.fs_size;
	dupper = cgdmin(&sblock, cylno) - cbase;
	if (cylno == 0) { /* XXX fscs may be relocated */
		dupper += howmany(sblock.fs_cssize, sblock.fs_fsize);
	}

	/*
	 * Set pointer to the cylinder summary for our cylinder group.
	 */
	cs = fscs + cylno;

	/*
	 * Touch the cylinder group, update all fields in the cylinder group as
	 * needed, update the free space in the superblock.
	 */
	acg.cg_time = utime;
	if (cylno == sblock.fs_ncg - 1) {
		/*
		 * This is still the last cylinder group.
		 */
		acg.cg_ncyl = sblock.fs_ncyl % sblock.fs_cpg;
	} else {
		acg.cg_ncyl = sblock.fs_cpg;
	}
	DBG_PRINT4("jcg dbg: %d %u %d %u\n",
	    cylno,
	    sblock.fs_ncg,
	    acg.cg_ncyl,
	    sblock.fs_cpg);
	acg.cg_ndblk = dmax - cbase;
	sblock.fs_dsize += acg.cg_ndblk-aocg.cg_ndblk;
	if (sblock.fs_contigsumsize > 0) {
		acg.cg_nclusterblks = acg.cg_ndblk / sblock.fs_frag;
	}

	/*
	 * Now  we have to update the free fragment bitmap for our new  free
	 * space.  There again we have to handle the fragmentation and  also
	 * the  rotational  layout tables and the cluster summary.  This  is
	 * also  done per fragment for the first new block if the  old  file
	 * system end was not on a block boundary, per fragment for the  new
	 * last block if the new file system end is not on a block boundary,
	 * and per block for all space in between.
	 *
	 * Handle the first new block here if it was partially available
	 * before.
	 */
	if(osblock.fs_size % sblock.fs_frag) {
		if(roundup(osblock.fs_size, sblock.fs_frag)<=sblock.fs_size) {
			/*
			 * The new space is enough to fill at least this
			 * block
			 */
			j=0;
			for(i=roundup(osblock.fs_size-cbase, sblock.fs_frag)-1;
			    i>=osblock.fs_size-cbase;
			    i--) {
				setbit(cg_blksfree(&acg), i);
				acg.cg_cs.cs_nffree++;
				j++;
			}

			/*
			 * Check  if the fragment just created could join  an
			 * already existing fragment at the former end of the
			 * file system.
			 */
			if(isblock(&sblock, cg_blksfree(&acg),
			    ((osblock.fs_size - cgbase(&sblock, cylno))/
			    sblock.fs_frag))) {
				/*
				 * The block is now completely available
				 */
				DBG_PRINT0("block was\n");
				acg.cg_frsum[osblock.fs_size%sblock.fs_frag]--;
				acg.cg_cs.cs_nbfree++;
				acg.cg_cs.cs_nffree-=sblock.fs_frag;
				k=rounddown(osblock.fs_size-cbase,
				    sblock.fs_frag);
				cg_blktot(&acg)[cbtocylno(&sblock, k)]++;
				cg_blks(&sblock, &acg, cbtocylno(&sblock, k))
		   		    [cbtorpos(&sblock, k)]++;
				updclst((osblock.fs_size-cbase)/sblock.fs_frag);
			} else {
				/*
				 * Lets rejoin a possible partially growed
				 * fragment.
				 */
				k=0;
				while(isset(cg_blksfree(&acg), i) &&
				    (i>=rounddown(osblock.fs_size-cbase,
				    sblock.fs_frag))) {
					i--;
					k++;
				}
				if(k) {
					acg.cg_frsum[k]--;
				}
				acg.cg_frsum[k+j]++;
			}
		} else {
			/*
			 * We only grow by some fragments within this last
			 * block.
			 */
			for(i=sblock.fs_size-cbase-1;
				i>=osblock.fs_size-cbase;
				i--) {
				setbit(cg_blksfree(&acg), i);
				acg.cg_cs.cs_nffree++;
				j++;
			}
			/*
			 * Lets rejoin a possible partially growed fragment.
			 */
			k=0;
			while(isset(cg_blksfree(&acg), i) &&
			    (i>=rounddown(osblock.fs_size-cbase,
			    sblock.fs_frag))) {
				i--;
				k++;
			}
			if(k) {
				acg.cg_frsum[k]--;
			}
			acg.cg_frsum[k+j]++;
		}
	}

	/*
	 * Handle all new complete blocks here.
	 */
	for(i=roundup(osblock.fs_size-cbase, sblock.fs_frag);
	    i+sblock.fs_frag<=dmax-cbase;	/* XXX <= or only < ? */
	    i+=sblock.fs_frag) {
		j = i / sblock.fs_frag;
		setblock(&sblock, cg_blksfree(&acg), j);
		updclst(j);
		acg.cg_cs.cs_nbfree++;
		cg_blktot(&acg)[cbtocylno(&sblock, i)]++;
		cg_blks(&sblock, &acg, cbtocylno(&sblock, i))
		    [cbtorpos(&sblock, i)]++;
	}

	/*
	 * Handle the last new block if there are stll some new fragments left.
	 * Here  we don't have to bother about the cluster summary or the  even
	 * the rotational layout table.
	 */
	if (i < (dmax - cbase)) {
		acg.cg_frsum[dmax - cbase - i]++;
		for (; i < dmax - cbase; i++) {
			setbit(cg_blksfree(&acg), i);
			acg.cg_cs.cs_nffree++;
		}
	}

	sblock.fs_cstotal.cs_nffree +=
	    (acg.cg_cs.cs_nffree - aocg.cg_cs.cs_nffree);
	sblock.fs_cstotal.cs_nbfree +=
	    (acg.cg_cs.cs_nbfree - aocg.cg_cs.cs_nbfree);
	/*
	 * The following statistics are not changed here:
	 *     sblock.fs_cstotal.cs_ndir
	 *     sblock.fs_cstotal.cs_nifree
	 * As the statistics for this cylinder group are ready, copy it to
	 * the summary information array.
	 */
	*cs = acg.cg_cs;

	/*
	 * Write the updated "joining" cylinder group back to disk.
	 */
	wtfs(fsbtodb(&sblock, cgtod(&sblock, cylno)), (size_t)sblock.fs_cgsize,
	    (void *)&acg, fso, Nflag);
	DBG_PRINT0("jcg written\n");
	DBG_DUMP_CG(&sblock,
	    "new joining cg",
	    &acg);

	DBG_LEAVE;
	return;
}

/* ********************************************************** updcsloc ***** */
/*
 * Here  we update the location of the cylinder summary. We have  two  possible
 * ways of growing the cylinder summary.
 * (1)	We can try to grow the summary in the current location, and  relocate
 *	possibly used blocks within the current cylinder group.
 * (2)	Alternatively we can relocate the whole cylinder summary to the first
 *	new completely empty cylinder group. Once the cylinder summary is  no
 *	longer in the beginning of the first cylinder group you should  never
 *	use  a version of fsck which is not aware of the possibility to  have
 *	this structure in a non standard place.
 * Option (1) is considered to be less intrusive to the structure of the  file-
 * system. So we try to stick to that whenever possible. If there is not enough
 * space  in the cylinder group containing the cylinder summary we have to  use
 * method  (2). In case of active snapshots in the filesystem we  probably  can
 * completely avoid implementing copy on write if we stick to method (2) only.
 */
static void
updcsloc(time_t utime, int fsi, int fso, unsigned int Nflag)
{
	DBG_FUNC("updcsloc")
	struct csum	*cs;
	int	ocscg, ncscg;
	int	blocks;
	daddr_t	cbase, dupper, odupper, d, f, g;
	int	ind;
	int	cylno, inc;
	struct gfs_bpp	*bp;
	int	i, l;
	int	lcs=0;
	int	block;

	DBG_ENTER;

	if(howmany(sblock.fs_cssize, sblock.fs_fsize) ==
	    howmany(osblock.fs_cssize, osblock.fs_fsize)) {
		/*
		 * No new fragment needed.
		 */
		DBG_LEAVE;
		return;
	}
	ocscg=dtog(&osblock, osblock.fs_csaddr);
	cs=fscs+ocscg;
	blocks = 1+howmany(sblock.fs_cssize, sblock.fs_bsize)-
	    howmany(osblock.fs_cssize, osblock.fs_bsize);

	/*
	 * Read original cylinder group from disk, and make a copy.
	 * XXX	If Nflag is set in some very rare cases we now miss
	 *	some changes done in updjcg by reading the unmodified
	 *	block from disk.
	 */
	rdfs(fsbtodb(&osblock, cgtod(&osblock, ocscg)),
	    (size_t)osblock.fs_cgsize, (void *)&aocg, fsi);
	DBG_PRINT0("oscg read\n");
	DBG_DUMP_CG(&sblock,
	    "old summary cg",
	    &aocg);

	memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

	/*
	 * Touch the cylinder group, set up local variables needed later
	 * and update the superblock.
	 */
	acg.cg_time = utime;

	/*
	 * XXX	In the case of having active snapshots we may need much more
	 *	blocks for the copy on write. We need each block twice,  and
	 *	also  up to 8*3 blocks for indirect blocks for all  possible
	 *	references.
	 */
	if(/*((int)sblock.fs_time&0x3)>0||*/ cs->cs_nbfree < blocks) {
		/*
		 * There  is  not enough space in the old cylinder  group  to
		 * relocate  all blocks as needed, so we relocate  the  whole
		 * cylinder  group summary to a new group. We try to use  the
		 * first complete new cylinder group just created. Within the
		 * cylinder  group we allign the area immediately  after  the
		 * cylinder  group  information location in order  to  be  as
		 * close as possible to the original implementation of ffs.
		 *
		 * First  we have to make sure we'll find enough space in  the
		 * new  cylinder  group. If not, then we  currently  give  up.
		 * We  start  with freeing everything which was  used  by  the
		 * fragments of the old cylinder summary in the current group.
		 * Now  we write back the group meta data, read in the  needed
		 * meta data from the new cylinder group, and start allocating
		 * within  that  group. Here we can assume, the  group  to  be
		 * completely empty. Which makes the handling of fragments and
		 * clusters a lot easier.
		 */
		DBG_TRC;
		if(sblock.fs_ncg-osblock.fs_ncg < 2) {
			errx(2, "panic: not enough space");
		}

		/*
		 * Point "d" to the first fragment not used by the cylinder
		 * summary.
		 */
		d=osblock.fs_csaddr+(osblock.fs_cssize/osblock.fs_fsize);

		/*
		 * Set up last cluster size ("lcs") already here. Calculate
		 * the size for the trailing cluster just behind where  "d"
		 * points to.
		 */
		if(sblock.fs_contigsumsize > 0) {
			for(block=howmany(d%sblock.fs_fpg, sblock.fs_frag),
			    lcs=0; lcs<sblock.fs_contigsumsize;
			    block++, lcs++) {
				if(isclr(cg_clustersfree(&acg), block)){
					break;
				}
			}
		}

		/*
		 * Point "d" to the last frag used by the cylinder summary.
		 */
		d--;

		DBG_PRINT1("d=%d\n",
		    d);
		if((d+1)%sblock.fs_frag) {
			/*
			 * The end of the cylinder summary is not a complete
			 * block.
			 */
			DBG_TRC;
			frag_adjust(d%sblock.fs_fpg, -1);
			for(; (d+1)%sblock.fs_frag; d--) {
				DBG_PRINT1("d=%d\n",
				    d);
				setbit(cg_blksfree(&acg), d%sblock.fs_fpg);
				acg.cg_cs.cs_nffree++;
				sblock.fs_cstotal.cs_nffree++;
			}
			/*
			 * Point  "d" to the last fragment of the  last
			 * (incomplete) block of the clinder summary.
			 */
			d++;
			frag_adjust(d%sblock.fs_fpg, 1);

			if(isblock(&sblock, cg_blksfree(&acg),
			    (d%sblock.fs_fpg)/sblock.fs_frag)) {
				DBG_PRINT1("d=%d\n",
				    d);
				acg.cg_cs.cs_nffree-=sblock.fs_frag;
				acg.cg_cs.cs_nbfree++;
				sblock.fs_cstotal.cs_nffree-=sblock.fs_frag;
				sblock.fs_cstotal.cs_nbfree++;
				cg_blktot(&acg)[cbtocylno(&sblock,
				    d%sblock.fs_fpg)]++;
				cg_blks(&sblock, &acg, cbtocylno(&sblock,
				    d%sblock.fs_fpg))[cbtorpos(&sblock,
				    d%sblock.fs_fpg)]++;
				if(sblock.fs_contigsumsize > 0) {
					setbit(cg_clustersfree(&acg),
					    (d%sblock.fs_fpg)/sblock.fs_frag);
					if(lcs < sblock.fs_contigsumsize) {
						if(lcs) {
							cg_clustersum(&acg)
							    [lcs]--;
						}
						lcs++;
						cg_clustersum(&acg)[lcs]++;
					}
				}
			}
			/*
			 * Point "d" to the first fragment of the block before
			 * the last incomplete block.
			 */
			d--;
		}

		DBG_PRINT1("d=%d\n",
		    d);
		for(d=rounddown(d, sblock.fs_frag); d >= osblock.fs_csaddr;
		    d-=sblock.fs_frag) {
			DBG_TRC;
			DBG_PRINT1("d=%d\n",
			    d);
			setblock(&sblock, cg_blksfree(&acg),
			    (d%sblock.fs_fpg)/sblock.fs_frag);
			acg.cg_cs.cs_nbfree++;
			sblock.fs_cstotal.cs_nbfree++;
			cg_blktot(&acg)[cbtocylno(&sblock, d%sblock.fs_fpg)]++;
			cg_blks(&sblock, &acg, cbtocylno(&sblock,
			    d%sblock.fs_fpg))[cbtorpos(&sblock,
			    d%sblock.fs_fpg)]++;
			if(sblock.fs_contigsumsize > 0) {
				setbit(cg_clustersfree(&acg),
				    (d%sblock.fs_fpg)/sblock.fs_frag);
				/*
				 * The last cluster size is already set up.
				 */
				if(lcs < sblock.fs_contigsumsize) {
					if(lcs) {
						cg_clustersum(&acg)[lcs]--;
					}
					lcs++;
					cg_clustersum(&acg)[lcs]++;
				}
			}
		}
		*cs = acg.cg_cs;

		/*
		 * Now write the former cylinder group containing the cylinder
		 * summary back to disk.
		 */
		wtfs(fsbtodb(&sblock, cgtod(&sblock, ocscg)),
		    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
		DBG_PRINT0("oscg written\n");
		DBG_DUMP_CG(&sblock,
		    "old summary cg",
		    &acg);

		/*
		 * Find the beginning of the new cylinder group containing the
		 * cylinder summary.
		 */
		sblock.fs_csaddr=cgdmin(&sblock, osblock.fs_ncg);
		ncscg=dtog(&sblock, sblock.fs_csaddr);
		cs=fscs+ncscg;


		/*
		 * If Nflag is specified, we would now read random data instead
		 * of an empty cg structure from disk. So we can't simulate that
		 * part for now.
		 */
		if(Nflag) {
			DBG_PRINT0("nscg update skipped\n");
			DBG_LEAVE;
			return;
		}

		/*
		 * Read the future cylinder group containing the cylinder
		 * summary from disk, and make a copy.
		 */
		rdfs(fsbtodb(&sblock, cgtod(&sblock, ncscg)),
		    (size_t)sblock.fs_cgsize, (void *)&aocg, fsi);
		DBG_PRINT0("nscg read\n");
		DBG_DUMP_CG(&sblock,
		    "new summary cg",
		    &aocg);

		memcpy((void *)&cgun1, (void *)&cgun2, sizeof(cgun2));

		/*
		 * Allocate all complete blocks used by the new cylinder
		 * summary.
		 */
		for(d=sblock.fs_csaddr; d+sblock.fs_frag <=
		    sblock.fs_csaddr+(sblock.fs_cssize/sblock.fs_fsize);
		    d+=sblock.fs_frag) {
			clrblock(&sblock, cg_blksfree(&acg),
			    (d%sblock.fs_fpg)/sblock.fs_frag);
			acg.cg_cs.cs_nbfree--;
			sblock.fs_cstotal.cs_nbfree--;
			cg_blktot(&acg)[cbtocylno(&sblock, d%sblock.fs_fpg)]--;
			cg_blks(&sblock, &acg, cbtocylno(&sblock,
			    d%sblock.fs_fpg))[cbtorpos(&sblock,
			    d%sblock.fs_fpg)]--;
			if(sblock.fs_contigsumsize > 0) {
				clrbit(cg_clustersfree(&acg),
				    (d%sblock.fs_fpg)/sblock.fs_frag);
			}
		}

		/*
		 * Allocate all fragments used by the cylinder summary in the
		 * last block.
		 */
		if(d<sblock.fs_csaddr+(sblock.fs_cssize/sblock.fs_fsize)) {
			for(; d-sblock.fs_csaddr<
			    sblock.fs_cssize/sblock.fs_fsize;
			    d++) {
				clrbit(cg_blksfree(&acg), d%sblock.fs_fpg);
				acg.cg_cs.cs_nffree--;
				sblock.fs_cstotal.cs_nffree--;
			}
			acg.cg_cs.cs_nbfree--;
			acg.cg_cs.cs_nffree+=sblock.fs_frag;
			sblock.fs_cstotal.cs_nbfree--;
			sblock.fs_cstotal.cs_nffree+=sblock.fs_frag;
			cg_blktot(&acg)[cbtocylno(&sblock, d%sblock.fs_fpg)]--;
			cg_blks(&sblock, &acg, cbtocylno(&sblock,
			    d%sblock.fs_fpg))[cbtorpos(&sblock,
			    d%sblock.fs_fpg)]--;
			if(sblock.fs_contigsumsize > 0) {
				clrbit(cg_clustersfree(&acg),
				    (d%sblock.fs_fpg)/sblock.fs_frag);
			}

			frag_adjust(d%sblock.fs_fpg, +1);
		}
		/*
		 * XXX	Handle the cluster statistics here in the case  this
		 *	cylinder group is now almost full, and the remaining
		 *	space is less then the maximum cluster size. This is
		 *	probably not needed, as you would hardly find a file
		 *	system which has only MAXCSBUFS+FS_MAXCONTIG of free
		 *	space right behind the cylinder group information in
		 *	any new cylinder group.
		 */

		/*
		 * Update our statistics in the cylinder summary.
		 */
		*cs = acg.cg_cs;

		/*
		 * Write the new cylinder group containing the cylinder summary
		 * back to disk.
		 */
		wtfs(fsbtodb(&sblock, cgtod(&sblock, ncscg)),
		    (size_t)sblock.fs_cgsize, (void *)&acg, fso, Nflag);
		DBG_PRINT0("nscg written\n");
		DBG_DUMP_CG(&sblock,
		    "new summary cg",
		    &acg);

		DBG_LEAVE;
		return;
	}
	/*
	 * We have got enough of space in the current cylinder group, so we
	 * can relocate just a few blocks, and let the summary  information
	 * grow in place where it is right now.
	 */
	DBG_TRC;

	cbase = cgbase(&osblock, ocscg);	/* old and new are equal */
	dupper = sblock.fs_csaddr - cbase +
	    howmany(sblock.fs_cssize, sblock.fs_fsize);
	odupper = osblock.fs_csaddr - cbase +
	    howmany(osblock.fs_cssize, osblock.fs_fsize);

	sblock.fs_dsize -= dupper-odupper;

	/*
	 * Allocate the space for the array of blocks to be relocated.
	 */
 	bp=(struct gfs_bpp *)malloc(((dupper-odupper)/sblock.fs_frag+2)*
	    sizeof(struct gfs_bpp));
	if(bp == NULL) {
		errx(1, "malloc failed");
	}
	memset((char *)bp, 0, ((dupper-odupper)/sblock.fs_frag+2)*
	    sizeof(struct gfs_bpp));

	/*
	 * Lock all new frags needed for the cylinder group summary. This  is
	 * done per fragment in the first and last block of the new  required
	 * area, and per block for all other blocks.
	 *
	 * Handle the first new  block here (but only if some fragments where
	 * already used for the cylinder summary).
	 */
	ind=0;
	frag_adjust(odupper, -1);
	for(d=odupper; ((d<dupper)&&(d%sblock.fs_frag)); d++) {
		DBG_PRINT1("scg first frag check loop d=%d\n",
		    d);
		if(isclr(cg_blksfree(&acg), d)) {
			if (!ind) {
				bp[ind].old=d/sblock.fs_frag;
				bp[ind].flags|=GFS_FL_FIRST;
				if(roundup(d, sblock.fs_frag) >= dupper) {
					bp[ind].flags|=GFS_FL_LAST;
				}
				ind++;
			}
		} else {
			clrbit(cg_blksfree(&acg), d);
			acg.cg_cs.cs_nffree--;
			sblock.fs_cstotal.cs_nffree--;
		}
		/*
		 * No cluster handling is needed here, as there was at least
		 * one  fragment in use by the cylinder summary in  the  old
		 * file system.
		 * No block-free counter handling here as this block was not
		 * a free block.
		 */
	}
	frag_adjust(odupper, 1);

	/*
	 * Handle all needed complete blocks here.
	 */
	for(; d+sblock.fs_frag<=dupper; d+=sblock.fs_frag) {
		DBG_PRINT1("scg block check loop d=%d\n",
		    d);
		if(!isblock(&sblock, cg_blksfree(&acg), d/sblock.fs_frag)) {
			for(f=d; f<d+sblock.fs_frag; f++) {
				if(isset(cg_blksfree(&aocg), f)) {
					acg.cg_cs.cs_nffree--;
					sblock.fs_cstotal.cs_nffree--;
				}
			}
			clrblock(&sblock, cg_blksfree(&acg), d/sblock.fs_frag);
			bp[ind].old=d/sblock.fs_frag;
			ind++;
		} else {
			clrblock(&sblock, cg_blksfree(&acg), d/sblock.fs_frag);
			acg.cg_cs.cs_nbfree--;
			sblock.fs_cstotal.cs_nbfree--;
			cg_blktot(&acg)[cbtocylno(&sblock, d)]--;
			cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
			    [cbtorpos(&sblock, d)]--;
			if(sblock.fs_contigsumsize > 0) {
				clrbit(cg_clustersfree(&acg), d/sblock.fs_frag);
				for(lcs=0, l=(d/sblock.fs_frag)+1;
				    lcs<sblock.fs_contigsumsize;
				    l++, lcs++ ) {
					if(isclr(cg_clustersfree(&acg),l)){
						break;
					}
				}
				if(lcs < sblock.fs_contigsumsize) {
					cg_clustersum(&acg)[lcs+1]--;
					if(lcs) {
						cg_clustersum(&acg)[lcs]++;
					}
				}
			}
		}
		/*
		 * No fragment counter handling is needed here, as this finally
		 * doesn't change after the relocation.
		 */
	}

	/*
	 * Handle all fragments needed in the last new affected block.
	 */
	if(d<dupper) {
		frag_adjust(dupper-1, -1);

		if(isblock(&sblock, cg_blksfree(&acg), d/sblock.fs_frag)) {
			acg.cg_cs.cs_nbfree--;
			sblock.fs_cstotal.cs_nbfree--;
			acg.cg_cs.cs_nffree+=sblock.fs_frag;
			sblock.fs_cstotal.cs_nffree+=sblock.fs_frag;
			cg_blktot(&acg)[cbtocylno(&sblock, d)]--;
			cg_blks(&sblock, &acg, cbtocylno(&sblock, d))
			    [cbtorpos(&sblock, d)]--;
			if(sblock.fs_contigsumsize > 0) {
				clrbit(cg_clustersfree(&acg), d/sblock.fs_frag);
				for(lcs=0, l=(d/sblock.fs_frag)+1;
				    lcs<sblock.fs_contigsumsize;
				    l++, lcs++ ) {
					if(isclr(cg_clustersfree(&acg),l)){
						break;
					}
				}
				if(lcs < sblock.fs_contigsumsize) {
					cg_clustersum(&acg)[lcs+1]--;
					if(lcs) {
						cg_clustersum(&acg)[lcs]++;
					}
				}
			}
		}

		for(; d<dupper; d++) {
			DBG_PRINT1("scg second frag check loop d=%d\n",
			    d);
			if(isclr(cg_blksfree(&acg), d)) {
				bp[ind].old=d/sblock.fs_frag;
				bp[ind].flags|=GFS_FL_LAST;
			} else {
				clrbit(cg_blksfree(&acg), d);
				acg.cg_cs.cs_nffree--;
				sblock.fs_cstotal.cs_nffree--;
			}
		}
		if(bp[ind].flags & GFS_FL_LAST) { /* we have to advance here */
			ind++;
		}
		frag_adjust(dupper-1, 1);
	}

	/*
	 * If we found a block to relocate just do so.
	 */
	if(ind) {
		for(i=0; i<ind; i++) {
			if(!bp[i].old) { /* no more blocks listed */
				/*
				 * XXX	A relative blocknumber should not be
				 *	zero,   which  is   not   explicitly
				 *	guaranteed by our code.
				 */
				break;
			}
			/*
			 * Allocate a complete block in the same (current)
			 * cylinder group.
			 */
			bp[i].new=alloc()/sblock.fs_frag;

			/*
			 * There is no frag_adjust() needed for the new block
			 * as it will have no fragments yet :-).
			 */
			for(f=bp[i].old*sblock.fs_frag,
			    g=bp[i].new*sblock.fs_frag;
			    f<(bp[i].old+1)*sblock.fs_frag;
			    f++, g++) {
				if(isset(cg_blksfree(&aocg), f)) {
					setbit(cg_blksfree(&acg), g);
					acg.cg_cs.cs_nffree++;
					sblock.fs_cstotal.cs_nffree++;
				}
			}

			/*
			 * Special handling is required if this was the  first
			 * block. We have to consider the fragments which were
			 * used by the cylinder summary in the original  block
			 * which  re to be free in the copy of our  block.  We
			 * have  to be careful if this first block happens  to
			 * be also the last block to be relocated.
			 */
			if(bp[i].flags & GFS_FL_FIRST) {
				for(f=bp[i].old*sblock.fs_frag,
				    g=bp[i].new*sblock.fs_frag;
				    f<odupper;
				    f++, g++) {
					setbit(cg_blksfree(&acg), g);
					acg.cg_cs.cs_nffree++;
					sblock.fs_cstotal.cs_nffree++;
				}
				if(!(bp[i].flags & GFS_FL_LAST)) {
					frag_adjust(bp[i].new*sblock.fs_frag,1);
				}
				
			}

			/*
			 * Special handling is required if this is the last
			 * block to be relocated.
			 */
			if(bp[i].flags & GFS_FL_LAST) {
				frag_adjust(bp[i].new*sblock.fs_frag, 1);
				frag_adjust(bp[i].old*sblock.fs_frag, -1);
				for(f=dupper;
				    f<roundup(dupper, sblock.fs_frag);
				    f++) {
					if(isclr(cg_blksfree(&acg), f)) {
						setbit(cg_blksfree(&acg), f);
						acg.cg_cs.cs_nffree++;
						sblock.fs_cstotal.cs_nffree++;
					}
				}
				frag_adjust(bp[i].old*sblock.fs_frag, 1);
			}

			/*
			 * !!! Attach the cylindergroup offset here. 
			 */
			bp[i].old+=cbase/sblock.fs_frag;
			bp[i].new+=cbase/sblock.fs_frag;

			/*
			 * Copy the content of the block.
			 */
			/*
			 * XXX	Here we will have to implement a copy on write
			 *	in the case we have any active snapshots.
			 */
			rdfs(fsbtodb(&sblock, bp[i].old*sblock.fs_frag),
			    (size_t)sblock.fs_bsize, (void *)&ablk, fsi);
			wtfs(fsbtodb(&sblock, bp[i].new*sblock.fs_frag),
			    (size_t)sblock.fs_bsize, (void *)&ablk, fso, Nflag);
			DBG_DUMP_HEX(&sblock,
			    "copied full block",
			    (unsigned char *)&ablk);

			DBG_PRINT2("scg (%d->%d) block relocated\n",
			    bp[i].old,
			    bp[i].new);
		}

		/*
		 * Now we have to update all references to any fragment which
		 * belongs  to any block relocated. We iterate now  over  all
		 * cylinder  groups,  within those over all non  zero  length 
		 * inodes.
		 */
		for(cylno=0; cylno<osblock.fs_ncg; cylno++) {
			DBG_PRINT1("scg doing cg (%d)\n",
			    cylno);
			for(inc=osblock.fs_ipg-1 ; inc>=0 ; inc--) {
				updrefs(cylno, (ino_t)inc, bp, fsi, fso, Nflag);
			}
		}

		/*
		 * All inodes are checked, now make sure the number of
		 * references found make sense.
		 */
		for(i=0; i<ind; i++) {
			if(!bp[i].found || (bp[i].found>sblock.fs_frag)) {
				warnx("error: %d refs found for block %d.",
				    bp[i].found, bp[i].old);
			}

		}
	}
	/*
	 * The following statistics are not changed here:
	 *     sblock.fs_cstotal.cs_ndir
	 *     sblock.fs_cstotal.cs_nifree
	 * The following statistics were already updated on the fly:
	 *     sblock.fs_cstotal.cs_nffree
	 *     sblock.fs_cstotal.cs_nbfree
	 * As the statistics for this cylinder group are ready, copy it to
	 * the summary information array.
	 */

	*cs = acg.cg_cs;

	/*
	 * Write summary cylinder group back to disk.
	 */
	wtfs(fsbtodb(&sblock, cgtod(&sblock, ocscg)), (size_t)sblock.fs_cgsize,
	    (void *)&acg, fso, Nflag);
	DBG_PRINT0("scg written\n");
	DBG_DUMP_CG(&sblock,
	    "new summary cg",
	    &acg);

	DBG_LEAVE;
	return;
}

/* ************************************************************** rdfs ***** */
/*
 * Here we read some block(s) from disk.
 */
static void
rdfs(daddr_t bno, size_t size, void *bf, int fsi)
{
	DBG_FUNC("rdfs")
	ssize_t	n;

	DBG_ENTER;

	if (lseek(fsi, (off_t)bno * DEV_BSIZE, 0) < 0) {
		err(33, "rdfs: seek error: %ld", (long)bno);
	}
	n = read(fsi, bf, size);
	if (n != (ssize_t)size) {
		err(34, "rdfs: read error: %ld", (long)bno);
	}

	DBG_LEAVE;
	return;
}

/* ************************************************************** wtfs ***** */
/*
 * Here we write some block(s) to disk.
 */
static void
wtfs(daddr_t bno, size_t size, void *bf, int fso, unsigned int Nflag)
{
	DBG_FUNC("wtfs")
	ssize_t	n;

	DBG_ENTER;

	if (Nflag) {
		DBG_LEAVE;
		return;
	}
	if (lseek(fso, (off_t)bno * DEV_BSIZE, SEEK_SET) < 0) {
		err(35, "wtfs: seek error: %ld", (long)bno);
	}
	n = write(fso, bf, size);
	if (n != (ssize_t)size) {
		err(36, "wtfs: write error: %ld", (long)bno);
	}

	DBG_LEAVE;
	return;
}

/* ************************************************************* alloc ***** */
/*
 * Here we allocate a free block in the current cylinder group. It is assumed,
 * that  acg contains the current cylinder group. As we may take a block  from
 * somewhere in the filesystem we have to handle cluster summary here.
 */
static daddr_t
alloc(void)
{
	DBG_FUNC("alloc")
	daddr_t	d, blkno;
	int	lcs1, lcs2;
	int	l;
	int	csmin, csmax;
	int	dlower, dupper, dmax;

	DBG_ENTER;

	if (acg.cg_magic != CG_MAGIC) {
		warnx("acg: bad magic number");
		DBG_LEAVE;
		return (0);
	}
	if (acg.cg_cs.cs_nbfree == 0) {
		warnx("error: cylinder group ran out of space");
		DBG_LEAVE;
		return (0);
	}
	/*
	 * We start seeking for free blocks only from the space available after
	 * the  end of the new grown cylinder summary. Otherwise we allocate  a
	 * block here which we have to relocate a couple of seconds later again
	 * again, and we are not prepared to to this anyway.
	 */
	blkno=-1;
	dlower=cgsblock(&sblock, acg.cg_cgx)-cgbase(&sblock, acg.cg_cgx);
	dupper=cgdmin(&sblock, acg.cg_cgx)-cgbase(&sblock, acg.cg_cgx);
	dmax=cgbase(&sblock, acg.cg_cgx)+sblock.fs_fpg;
	if (dmax > sblock.fs_size) {
		dmax = sblock.fs_size;
	}
	dmax-=cgbase(&sblock, acg.cg_cgx); /* retransform into cg */
	csmin=sblock.fs_csaddr-cgbase(&sblock, acg.cg_cgx);
	csmax=csmin+howmany(sblock.fs_cssize, sblock.fs_fsize);
	DBG_PRINT3("seek range: dl=%d, du=%d, dm=%d\n",
	    dlower,
	    dupper,
	    dmax);
	DBG_PRINT2("range cont: csmin=%d, csmax=%d\n",
	    csmin,
	    csmax);

	for(d=0; (d<dlower && blkno==-1); d+=sblock.fs_frag) {
		if(d>=csmin && d<=csmax) {
			continue;
		}
		if(isblock(&sblock, cg_blksfree(&acg), fragstoblks(&sblock,
		    d))) {
			blkno = fragstoblks(&sblock, d);/* Yeah found a block */
			break;
		}
	}
	for(d=dupper; (d<dmax && blkno==-1); d+=sblock.fs_frag) {
		if(d>=csmin && d<=csmax) {
			continue;
		}
		if(isblock(&sblock, cg_blksfree(&acg), fragstoblks(&sblock,
		    d))) {
			blkno = fragstoblks(&sblock, d);/* Yeah found a block */
			break;
		}
	}
	if(blkno==-1) {
		warnx("internal error: couldn't find promised block in cg");
		DBG_LEAVE;
		return (0);
	}

	/*
	 * This is needed if the block was found already in the first loop.
	 */
	d=blkstofrags(&sblock, blkno);

	clrblock(&sblock, cg_blksfree(&acg), blkno);
	if (sblock.fs_contigsumsize > 0) {
		/*
		 * Handle the cluster allocation bitmap.
		 */
		clrbit(cg_clustersfree(&acg), blkno);
		/*
		 * We  possibly have split a cluster here, so we have  to  do
		 * recalculate the sizes of the remaining cluster halves now,
		 * and use them for updating the cluster summary information.
		 *
		 * Lets start with the blocks before our allocated block ...
		 */
		for(lcs1=0, l=blkno-1; lcs1<sblock.fs_contigsumsize;
		    l--, lcs1++ ) {
			if(isclr(cg_clustersfree(&acg),l)){
				break;
			}
		}
		/*
		 * ... and continue with the blocks right after our allocated
		 * block.
		 */
		for(lcs2=0, l=blkno+1; lcs2<sblock.fs_contigsumsize;
		    l++, lcs2++ ) {
			if(isclr(cg_clustersfree(&acg),l)){
				break;
			}
		}

		/*
		 * Now update all counters.
		 */
		cg_clustersum(&acg)[MIN(lcs1+lcs2+1,sblock.fs_contigsumsize)]--;
		if(lcs1) {
			cg_clustersum(&acg)[lcs1]++;
		}
		if(lcs2) {
			cg_clustersum(&acg)[lcs2]++;
		}
	}
	/*
	 * Update all statistics based on blocks.
	 */
	acg.cg_cs.cs_nbfree--;
	sblock.fs_cstotal.cs_nbfree--;
	cg_blktot(&acg)[cbtocylno(&sblock, d)]--;
	cg_blks(&sblock, &acg, cbtocylno(&sblock, d))[cbtorpos(&sblock, d)]--;

	DBG_LEAVE;
	return (d);
}

/* *********************************************************** isblock ***** */
/*
 * Here  we check if all frags of a block are free. For more details  again
 * please see the source of newfs(8), as this function is taken over almost
 * unchanged.
 */
static int
isblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("isblock")
	unsigned char	mask;

	DBG_ENTER;

	switch (fs->fs_frag) {
	case 8:
		DBG_LEAVE;
		return (cp[h] == 0xff);
	case 4:
		mask = 0x0f << ((h & 0x1) << 2);
		DBG_LEAVE;
		return ((cp[h >> 1] & mask) == mask);
	case 2:
		mask = 0x03 << ((h & 0x3) << 1);
		DBG_LEAVE;
		return ((cp[h >> 2] & mask) == mask);
	case 1:
		mask = 0x01 << (h & 0x7);
		DBG_LEAVE;
		return ((cp[h >> 3] & mask) == mask);
	default:
		fprintf(stderr, "isblock bad fs_frag %d\n", fs->fs_frag);
		DBG_LEAVE;
		return (0);
	}
}

/* ********************************************************** clrblock ***** */
/*
 * Here we allocate a complete block in the block map. For more details again
 * please  see the source of newfs(8), as this function is taken over  almost
 * unchanged.
 */
static void
clrblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("clrblock")

	DBG_ENTER;

	switch ((fs)->fs_frag) {
	case 8:
		cp[h] = 0;
		break;
	case 4:
		cp[h >> 1] &= ~(0x0f << ((h & 0x1) << 2));
		break;
	case 2:
		cp[h >> 2] &= ~(0x03 << ((h & 0x3) << 1));
		break;
	case 1:
		cp[h >> 3] &= ~(0x01 << (h & 0x7));
		break;
	default:
		warnx("clrblock bad fs_frag %d", fs->fs_frag);
		break;
	}

	DBG_LEAVE;
	return;
}

/* ********************************************************** setblock ***** */
/*
 * Here we free a complete block in the free block map. For more details again
 * please  see the source of newfs(8), as this function is taken  over  almost
 * unchanged.
 */
static void
setblock(struct fs *fs, unsigned char *cp, int h)
{
	DBG_FUNC("setblock")

	DBG_ENTER;

	switch (fs->fs_frag) {
	case 8:
		cp[h] = 0xff;
		break;
	case 4:
		cp[h >> 1] |= (0x0f << ((h & 0x1) << 2));
		break;
	case 2:
		cp[h >> 2] |= (0x03 << ((h & 0x3) << 1));
		break;
	case 1:
		cp[h >> 3] |= (0x01 << (h & 0x7));
		break;
	default:
		warnx("setblock bad fs_frag %d", fs->fs_frag);
		break;
	}

	DBG_LEAVE;
	return;
}

/* ************************************************************ ginode ***** */
/*
 * This function provides access to an individual inode. We find out in which
 * block  the  requested inode is located, read it from disk if  needed,  and
 * return  the pointer into that block. We maintain a cache of one  block  to
 * not  read the same block again and again if we iterate linearly  over  all
 * inodes.
 */
static struct dinode *
ginode(ino_t inumber, int fsi, int cg)
{
	DBG_FUNC("ginode")
	ufs_daddr_t	iblk;
	static ino_t	startinum=0;	/* first inode in cached block */
	struct dinode	*pi;

	DBG_ENTER;

	pi=(struct dinode *)(void *)ablk;
	inumber+=(cg * sblock.fs_ipg);
	if (startinum == 0 || inumber < startinum ||
	    inumber >= startinum + INOPB(&sblock)) {
		/*
		 * The block needed is not cached, so we have to read it from
		 * disk now.
		 */
		iblk = ino_to_fsba(&sblock, inumber);
		in_src=fsbtodb(&sblock, iblk);
		rdfs(in_src, (size_t)sblock.fs_bsize, (void *)&ablk, fsi);
		startinum = (inumber / INOPB(&sblock)) * INOPB(&sblock);
	}

	DBG_LEAVE;
	return (&(pi[inumber % INOPB(&sblock)]));
}

/* ****************************************************** charsperline ***** */
/*
 * Figure out how many lines our current terminal has. For more details again
 * please  see the source of newfs(8), as this function is taken over  almost
 * unchanged.
 */
static int
charsperline(void)
{
	DBG_FUNC("charsperline")
	int	columns;
	char	*cp;
	struct winsize	ws;

	DBG_ENTER;

	columns = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) != -1) {
		columns = ws.ws_col;
	}
	if (columns == 0 && (cp = getenv("COLUMNS"))) {
		columns = atoi(cp);
	}
	if (columns == 0) {
		columns = 80;	/* last resort */
	}

	DBG_LEAVE;
	return columns;
}

/* ************************************************************** main ***** */
/*
 * growfs(8)  is a utility which allows to increase the size of  an  existing
 * ufs filesystem. Currently this can only be done on unmounted file  system.
 * It  recognizes some command line options to specify the new desired  size,
 * and  it does some basic checkings. The old file system size is  determined
 * and  after some more checks like we can really access the new  last  block
 * on the disk etc. we calculate the new parameters for the superblock. After
 * having  done  this we just call growfs() which will do  the  work.  Before
 * we finish the only thing left is to update the disklabel.
 * We still have to provide support for snapshots. Therefore we first have to
 * understand  what data structures are always replicated in the snapshot  on
 * creation,  for all other blocks we touch during our procedure, we have  to
 * keep the old blocks unchanged somewhere available for the snapshots. If we
 * are lucky, then we only have to handle our blocks to be relocated in  that
 * way.
 * Also  we  have to consider in what order we actually update  the  critical
 * data structures of the filesystem to make sure, that in case of a disaster
 * fsck(8) is still able to restore any lost data.
 * The  foreseen last step then will be to provide for growing  even  mounted
 * file  systems. There we have to extend the mount() system call to  provide
 * userland access to the file system locking facility.
 */
int
main(int argc, char **argv)
{
	DBG_FUNC("main")
	char	*device, *special, *cp;
	char	ch;
	unsigned int	size=0;
	size_t	len;
	unsigned int	Nflag=0;
	int	ExpertFlag=0;
	struct stat	st;
	struct disklabel	*lp;
	struct partition	*pp;
	int	fsi,fso;
	char	reply[5];
#ifdef FSMAXSNAP
	int	j;
#endif /* FSMAXSNAP */

	DBG_ENTER;

	while((ch=getopt(argc, argv, "Ns:vy")) != -1) {
		switch(ch) {
		case 'N':
			Nflag=1;
			break;
		case 's':
			size=(size_t)atol(optarg);
			if(size<1) {
				usage();
			}
			break;
		case 'v': /* for compatibility to newfs */
			break;
		case 'y':
			ExpertFlag=1;
			break;
		case '?':
			/* FALLTHROUGH */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if(argc != 1) {
		usage();
	}
	device=*argv;

	/*
	 * Now try to guess the (raw)device name.
	 */
	if (0 == strrchr(device, '/')) {
		/*
		 * No path prefix was given, so try in that order:
		 *     /dev/r%s
		 *     /dev/%s
		 *     /dev/vinum/r%s
		 *     /dev/vinum/%s.
		 * 
		 * FreeBSD now doesn't distinguish between raw and  block
		 * devices any longer, but it should still work this way.
		 */
		len=strlen(device)+strlen(_PATH_DEV)+2+strlen("vinum/");
		special=(char *)malloc(len);
		if(special == NULL) {
			errx(1, "malloc failed");
		}
		snprintf(special, len, "%sr%s", _PATH_DEV, device);
		if (stat(special, &st) == -1) {
			snprintf(special, len, "%s%s", _PATH_DEV, device);
			if (stat(special, &st) == -1) {
				snprintf(special, len, "%svinum/r%s",
				    _PATH_DEV, device);
				if (stat(special, &st) == -1) {
					/* For now this is the 'last resort' */
					snprintf(special, len, "%svinum/%s",
					    _PATH_DEV, device);
				}
			}
		}
		device = special;
	}

	/*
	 * Try to access our devices for writing ...
	 */
	if (Nflag) {
		fso = -1;
	} else {
		fso = open(device, O_WRONLY);
		if (fso < 0) {
			err(1, "%s", device);
		}
	}

	/*
	 * ... and reading.
	 */
	fsi = open(device, O_RDONLY);
	if (fsi < 0) {
		err(1, "%s", device);
	}

	/*
	 * Try  to read a label and gess the slice if not  specified.  This
	 * code  should guess the right thing and avaid to bother the  user
	 * user with the task of specifying the option -v on vinum volumes.
	 */
	cp=device+strlen(device)-1;
	lp = get_disklabel(fsi);
	if(lp->d_type == DTYPE_VINUM) {
		pp = &lp->d_partitions[0];
	} else if (isdigit(*cp)) {
		pp = &lp->d_partitions[2];
	} else if (*cp>='a' && *cp<='h') {
		pp = &lp->d_partitions[*cp - 'a'];
	} else {
		errx(1, "unknown device");
	}

	/*
	 * Check if that partition looks suited for growing a file system.
	 */
	if (pp->p_size < 1) {
		errx(1, "partition is unavailable");
	}
	if (pp->p_fstype != FS_BSDFFS) {
		errx(1, "partition not 4.2BSD");
	}

	/*
	 * Read the current superblock, and take a backup.
	 */
	rdfs((daddr_t)(SBOFF/DEV_BSIZE), (size_t)SBSIZE, (void *)&(osblock),
	    fsi);
	if (osblock.fs_magic != FS_MAGIC) {
		errx(1, "superblock not recognized");
	}
	memcpy((void *)&fsun1, (void *)&fsun2, sizeof(fsun2));

	DBG_OPEN("/tmp/growfs.debug"); /* already here we need a superblock */
	DBG_DUMP_FS(&sblock,
	    "old sblock");

	/*
	 * Determine size to grow to. Default to the full size specified in
	 * the disk label.
	 */
	sblock.fs_size = dbtofsb(&osblock, pp->p_size);
	if (size != 0) {
		if (size > pp->p_size){
			errx(1, "There is not enough space (%d < %d)",
			    pp->p_size, size);
		}
		sblock.fs_size = dbtofsb(&osblock, size);	
	}

	/*
	 * Are we really growing ?
	 */
	if(osblock.fs_size >= sblock.fs_size) {
		errx(1, "we are not growing (%d->%d)", osblock.fs_size,
		    sblock.fs_size);
	}


#ifdef FSMAXSNAP
	/*
	 * Check if we find an active snapshot.
	 */
	if(ExpertFlag == 0) {
		for(j=0; j<FSMAXSNAP; j++) {
			if(sblock.fs_snapinum[j]) {
				errx(1, "active snapshot found in filesystem\n"
				    "	please remove all snapshots before "
				    "using growfs\n");
			}
			if(!sblock.fs_snapinum[j]) { /* list is dense */
				break;
			}
		}
	}
#endif

	if (ExpertFlag == 0 && Nflag == 0) {
		printf("We strongly recommend you to make a backup "
		    "before growing the Filesystem\n\n"
		    " Did you backup your data (Yes/No) ? ");
		fgets(reply, (int)sizeof(reply), stdin);
		if (strcmp(reply, "Yes\n")){
			printf("\n Nothing done \n");
			exit (0);
		}		
	}

	printf("new filesystemsize is: %d frags\n", sblock.fs_size);

	/*
	 * Try to access our new last block in the filesystem. Even if we
	 * later on realize we have to abort our operation, on that block
	 * there should be no data, so we can't destroy something yet.
	 */
	wtfs((daddr_t)pp->p_size-1, (size_t)DEV_BSIZE, (void *)&sblock, fso,
	    Nflag);

	/*
	 * Now calculate new superblock values and check for reasonable
	 * bound for new file system size:
	 *     fs_size:    is derived from label or user input
	 *     fs_dsize:   should get updated in the routines creating or
	 *                 updating the cylinder groups on the fly
	 *     fs_cstotal: should get updated in the routines creating or
	 *                 updating the cylinder groups
	 */

	/*
	 * Update the number of cylinders in the filesystem.
	 */
	sblock.fs_ncyl = sblock.fs_size * NSPF(&sblock) / sblock.fs_spc;
	if (sblock.fs_size * NSPF(&sblock) > sblock.fs_ncyl * sblock.fs_spc) {
		sblock.fs_ncyl++;
	}

	/*
	 * Update the number of cylinder groups in the filesystem.
	 */
	sblock.fs_ncg = sblock.fs_ncyl / sblock.fs_cpg;
	if (sblock.fs_ncyl % sblock.fs_cpg) {
		sblock.fs_ncg++;
	}

	if ((sblock.fs_size - (sblock.fs_ncg-1) * sblock.fs_fpg) <
	    sblock.fs_fpg && cgdmin(&sblock, (sblock.fs_ncg-1))-
	    cgbase(&sblock, (sblock.fs_ncg-1)) > (sblock.fs_size -
	    (sblock.fs_ncg-1) * sblock.fs_fpg )) {
		/*
		 * The space in the new last cylinder group is too small,
		 * so revert back.
		 */
		sblock.fs_ncg--;
#if 1 /* this is a bit more safe */
		sblock.fs_ncyl = sblock.fs_ncg * sblock.fs_cpg;
#else
		sblock.fs_ncyl -= sblock.fs_ncyl % sblock.fs_cpg;
#endif
		sblock.fs_ncyl -= sblock.fs_ncyl % sblock.fs_cpg;
		printf( "Warning: %d sector(s) cannot be allocated.\n",
		    (sblock.fs_size-(sblock.fs_ncg)*sblock.fs_fpg) *
		    NSPF(&sblock));
		sblock.fs_size = sblock.fs_ncyl * sblock.fs_spc / NSPF(&sblock);
	}

	/*
	 * Update the space for the cylinder group summary information in the
	 * respective cylinder group data area.
	 */
	sblock.fs_cssize =
	    fragroundup(&sblock, sblock.fs_ncg * sizeof(struct csum));
	
	if(osblock.fs_size >= sblock.fs_size) {
		errx(1, "not enough new space");
	}

	DBG_PRINT0("sblock calculated\n");

	/*
	 * Ok, everything prepared, so now let's do the tricks.
	 */
	growfs(fsi, fso, Nflag);

	/*
	 * Update the disk label.
	 */
	pp->p_fsize = sblock.fs_fsize;
	pp->p_frag = sblock.fs_frag;
	pp->p_cpg = sblock.fs_cpg;

	return_disklabel(fso, lp, Nflag);
	DBG_PRINT0("label rewritten\n");

	close(fsi);
	if(fso>-1) close(fso);

	DBG_CLOSE;

	DBG_LEAVE;
	return 0;
}

/* ************************************************** return_disklabel ***** */
/*
 * Write the updated disklabel back to disk.
 */
static void
return_disklabel(int fd, struct disklabel *lp, unsigned int Nflag)
{
	DBG_FUNC("return_disklabel")
	u_short	sum;
	u_short	*ptr;

	DBG_ENTER;

	if(!lp) {
		DBG_LEAVE;
		return;
	}
	if(!Nflag) {
		lp->d_checksum=0;
		sum = 0;
		ptr=(u_short *)lp;

		/*
		 * recalculate checksum
		 */
		while(ptr < (u_short *)&lp->d_partitions[lp->d_npartitions]) {
			sum ^= *ptr++;
		}
		lp->d_checksum=sum;

		if (ioctl(fd, DIOCWDINFO, (char *)lp) < 0) {
			errx(1, "DIOCWDINFO failed");
		}
	}
	free(lp);

	DBG_LEAVE;
	return ;
}

/* ***************************************************** get_disklabel ***** */
/*
 * Read the disklabel from disk.
 */
static struct disklabel *
get_disklabel(int fd)
{
	DBG_FUNC("get_disklabel")
	static struct	disklabel *lab;

	DBG_ENTER;

	lab=(struct disklabel *)malloc(sizeof(struct disklabel));
	if (!lab) {
		errx(1, "malloc failed");
	}
	if (ioctl(fd, DIOCGDINFO, (char *)lab) < 0) {
		errx(1, "DIOCGDINFO failed");
	}

	DBG_LEAVE;
	return (lab);
}


/* ************************************************************* usage ***** */
/*
 * Dump a line of usage.
 */
static void
usage(void)
{	
	DBG_FUNC("usage")

	DBG_ENTER;

	fprintf(stderr, "usage: growfs [-Ny] [-s size] special\n");

	DBG_LEAVE;
	exit(1);
}

/* *********************************************************** updclst ***** */
/*
 * This updates most paramters and the bitmap related to cluster. We have to
 * assume, that sblock, osblock, acg are set up.
 */
static void
updclst(int block)
{	
	DBG_FUNC("updclst")
	static int	lcs=0;

	DBG_ENTER;

	if(sblock.fs_contigsumsize < 1) { /* no clustering */
		return;
	}
	/*
	 * update cluster allocation map
	 */
	setbit(cg_clustersfree(&acg), block);

	/*
	 * update cluster summary table
	 */
	if(!lcs) {
		/*
		 * calculate size for the trailing cluster
		 */
		for(block--; lcs<sblock.fs_contigsumsize; block--, lcs++ ) {
			if(isclr(cg_clustersfree(&acg), block)){
				break;
			}
		}
	} 
	if(lcs < sblock.fs_contigsumsize) {
		if(lcs) {
			cg_clustersum(&acg)[lcs]--;
		}
		lcs++;
		cg_clustersum(&acg)[lcs]++;
	}

	DBG_LEAVE;
	return;
}

/* *********************************************************** updrefs ***** */
/*
 * This updates all references to relocated blocks for the given inode.  The
 * inode is given as number within the cylinder group, and the number of the
 * cylinder group.
 */
static void
updrefs(int cg, ino_t in, struct gfs_bpp *bp, int fsi, int fso, unsigned int
    Nflag)
{	
	DBG_FUNC("updrefs")
	unsigned int	ictr, ind2ctr, ind3ctr;
	ufs_daddr_t	*iptr, *ind2ptr, *ind3ptr;
	struct dinode	*ino;
	int	remaining_blocks;

	DBG_ENTER;

	/*
	 * XXX We should skip unused inodes even from beeing read from disk
	 *     here by using the bitmap.
	 */
	ino=ginode(in, fsi, cg);
	if(!((ino->di_mode & IFMT)==IFDIR || (ino->di_mode & IFMT)==IFREG ||
	    (ino->di_mode & IFMT)==IFLNK)) {
		DBG_LEAVE;
		return; /* only check DIR, FILE, LINK */
	}
	if(((ino->di_mode & IFMT)==IFLNK) && (ino->di_size<MAXSYMLINKLEN)) {
		DBG_LEAVE;
		return;	/* skip short symlinks */
	}
	if(!ino->di_size) {
		DBG_LEAVE;
		return;	/* skip empty file */
	}
	if(!ino->di_blocks) {
		DBG_LEAVE;
		return;	/* skip empty swiss cheesy file or old fastlink */
	}
	DBG_PRINT2("scg checking inode (%d in %d)\n",
	    in,
	    cg);

	/*
	 * Start checking all direct blocks.
	 */
	remaining_blocks=howmany(ino->di_size, sblock.fs_bsize);
	for(ictr=0; ictr < MIN(NDADDR, (unsigned int)remaining_blocks);
	    ictr++) {
		iptr=&(ino->di_db[ictr]);
		if(*iptr) {
			cond_bl_upd(iptr, bp, GFS_PS_INODE, fso, Nflag);
		}
	}
	DBG_PRINT0("~~scg direct blocks checked\n");

	remaining_blocks-=NDADDR;
	if(remaining_blocks<0) {
		DBG_LEAVE;
		return;
	}
	if(ino->di_ib[0]) {
		/*
		 * Start checking first indirect block
		 */
		cond_bl_upd(&(ino->di_ib[0]), bp, GFS_PS_INODE, fso, Nflag);
		i1_src=fsbtodb(&sblock, ino->di_ib[0]);
		rdfs(i1_src, (size_t)sblock.fs_bsize, (void *)&i1blk, fsi);
		for(ictr=0; ictr < MIN(howmany(sblock.fs_bsize,
		    sizeof(ufs_daddr_t)), (unsigned int)remaining_blocks);
		    ictr++) {
			iptr=&((ufs_daddr_t *)(void *)&i1blk)[ictr];
			if(*iptr) {
				cond_bl_upd(iptr, bp, GFS_PS_IND_BLK_LVL1,
				    fso, Nflag);
			}
		}
	}
	DBG_PRINT0("scg indirect_1 blocks checked\n");

	remaining_blocks-= howmany(sblock.fs_bsize, sizeof(ufs_daddr_t));
	if(remaining_blocks<0) {
		DBG_LEAVE;
		return;
	}
	if(ino->di_ib[1]) {
		/*
		 * Start checking second indirect block
		 */
		cond_bl_upd(&(ino->di_ib[1]), bp, GFS_PS_INODE, fso, Nflag);
		i2_src=fsbtodb(&sblock, ino->di_ib[1]);
		rdfs(i2_src, (size_t)sblock.fs_bsize, (void *)&i2blk, fsi);
		for(ind2ctr=0; ind2ctr < howmany(sblock.fs_bsize,
		    sizeof(ufs_daddr_t)); ind2ctr++) {
			ind2ptr=&((ufs_daddr_t *)(void *)&i2blk)[ind2ctr];
			if(!*ind2ptr) {
				continue;
			}
			cond_bl_upd(ind2ptr, bp, GFS_PS_IND_BLK_LVL2, fso,
			    Nflag);
			i1_src=fsbtodb(&sblock, *ind2ptr);
			rdfs(i1_src, (size_t)sblock.fs_bsize, (void *)&i1blk,
			    fsi);
			for(ictr=0; ictr<MIN(howmany((unsigned int)
			    sblock.fs_bsize, sizeof(ufs_daddr_t)),
			    (unsigned int)remaining_blocks); ictr++) {
				iptr=&((ufs_daddr_t *)(void *)&i1blk)[ictr];
				if(*iptr) {
					cond_bl_upd(iptr, bp,
					    GFS_PS_IND_BLK_LVL1, fso, Nflag);
				}
			}
		}
	}
	DBG_PRINT0("scg indirect_2 blocks checked\n");

#define SQUARE(a) ((a)*(a))
	remaining_blocks-=SQUARE(howmany(sblock.fs_bsize, sizeof(ufs_daddr_t)));
#undef SQUARE
	if(remaining_blocks<0) {
		DBG_LEAVE;
		return;
	}
			
	if(ino->di_ib[2]) {
		/*
		 * Start checking third indirect block
		 */
		cond_bl_upd(&(ino->di_ib[2]), bp, GFS_PS_INODE, fso, Nflag);
		i3_src=fsbtodb(&sblock, ino->di_ib[2]);
		rdfs(i3_src, (size_t)sblock.fs_bsize, (void *)&i3blk, fsi);
		for(ind3ctr=0; ind3ctr < howmany(sblock.fs_bsize,
		    sizeof(ufs_daddr_t)); ind3ctr ++) {
			ind3ptr=&((ufs_daddr_t *)(void *)&i3blk)[ind3ctr];
			if(!*ind3ptr) {
				continue;
			}
			cond_bl_upd(ind3ptr, bp, GFS_PS_IND_BLK_LVL3, fso,
			    Nflag);
			i2_src=fsbtodb(&sblock, *ind3ptr);
			rdfs(i2_src, (size_t)sblock.fs_bsize, (void *)&i2blk,
			    fsi);
			for(ind2ctr=0; ind2ctr < howmany(sblock.fs_bsize,
			    sizeof(ufs_daddr_t)); ind2ctr ++) {
				ind2ptr=&((ufs_daddr_t *)(void *)&i2blk)
				    [ind2ctr];
				if(!*ind2ptr) {
					continue;
				}
				cond_bl_upd(ind2ptr, bp, GFS_PS_IND_BLK_LVL2,
				    fso, Nflag);
				i1_src=fsbtodb(&sblock, *ind2ptr);
				rdfs(i1_src, (size_t)sblock.fs_bsize,
				    (void *)&i1blk, fsi);
				for(ictr=0; ictr < MIN(howmany(sblock.fs_bsize,
				    sizeof(ufs_daddr_t)),
				    (unsigned int)remaining_blocks); ictr++) {
					iptr=&((ufs_daddr_t *)(void *)&i1blk)
					    [ictr];
					if(*iptr) {
						cond_bl_upd(iptr, bp,
						    GFS_PS_IND_BLK_LVL1, fso,
						    Nflag);
					}
				}
			}
		}
	}

	DBG_PRINT0("scg indirect_3 blocks checked\n");

	DBG_LEAVE;
	return;
}

