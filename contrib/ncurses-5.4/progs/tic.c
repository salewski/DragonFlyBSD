/****************************************************************************
 * Copyright (c) 1998-2002,2003 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 *     and: Thomas E. Dickey 1996 on                                        *
 ****************************************************************************/

/*
 *	tic.c --- Main program for terminfo compiler
 *			by Eric S. Raymond
 *
 */

#include <progs.priv.h>
#include <sys/stat.h>

#include <dump_entry.h>
#include <term_entry.h>
#include <transform.h>

MODULE_ID("$Id: tic.c,v 1.109 2003/12/06 17:36:57 tom Exp $")

const char *_nc_progname = "tic";

static FILE *log_fp;
static FILE *tmp_fp;
static bool showsummary = FALSE;
static const char *to_remove;
static int tparm_errs;

static void (*save_check_termtype) (TERMTYPE *);
static void check_termtype(TERMTYPE * tt);

static const char usage_string[] = "[-V] [-v[n]] [-e names] [-o dir] [-R name] [-CILNTcfrswx1] source-file\n";

static void
cleanup(void)
{
    if (tmp_fp != 0)
	fclose(tmp_fp);
    if (to_remove != 0) {
#if HAVE_REMOVE
	remove(to_remove);
#else
	unlink(to_remove);
#endif
    }
}

static void
failed(const char *msg)
{
    perror(msg);
    cleanup();
    ExitProgram(EXIT_FAILURE);
}

static void
usage(void)
{
    static const char *const tbl[] =
    {
	"Options:",
	"  -1         format translation output one capability per line",
#if NCURSES_XNAMES
	"  -a         retain commented-out capabilities (sets -x also)",
#endif
	"  -C         translate entries to termcap source form",
	"  -c         check only, validate input without compiling or translating",
	"  -e<names>  translate/compile only entries named by comma-separated list",
	"  -f         format complex strings for readability",
	"  -G         format %{number} to %'char'",
	"  -g         format %'char' to %{number}",
	"  -I         translate entries to terminfo source form",
	"  -L         translate entries to full terminfo source form",
	"  -N         disable smart defaults for source translation",
	"  -o<dir>    set output directory for compiled entry writes",
	"  -R<name>   restrict translation to given terminfo/termcap version",
	"  -r         force resolution of all use entries in source translation",
	"  -s         print summary statistics",
	"  -T         remove size-restrictions on compiled description",
#if NCURSES_XNAMES
	"  -t         suppress commented-out capabilities",
#endif
	"  -V         print version",
	"  -v[n]      set verbosity level",
	"  -w[n]      set format width for translation output",
#if NCURSES_XNAMES
	"  -x         treat unknown capabilities as user-defined",
#endif
	"",
	"Parameters:",
	"  <file>     file to translate or compile"
    };
    size_t j;

    fprintf(stderr, "Usage: %s %s\n", _nc_progname, usage_string);
    for (j = 0; j < SIZEOF(tbl); j++) {
	fputs(tbl[j], stderr);
	putc('\n', stderr);
    }
    ExitProgram(EXIT_FAILURE);
}

#define L_BRACE '{'
#define R_BRACE '}'
#define S_QUOTE '\'';

static void
write_it(ENTRY * ep)
{
    unsigned n;
    int ch;
    char *s, *d, *t;
    char result[MAX_ENTRY_SIZE];

    /*
     * Look for strings that contain %{number}, convert them to %'char',
     * which is shorter and runs a little faster.
     */
    for (n = 0; n < STRCOUNT; n++) {
	s = ep->tterm.Strings[n];
	if (VALID_STRING(s)
	    && strchr(s, L_BRACE) != 0) {
	    d = result;
	    t = s;
	    while ((ch = *t++) != 0) {
		*d++ = ch;
		if (ch == '\\') {
		    *d++ = *t++;
		} else if ((ch == '%')
			   && (*t == L_BRACE)) {
		    char *v = 0;
		    long value = strtol(t + 1, &v, 0);
		    if (v != 0
			&& *v == R_BRACE
			&& value > 0
			&& value != '\\'	/* FIXME */
			&& value < 127
			&& isprint((int) value)) {
			*d++ = S_QUOTE;
			*d++ = (int) value;
			*d++ = S_QUOTE;
			t = (v + 1);
		    }
		}
	    }
	    *d = 0;
	    if (strlen(result) < strlen(s))
		strcpy(s, result);
	}
    }

    _nc_set_type(_nc_first_name(ep->tterm.term_names));
    _nc_curr_line = ep->startline;
    _nc_write_entry(&ep->tterm);
}

