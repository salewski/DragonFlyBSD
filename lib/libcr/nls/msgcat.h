/* $FreeBSD: src/lib/libc/nls/msgcat.h,v 1.6.2.1 2000/09/07 16:46:33 ache Exp $ */
/* $DragonFly: src/lib/libcr/nls/Attic/msgcat.h,v 1.2 2003/06/17 04:26:44 dillon Exp $ */

#ifndef _MSGCAT_H_
#define _MSGCAT_H_


/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/


#include <sys/types.h>

/*
 * On disk data structures
 */

/* For or'd constants */
#define MCMakeId(s,m)		(unsigned long) ( ((unsigned short)s << (sizeof(short)*8)) \
						 | (unsigned short)m )
#define MCSetId(id)		(unsigned int) ( id >> (sizeof(short) * 8) )
#define MCMsgId(id)		(unsigned int) ( (id << (sizeof(short) * 8)) \
						>> (sizeof(short) * 8) )
#define MCMagicLen	8
#define MCMagic		"*nazgul*"
#define MCLastMsg	0
#define MCLastSet	0

#define MCMajorVer	1L
#define MCMinorVer	0

/*
 * Critical note here.  Sets and Messages *MUST* be stored in ascending
 * order.  There are stored that way (by specification) in the original
 * data file, however in the process of merging in new stuff you might
 * mix that up.  Don't!  The catget stuff does a binary search and will
 * totally lose it if these aren't in order (not contiguous mind you, just
 * in order.  If this turns out to be a major problem this could be enhanced
 * by adding a 'sorted' flag to the db, and sorting msgs and sets at load
 * time if things aren't sorted, but I'd like not to have to do that.
 */

/*
 * I have tried here to define data structures which can be used
 * while the catalog is on disk, and at runtime.
 * This is rather dangerous of course, but I think it can be done without
 * overly increasing the memory usage, and it makes loading and storing
 * somewhat simpler and less prone to accidents.  I have also tried to
 * define on disk data structures which can be updated in place, so that
 * with a very large catalog (e.g. all system errors) you don't have to
 * load everything in memory in order to add or update one set.  With
 * this in mind there are "invalid" flags which allow items to be
 * invalidated and thus not loaded at runtime.  Note however that although
 * I pay attention to these when I load the DB, I do not currently use
 * them in gencat (it just reads everything into memory), so there is
 * no guarantee that this will all work.
 */

/* These should be publicly available */

#define MCLoadBySet	0	/* Load entire sets as they are used */
#define MCLoadAll	1	/* Load entire DB on catopen */

/*
 * MCOffsetT - Union to handle both disk and runtime pointers
 */
typedef union {
    off_t	off;
    char	*str;
    void	*ptr;
    struct _MCMsgT	*msg;
    struct _MCSetT	*set;
} MCOffsetT;

/*
 * MCMsgT - Message structure (disk and runtime)
 */
typedef struct _MCMsgT {
    long	msgId;		/* Id of this message */
    MCOffsetT	msg;		/* Relative offset on disk or pointer in memory */
    long	invalid;	/* Valid on disk, loaded in memory */
} MCMsgT;

/*
 * MCSetT - Set structure (disk and runtime)
 */
typedef struct _MCSetT {
    long	setId;		/* Id of this set */
    off_t	nextSet;	/* Offset of next set on disk */
    union {
	off_t	firstMsg;	/* Offset to first Msg (while on disk) */
	MCMsgT	*msgs;		/* Pointer to array of msgs (in mem, loaded) */
    } u;
    MCOffsetT	data;		/* Offset to data, or pointer to data */
    long	dataLen;	/* Length of data area on disk */
    long	numMsgs;	/* Number of messages */
    long	invalid;	/* Valid on disk, loaded in memory */
} MCSetT;

/*
 * MCCatT - Runtime catalog pointer
 */
typedef struct {
    long	loadType;	/* How to load the messages (see MSLoadType) */
    FILE        *fp;            /* File descriptor of catalog (if load-on-demand) */
    long	numSets;	/* Number of sets */
    MCSetT	*sets;		/* Pointer to the sets */
    off_t	firstSet;	/* Offset of first set on disk */
} MCCatT;

/*
 * MCHeaderT - Disk file header
 */
typedef struct {
    char	magic[MCMagicLen];	/* Magic cookie "*nazgul*" */
    long	majorVer;		/* ++ on incompatible changes */
    long	minorVer;		/* ++ on compatible changes */
    long	flags;			/* Informational flags */
    long	numSets;		/* Number of valid Sets */
    off_t	firstSet;		/* Offset of first set on disk */
} MCHeaderT;

/* Some flags */
#define MC68KByteOrder	0x01
#define MCn86ByteOrder	0x02

#endif /* !_MSGCAT_H_ */
