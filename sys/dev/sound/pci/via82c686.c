/*
 * Copyright (c) 2000 David Jones <dej@ox.org>
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
 * $FreeBSD: src/sys/dev/sound/pci/via82c686.c,v 1.4.2.10 2003/05/11 01:45:53 orion Exp $
 * $DragonFly: src/sys/dev/sound/pci/via82c686.c,v 1.3 2003/08/07 21:17:13 dillon Exp $
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>
#include <sys/sysctl.h>

#include <dev/sound/pci/via82c686.h>

SND_DECLARE_FILE("$DragonFly: src/sys/dev/sound/pci/via82c686.c,v 1.3 2003/08/07 21:17:13 dillon Exp $");

#define VIA_PCI_ID 0x30581106
#define	NSEGS		4	/* Number of segments in SGD table */

#define SEGS_PER_CHAN	(NSEGS/2)

#define TIMEOUT	50
#define	VIA_DEFAULT_BUFSZ	0x1000

#undef DEB
#define DEB(x)

/* we rely on this struct being packed to 64 bits */
struct via_dma_op {
        u_int32_t ptr;
        u_int32_t flags;
#define VIA_DMAOP_EOL         0x80000000
#define VIA_DMAOP_FLAG        0x40000000
#define VIA_DMAOP_STOP        0x20000000
#define VIA_DMAOP_COUNT(x)    ((x)&0x00FFFFFF)
};

struct via_info;

struct via_chinfo {
	struct via_info *parent;
	struct pcm_channel *channel;
	struct snd_dbuf *buffer;
	struct via_dma_op *sgd_table;
	int dir, blksz;
	int base, count, mode, ctrl;
};

struct via_info {
	bus_space_tag_t st;
	bus_space_handle_t sh;
	bus_dma_tag_t parent_dmat;
	bus_dma_tag_t sgd_dmat;
	bus_dmamap_t sgd_dmamap;

	struct resource *reg, *irq;
	int regid, irqid;
	void *ih;
	struct ac97_info *codec;

	unsigned int bufsz;

	struct via_chinfo pch, rch;
	struct via_dma_op *sgd_table;
	u_int16_t codec_caps;
};

static u_int32_t via_fmt[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};
static struct pcmchan_caps via_vracaps = {4000, 48000, via_fmt, 0};
static struct pcmchan_caps via_caps = {48000, 48000, via_fmt, 0};

static u_int32_t
via_rd(struct via_info *via, int regno, int size)
{

	switch (size) {
	case 1:
		return bus_space_read_1(via->st, via->sh, regno);
	case 2:
		return bus_space_read_2(via->st, via->sh, regno);
	case 4:
		return bus_space_read_4(via->st, via->sh, regno);
	default:
		return 0xFFFFFFFF;
	}
}


static void
via_wr(struct via_info *via, int regno, u_int32_t data, int size)
{

	switch (size) {
	case 1:
		bus_space_write_1(via->st, via->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(via->st, via->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(via->st, via->sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/* Codec interface */

static int
via_waitready_codec(struct via_info *via)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; (i < TIMEOUT) &&
	    (via_rd(via, VIA_CODEC_CTL, 4) & VIA_CODEC_BUSY); i++)
		DELAY(1);
	if (i >= TIMEOUT) {
		printf("via: codec busy\n");
		return 1;
	}

	return 0;
}


static int
via_waitvalid_codec(struct via_info *via)
{
	int i;

	/* poll until codec valid */
	for (i = 0; (i < TIMEOUT) &&
	    !(via_rd(via, VIA_CODEC_CTL, 4) & VIA_CODEC_PRIVALID); i++)
		    DELAY(1);
	if (i >= TIMEOUT) {
		printf("via: codec invalid\n");
		return 1;
	}

	return 0;
}


static int
via_write_codec(kobj_t obj, void *addr, int reg, u_int32_t val)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via)) return -1;

	via_wr(via, VIA_CODEC_CTL, VIA_CODEC_PRIVALID | VIA_CODEC_INDEX(reg) | val, 4);

	return 0;
}


static int
via_read_codec(kobj_t obj, void *addr, int reg)
{
	struct via_info *via = addr;

	if (via_waitready_codec(via))
		return -1;

	via_wr(via, VIA_CODEC_CTL, VIA_CODEC_PRIVALID | VIA_CODEC_READ | VIA_CODEC_INDEX(reg),4);

	if (via_waitready_codec(via))
		return -1;

	if (via_waitvalid_codec(via))
		return -1;

	return via_rd(via, VIA_CODEC_CTL, 2);
}

