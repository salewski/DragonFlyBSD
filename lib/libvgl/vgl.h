/*-
 * Copyright (c) 1991-1997 S�ren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libvgl/vgl.h,v 1.5.2.1 2001/06/19 06:49:15 sobomax Exp $
 * $DragonFly: src/lib/libvgl/vgl.h,v 1.2 2003/06/17 04:26:52 dillon Exp $
 */

#ifndef _VGL_H_
#define _VGL_H_

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <machine/cpufunc.h>

typedef unsigned char byte;
typedef struct {
  byte 	Type;
  int  	Xsize, Ysize;
  int  	VXsize, VYsize;
  int   Xorigin, Yorigin;
  byte 	*Bitmap;
} VGLBitmap;

#define VGLBITMAP_INITIALIZER(t, x, y, bits)	\
	{ (t), (x), (y), (x), (y), 0, 0, (bits) }

/*
 * Defined Type's
 */
#define MEMBUF		0
#define VIDBUF4		1
#define VIDBUF8		2
#define VIDBUF8X	3
#define VIDBUF8S	4
#define VIDBUF4S	5
#define NOBUF		255

typedef struct VGLText {
  byte	Width, Height;
  byte	*BitmapArray;
} VGLText;

typedef struct VGLObject {
  int	  	Id;
  int	  	Type;
  int	  	Status;
  int	  	Xpos, Ypos;
  int	  	Xhot, Yhot;
  VGLBitmap 	*Image;
  VGLBitmap 	*Mask;
  int		(*CallBackFunction)();
} VGLObject;

#define MOUSE_IMG_SIZE		16
#define VGL_MOUSEHIDE		0
#define VGL_MOUSESHOW		1
#define VGL_MOUSEFREEZE		0
#define VGL_MOUSEUNFREEZE	1
#define VGL_DIR_RIGHT		0
#define VGL_DIR_UP		1
#define VGL_DIR_LEFT		2
#define VGL_DIR_DOWN		3
#define VGL_RAWKEYS		1
#define VGL_CODEKEYS		2
#define VGL_XLATEKEYS		3

extern video_adapter_info_t	VGLAdpInfo;
extern video_info_t		VGLModeInfo;
extern VGLBitmap 		*VGLDisplay;
extern byte 			*VGLBuf;

/*
 * Prototypes
 */
/* bitmap.c */
int __VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy, VGLBitmap *dst, int dstx, int dsty, int width, int hight);
int VGLBitmapCopy(VGLBitmap *src, int srcx, int srcy, VGLBitmap *dst, int dstx, int dsty, int width, int hight);
VGLBitmap *VGLBitmapCreate(int type, int xsize, int ysize, byte *bits);
void VGLBitmapDestroy(VGLBitmap *object);
int VGLBitmapAllocateBits(VGLBitmap *object);
/* keyboard.c */
int VGLKeyboardInit(int mode);
void VGLKeyboardEnd(void);
int VGLKeyboardGetCh(void);
/* main.c */
void VGLEnd(void);
int VGLInit(int mode);
void VGLCheckSwitch(void);
int VGLSetVScreenSize(VGLBitmap *object, int VXsize, int VYsize);
int VGLPanScreen(VGLBitmap *object, int x, int y);
int VGLSetSegment(unsigned int offset);
/* mouse.c */
void VGLMousePointerShow(void);
void VGLMousePointerHide(void);
void VGLMouseMode(int mode);
void VGLMouseAction(int dummy);
void VGLMouseSetImage(VGLBitmap *AndMask, VGLBitmap *OrMask);
void VGLMouseSetStdImage(void);
int VGLMouseInit(int mode);
int VGLMouseStatus(int *x, int *y, char *buttons);
int VGLMouseFreeze(int x, int y, int width, int hight, byte color);
void VGLMouseUnFreeze(void);
/* simple.c */
void VGLSetXY(VGLBitmap *object, int x, int y, byte color);
byte VGLGetXY(VGLBitmap *object, int x, int y);
void VGLLine(VGLBitmap *object, int x1, int y1, int x2, int y2, byte color);
void VGLBox(VGLBitmap *object, int x1, int y1, int x2, int y2, byte color);
void VGLFilledBox(VGLBitmap *object, int x1, int y1, int x2, int y2, byte color);
void VGLEllipse(VGLBitmap *object, int xc, int yc, int a, int b, byte color);
void VGLFilledEllipse(VGLBitmap *object, int xc, int yc, int a, int b, byte color);
void VGLClear(VGLBitmap *object, byte color);
void VGLRestorePalette(void);
void VGLSavePalette(void);
void VGLSetPalette(byte *red, byte *green, byte *blue);
void VGLSetPaletteIndex(byte color, byte red, byte green, byte blue);
void VGLSetBorder(byte color);
void VGLBlankDisplay(int blank);
/* text.c */
int VGLTextSetFontFile(char *filename);
void VGLBitmapPutChar(VGLBitmap *Object, int x, int y, byte ch, byte fgcol, byte bgcol, int fill, int dir);
void VGLBitmapString(VGLBitmap *Object, int x, int y, char *str, byte fgcol, byte bgcol, int fill, int dir);

#endif /* !_VGL_H_ */