static bool
immedhook(ENTRY * ep GCC_UNUSED)
/* write out entries with no use capabilities immediately to save storage */
{
#if !HAVE_BIG_CORE
    /*
     * This is strictly a core-economy kluge.  The really clean way to handle
     * compilation is to slurp the whole file into core and then do all the
     * name-collision checks and entry writes in one swell foop.  But the
     * terminfo master file is large enough that some core-poor systems swap
     * like crazy when you compile it this way...there have been reports of
     * this process taking *three hours*, rather than the twenty seconds or
     * less typical on my development box.
     *
     * So.  This hook *immediately* writes out the referenced entry if it
     * has no use capabilities.  The compiler main loop refrains from
     * adding the entry to the in-core list when this hook fires.  If some
     * other entry later needs to reference an entry that got written
     * immediately, that's OK; the resolution code will fetch it off disk
     * when it can't find it in core.
     *
     * Name collisions will still be detected, just not as cleanly.  The
     * write_entry() code complains before overwriting an entry that
     * postdates the time of tic's first call to write_entry().  Thus
     * it will complain about overwriting entries newly made during the
     * tic run, but not about overwriting ones that predate it.
     *
     * The reason this is a hook, and not in line with the rest of the
     * compiler code, is that the support for termcap fallback cannot assume
     * it has anywhere to spool out these entries!
     *
     * The _nc_set_type() call here requires a compensating one in
     * _nc_parse_entry().
     *
     * If you define HAVE_BIG_CORE, you'll disable this kluge.  This will
     * make tic a bit faster (because the resolution code won't have to do
     * disk I/O nearly as often).
     */
    if (ep->nuses == 0) {
	int oldline = _nc_curr_line;

	write_it(ep);
	_nc_curr_line = oldline;
	free(ep->tterm.str_table);
	return (TRUE);
    }
#endif /* HAVE_BIG_CORE */
    return (FALSE);
}

static void
put_translate(int c)
/* emit a comment char, translating terminfo names to termcap names */
{
    static bool in_name = FALSE;
    static size_t have, used;
    static char *namebuf, *suffix;

    if (in_name) {
	if (used + 1 >= have) {
	    have += 132;
	    namebuf = typeRealloc(char, have, namebuf);
	    suffix = typeRealloc(char, have, suffix);
	}
	if (c == '\n' || c == '@') {
	    namebuf[used++] = '\0';
	    (void) putchar('<');
	    (void) fputs(namebuf, stdout);
	    putchar(c);
	    in_name = FALSE;
	} else if (c != '>') {
	    namebuf[used++] = c;
	} else {		/* ah! candidate name! */
	    char *up;
	    NCURSES_CONST char *tp;

	    namebuf[used++] = '\0';
	    in_name = FALSE;

	    suffix[0] = '\0';
	    if ((up = strchr(namebuf, '#')) != 0
		|| (up = strchr(namebuf, '=')) != 0
		|| ((up = strchr(namebuf, '@')) != 0 && up[1] == '>')) {
		(void) strcpy(suffix, up);
		*up = '\0';
	    }

	    if ((tp = nametrans(namebuf)) != 0) {
		(void) putchar(':');
		(void) fputs(tp, stdout);
		(void) fputs(suffix, stdout);
		(void) putchar(':');
	    } else {
		/* couldn't find a translation, just dump the name */
		(void) putchar('<');
		(void) fputs(namebuf, stdout);
		(void) fputs(suffix, stdout);
		(void) putchar('>');
	    }
	}
    } else {
	used = 0;
	if (c == '<') {
	    in_name = TRUE;
	} else {
	    putchar(c);
	}
    }
}

/* Returns a string, stripped of leading/trailing whitespace */
static char *
stripped(char *src)
{
    while (isspace(UChar(*src)))
	src++;
    if (*src != '\0') {
	char *dst = strcpy((char *) malloc(strlen(src) + 1), src);
	size_t len = strlen(dst);
	while (--len != 0 && isspace(UChar(dst[len])))
	    dst[len] = '\0';
	return dst;
    }
    return 0;
}

static FILE *
open_input(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    struct stat sb;

    if (fp == 0) {
	fprintf(stderr, "%s: Can't open %s\n", _nc_progname, filename);
	ExitProgram(EXIT_FAILURE);
    }
    if (fstat(fileno(fp), &sb) < 0
	|| (sb.st_mode & S_IFMT) != S_IFREG) {
	fprintf(stderr, "%s: %s is not a file\n", _nc_progname, filename);
	ExitProgram(EXIT_FAILURE);
    }
    return fp;
}

