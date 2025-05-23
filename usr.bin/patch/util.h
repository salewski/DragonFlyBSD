/*
 * $OpenBSD: util.h,v 1.13 2004/08/05 21:47:24 deraadt Exp $
 * $DragonFly: src/usr.bin/patch/util.h,v 1.1 2004/09/24 18:44:28 joerg Exp $
 */

/*
 * patch - a program to apply diffs to original files
 * 
 * Copyright 1986, Larry Wall
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this condition and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * -C option added in 1998, original code by Marc Espie, based on FreeBSD
 * behaviour
 */

char		*fetchname(const char *, bool *, int);
char		*checked_in(char *);
int		backup_file(const char *);
int		move_file(const char *, const char *);
int		copy_file(const char *, const char *);
void		say(const char *, ...)
		    __attribute__((__format__(__printf__, 1, 2)));
void		fatal(const char *, ...)
		    __attribute__((__format__(__printf__, 1, 2)));
void		pfatal(const char *, ...)
		    __attribute__((__format__(__printf__, 1, 2)));
void		ask(const char *, ...)
		    __attribute__((__format__(__printf__, 1, 2)));
char		*savestr(const char *);
void		set_signals(int);
void		ignore_signals(void);
void		makedirs(const char *, bool);
void		version(void);
void		my_exit(int) __attribute__((noreturn));
