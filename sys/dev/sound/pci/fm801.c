/*
 * Copyright (c) 2000 Dmitry Dicky diwil@dataart.com
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND
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
 * $FreeBSD: src/sys/dev/sound/pci/fm801.c,v 1.3.2.8 2002/12/24 21:17:42 semenu Exp $
 * $DragonFly: src/sys/dev/sound/pci/fm801.c,v 1.5 2004/08/09 19:49:28 dillon Exp $
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>
#include <bus/pci/pcireg.h>
#include <bus/pci/pcivar.h>

SND_DECLARE_FILE("$DragonFly: src/sys/dev/sound/pci/fm801.c,v 1.5 2004/08/09 19:49:28 dillon Exp $");

#define PCI_VENDOR_FORTEMEDIA	0x1319
#define PCI_DEVICE_FORTEMEDIA1	0x08011319
#define PCI_DEVICE_FORTEMEDIA2	0x08021319	/* ??? have no idea what's this... */

#define FM_PCM_VOLUME           0x00
#define FM_FM_VOLUME            0x02
#define FM_I2S_VOLUME           0x04
#define FM_RECORD_SOURCE        0x06

#define FM_PLAY_CTL             0x08
#define  FM_PLAY_RATE_MASK              0x0f00
#define  FM_PLAY_BUF1_LAST              0x0001
#define  FM_PLAY_BUF2_LAST              0x0002
#define  FM_PLAY_START                  0x0020
#define  FM_PLAY_PAUSE                  0x0040
#define  FM_PLAY_STOPNOW                0x0080
#define  FM_PLAY_16BIT                  0x4000
#define  FM_PLAY_STEREO                 0x8000

#define FM_PLAY_DMALEN          0x0a
#define FM_PLAY_DMABUF1         0x0c
#define FM_PLAY_DMABUF2         0x10


#define FM_REC_CTL              0x14
#define  FM_REC_RATE_MASK               0x0f00
#define  FM_REC_BUF1_LAST               0x0001
#define  FM_REC_BUF2_LAST               0x0002
#define  FM_REC_START                   0x0020
#define  FM_REC_PAUSE                   0x0040
#define  FM_REC_STOPNOW                 0x0080
#define  FM_REC_16BIT                   0x4000
#define  FM_REC_STEREO                  0x8000


#define FM_REC_DMALEN           0x16
#define FM_REC_DMABUF1          0x18
#define FM_REC_DMABUF2          0x1c

#define FM_CODEC_CTL            0x22
#define FM_VOLUME               0x26
#define  FM_VOLUME_MUTE                 0x8000

#define FM_CODEC_CMD            0x2a
#define  FM_CODEC_CMD_READ              0x0080
#define  FM_CODEC_CMD_VALID             0x0100
#define  FM_CODEC_CMD_BUSY              0x0200

#define FM_CODEC_DATA           0x2c

#define FM_IO_CTL               0x52
#define FM_CARD_CTL             0x54

#define FM_INTMASK              0x56
#define  FM_INTMASK_PLAY                0x0001
#define  FM_INTMASK_REC                 0x0002
#define  FM_INTMASK_VOL                 0x0040
#define  FM_INTMASK_MPU                 0x0080

#define FM_INTSTATUS            0x5a
#define  FM_INTSTATUS_PLAY              0x0100
#define  FM_INTSTATUS_REC               0x0200
#define  FM_INTSTATUS_VOL               0x4000
#define  FM_INTSTATUS_MPU               0x8000

#define FM801_DEFAULT_BUFSZ	4096	/* Other values do not work!!! */

/* debug purposes */
#define DPRINT	 if(0) printf

/*
static int fm801ch_setup(struct pcm_channel *c);
*/

static u_int32_t fmts[] = {
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	0
};

static struct pcmchan_caps fm801ch_caps = {
	4000, 48000,
	fmts, 0
};

struct fm801_info;

struct fm801_chinfo {
	struct fm801_info 	*parent;
	struct pcm_channel 		*channel;
	struct snd_dbuf 		*buffer;
	u_int32_t 		spd, dir, fmt;	/* speed, direction, format */
	u_int32_t		shift;
};

