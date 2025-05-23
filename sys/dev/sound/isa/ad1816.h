/*
 * (C) 1997 Luigi Rizzo (luigi@iet.unipi.it)
 *
 * This file contains information and macro definitions for
 * the ad1816 chip
 *
 * $FreeBSD: src/sys/dev/sound/isa/ad1816.h,v 1.1.2.2 2002/04/22 15:49:30 cg Exp $
 * $DragonFly: src/sys/dev/sound/isa/ad1816.h,v 1.2 2003/06/17 04:28:30 dillon Exp $
 */

/* AD1816 register macros */

#define AD1816_ALE	0 	/* indirect reg access 		*/
#define AD1816_INT	1 	/* interupt status     		*/
#define AD1816_LOW	2 	/* indirect low byte   		*/
#define AD1816_HIGH	3 	/* indirect high byte  		*/

#if 0
#define ad1816_pioD(d) ((d)->io_base+4) /* PIO debug		*/
#define ad1816_pios(d) ((d)->io_base+5) /* PIO status		*/
#define ad1816_piod(d) ((d)->io_base+6) /* PIO data 		*/
#endif

/* values for playback/capture config:
   bits: 0   enable/disable
         1   pio/dma
         2   stereo/mono
         3   companded/linearPCM
         4-5 format : 00 8bit  linear (uncomp)
                      00 8bit  mulaw  (comp)
                      01 16bit le     (uncomp)
                      01 8bit  alaw   (comp)
                      11 16bit be     (uncomp)
*/

#define AD1816_PLAY	8 	/* playback config     		*/
#define AD1816_CAPT 	9	/* capture config      		*/

#define	AD1816_BUSY	0x80	/* chip is busy			*/
#define	AD1816_ALEMASK	0x3F	/* mask for indirect adr.	*/

#if 0
#define	AD1816_INTRSI	0x01	/* sb intr			*/
#define	AD1816_INTRGI	0x02	/* game intr			*/
#define	AD1816_INTRRI	0x04	/* ring intr			*/
#define	AD1816_INTRDI	0x08	/* dsp intr			*/
#define	AD1816_INTRVI	0x10	/* vol intr			*/
#define	AD1816_INTRTI	0x20 	/* timer intr			*/
#endif

#define	AD1816_INTRCI	0x40	/* capture intr			*/
#define	AD1816_INTRPI	0x80	/* playback intr		*/
/* PIO stuff is not supplied here */
/* playback / capture config      */
#define	AD1816_ENABLE	0x01	/* enable pl/cp			*/
#define	AD1816_PIO	0x02	/* use pio			*/
#define	AD1816_STEREO	0x04
#define	AD1816_COMP	0x08	/* data is companded		*/
#define	AD1816_U8	0x00	/* 8 bit linear pcm		*/
#define	AD1816_MULAW	0x08	/* 8 bit mulaw			*/
#define	AD1816_ALAW	0x18	/* 8 bit alaw			*/
#define	AD1816_S16LE	0x10	/* 16 bit linear little endian	*/
#define	AD1816_S16BE	0x30	/* 16 bit linear big endian	*/
#define	AD1816_FORMASK  0x38	/* format mask			*/

#define AD1816_REC_DEVICES	\
    (SOUND_MASK_LINE | SOUND_MASK_MIC | SOUND_MASK_CD)

#define AD1816_MIXER_DEVICES	\
    (SOUND_MASK_VOLUME | SOUND_MASK_PCM | SOUND_MASK_SYNTH | \
     SOUND_MASK_LINE   | SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_IGAIN)