/* Parse the "-e" option-value into a list of names */
static const char **
make_namelist(char *src)
{
    const char **dst = 0;

    char *s, *base;
    unsigned pass, n, nn;
    char buffer[BUFSIZ];

    if (src == 0) {
	/* EMPTY */ ;
    } else if (strchr(src, '/') != 0) {		/* a filename */
	FILE *fp = open_input(src);

	for (pass = 1; pass <= 2; pass++) {
	    nn = 0;
	    while (fgets(buffer, sizeof(buffer), fp) != 0) {
		if ((s = stripped(buffer)) != 0) {
		    if (dst != 0)
			dst[nn] = s;
		    nn++;
		}
	    }
	    if (pass == 1) {
		dst = typeCalloc(const char *, nn + 1);
		rewind(fp);
	    }
	}
	fclose(fp);
    } else {			/* literal list of names */
	for (pass = 1; pass <= 2; pass++) {
	    for (n = nn = 0, base = src;; n++) {
		int mark = src[n];
		if (mark == ',' || mark == '\0') {
		    if (pass == 1) {
			nn++;
		    } else {
			src[n] = '\0';
			if ((s = stripped(base)) != 0)
			    dst[nn++] = s;
			base = &src[n + 1];
		    }
		}
		if (mark == '\0')
		    break;
	    }
	    if (pass == 1)
		dst = typeCalloc(const char *, nn + 1);
	}
    }
    if (showsummary) {
	fprintf(log_fp, "Entries that will be compiled:\n");
	for (n = 0; dst[n] != 0; n++)
	    fprintf(log_fp, "%d:%s\n", n + 1, dst[n]);
    }
    return dst;
}

static bool
matches(const char **needle, const char *haystack)
/* does entry in needle list match |-separated field in haystack? */
{
    bool code = FALSE;
    size_t n;

    if (needle != 0) {
	for (n = 0; needle[n] != 0; n++) {
	    if (_nc_name_match(haystack, needle[n], "|")) {
		code = TRUE;
		break;
	    }
	}
    } else
	code = TRUE;
    return (code);
}

static FILE *
open_tempfile(char *name)
{
    FILE *result = 0;
#if HAVE_MKSTEMP
    int fd = mkstemp(name);
    if (fd >= 0)
	result = fdopen(fd, "w");
#else
    if (tmpnam(name) != 0)
	result = fopen(name, "w");
#endif
    return result;
}

