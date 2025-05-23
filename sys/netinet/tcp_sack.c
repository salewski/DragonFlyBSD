/*
 * Copyright (c) 2003, 2004 Jeffrey M. Hsu.  All rights reserved.
 * Copyright (c) 2003, 2004 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Jeffrey M. Hsu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $DragonFly: src/sys/netinet/tcp_sack.c,v 1.2 2005/03/09 06:54:34 hsu Exp $
 */

/*
 * Copyright (c) 2003, 2004 Jeffrey M. Hsu.  All rights reserved.
 *
 * License terms: all terms for the DragonFly license above plus the following:
 *
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Jeffrey M. Hsu
 *	for the DragonFly Project.
 *
 *    This requirement may be waived with permission from Jeffrey Hsu.
 *    This requirement will sunset and may be removed on November 13, 2005,
 *    after which the standard DragonFly license (as shown above) will
 *    apply.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/thread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>

#include <vm/vm_zone.h>

struct sackblock {
	tcp_seq			sblk_start;
	tcp_seq			sblk_end;
	TAILQ_ENTRY(sackblock)	sblk_list;
};

#define	MAXSAVEDBLOCKS	8			/* per connection limit */

static void insert_block(struct scoreboard *scb, struct sackblock *newblock);
static void update_lostseq(struct scoreboard *scb, tcp_seq snd_una,
			   u_int maxseg);

static vm_zone_t sackblock_zone;

/*
 * Initialize SACK module.
 */
void
tcp_sack_init()
{
	sackblock_zone = zinit("sack blocks", sizeof(struct sackblock),
	    maxsockets, ZONE_INTERRUPT, 0);
}

/*
 * Per-tcpcb initialization.
 */
void
tcp_sack_tcpcb_init(struct tcpcb *tp)
{
	struct scoreboard *scb = &tp->scb;

	scb->nblocks = 0;
	TAILQ_INIT(&scb->sackblocks);
	scb->lastfound = NULL;
}

/*
 * Find the SACK block containing or immediately preceding "seq".
 * The boolean result indicates whether the sequence is actually
 * contained in the SACK block.
 */
static boolean_t
sack_block_lookup(struct scoreboard *scb, tcp_seq seq, struct sackblock **sb)
{
	struct sackblock *hint = scb->lastfound;
	struct sackblock *cur, *last, *prev;

	if (TAILQ_EMPTY(&scb->sackblocks)) {
		*sb = NULL;
		return FALSE;
	}

	if (hint == NULL) {
		/* No hint.  Search from start to end. */
		cur = TAILQ_FIRST(&scb->sackblocks);
		last = NULL;
		prev = TAILQ_LAST(&scb->sackblocks, sackblock_list);
	} else  {
		if (SEQ_GEQ(seq, hint->sblk_start)) {
			/* Search from hint to end of list. */
			cur = hint;
			last = NULL;
			prev = TAILQ_LAST(&scb->sackblocks, sackblock_list);
		} else {
			/* Search from front of list to hint. */
			cur = TAILQ_FIRST(&scb->sackblocks);
			last = hint;
			prev = TAILQ_PREV(hint, sackblock_list, sblk_list);
		}
	}

	do {
		if (SEQ_GT(cur->sblk_end, seq)) {
			if (SEQ_GEQ(seq, cur->sblk_start)) {
				*sb = scb->lastfound = cur;
				return TRUE;
			} else {
				*sb = scb->lastfound =
				    TAILQ_PREV(cur, sackblock_list, sblk_list);
				return FALSE;
			}
		}
		cur = TAILQ_NEXT(cur, sblk_list);
	} while (cur != last);

	*sb = scb->lastfound = prev;
	return FALSE;
}

/*
 * Allocate a SACK block.
 */
static __inline struct sackblock *
alloc_sackblock(void)
{
	return zalloc(sackblock_zone);
}

/*
 * Free a SACK block.
 */
static __inline void
free_sackblock(struct sackblock *s)
{
	zfree(sackblock_zone, s);
}

/*
 * Free up SACK blocks for data that's been acked.
 */