struct fm801_info {
	int 			type;
	bus_space_tag_t 	st;
	bus_space_handle_t 	sh;
	bus_dma_tag_t   	parent_dmat;

	device_t 		dev;
	int 			num;
	u_int32_t 		unit;

	struct resource 	*reg, *irq;
	int             	regtype, regid, irqid;
	void            	*ih;

	u_int32_t		play_flip,
				play_nextblk,
				play_start,
				play_blksize,
				play_fmt,
				play_shift,
				play_size;

	u_int32_t		rec_flip,
				rec_nextblk,
				rec_start,
				rec_blksize,
				rec_fmt,
				rec_shift,
				rec_size;

	unsigned int 		bufsz;

	struct fm801_chinfo	pch, rch;

	device_t		radio;
};

/* Bus Read / Write routines */
static u_int32_t
fm801_rd(struct fm801_info *fm801, int regno, int size)
{
	switch(size) {
	case 1:
		return (bus_space_read_1(fm801->st, fm801->sh, regno));
	case 2:
		return (bus_space_read_2(fm801->st, fm801->sh, regno));
	case 4:
		return (bus_space_read_4(fm801->st, fm801->sh, regno));
	default:
		return 0xffffffff;
	}
}

static void
fm801_wr(struct fm801_info *fm801, int regno, u_int32_t data, int size)
{

	switch(size) {
	case 1:
		bus_space_write_1(fm801->st, fm801->sh, regno, data);
		break;
	case 2:
		bus_space_write_2(fm801->st, fm801->sh, regno, data);
		break;
	case 4:
		bus_space_write_4(fm801->st, fm801->sh, regno, data);
		break;
	}
}

/* -------------------------------------------------------------------- */
/*
 *  ac97 codec routines
 */
#define TIMO 50
static int
fm801_rdcd(kobj_t obj, void *devinfo, int regno)
{
	struct fm801_info *fm801 = (struct fm801_info *)devinfo;
	int i;

	for (i = 0; i < TIMO && fm801_rd(fm801,FM_CODEC_CMD,2) & FM_CODEC_CMD_BUSY; i++) {
		DELAY(10000);
		DPRINT("fm801 rdcd: 1 - DELAY\n");
	}
	if (i >= TIMO) {
		printf("fm801 rdcd: codec busy\n");
		return 0;
	}

	fm801_wr(fm801,FM_CODEC_CMD, regno|FM_CODEC_CMD_READ,2);

	for (i = 0; i < TIMO && !(fm801_rd(fm801,FM_CODEC_CMD,2) & FM_CODEC_CMD_VALID); i++)
	{
		DELAY(10000);
		DPRINT("fm801 rdcd: 2 - DELAY\n");
	}
	if (i >= TIMO) {
		printf("fm801 rdcd: write codec invalid\n");
		return 0;
	}

	return fm801_rd(fm801,FM_CODEC_DATA,2);
}

static int
fm801_wrcd(kobj_t obj, void *devinfo, int regno, u_int32_t data)
{
	struct fm801_info *fm801 = (struct fm801_info *)devinfo;
	int i;

	DPRINT("fm801_wrcd reg 0x%x val 0x%x\n",regno, data);
/*
	if(regno == AC97_REG_RECSEL)	return;
*/
	/* Poll until codec is ready */
	for (i = 0; i < TIMO && fm801_rd(fm801,FM_CODEC_CMD,2) & FM_CODEC_CMD_BUSY; i++) {
		DELAY(10000);
		DPRINT("fm801 rdcd: 1 - DELAY\n");
	}
	if (i >= TIMO) {
		printf("fm801 wrcd: read codec busy\n");
		return -1;
	}

	fm801_wr(fm801,FM_CODEC_DATA,data, 2);
	fm801_wr(fm801,FM_CODEC_CMD, regno,2);

	/* wait until codec is ready */
	for (i = 0; i < TIMO && fm801_rd(fm801,FM_CODEC_CMD,2) & FM_CODEC_CMD_BUSY; i++) {
		DELAY(10000);
		DPRINT("fm801 wrcd: 2 - DELAY\n");
	}
	if (i >= TIMO) {
		printf("fm801 wrcd: read codec busy\n");
		return -1;
	}
	DPRINT("fm801 wrcd release reg 0x%x val 0x%x\n",regno, data);
	return 0;
}