int
main(int argc, char *argv[])
{
    char my_tmpname[PATH_MAX];
    int v_opt = -1, debug_level;
    int smart_defaults = TRUE;
    char *termcap;
    ENTRY *qp;

    int this_opt, last_opt = '?';

    int outform = F_TERMINFO;	/* output format */
    int sortmode = S_TERMINFO;	/* sort_mode */

    int width = 60;
    bool formatted = FALSE;	/* reformat complex strings? */
    int numbers = 0;		/* format "%'char'" to/from "%{number}" */
    bool infodump = FALSE;	/* running as captoinfo? */
    bool capdump = FALSE;	/* running as infotocap? */
    bool forceresolve = FALSE;	/* force resolution */
    bool limited = TRUE;
    char *tversion = (char *) NULL;
    const char *source_file = "terminfo";
    const char **namelst = 0;
    char *outdir = (char *) NULL;
    bool check_only = FALSE;
    bool suppress_untranslatable = FALSE;

    log_fp = stderr;

    _nc_progname = _nc_rootname(argv[0]);

    if ((infodump = (strcmp(_nc_progname, PROG_CAPTOINFO) == 0)) != FALSE) {
	outform = F_TERMINFO;
	sortmode = S_TERMINFO;
    }
    if ((capdump = (strcmp(_nc_progname, PROG_INFOTOCAP) == 0)) != FALSE) {
	outform = F_TERMCAP;
	sortmode = S_TERMCAP;
    }
#if NCURSES_XNAMES
    use_extended_names(FALSE);
#endif

    /*
     * Processing arguments is a little complicated, since someone made a
     * design decision to allow the numeric values for -w, -v options to
     * be optional.
     */
    while ((this_opt = getopt(argc, argv,
			      "0123456789CILNR:TVace:fGgo:rstvwx")) != EOF) {
	if (isdigit(this_opt)) {
	    switch (last_opt) {
	    case 'v':
		v_opt = (v_opt * 10) + (this_opt - '0');
		break;
	    case 'w':
		width = (width * 10) + (this_opt - '0');
		break;
	    default:
		if (this_opt != '1')
		    usage();
		last_opt = this_opt;
		width = 0;
	    }
	    continue;
	}
	switch (this_opt) {
	case 'C':
	    capdump = TRUE;
	    outform = F_TERMCAP;
	    sortmode = S_TERMCAP;
	    break;
	case 'I':
	    infodump = TRUE;
	    outform = F_TERMINFO;
	    sortmode = S_TERMINFO;
	    break;
	case 'L':
	    infodump = TRUE;
	    outform = F_VARIABLE;
	    sortmode = S_VARIABLE;
	    break;
	case 'N':
	    smart_defaults = FALSE;
	    break;
	case 'R':
	    tversion = optarg;
	    break;
	case 'T':
	    limited = FALSE;
	    break;
	case 'V':
	    puts(curses_version());
	    return EXIT_SUCCESS;
	case 'c':
	    check_only = TRUE;
	    break;
	case 'e':
	    namelst = make_namelist(optarg);
	    break;
	case 'f':
	    formatted = TRUE;
	    break;
	case 'G':
	    numbers = 1;
	    break;
	case 'g':
	    numbers = -1;
	    break;
	case 'o':
	    outdir = optarg;
	    break;
	case 'r':
	    forceresolve = TRUE;
	    break;
	case 's':
	    showsummary = TRUE;
	    break;
	case 'v':
	    v_opt = 0;
	    break;
	case 'w':
	    width = 0;
	    break;
#if NCURSES_XNAMES
	case 't':
	    _nc_disable_period = FALSE;
	    suppress_untranslatable = TRUE;
	    break;
	case 'a':
	    _nc_disable_period = TRUE;
	    /* FALLTHRU */
	case 'x':
	    use_extended_names(TRUE);
	    break;
#endif
	default:
	    usage();
	}
	last_opt = this_opt;
    }

    debug_level = (v_opt > 0) ? v_opt : (v_opt == 0);
    set_trace_level(debug_level);

    if (_nc_tracing) {
	save_check_termtype = _nc_check_termtype;
	_nc_check_termtype = check_termtype;
    }
#if !HAVE_BIG_CORE
    /*
     * Aaargh! immedhook seriously hoses us!
     *
     * One problem with immedhook is it means we can't do -e.  Problem
     * is that we can't guarantee that for each terminal listed, all the
     * terminals it depends on will have been kept in core for reference
     * resolution -- in fact it's certain the primitive types at the end
     * of reference chains *won't* be in core unless they were explicitly
     * in the select list themselves.
     */
    if (namelst && (!infodump && !capdump)) {
	(void) fprintf(stderr,
		       "Sorry, -e can't be used without -I or -C\n");
	cleanup();
	return EXIT_FAILURE;
    }
#endif /* HAVE_BIG_CORE */

    if (optind < argc) {
	source_file = argv[optind++];
	if (optind < argc) {
	    fprintf(stderr,
		    "%s: Too many file names.  Usage:\n\t%s %s",
		    _nc_progname,
		    _nc_progname,
		    usage_string);
	    return EXIT_FAILURE;
	}
    } else {
	if (infodump == TRUE) {
	    /* captoinfo's no-argument case */
	    source_file = "/etc/termcap";
	    if ((termcap = getenv("TERMCAP")) != 0
		&& (namelst = make_namelist(getenv("TERM"))) != 0) {
		if (access(termcap, F_OK) == 0) {
		    /* file exists */
		    source_file = termcap;
		} else if ((tmp_fp = open_tempfile(strcpy(my_tmpname,
							  "/tmp/XXXXXX")))
			   != 0) {
		    source_file = my_tmpname;
		    fprintf(tmp_fp, "%s\n", termcap);
		    fclose(tmp_fp);
		    tmp_fp = open_input(source_file);
		    to_remove = source_file;
		} else {
		    failed("tmpnam");
		}
	    }
	} else {
	    /* tic */
	    fprintf(stderr,
		    "%s: File name needed.  Usage:\n\t%s %s",
		    _nc_progname,
		    _nc_progname,
		    usage_string);
	    cleanup();
	    return EXIT_FAILURE;
	}
    }

    if (tmp_fp == 0)
	tmp_fp = open_input(source_file);

    if (infodump)
	dump_init(tversion,
		  smart_defaults
		  ? outform
		  : F_LITERAL,
		  sortmode, width, debug_level, formatted);
    else if (capdump)
	dump_init(tversion,
		  outform,
		  sortmode, width, debug_level, FALSE);

    /* parse entries out of the source file */
    _nc_set_source(source_file);
#if !HAVE_BIG_CORE
    if (!(check_only || infodump || capdump))
	_nc_set_writedir(outdir);
#endif /* HAVE_BIG_CORE */
    _nc_read_entry_source(tmp_fp, (char *) NULL,
			  !smart_defaults, FALSE,
			  (check_only || infodump || capdump) ? NULLHOOK : immedhook);

    /* do use resolution */
    if (check_only || (!infodump && !capdump) || forceresolve) {
	if (!_nc_resolve_uses(TRUE) && !check_only) {
	    cleanup();
	    return EXIT_FAILURE;
	}
    }

    /* length check */
    if (check_only && (capdump || infodump)) {
	for_entry_list(qp) {
	    if (matches(namelst, qp->tterm.term_names)) {
		int len = fmt_entry(&qp->tterm, NULL, FALSE, TRUE, infodump, numbers);

		if (len > (infodump ? MAX_TERMINFO_LENGTH : MAX_TERMCAP_LENGTH))
		    (void) fprintf(stderr,
				   "warning: resolved %s entry is %d bytes long\n",
				   _nc_first_name(qp->tterm.term_names),
				   len);
	    }
	}
    }

    /* write or dump all entries */
    if (!check_only) {
	if (!infodump && !capdump) {
	    _nc_set_writedir(outdir);
	    for_entry_list(qp) {
		if (matches(namelst, qp->tterm.term_names))
		    write_it(qp);
	    }
	} else {
	    /* this is in case infotocap() generates warnings */
	    _nc_curr_col = _nc_curr_line = -1;

	    for_entry_list(qp) {
		if (matches(namelst, qp->tterm.term_names)) {
		    int j = qp->cend - qp->cstart;
		    int len = 0;

		    /* this is in case infotocap() generates warnings */
		    _nc_set_type(_nc_first_name(qp->tterm.term_names));

		    (void) fseek(tmp_fp, qp->cstart, SEEK_SET);
		    while (j--) {
			if (infodump)
			    (void) putchar(fgetc(tmp_fp));
			else
			    put_translate(fgetc(tmp_fp));
		    }

		    len = dump_entry(&qp->tterm, suppress_untranslatable,
				     limited, 0, numbers, NULL);
		    for (j = 0; j < qp->nuses; j++)
			len += dump_uses(qp->uses[j].name, !capdump);
		    (void) putchar('\n');
		    if (debug_level != 0 && !limited)
			printf("# length=%d\n", len);
		}
	    }
	    if (!namelst && _nc_tail) {
		int c, oldc = '\0';
		bool in_comment = FALSE;
		bool trailing_comment = FALSE;

		(void) fseek(tmp_fp, _nc_tail->cend, SEEK_SET);
		while ((c = fgetc(tmp_fp)) != EOF) {
		    if (oldc == '\n') {
			if (c == '#') {
			    trailing_comment = TRUE;
			    in_comment = TRUE;
			} else {
			    in_comment = FALSE;
			}
		    }
		    if (trailing_comment
			&& (in_comment || (oldc == '\n' && c == '\n')))
			putchar(c);
		    oldc = c;
		}
	    }
	}
    }

    /* Show the directory into which entries were written, and the total
     * number of entries
     */
    if (showsummary
	&& (!(check_only || infodump || capdump))) {
	int total = _nc_tic_written();
	if (total != 0)
	    fprintf(log_fp, "%d entries written to %s\n",
		    total,
		    _nc_tic_dir((char *) 0));
	else
	    fprintf(log_fp, "No entries written\n");
    }
    cleanup();
    return (EXIT_SUCCESS);
}

