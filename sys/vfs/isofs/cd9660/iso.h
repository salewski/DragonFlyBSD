/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)iso.h	8.6 (Berkeley) 5/10/95
 * $FreeBSD: src/sys/isofs/cd9660/iso.h,v 1.19.2.1 2000/07/08 14:35:56 bp Exp $
 * $DragonFly: src/sys/vfs/isofs/cd9660/iso.h,v 1.5 2004/08/17 18:57:33 dillon Exp $
 */

#define ISODCL(from, to) (to - from + 1)

struct iso_volume_descriptor {
	char type[ISODCL(1,1)]; /* 711 */
	char id[ISODCL(2,6)];
	char version[ISODCL(7,7)];
	char unused[ISODCL(8,8)];
	char type_sierra[ISODCL(9,9)]; /* 711 */
	char id_sierra[ISODCL(10,14)];
	char version_sierra[ISODCL(15,15)];
	char data[ISODCL(16,2048)];
};

/* volume descriptor types */
#define ISO_VD_PRIMARY 1
#define ISO_VD_SUPPLEMENTARY 2
#define ISO_VD_END 255

#define ISO_STANDARD_ID "CD001"
#define ISO_ECMA_ID	"CDW01"

#define ISO_SIERRA_ID	"CDROM"

struct iso_primary_descriptor {
	char type			[ISODCL (  1,	1)]; /* 711 */
	char id				[ISODCL (  2,	6)];
	char version			[ISODCL (  7,	7)]; /* 711 */
	char unused1			[ISODCL (  8,	8)];
	char system_id			[ISODCL (  9,  40)]; /* achars */
	char volume_id			[ISODCL ( 41,  72)]; /* dchars */
	char unused2			[ISODCL ( 73,  80)];
	char volume_space_size		[ISODCL ( 81,  88)]; /* 733 */
	char unused3			[ISODCL ( 89, 120)];
	char volume_set_size		[ISODCL (121, 124)]; /* 723 */
	char volume_sequence_number	[ISODCL (125, 128)]; /* 723 */
	char logical_block_size		[ISODCL (129, 132)]; /* 723 */
	char path_table_size		[ISODCL (133, 140)]; /* 733 */
	char type_l_path_table		[ISODCL (141, 144)]; /* 731 */
	char opt_type_l_path_table	[ISODCL (145, 148)]; /* 731 */
	char type_m_path_table		[ISODCL (149, 152)]; /* 732 */
	char opt_type_m_path_table	[ISODCL (153, 156)]; /* 732 */
	char root_directory_record	[ISODCL (157, 190)]; /* 9.1 */
	char volume_set_id		[ISODCL (191, 318)]; /* dchars */
	char publisher_id		[ISODCL (319, 446)]; /* achars */
	char preparer_id		[ISODCL (447, 574)]; /* achars */
	char application_id		[ISODCL (575, 702)]; /* achars */
	char copyright_file_id		[ISODCL (703, 739)]; /* 7.5 dchars */
	char abstract_file_id		[ISODCL (740, 776)]; /* 7.5 dchars */
	char bibliographic_file_id	[ISODCL (777, 813)]; /* 7.5 dchars */
	char creation_date		[ISODCL (814, 830)]; /* 8.4.26.1 */
	char modification_date		[ISODCL (831, 847)]; /* 8.4.26.1 */
	char expiration_date		[ISODCL (848, 864)]; /* 8.4.26.1 */
	char effective_date		[ISODCL (865, 881)]; /* 8.4.26.1 */
	char file_structure_version	[ISODCL (882, 882)]; /* 711 */
	char unused4			[ISODCL (883, 883)];
	char application_data		[ISODCL (884, 1395)];
	char unused5			[ISODCL (1396, 2048)];
};
#define ISO_DEFAULT_BLOCK_SIZE		2048

/*
 * Used by Microsoft Joliet extension to ISO9660. Almost the same
 * as PVD, but byte position 8 is a flag, and 89-120 is for escape.
 */

