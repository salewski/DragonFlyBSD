/* strings -- print the strings of printable characters in files
   Copyright 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
   2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* $FreeBSD: src/contrib/binutils/binutils/strings.c,v 1.2.6.4 2002/09/01 23:43:48 obrien Exp $ */
/* $DragonFly: src/contrib/binutils/binutils/Attic/strings.c,v 1.2 2003/06/17 04:23:58 dillon Exp $ */

/* Usage: strings [options] file...

   Options:
   --all
   -a
   -		Do not scan only the initialized data section of object files.

   --print-file-name
   -f		Print the name of the file before each string.

   --bytes=min-len
   -n min-len
   -min-len	Print graphic char sequences, MIN-LEN or more bytes long,
		that are followed by a NUL or a newline.  Default is 4.

   --radix={o,x,d}
   -t {o,x,d}	Print the offset within the file before each string,
		in octal/hex/decimal.

   -o		Like -to.  (Some other implementations have -o like -to,
		others like -td.  We chose one arbitrarily.)

   --encoding={s,b,l,B,L}
   -e {s,b,l,B,L}
		Select character encoding: single-byte, bigendian 16-bit,
		littleendian 16-bit, bigendian 32-bit, littleendian 32-bit

   --target=BFDNAME
		Specify a non-default object file format.

   --help
   -h		Print the usage message on the standard output.

   --version
   -v		Print the program version number.

   Written by Richard Stallman <rms@gnu.ai.mit.edu>
   and David MacKenzie <djm@gnu.ai.mit.edu>.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "bfd.h"
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include "bucomm.h"
#include "libiberty.h"
#include "safe-ctype.h"

/* Some platforms need to put stdin into binary mode, to read
    binary files.  */
#ifdef HAVE_SETMODE
#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#define setmode _setmode
#else
#define O_BINARY 0
#endif
#endif
#if O_BINARY
#include <io.h>
#define SET_BINARY(f) do { if (!isatty(f)) setmode(f,O_BINARY); } while (0)
#endif
#endif

#define isgraphic(c) (ISPRINT (c) || (c) == '\t')

#ifndef errno
extern int errno;
#endif

/* The BFD section flags that identify an initialized data section.  */
#define DATA_FLAGS (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS)

#ifdef HAVE_FOPEN64
typedef off64_t file_off;
#define file_open(s,m) fopen64(s,m)
#else
typedef off_t file_off;
#define file_open(s,m) fopen(s,m)
#endif

/* Radix for printing addresses (must be 8, 10 or 16).  */
static int address_radix;

/* Minimum length of sequence of graphic chars to trigger output.  */
static int string_min;

/* true means print address within file for each string.  */
static boolean print_addresses;

/* true means print filename for each string.  */
static boolean print_filenames;

/* true means for object files scan only the data section.  */
static boolean datasection_only;

/* true if we found an initialized data section in the current file.  */
static boolean got_a_section;

/* The BFD object file format.  */
static char *target;

/* The character encoding format.  */
static char encoding;
static int encoding_bytes;

static struct option long_options[] =
{
  {"all", no_argument, NULL, 'a'},
  {"print-file-name", no_argument, NULL, 'f'},
  {"bytes", required_argument, NULL, 'n'},
  {"radix", required_argument, NULL, 't'},
  {"encoding", required_argument, NULL, 'e'},
  {"target", required_argument, NULL, 'T'},
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {NULL, 0, NULL, 0}
};

static void strings_a_section PARAMS ((bfd *, asection *, PTR));
static boolean strings_object_file PARAMS ((const char *));
static boolean strings_file PARAMS ((char *file));
static int integer_arg PARAMS ((char *s));
static void print_strings PARAMS ((const char *filename, FILE *stream,
				  file_off address, int stop_point,
				  int magiccount, char *magic));
static void usage PARAMS ((FILE *stream, int status));
static long get_char PARAMS ((FILE *stream, file_off *address,
			      int *magiccount, char **magic));

int main PARAMS ((int, char **));