/*
 * This bit of legerdemain turns all the terminfo variable names into
 * references to locations in the arrays Booleans, Numbers, and Strings ---
 * precisely what's needed (see comp_parse.c).
 */

TERMINAL *cur_term;		/* tweak to avoid linking lib_cur_term.c */

#undef CUR
#define CUR tp->

/*
 * Check if the alternate character-set capabilities are consistent.
 */
static void
check_acs(TERMTYPE * tp)
{
    if (VALID_STRING(acs_chars)) {
	const char *boxes = "lmkjtuvwqxn";
	char mapped[256];
	char missing[256];
	const char *p;
	char *q;

	memset(mapped, 0, sizeof(mapped));
	for (p = acs_chars; *p != '\0'; p += 2) {
	    if (p[1] == '\0') {
		_nc_warning("acsc has odd number of characters");
		break;
	    }
	    mapped[UChar(p[0])] = p[1];
	}
	if (mapped[UChar('I')] && !mapped[UChar('i')]) {
	    _nc_warning("acsc refers to 'I', which is probably an error");
	}
	for (p = boxes, q = missing; *p != '\0'; ++p) {
	    if (!mapped[UChar(p[0])]) {
		*q++ = p[0];
	    }
	    *q = '\0';
	}
	if (*missing != '\0' && strcmp(missing, boxes)) {
	    _nc_warning("acsc is missing some line-drawing mapping: %s", missing);
	}
    }
}

/*
 * Check if the color capabilities are consistent
 */
static void
check_colors(TERMTYPE * tp)
{
    if ((max_colors > 0) != (max_pairs > 0)
	|| ((max_colors > max_pairs) && (initialize_pair == 0)))
	_nc_warning("inconsistent values for max_colors (%d) and max_pairs (%d)",
		    max_colors, max_pairs);

    PAIRED(set_foreground, set_background);
    PAIRED(set_a_foreground, set_a_background);
    PAIRED(set_color_pair, initialize_pair);

    if (VALID_STRING(set_foreground)
	&& VALID_STRING(set_a_foreground)
	&& !strcmp(set_foreground, set_a_foreground))
	_nc_warning("expected setf/setaf to be different");

    if (VALID_STRING(set_background)
	&& VALID_STRING(set_a_background)
	&& !strcmp(set_background, set_a_background))
	_nc_warning("expected setb/setab to be different");

    /* see: has_colors() */
    if (VALID_NUMERIC(max_colors) && VALID_NUMERIC(max_pairs)
	&& (((set_foreground != NULL)
	     && (set_background != NULL))
	    || ((set_a_foreground != NULL)
		&& (set_a_background != NULL))
	    || set_color_pair)) {
	if (!VALID_STRING(orig_pair) && !VALID_STRING(orig_colors))
	    _nc_warning("expected either op/oc string for resetting colors");
    }
}