struct iso_supplementary_descriptor {
      char type                       [ISODCL (  1,   1)]; /* 711 */
      char id                         [ISODCL (  2,   6)];
      char version                    [ISODCL (  7,   7)]; /* 711 */
      char flags                      [ISODCL (  8,   8)]; /* 711? */
      char system_id                  [ISODCL (  9,  40)]; /* achars */
      char volume_id                  [ISODCL ( 41,  72)]; /* dchars */
      char unused2                    [ISODCL ( 73,  80)];
      char volume_space_size          [ISODCL ( 81,  88)]; /* 733 */
      char escape                     [ISODCL ( 89, 120)];
      char volume_set_size            [ISODCL (121, 124)]; /* 723 */
      char volume_sequence_number     [ISODCL (125, 128)]; /* 723 */
      char logical_block_size         [ISODCL (129, 132)]; /* 723 */
      char path_table_size            [ISODCL (133, 140)]; /* 733 */
      char type_l_path_table          [ISODCL (141, 144)]; /* 731 */
      char opt_type_l_path_table      [ISODCL (145, 148)]; /* 731 */
      char type_m_path_table          [ISODCL (149, 152)]; /* 732 */
      char opt_type_m_path_table      [ISODCL (153, 156)]; /* 732 */
      char root_directory_record      [ISODCL (157, 190)]; /* 9.1 */
      char volume_set_id              [ISODCL (191, 318)]; /* dchars */
      char publisher_id               [ISODCL (319, 446)]; /* achars */
      char preparer_id                [ISODCL (447, 574)]; /* achars */
      char application_id             [ISODCL (575, 702)]; /* achars */
      char copyright_file_id          [ISODCL (703, 739)]; /* 7.5 dchars */
      char abstract_file_id           [ISODCL (740, 776)]; /* 7.5 dchars */
      char bibliographic_file_id      [ISODCL (777, 813)]; /* 7.5 dchars */
      char creation_date              [ISODCL (814, 830)]; /* 8.4.26.1 */
      char modification_date          [ISODCL (831, 847)]; /* 8.4.26.1 */
      char expiration_date            [ISODCL (848, 864)]; /* 8.4.26.1 */
      char effective_date             [ISODCL (865, 881)]; /* 8.4.26.1 */
      char file_structure_version     [ISODCL (882, 882)]; /* 711 */
      char unused4                    [ISODCL (883, 883)];
      char application_data           [ISODCL (884, 1395)];
      char unused5                    [ISODCL (1396, 2048)];
};

struct iso_sierra_primary_descriptor {
	char unknown1			[ISODCL (  1,	8)]; /* 733 */
	char type			[ISODCL (  9,	9)]; /* 711 */
	char id				[ISODCL ( 10,  14)];
	char version			[ISODCL ( 15,  15)]; /* 711 */
	char unused1			[ISODCL ( 16,  16)];
	char system_id			[ISODCL ( 17,  48)]; /* achars */
	char volume_id			[ISODCL ( 49,  80)]; /* dchars */
	char unused2			[ISODCL ( 81,  88)];
	char volume_space_size		[ISODCL ( 89,  96)]; /* 733 */
	char unused3			[ISODCL ( 97, 128)];
	char volume_set_size		[ISODCL (129, 132)]; /* 723 */
	char volume_sequence_number	[ISODCL (133, 136)]; /* 723 */
	char logical_block_size		[ISODCL (137, 140)]; /* 723 */
	char path_table_size		[ISODCL (141, 148)]; /* 733 */
	char type_l_path_table		[ISODCL (149, 152)]; /* 731 */
	char opt_type_l_path_table	[ISODCL (153, 156)]; /* 731 */
	char unknown2			[ISODCL (157, 160)]; /* 731 */
	char unknown3			[ISODCL (161, 164)]; /* 731 */
	char type_m_path_table		[ISODCL (165, 168)]; /* 732 */
	char opt_type_m_path_table	[ISODCL (169, 172)]; /* 732 */
	char unknown4			[ISODCL (173, 176)]; /* 732 */
	char unknown5			[ISODCL (177, 180)]; /* 732 */
	char root_directory_record	[ISODCL (181, 214)]; /* 9.1 */
	char volume_set_id		[ISODCL (215, 342)]; /* dchars */
	char publisher_id		[ISODCL (343, 470)]; /* achars */
	char preparer_id		[ISODCL (471, 598)]; /* achars */
	char application_id		[ISODCL (599, 726)]; /* achars */
	char copyright_id		[ISODCL (727, 790)]; /* achars */
	char creation_date		[ISODCL (791, 806)]; /* ? */
	char modification_date		[ISODCL (807, 822)]; /* ? */
	char expiration_date		[ISODCL (823, 838)]; /* ? */
	char effective_date		[ISODCL (839, 854)]; /* ? */
	char file_structure_version	[ISODCL (855, 855)]; /* 711 */
	char unused4			[ISODCL (856, 2048)];
};

struct iso_directory_record {
	char length			[ISODCL (1, 1)]; /* 711 */
	char ext_attr_length		[ISODCL (2, 2)]; /* 711 */
	u_char extent			[ISODCL (3, 10)]; /* 733 */
	u_char size			[ISODCL (11, 18)]; /* 733 */
	char date			[ISODCL (19, 25)]; /* 7 by 711 */
	char flags			[ISODCL (26, 26)];
	char file_unit_size		[ISODCL (27, 27)]; /* 711 */
	char interleave			[ISODCL (28, 28)]; /* 711 */
	char volume_sequence_number	[ISODCL (29, 32)]; /* 723 */
	char name_len			[ISODCL (33, 33)]; /* 711 */
	char name			[1];			/* XXX */
};
/* can't take sizeof(iso_directory_record), because of possible alignment
   of the last entry (34 instead of 33) */