static kobj_method_t fm801_ac97_methods[] = {
    	KOBJMETHOD(ac97_read,		fm801_rdcd),
    	KOBJMETHOD(ac97_write,		fm801_wrcd),
	{ 0, 0 }
};
AC97_DECLARE(fm801_ac97);

/* -------------------------------------------------------------------- */

/*
 * The interrupt handler
 */
static void
fm801_intr(void *p)
{
	struct fm801_info 	*fm801 = (struct fm801_info *)p;
	u_int32_t       	intsrc = fm801_rd(fm801, FM_INTSTATUS, 2);

	DPRINT("\nfm801_intr intsrc 0x%x ", intsrc);

	if(intsrc & FM_INTSTATUS_PLAY) {
		fm801->play_flip++;
		if(fm801->play_flip & 1) {
			fm801_wr(fm801, FM_PLAY_DMABUF1, fm801->play_start,4);
		} else
			fm801_wr(fm801, FM_PLAY_DMABUF2, fm801->play_nextblk,4);
		chn_intr(fm801->pch.channel);
	}

	if(intsrc & FM_INTSTATUS_REC) {
		fm801->rec_flip++;
		if(fm801->rec_flip & 1) {
			fm801_wr(fm801, FM_REC_DMABUF1, fm801->rec_start,4);
		} else
			fm801_wr(fm801, FM_REC_DMABUF2, fm801->rec_nextblk,4);
		chn_intr(fm801->rch.channel);
	}

	if ( intsrc & FM_INTSTATUS_MPU ) {
		/* This is a TODOish thing... */
		fm801_wr(fm801, FM_INTSTATUS, intsrc & FM_INTSTATUS_MPU,2);
	}

	if ( intsrc & FM_INTSTATUS_VOL ) {
		/* This is a TODOish thing... */
		fm801_wr(fm801, FM_INTSTATUS, intsrc & FM_INTSTATUS_VOL,2);
	}

	DPRINT("fm801_intr clear\n\n");
	fm801_wr(fm801, FM_INTSTATUS, intsrc & (FM_INTSTATUS_PLAY | FM_INTSTATUS_REC), 2);
}

/* -------------------------------------------------------------------- */
/* channel interface */
static void *
fm801ch_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct fm801_info *fm801 = (struct fm801_info *)devinfo;
	struct fm801_chinfo *ch = (dir == PCMDIR_PLAY)? &fm801->pch : &fm801->rch;

	DPRINT("fm801ch_init, direction = %d\n", dir);
	ch->parent = fm801;
	ch->channel = c;
	ch->buffer = b;
	ch->dir = dir;
	if (sndbuf_alloc(ch->buffer, fm801->parent_dmat, fm801->bufsz) == -1) return NULL;
	return (void *)ch;
}

static int
fm801ch_setformat(kobj_t obj, void *data, u_int32_t format)
{
	struct fm801_chinfo *ch = data;
	struct fm801_info *fm801 = ch->parent;

	DPRINT("fm801ch_setformat 0x%x : %s, %s, %s, %s\n", format,
		(format & AFMT_STEREO)?"stereo":"mono",
		(format & (AFMT_S16_LE | AFMT_S16_BE | AFMT_U16_LE | AFMT_U16_BE)) ? "16bit":"8bit",
		(format & AFMT_SIGNED)? "signed":"unsigned",
		(format & AFMT_BIGENDIAN)?"bigendiah":"littleendian" );

	if(ch->dir == PCMDIR_PLAY) {
		fm801->play_fmt =  (format & AFMT_STEREO)? FM_PLAY_STEREO : 0;
		fm801->play_fmt |= (format & AFMT_16BIT) ? FM_PLAY_16BIT : 0;
		return 0;
	}

	if(ch->dir == PCMDIR_REC ) {
		fm801->rec_fmt = (format & AFMT_STEREO)? FM_REC_STEREO:0;
		fm801->rec_fmt |= (format & AFMT_16BIT) ? FM_PLAY_16BIT : 0;
		return 0;
	}

	return 0;
}