static int
keypad_final(const char *string)
{
    int result = '\0';

    if (VALID_STRING(string)
	&& *string++ == '\033'
	&& *string++ == 'O'
	&& strlen(string) == 1) {
	result = *string;
    }

    return result;
}

static int
keypad_index(const char *string)
{
    char *test;
    const char *list = "PQRSwxymtuvlqrsPpn";	/* app-keypad except "Enter" */
    int ch;
    int result = -1;

    if ((ch = keypad_final(string)) != '\0') {
	test = strchr(list, ch);
	if (test != 0)
	    result = (test - list);
    }
    return result;
}

/*
 * Do a quick sanity-check for vt100-style keypads to see if the 5-key keypad
 * is mapped inconsistently.
 */
static void
check_keypad(TERMTYPE * tp)
{
    char show[80];

    if (VALID_STRING(key_a1) &&
	VALID_STRING(key_a3) &&
	VALID_STRING(key_b2) &&
	VALID_STRING(key_c1) &&
	VALID_STRING(key_c3)) {
	char final[6];
	int list[5];
	int increase = 0;
	int j, k, kk;
	int last;
	int test;

	final[0] = keypad_final(key_a1);
	final[1] = keypad_final(key_a3);
	final[2] = keypad_final(key_b2);
	final[3] = keypad_final(key_c1);
	final[4] = keypad_final(key_c3);
	final[5] = '\0';

	/* special case: legacy coding using 1,2,3,0,. on the bottom */
	if (!strcmp(final, "qsrpn"))
	    return;

	list[0] = keypad_index(key_a1);
	list[1] = keypad_index(key_a3);
	list[2] = keypad_index(key_b2);
	list[3] = keypad_index(key_c1);
	list[4] = keypad_index(key_c3);

	/* check that they're all vt100 keys */
	for (j = 0; j < 5; ++j) {
	    if (list[j] < 0) {
		return;
	    }
	}

	/* check if they're all in increasing order */
	for (j = 1; j < 5; ++j) {
	    if (list[j] > list[j - 1]) {
		++increase;
	    }
	}
	if (increase != 4) {
	    show[0] = '\0';

	    for (j = 0, last = -1; j < 5; ++j) {
		for (k = 0, kk = -1, test = 100; k < 5; ++k) {
		    if (list[k] > last &&
			list[k] < test) {
			test = list[k];
			kk = k;
		    }
		}
		last = test;
		switch (kk) {
		case 0:
		    strcat(show, " ka1");
		    break;
		case 1:
		    strcat(show, " ka3");
		    break;
		case 2:
		    strcat(show, " kb2");
		    break;
		case 3:
		    strcat(show, " kc1");
		    break;
		case 4:
		    strcat(show, " kc3");
		    break;
		}
	    }

	    _nc_warning("vt100 keypad order inconsistent: %s", show);
	}

    } else if (VALID_STRING(key_a1) ||
	       VALID_STRING(key_a3) ||
	       VALID_STRING(key_b2) ||
	       VALID_STRING(key_c1) ||
	       VALID_STRING(key_c3)) {
	show[0] = '\0';
	if (keypad_index(key_a1) >= 0)
	    strcat(show, " ka1");
	if (keypad_index(key_a3) >= 0)
	    strcat(show, " ka3");
	if (keypad_index(key_b2) >= 0)
	    strcat(show, " kb2");
	if (keypad_index(key_c1) >= 0)
	    strcat(show, " kc1");
	if (keypad_index(key_c3) >= 0)
	    strcat(show, " kc3");
	if (*show != '\0')
	    _nc_warning("vt100 keypad map incomplete:%s", show);
    }
}

/*
 * Returns the expected number of parameters for the given capability.
 */