static void
tcp_sack_ack_blocks(struct scoreboard *scb, tcp_seq th_ack)
{
	struct sackblock *sb, *nb;

	sb = TAILQ_FIRST(&scb->sackblocks);
	while (sb && SEQ_LEQ(sb->sblk_end, th_ack)) {
		nb = TAILQ_NEXT(sb, sblk_list);
		if (sb == scb->lastfound)
			scb->lastfound = NULL;
		TAILQ_REMOVE(&scb->sackblocks, sb, sblk_list);
		free_sackblock(sb);
		--scb->nblocks;
		KASSERT(scb->nblocks >= 0,
		    ("SACK block count underflow: %d < 0", scb->nblocks));
		sb = nb;
	}
	if (sb && SEQ_GT(th_ack, sb->sblk_start))
		sb->sblk_start = th_ack;	/* other side reneged? XXX */
}

/*
 * Delete and free SACK blocks saved in scoreboard.
 */
void
tcp_sack_cleanup(struct scoreboard *scb)
{
	struct sackblock *sb, *nb;

	TAILQ_FOREACH_MUTABLE(sb, &scb->sackblocks, sblk_list, nb) {
		free_sackblock(sb);
		--scb->nblocks;
	}
	KASSERT(scb->nblocks == 0,
	    ("SACK block %d count not zero", scb->nblocks));
	TAILQ_INIT(&scb->sackblocks);
	scb->lastfound = NULL;
}

/*
 * Returns	0 if not D-SACK block,
 *		1 if D-SACK,
 *		2 if duplicate of out-of-order D-SACK block.
 */
int
tcp_sack_ndsack_blocks(struct raw_sackblock *blocks, const int numblocks,
		       tcp_seq snd_una)
{
	if (numblocks == 0)
		return 0;

	if (SEQ_LT(blocks[0].rblk_start, snd_una))
		return 1;

	/* block 0 inside block 1 */
	if (numblocks > 1 &&
	    SEQ_GEQ(blocks[0].rblk_start, blocks[1].rblk_start) &&
	    SEQ_LEQ(blocks[0].rblk_end, blocks[1].rblk_end))
		return 2;

	return 0;
}

/*
 * Update scoreboard on new incoming ACK.
 */
static void
tcp_sack_add_blocks(struct tcpcb *tp, struct tcpopt *to)
{
	const int numblocks = to->to_nsackblocks;
	struct raw_sackblock *blocks = to->to_sackblocks;
	struct scoreboard *scb = &tp->scb;
	struct sackblock *sb;
	int startblock;
	int i;

	if (tcp_sack_ndsack_blocks(blocks, numblocks, tp->snd_una) > 0)
		startblock = 1;
	else
		startblock = 0;

	for (i = startblock; i < numblocks; i++) {
		struct raw_sackblock *newsackblock = &blocks[i];

		/* don't accept bad SACK blocks */
		if (SEQ_GT(newsackblock->rblk_end, tp->snd_max))
			break;		/* skip all other blocks */

		sb = alloc_sackblock();
		if (sb == NULL)		/* do some sort of cleanup? XXX */
			break;		/* just skip rest of blocks */
		sb->sblk_start = newsackblock->rblk_start;
		sb->sblk_end = newsackblock->rblk_end;
		if (TAILQ_EMPTY(&scb->sackblocks)) {
			KASSERT(scb->nblocks == 0, ("emply scb w/ blocks"));
			scb->nblocks = 1;
			TAILQ_INSERT_HEAD(&scb->sackblocks, sb, sblk_list);
		} else {
			insert_block(scb, sb);
		}
	}
}

void
tcp_sack_update_scoreboard(struct tcpcb *tp, struct tcpopt *to)
{
	struct scoreboard *scb = &tp->scb;

	tcp_sack_ack_blocks(scb, tp->snd_una);
	tcp_sack_add_blocks(tp, to);
	update_lostseq(scb, tp->snd_una, tp->t_maxseg);
	if (SEQ_LT(tp->rexmt_high, tp->snd_una))
		tp->rexmt_high = tp->snd_una;
}

/*
 * Insert SACK block into sender's scoreboard.
 */