int
main (argc, argv)
     int argc;
     char **argv;
{
  int optc;
  int exit_status = 0;
  boolean files_given = false;

#if defined (HAVE_SETLOCALE)
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  program_name = argv[0];
  xmalloc_set_program_name (program_name);
  string_min = -1;
  print_addresses = false;
  print_filenames = false;
  datasection_only = true;
  target = NULL;
  encoding = 's';

  while ((optc = getopt_long (argc, argv, "afhHn:ot:e:Vv0123456789",
			      long_options, (int *) 0)) != EOF)
    {
      switch (optc)
	{
	case 'a':
	  datasection_only = false;
	  break;

	case 'f':
	  print_filenames = true;
	  break;

	case 'H':
	case 'h':
	  usage (stdout, 0);

	case 'n':
	  string_min = integer_arg (optarg);
	  if (string_min < 1)
	    {
	      fatal (_("invalid number %s"), optarg);
	    }
	  break;

	case 'o':
	  print_addresses = true;
	  address_radix = 8;
	  break;

	case 't':
	  print_addresses = true;
	  if (optarg[1] != '\0')
	    usage (stderr, 1);
	  switch (optarg[0])
	    {
	    case 'o':
	      address_radix = 8;
	      break;

	    case 'd':
	      address_radix = 10;
	      break;

	    case 'x':
	      address_radix = 16;
	      break;

	    default:
	      usage (stderr, 1);
	    }
	  break;

	case 'T':
	  target = optarg;
	  break;

	case 'e':
	  if (optarg[1] != '\0')
	    usage (stderr, 1);
	  encoding = optarg[0];
	  break;

	case 'V':
	case 'v':
	  print_version ("strings");
	  break;

	case '?':
	  usage (stderr, 1);

	default:
	  if (string_min < 0)
	    string_min = optc - '0';
	  else
	    string_min = string_min * 10 + optc - '0';
	  break;
	}
    }

  if (string_min < 0)
    string_min = 4;

  switch (encoding)
    {
    case 's':
      encoding_bytes = 1;
      break;
    case 'b':
    case 'l':
      encoding_bytes = 2;
      break;
    case 'B':
    case 'L':
      encoding_bytes = 4;
      break;
    default:
      usage (stderr, 1);
    }

  bfd_init ();
  set_default_bfd_target ();

  if (optind >= argc)
    {
      datasection_only = false;
#ifdef SET_BINARY
      SET_BINARY (fileno (stdin));
#endif
      print_strings ("{standard input}", stdin, 0, 0, 0, (char *) NULL);
      files_given = true;
    }
  else
    {
      for (; optind < argc; ++optind)
	{
	  if (strcmp (argv[optind], "-") == 0)
	    datasection_only = false;
	  else
	    {
	      files_given = true;
	      exit_status |= (strings_file (argv[optind]) == false);
	    }
	}
    }

  if (files_given == false)
    usage (stderr, 1);

  return (exit_status);
}

/* Scan section SECT of the file ABFD, whose printable name is FILE.
   If it contains initialized data,
   set `got_a_section' and print the strings in it.  */

static void
strings_a_section (abfd, sect, filearg)
     bfd *abfd;
     asection *sect;
     PTR filearg;
{
  const char *file = (const char *) filearg;

  if ((sect->flags & DATA_FLAGS) == DATA_FLAGS)
    {
      bfd_size_type sz = bfd_get_section_size_before_reloc (sect);
      PTR mem = xmalloc (sz);
      if (bfd_get_section_contents (abfd, sect, mem, (file_ptr) 0, sz))
	{
	  got_a_section = true;
	  print_strings (file, (FILE *) NULL, sect->filepos, 0, sz, mem);
	}
      free (mem);
    }
}

/* Scan all of the sections in FILE, and print the strings
   in the initialized data section(s).

   Return true if successful,
   false if not (such as if FILE is not an object file).  */

static boolean
strings_object_file (file)
     const char *file;
{
  bfd *abfd = bfd_openr (file, target);

  if (abfd == NULL)
    {
      /* Treat the file as a non-object file.  */
      return false;
    }

  /* This call is mainly for its side effect of reading in the sections.
     We follow the traditional behavior of `strings' in that we don't
     complain if we don't recognize a file to be an object file.  */
  if (bfd_check_format (abfd, bfd_object) == false)
    {
      bfd_close (abfd);
      return false;
    }

  got_a_section = false;
  bfd_map_over_sections (abfd, strings_a_section, (PTR) file);

  if (!bfd_close (abfd))
    {
      bfd_nonfatal (file);
      return false;
    }

  return got_a_section;
}