static int
expected_params(const char *name)
{
    /* *INDENT-OFF* */
    static const struct {
	const char *name;
	int count;
    } table[] = {
	{ "S0",			1 },	/* 'screen' extension */
	{ "birep",		2 },
	{ "chr",		1 },
	{ "colornm",		1 },
	{ "cpi",		1 },
	{ "csnm",		1 },
	{ "csr",		2 },
	{ "cub",		1 },
	{ "cud",		1 },
	{ "cuf",		1 },
	{ "cup",		2 },
	{ "cuu",		1 },
	{ "cvr",		1 },
	{ "cwin",		5 },
	{ "dch",		1 },
	{ "defc",		3 },
	{ "dial",		1 },
	{ "dispc",		1 },
	{ "dl",			1 },
	{ "ech",		1 },
	{ "getm",		1 },
	{ "hpa",		1 },
	{ "ich",		1 },
	{ "il",			1 },
	{ "indn",		1 },
	{ "initc",		4 },
	{ "initp",		7 },
	{ "lpi",		1 },
	{ "mc5p",		1 },
	{ "mrcup",		2 },
	{ "mvpa",		1 },
	{ "pfkey",		2 },
	{ "pfloc",		2 },
	{ "pfx",		2 },
	{ "pfxl",		3 },
	{ "pln",		2 },
	{ "qdial",		1 },
	{ "rcsd",		1 },
	{ "rep",		2 },
	{ "rin",		1 },
	{ "sclk",		3 },
	{ "scp",		1 },
	{ "scs",		1 },
	{ "scsd",		2 },
	{ "setab",		1 },
	{ "setaf",		1 },
	{ "setb",		1 },
	{ "setcolor",		1 },
	{ "setf",		1 },
	{ "sgr",		9 },
	{ "sgr1",		6 },
	{ "slength",		1 },
	{ "slines",		1 },
	{ "smgbp",		1 },	/* 2 if smgtp is not given */
	{ "smglp",		1 },
	{ "smglr",		2 },
	{ "smgrp",		1 },
	{ "smgtb",		2 },
	{ "smgtp",		1 },
	{ "tsl",		1 },
	{ "u6",			-1 },
	{ "vpa",		1 },
	{ "wind",		4 },
	{ "wingo",		1 },
    };
    /* *INDENT-ON* */

    unsigned n;
    int result = 0;		/* function-keys, etc., use none */

    for (n = 0; n < SIZEOF(table); n++) {
	if (!strcmp(name, table[n].name)) {
	    result = table[n].count;
	    break;
	}
    }

    return result;
}

/*
 * Make a quick sanity check for the parameters which are used in the given
 * strings.  If there are no "%p" tokens, then there should be no other "%"
 * markers.
 */
static void
check_params(TERMTYPE * tp, const char *name, char *value)
{
    int expected = expected_params(name);
    int actual = 0;
    int n;
    bool params[10];
    char *s = value;

#ifdef set_top_margin_parm
    if (!strcmp(name, "smgbp")
	&& set_top_margin_parm == 0)
	expected = 2;
#endif

    for (n = 0; n < 10; n++)
	params[n] = FALSE;

    while (*s != 0) {
	if (*s == '%') {
	    if (*++s == '\0') {
		_nc_warning("expected character after %% in %s", name);
		break;
	    } else if (*s == 'p') {
		if (*++s == '\0' || !isdigit((int) *s)) {
		    _nc_warning("expected digit after %%p in %s", name);
		    return;
		} else {
		    n = (*s - '0');
		    if (n > actual)
			actual = n;
		    params[n] = TRUE;
		}
	    }
	}
	s++;
    }

    if (params[0]) {
	_nc_warning("%s refers to parameter 0 (%%p0), which is not allowed", name);
    }
    if (value == set_attributes || expected < 0) {
	;
    } else if (expected != actual) {
	_nc_warning("%s uses %d parameters, expected %d", name,
		    actual, expected);
	for (n = 1; n < actual; n++) {
	    if (!params[n])
		_nc_warning("%s omits parameter %d", name, n);
	}
    }
}

static char *
skip_delay(char *s)
{
    while (*s == '/' || isdigit(UChar(*s)))
	++s;
    return s;
}

/*
 * An sgr string may contain several settings other than the one we're
 * interested in, essentially sgr0 + rmacs + whatever.  As long as the
 * "whatever" is contained in the sgr string, that is close enough for our
 * sanity check.
 */
static bool
similar_sgr(int num, char *a, char *b)
{
    static const char *names[] =
    {
	"none"
	,"standout"
	,"underline"
	,"reverse"
	,"blink"
	,"dim"
	,"bold"
	,"invis"
	,"protect"
	,"altcharset"
    };
    char *base_a = a;
    char *base_b = b;
    int delaying = 0;

    while (*b != 0) {
	while (*a != *b) {
	    if (*a == 0) {
		if (b[0] == '$'
		    && b[1] == '<') {
		    _nc_warning("Did not find delay %s", _nc_visbuf(b));
		} else {
		    _nc_warning("checking sgr(%s) %s\n\tcompare to %s\n\tunmatched %s",
				names[num], _nc_visbuf2(1, base_a),
				_nc_visbuf2(2, base_b),
				_nc_visbuf2(3, b));
		}
		return FALSE;
	    } else if (delaying) {
		a = skip_delay(a);
		b = skip_delay(b);
	    } else {
		a++;
	    }
	}
	switch (*a) {
	case '$':
	    if (delaying == 0)
		delaying = 1;
	    break;
	case '<':
	    if (delaying == 1)
		delaying = 2;
	    break;
	default:
	    delaying = 0;
	    break;
	}
	a++;
	b++;
    }
    return TRUE;
}