static kobj_method_t via_ac97_methods[] = {
    	KOBJMETHOD(ac97_read,		via_read_codec),
    	KOBJMETHOD(ac97_write,		via_write_codec),
	{ 0, 0 }
};
AC97_DECLARE(via_ac97);

/* -------------------------------------------------------------------- */

static int
via_buildsgdt(struct via_chinfo *ch)
{
	u_int32_t phys_addr, flag;
	int i, segs, seg_size;

	/*
	 *  Build the scatter/gather DMA (SGD) table.
	 *  There are four slots in the table: two for play, two for record.
	 *  This creates two half-buffers, one of which is playing; the other
	 *  is feeding.
	 */
	seg_size = ch->blksz;
	segs = sndbuf_getsize(ch->buffer) / seg_size;
	phys_addr = vtophys(sndbuf_getbuf(ch->buffer));

	for (i = 0; i < segs; i++) {
		flag = (i == segs - 1)? VIA_DMAOP_EOL : VIA_DMAOP_FLAG;
		ch->sgd_table[i].ptr = phys_addr + (i * seg_size);
		ch->sgd_table[i].flags = flag | seg_size;
	}

	return 0;
}

/* channel interface */
static void *
viachan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct via_info *via = devinfo;
	struct via_chinfo *ch = (dir == PCMDIR_PLAY)? &via->pch : &via->rch;

	ch->parent = via;
	ch->channel = c;
	ch->buffer = b;
	ch->dir = dir;
	ch->sgd_table = &via->sgd_table[(dir == PCMDIR_PLAY)? 0 : SEGS_PER_CHAN];
	if (ch->dir == PCMDIR_PLAY) {
		ch->base = VIA_PLAY_DMAOPS_BASE;
		ch->count = VIA_PLAY_DMAOPS_COUNT;
		ch->ctrl = VIA_PLAY_CONTROL;
		ch->mode = VIA_PLAY_MODE;
	} else {
		ch->base = VIA_RECORD_DMAOPS_BASE;
		ch->count = VIA_RECORD_DMAOPS_COUNT;
		ch->ctrl = VIA_RECORD_CONTROL;
		ch->mode = VIA_RECORD_MODE;
	}

	if (sndbuf_alloc(ch->buffer, via->parent_dmat, via->bufsz) == -1)
		return NULL;
	return ch;
}

static int
viachan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	int mode, mode_set;

	mode_set = 0;
	if (format & AFMT_STEREO)
		mode_set |= VIA_RPMODE_STEREO;
	if (format & AFMT_S16_LE)
		mode_set |= VIA_RPMODE_16BIT;

	DEB(printf("set format: dir = %d, format=%x\n", ch->dir, format));
	mode = via_rd(via, ch->mode, 1);
	mode &= ~(VIA_RPMODE_16BIT | VIA_RPMODE_STEREO);
	mode |= mode_set;
	via_wr(via, ch->mode, mode, 1);

	return 0;
}

static int
viachan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	int reg;

	/*
	 *  Basic AC'97 defines a 48 kHz sample rate only.  For other rates,
	 *  upsampling is required.
	 *
	 *  The VT82C686A does not perform upsampling, and neither do we.
	 *  If the codec supports variable-rate audio (i.e. does the upsampling
	 *  itself), then negotiate the rate with the codec.  Otherwise,
	 *  return 48 kHz cuz that's all you got.
	 */
	if (via->codec_caps & AC97_EXTCAP_VRA) {
		reg = (ch->dir == PCMDIR_PLAY)? AC97_REGEXT_FDACRATE : AC97_REGEXT_LADCRATE;
		return ac97_setrate(via->codec, reg, speed);
	} else
		return 48000;
}

static int
viachan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct via_chinfo *ch = data;

	ch->blksz = blocksize;
	sndbuf_resize(ch->buffer, SEGS_PER_CHAN, ch->blksz);

	return ch->blksz;
}

static int
viachan_trigger(kobj_t obj, void *data, int go)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado;

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD)
		return 0;

	ado = ch->sgd_table;
	DEB(printf("ado located at va=%p pa=%x\n", ado, vtophys(ado)));

	if (go == PCMTRIG_START) {
		via_buildsgdt(ch);
		via_wr(via, ch->base, vtophys(ado), 4);
		via_wr(via, ch->ctrl, VIA_RPCTRL_START, 1);
	} else
		via_wr(via, ch->ctrl, VIA_RPCTRL_TERMINATE, 1);

	DEB(printf("viachan_trigger: go=%d\n", go));
	return 0;
}

