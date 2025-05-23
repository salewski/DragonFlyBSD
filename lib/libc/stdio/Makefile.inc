#	@(#)Makefile.inc	8.3 (Berkeley) 4/17/94
# $FreeBSD: src/lib/libc/stdio/Makefile.inc,v 1.19.2.1 2001/04/25 10:04:11 ru Exp $
# $DragonFly: src/lib/libc/stdio/Makefile.inc,v 1.2 2003/06/17 04:26:45 dillon Exp $

# stdio sources
.PATH: ${.CURDIR}/../libc/stdio

SRCS+=	_flock_stub.c \
	asprintf.c clrerr.c fclose.c fdopen.c feof.c ferror.c fflush.c \
	fgetc.c fgetln.c fgetpos.c fgets.c fileno.c findfp.c flags.c fopen.c \
	fprintf.c fpurge.c fputc.c fputs.c fread.c freopen.c fscanf.c \
	fseek.c fsetpos.c ftell.c funopen.c fvwrite.c fwalk.c fwrite.c \
	getc.c getchar.c gets.c getw.c makebuf.c mktemp.c perror.c \
	printf.c putc.c putchar.c puts.c putw.c refill.c remove.c rewind.c \
	rget.c scanf.c setbuf.c setbuffer.c setvbuf.c snprintf.c sprintf.c \
	sscanf.c stdio.c tempnam.c tmpfile.c tmpnam.c ungetc.c vasprintf.c \
	vfprintf.c vfscanf.c vprintf.c vscanf.c vsnprintf.c vsprintf.c \
	vsscanf.c wbuf.c wsetup.c

.if ${LIB} == "c"
MAN+=	fclose.3 ferror.3 fflush.3 fgetln.3 fgets.3 fopen.3 fputs.3 \
	fread.3 fseek.3 funopen.3 getc.3 mktemp.3 printf.3 putc.3 remove.3 \
	scanf.3 setbuf.3 stdio.3 tmpnam.3 ungetc.3

MLINKS+=ferror.3 clearerr.3 ferror.3 feof.3 ferror.3 fileno.3
MLINKS+=fflush.3 fpurge.3
MLINKS+=fgets.3 gets.3
MLINKS+=fopen.3 fdopen.3 fopen.3 freopen.3
MLINKS+=fputs.3 puts.3
MLINKS+=fread.3 fwrite.3
MLINKS+=fseek.3 fgetpos.3 fseek.3 fseeko.3 fseek.3 fsetpos.3 fseek.3 ftell.3 \
	fseek.3 ftello.3 fseek.3 rewind.3
MLINKS+=funopen.3 fropen.3 funopen.3 fwopen.3
MLINKS+=getc.3 fgetc.3 getc.3 getchar.3 getc.3 getw.3
MLINKS+=mktemp.3 mkdtemp.3 mktemp.3 mkstemp.3 mktemp.3 mkstemps.3
MLINKS+=printf.3 asprintf.3 printf.3 fprintf.3 \
	printf.3 snprintf.3 printf.3 sprintf.3 \
	printf.3 vasprintf.3 \
	printf.3 vfprintf.3 printf.3 vprintf.3 printf.3 vsnprintf.3 \
	printf.3 vsprintf.3
MLINKS+=putc.3 fputc.3 putc.3 putchar.3 putc.3 putw.3
MLINKS+=scanf.3 fscanf.3 scanf.3 sscanf.3 scanf.3 vfscanf.3 scanf.3 vscanf.3 \
	scanf.3 vsscanf.3
MLINKS+=setbuf.3 setbuffer.3 setbuf.3 setlinebuf.3 setbuf.3 setvbuf.3
MLINKS+=tmpnam.3 tempnam.3 tmpnam.3 tmpfile.3
.endif
