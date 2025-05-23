/* Copyright (C) 1992, 2001 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/* This translates ps fonts in .pfb format to ASCII ps files. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>

#include "nonposix.h"

/* Binary bytes per output line. */
#define BYTES_PER_LINE (64/2)
#define HEX_DIGITS "0123456789abcdef"

static char *program_name;

static void error(s)
     char *s;
{
  fprintf(stderr, "%s: %s\n", program_name, s);
  exit(2);
}

static void usage(FILE *stream)
{
  fprintf(stream, "usage: %s [-v] [pfb_file]\n", program_name);
}

int main(argc, argv)
     int argc;
     char **argv;
{
  int opt;
  extern int optind;
  static const struct option long_options[] = {
    { "help", no_argument, 0, CHAR_MAX + 1 },
    { "version", no_argument, 0, 'v' },
    { NULL, 0, 0, 0 }
  };

  program_name = argv[0];

  while ((opt = getopt_long(argc, argv, "v", long_options, NULL)) != EOF) {
    switch (opt) {
    case 'v':
      {
	extern const char *Version_string;
	printf("GNU pfbtops (groff) version %s\n", Version_string);
	exit(0);
	break;
      }
    case CHAR_MAX + 1: /* --help */
      usage(stdout);
      exit(0);
      break;
    case '?':
      usage(stderr);
      exit(1);
      break;
    }
  }

  if (argc - optind > 1) {
    usage(stderr);
    exit(1);
  }
  if (argc > optind && !freopen(argv[optind], "r", stdin))
    {
      perror(argv[optind]);
      exit(1);
    }
#ifdef SET_BINARY
  SET_BINARY(fileno(stdin));
#endif
  for (;;)
    {
      int type, c, i;
      long n;

      c = getchar();
      if (c != 0x80)
	error("first byte of packet not 0x80");
      type = getchar();
      if (type == 3)
	break;
      if (type != 1 && type != 2)
	error("bad packet type");
      n = 0;
      for (i = 0; i < 4; i++)
	{
	  c = getchar();
	  if (c == EOF)
	    error("end of file in packet header");
	  n |= (long)c << (i << 3);
	}
      if (n < 0)
	error("negative packet length");
      if (type == 1)
	{
	  while (--n >= 0)
	    {
	      c = getchar();
	      if (c == EOF)
		error("end of file in text packet");
	      if (c == '\r')
		c = '\n';
	      putchar(c);
	    }
	  if (c != '\n')
	    putchar('\n');
	}
      else
	{
	  int count = 0;
	  while (--n >= 0)
	    {
	      c = getchar();
	      if (c == EOF)
		error("end of file in binary packet");
	      if (count >= BYTES_PER_LINE)
		{
		  putchar('\n');
		  count = 0;
		}
	      count++;
	      putchar(HEX_DIGITS[(c >> 4) & 0xf]);
	      putchar(HEX_DIGITS[c & 0xf]);
	    }
	  putchar('\n');
	}
    }
  exit(0);
}
