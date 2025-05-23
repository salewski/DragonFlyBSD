/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
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
 * $FreeBSD: src/lib/libc/locale/table.c,v 1.13.2.1 2000/06/04 21:47:39 ache Exp $
 * $DragonFly: src/lib/libcr/locale/Attic/table.c,v 1.4 2004/01/22 12:01:18 eirikn Exp $
 *
 * @(#)table.c	8.1 (Berkeley) 6/27/93
 */

#include <ctype.h>
#include <rune.h>
#include <stdlib.h>

extern rune_t	_none_sgetrune (const char *, size_t, char const **);
extern int	_none_sputrune (rune_t, char *, size_t, char **);
extern int	_none_init (char *, char **);

_RuneLocale _DefaultRuneLocale = {
    _RUNE_MAGIC_1,
    "NONE",
    _none_sgetrune,
    _none_sputrune,
    0xFFFD,

    {	/*00*/	_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
	/*08*/	_CTYPE_C,
		_CTYPE_C|_CTYPE_S|_CTYPE_B,
		_CTYPE_C|_CTYPE_S,
		_CTYPE_C|_CTYPE_S,
		_CTYPE_C|_CTYPE_S,
		_CTYPE_C|_CTYPE_S,
		_CTYPE_C,
		_CTYPE_C,
	/*10*/	_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
	/*18*/	_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
		_CTYPE_C,
	/*20*/	_CTYPE_S|_CTYPE_B|_CTYPE_R,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
	/*28*/	_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
	/*30*/	_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|0,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|1,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|2,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|3,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|4,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|5,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|6,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|7,
	/*38*/	_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|8,
		_CTYPE_D|_CTYPE_R|_CTYPE_G|_CTYPE_X|9,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
	/*40*/	_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|10,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|11,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|12,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|13,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|14,
		_CTYPE_U|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|15,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*48*/	_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*50*/	_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*58*/	_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_U|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
	/*60*/	_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|10,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|11,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|12,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|13,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|14,
		_CTYPE_L|_CTYPE_X|_CTYPE_R|_CTYPE_G|_CTYPE_A|15,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*68*/	_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*70*/	_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
	/*78*/	_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_L|_CTYPE_R|_CTYPE_G|_CTYPE_A,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_P|_CTYPE_R|_CTYPE_G,
		_CTYPE_C,
    },
    {	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
     	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
     	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
     	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
     	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
     	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
     	'x',	'y',	'z',	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
     	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
     	'x',	'y',	'z',	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
     	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
     	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
     	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
     	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
     	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
     	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
     	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
     	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff,
    },
    {	0x00,	0x01,	0x02,	0x03,	0x04,	0x05,	0x06,	0x07,
     	0x08,	0x09,	0x0a,	0x0b,	0x0c,	0x0d,	0x0e,	0x0f,
	0x10,	0x11,	0x12,	0x13,	0x14,	0x15,	0x16,	0x17,
     	0x18,	0x19,	0x1a,	0x1b,	0x1c,	0x1d,	0x1e,	0x1f,
	0x20,	0x21,	0x22,	0x23,	0x24,	0x25,	0x26,	0x27,
     	0x28,	0x29,	0x2a,	0x2b,	0x2c,	0x2d,	0x2e,	0x2f,
	0x30,	0x31,	0x32,	0x33,	0x34,	0x35,	0x36,	0x37,
     	0x38,	0x39,	0x3a,	0x3b,	0x3c,	0x3d,	0x3e,	0x3f,
	0x40,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
     	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
     	'X',	'Y',	'Z',	0x5b,	0x5c,	0x5d,	0x5e,	0x5f,
	0x60,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
     	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
     	'X',	'Y',	'Z',	0x7b,	0x7c,	0x7d,	0x7e,	0x7f,
	0x80,	0x81,	0x82,	0x83,	0x84,	0x85,	0x86,	0x87,
     	0x88,	0x89,	0x8a,	0x8b,	0x8c,	0x8d,	0x8e,	0x8f,
	0x90,	0x91,	0x92,	0x93,	0x94,	0x95,	0x96,	0x97,
     	0x98,	0x99,	0x9a,	0x9b,	0x9c,	0x9d,	0x9e,	0x9f,
	0xa0,	0xa1,	0xa2,	0xa3,	0xa4,	0xa5,	0xa6,	0xa7,
     	0xa8,	0xa9,	0xaa,	0xab,	0xac,	0xad,	0xae,	0xaf,
	0xb0,	0xb1,	0xb2,	0xb3,	0xb4,	0xb5,	0xb6,	0xb7,
     	0xb8,	0xb9,	0xba,	0xbb,	0xbc,	0xbd,	0xbe,	0xbf,
	0xc0,	0xc1,	0xc2,	0xc3,	0xc4,	0xc5,	0xc6,	0xc7,
     	0xc8,	0xc9,	0xca,	0xcb,	0xcc,	0xcd,	0xce,	0xcf,
	0xd0,	0xd1,	0xd2,	0xd3,	0xd4,	0xd5,	0xd6,	0xd7,
     	0xd8,	0xd9,	0xda,	0xdb,	0xdc,	0xdd,	0xde,	0xdf,
	0xe0,	0xe1,	0xe2,	0xe3,	0xe4,	0xe5,	0xe6,	0xe7,
     	0xe8,	0xe9,	0xea,	0xeb,	0xec,	0xed,	0xee,	0xef,
	0xf0,	0xf1,	0xf2,	0xf3,	0xf4,	0xf5,	0xf6,	0xf7,
     	0xf8,	0xf9,	0xfa,	0xfb,	0xfc,	0xfd,	0xfe,	0xff,
    },
};

_RuneLocale *_CurrentRuneLocale = &_DefaultRuneLocale;

int __mb_cur_max = 1;

char	*_PathLocale;
