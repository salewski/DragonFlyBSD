/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 * @(#)map.c	8.1 (Berkeley) 6/4/93
 * $DragonFly: src/lib/libedit/map.c,v 1.5 2004/10/25 19:38:45 drhodus Exp $
 */

/*
 * map.c: Editor function definitions
 */
#include "sys.h"
#include <stdlib.h>
#include "el.h"

#define N_KEYS 256

private void map_print_key	 (EditLine *, el_action_t *, char *);
private void map_print_some_keys (EditLine *, el_action_t *, int, int);
private void map_print_all_keys	 (EditLine *);
private void map_init_nls 	 (EditLine *);
private void map_init_meta	 (EditLine *);

/* keymap tables ; should be N_KEYS*sizeof(KEYCMD) bytes long */

private el_action_t  el_map_emacs[] = {
    /*   0 */	EM_SET_MARK,		/* ^@ */
    /*   1 */	ED_MOVE_TO_BEG,		/* ^A */
    /*   2 */	ED_PREV_CHAR,		/* ^B */
    /*   3 */	ED_TTY_SIGINT,		/* ^C */
    /*   4 */	EM_DELETE_OR_LIST,	/* ^D */
    /*   5 */	ED_MOVE_TO_END,		/* ^E */
    /*   6 */	ED_NEXT_CHAR,		/* ^F */
    /*   7 */	ED_UNASSIGNED,		/* ^G */
    /*   8 */	ED_DELETE_PREV_CHAR,	/* ^H */
    /*   9 */	ED_UNASSIGNED,		/* ^I */
    /*  10 */	ED_NEWLINE,		/* ^J */
    /*  11 */	ED_KILL_LINE,		/* ^K */
    /*  12 */	ED_CLEAR_SCREEN,	/* ^L */
    /*  13 */	ED_NEWLINE,		/* ^M */
    /*  14 */	ED_NEXT_HISTORY,	/* ^N */
    /*  15 */	ED_TTY_FLUSH_OUTPUT,	/* ^O */
    /*  16 */	ED_PREV_HISTORY,	/* ^P */
    /*  17 */	ED_TTY_START_OUTPUT,	/* ^Q */
    /*  18 */	ED_REDISPLAY,		/* ^R */
    /*  19 */	ED_TTY_STOP_OUTPUT,	/* ^S */
    /*  20 */	ED_TRANSPOSE_CHARS,	/* ^T */
    /*  21 */	EM_KILL_LINE,		/* ^U */
    /*  22 */	ED_QUOTED_INSERT,	/* ^V */
    /*  23 */	EM_KILL_REGION,		/* ^W */
    /*  24 */	ED_SEQUENCE_LEAD_IN,	/* ^X */
    /*  25 */	EM_YANK,		/* ^Y */
    /*  26 */	ED_TTY_SIGTSTP,		/* ^Z */
    /*  27 */	EM_META_NEXT,		/* ^[ */
    /*  28 */	ED_TTY_SIGQUIT,		/* ^\ */
    /*  29 */	ED_TTY_DSUSP,		/* ^] */
    /*  30 */	ED_UNASSIGNED,		/* ^^ */
    /*  31 */	ED_UNASSIGNED,		/* ^_ */
    /*  32 */	ED_INSERT,		/* SPACE */
    /*  33 */	ED_INSERT,		/* ! */
    /*  34 */	ED_INSERT,		/* " */
    /*  35 */	ED_INSERT,		/* # */
    /*  36 */	ED_INSERT,		/* $ */
    /*  37 */	ED_INSERT,		/* % */
    /*  38 */	ED_INSERT,		/* & */
    /*  39 */	ED_INSERT,		/* ' */
    /*  40 */	ED_INSERT,		/* ( */
    /*  41 */	ED_INSERT,		/* ) */
    /*  42 */	ED_INSERT,		/* * */
    /*  43 */	ED_INSERT,		/* + */
    /*  44 */	ED_INSERT,		/* , */
    /*  45 */	ED_INSERT,		/* - */
    /*  46 */	ED_INSERT,		/* . */
    /*  47 */	ED_INSERT,		/* / */
    /*  48 */	ED_DIGIT,		/* 0 */
    /*  49 */	ED_DIGIT,		/* 1 */
    /*  50 */	ED_DIGIT,		/* 2 */
    /*  51 */	ED_DIGIT,		/* 3 */
    /*  52 */	ED_DIGIT,		/* 4 */
    /*  53 */	ED_DIGIT,		/* 5 */
    /*  54 */	ED_DIGIT,		/* 6 */
    /*  55 */	ED_DIGIT,		/* 7 */
    /*  56 */	ED_DIGIT,		/* 8 */
    /*  57 */	ED_DIGIT,		/* 9 */
    /*  58 */	ED_INSERT,		/* : */
    /*  59 */	ED_INSERT,		/* ; */
    /*  60 */	ED_INSERT,		/* < */
    /*  61 */	ED_INSERT,		/* = */
    /*  62 */	ED_INSERT,		/* > */
    /*  63 */	ED_INSERT,		/* ? */
    /*  64 */	ED_INSERT,		/* @ */
    /*  65 */	ED_INSERT,		/* A */
    /*  66 */	ED_INSERT,		/* B */
    /*  67 */	ED_INSERT,		/* C */
    /*  68 */	ED_INSERT,		/* D */
    /*  69 */	ED_INSERT,		/* E */
    /*  70 */	ED_INSERT,		/* F */
    /*  71 */	ED_INSERT,		/* G */
    /*  72 */	ED_INSERT,		/* H */
    /*  73 */	ED_INSERT,		/* I */
    /*  74 */	ED_INSERT,		/* J */
    /*  75 */	ED_INSERT,		/* K */
    /*  76 */	ED_INSERT,		/* L */
    /*  77 */	ED_INSERT,		/* M */
    /*  78 */	ED_INSERT,		/* N */
    /*  79 */	ED_INSERT,		/* O */
    /*  80 */	ED_INSERT,		/* P */
    /*  81 */	ED_INSERT,		/* Q */
    /*  82 */	ED_INSERT,		/* R */
    /*  83 */	ED_INSERT,		/* S */
    /*  84 */	ED_INSERT,		/* T */
    /*  85 */	ED_INSERT,		/* U */
    /*  86 */	ED_INSERT,		/* V */
    /*  87 */	ED_INSERT,		/* W */
    /*  88 */	ED_INSERT,		/* X */
    /*  89 */	ED_INSERT,		/* Y */
    /*  90 */	ED_INSERT,		/* Z */
    /*  91 */	ED_INSERT,		/* [ */
    /*  92 */	ED_INSERT,		/* \ */
    /*  93 */	ED_INSERT,		/* ] */
    /*  94 */	ED_INSERT,		/* ^ */
    /*  95 */	ED_INSERT,		/* _ */
    /*  96 */	ED_INSERT,		/* ` */
    /*  97 */	ED_INSERT,		/* a */
    /*  98 */	ED_INSERT,		/* b */
    /*  99 */	ED_INSERT,		/* c */
    /* 100 */	ED_INSERT,		/* d */
    /* 101 */	ED_INSERT,		/* e */
    /* 102 */	ED_INSERT,		/* f */
    /* 103 */	ED_INSERT,		/* g */
    /* 104 */	ED_INSERT,		/* h */
    /* 105 */	ED_INSERT,		/* i */
    /* 106 */	ED_INSERT,		/* j */
    /* 107 */	ED_INSERT,		/* k */
    /* 108 */	ED_INSERT,		/* l */
    /* 109 */	ED_INSERT,		/* m */
    /* 110 */	ED_INSERT,		/* n */
    /* 111 */	ED_INSERT,		/* o */
    /* 112 */	ED_INSERT,		/* p */
    /* 113 */	ED_INSERT,		/* q */
    /* 114 */	ED_INSERT,		/* r */
    /* 115 */	ED_INSERT,		/* s */
    /* 116 */	ED_INSERT,		/* t */
    /* 117 */	ED_INSERT,		/* u */
    /* 118 */	ED_INSERT,		/* v */
    /* 119 */	ED_INSERT,		/* w */
    /* 120 */	ED_INSERT,		/* x */
    /* 121 */	ED_INSERT,		/* y */
    /* 122 */	ED_INSERT,		/* z */
    /* 123 */	ED_INSERT,		/* { */
    /* 124 */	ED_INSERT,		/* | */
    /* 125 */	ED_INSERT,		/* } */
    /* 126 */	ED_INSERT,		/* ~ */
    /* 127 */	ED_DELETE_PREV_CHAR,	/* ^? */
    /* 128 */	ED_UNASSIGNED,		/* M-^@ */
    /* 129 */	ED_UNASSIGNED,		/* M-^A */
    /* 130 */	ED_UNASSIGNED,		/* M-^B */
    /* 131 */	ED_UNASSIGNED,		/* M-^C */
    /* 132 */	ED_UNASSIGNED,		/* M-^D */
    /* 133 */	ED_UNASSIGNED,		/* M-^E */
    /* 134 */	ED_UNASSIGNED,		/* M-^F */
    /* 135 */	ED_UNASSIGNED,		/* M-^G */
    /* 136 */	ED_DELETE_PREV_WORD,	/* M-^H */
    /* 137 */	ED_UNASSIGNED,		/* M-^I */
    /* 138 */	ED_UNASSIGNED,		/* M-^J */
    /* 139 */	ED_UNASSIGNED,		/* M-^K */
    /* 140 */	ED_CLEAR_SCREEN,		/* M-^L */
    /* 141 */	ED_UNASSIGNED,		/* M-^M */
    /* 142 */	ED_UNASSIGNED,		/* M-^N */
    /* 143 */	ED_UNASSIGNED,		/* M-^O */
    /* 144 */	ED_UNASSIGNED,		/* M-^P */
    /* 145 */	ED_UNASSIGNED,		/* M-^Q */
    /* 146 */	ED_UNASSIGNED,		/* M-^R */
    /* 147 */	ED_UNASSIGNED,		/* M-^S */
    /* 148 */	ED_UNASSIGNED,		/* M-^T */
    /* 149 */	ED_UNASSIGNED,		/* M-^U */
    /* 150 */	ED_UNASSIGNED,		/* M-^V */
    /* 151 */	ED_UNASSIGNED,		/* M-^W */
    /* 152 */	ED_UNASSIGNED,		/* M-^X */
    /* 153 */	ED_UNASSIGNED,		/* M-^Y */
    /* 154 */	ED_UNASSIGNED,		/* M-^Z */
    /* 155 */	ED_UNASSIGNED,		/* M-^[ */
    /* 156 */	ED_UNASSIGNED,		/* M-^\ */
    /* 157 */	ED_UNASSIGNED,		/* M-^] */
    /* 158 */	ED_UNASSIGNED,		/* M-^^ */
    /* 159 */	EM_COPY_PREV_WORD,	/* M-^_ */
    /* 160 */	ED_UNASSIGNED,		/* M-SPACE */
    /* 161 */	ED_UNASSIGNED,		/* M-! */
    /* 162 */	ED_UNASSIGNED,		/* M-" */
    /* 163 */	ED_UNASSIGNED,		/* M-# */
    /* 164 */	ED_UNASSIGNED,		/* M-$ */
    /* 165 */	ED_UNASSIGNED,		/* M-% */
    /* 166 */	ED_UNASSIGNED,		/* M-& */
    /* 167 */	ED_UNASSIGNED,		/* M-' */
    /* 168 */	ED_UNASSIGNED,		/* M-( */
    /* 169 */	ED_UNASSIGNED,		/* M-) */
    /* 170 */	ED_UNASSIGNED,		/* M-* */
    /* 171 */	ED_UNASSIGNED,		/* M-+ */
    /* 172 */	ED_UNASSIGNED,		/* M-, */
    /* 173 */	ED_UNASSIGNED,		/* M-- */
    /* 174 */	ED_UNASSIGNED,		/* M-. */
    /* 175 */	ED_UNASSIGNED,		/* M-/ */
    /* 176 */	ED_ARGUMENT_DIGIT,	/* M-0 */
    /* 177 */	ED_ARGUMENT_DIGIT,	/* M-1 */
    /* 178 */	ED_ARGUMENT_DIGIT,	/* M-2 */
    /* 179 */	ED_ARGUMENT_DIGIT,	/* M-3 */
    /* 180 */	ED_ARGUMENT_DIGIT,	/* M-4 */
    /* 181 */	ED_ARGUMENT_DIGIT,	/* M-5 */
    /* 182 */	ED_ARGUMENT_DIGIT,	/* M-6 */
    /* 183 */	ED_ARGUMENT_DIGIT,	/* M-7 */
    /* 184 */	ED_ARGUMENT_DIGIT,	/* M-8 */
    /* 185 */	ED_ARGUMENT_DIGIT,	/* M-9 */
    /* 186 */	ED_UNASSIGNED,		/* M-: */
    /* 187 */	ED_UNASSIGNED,		/* M-; */
    /* 188 */	ED_UNASSIGNED,		/* M-< */
    /* 189 */	ED_UNASSIGNED,		/* M-= */
    /* 190 */	ED_UNASSIGNED,		/* M-> */
    /* 191 */	ED_UNASSIGNED,		/* M-? */
    /* 192 */	ED_UNASSIGNED,		/* M-@ */
    /* 193 */	ED_UNASSIGNED,		/* M-A */
    /* 194 */	ED_PREV_WORD,		/* M-B */
    /* 195 */	EM_CAPITOL_CASE,		/* M-C */
    /* 196 */	EM_DELETE_NEXT_WORD,	/* M-D */
    /* 197 */	ED_UNASSIGNED,		/* M-E */
    /* 198 */	EM_NEXT_WORD,		/* M-F */
    /* 199 */	ED_UNASSIGNED,		/* M-G */
    /* 200 */	ED_UNASSIGNED,		/* M-H */
    /* 201 */	ED_UNASSIGNED,		/* M-I */
    /* 202 */	ED_UNASSIGNED,		/* M-J */
    /* 203 */	ED_UNASSIGNED,		/* M-K */
    /* 204 */	EM_LOWER_CASE,		/* M-L */
    /* 205 */	ED_UNASSIGNED,		/* M-M */
    /* 206 */	ED_SEARCH_NEXT_HISTORY,	/* M-N */
    /* 207 */	ED_SEQUENCE_LEAD_IN,	/* M-O */
    /* 208 */	ED_SEARCH_PREV_HISTORY,	/* M-P */
    /* 209 */	ED_UNASSIGNED,		/* M-Q */
    /* 210 */	ED_UNASSIGNED,		/* M-R */
    /* 211 */	ED_UNASSIGNED,		/* M-S */
    /* 212 */	ED_UNASSIGNED,		/* M-T */
    /* 213 */	EM_UPPER_CASE,		/* M-U */
    /* 214 */	ED_UNASSIGNED,		/* M-V */
    /* 215 */	EM_COPY_REGION,		/* M-W */
    /* 216 */	ED_COMMAND,		/* M-X */
    /* 217 */	ED_UNASSIGNED,		/* M-Y */
    /* 218 */	ED_UNASSIGNED,		/* M-Z */
    /* 219 */	ED_SEQUENCE_LEAD_IN,	/* M-[ */
    /* 220 */	ED_UNASSIGNED,		/* M-\ */
    /* 221 */	ED_UNASSIGNED,		/* M-] */
    /* 222 */	ED_UNASSIGNED,		/* M-^ */
    /* 223 */	ED_UNASSIGNED,		/* M-_ */
    /* 223 */	ED_UNASSIGNED,		/* M-` */
    /* 224 */	ED_UNASSIGNED,		/* M-a */
    /* 225 */	ED_PREV_WORD,		/* M-b */
    /* 226 */	EM_CAPITOL_CASE,	/* M-c */
    /* 227 */	EM_DELETE_NEXT_WORD,	/* M-d */
    /* 228 */	ED_UNASSIGNED,		/* M-e */
    /* 229 */	EM_NEXT_WORD,		/* M-f */
    /* 230 */	ED_UNASSIGNED,		/* M-g */
    /* 231 */	ED_UNASSIGNED,		/* M-h */
    /* 232 */	ED_UNASSIGNED,		/* M-i */
    /* 233 */	ED_UNASSIGNED,		/* M-j */
    /* 234 */	ED_UNASSIGNED,		/* M-k */
    /* 235 */	EM_LOWER_CASE,		/* M-l */
    /* 236 */	ED_UNASSIGNED,		/* M-m */
    /* 237 */	ED_SEARCH_NEXT_HISTORY,	/* M-n */
    /* 238 */	ED_UNASSIGNED,		/* M-o */
    /* 239 */	ED_SEARCH_PREV_HISTORY,	/* M-p */
    /* 240 */	ED_UNASSIGNED,		/* M-q */
    /* 241 */	ED_UNASSIGNED,		/* M-r */
    /* 242 */	ED_UNASSIGNED,		/* M-s */
    /* 243 */	ED_UNASSIGNED,		/* M-t */
    /* 244 */	EM_UPPER_CASE,		/* M-u */
    /* 245 */	ED_UNASSIGNED,		/* M-v */
    /* 246 */	EM_COPY_REGION,		/* M-w */
    /* 247 */	ED_COMMAND,		/* M-x */
    /* 248 */	ED_UNASSIGNED,		/* M-y */
    /* 249 */	ED_UNASSIGNED,		/* M-z */
    /* 250 */	ED_UNASSIGNED,		/* M-{ */
    /* 251 */	ED_UNASSIGNED,		/* M-| */
    /* 252 */	ED_UNASSIGNED,		/* M-} */
    /* 253 */	ED_UNASSIGNED,		/* M-~ */
    /* 254 */	ED_DELETE_PREV_WORD		/* M-^? */
    /* 255 */
};

/*
 * keymap table for vi.  Each index into above tbl; should be
 * N_KEYS entries long.  Vi mode uses a sticky-extend to do command mode:
 * insert mode characters are in the normal keymap, and command mode
 * in the extended keymap.
 */
private el_action_t  el_map_vi_insert[] = {
#ifdef KSHVI
    /*   0 */	ED_UNASSIGNED,		/* ^@ */
    /*   1 */	ED_INSERT,		/* ^A */
    /*   2 */	ED_INSERT,		/* ^B */
    /*   3 */	ED_INSERT,		/* ^C */
    /*   4 */	VI_LIST_OR_EOF,		/* ^D */
    /*   5 */	ED_INSERT,		/* ^E */
    /*   6 */	ED_INSERT,		/* ^F */
    /*   7 */	ED_INSERT,		/* ^G */
    /*   8 */	VI_DELETE_PREV_CHAR,	/* ^H */   /* BackSpace key */
    /*   9 */	ED_INSERT,		/* ^I */   /* Tab Key  */
    /*  10 */	ED_NEWLINE,		/* ^J */
    /*  11 */	ED_INSERT,		/* ^K */
    /*  12 */	ED_INSERT,		/* ^L */
    /*  13 */	ED_NEWLINE,		/* ^M */
    /*  14 */	ED_INSERT,		/* ^N */
    /*  15 */	ED_INSERT,		/* ^O */
    /*  16 */	ED_INSERT,		/* ^P */
    /*  17 */	ED_TTY_START_OUTPUT,	/* ^Q */
    /*  18 */	ED_INSERT,		/* ^R */
    /*  19 */	ED_TTY_STOP_OUTPUT,	/* ^S */
    /*  20 */	ED_INSERT,		/* ^T */
    /*  21 */	VI_KILL_LINE_PREV,	/* ^U */
    /*  22 */	ED_QUOTED_INSERT,	/* ^V */
    /*  23 */	ED_DELETE_PREV_WORD,	/* ^W */  /* Only until strt edit pos */
    /*  24 */	ED_INSERT,		/* ^X */
    /*  25 */	ED_INSERT,		/* ^Y */
    /*  26 */	ED_INSERT,		/* ^Z */
    /*  27 */	VI_COMMAND_MODE,	/* ^[ */  /* [ Esc ] key */
    /*  28 */	ED_TTY_SIGQUIT,		/* ^\ */
    /*  29 */	ED_INSERT,		/* ^] */
    /*  30 */	ED_INSERT,		/* ^^ */
    /*  31 */	ED_INSERT,		/* ^_ */
#else /* !KSHVI */
    /*   0 */	ED_UNASSIGNED,		/* ^@ */   /* NOTE: These mappings do */
    /*   1 */	ED_MOVE_TO_BEG,		/* ^A */   /* NOT Correspond well to  */
    /*   2 */	ED_PREV_CHAR,		/* ^B */   /* the KSH VI editing as-  */
    /*   3 */	ED_TTY_SIGINT,		/* ^C */   /* signments. On the other */
    /*   4 */	VI_LIST_OR_EOF,		/* ^D */   /* hand they are convenient*/
    /*   5 */	ED_MOVE_TO_END,		/* ^E */   /* and many people have    */
    /*   6 */	ED_NEXT_CHAR,		/* ^F */   /* have gotten used to them*/
    /*   7 */	ED_UNASSIGNED,		/* ^G */
    /*   8 */	ED_DELETE_PREV_CHAR,	/* ^H */   /* BackSpace key */
    /*   9 */	ED_UNASSIGNED,		/* ^I */   /* Tab Key */
    /*  10 */	ED_NEWLINE,		/* ^J */
    /*  11 */	ED_KILL_LINE,		/* ^K */
    /*  12 */	ED_CLEAR_SCREEN,	/* ^L */
    /*  13 */	ED_NEWLINE,		/* ^M */
    /*  14 */	ED_NEXT_HISTORY,	/* ^N */
    /*  15 */	ED_TTY_FLUSH_OUTPUT,	/* ^O */
    /*  16 */	ED_PREV_HISTORY,	/* ^P */
    /*  17 */	ED_TTY_START_OUTPUT,	/* ^Q */
    /*  18 */	ED_REDISPLAY,		/* ^R */
    /*  19 */	ED_TTY_STOP_OUTPUT,	/* ^S */
    /*  20 */	ED_TRANSPOSE_CHARS,	/* ^T */
    /*  21 */	VI_KILL_LINE_PREV,	/* ^U */
    /*  22 */	ED_QUOTED_INSERT,	/* ^V */
    /*  23 */	ED_DELETE_PREV_WORD,	/* ^W */
    /*  24 */	ED_UNASSIGNED,		/* ^X */
    /*  25 */	ED_TTY_DSUSP,		/* ^Y */
    /*  26 */	ED_TTY_SIGTSTP,		/* ^Z */
    /*  27 */	VI_COMMAND_MODE,	/* ^[ */
    /*  28 */	ED_TTY_SIGQUIT,		/* ^\ */
    /*  29 */	ED_UNASSIGNED,		/* ^] */
    /*  30 */	ED_UNASSIGNED,		/* ^^ */
    /*  31 */	ED_UNASSIGNED,		/* ^_ */
#endif  /* KSHVI */
    /*  32 */	ED_INSERT,		/* SPACE */
    /*  33 */	ED_INSERT,		/* ! */
    /*  34 */	ED_INSERT,		/* " */
    /*  35 */	ED_INSERT,		/* # */
    /*  36 */	ED_INSERT,		/* $ */
    /*  37 */	ED_INSERT,		/* % */
    /*  38 */	ED_INSERT,		/* & */
    /*  39 */	ED_INSERT,		/* ' */
    /*  40 */	ED_INSERT,		/* ( */
    /*  41 */	ED_INSERT,		/* ) */
    /*  42 */	ED_INSERT,		/* * */
    /*  43 */	ED_INSERT,		/* + */
    /*  44 */	ED_INSERT,		/* , */
    /*  45 */	ED_INSERT,		/* - */
    /*  46 */	ED_INSERT,		/* . */
    /*  47 */	ED_INSERT,		/* / */
    /*  48 */	ED_INSERT,		/* 0 */
    /*  49 */	ED_INSERT,		/* 1 */
    /*  50 */	ED_INSERT,		/* 2 */
    /*  51 */	ED_INSERT,		/* 3 */
    /*  52 */	ED_INSERT,		/* 4 */
    /*  53 */	ED_INSERT,		/* 5 */
    /*  54 */	ED_INSERT,		/* 6 */
    /*  55 */	ED_INSERT,		/* 7 */
    /*  56 */	ED_INSERT,		/* 8 */
    /*  57 */	ED_INSERT,		/* 9 */
    /*  58 */	ED_INSERT,		/* : */
    /*  59 */	ED_INSERT,		/* ; */
    /*  60 */	ED_INSERT,		/* < */
    /*  61 */	ED_INSERT,		/* = */
    /*  62 */	ED_INSERT,		/* > */
    /*  63 */	ED_INSERT,		/* ? */
    /*  64 */	ED_INSERT,		/* @ */
    /*  65 */	ED_INSERT,		/* A */
    /*  66 */	ED_INSERT,		/* B */
    /*  67 */	ED_INSERT,		/* C */
    /*  68 */	ED_INSERT,		/* D */
    /*  69 */	ED_INSERT,		/* E */
    /*  70 */	ED_INSERT,		/* F */
    /*  71 */	ED_INSERT,		/* G */
    /*  72 */	ED_INSERT,		/* H */
    /*  73 */	ED_INSERT,		/* I */
    /*  74 */	ED_INSERT,		/* J */
    /*  75 */	ED_INSERT,		/* K */
    /*  76 */	ED_INSERT,		/* L */
    /*  77 */	ED_INSERT,		/* M */
    /*  78 */	ED_INSERT,		/* N */
    /*  79 */	ED_INSERT,		/* O */
    /*  80 */	ED_INSERT,		/* P */
    /*  81 */	ED_INSERT,		/* Q */
    /*  82 */	ED_INSERT,		/* R */
    /*  83 */	ED_INSERT,		/* S */
    /*  84 */	ED_INSERT,		/* T */
    /*  85 */	ED_INSERT,		/* U */
    /*  86 */	ED_INSERT,		/* V */
    /*  87 */	ED_INSERT,		/* W */
    /*  88 */	ED_INSERT,		/* X */
    /*  89 */	ED_INSERT,		/* Y */
    /*  90 */	ED_INSERT,		/* Z */
    /*  91 */	ED_INSERT,		/* [ */
    /*  92 */	ED_INSERT,		/* \ */
    /*  93 */	ED_INSERT,		/* ] */
    /*  94 */	ED_INSERT,		/* ^ */
    /*  95 */	ED_INSERT,		/* _ */
    /*  96 */	ED_INSERT,		/* ` */
    /*  97 */	ED_INSERT,		/* a */
    /*  98 */	ED_INSERT,		/* b */
    /*  99 */	ED_INSERT,		/* c */
    /* 100 */	ED_INSERT,		/* d */
    /* 101 */	ED_INSERT,		/* e */
    /* 102 */	ED_INSERT,		/* f */
    /* 103 */	ED_INSERT,		/* g */
    /* 104 */	ED_INSERT,		/* h */
    /* 105 */	ED_INSERT,		/* i */
    /* 106 */	ED_INSERT,		/* j */
    /* 107 */	ED_INSERT,		/* k */
    /* 108 */	ED_INSERT,		/* l */
    /* 109 */	ED_INSERT,		/* m */
    /* 110 */	ED_INSERT,		/* n */
    /* 111 */	ED_INSERT,		/* o */
    /* 112 */	ED_INSERT,		/* p */
    /* 113 */	ED_INSERT,		/* q */
    /* 114 */	ED_INSERT,		/* r */
    /* 115 */	ED_INSERT,		/* s */
    /* 116 */	ED_INSERT,		/* t */
    /* 117 */	ED_INSERT,		/* u */
    /* 118 */	ED_INSERT,		/* v */
    /* 119 */	ED_INSERT,		/* w */
    /* 120 */	ED_INSERT,		/* x */
    /* 121 */	ED_INSERT,		/* y */
    /* 122 */	ED_INSERT,		/* z */
    /* 123 */	ED_INSERT,		/* { */
    /* 124 */	ED_INSERT,		/* | */
    /* 125 */	ED_INSERT,		/* } */
    /* 126 */	ED_INSERT,		/* ~ */
    /* 127 */	ED_DELETE_PREV_CHAR,	/* ^? */
    /* 128 */	ED_UNASSIGNED,		/* M-^@ */
    /* 129 */	ED_UNASSIGNED,		/* M-^A */
    /* 130 */	ED_UNASSIGNED,		/* M-^B */
    /* 131 */	ED_UNASSIGNED,		/* M-^C */
    /* 132 */	ED_UNASSIGNED,		/* M-^D */
    /* 133 */	ED_UNASSIGNED,		/* M-^E */
    /* 134 */	ED_UNASSIGNED,		/* M-^F */
    /* 135 */	ED_UNASSIGNED,		/* M-^G */
    /* 136 */	ED_UNASSIGNED,		/* M-^H */
    /* 137 */	ED_UNASSIGNED,		/* M-^I */
    /* 138 */	ED_UNASSIGNED,		/* M-^J */
    /* 139 */	ED_UNASSIGNED,		/* M-^K */
    /* 140 */	ED_UNASSIGNED,		/* M-^L */
    /* 141 */	ED_UNASSIGNED,		/* M-^M */
    /* 142 */	ED_UNASSIGNED,		/* M-^N */
    /* 143 */	ED_UNASSIGNED,		/* M-^O */
    /* 144 */	ED_UNASSIGNED,		/* M-^P */
    /* 145 */	ED_UNASSIGNED,		/* M-^Q */
    /* 146 */	ED_UNASSIGNED,		/* M-^R */
    /* 147 */	ED_UNASSIGNED,		/* M-^S */
    /* 148 */	ED_UNASSIGNED,		/* M-^T */
    /* 149 */	ED_UNASSIGNED,		/* M-^U */
    /* 150 */	ED_UNASSIGNED,		/* M-^V */
    /* 151 */	ED_UNASSIGNED,		/* M-^W */
    /* 152 */	ED_UNASSIGNED,		/* M-^X */
    /* 153 */	ED_UNASSIGNED,		/* M-^Y */
    /* 154 */	ED_UNASSIGNED,		/* M-^Z */
    /* 155 */	ED_UNASSIGNED,		/* M-^[ */
    /* 156 */	ED_UNASSIGNED,		/* M-^\ */
    /* 157 */	ED_UNASSIGNED,		/* M-^] */
    /* 158 */	ED_UNASSIGNED,		/* M-^^ */
    /* 159 */	ED_UNASSIGNED,		/* M-^_ */
    /* 160 */	ED_UNASSIGNED,		/* M-SPACE */
    /* 161 */	ED_UNASSIGNED,		/* M-! */
    /* 162 */	ED_UNASSIGNED,		/* M-" */
    /* 163 */	ED_UNASSIGNED,		/* M-# */
    /* 164 */	ED_UNASSIGNED,		/* M-$ */
    /* 165 */	ED_UNASSIGNED,		/* M-% */
    /* 166 */	ED_UNASSIGNED,		/* M-& */
    /* 167 */	ED_UNASSIGNED,		/* M-' */
    /* 168 */	ED_UNASSIGNED,		/* M-( */
    /* 169 */	ED_UNASSIGNED,		/* M-) */
    /* 170 */	ED_UNASSIGNED,		/* M-* */
    /* 171 */	ED_UNASSIGNED,		/* M-+ */
    /* 172 */	ED_UNASSIGNED,		/* M-, */
    /* 173 */	ED_UNASSIGNED,		/* M-- */
    /* 174 */	ED_UNASSIGNED,		/* M-. */
    /* 175 */	ED_UNASSIGNED,		/* M-/ */
    /* 176 */	ED_UNASSIGNED,		/* M-0 */
    /* 177 */	ED_UNASSIGNED,		/* M-1 */
    /* 178 */	ED_UNASSIGNED,		/* M-2 */
    /* 179 */	ED_UNASSIGNED,		/* M-3 */
    /* 180 */	ED_UNASSIGNED,		/* M-4 */
    /* 181 */	ED_UNASSIGNED,		/* M-5 */
    /* 182 */	ED_UNASSIGNED,		/* M-6 */
    /* 183 */	ED_UNASSIGNED,		/* M-7 */
    /* 184 */	ED_UNASSIGNED,		/* M-8 */
    /* 185 */	ED_UNASSIGNED,		/* M-9 */
    /* 186 */	ED_UNASSIGNED,		/* M-: */
    /* 187 */	ED_UNASSIGNED,		/* M-; */
    /* 188 */	ED_UNASSIGNED,		/* M-< */
    /* 189 */	ED_UNASSIGNED,		/* M-= */
    /* 190 */	ED_UNASSIGNED,		/* M-> */
    /* 191 */	ED_UNASSIGNED,		/* M-? */
    /* 192 */	ED_UNASSIGNED,		/* M-@ */
    /* 193 */	ED_UNASSIGNED,		/* M-A */
    /* 194 */	ED_UNASSIGNED,		/* M-B */
    /* 195 */	ED_UNASSIGNED,		/* M-C */
    /* 196 */	ED_UNASSIGNED,		/* M-D */
    /* 197 */	ED_UNASSIGNED,		/* M-E */
    /* 198 */	ED_UNASSIGNED,		/* M-F */
    /* 199 */	ED_UNASSIGNED,		/* M-G */
    /* 200 */	ED_UNASSIGNED,		/* M-H */
    /* 201 */	ED_UNASSIGNED,		/* M-I */
    /* 202 */	ED_UNASSIGNED,		/* M-J */
    /* 203 */	ED_UNASSIGNED,		/* M-K */
    /* 204 */	ED_UNASSIGNED,		/* M-L */
    /* 205 */	ED_UNASSIGNED,		/* M-M */
    /* 206 */	ED_UNASSIGNED,		/* M-N */
    /* 207 */	ED_UNASSIGNED,		/* M-O */
    /* 208 */	ED_UNASSIGNED,		/* M-P */
    /* 209 */	ED_UNASSIGNED,		/* M-Q */
    /* 210 */	ED_UNASSIGNED,		/* M-R */
    /* 211 */	ED_UNASSIGNED,		/* M-S */
    /* 212 */	ED_UNASSIGNED,		/* M-T */
    /* 213 */	ED_UNASSIGNED,		/* M-U */
    /* 214 */	ED_UNASSIGNED,		/* M-V */
    /* 215 */	ED_UNASSIGNED,		/* M-W */
    /* 216 */	ED_UNASSIGNED,		/* M-X */
    /* 217 */	ED_UNASSIGNED,		/* M-Y */
    /* 218 */	ED_UNASSIGNED,		/* M-Z */
    /* 219 */	ED_UNASSIGNED,		/* M-[ */
    /* 220 */	ED_UNASSIGNED,		/* M-\ */
    /* 221 */	ED_UNASSIGNED,		/* M-] */
    /* 222 */	ED_UNASSIGNED,		/* M-^ */
    /* 223 */	ED_UNASSIGNED,		/* M-_ */
    /* 224 */	ED_UNASSIGNED,		/* M-` */
    /* 225 */	ED_UNASSIGNED,		/* M-a */
    /* 226 */	ED_UNASSIGNED,		/* M-b */
    /* 227 */	ED_UNASSIGNED,		/* M-c */
    /* 228 */	ED_UNASSIGNED,		/* M-d */
    /* 229 */	ED_UNASSIGNED,		/* M-e */
    /* 230 */	ED_UNASSIGNED,		/* M-f */
    /* 231 */	ED_UNASSIGNED,		/* M-g */
    /* 232 */	ED_UNASSIGNED,		/* M-h */
    /* 233 */	ED_UNASSIGNED,		/* M-i */
    /* 234 */	ED_UNASSIGNED,		/* M-j */
    /* 235 */	ED_UNASSIGNED,		/* M-k */
    /* 236 */	ED_UNASSIGNED,		/* M-l */
    /* 237 */	ED_UNASSIGNED,		/* M-m */
    /* 238 */	ED_UNASSIGNED,		/* M-n */
    /* 239 */	ED_UNASSIGNED,		/* M-o */
    /* 240 */	ED_UNASSIGNED,		/* M-p */
    /* 241 */	ED_UNASSIGNED,		/* M-q */
    /* 242 */	ED_UNASSIGNED,		/* M-r */
    /* 243 */	ED_UNASSIGNED,		/* M-s */
    /* 244 */	ED_UNASSIGNED,		/* M-t */
    /* 245 */	ED_UNASSIGNED,		/* M-u */
    /* 246 */	ED_UNASSIGNED,		/* M-v */
    /* 247 */	ED_UNASSIGNED,		/* M-w */
    /* 248 */	ED_UNASSIGNED,		/* M-x */
    /* 249 */	ED_UNASSIGNED,		/* M-y */
    /* 250 */	ED_UNASSIGNED,		/* M-z */
    /* 251 */	ED_UNASSIGNED,		/* M-{ */
    /* 252 */	ED_UNASSIGNED,		/* M-| */
    /* 253 */	ED_UNASSIGNED,		/* M-} */
    /* 254 */	ED_UNASSIGNED,		/* M-~ */
    /* 255 */	ED_UNASSIGNED		/* M-^? */
};

private el_action_t  el_map_vi_command[] = {
    /*   0 */	ED_UNASSIGNED,		/* ^@ */
    /*   1 */	ED_MOVE_TO_BEG,		/* ^A */
    /*   2 */	ED_UNASSIGNED,		/* ^B */
    /*   3 */	ED_TTY_SIGINT,		/* ^C */
    /*   4 */	ED_UNASSIGNED,		/* ^D */
    /*   5 */	ED_MOVE_TO_END,		/* ^E */
    /*   6 */	ED_UNASSIGNED,		/* ^F */
    /*   7 */	ED_UNASSIGNED,		/* ^G */
    /*   8 */	ED_PREV_CHAR,		/* ^H */
    /*   9 */	ED_UNASSIGNED,		/* ^I */
    /*  10 */	ED_NEWLINE,		/* ^J */
    /*  11 */	ED_KILL_LINE,		/* ^K */
    /*  12 */	ED_CLEAR_SCREEN,	/* ^L */
    /*  13 */	ED_NEWLINE,		/* ^M */
    /*  14 */	ED_NEXT_HISTORY,	/* ^N */
    /*  15 */	ED_TTY_FLUSH_OUTPUT,	/* ^O */
    /*  16 */	ED_PREV_HISTORY,	/* ^P */
    /*  17 */	ED_TTY_START_OUTPUT,	/* ^Q */
    /*  18 */	ED_REDISPLAY,		/* ^R */
    /*  19 */	ED_TTY_STOP_OUTPUT,	/* ^S */
    /*  20 */	ED_UNASSIGNED,		/* ^T */
    /*  21 */	VI_KILL_LINE_PREV,	/* ^U */
    /*  22 */	ED_UNASSIGNED,		/* ^V */
    /*  23 */	ED_DELETE_PREV_WORD,	/* ^W */
    /*  24 */	ED_UNASSIGNED,		/* ^X */
    /*  25 */	ED_UNASSIGNED,		/* ^Y */
    /*  26 */	ED_UNASSIGNED,		/* ^Z */
    /*  27 */	EM_META_NEXT,		/* ^[ */
    /*  28 */	ED_TTY_SIGQUIT,		/* ^\ */
    /*  29 */	ED_UNASSIGNED,		/* ^] */
    /*  30 */	ED_UNASSIGNED,		/* ^^ */
    /*  31 */	ED_UNASSIGNED,		/* ^_ */
    /*  32 */	ED_NEXT_CHAR,		/* SPACE */
    /*  33 */	ED_UNASSIGNED,		/* ! */
    /*  34 */	ED_UNASSIGNED,		/* " */
    /*  35 */	ED_UNASSIGNED,		/* # */
    /*  36 */	ED_MOVE_TO_END,		/* $ */
    /*  37 */	ED_UNASSIGNED,		/* % */
    /*  38 */	ED_UNASSIGNED,		/* & */
    /*  39 */	ED_UNASSIGNED,		/* ' */
    /*  40 */	ED_UNASSIGNED,		/* ( */
    /*  41 */	ED_UNASSIGNED,		/* ) */
    /*  42 */	ED_UNASSIGNED,		/* * */
    /*  43 */	ED_NEXT_HISTORY,	/* + */
    /*  44 */	VI_REPEAT_PREV_CHAR,	/* , */
    /*  45 */	ED_PREV_HISTORY,	/* - */
    /*  46 */	ED_UNASSIGNED,		/* . */
    /*  47 */	VI_SEARCH_PREV,		/* / */
    /*  48 */	VI_ZERO,		/* 0 */
    /*  49 */	ED_ARGUMENT_DIGIT,	/* 1 */
    /*  50 */	ED_ARGUMENT_DIGIT,	/* 2 */
    /*  51 */	ED_ARGUMENT_DIGIT,	/* 3 */
    /*  52 */	ED_ARGUMENT_DIGIT,	/* 4 */
    /*  53 */	ED_ARGUMENT_DIGIT,	/* 5 */
    /*  54 */	ED_ARGUMENT_DIGIT,	/* 6 */
    /*  55 */	ED_ARGUMENT_DIGIT,	/* 7 */
    /*  56 */	ED_ARGUMENT_DIGIT,	/* 8 */
    /*  57 */	ED_ARGUMENT_DIGIT,	/* 9 */
    /*  58 */	ED_COMMAND,		/* : */
    /*  59 */	VI_REPEAT_NEXT_CHAR,	/* ; */
    /*  60 */	ED_UNASSIGNED,		/* < */
    /*  61 */	ED_UNASSIGNED,		/* = */
    /*  62 */	ED_UNASSIGNED,		/* > */
    /*  63 */	VI_SEARCH_NEXT,		/* ? */
    /*  64 */	ED_UNASSIGNED,		/* @ */
    /*  65 */	VI_ADD_AT_EOL,		/* A */
    /*  66 */	VI_PREV_SPACE_WORD,	/* B */
    /*  67 */	VI_CHANGE_TO_EOL,	/* C */
    /*  68 */	ED_KILL_LINE,		/* D */
    /*  69 */	VI_TO_END_WORD,		/* E */
    /*  70 */	VI_PREV_CHAR,		/* F */
    /*  71 */	ED_UNASSIGNED,		/* G */
    /*  72 */	ED_UNASSIGNED,		/* H */
    /*  73 */	VI_INSERT_AT_BOL,	/* I */
    /*  74 */	ED_SEARCH_NEXT_HISTORY,	/* J */
    /*  75 */	ED_SEARCH_PREV_HISTORY,	/* K */
    /*  76 */	ED_UNASSIGNED,		/* L */
    /*  77 */	ED_UNASSIGNED,		/* M */
    /*  78 */	VI_REPEAT_SEARCH_PREV,	/* N */
    /*  79 */	ED_SEQUENCE_LEAD_IN,	/* O */
    /*  80 */	VI_PASTE_PREV,		/* P */
    /*  81 */	ED_UNASSIGNED,		/* Q */
    /*  82 */	VI_REPLACE_MODE,	/* R */
    /*  83 */	VI_SUBSTITUTE_LINE,	/* S */
    /*  84 */	VI_TO_PREV_CHAR,	/* T */
    /*  85 */	VI_UNDO_LINE,		/* U */
    /*  86 */	ED_UNASSIGNED,		/* V */
    /*  87 */	VI_NEXT_SPACE_WORD,	/* W */
    /*  88 */	ED_DELETE_PREV_CHAR,	/* X */
    /*  89 */	ED_UNASSIGNED,		/* Y */
    /*  90 */	ED_UNASSIGNED,		/* Z */
    /*  91 */	ED_SEQUENCE_LEAD_IN,	/* [ */
    /*  92 */	ED_UNASSIGNED,		/* \ */
    /*  93 */	ED_UNASSIGNED,		/* ] */
    /*  94 */	ED_MOVE_TO_BEG,		/* ^ */
    /*  95 */	ED_UNASSIGNED,		/* _ */
    /*  96 */	ED_UNASSIGNED,		/* ` */
    /*  97 */	VI_ADD,			/* a */
    /*  98 */	VI_PREV_WORD,		/* b */
    /*  99 */	VI_CHANGE_META,		/* c */
    /* 100 */	VI_DELETE_META,		/* d */
    /* 101 */	VI_END_WORD,		/* e */
    /* 102 */	VI_NEXT_CHAR,		/* f */
    /* 103 */	ED_UNASSIGNED,		/* g */
    /* 104 */	ED_PREV_CHAR,		/* h */
    /* 105 */	VI_INSERT,		/* i */
    /* 106 */	ED_NEXT_HISTORY,	/* j */
    /* 107 */	ED_PREV_HISTORY,	/* k */
    /* 108 */	ED_NEXT_CHAR,		/* l */
    /* 109 */	ED_UNASSIGNED,		/* m */
    /* 110 */	VI_REPEAT_SEARCH_NEXT,	/* n */
    /* 111 */	ED_UNASSIGNED,		/* o */
    /* 112 */	VI_PASTE_NEXT,		/* p */
    /* 113 */	ED_UNASSIGNED,		/* q */
    /* 114 */	VI_REPLACE_CHAR,	/* r */
    /* 115 */	VI_SUBSTITUTE_CHAR,	/* s */
    /* 116 */	VI_TO_NEXT_CHAR,	/* t */
    /* 117 */	VI_UNDO,		/* u */
    /* 118 */	ED_UNASSIGNED,		/* v */
    /* 119 */	VI_NEXT_WORD,		/* w */
    /* 120 */	ED_DELETE_NEXT_CHAR,	/* x */
    /* 121 */	ED_UNASSIGNED,		/* y */
    /* 122 */	ED_UNASSIGNED,		/* z */
    /* 123 */	ED_UNASSIGNED,		/* { */
    /* 124 */	ED_UNASSIGNED,		/* | */
    /* 125 */	ED_UNASSIGNED,		/* } */
    /* 126 */	VI_CHANGE_CASE,		/* ~ */
    /* 127 */	ED_DELETE_PREV_CHAR,	/* ^? */
    /* 128 */	ED_UNASSIGNED,		/* M-^@ */
    /* 129 */	ED_UNASSIGNED,		/* M-^A */
    /* 130 */	ED_UNASSIGNED,		/* M-^B */
    /* 131 */	ED_UNASSIGNED,		/* M-^C */
    /* 132 */	ED_UNASSIGNED,		/* M-^D */
    /* 133 */	ED_UNASSIGNED,		/* M-^E */
    /* 134 */	ED_UNASSIGNED,		/* M-^F */
    /* 135 */	ED_UNASSIGNED,		/* M-^G */
    /* 136 */	ED_UNASSIGNED,		/* M-^H */
    /* 137 */	ED_UNASSIGNED,		/* M-^I */
    /* 138 */	ED_UNASSIGNED,		/* M-^J */
    /* 139 */	ED_UNASSIGNED,		/* M-^K */
    /* 140 */	ED_UNASSIGNED,		/* M-^L */
    /* 141 */	ED_UNASSIGNED,		/* M-^M */
    /* 142 */	ED_UNASSIGNED,		/* M-^N */
    /* 143 */	ED_UNASSIGNED,		/* M-^O */
    /* 144 */	ED_UNASSIGNED,		/* M-^P */
    /* 145 */	ED_UNASSIGNED,		/* M-^Q */
    /* 146 */	ED_UNASSIGNED,		/* M-^R */
    /* 147 */	ED_UNASSIGNED,		/* M-^S */
    /* 148 */	ED_UNASSIGNED,		/* M-^T */
    /* 149 */	ED_UNASSIGNED,		/* M-^U */
    /* 150 */	ED_UNASSIGNED,		/* M-^V */
    /* 151 */	ED_UNASSIGNED,		/* M-^W */
    /* 152 */	ED_UNASSIGNED,		/* M-^X */
    /* 153 */	ED_UNASSIGNED,		/* M-^Y */
    /* 154 */	ED_UNASSIGNED,		/* M-^Z */
    /* 155 */	ED_UNASSIGNED,		/* M-^[ */
    /* 156 */	ED_UNASSIGNED,		/* M-^\ */
    /* 157 */	ED_UNASSIGNED,		/* M-^] */
    /* 158 */	ED_UNASSIGNED,		/* M-^^ */
    /* 159 */	ED_UNASSIGNED,		/* M-^_ */
    /* 160 */	ED_UNASSIGNED,		/* M-SPACE */
    /* 161 */	ED_UNASSIGNED,		/* M-! */
    /* 162 */	ED_UNASSIGNED,		/* M-" */
    /* 163 */	ED_UNASSIGNED,		/* M-# */
    /* 164 */	ED_UNASSIGNED,		/* M-$ */
    /* 165 */	ED_UNASSIGNED,		/* M-% */
    /* 166 */	ED_UNASSIGNED,		/* M-& */
    /* 167 */	ED_UNASSIGNED,		/* M-' */
    /* 168 */	ED_UNASSIGNED,		/* M-( */
    /* 169 */	ED_UNASSIGNED,		/* M-) */
    /* 170 */	ED_UNASSIGNED,		/* M-* */
    /* 171 */	ED_UNASSIGNED,		/* M-+ */
    /* 172 */	ED_UNASSIGNED,		/* M-, */
    /* 173 */	ED_UNASSIGNED,		/* M-- */
    /* 174 */	ED_UNASSIGNED,		/* M-. */
    /* 175 */	ED_UNASSIGNED,		/* M-/ */
    /* 176 */	ED_UNASSIGNED,		/* M-0 */
    /* 177 */	ED_UNASSIGNED,		/* M-1 */
    /* 178 */	ED_UNASSIGNED,		/* M-2 */
    /* 179 */	ED_UNASSIGNED,		/* M-3 */
    /* 180 */	ED_UNASSIGNED,		/* M-4 */
    /* 181 */	ED_UNASSIGNED,		/* M-5 */
    /* 182 */	ED_UNASSIGNED,		/* M-6 */
    /* 183 */	ED_UNASSIGNED,		/* M-7 */
    /* 184 */	ED_UNASSIGNED,		/* M-8 */
    /* 185 */	ED_UNASSIGNED,		/* M-9 */
    /* 186 */	ED_UNASSIGNED,		/* M-: */
    /* 187 */	ED_UNASSIGNED,		/* M-; */
    /* 188 */	ED_UNASSIGNED,		/* M-< */
    /* 189 */	ED_UNASSIGNED,		/* M-= */
    /* 190 */	ED_UNASSIGNED,		/* M-> */
    /* 191 */	ED_UNASSIGNED,		/* M-? */
    /* 192 */	ED_UNASSIGNED,		/* M-@ */
    /* 193 */	ED_UNASSIGNED,		/* M-A */
    /* 194 */	ED_UNASSIGNED,		/* M-B */
    /* 195 */	ED_UNASSIGNED,		/* M-C */
    /* 196 */	ED_UNASSIGNED,		/* M-D */
    /* 197 */	ED_UNASSIGNED,		/* M-E */
    /* 198 */	ED_UNASSIGNED,		/* M-F */
    /* 199 */	ED_UNASSIGNED,		/* M-G */
    /* 200 */	ED_UNASSIGNED,		/* M-H */
    /* 201 */	ED_UNASSIGNED,		/* M-I */
    /* 202 */	ED_UNASSIGNED,		/* M-J */
    /* 203 */	ED_UNASSIGNED,		/* M-K */
    /* 204 */	ED_UNASSIGNED,		/* M-L */
    /* 205 */	ED_UNASSIGNED,		/* M-M */
    /* 206 */	ED_UNASSIGNED,		/* M-N */
    /* 207 */	ED_SEQUENCE_LEAD_IN,	/* M-O */
    /* 208 */	ED_UNASSIGNED,		/* M-P */
    /* 209 */	ED_UNASSIGNED,		/* M-Q */
    /* 210 */	ED_UNASSIGNED,		/* M-R */
    /* 211 */	ED_UNASSIGNED,		/* M-S */
    /* 212 */	ED_UNASSIGNED,		/* M-T */
    /* 213 */	ED_UNASSIGNED,		/* M-U */
    /* 214 */	ED_UNASSIGNED,		/* M-V */
    /* 215 */	ED_UNASSIGNED,		/* M-W */
    /* 216 */	ED_UNASSIGNED,		/* M-X */
    /* 217 */	ED_UNASSIGNED,		/* M-Y */
    /* 218 */	ED_UNASSIGNED,		/* M-Z */
    /* 219 */	ED_SEQUENCE_LEAD_IN,	/* M-[ */
    /* 220 */	ED_UNASSIGNED,		/* M-\ */
    /* 221 */	ED_UNASSIGNED,		/* M-] */
    /* 222 */	ED_UNASSIGNED,		/* M-^ */
    /* 223 */	ED_UNASSIGNED,		/* M-_ */
    /* 224 */	ED_UNASSIGNED,		/* M-` */
    /* 225 */	ED_UNASSIGNED,		/* M-a */
    /* 226 */	ED_UNASSIGNED,		/* M-b */
    /* 227 */	ED_UNASSIGNED,		/* M-c */
    /* 228 */	ED_UNASSIGNED,		/* M-d */
    /* 229 */	ED_UNASSIGNED,		/* M-e */
    /* 230 */	ED_UNASSIGNED,		/* M-f */
    /* 231 */	ED_UNASSIGNED,		/* M-g */
    /* 232 */	ED_UNASSIGNED,		/* M-h */
    /* 233 */	ED_UNASSIGNED,		/* M-i */
    /* 234 */	ED_UNASSIGNED,		/* M-j */
    /* 235 */	ED_UNASSIGNED,		/* M-k */
    /* 236 */	ED_UNASSIGNED,		/* M-l */
    /* 237 */	ED_UNASSIGNED,		/* M-m */
    /* 238 */	ED_UNASSIGNED,		/* M-n */
    /* 239 */	ED_UNASSIGNED,		/* M-o */
    /* 240 */	ED_UNASSIGNED,		/* M-p */
    /* 241 */	ED_UNASSIGNED,		/* M-q */
    /* 242 */	ED_UNASSIGNED,		/* M-r */
    /* 243 */	ED_UNASSIGNED,		/* M-s */
    /* 244 */	ED_UNASSIGNED,		/* M-t */
    /* 245 */	ED_UNASSIGNED,		/* M-u */
    /* 246 */	ED_UNASSIGNED,		/* M-v */
    /* 247 */	ED_UNASSIGNED,		/* M-w */
    /* 248 */	ED_UNASSIGNED,		/* M-x */
    /* 249 */	ED_UNASSIGNED,		/* M-y */
    /* 250 */	ED_UNASSIGNED,		/* M-z */
    /* 251 */	ED_UNASSIGNED,		/* M-{ */
    /* 252 */	ED_UNASSIGNED,		/* M-| */
    /* 253 */	ED_UNASSIGNED,		/* M-} */
    /* 254 */	ED_UNASSIGNED,		/* M-~ */
    /* 255 */	ED_UNASSIGNED		/* M-^? */
};


/* map_init():
 *	Initialize and allocate the maps
 */
protected int
map_init(el)
    EditLine *el;
{

    /*
     * Make sure those are correct before starting.
     */
#ifdef MAP_DEBUG
    if (sizeof(el_map_emacs)      != N_KEYS * sizeof(el_action_t))
	abort();
    if (sizeof(el_map_vi_command) != N_KEYS * sizeof(el_action_t))
	abort();
    if (sizeof(el_map_vi_insert)  != N_KEYS * sizeof(el_action_t))
	abort();
#endif

    el->el_map.alt   = (el_action_t *) el_malloc(sizeof(el_action_t) * N_KEYS);
    el->el_map.key   = (el_action_t *) el_malloc(sizeof(el_action_t) * N_KEYS);
    el->el_map.emacs = el_map_emacs;
    el->el_map.vic   = el_map_vi_command;
    el->el_map.vii   = el_map_vi_insert;
    el->el_map.help  = (el_bindings_t *) el_malloc(sizeof(el_bindings_t) *
						   EL_NUM_FCNS);
    (void) memcpy(el->el_map.help, help__get(),
		  sizeof(el_bindings_t) * EL_NUM_FCNS);
    el->el_map.func  = (el_func_t *) el_malloc(sizeof(el_func_t) * EL_NUM_FCNS);
    memcpy(el->el_map.func, func__get(), sizeof(el_func_t) * EL_NUM_FCNS);
    el->el_map.nfunc = EL_NUM_FCNS;

#ifdef VIDEFAULT
    map_init_vi(el);
#else
    map_init_emacs(el);
#endif /* VIDEFAULT */
    return 0;
}


/* map_end():
 *	Free the space taken by the editor maps
 */
protected void
map_end(el)
    EditLine *el;
{
    el_free((ptr_t) el->el_map.alt);
    el->el_map.alt   = NULL;
    el_free((ptr_t) el->el_map.key);
    el->el_map.key   = NULL;
    el->el_map.emacs = NULL;
    el->el_map.vic   = NULL;
    el->el_map.vii   = NULL;
    el_free((ptr_t) el->el_map.help);
    el->el_map.help  = NULL;
    el_free((ptr_t) el->el_map.func);
    el->el_map.func  = NULL;
}


/* map_init_nls():
 *	Find all the printable keys and bind them to self insert
 */
private void
map_init_nls(el)
    EditLine *el;
{
    int i;
    el_action_t *map = el->el_map.key;

    for (i = 0200; i <= 0377; i++)
	if (isprint(i))
	    map[i] = ED_INSERT;
}


/* map_init_meta():
 *	Bind all the meta keys to the appropriate ESC-<key> sequence
 */
private void
map_init_meta(el)
    EditLine *el;
{
    char    buf[3];
    int i;
    el_action_t *map = el->el_map.key;
    el_action_t *alt = el->el_map.alt;

    for (i = 0; i <= 0377 && map[i] != EM_META_NEXT; i++)
	continue;

    if (i > 0377) {
	for (i = 0; i <= 0377 && alt[i] != EM_META_NEXT; i++)
	    continue;
	if (i > 0377) {
	    i = 033;
	    if (el->el_map.type == MAP_VI)
		map = alt;
	}
	else
	    map = alt;
    }
    buf[0] = (char) i;
    buf[2] = 0;
    for (i = 0200; i <= 0377; i++)
	switch (map[i]) {
	case ED_INSERT:
	case ED_UNASSIGNED:
	case ED_SEQUENCE_LEAD_IN:
	    break;
	default:
	    buf[1] = i & 0177;
	    key_add(el, buf, key_map_cmd(el, (int) map[i]), XK_CMD);
	    break;
	}
    map[buf[0]] = ED_SEQUENCE_LEAD_IN;
}


/* map_init_vi():
 *	Initialize the vi bindings
 */
protected void
map_init_vi(el)
    EditLine *el;
{
    int i;
    el_action_t *key = el->el_map.key;
    el_action_t *alt = el->el_map.alt;
    el_action_t *vii = el->el_map.vii;
    el_action_t *vic = el->el_map.vic;

    el->el_map.type = MAP_VI;
    el->el_map.current = el->el_map.key;

    key_reset(el);

    for (i = 0; i < N_KEYS; i++) {
	key[i] = vii[i];
	alt[i] = vic[i];
    }

    map_init_meta(el);
#ifdef notyet
    if (0 /* XXX: USER has set LC_CTYPE */)
	map_init_nls(el);
#endif

    tty_bind_char(el, 1);
    term_bind_arrow(el);
}


/* map_init_emacs():
 *	Initialize the emacs bindings
 */
protected void
map_init_emacs(el)
    EditLine *el;
{
    int i;
    char    buf[3];
    el_action_t *key   = el->el_map.key;
    el_action_t *alt   = el->el_map.alt;
    el_action_t *emacs = el->el_map.emacs;

    el->el_map.type = MAP_EMACS;
    el->el_map.current = el->el_map.key;
    key_reset(el);

    for (i = 0; i < N_KEYS; i++) {
	key[i] = emacs[i];
	alt[i] = ED_UNASSIGNED;
    }

    map_init_meta(el);
#ifdef notyet
    if (0 /* XXX: USER has set LC_CTYPE */)
	map_init_nls(el);
#endif
    map_init_nls(el);

    buf[0] = CONTROL('X');
    buf[1] = CONTROL('X');
    buf[2] = 0;
    key_add(el, buf, key_map_cmd(el, EM_EXCHANGE_MARK), XK_CMD);

    tty_bind_char(el, 1);
    term_bind_arrow(el);
}


/* map_set_editor():
 *	Set the editor
 */
protected int
map_set_editor(el, editor)
    EditLine *el;
    char *editor;
{
    if (strcmp(editor, "emacs") == 0) {
	map_init_emacs(el);
	return 0;
    }
    if (strcmp(editor, "vi") == 0) {
	map_init_vi(el);
	return 0;
    }
    return -1;
}


/* map_print_key():
 *	Print the function description for 1 key
 */
private void
map_print_key(el, map, in)
    EditLine *el;
    el_action_t *map;
    char *in;
{
    char outbuf[EL_BUFSIZ];
    el_bindings_t *bp;

    if (in[0] == '\0' || in[1] == '\0') {
	(void) key__decode_str(in, outbuf, "");
	for (bp = el->el_map.help; bp->name != NULL; bp++)
	    if (bp->func == map[(unsigned char) *in]) {
		(void) fprintf(el->el_outfile,
			       "%s\t->\t%s\n", outbuf, bp->name);
		return;
	    }
    }
    else
	key_print(el, in);
}


/* map_print_some_keys():
 *	Print keys from first to last
 */
private void
map_print_some_keys(el, map, first, last)
    EditLine *el;
    el_action_t *map;
    int     first, last;
{
    el_bindings_t *bp;
    char    firstbuf[2], lastbuf[2];
    char unparsbuf[EL_BUFSIZ], extrabuf[EL_BUFSIZ];

    firstbuf[0] = first;
    firstbuf[1] = 0;
    lastbuf[0] = last;
    lastbuf[1] = 0;
    if (map[first] == ED_UNASSIGNED) {
	if (first == last)
	    (void) fprintf(el->el_outfile, "%-15s->  is undefined\n",
		    key__decode_str(firstbuf, unparsbuf, STRQQ));
	return;
    }

    for (bp = el->el_map.help; bp->name != NULL; bp++) {
	if (bp->func == map[first]) {
	    if (first == last) {
		(void) fprintf(el->el_outfile, "%-15s->  %s\n",
			       key__decode_str(firstbuf, unparsbuf, STRQQ),
			       bp->name);
	    }
	    else {
		(void) fprintf(el->el_outfile, "%-4s to %-7s->  %s\n",
			       key__decode_str(firstbuf, unparsbuf, STRQQ),
			       key__decode_str(lastbuf, extrabuf, STRQQ),
			       bp->name);
	    }
	    return;
	}
    }
#ifdef MAP_DEBUG
    if (map == el->el_map.key) {
	(void) fprintf(el->el_outfile, "BUG!!! %s isn't bound to anything.\n",
		       key__decode_str(firstbuf, unparsbuf, STRQQ));
	(void) fprintf(el->el_outfile, "el->el_map.key[%d] == %d\n",
		       first, el->el_map.key[first]);
    }
    else {
	(void) fprintf(el->el_outfile, "BUG!!! %s isn't bound to anything.\n",
		       key__decode_str(firstbuf, unparsbuf, STRQQ));
	(void) fprintf(el->el_outfile, "el->el_map.alt[%d] == %d\n",
		       first, el->el_map.alt[first]);
    }
#endif
    abort();
}


/* map_print_all_keys():
 *	Print the function description for all keys.
 */
private void
map_print_all_keys(el)
    EditLine *el;
{
    int     prev, i;

    (void) fprintf(el->el_outfile, "Standard key bindings\n");
    prev = 0;
    for (i = 0; i < N_KEYS; i++) {
	if (el->el_map.key[prev] == el->el_map.key[i])
	    continue;
	map_print_some_keys(el, el->el_map.key, prev, i - 1);
	prev = i;
    }
    map_print_some_keys(el, el->el_map.key, prev, i - 1);

    (void) fprintf(el->el_outfile, "Alternative key bindings\n");
    prev = 0;
    for (i = 0; i < N_KEYS; i++) {
	if (el->el_map.alt[prev] == el->el_map.alt[i])
	    continue;
	map_print_some_keys(el, el->el_map.alt, prev, i - 1);
	prev = i;
    }
    map_print_some_keys(el, el->el_map.alt, prev, i - 1);

    (void) fprintf(el->el_outfile, "Multi-character bindings\n");
    key_print(el, "");
    (void) fprintf(el->el_outfile, "Arrow key bindings\n");
    term_print_arrow(el, "");
}


/* map_bind():
 *	Add/remove/change bindings
 */
protected int
map_bind(el, argc, argv)
    EditLine *el;
    int argc;
    char **argv;
{
    el_action_t *map;
    int     ntype, remove;
    char   *p;
    char    inbuf[EL_BUFSIZ];
    char    outbuf[EL_BUFSIZ];
    char   *in = NULL;
    char   *out = NULL;
    el_bindings_t *bp;
    int     cmd;
    int	    key;

    if (argv == NULL)
	return -1;

    map = el->el_map.key;
    ntype = XK_CMD;
    key = remove = 0;
    for (argc = 1; (p = argv[argc]) != NULL; argc++)
	if (p[0] == '-')
	    switch (p[1]) {
	    case 'a':
		map = el->el_map.alt;
		break;

	    case 's':
		ntype = XK_STR;
		break;
#ifdef notyet
	    case 'c':
		ntype = XK_EXE;
		break;
#endif
	    case 'k':
		key = 1;
		break;

	    case 'r':
		remove = 1;
		break;

	    case 'v':
		map_init_vi(el);
		return 0;

	    case 'e':
		map_init_emacs(el);
		return 0;

	    case 'l':
		for (bp = el->el_map.help; bp->name != NULL; bp++)
		    (void) fprintf(el->el_outfile, "%s\n\t%s\n",
				   bp->name, bp->description);
		return 0;
	    default:
		(void) fprintf(el->el_errfile, "%s: Invalid switch `%c'.\n",
			       argv[0], p[1]);
	    }
	else
	    break;

    if (argv[argc] == NULL) {
	map_print_all_keys(el);
	return 0;
    }

    if (key)
	in = argv[argc++];
    else
	if ((in = parse__string(inbuf, argv[argc++])) == NULL) {
	    (void) fprintf(el->el_errfile, "%s: Invalid \\ or ^ in instring.\n",
			   argv[0]);
	    return -1;
	}

    if (remove) {
	if (key) {
	    (void) term_clear_arrow(el, in);
	    return -1;
	}
	if (in[1])
	    (void) key_delete(el, in);
	else if (map[(unsigned char) *in] == ED_SEQUENCE_LEAD_IN)
	    (void) key_delete(el, in);
	else
	    map[(unsigned char) *in] = ED_UNASSIGNED;
	return 0;
    }

    if (argv[argc] == NULL) {
	if (key)
	    term_print_arrow(el, in);
	else
	    map_print_key(el, map, in);
	return 0;
    }

#ifdef notyet
    if (argv[argc + 1] != NULL) {
	bindkey_usage();
	return -1;
    }
#endif

    switch (ntype) {
    case XK_STR:
    case XK_EXE:
	if ((out = parse__string(outbuf, argv[argc])) == NULL) {
	    (void) fprintf(el->el_errfile,
			   "%s: Invalid \\ or ^ in outstring.\n", argv[0]);
	    return -1;
	}
	if (key)
	    term_set_arrow(el, in, key_map_str(el, out), ntype);
	else
	    key_add(el, in, key_map_str(el, out), ntype);
	map[(unsigned char) *in] = ED_SEQUENCE_LEAD_IN;
	break;

    case XK_CMD:
	if ((cmd = parse_cmd(el, argv[argc])) == -1) {
	    (void) fprintf(el->el_errfile,
			   "%s: Invalid command `%s'.\n", argv[0], argv[argc]);
	    return -1;
	}
	if (key)
	    term_set_arrow(el, in, key_map_str(el, out), ntype);
	else {
	    if (in[1]) {
		key_add(el, in, key_map_cmd(el, cmd), ntype);
		map[(unsigned char) *in] = ED_SEQUENCE_LEAD_IN;
	    }
	    else  {
		key_clear(el, map, in);
		map[(unsigned char) *in] = cmd;
	    }
	}
	break;

    default:
	abort();
	break;
    }
    return 0;
}


/* map_addfunc():
 *	add a user defined function
 */
protected int
map_addfunc(el, name, help, func)
    EditLine *el;
    const char *name;
    const char *help;
    el_func_t func;
{
    int nf = el->el_map.nfunc + 2;
    if (name == NULL || help == NULL || func == NULL)
	return -1;

    el->el_map.func = (el_func_t *)
		el_reallocf(el->el_map.func, nf * sizeof(el_func_t));
    el->el_map.help = (el_bindings_t *)
		el_reallocf(el->el_map.help, nf * sizeof(el_bindings_t));

    nf = el->el_map.nfunc;
    el->el_map.func[nf] = func;

    el->el_map.help[nf].name = name;
    el->el_map.help[nf].func = nf;
    el->el_map.help[nf].description = help;
    el->el_map.help[++nf].name = NULL;
    el->el_map.nfunc++;

    return 0;
}