static void
insert_block(struct scoreboard *scb, struct sackblock *newblock)
{
	struct sackblock *sb, *workingblock;
	boolean_t overlap_front;

	KASSERT(scb->nblocks > 0, ("insert_block() called w/ no blocks"));

	if (scb->nblocks == MAXSAVEDBLOCKS) {
		/*
		 * Should try to kick out older blocks XXX JH
		 * May be able to coalesce with existing block.
		 * Or, go other way and free all blocks if we hit this limit.
		 */
		free_sackblock(newblock);
		return;
	}
	KASSERT(scb->nblocks < MAXSAVEDBLOCKS,
	    ("too many SACK blocks %d", scb->nblocks));

	overlap_front = sack_block_lookup(scb, newblock->sblk_start,  &sb);

	if (sb == NULL) {
		workingblock = newblock;
		TAILQ_INSERT_HEAD(&scb->sackblocks, newblock, sblk_list);
		++scb->nblocks;
	} else {
		if (overlap_front || sb->sblk_end == newblock->sblk_start) {
			/* extend old block and discard new one */
			workingblock = sb;
			if (SEQ_GT(newblock->sblk_end, sb->sblk_end))
				sb->sblk_end = newblock->sblk_end;
			free_sackblock(newblock);
		} else {
			workingblock = newblock;
			TAILQ_INSERT_AFTER(&scb->sackblocks, sb, newblock,
					   sblk_list);
			++scb->nblocks;
		}
	}

	/* Consolidate right-hand side. */
	sb = TAILQ_NEXT(workingblock, sblk_list);
	while (sb != NULL &&
	    SEQ_GEQ(workingblock->sblk_end, sb->sblk_end)) {
		struct sackblock *nextblock;

		nextblock = TAILQ_NEXT(sb, sblk_list);
		/* Remove completely overlapped block */
		TAILQ_REMOVE(&scb->sackblocks, sb, sblk_list);
		free_sackblock(sb);
		--scb->nblocks;
		KASSERT(scb->nblocks > 0,
		    ("removed overlapped block: %d blocks left", scb->nblocks));
		sb = nextblock;
	}
	if (sb != NULL &&
	    SEQ_GEQ(workingblock->sblk_end, sb->sblk_start)) {
		/* Extend new block to cover partially overlapped old block. */
		workingblock->sblk_end = sb->sblk_end;
		TAILQ_REMOVE(&scb->sackblocks, sb, sblk_list);
		free_sackblock(sb);
		--scb->nblocks;
		KASSERT(scb->nblocks > 0,
		    ("removed partial right: %d blocks left", scb->nblocks));
	}
}

#ifdef DEBUG_SACK_BLOCKS
static void
tcp_sack_dump_blocks(struct scoreboard *scb)
{
	struct sackblock *sb;

	printf("%d blocks:", scb->nblocks);
	TAILQ_FOREACH(sb, &scb->sackblocks, sblk_list)
		printf(" [%u, %u)", sb->sblk_start, sb->sblk_end);
	printf("\n");
}
#else
static __inline void
tcp_sack_dump_blocks(struct scoreboard *scb)
{
}
#endif

/*
 * Optimization to quickly determine which packets are lost.
 */
static void
update_lostseq(struct scoreboard *scb, tcp_seq snd_una, u_int maxseg)
{
	struct sackblock *sb;
	int nsackblocks = 0;
	int bytes_sacked = 0;

	sb = TAILQ_LAST(&scb->sackblocks, sackblock_list);
	while (sb != NULL) {
		++nsackblocks;
		bytes_sacked += sb->sblk_end - sb->sblk_start;
		if (nsackblocks == tcprexmtthresh ||
		    bytes_sacked >= tcprexmtthresh * maxseg) {
			scb->lostseq = sb->sblk_start;
			return;
		}
		sb = TAILQ_PREV(sb, sackblock_list, sblk_list);
	}
	scb->lostseq = snd_una;
}

/*
 * Return whether the given sequence number is considered lost.
 */
static boolean_t
scb_islost(struct scoreboard *scb, tcp_seq seqnum)
{
	return SEQ_LT(seqnum, scb->lostseq);
}

/*
 * True if at least "amount" has been SACKed.  Used by Early Retransmit.
 */