/* Print the strings in FILE.  Return true if ok, false if an error occurs.  */

static boolean
strings_file (file)
     char *file;
{
  /* If we weren't told to scan the whole file,
     try to open it as an object file and only look at
     initialized data sections.  If that fails, fall back to the
     whole file.  */
  if (!datasection_only || !strings_object_file (file))
    {
      FILE *stream;

      stream = file_open (file, FOPEN_RB);
      if (stream == NULL)
	{
	  fprintf (stderr, "%s: ", program_name);
	  perror (file);
	  return false;
	}

      print_strings (file, stream, (file_off) 0, 0, 0, (char *) 0);

      if (fclose (stream) == EOF)
	{
	  fprintf (stderr, "%s: ", program_name);
	  perror (file);
	  return false;
	}
    }

  return true;
}

/* Read the next character, return EOF if none available.
   Assume that STREAM is positioned so that the next byte read
   is at address ADDRESS in the file.

   If STREAM is NULL, do not read from it.
   The caller can supply a buffer of characters
   to be processed before the data in STREAM.
   MAGIC is the address of the buffer and
   MAGICCOUNT is how many characters are in it.  */

static long
get_char (stream, address, magiccount, magic)
     FILE *stream;
     file_off *address;
     int *magiccount;
     char **magic;
{
  int c, i;
  long r = EOF;
  unsigned char buf[4];

  for (i = 0; i < encoding_bytes; i++)
    {
      if (*magiccount)
	{
	  (*magiccount)--;
	  c = *(*magic)++;
	}
      else
	{
	  if (stream == NULL)
	    return EOF;
#ifdef HAVE_GETC_UNLOCKED
	  c = getc_unlocked (stream);
#else
	  c = getc (stream);
#endif
	  if (c == EOF)
	    return EOF;
	}

      (*address)++;
      buf[i] = c;
    }

  switch (encoding)
    {
    case 's':
      r = buf[0];
      break;
    case 'b':
      r = (buf[0] << 8) | buf[1];
      break;
    case 'l':
      r = buf[0] | (buf[1] << 8);
      break;
    case 'B':
      r = ((long) buf[0] << 24) | ((long) buf[1] << 16) |
	((long) buf[2] << 8) | buf[3];
      break;
    case 'L':
      r = buf[0] | ((long) buf[1] << 8) | ((long) buf[2] << 16) |
	((long) buf[3] << 24);
      break;
    }

  if (r == EOF)
    return 0;

  return r;
}

/* Find the strings in file FILENAME, read from STREAM.
   Assume that STREAM is positioned so that the next byte read
   is at address ADDRESS in the file.
   Stop reading at address STOP_POINT in the file, if nonzero.

   If STREAM is NULL, do not read from it.
   The caller can supply a buffer of characters
   to be processed before the data in STREAM.
   MAGIC is the address of the buffer and
   MAGICCOUNT is how many characters are in it.
   Those characters come at address ADDRESS and the data in STREAM follow.  */

