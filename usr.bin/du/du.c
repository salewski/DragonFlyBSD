/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Newcomb.
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
 * @(#) Copyright (c) 1989, 1993, 1994 The Regents of the University of California.  All rights reserved.
 * @(#)du.c	8.5 (Berkeley) 5/4/95
 * $FreeBSD: src/usr.bin/du/du.c,v 1.17.2.4 2002/12/12 16:29:39 trhodes Exp $
 * $DragonFly: src/usr.bin/du/du.c,v 1.7 2005/01/01 23:02:42 cpressey Exp $
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	KILO_SZ(n) (n)
#define	MEGA_SZ(n) ((n) * (n))
#define	GIGA_SZ(n) ((n) * (n) * (n))
#define	TERA_SZ(n) ((n) * (n) * (n) * (n))
#define	PETA_SZ(n) ((n) * (n) * (n) * (n) * (n))

#define	KILO_2_SZ (KILO_SZ(1024ULL))
#define	MEGA_2_SZ (MEGA_SZ(1024ULL))
#define	GIGA_2_SZ (GIGA_SZ(1024ULL))
#define	TERA_2_SZ (TERA_SZ(1024ULL))
#define	PETA_2_SZ (PETA_SZ(1024ULL))

#define	KILO_SI_SZ (KILO_SZ(1000ULL))
#define	MEGA_SI_SZ (MEGA_SZ(1000ULL))
#define	GIGA_SI_SZ (GIGA_SZ(1000ULL))
#define	TERA_SI_SZ (TERA_SZ(1000ULL))
#define	PETA_SI_SZ (PETA_SZ(1000ULL))

#define	HASHSIZE	256		/* power of 2 only */
#define HASHMASK	(HASHSIZE - 1)

unsigned long long vals_si [] = {1, KILO_SI_SZ, MEGA_SI_SZ, GIGA_SI_SZ, TERA_SI_SZ, PETA_SI_SZ};
unsigned long long vals_base2[] = {1, KILO_2_SZ, MEGA_2_SZ, GIGA_2_SZ, TERA_2_SZ, PETA_2_SZ};
unsigned long long *valp;

typedef enum { NONE, KILO, MEGA, GIGA, TERA, PETA, UNIT_MAX } unit_t;

int unitp [] = { NONE, KILO, MEGA, GIGA, TERA, PETA };

SLIST_HEAD(ignhead, ignentry) ignores;
struct ignentry {
	char			*mask;
	SLIST_ENTRY(ignentry)	next;
};

static int	linkchk(FTSENT *);
static void	usage(void);
void		prthumanval(double);
unit_t		unit_adjust(double *);
void		ignoreadd(const char *);
void		ignoreclean(void);
int		ignorep(FTSENT *);

static char period[] = ".";

int
main(int argc, char **argv)
{
	FTS		*fts;
	FTSENT		*p;
	long		blocksize, savednumber = 0;
	int		ftsoptions;
	int		listall;
	int		depth;
	int		Hflag, Lflag, Pflag, aflag, sflag, dflag, cflag, hflag, ch, notused, rval;
	char 		**save;

	Hflag = Lflag = Pflag = aflag = sflag = dflag = cflag = hflag = 0;
	
	save = argv;
	ftsoptions = 0;
	depth = INT_MAX;
	SLIST_INIT(&ignores);
	
	while ((ch = getopt(argc, argv, "HI:LPasd:chkrx")) != -1)
		switch (ch) {
			case 'H':
				Hflag = 1;
				break;
			case 'I':
				ignoreadd(optarg);
				break;
			case 'L':
				if (Pflag)
					usage();
				Lflag = 1;
				break;
			case 'P':
				if (Lflag)
					usage();
				Pflag = 1;
				break;
			case 'a':
				aflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			case 'd':
				dflag = 1;
				errno = 0;
				depth = atoi(optarg);
				if (errno == ERANGE || depth < 0) {
					warnx("invalid argument to option d: %s", optarg);
					usage();
				}
				break;
			case 'c':
				cflag = 1;
				break;
			case 'h':
				putenv("BLOCKSIZE=512");
				hflag = 1;
				valp = vals_base2;
				break;
			case 'k':
				hflag = 0;
				putenv("BLOCKSIZE=1024");
				break;
			case 'r':		 /* Compatibility. */
				break;
			case 'x':
				ftsoptions |= FTS_XDEV;
				break;
			case '?':
			default:
				usage();
		}

	argc -= optind;
	argv += optind;

	/*
	 * XXX
	 * Because of the way that fts(3) works, logical walks will not count
	 * the blocks actually used by symbolic links.  We rationalize this by
	 * noting that users computing logical sizes are likely to do logical
	 * copies, so not counting the links is correct.  The real reason is
	 * that we'd have to re-implement the kernel's symbolic link traversing
	 * algorithm to get this right.  If, for example, you have relative
	 * symbolic links referencing other relative symbolic links, it gets
	 * very nasty, very fast.  The bottom line is that it's documented in
	 * the man page, so it's a feature.
	 */

	if (Hflag + Lflag + Pflag > 1)
		usage();

	if (Hflag + Lflag + Pflag == 0)
		Pflag = 1;			/* -P (physical) is default */

	if (Hflag)
		ftsoptions |= FTS_COMFOLLOW;

	if (Lflag)
		ftsoptions |= FTS_LOGICAL;

	if (Pflag)
		ftsoptions |= FTS_PHYSICAL;

	listall = 0;

	if (aflag) {
		if (sflag || dflag)
			usage();
		listall = 1;
	} else if (sflag) {
		if (dflag)
			usage();
		depth = 0;
	}

	if (!*argv) {
		argv = save;
		argv[0] = period;
		argv[1] = NULL;
	}

	(void) getbsize(&notused, &blocksize);
	blocksize /= 512;

	rval = 0;
	
	if ((fts = fts_open(argv, ftsoptions, NULL)) == NULL)
		err(1, "fts_open");

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
			case FTS_D:			/* Ignore. */
				if (ignorep(p))
					fts_set(fts, p, FTS_SKIP);
				break;
			case FTS_DP:
				if (ignorep(p))
					break;

				p->fts_parent->fts_number +=
				    p->fts_number += p->fts_statp->st_blocks;
				
				if (p->fts_level <= depth) {
					if (hflag) {
						(void) prthumanval(howmany(p->fts_number, blocksize));
						(void) printf("\t%s\n", p->fts_path);
					} else {
					(void) printf("%ld\t%s\n",
					    howmany(p->fts_number, blocksize),
					    p->fts_path);
					}
				}
				break;
			case FTS_DC:			/* Ignore. */
				break;
			case FTS_DNR:			/* Warn, continue. */
			case FTS_ERR:
			case FTS_NS:
				warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
				rval = 1;
				break;
			default:
				if (ignorep(p))
					break;

				if (p->fts_statp->st_nlink > 1 && linkchk(p))
					break;
				
				if (listall || p->fts_level == 0) {
					if (hflag) {
						(void) prthumanval(howmany(p->fts_statp->st_blocks,
							blocksize));
						(void) printf("\t%s\n", p->fts_path);
					} else {
						(void) printf("%qd\t%s\n",
							howmany(p->fts_statp->st_blocks, blocksize),
							p->fts_path);
					}
				}

				p->fts_parent->fts_number += p->fts_statp->st_blocks;
		}
		savednumber = p->fts_parent->fts_number;
	}

	if (errno)
		err(1, "fts_read");

	if (cflag) {
		if (hflag) {
			(void) prthumanval(howmany(savednumber, blocksize));
			(void) printf("\ttotal\n");
		} else {
			(void) printf("%ld\ttotal\n", howmany(savednumber, blocksize));
		}
	}

	ignoreclean();
	exit(rval);
}