boolean_t
tcp_sack_has_sacked(struct scoreboard *scb, u_int amount)
{
	struct sackblock *sb;
	int bytes_sacked = 0;

	TAILQ_FOREACH(sb, &scb->sackblocks, sblk_list) {
		bytes_sacked += sb->sblk_end - sb->sblk_start;
		if (bytes_sacked >= amount)
			return TRUE;
	}
	return FALSE;
}

/*
 * Number of bytes SACKed below seq.
 */
int
tcp_sack_bytes_below(struct scoreboard *scb, tcp_seq seq)
{
	struct sackblock *sb;
	int bytes_sacked = 0;

	sb = TAILQ_FIRST(&scb->sackblocks);
	while (sb && SEQ_GT(seq, sb->sblk_start)) {
		bytes_sacked += seq_min(seq, sb->sblk_end) - sb->sblk_start;
		sb = TAILQ_NEXT(sb, sblk_list);
	}
	return bytes_sacked;
}

/*
 * Return estimate of the number of bytes outstanding in the network.
 */
uint32_t
tcp_sack_compute_pipe(struct tcpcb *tp)
{
	struct scoreboard *scb = &tp->scb;
	struct sackblock *sb;
	int nlost, nretransmitted;
	tcp_seq end;

	nlost = tp->snd_max - scb->lostseq;
	nretransmitted = tp->rexmt_high - tp->snd_una;

	TAILQ_FOREACH(sb, &scb->sackblocks, sblk_list) {
		if (SEQ_LT(sb->sblk_start, tp->rexmt_high)) {
			end = seq_min(sb->sblk_end, tp->rexmt_high);
			nretransmitted -= end - sb->sblk_start;
		}
		if (SEQ_GEQ(sb->sblk_start, scb->lostseq))
			nlost -= sb->sblk_end - sb->sblk_start;
	}

	return (nlost + nretransmitted);
}

/*
 * Return the sequence number and length of the next segment to transmit
 * when in Fast Recovery.
 */
boolean_t
tcp_sack_nextseg(struct tcpcb *tp, tcp_seq *nextrexmt, uint32_t *plen,
		 boolean_t *lostdup)
{
	struct scoreboard *scb = &tp->scb;
	struct socket *so = tp->t_inpcb->inp_socket;
	struct sackblock *sb;
	const struct sackblock *lastblock =
	    TAILQ_LAST(&scb->sackblocks, sackblock_list);
	tcp_seq torexmt;
	long len, off;

	/* skip SACKed data */
	tcp_sack_skip_sacked(scb, &tp->rexmt_high);

	/* Look for lost data. */
	torexmt = tp->rexmt_high;
	*lostdup = FALSE;
	if (lastblock != NULL) {
		if (SEQ_LT(torexmt, lastblock->sblk_end) &&
		    scb_islost(scb, torexmt)) {
sendunsacked:
			*nextrexmt = torexmt;
			/* If the left-hand edge has been SACKed, pull it in. */
			if (sack_block_lookup(scb, torexmt + tp->t_maxseg, &sb))
				*plen = sb->sblk_start - torexmt;
			else
				*plen = tp->t_maxseg;
			return TRUE;
		}
	}

	/* See if unsent data available within send window. */
	off = tp->snd_max - tp->snd_una;
	len = (long) ulmin(so->so_snd.sb_cc, tp->snd_wnd) - off;
	if (len > 0) {
		*nextrexmt = tp->snd_max;	/* Send new data. */
		*plen = tp->t_maxseg;
		return TRUE;
	}

	/* We're less certain this data has been lost. */
	if (lastblock == NULL || SEQ_LT(torexmt, lastblock->sblk_end))
		goto sendunsacked;

	return FALSE;
}

/*
 * Return the next sequence number higher than "*prexmt" that has
 * not been SACKed.
 */
void
tcp_sack_skip_sacked(struct scoreboard *scb, tcp_seq *prexmt)
{
	struct sackblock *sb;

	/* skip SACKed data */
	if (sack_block_lookup(scb, *prexmt, &sb))
		*prexmt = sb->sblk_end;
}

#ifdef later
void
tcp_sack_save_scoreboard(struct scoreboard *scb)
{
	struct scoreboard *scb = &tp->scb;

	scb->sackblocks_prev = scb->sackblocks;
	TAILQ_INIT(&scb->sackblocks);
}

