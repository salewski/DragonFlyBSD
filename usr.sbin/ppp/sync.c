/*-
 * Copyright (c) 1999 Brian Somers <brian@Awfulhak.org>
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
 * $FreeBSD: src/usr.sbin/ppp/sync.c,v 1.6 1999/12/20 20:29:47 brian Exp $
 * $DragonFly: src/usr.sbin/ppp/sync.c,v 1.2 2003/06/17 04:30:01 dillon Exp $
 */

#include <sys/types.h>

#include <stdio.h>
#include <termios.h>

#include "layer.h"
#include "defs.h"
#include "mbuf.h"
#include "log.h"
#include "sync.h"
#include "timer.h"
#include "lqr.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "ccp.h"
#include "link.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"

static struct mbuf *
sync_LayerPush(struct bundle *bundle, struct link *l, struct mbuf *bp,
                int pri, u_short *proto)
{
  log_DumpBp(LogSYNC, "Write", bp);
  m_settype(bp, MB_SYNCOUT);
  return bp;
}

static struct mbuf *
sync_LayerPull(struct bundle *b, struct link *l, struct mbuf *bp,
               u_short *proto)
{
  struct physical *p = link2physical(l);

  if (!p)
    log_Printf(LogERROR, "Can't Pull a sync packet from a logical link\n");
  else {
    log_DumpBp(LogSYNC, "Read", bp);

    /* Either done here or by the HDLC layer */
    p->hdlc.lqm.SaveInOctets += m_length(bp) + 1;
    p->hdlc.lqm.SaveInPackets++;
    m_settype(bp, MB_SYNCIN);
  }

  return bp;
}

struct layer synclayer = { LAYER_SYNC, "sync", sync_LayerPush, sync_LayerPull };