struct {
	int limit;
	int rate;
} fm801_rates[11] = {
	{  6600,  5500 },
	{  8750,  8000 },
	{ 10250,  9600 },
	{ 13200, 11025 },
	{ 17500, 16000 },
	{ 20500, 19200 },
	{ 26500, 22050 },
	{ 35000, 32000 },
	{ 41000, 38400 },
	{ 46000, 44100 },
	{ 48000, 48000 },
/* anything above -> 48000 */
};

static int
fm801ch_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	struct fm801_chinfo *ch = data;
	struct fm801_info *fm801 = ch->parent;
	int i;


	for (i = 0; i < 10 && fm801_rates[i].limit <= speed; i++) ;

	if(ch->dir == PCMDIR_PLAY) {
		fm801->pch.spd = fm801_rates[i].rate;
		fm801->play_shift = (i<<8);
		fm801->play_shift &= FM_PLAY_RATE_MASK;
	}

	if(ch->dir == PCMDIR_REC ) {
		fm801->rch.spd = fm801_rates[i].rate;
		fm801->rec_shift = (i<<8);
		fm801->rec_shift &= FM_REC_RATE_MASK;
	}

	ch->spd = fm801_rates[i].rate;

	return fm801_rates[i].rate;
}

static int
fm801ch_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	struct fm801_chinfo *ch = data;
	struct fm801_info *fm801 = ch->parent;

	if(ch->dir == PCMDIR_PLAY) {
		if(fm801->play_flip) return fm801->play_blksize;
		fm801->play_blksize = blocksize;
	}

	if(ch->dir == PCMDIR_REC) {
		if(fm801->rec_flip) return fm801->rec_blksize;
		fm801->rec_blksize = blocksize;
	}

	DPRINT("fm801ch_setblocksize %d (dir %d)\n",blocksize, ch->dir);

	return blocksize;
}

static int
fm801ch_trigger(kobj_t obj, void *data, int go)
{
	struct fm801_chinfo *ch = data;
	struct fm801_info *fm801 = ch->parent;
	u_int32_t baseaddr = vtophys(sndbuf_getbuf(ch->buffer));
	u_int32_t k1;

	DPRINT("fm801ch_trigger go %d , ", go);

	if (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD) {
		return 0;
	}

	if (ch->dir == PCMDIR_PLAY) {
		if (go == PCMTRIG_START) {

			fm801->play_start = baseaddr;
			fm801->play_nextblk = fm801->play_start + fm801->play_blksize;
			fm801->play_flip = 0;
			fm801_wr(fm801, FM_PLAY_DMALEN, fm801->play_blksize - 1, 2);
			fm801_wr(fm801, FM_PLAY_DMABUF1,fm801->play_start,4);
			fm801_wr(fm801, FM_PLAY_DMABUF2,fm801->play_nextblk,4);
			fm801_wr(fm801, FM_PLAY_CTL,
					FM_PLAY_START | FM_PLAY_STOPNOW | fm801->play_fmt | fm801->play_shift,
					2 );
			} else {
			fm801->play_flip = 0;
			k1 = fm801_rd(fm801, FM_PLAY_CTL,2);
			fm801_wr(fm801, FM_PLAY_CTL,
				(k1 & ~(FM_PLAY_STOPNOW | FM_PLAY_START)) |
				FM_PLAY_BUF1_LAST | FM_PLAY_BUF2_LAST, 2 );
		}
	} else if(ch->dir == PCMDIR_REC) {
		if (go == PCMTRIG_START) {
			fm801->rec_start = baseaddr;
			fm801->rec_nextblk = fm801->rec_start + fm801->rec_blksize;
			fm801->rec_flip = 0;
			fm801_wr(fm801, FM_REC_DMALEN, fm801->rec_blksize - 1, 2);
			fm801_wr(fm801, FM_REC_DMABUF1,fm801->rec_start,4);
			fm801_wr(fm801, FM_REC_DMABUF2,fm801->rec_nextblk,4);
			fm801_wr(fm801, FM_REC_CTL,
					FM_REC_START | FM_REC_STOPNOW | fm801->rec_fmt | fm801->rec_shift,
					2 );
			} else {
			fm801->rec_flip = 0;
			k1 = fm801_rd(fm801, FM_REC_CTL,2);
			fm801_wr(fm801, FM_REC_CTL,
				(k1 & ~(FM_REC_STOPNOW | FM_REC_START)) |
				FM_REC_BUF1_LAST | FM_REC_BUF2_LAST, 2);
		}
	}

	return 0;
}