static void
check_sgr(TERMTYPE * tp, char *zero, int num, char *cap, const char *name)
{
    char *test = tparm(set_attributes,
		       num == 1,
		       num == 2,
		       num == 3,
		       num == 4,
		       num == 5,
		       num == 6,
		       num == 7,
		       num == 8,
		       num == 9);
    tparm_errs += _nc_tparm_err;
    if (test != 0) {
	if (PRESENT(cap)) {
	    if (!similar_sgr(num, test, cap)) {
		_nc_warning("%s differs from sgr(%d)\n\t%s=%s\n\tsgr(%d)=%s",
			    name, num,
			    name, _nc_visbuf2(1, cap),
			    num, _nc_visbuf2(2, test));
	    }
	} else if (strcmp(test, zero)) {
	    _nc_warning("sgr(%d) present, but not %s", num, name);
	}
    } else if (PRESENT(cap)) {
	_nc_warning("sgr(%d) missing, but %s present", num, name);
    }
}

#define CHECK_SGR(num,name) check_sgr(tp, zero, num, name, #name)

/* other sanity-checks (things that we don't want in the normal
 * logic that reads a terminfo entry)
 */
static void
check_termtype(TERMTYPE * tp)
{
    bool conflict = FALSE;
    unsigned j, k;
    char fkeys[STRCOUNT];

    /*
     * A terminal entry may contain more than one keycode assigned to
     * a given string (e.g., KEY_END and KEY_LL).  But curses will only
     * return one (the last one assigned).
     */
    memset(fkeys, 0, sizeof(fkeys));
    for (j = 0; _nc_tinfo_fkeys[j].code; j++) {
	char *a = tp->Strings[_nc_tinfo_fkeys[j].offset];
	bool first = TRUE;
	if (!VALID_STRING(a))
	    continue;
	for (k = j + 1; _nc_tinfo_fkeys[k].code; k++) {
	    char *b = tp->Strings[_nc_tinfo_fkeys[k].offset];
	    if (!VALID_STRING(b)
		|| fkeys[k])
		continue;
	    if (!strcmp(a, b)) {
		fkeys[j] = 1;
		fkeys[k] = 1;
		if (first) {
		    if (!conflict) {
			_nc_warning("Conflicting key definitions (using the last)");
			conflict = TRUE;
		    }
		    fprintf(stderr, "... %s is the same as %s",
			    keyname(_nc_tinfo_fkeys[j].code),
			    keyname(_nc_tinfo_fkeys[k].code));
		    first = FALSE;
		} else {
		    fprintf(stderr, ", %s",
			    keyname(_nc_tinfo_fkeys[k].code));
		}
	    }
	}
	if (!first)
	    fprintf(stderr, "\n");
    }

    for (j = 0; j < NUM_STRINGS(tp); j++) {
	char *a = tp->Strings[j];
	if (VALID_STRING(a))
	    check_params(tp, ExtStrname(tp, j, strnames), a);
    }

    check_acs(tp);
    check_colors(tp);
    check_keypad(tp);

    /*
     * These may be mismatched because the terminal description relies on
     * restoring the cursor visibility by resetting it.
     */
    ANDMISSING(cursor_invisible, cursor_normal);
    ANDMISSING(cursor_visible, cursor_normal);

    if (PRESENT(cursor_visible) && PRESENT(cursor_normal)
	&& !strcmp(cursor_visible, cursor_normal))
	_nc_warning("cursor_visible is same as cursor_normal");

    /*
     * From XSI & O'Reilly, we gather that sc/rc are required if csr is
     * given, because the cursor position after the scrolling operation is
     * performed is undefined.
     */
    ANDMISSING(change_scroll_region, save_cursor);
    ANDMISSING(change_scroll_region, restore_cursor);

    tparm_errs = 0;
    if (PRESENT(set_attributes)) {
	char *zero = tparm(set_attributes, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	zero = strdup(zero);
	CHECK_SGR(1, enter_standout_mode);
	CHECK_SGR(2, enter_underline_mode);
	CHECK_SGR(3, enter_reverse_mode);
	CHECK_SGR(4, enter_blink_mode);
	CHECK_SGR(5, enter_dim_mode);
	CHECK_SGR(6, enter_bold_mode);
	CHECK_SGR(7, enter_secure_mode);
	CHECK_SGR(8, enter_protected_mode);
	CHECK_SGR(9, enter_alt_charset_mode);
	free(zero);
	if (tparm_errs)
	    _nc_warning("stack error in sgr string");
    }

    /*
     * Some standard applications (e.g., vi) and some non-curses
     * applications (e.g., jove) get confused if we have both ich1 and
     * smir/rmir.  Let's be nice and warn about that, too, even though
     * ncurses handles it.
     */
    if ((PRESENT(enter_insert_mode) || PRESENT(exit_insert_mode))
	&& PRESENT(parm_ich)) {
	_nc_warning("non-curses applications may be confused by ich1 with smir/rmir");
    }

    /*
     * Finally, do the non-verbose checks
     */
    if (save_check_termtype != 0)
	save_check_termtype(tp);
}