static int
linkchk(FTSENT *p)
{
	struct links_entry {
		struct links_entry *next;
		struct links_entry *previous;
		int		links;
		dev_t		dev;
		ino_t		ino;
	};

	static const size_t links_hash_initial_size = 8192;
	static struct links_entry **buckets;
	static struct links_entry *free_list;
	static size_t number_buckets;
	static unsigned long number_entries;
	static char stop_allocating;
	struct links_entry *le, **new_buckets;
	struct stat *st;
	size_t i, new_size;
	int count, hash;

	st = p->fts_statp;

	/* If necessary, initialize the hash table. */
	if (buckets == NULL) {
		number_buckets = links_hash_initial_size;
		buckets = malloc(number_buckets * sizeof(buckets[0]));
		if (buckets == NULL)
			errx(1, "No memory for hardlink detection");
		for (i = 0; i < number_buckets; i++)
			buckets[i] = NULL;
	}

	/* If the hash table is getting too full, enlarge it. */
	if (number_entries > number_buckets * 10 && !stop_allocating) {
		new_size = number_buckets * 2;
		new_buckets = malloc(new_size * sizeof(struct links_entry *));
		count = 0;

		/* Try releasing the free list to see if that helps. */
		if (new_buckets == NULL && free_list != NULL) {
			while (free_list != NULL) {
				le = free_list;
				free_list = le->next;
				free(le);
			}
			new_buckets = malloc(new_size * sizeof(new_buckets[0]));
		}

		if (new_buckets == NULL) {
			stop_allocating = 1;
			warnx("No more memory for tracking hard links");
		} else {
			memset(new_buckets, 0, new_size * sizeof(struct links_entry *));
			for (i = 0; i < number_buckets; i++) {
				while (buckets[i] != NULL) {
					/* Remove entry from old bucket. */
					le = buckets[i];
					buckets[i] = le->next;
	
					/* Add entry to new bucket. */
					hash = (le->dev ^ le->ino) % new_size;
	
					if (new_buckets[hash] != NULL)
						new_buckets[hash]->previous = le;
					le->next = new_buckets[hash];
					le->previous = NULL;
					new_buckets[hash] = le;
				}
			}
			free(buckets);
			buckets = new_buckets;
			number_buckets = new_size;
		}
	}

	/* Try to locate this entry in the hash table. */
	hash = ( st->st_dev ^ st->st_ino ) % number_buckets;
	for (le = buckets[hash]; le != NULL; le = le->next) {
		if (le->dev == st->st_dev && le->ino == st->st_ino) {
			/*
			 * Save memory by releasing an entry when we've seen
			 * all of it's links.
			 */
			if (--le->links <= 0) {
				if (le->previous != NULL)
					le->previous->next = le->next;
				if (le->next != NULL)
					le->next->previous = le->previous;
				if (buckets[hash] == le)
					buckets[hash] = le->next;
				number_entries--;
				/* Recycle this node through the free list */
				if (stop_allocating) {
					free(le);
				} else {
					le->next = free_list;
					free_list = le;
				}
			}
			return (1);
		}
	}

	if (stop_allocating)
		return (0);

	/* Add this entry to the links cache. */
	if (free_list != NULL) {
		/* Pull a node from the free list if we can. */
		le = free_list;
		free_list = le->next;
	} else
		/* Malloc one if we have to. */
		le = malloc(sizeof(struct links_entry));
	if (le == NULL) {
		stop_allocating = 1;
		warnx("No more memory for tracking hard links");
		return (0);
	}
	le->dev = st->st_dev;
	le->ino = st->st_ino;
	le->links = st->st_nlink - 1;
	number_entries++;
	le->next = buckets[hash];
	le->previous = NULL;
	if (buckets[hash] != NULL)
		buckets[hash]->previous = le;
	buckets[hash] = le;
	return (0);
}