static int
viachan_getptr(kobj_t obj, void *data)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;
	struct via_dma_op *ado;
	int ptr, base, base1, len, seg;

	ado = ch->sgd_table;
	base1 = via_rd(via, ch->base, 4);
	len = via_rd(via, ch->count, 4);
	base = via_rd(via, ch->base, 4);
	if (base != base1) 	/* Avoid race hazard */
		len = via_rd(via, ch->count, 4);

	DEB(printf("viachan_getptr: len / base = %x / %x\n", len, base));

	/* Base points to SGD segment to do, one past current */

	/* Determine how many segments have been done */
	seg = (base - vtophys(ado)) / sizeof(struct via_dma_op);
	if (seg == 0)
		seg = SEGS_PER_CHAN;

	/* Now work out offset: seg less count */
	ptr = (seg * sndbuf_getsize(ch->buffer) / SEGS_PER_CHAN) - len;
	if (ch->dir == PCMDIR_REC) {
		/* DMA appears to operate on memory 'lines' of 32 bytes	*/
		/* so don't return any part line - it isn't in RAM yet	*/
		ptr = ptr & ~0x1f;
	}

	DEB(printf("return ptr=%d\n", ptr));
	return ptr;
}

static struct pcmchan_caps *
viachan_getcaps(kobj_t obj, void *data)
{
	struct via_chinfo *ch = data;
	struct via_info *via = ch->parent;

	return (via->codec_caps & AC97_EXTCAP_VRA)? &via_vracaps : &via_caps;
}

static kobj_method_t viachan_methods[] = {
    	KOBJMETHOD(channel_init,		viachan_init),
    	KOBJMETHOD(channel_setformat,		viachan_setformat),
    	KOBJMETHOD(channel_setspeed,		viachan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	viachan_setblocksize),
    	KOBJMETHOD(channel_trigger,		viachan_trigger),
    	KOBJMETHOD(channel_getptr,		viachan_getptr),
    	KOBJMETHOD(channel_getcaps,		viachan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(viachan);

/* -------------------------------------------------------------------- */

static void
via_intr(void *p)
{
	struct via_info *via = p;
	int		st;

	/* DEB(printf("viachan_intr\n")); */
	/* Read channel */
	st = via_rd(via, VIA_PLAY_STAT, 1);
	if (st & VIA_RPSTAT_INTR) {
		via_wr(via, VIA_PLAY_STAT, VIA_RPSTAT_INTR, 1);
		chn_intr(via->pch.channel);
	}

	/* Write channel */
	st = via_rd(via, VIA_RECORD_STAT, 1);
	if (st & VIA_RPSTAT_INTR) {
		via_wr(via, VIA_RECORD_STAT, VIA_RPSTAT_INTR, 1);
		chn_intr(via->rch.channel);
	}
}

/*
 *  Probe and attach the card
 */
static int
via_probe(device_t dev)
{
	if (pci_get_devid(dev) == VIA_PCI_ID) {
	    device_set_desc(dev, "VIA VT82C686A");
	    return 0;
	}
	return ENXIO;
}


static void
dma_cb(void *p, bus_dma_segment_t *bds, int a, int b)
{
}


static int
via_attach(device_t dev)
{
	struct via_info *via = 0;
	char status[SND_STATUSLEN];
	u_int32_t data, cnt;

	if ((via = malloc(sizeof *via, M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	/* Get resources */
	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	/* Wake up and reset AC97 if necessary */
	data = pci_read_config(dev, VIA_AC97STATUS, 1);

	if ((data & VIA_AC97STATUS_RDY) == 0) {
		/* Cold reset per ac97r2.3 spec (page 95) */
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN, 1);			/* Assert low */
		DELAY(100);									/* Wait T_rst_low */
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN | VIA_ACLINK_NRST, 1);	/* Assert high */
		DELAY(5);									/* Wait T_rst2clk */
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN, 1);			/* Assert low */
	} else {
		/* Warm reset */
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN, 1);			/* Force no sync */
		DELAY(100);
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN | VIA_ACLINK_SYNC, 1);	/* Sync */
		DELAY(5);									/* Wait T_sync_high */
		pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_EN, 1);			/* Force no sync */
		DELAY(5);									/* Wait T_sync2clk */
	}

	/* Power everything up */
	pci_write_config(dev, VIA_ACLINKCTRL, VIA_ACLINK_DESIRED, 1);	

	/* Wait for codec to become ready (largest reported delay here 310ms) */
	for (cnt = 0; cnt < 2000; cnt++) {
		data = pci_read_config(dev, VIA_AC97STATUS, 1);
		if (data & VIA_AC97STATUS_RDY) 
			break;
		DELAY(5000);
	}

	via->regid = PCIR_MAPS;
	via->reg = bus_alloc_resource(dev, SYS_RES_IOPORT, &via->regid, 0, ~0, 1, RF_ACTIVE);
	if (!via->reg) {
		device_printf(dev, "cannot allocate bus resource.");
		goto bad;
	}
	via->st = rman_get_bustag(via->reg);
	via->sh = rman_get_bushandle(via->reg);

	via->bufsz = pcm_getbuffersize(dev, 4096, VIA_DEFAULT_BUFSZ, 65536);

	via->irqid = 0;
	via->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &via->irqid, 0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!via->irq || snd_setup_intr(dev, via->irq, 0, via_intr, via, &via->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto bad;
	}

	via_wr(via, VIA_PLAY_MODE, VIA_RPMODE_AUTOSTART | VIA_RPMODE_INTR_FLAG | VIA_RPMODE_INTR_EOL, 1);
	via_wr(via, VIA_RECORD_MODE, VIA_RPMODE_AUTOSTART | VIA_RPMODE_INTR_FLAG | VIA_RPMODE_INTR_EOL, 1);

	via->codec = AC97_CREATE(dev, via, via_ac97);
	if (!via->codec)
		goto bad;

	if (mixer_init(dev, ac97_getmixerclass(), via->codec))
		goto bad;

	via->codec_caps = ac97_getextcaps(via->codec);
	ac97_setextmode(via->codec, 
			via->codec_caps & (AC97_EXTCAP_VRA | AC97_EXTCAP_VRM));

	/* DMA tag for buffers */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/via->bufsz, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &via->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	/*
	 *  DMA tag for SGD table.  The 686 uses scatter/gather DMA and
	 *  requires a list in memory of work to do.  We need only 16 bytes
	 *  for this list, and it is wasteful to allocate 16K.
	 */
	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/NSEGS * sizeof(struct via_dma_op),
		/*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &via->sgd_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto bad;
	}

	if (bus_dmamem_alloc(via->sgd_dmat, (void **)&via->sgd_table, BUS_DMA_NOWAIT, &via->sgd_dmamap) == -1)
		goto bad;
	if (bus_dmamap_load(via->sgd_dmat, via->sgd_dmamap, via->sgd_table, NSEGS * sizeof(struct via_dma_op), dma_cb, 0, 0))
		goto bad;

	snprintf(status, SND_STATUSLEN, "at io 0x%lx irq %ld", rman_get_start(via->reg), rman_get_start(via->irq));

	/* Register */
	if (pcm_register(dev, via, 1, 1)) goto bad;
	pcm_addchan(dev, PCMDIR_PLAY, &viachan_class, via);
	pcm_addchan(dev, PCMDIR_REC, &viachan_class, via);
	pcm_setstatus(dev, status);
	return 0;