void
tcp_sack_revert_scoreboard(struct scoreboard *scb, tcp_seq snd_una,
			   u_int maxseg)
{
	struct sackblock *sb;

	scb->sackblocks = scb->sackblocks_prev;
	scb->nblocks = 0;
	TAILQ_FOREACH(sb, &scb->sackblocks, sblk_list)
		++scb->nblocks;
	tcp_sack_ack_blocks(scb, snd_una);
	scb->lastfound = NULL;
}
#endif

#ifdef DEBUG_SACK_HISTORY
static void
tcp_sack_dump_history(char *msg, struct tcpcb *tp)
{
	int i;
	static int ndumped;

	/* only need a couple of these to debug most problems */
	if (++ndumped > 900)
		return;

	printf("%s:\tnsackhistory %d: ", msg, tp->nsackhistory);
	for (i = 0; i < tp->nsackhistory; ++i)
		printf("[%u, %u) ", tp->sackhistory[i].rblk_start,
		    tp->sackhistory[i].rblk_end);
	printf("\n");
}
#else
static __inline void
tcp_sack_dump_history(char *msg, struct tcpcb *tp)
{
}
#endif

/*
 * Remove old SACK blocks from the SACK history that have already been ACKed.
 */
static void
tcp_sack_ack_history(struct tcpcb *tp)
{
	int i, nblocks, openslot;

	tcp_sack_dump_history("before tcp_sack_ack_history", tp);
	nblocks = tp->nsackhistory;
	for (i = openslot = 0; i < nblocks; ++i) {
		if (SEQ_LEQ(tp->sackhistory[i].rblk_end, tp->rcv_nxt)) {
			--tp->nsackhistory;
			continue;
		}
		if (SEQ_LT(tp->sackhistory[i].rblk_start, tp->rcv_nxt))
			tp->sackhistory[i].rblk_start = tp->rcv_nxt;
		if (i == openslot)
			++openslot;
		else
			tp->sackhistory[openslot++] = tp->sackhistory[i];
	}
	tcp_sack_dump_history("after tcp_sack_ack_history", tp);
	KASSERT(openslot == tp->nsackhistory,
	    ("tcp_sack_ack_history miscounted: %d != %d",
	    openslot, tp->nsackhistory));
}

/*
 * Add or merge newblock into reported history.
 * Also remove or update SACK blocks that will be acked.
 */
static void
tcp_sack_update_reported_history(struct tcpcb *tp, tcp_seq start, tcp_seq end)
{
	struct raw_sackblock copy[MAX_SACK_REPORT_BLOCKS];
	int i, cindex;

	tcp_sack_dump_history("before tcp_sack_update_reported_history", tp);
	/*
	 * Six cases:
	 *	0) no overlap
	 *	1) newblock == oldblock
	 *	2) oldblock contains newblock
	 *	3) newblock contains oldblock
	 *	4) tail of oldblock overlaps or abuts start of newblock
	 *	5) tail of newblock overlaps or abuts head of oldblock
	 */
	for (i = cindex = 0; i < tp->nsackhistory; ++i) {
		struct raw_sackblock *oldblock = &tp->sackhistory[i];
		tcp_seq old_start = oldblock->rblk_start;
		tcp_seq old_end = oldblock->rblk_end;

		if (SEQ_LT(end, old_start) || SEQ_GT(start, old_end)) {
			/* Case 0:  no overlap.  Copy old block. */
			copy[cindex++] = *oldblock;
			continue;
		}

		if (SEQ_GEQ(start, old_start) && SEQ_LEQ(end, old_end)) {
			/* Cases 1 & 2.  Move block to front of history. */
			int j;

			start = old_start;
			end = old_end;
			/* no need to check rest of blocks */
			for (j = i + 1; j < tp->nsackhistory; ++j)
				copy[cindex++] = tp->sackhistory[j];
			break;
		}

		if (SEQ_GEQ(old_end, start) && SEQ_LT(old_start, start)) {
			/* Case 4:  extend start of new block. */
			start = old_start;
		} else if (SEQ_GEQ(end, old_start) && SEQ_GT(old_end, end)) {
			/* Case 5: extend end of new block */
			end = old_end;
		} else {
			/* Case 3.  Delete old block by not copying it. */
			KASSERT(SEQ_LEQ(start, old_start) &&
				SEQ_GEQ(end, old_end),
			    ("bad logic: old [%u, %u), new [%u, %u)",
			     old_start, old_end, start, end));
		}
	}

	/* insert new block */
	tp->sackhistory[0].rblk_start = start;
	tp->sackhistory[0].rblk_end = end;
	cindex = min(cindex, MAX_SACK_REPORT_BLOCKS - 1);
	for (i = 0; i < cindex; ++i)
		tp->sackhistory[i + 1] = copy[i];
	tp->nsackhistory = cindex + 1;
	tcp_sack_dump_history("after tcp_sack_update_reported_history", tp);
}