#define ISO_DIRECTORY_RECORD_SIZE	33

struct iso_extended_attributes {
	u_char owner			[ISODCL (1, 4)]; /* 723 */
	u_char group			[ISODCL (5, 8)]; /* 723 */
	u_char perm			[ISODCL (9, 10)]; /* 9.5.3 */
	char ctime			[ISODCL (11, 27)]; /* 8.4.26.1 */
	char mtime			[ISODCL (28, 44)]; /* 8.4.26.1 */
	char xtime			[ISODCL (45, 61)]; /* 8.4.26.1 */
	char ftime			[ISODCL (62, 78)]; /* 8.4.26.1 */
	char recfmt			[ISODCL (79, 79)]; /* 711 */
	char recattr			[ISODCL (80, 80)]; /* 711 */
	u_char reclen			[ISODCL (81, 84)]; /* 723 */
	char system_id			[ISODCL (85, 116)]; /* achars */
	char system_use			[ISODCL (117, 180)];
	char version			[ISODCL (181, 181)]; /* 711 */
	char len_esc			[ISODCL (182, 182)]; /* 711 */
	char reserved			[ISODCL (183, 246)];
	u_char len_au			[ISODCL (247, 250)]; /* 723 */
};

#ifdef _KERNEL

/* CD-ROM Format type */
enum ISO_FTYPE	{ ISO_FTYPE_DEFAULT, ISO_FTYPE_9660, ISO_FTYPE_RRIP,
		  ISO_FTYPE_JOLIET, ISO_FTYPE_ECMA, ISO_FTYPE_HIGH_SIERRA };

#ifndef	ISOFSMNT_ROOT
#define	ISOFSMNT_ROOT	0
#endif

struct iso_mnt {
	int im_flags;

	struct mount *im_mountp;
	dev_t im_dev;
	struct vnode *im_devvp;

	int logical_block_size;
	int im_bshift;
	int im_bmask;

	int volume_space_size;
	struct netexport im_export;

	char root[ISODCL (157, 190)];
	int root_extent;
	int root_size;
	enum ISO_FTYPE	iso_ftype;

	int rr_skip;
	int rr_skip0;

	int joliet_level;
};

#define VFSTOISOFS(mp)	((struct iso_mnt *)((mp)->mnt_data))

#define blkoff(imp, loc)	((loc) & (imp)->im_bmask)
#define lblktosize(imp, blk)	((blk) << (imp)->im_bshift)
#define lblkno(imp, loc)	((loc) >> (imp)->im_bshift)
#define blksize(imp, ip, lbn)	((imp)->logical_block_size)

int cd9660_vget_internal (struct mount *, ino_t, struct vnode **, int,
			      struct iso_directory_record *);
int cd9660_init (struct vfsconf *);
int cd9660_uninit (struct vfsconf *);
#define cd9660_sysctl ((int (*) (int *, u_int, void *, size_t *, void *, \
                                    size_t, struct proc *))eopnotsupp)

int isochar (u_char *, u_char *, int, u_char *);
int isofncmp (u_char *, int, u_char *, int, int);
void isofntrans (u_char *, int, u_char *, u_short *, int, int, int);
ino_t isodirino (struct iso_directory_record *, struct iso_mnt *);

#endif /* _KERNEL */

/*
 * The isonum_xxx functions are inlined anyway, and could come handy even
 * outside the kernel.  Thus we don't hide them here.
 */

static __inline int isonum_711 (u_char *);
static __inline int
isonum_711(p)
	u_char *p;
{
	return *p;
}

static __inline int isonum_712 (char *);
static __inline int
isonum_712(p)
	char *p;
{
	return *p;
}

#ifndef UNALIGNED_ACCESS

static __inline int isonum_723 (u_char *);
static __inline int
isonum_723(p)
	u_char *p;
{
	return *p|(p[1] << 8);
}

static __inline int isonum_733 (u_char *);
static __inline int
isonum_733(p)
	u_char *p;
{
	return *p|(p[1] << 8)|(p[2] << 16)|(p[3] << 24);
}

#else /* UNALIGNED_ACCESS */

#if BYTE_ORDER == LITTLE_ENDIAN

static __inline int
isonum_723(p)
	u_char *p
{
	return *(u_int16t *)p;
}

static __inline int
isonum_733(p)
	u_char *p;
{
	return *(u_int32t *)p;
}

#endif

#if BYTE_ORDER == BIG_ENDIAN

static __inline int
isonum_723(p)
	u_char *p
{
	return *(u_int16t *)(p + 2);
}

static __inline int
isonum_733(p)
	u_char *p;
{
	return *(u_int32t *)(p + 4);
}

#endif

#endif /* UNALIGNED_ACCESS */

/*
 * Associated files have a leading '='.
 */
#define	ASSOCCHAR	'='