bad:
	if (via->codec) ac97_destroy(via->codec);
	if (via->reg) bus_release_resource(dev, SYS_RES_IOPORT, via->regid, via->reg);
	if (via->ih) bus_teardown_intr(dev, via->irq, via->ih);
	if (via->irq) bus_release_resource(dev, SYS_RES_IRQ, via->irqid, via->irq);
	if (via->parent_dmat) bus_dma_tag_destroy(via->parent_dmat);
	if (via->sgd_dmamap) bus_dmamap_unload(via->sgd_dmat, via->sgd_dmamap);
	if (via->sgd_dmat) bus_dma_tag_destroy(via->sgd_dmat);
	if (via) free(via, M_DEVBUF);
	return ENXIO;
}

static int
via_detach(device_t dev)
{
	int r;
	struct via_info *via = 0;

	r = pcm_unregister(dev);
	if (r)
		return r;

	via = pcm_getdevinfo(dev);
	bus_release_resource(dev, SYS_RES_IOPORT, via->regid, via->reg);
	bus_teardown_intr(dev, via->irq, via->ih);
	bus_release_resource(dev, SYS_RES_IRQ, via->irqid, via->irq);
	bus_dma_tag_destroy(via->parent_dmat);
	bus_dmamap_unload(via->sgd_dmat, via->sgd_dmamap);
	bus_dma_tag_destroy(via->sgd_dmat);
	free(via, M_DEVBUF);
	return 0;
}


static device_method_t via_methods[] = {
	DEVMETHOD(device_probe,		via_probe),
	DEVMETHOD(device_attach,	via_attach),
	DEVMETHOD(device_detach,	via_detach),
	{ 0, 0}
};

static driver_t via_driver = {
	"pcm",
	via_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_via82c686, pci, via_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_via82c686, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_via82c686, 1);