static void
print_strings (filename, stream, address, stop_point, magiccount, magic)
     const char *filename;
     FILE *stream;
     file_off address;
     int stop_point;
     int magiccount;
     char *magic;
{
  char *buf = (char *) xmalloc (sizeof (char) * (string_min + 1));

  while (1)
    {
      file_off start;
      int i;
      long c;

      /* See if the next `string_min' chars are all graphic chars.  */
    tryline:
      if (stop_point && address >= stop_point)
	break;
      start = address;
      for (i = 0; i < string_min; i++)
	{
	  c = get_char (stream, &address, &magiccount, &magic);
	  if (c == EOF)
	    return;
	  if (c > 255 || c < 0 || !isgraphic (c))
	    /* Found a non-graphic.  Try again starting with next char.  */
	    goto tryline;
	  buf[i] = c;
	}

      /* We found a run of `string_min' graphic characters.  Print up
         to the next non-graphic character.  */

      if (print_filenames)
	printf ("%s: ", filename);
      if (print_addresses)
	switch (address_radix)
	  {
	  case 8:
#if __STDC_VERSION__ >= 199901L || (defined(__GNUC__) && __GNUC__ >= 2)
	    if (sizeof (start) > sizeof (long))
	      printf ("%7Lo ", (unsigned long long) start);
	    else
#else
# if !BFD_HOST_64BIT_LONG
	    if (start != (unsigned long) start)
	      printf ("++%7lo ", (unsigned long) start);
	    else
# endif
#endif
	      printf ("%7lo ", (unsigned long) start);
	    break;

	  case 10:
#if __STDC_VERSION__ >= 199901L || (defined(__GNUC__) && __GNUC__ >= 2)
	    if (sizeof (start) > sizeof (long))
	      printf ("%7Ld ", (unsigned long long) start);
	    else
#else
# if !BFD_HOST_64BIT_LONG
	    if (start != (unsigned long) start)
	      printf ("++%7ld ", (unsigned long) start);
	    else
# endif
#endif
	      printf ("%7ld ", (long) start);
	    break;

	  case 16:
#if __STDC_VERSION__ >= 199901L || (defined(__GNUC__) && __GNUC__ >= 2)
	    if (sizeof (start) > sizeof (long))
	      printf ("%7Lx ", (unsigned long long) start);
	    else
#else
# if !BFD_HOST_64BIT_LONG
	    if (start != (unsigned long) start)
	      printf ("%lx%8.8lx ", start >> 32, start & 0xffffffff);
	    else
# endif
#endif
	      printf ("%7lx ", (unsigned long) start);
	    break;
	  }

      buf[i] = '\0';
      fputs (buf, stdout);

      while (1)
	{
	  c = get_char (stream, &address, &magiccount, &magic);
	  if (c == EOF)
	    break;
	  if (c > 255 || c < 0 || !isgraphic (c))
	    break;
	  putchar (c);
	}

      putchar ('\n');
    }
}

/* Parse string S as an integer, using decimal radix by default,
   but allowing octal and hex numbers as in C.  */

static int
integer_arg (s)
     char *s;
{
  int value;
  int radix = 10;
  char *p = s;
  int c;

  if (*p != '0')
    radix = 10;
  else if (*++p == 'x')
    {
      radix = 16;
      p++;
    }
  else
    radix = 8;

  value = 0;
  while (((c = *p++) >= '0' && c <= '9')
	 || (radix == 16 && (c & ~40) >= 'A' && (c & ~40) <= 'Z'))
    {
      value *= radix;
      if (c >= '0' && c <= '9')
	value += c - '0';
      else
	value += (c & ~40) - 'A';
    }

  if (c == 'b')
    value *= 512;
  else if (c == 'B')
    value *= 1024;
  else
    p--;

  if (*p)
    {
      fatal (_("invalid integer argument %s"), s);
    }
  return value;
}

static void
usage (stream, status)
     FILE *stream;
     int status;
{
  fprintf (stream, _("Usage: %s [option(s)] [file(s)]\n"), program_name);
  fprintf (stream, _(" Display printable strings in [file(s)] (stdin by default)\n"));
  fprintf (stream, _(" The options are:\n\
  -a - --all                Scan the entire file, not just the data section\n\
  -f --print-file-name      Print the name of the file before each string\n\
  -n --bytes=[number]       Locate & print any NUL-terminated sequence of at\n\
  -<number>                 least [number] characters (default 4).\n\
  -t --radix={o,x,d}        Print the location of the string in base 8, 10 or 16\n\
  -o                        An alias for --radix=o\n\
  -T --target=<BFDNAME>     Specify the binary file format\n\
  -e --encoding={s,b,l,B,L} Select character size and endianness:\n\
                            s = 8-bit, {b,l} = 16-bit, {B,L} = 32-bit\n\
  -h --help                 Display this information\n\
  -v --version              Print the program's version number\n"));
  list_supported_targets (program_name, stream);
  if (status == 0)
    fprintf (stream, _("Report bugs to %s\n"), REPORT_BUGS_TO);
  exit (status);
}