/* Almost ALSA copy */
static int
fm801ch_getptr(kobj_t obj, void *data)
{
	struct fm801_chinfo *ch = data;
	struct fm801_info *fm801 = ch->parent;
	int result = 0;

	if (ch->dir == PCMDIR_PLAY) {
		result = fm801_rd(fm801,
			(fm801->play_flip&1) ?
			FM_PLAY_DMABUF2:FM_PLAY_DMABUF1, 4) - fm801->play_start;
	}

	if (ch->dir == PCMDIR_REC) {
		result = fm801_rd(fm801,
			(fm801->rec_flip&1) ?
			FM_REC_DMABUF2:FM_REC_DMABUF1, 4) - fm801->rec_start;
	}

	return result;
}

static struct pcmchan_caps *
fm801ch_getcaps(kobj_t obj, void *data)
{
	return &fm801ch_caps;
}

static kobj_method_t fm801ch_methods[] = {
    	KOBJMETHOD(channel_init,		fm801ch_init),
    	KOBJMETHOD(channel_setformat,		fm801ch_setformat),
    	KOBJMETHOD(channel_setspeed,		fm801ch_setspeed),
    	KOBJMETHOD(channel_setblocksize,	fm801ch_setblocksize),
    	KOBJMETHOD(channel_trigger,		fm801ch_trigger),
    	KOBJMETHOD(channel_getptr,		fm801ch_getptr),
    	KOBJMETHOD(channel_getcaps,		fm801ch_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(fm801ch);

/* -------------------------------------------------------------------- */

/*
 *  Init routine is taken from an original NetBSD driver
 */
static int
fm801_init(struct fm801_info *fm801)
{
	u_int32_t k1;

	/* reset codec */
	fm801_wr(fm801, FM_CODEC_CTL, 0x0020,2);
	DELAY(100000);
	fm801_wr(fm801, FM_CODEC_CTL, 0x0000,2);
	DELAY(100000);

	fm801_wr(fm801, FM_PCM_VOLUME, 0x0808,2);
	fm801_wr(fm801, FM_FM_VOLUME, 0x0808,2);
	fm801_wr(fm801, FM_I2S_VOLUME, 0x0808,2);
	fm801_wr(fm801, 0x40,0x107f,2);	/* enable legacy audio */

	fm801_wr((void *)fm801, FM_RECORD_SOURCE, 0x0000,2);

	/* Unmask playback, record and mpu interrupts, mask the rest */
	k1 = fm801_rd((void *)fm801, FM_INTMASK,2);
	fm801_wr(fm801, FM_INTMASK,
		(k1 & ~(FM_INTMASK_PLAY | FM_INTMASK_REC | FM_INTMASK_MPU)) |
		FM_INTMASK_VOL,2);
	fm801_wr(fm801, FM_INTSTATUS,
		FM_INTSTATUS_PLAY | FM_INTSTATUS_REC | FM_INTSTATUS_MPU |
		FM_INTSTATUS_VOL,2);

	DPRINT("FM801 init Ok\n");
	return 0;
}

static int
fm801_pci_attach(device_t dev)
{
	u_int32_t 		data;
	struct ac97_info 	*codec = 0;
	struct fm801_info 	*fm801;
	int 			i;
	int 			mapped = 0;
	char 			status[SND_STATUSLEN];

	if ((fm801 = (struct fm801_info *)malloc(sizeof(*fm801), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return ENXIO;
	}

	fm801->type = pci_get_devid(dev);

	data = pci_read_config(dev, PCIR_COMMAND, 2);
	data |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, data, 2);
	data = pci_read_config(dev, PCIR_COMMAND, 2);

	for (i = 0; (mapped == 0) && (i < PCI_MAXMAPS_0); i++) {
		fm801->regid = PCIR_MAPS + i*4;
		fm801->regtype = SYS_RES_MEMORY;
		fm801->reg = bus_alloc_resource(dev, fm801->regtype, &fm801->regid,
						0, ~0, 1, RF_ACTIVE);
		if(!fm801->reg)
		{
			fm801->regtype = SYS_RES_IOPORT;
			fm801->reg = bus_alloc_resource(dev, fm801->regtype, &fm801->regid,
						0, ~0, 1, RF_ACTIVE);
		}

		if(fm801->reg) {
			fm801->st = rman_get_bustag(fm801->reg);
			fm801->sh = rman_get_bushandle(fm801->reg);
			mapped++;
		}
	}

	if (mapped == 0) {
		device_printf(dev, "unable to map register space\n");
		goto oops;
	}

	fm801->bufsz = pcm_getbuffersize(dev, 4096, FM801_DEFAULT_BUFSZ, 65536);

	fm801_init(fm801);

	codec = AC97_CREATE(dev, fm801, fm801_ac97);
	if (codec == NULL) goto oops;

	if (mixer_init(dev, ac97_getmixerclass(), codec) == -1) goto oops;

	fm801->irqid = 0;
	fm801->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &fm801->irqid,
				0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!fm801->irq || snd_setup_intr(dev, fm801->irq, 0, fm801_intr, fm801, &fm801->ih)) {
		device_printf(dev, "unable to map interrupt\n");
		goto oops;
	}

	if (bus_dma_tag_create(/*parent*/NULL, /*alignment*/2, /*boundary*/0,
		/*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
		/*highaddr*/BUS_SPACE_MAXADDR,
		/*filter*/NULL, /*filterarg*/NULL,
		/*maxsize*/fm801->bufsz, /*nsegments*/1, /*maxsegz*/0x3ffff,
		/*flags*/0, &fm801->parent_dmat) != 0) {
		device_printf(dev, "unable to create dma tag\n");
		goto oops;
	}

	snprintf(status, 64, "at %s 0x%lx irq %ld",
		(fm801->regtype == SYS_RES_IOPORT)? "io" : "memory",
		rman_get_start(fm801->reg), rman_get_start(fm801->irq));

#define FM801_MAXPLAYCH	1
	if (pcm_register(dev, fm801, FM801_MAXPLAYCH, 1)) goto oops;
	pcm_addchan(dev, PCMDIR_PLAY, &fm801ch_class, fm801);
	pcm_addchan(dev, PCMDIR_REC, &fm801ch_class, fm801);
	pcm_setstatus(dev, status);

	fm801->radio = device_add_child(dev, "radio", -1);
	bus_generic_attach(dev);

	return 0;

oops:
	if (codec) ac97_destroy(codec);
	if (fm801->reg) bus_release_resource(dev, fm801->regtype, fm801->regid, fm801->reg);
	if (fm801->ih) bus_teardown_intr(dev, fm801->irq, fm801->ih);
	if (fm801->irq) bus_release_resource(dev, SYS_RES_IRQ, fm801->irqid, fm801->irq);
	if (fm801->parent_dmat) bus_dma_tag_destroy(fm801->parent_dmat);
	free(fm801, M_DEVBUF);
	return ENXIO;
}

static int
fm801_pci_detach(device_t dev)
{
	int r;
	struct fm801_info *fm801;

	DPRINT("Forte Media FM801 detach\n");

	fm801 = pcm_getdevinfo(dev);

	r = bus_generic_detach(dev);
	if (r)
		return r;
	if (fm801->radio != NULL) {
		r = device_delete_child(dev, fm801->radio);
		if (r)
			return r;
		fm801->radio = NULL;
	}

	r = pcm_unregister(dev);
	if (r)
		return r;

	bus_release_resource(dev, fm801->regtype, fm801->regid, fm801->reg);
	bus_teardown_intr(dev, fm801->irq, fm801->ih);
	bus_release_resource(dev, SYS_RES_IRQ, fm801->irqid, fm801->irq);
	bus_dma_tag_destroy(fm801->parent_dmat);
	free(fm801, M_DEVBUF);
	return 0;
}

static int
fm801_pci_probe( device_t dev )
{
	u_int32_t data;
	int id, regtype, regid, result;
	struct resource *reg;
	bus_space_tag_t st;
	bus_space_handle_t sh;

	result = ENXIO;
	
	if ((id = pci_get_devid(dev)) == PCI_DEVICE_FORTEMEDIA1 ) {
		data = pci_read_config(dev, PCIR_COMMAND, 2);
		data |= (PCIM_CMD_PORTEN|PCIM_CMD_BUSMASTEREN);
		pci_write_config(dev, PCIR_COMMAND, data, 2);
		data = pci_read_config(dev, PCIR_COMMAND, 2);

		regid = PCIR_MAPS;
		regtype = SYS_RES_IOPORT;
		reg = bus_alloc_resource(dev, regtype, &regid, 0, ~0, 1,
		    RF_ACTIVE);

		if (reg == NULL)
			return ENXIO;

		st = rman_get_bustag(reg);
		sh = rman_get_bushandle(reg);
		/*
		 * XXX: quick check that device actually has sound capabilities.
		 * The problem is that some cards built around FM801 chip only
		 * have radio tuner onboard, but no sound capabilities. There
		 * is no "official" way to quickly check this, because all
		 * IDs are exactly the same. The only difference is 0x28
		 * device control register, described in FM801 specification
		 * as "SRC/Mixer Test Control/DFC Status", but without
		 * any more detailed explanation. According to specs, and
		 * available sample cards (SF256-PCP-R and SF256-PCS-R) its
		 * power-on value should be `0', while on AC97-less tuner
		 * card (SF64-PCR) it was 0x80.
		 */
		/*
		 * HACK (on top of the hack above?)
		 * Unfortunately, some fully-sound-capable cards, like the
		 * TerraTec 512i, return 0x80.  I don't know of any other
		 * differences to detect between the cards.  As I have one
		 * of these cards but not an SF64-PCR, I'd prefer to have
		 * the card work, as would others with the same card.
		 * Therefore, not knowing better, disable this check.
		 */
#ifdef I_HAVE_SF64_PCR
		if (bus_space_read_1(st, sh, 0x28) == 0) {
#endif
			device_set_desc(dev,
			    "Forte Media FM801 Audio Controller");
			result = 0;
#ifdef I_HAVE_SF64_PCR
		}
#endif

		bus_release_resource(dev, regtype, regid, reg);
	}
/*
	if ((id = pci_get_devid(dev)) == PCI_DEVICE_FORTEMEDIA2 ) {
		device_set_desc(dev, "Forte Media FM801 Joystick (Not Supported)");
		return ENXIO;
	}
*/
	return (result);
}

static struct resource *
fm801_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     u_long start, u_long end, u_long count, u_int flags)
{
	struct fm801_info *fm801;

	fm801 = pcm_getdevinfo(bus);

	if (type == SYS_RES_IOPORT && *rid == PCIR_MAPS)
		return (fm801->reg);

	return (NULL);
}

static int
fm801_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	return (0);
}

static device_method_t fm801_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fm801_pci_probe),
	DEVMETHOD(device_attach,	fm801_pci_attach),
	DEVMETHOD(device_detach,	fm801_pci_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	fm801_alloc_resource),
	DEVMETHOD(bus_release_resource,	fm801_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	{ 0, 0}
};

static driver_t fm801_driver = {
	"pcm",
	fm801_methods,
	PCM_SOFTC_SIZE,
};

DRIVER_MODULE(snd_fm801, pci, fm801_driver, pcm_devclass, 0, 0);
MODULE_DEPEND(snd_fm801, snd_pcm, PCM_MINVER, PCM_PREFVER, PCM_MAXVER);
MODULE_VERSION(snd_fm801, 1);