/*
 * Fill in SACK report to return to data sender.
 */
void
tcp_sack_fill_report(struct tcpcb *tp, u_char *opt, u_int *plen)
{
	u_int optlen = *plen;
	uint32_t *lp = (uint32_t *)(opt + optlen);
	uint32_t *olp;
	tcp_seq hstart = tp->rcv_nxt, hend;
	int nblocks;

	KASSERT(TCP_MAXOLEN - optlen >=
	    TCPOLEN_SACK_ALIGNED + TCPOLEN_SACK_BLOCK,
	    ("no room for SACK header and one block: optlen %d", optlen));

	olp = lp++;
	optlen += TCPOLEN_SACK_ALIGNED;

	tcp_sack_ack_history(tp);
	if (tp->reportblk.rblk_start != tp->reportblk.rblk_end) {
		*lp++ = htonl(tp->reportblk.rblk_start);
		*lp++ = htonl(tp->reportblk.rblk_end);
		optlen += TCPOLEN_SACK_BLOCK;
		hstart = tp->reportblk.rblk_start;
		hend = tp->reportblk.rblk_end;
		if (tp->t_flags & TF_ENCLOSESEG) {
			KASSERT(TCP_MAXOLEN - optlen >= TCPOLEN_SACK_BLOCK,
			    ("no room for enclosing SACK block: oplen %d",
			    optlen));
			*lp++ = htonl(tp->encloseblk.rblk_start);
			*lp++ = htonl(tp->encloseblk.rblk_end);
			optlen += TCPOLEN_SACK_BLOCK;
			hstart = tp->encloseblk.rblk_start;
			hend = tp->encloseblk.rblk_end;
		}
		if (SEQ_GT(hstart, tp->rcv_nxt))
			tcp_sack_update_reported_history(tp, hstart, hend);
	}
	if (tcp_do_smartsack && (tp->t_flags & TF_SACKLEFT)) {
		/* Fill in from left!  Walk re-assembly queue. */
		struct tseg_qent *q;

		q = LIST_FIRST(&tp->t_segq);
		while (q != NULL &&
		    TCP_MAXOLEN - optlen >= TCPOLEN_SACK_BLOCK) {
			*lp++ = htonl(q->tqe_th->th_seq);
			*lp++ = htonl(q->tqe_th->th_seq + q->tqe_len);
			optlen += TCPOLEN_SACK_BLOCK;
			q = LIST_NEXT(q, tqe_q);
		}
	} else {
		int n = 0;

		/* Fill in SACK blocks from right side. */
		while (n < tp->nsackhistory &&
		    TCP_MAXOLEN - optlen >= TCPOLEN_SACK_BLOCK) {
			if (tp->sackhistory[n].rblk_start != hstart) {
				*lp++ = htonl(tp->sackhistory[n].rblk_start);
				*lp++ = htonl(tp->sackhistory[n].rblk_end);
				optlen += TCPOLEN_SACK_BLOCK;
			}
			++n;
		}
	}
	tp->reportblk.rblk_start = tp->reportblk.rblk_end;
	tp->t_flags &= ~(TF_DUPSEG | TF_ENCLOSESEG | TF_SACKLEFT);
	nblocks = (lp - olp - 1) / 2;
	*olp = htonl(TCPOPT_SACK_ALIGNED |
		     (TCPOLEN_SACK + nblocks * TCPOLEN_SACK_BLOCK));
	*plen = optlen;
}