/*
 * Output in "human-readable" format.  Uses 3 digits max and puts
 * unit suffixes at the end.  Makes output compact and easy to read,
 * especially on huge disks.
 *
 */
unit_t
unit_adjust(double *val)
{
	double abval;
	unit_t unit;
	unsigned int unit_sz;

	abval = fabs(*val);

	unit_sz = abval ? ilogb(abval) / 10 : 0;

	if (unit_sz >= UNIT_MAX) {
		unit = NONE;
	} else {
		unit = unitp[unit_sz];
		*val /= (double)valp[unit_sz];
	}

	return (unit);
}

void
prthumanval(double bytes)
{
	unit_t unit;

	bytes *= 512;
	unit = unit_adjust(&bytes);

	if (bytes == 0)
		(void)printf("  0B");
	else if (bytes > 10)
		(void)printf("%3.0f%c", bytes, "BKMGTPE"[unit]);
	else
		(void)printf("%3.1f%c", bytes, "BKMGTPE"[unit]);
}

static void
usage(void)
{
	(void)fprintf(stderr,
		"usage: du [-H | -L | -P] [-a | -s | -d depth] [-c] [-h | -k] [-x] [-I mask] [file ...]\n");
	exit(EX_USAGE);
}

void
ignoreadd(const char *mask)
{
	struct ignentry *ign;

	ign = calloc(1, sizeof(*ign));
	if (ign == NULL)
		errx(1, "cannot allocate memory");
	ign->mask = strdup(mask);
	if (ign->mask == NULL)
		errx(1, "cannot allocate memory");
	SLIST_INSERT_HEAD(&ignores, ign, next);
}

void
ignoreclean(void)
{
	struct ignentry *ign;
	
	while (!SLIST_EMPTY(&ignores)) {
		ign = SLIST_FIRST(&ignores);
		SLIST_REMOVE_HEAD(&ignores, next);
		free(ign->mask);
		free(ign);
	}
}

int
ignorep(FTSENT *ent)
{
	struct ignentry *ign;

	SLIST_FOREACH(ign, &ignores, next)
		if (fnmatch(ign->mask, ent->fts_name, 0) != FNM_NOMATCH)
			return 1;
	return 0;
}
