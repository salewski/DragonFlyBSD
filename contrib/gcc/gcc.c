/* Compiler driver program that can handle many languages.
   Copyright (C) 1987, 1989, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999,
   2000 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

This paragraph is here to try to keep Sun CC from dying.
The number of chars here seems crucial!!!!  */

/* $FreeBSD: src/contrib/gcc/gcc.c,v 1.17.2.9 2002/06/20 23:12:28 obrien Exp $ */
/* $DragonFly: src/contrib/gcc/Attic/gcc.c,v 1.6 2004/06/21 03:31:00 dillon Exp $ */

/* This program is the user interface to the C compiler and possibly to
other compilers.  It is used because compilation is a complicated procedure
which involves running several programs and passing temporary files between
them, forwarding the users switches to those programs selectively,
and deleting the temporary files at the end.

CC recognizes how to compile each input file by suffixes in the file names.
Once it knows which kind of compilation to perform, the procedure for
compilation is specified by a string called a "spec".  */

#include "config.h"
#include "system.h"
#include <signal.h>

#include "obstack.h"
#include "intl.h"
#include "prefix.h"

#ifdef VMS
#define exit __posix_exit
#endif

/* By default there is no special suffix for executables.  */
#ifdef EXECUTABLE_SUFFIX
#define HAVE_EXECUTABLE_SUFFIX
#else
#define EXECUTABLE_SUFFIX ""
#endif

/* By default, the suffix for object files is ".o".  */
#ifdef OBJECT_SUFFIX
#define HAVE_OBJECT_SUFFIX
#else
#define OBJECT_SUFFIX ".o"
#endif

/* By default, colon separates directories in a path.  */
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ':'
#endif

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

/* Define IS_DIR_SEPARATOR.  */
#ifndef DIR_SEPARATOR_2
# define IS_DIR_SEPARATOR(ch) ((ch) == DIR_SEPARATOR)
#else /* DIR_SEPARATOR_2 */
# define IS_DIR_SEPARATOR(ch) \
	(((ch) == DIR_SEPARATOR) || ((ch) == DIR_SEPARATOR_2))
#endif /* DIR_SEPARATOR_2 */

static char dir_separator_str[] = {DIR_SEPARATOR, 0};

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free

#ifndef GET_ENV_PATH_LIST
#define GET_ENV_PATH_LIST(VAR,NAME)	do { (VAR) = getenv (NAME); } while (0)
#endif

#ifndef HAVE_KILL
#define kill(p,s) raise(s)
#endif

/* If a stage of compilation returns an exit status >= 1,
   compilation of that file ceases.  */

#define MIN_FATAL_STATUS 1

/* Flag saying to print the directories gcc will search through looking for
   programs, libraries, etc.  */

static int print_search_dirs;

/* Flag saying to print the full filename of this file
   as found through our usual search mechanism.  */

static const char *print_file_name = NULL;

/* As print_file_name, but search for executable file.  */

static const char *print_prog_name = NULL;

/* Flag saying to print the relative path we'd use to
   find libgcc.a given the current compiler flags.  */

static int print_multi_directory;

/* Flag saying to print the list of subdirectories and
   compiler flags used to select them in a standard form.  */

static int print_multi_lib;

/* Flag saying to print the command line options understood by gcc and its
   sub-processes.  */

static int print_help_list;

/* Flag indicating whether we should print the command and arguments */

static int verbose_flag;

/* Nonzero means write "temp" files in source directory
   and use the source file's name in them, and don't delete them.  */

static int save_temps_flag;

/* The compiler version.  */

static char *compiler_version;

/* The target version specified with -V */

static char *spec_version = DEFAULT_TARGET_VERSION;

/* The target machine specified with -b.  */

static const char *spec_machine = DEFAULT_TARGET_MACHINE;

/* Nonzero if cross-compiling.
   When -b is used, the value comes from the `specs' file.  */

#ifdef CROSS_COMPILE
static char *cross_compile = "1";
#else
static char *cross_compile = "0";
#endif

/* The number of errors that have occurred; the link phase will not be
   run if this is non-zero.  */
static int error_count = 0;

/* This is the obstack which we use to allocate many strings.  */

static struct obstack obstack;

/* This is the obstack to build an environment variable to pass to
   collect2 that describes all of the relevant switches of what to
   pass the compiler in building the list of pointers to constructors
   and destructors.  */

static struct obstack collect_obstack;

extern char *version_string;

/* Forward declaration for prototypes.  */
struct path_prefix;

static void init_spec		PROTO((void));
static void read_specs		PROTO((const char *, int));
static void set_spec		PROTO((const char *, const char *));
static struct compiler *lookup_compiler PROTO((const char *, size_t, const char *));
static char *build_search_list	PROTO((struct path_prefix *, const char *, int));
static void putenv_from_prefixes PROTO((struct path_prefix *, const char *));
static char *find_a_file	PROTO((struct path_prefix *, const char *, int));
static void add_prefix		PROTO((struct path_prefix *, const char *,
				       const char *, int, int, int *));
static char *skip_whitespace	PROTO((char *));
static void record_temp_file	PROTO((const char *, int, int));
static void delete_if_ordinary	PROTO((const char *));
static void delete_temp_files	PROTO((void));
static void delete_failure_queue PROTO((void));
static void clear_failure_queue PROTO((void));
static int check_live_switch	PROTO((int, int));
static const char *handle_braces PROTO((const char *));
static char *save_string	PROTO((const char *, int));
extern int do_spec		PROTO((const char *));
static int do_spec_1		PROTO((const char *, int, const char *));
static const char *find_file	PROTO((const char *));
static int is_directory		PROTO((const char *, const char *, int));
static void validate_switches	PROTO((const char *));
static void validate_all_switches PROTO((void));
static void give_switch		PROTO((int, int, int));
static int used_arg		PROTO((const char *, int));
static int default_arg		PROTO((const char *, int));
static void set_multilib_dir	PROTO((void));
static void print_multilib_info	PROTO((void));
static void pfatal_with_name	PROTO((const char *)) ATTRIBUTE_NORETURN;
static void perror_with_name	PROTO((const char *));
static void pfatal_pexecute	PROTO((const char *, const char *))
  ATTRIBUTE_NORETURN;
static void error		PVPROTO((const char *, ...))
  ATTRIBUTE_PRINTF_1;
static void notice		PVPROTO((const char *, ...))
  ATTRIBUTE_PRINTF_1;
static void display_help 	PROTO((void));
static void add_preprocessor_option	PROTO ((const char *, int));
static void add_assembler_option	PROTO ((const char *, int));
static void add_linker_option		PROTO ((const char *, int));
static void process_command		PROTO ((int, char **));
static int execute			PROTO ((void));
static void unused_prefix_warnings	PROTO ((struct path_prefix *));
static void clear_args			PROTO ((void));
static void fatal_error			PROTO ((int));

void fancy_abort		PROTO((void)) ATTRIBUTE_NORETURN;

/* Called before processing to change/add/remove arguments. */
extern void lang_specific_driver PROTO ((void (*) PVPROTO((const char *, ...)),
					 int *, char ***, int *));

/* Called before linking.  Returns 0 on success and -1 on failure. */
extern int lang_specific_pre_link ();

/* Number of extra output files that lang_specific_pre_link may generate. */
extern int lang_specific_extra_outfiles;

/* Specs are strings containing lines, each of which (if not blank)
is made up of a program name, and arguments separated by spaces.
The program name must be exact and start from root, since no path
is searched and it is unreliable to depend on the current working directory.
Redirection of input or output is not supported; the subprograms must
accept filenames saying what files to read and write.

In addition, the specs can contain %-sequences to substitute variable text
or for conditional text.  Here is a table of all defined %-sequences.
Note that spaces are not generated automatically around the results of
expanding these sequences; therefore, you can concatenate them together
or with constant text in a single argument.

 %%	substitute one % into the program name or argument.
 %i     substitute the name of the input file being processed.
 %b     substitute the basename of the input file being processed.
	This is the substring up to (and not including) the last period
	and not including the directory.
 %gSUFFIX
	substitute a file name that has suffix SUFFIX and is chosen
	once per compilation, and mark the argument a la %d.  To reduce
	exposure to denial-of-service attacks, the file name is now
	chosen in a way that is hard to predict even when previously
	chosen file names are known.  For example, `%g.s ... %g.o ... %g.s'
	might turn into `ccUVUUAU.s ccXYAXZ12.o ccUVUUAU.s'.  SUFFIX matches
	the regexp "[.A-Za-z]*" or the special string "%O", which is
	treated exactly as if %O had been pre-processed.  Previously, %g
	was simply substituted with a file name chosen once per compilation,
	without regard to any appended suffix (which was therefore treated
	just like ordinary text), making such attacks more likely to succeed.
 %uSUFFIX
	like %g, but generates a new temporary file name even if %uSUFFIX
	was already seen.
 %USUFFIX
	substitutes the last file name generated with %uSUFFIX, generating a
	new one if there is no such last file name.  In the absence of any
	%uSUFFIX, this is just like %gSUFFIX, except they don't share
	the same suffix "space", so `%g.s ... %U.s ... %g.s ... %U.s'
	would involve the generation of two distinct file names, one
	for each `%g.s' and another for each `%U.s'.  Previously, %U was
	simply substituted with a file name chosen for the previous %u,
	without regard to any appended suffix.
 %d	marks the argument containing or following the %d as a
	temporary file name, so that that file will be deleted if CC exits
	successfully.  Unlike %g, this contributes no text to the argument.
 %w	marks the argument containing or following the %w as the
	"output file" of this compilation.  This puts the argument
	into the sequence of arguments that %o will substitute later.
 %W{...}
	like %{...} but mark last argument supplied within
	as a file to be deleted on failure.
 %o	substitutes the names of all the output files, with spaces
	automatically placed around them.  You should write spaces
	around the %o as well or the results are undefined.
	%o is for use in the specs for running the linker.
	Input files whose names have no recognized suffix are not compiled
	at all, but they are included among the output files, so they will
	be linked.
 %O	substitutes the suffix for object files.  Note that this is
	handled specially when it immediately follows %g, %u, or %U,
	because of the need for those to form complete file names.  The
	handling is such that %O is treated exactly as if it had already
	been substituted, except that %g, %u, and %U do not currently
	support additional SUFFIX characters following %O as they would
	following, for example, `.o'.
 %p	substitutes the standard macro predefinitions for the
	current target machine.  Use this when running cpp.
 %P	like %p, but puts `__' before and after the name of each macro.
	(Except macros that already have __.)
	This is for ANSI C.
 %I	Substitute a -iprefix option made from GCC_EXEC_PREFIX.
 %s     current argument is the name of a library or startup file of some sort.
        Search for that file in a standard list of directories
	and substitute the full name found.
 %eSTR  Print STR as an error message.  STR is terminated by a newline.
        Use this when inconsistent options are detected.
 %x{OPTION}	Accumulate an option for %X.
 %X	Output the accumulated linker options specified by compilations.
 %Y	Output the accumulated assembler options specified by compilations.
 %Z	Output the accumulated preprocessor options specified by compilations.
 %v1	Substitute the major version number of GCC.
	(For version 2.5.n, this is 2.)
 %v2	Substitute the minor version number of GCC.
	(For version 2.5.n, this is 5.)
 %a     process ASM_SPEC as a spec.
        This allows config.h to specify part of the spec for running as.
 %A	process ASM_FINAL_SPEC as a spec.  A capital A is actually
	used here.  This can be used to run a post-processor after the
	assembler has done its job.
 %D	Dump out a -L option for each directory in startfile_prefixes.
	If multilib_dir is set, extra entries are generated with it affixed.
 %l     process LINK_SPEC as a spec.
 %L     process LIB_SPEC as a spec.
 %G     process LIBGCC_SPEC as a spec.
 %S     process STARTFILE_SPEC as a spec.  A capital S is actually used here.
 %E     process ENDFILE_SPEC as a spec.  A capital E is actually used here.
 %c	process SIGNED_CHAR_SPEC as a spec.
 %C     process CPP_SPEC as a spec.  A capital C is actually used here.
 %1	process CC1_SPEC as a spec.
 %2	process CC1PLUS_SPEC as a spec.
 %|	output "-" if the input for the current command is coming from a pipe.
 %*	substitute the variable part of a matched option.  (See below.)
	Note that each comma in the substituted string is replaced by
	a single space.
 %{S}   substitutes the -S switch, if that switch was given to CC.
	If that switch was not specified, this substitutes nothing.
	Here S is a metasyntactic variable.
 %{S*}  substitutes all the switches specified to CC whose names start
	with -S.  This is used for -o, -D, -I, etc; switches that take
	arguments.  CC considers `-o foo' as being one switch whose
	name starts with `o'.  %{o*} would substitute this text,
	including the space; thus, two arguments would be generated.
 %{^S*} likewise, but don't put a blank between a switch and any args.
 %{S*:X} substitutes X if one or more switches whose names start with -S are
	specified to CC.  Note that the tail part of the -S option
	(i.e. the part matched by the `*') will be substituted for each
	occurrence of %* within X.
 %{S:X} substitutes X, but only if the -S switch was given to CC.
 %{!S:X} substitutes X, but only if the -S switch was NOT given to CC.
 %{|S:X} like %{S:X}, but if no S switch, substitute `-'.
 %{|!S:X} like %{!S:X}, but if there is an S switch, substitute `-'.
 %{.S:X} substitutes X, but only if processing a file with suffix S.
 %{!.S:X} substitutes X, but only if NOT processing a file with suffix S.
 %{S|P:X} substitutes X if either -S or -P was given to CC.  This may be
	  combined with ! and . as above binding stronger than the OR.
 %(Spec) processes a specification defined in a specs file as *Spec:
 %[Spec] as above, but put __ around -D arguments

The conditional text X in a %{S:X} or %{!S:X} construct may contain
other nested % constructs or spaces, or even newlines.  They are
processed as usual, as described above.

The -O, -f, -m, and -W switches are handled specifically in these
constructs.  If another value of -O or the negated form of a -f, -m, or
-W switch is found later in the command line, the earlier switch
value is ignored, except with {S*} where S is just one letter; this
passes all matching options.

The character | at the beginning of the predicate text is used to indicate
that a command should be piped to the following command, but only if -pipe
is specified.

Note that it is built into CC which switches take arguments and which
do not.  You might think it would be useful to generalize this to
allow each compiler's spec to say which switches take arguments.  But
this cannot be done in a consistent fashion.  CC cannot even decide
which input files have been specified without knowing which switches
take arguments, and it must know which input files to compile in order
to tell which compilers to run.

CC also knows implicitly that arguments starting in `-l' are to be
treated as compiler output files, and passed to the linker in their
proper position among the other output files.  */

/* Define the macros used for specs %a, %l, %L, %S, %c, %C, %1.  */

/* config.h can define ASM_SPEC to provide extra args to the assembler
   or extra switch-translations.  */
#ifndef ASM_SPEC
#define ASM_SPEC ""
#endif

/* config.h can define ASM_FINAL_SPEC to run a post processor after
   the assembler has run.  */
#ifndef ASM_FINAL_SPEC
#define ASM_FINAL_SPEC ""
#endif

/* config.h can define CPP_SPEC to provide extra args to the C preprocessor
   or extra switch-translations.  */
#ifndef CPP_SPEC
#define CPP_SPEC ""
#endif

/* config.h can define CC1_SPEC to provide extra args to cc1 and cc1plus
   or extra switch-translations.  */
#ifndef CC1_SPEC
#define CC1_SPEC ""
#endif

/* config.h can define CC1PLUS_SPEC to provide extra args to cc1plus
   or extra switch-translations.  */
#ifndef CC1PLUS_SPEC
#define CC1PLUS_SPEC ""
#endif

/* config.h can define LINK_SPEC to provide extra args to the linker
   or extra switch-translations.  */
#ifndef LINK_SPEC
#define LINK_SPEC ""
#endif

/* config.h can define LIB_SPEC to override the default libraries.  */
#ifndef LIB_SPEC
#define LIB_SPEC "%{!shared:%{g*:-lg} %{!p:%{!pg:-lc}}%{p:-lc_p}%{pg:-lc_p}}"
#endif

/* config.h can define LIBGCC_SPEC to override how and when libgcc.a is
   included.  */
#ifndef LIBGCC_SPEC
#if defined(LINK_LIBGCC_SPECIAL) || defined(LINK_LIBGCC_SPECIAL_1)
/* Have gcc do the search for libgcc.a.  */
#define LIBGCC_SPEC "libgcc.a%s"
#else
#define LIBGCC_SPEC "-lgcc"
#endif
#endif

/* config.h can define STARTFILE_SPEC to override the default crt0 files.  */
#ifndef STARTFILE_SPEC
#define STARTFILE_SPEC  \
  "%{!shared:%{pg:gcrt0%O%s}%{!pg:%{p:mcrt0%O%s}%{!p:crt0%O%s}}}"
#endif

/* config.h can define SWITCHES_NEED_SPACES to control which options
   require spaces between the option and the argument.  */
#ifndef SWITCHES_NEED_SPACES
#define SWITCHES_NEED_SPACES ""
#endif

/* config.h can define ENDFILE_SPEC to override the default crtn files.  */
#ifndef ENDFILE_SPEC
#define ENDFILE_SPEC ""
#endif

/* This spec is used for telling cpp whether char is signed or not.  */
#ifndef SIGNED_CHAR_SPEC
/* Use #if rather than ?:
   because MIPS C compiler rejects like ?: in initializers.  */
#if DEFAULT_SIGNED_CHAR
#define SIGNED_CHAR_SPEC "%{funsigned-char:-D__CHAR_UNSIGNED__}"
#else
#define SIGNED_CHAR_SPEC "%{!fsigned-char:-D__CHAR_UNSIGNED__}"
#endif
#endif

#ifndef LINKER_NAME
#define LINKER_NAME "collect2"
#endif

static char *cpp_spec = CPP_SPEC;
static char *cpp_predefines = CPP_PREDEFINES;
static char *cc1_spec = CC1_SPEC;
static char *cc1plus_spec = CC1PLUS_SPEC;
static char *signed_char_spec = SIGNED_CHAR_SPEC;
static char *asm_spec = ASM_SPEC;
static char *asm_final_spec = ASM_FINAL_SPEC;
static char *link_spec = LINK_SPEC;
static char *lib_spec = LIB_SPEC;
static char *libgcc_spec = LIBGCC_SPEC;
static char *endfile_spec = ENDFILE_SPEC;
static char *startfile_spec = STARTFILE_SPEC;
static char *switches_need_spaces = SWITCHES_NEED_SPACES;
static char *linker_name_spec = LINKER_NAME;

/* Some compilers have limits on line lengths, and the multilib_select
   and/or multilib_matches strings can be very long, so we build them at
   run time.  */
static struct obstack multilib_obstack;
static char *multilib_select;
static char *multilib_matches;
static char *multilib_defaults;
#include "multilib.h"

/* Check whether a particular argument is a default argument.  */

#ifndef MULTILIB_DEFAULTS
#define MULTILIB_DEFAULTS { "" }
#endif

static char *multilib_defaults_raw[] = MULTILIB_DEFAULTS;

struct user_specs {
  struct user_specs *next;
  const char *filename;
};

static struct user_specs *user_specs_head, *user_specs_tail;

/* This defines which switch letters take arguments.  */

#define DEFAULT_SWITCH_TAKES_ARG(CHAR) \
  ((CHAR) == 'D' || (CHAR) == 'U' || (CHAR) == 'o' \
   || (CHAR) == 'e' || (CHAR) == 'T' || (CHAR) == 'u' \
   || (CHAR) == 'I' || (CHAR) == 'm' || (CHAR) == 'x' \
   || (CHAR) == 'L' || (CHAR) == 'A' || (CHAR) == 'V' \
   || (CHAR) == 'B' || (CHAR) == 'b')

#ifndef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR) DEFAULT_SWITCH_TAKES_ARG(CHAR)
#endif

/* This defines which multi-letter switches take arguments.  */

#define DEFAULT_WORD_SWITCH_TAKES_ARG(STR)		\
 (!strcmp (STR, "Tdata") || !strcmp (STR, "Ttext")	\
  || !strcmp (STR, "Tbss") || !strcmp (STR, "include")	\
  || !strcmp (STR, "imacros") || !strcmp (STR, "aux-info") \
  || !strcmp (STR, "idirafter") || !strcmp (STR, "iprefix") \
  || !strcmp (STR, "iwithprefix") || !strcmp (STR, "iwithprefixbefore") \
  || !strcmp (STR, "isystem") || !strcmp (STR, "specs"))

#ifndef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR) DEFAULT_WORD_SWITCH_TAKES_ARG (STR)
#endif


#ifdef HAVE_EXECUTABLE_SUFFIX
/* This defines which switches stop a full compilation.  */
#define DEFAULT_SWITCH_CURTAILS_COMPILATION(CHAR) \
  ((CHAR) == 'c' || (CHAR) == 'S')

#ifndef SWITCH_CURTAILS_COMPILATION
#define SWITCH_CURTAILS_COMPILATION(CHAR) \
  DEFAULT_SWITCH_CURTAILS_COMPILATION(CHAR)
#endif
#endif

/* Record the mapping from file suffixes for compilation specs.  */

struct compiler
{
  const char *suffix;		/* Use this compiler for input files
				   whose names end in this suffix.  */

  const char *spec[4];		/* To use this compiler, concatenate these
				   specs and pass to do_spec.  */
};

/* Pointer to a vector of `struct compiler' that gives the spec for
   compiling a file, based on its suffix.
   A file that does not end in any of these suffixes will be passed
   unchanged to the loader and nothing else will be done to it.

   An entry containing two 0s is used to terminate the vector.

   If multiple entries match a file, the last matching one is used.  */

static struct compiler *compilers;

/* Number of entries in `compilers', not counting the null terminator.  */

static int n_compilers;

/* The default list of file name suffixes and their compilation specs.  */

static struct compiler default_compilers[] =
{
  /* Add lists of suffixes of known languages here.  If those languages
     were not present when we built the driver, we will hit these copies
     and be given a more meaningful error than "file not used since
     linking is not done".  */
  {".m", {"#Objective-C"}},
  {".cc", {"#C++"}}, {".cxx", {"#C++"}}, {".cpp", {"#C++"}},
  {".c++", {"#C++"}}, {".C", {"#C++"}},
  {".ads", {"#Ada"}}, {".adb", {"#Ada"}}, {".ada", {"#Ada"}},
  {".f", {"#Fortran"}}, {".for", {"#Fortran"}}, {".F", {"#Fortran"}},
  {".fpp", {"#Fortran"}},
  {".p", {"#Pascal"}}, {".pas", {"#Pascal"}},
  /* Next come the entries for C.  */
  {".c", {"@c"}},
  {"@c",
   {
#if USE_CPPLIB
     "%{E|M|MM:cpp0 -lang-c %{ansi:-std=c89} %{std*} %{nostdinc*}\
	%{C} %{v} %{A*} %{I*} %{P} %{$} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
        %{!no-gcc:-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2}\
	%{ansi|std=*:%{!std=gnu*:-trigraphs -D__STRICT_ANSI__}}\
	%{!undef:%{!ansi:%{!std=*:%p}%{std=gnu*:%p}} %P} %{trigraphs}\
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{ffast-math:-D__FAST_MATH__}\
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{fleading-underscore} %{fno-leading-underscore}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}}\n}\
      %{!E:%{!M:%{!MM:cc1 %i %1 \
                  %{std*} %{nostdinc*} %{A*} %{I*} %I\
                  %{!Q:-quiet} -dumpbase %b.c %{d*} %{m*} %{a*}\
                  %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
                  %{!no-gcc:-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2}\
		  %{ansi|std=*:%{!std=gnu*:-trigraphs -D__STRICT_ANSI__}}\
		  %{!undef:%{!ansi:%{!std=*:%p}%{std=gnu*:%p}} %P} %{trigraphs}\
                  %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
		  %{ffast-math:-D__FAST_MATH__}\
                  %{H} %C %{D*} %{U*} %{i*} %Z\
                  %{ftraditional:-traditional}\
                  %{traditional-cpp:-traditional}\
		  %{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
		  %{aux-info*} %{Qn:-fno-ident}\
		  %{--help:--help}\
		  %{g*} %{O*} %{W*} %{w} %{pedantic*}\
		  %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		  %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
                  %{!S:as %a %Y\
		     %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
                     %{!pipe:%g.s} %A\n }}}}"
#else /* ! USE_CPPLIB */
    "cpp0 -lang-c %{ansi:-std=c89} %{std*} %{nostdinc*}\
	%{C} %{v} %{A*} %{I*} %{P} %{$} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
        %{!no-gcc:-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2}\
	%{ansi|std=*:%{!std=gnu*:-trigraphs -D__STRICT_ANSI__}}\
	%{!undef:%{!ansi:%{!std=*:%p}%{std=gnu*:%p}} %P} %{trigraphs}\
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{ffast-math:-D__FAST_MATH__}\
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{fleading-underscore} %{fno-leading-underscore}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %{!M:%{!MM:%{!E:%{!pipe:%g.i}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",
   "%{!M:%{!MM:%{!E:cc1 %{!pipe:%g.i} %1 \
		   %{!Q:-quiet} -dumpbase %b.c %{d*} %{m*} %{a*}\
		   %{g*} %{O*} %{W*} %{w} %{pedantic*} %{std*}\
		   %{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
		   %{aux-info*} %{Qn:-fno-ident}\
		   %{--help:--help} \
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
              %{!S:as %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
                      %{!pipe:%g.s} %A\n }}}}"
#endif /* ! USE_CPPLIB */
  }},
  {"-",
   {"%{E:cpp0 -lang-c %{ansi:-std=c89} %{std*} %{nostdinc*}\
	%{C} %{v} %{A*} %{I*} %{P} %{$} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
        %{!no-gcc:-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2}\
	%{ansi|std=*:%{!std=gnu*:-trigraphs -D__STRICT_ANSI__}}\
	%{!undef:%{!ansi:%{!std=*:%p}%{std=gnu*:%p}} %P} %{trigraphs}\
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{ffast-math:-D__FAST_MATH__}\
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{fleading-underscore} %{fno-leading-underscore}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %W{o*}}\
    %{!E:%e-E required when input is from standard input}"}},
  {".h", {"@c-header"}},
  {"@c-header",
   {"%{!E:%eCompilation of header file requested} \
    cpp0 %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %{$} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
        %{!no-gcc:-D__GNUC__=%v1 -D__GNUC_MINOR__=%v2}\
	%{std=*:%{!std=gnu*:-trigraphs -D__STRICT_ANSI__}}\
	%{!undef:%{!std=*:%p}%{std=gnu*:%p} %P} %{trigraphs}\
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{ffast-math:-D__FAST_MATH__}\
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{fleading-underscore} %{fno-leading-underscore}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %W{o*}"}},
  {".i", {"@cpp-output"}},
  {"@cpp-output",
   {"%{!M:%{!MM:%{!E:cc1 %i %1 %{!Q:-quiet} %{d*} %{m*} %{a*}\
			%{g*} %{O*} %{W*} %{w} %{pedantic*} %{std*}\
			%{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
			%{aux-info*} %{Qn:-fno-ident}\
			%{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
			%{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
		     %{!S:as %a %Y\
			     %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
			     %{!pipe:%g.s} %A\n }}}}"}},
  {".s", {"@assembler"}},
  {"@assembler",
   {"%{!M:%{!MM:%{!E:%{!S:as %a %Y\
		            %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
			    %i %A\n }}}}"}},
  {".S", {"@assembler-with-cpp"}},
  {"@assembler-with-cpp",
   {"cpp0 -lang-asm %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %{$} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG} %{trigraphs}\
        -$ %{!undef:%p %P} -D__ASSEMBLER__ \
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
	%{ffast-math:-D__FAST_MATH__}\
        %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{fleading-underscore} %{fno-leading-underscore}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %{!M:%{!MM:%{!E:%{!pipe:%g.s}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",
    "%{!M:%{!MM:%{!E:%{!S:as %a %Y\
                    %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
		    %{!pipe:%g.s} %A\n }}}}"}},
#include "specs.h"
  /* Mark end of table */
  {0, {0}}
};

/* Number of elements in default_compilers, not counting the terminator.  */

static int n_default_compilers
  = (sizeof default_compilers / sizeof (struct compiler)) - 1;

/* Here is the spec for running the linker, after compiling all files.  */

/* -u* was put back because both BSD and SysV seem to support it.  */
/* %{static:} simply prevents an error message if the target machine
   doesn't handle -static.  */
/* We want %{T*} after %{L*} and %D so that it can be used to specify linker
   scripts which exist in user specified directories, or in standard
   directories.  */
#ifdef LINK_COMMAND_SPEC
/* Provide option to override link_command_spec from machine specific
   configuration files.  */
static const char *link_command_spec = 
	LINK_COMMAND_SPEC;
#else
#ifdef LINK_LIBGCC_SPECIAL
/* Don't generate -L options.  */
static const char *link_command_spec = "\
%{!fsyntax-only: \
 %{!c:%{!M:%{!MM:%{!E:%{!S:%(linker) %l %X %{o*} %{A} %{d} %{e*} %{m} %{N} %{n} \
			%{r} %{s} %{t} %{u*} %{x} %{z} %{Z}\
			%{!A:%{!nostdlib:%{!nostartfiles:%S}}}\
			%{static:} %{L*} %o\
			%{!nostdlib:%{!nodefaultlibs:%G %L %G}}\
			%{!A:%{!nostdlib:%{!nostartfiles:%E}}}\
			%{T*}\
			\n }}}}}}";
#else
/* Use -L.  */
static const char *link_command_spec = "\
%{!fsyntax-only: \
 %{!c:%{!M:%{!MM:%{!E:%{!S:%(linker) %l %X %{o*} %{A} %{d} %{e*} %{m} %{N} %{n} \
			%{r} %{s} %{t} %{u*} %{x} %{z} %{Z}\
			%{!A:%{!nostdlib:%{!nostartfiles:%S}}}\
			%{static:} %{L*} %D %o\
			%{!nostdlib:%{!nodefaultlibs:%G %L %G}}\
			%{!A:%{!nostdlib:%{!nostartfiles:%E}}}\
			%{T*}\
			\n }}}}}}";
#endif
#endif

/* A vector of options to give to the linker.
   These options are accumulated by %x,
   and substituted into the linker command with %X.  */
static int n_linker_options;
static char **linker_options;

/* A vector of options to give to the assembler.
   These options are accumulated by -Wa,
   and substituted into the assembler command with %Y.  */
static int n_assembler_options;
static char **assembler_options;

/* A vector of options to give to the preprocessor.
   These options are accumulated by -Wp,
   and substituted into the preprocessor command with %Z.  */
static int n_preprocessor_options;
static char **preprocessor_options;

/* Define how to map long options into short ones.  */

/* This structure describes one mapping.  */
struct option_map
{
  /* The long option's name.  */
  const char *name;
  /* The equivalent short option.  */
  const char *equivalent;
  /* Argument info.  A string of flag chars; NULL equals no options.
     a => argument required.
     o => argument optional.
     j => join argument to equivalent, making one word.
     * => require other text after NAME as an argument.  */
  const char *arg_info;
};

/* This is the table of mappings.  Mappings are tried sequentially
   for each option encountered; the first one that matches, wins.  */

struct option_map option_map[] =
 {
   {"--all-warnings", "-Wall", 0},
   {"--ansi", "-ansi", 0},
   {"--assemble", "-S", 0},
   {"--assert", "-A", "a"},
   {"--classpath", "-fclasspath=", "aj"},
   {"--CLASSPATH", "-fCLASSPATH=", "aj"},
   {"--comments", "-C", 0},
   {"--compile", "-c", 0},
   {"--debug", "-g", "oj"},
   {"--define-macro", "-D", "aj"},
   {"--dependencies", "-M", 0},
   {"--dump", "-d", "a"},
   {"--dumpbase", "-dumpbase", "a"},
   {"--entry", "-e", 0},
   {"--extra-warnings", "-W", 0},
   {"--for-assembler", "-Wa", "a"},
   {"--for-linker", "-Xlinker", "a"},
   {"--force-link", "-u", "a"},
   {"--imacros", "-imacros", "a"},
   {"--include", "-include", "a"},
   {"--include-barrier", "-I-", 0},
   {"--include-directory", "-I", "aj"},
   {"--include-directory-after", "-idirafter", "a"},
   {"--include-prefix", "-iprefix", "a"},
   {"--include-with-prefix", "-iwithprefix", "a"},
   {"--include-with-prefix-before", "-iwithprefixbefore", "a"},
   {"--include-with-prefix-after", "-iwithprefix", "a"},
   {"--language", "-x", "a"},
   {"--library-directory", "-L", "a"},
   {"--machine", "-m", "aj"},
   {"--machine-", "-m", "*j"},
   {"--no-line-commands", "-P", 0},
   {"--no-precompiled-includes", "-noprecomp", 0},
   {"--no-standard-includes", "-nostdinc", 0},
   {"--no-standard-libraries", "-nostdlib", 0},
   {"--no-warnings", "-w", 0},
   {"--optimize", "-O", "oj"},
   {"--output", "-o", "a"},
   {"--output-class-directory", "-foutput-class-dir=", "ja"},
   {"--pedantic", "-pedantic", 0},
   {"--pedantic-errors", "-pedantic-errors", 0},
   {"--pipe", "-pipe", 0},
   {"--prefix", "-B", "a"},
   {"--preprocess", "-E", 0},
   {"--print-search-dirs", "-print-search-dirs", 0},
   {"--print-file-name", "-print-file-name=", "aj"},
   {"--print-libgcc-file-name", "-print-libgcc-file-name", 0},
   {"--print-missing-file-dependencies", "-MG", 0},
   {"--print-multi-lib", "-print-multi-lib", 0},
   {"--print-multi-directory", "-print-multi-directory", 0},
   {"--print-prog-name", "-print-prog-name=", "aj"},
   {"--profile", "-p", 0},
   {"--profile-blocks", "-a", 0},
   {"--quiet", "-q", 0},
   {"--save-temps", "-save-temps", 0},
   {"--shared", "-shared", 0},
   {"--silent", "-q", 0},
   {"--specs", "-specs=", "aj"},
   {"--static", "-static", 0},
   {"--std", "-std=", "aj"},
   {"--symbolic", "-symbolic", 0},
   {"--target", "-b", "a"},
   {"--trace-includes", "-H", 0},
   {"--traditional", "-traditional", 0},
   {"--traditional-cpp", "-traditional-cpp", 0},
   {"--trigraphs", "-trigraphs", 0},
   {"--undefine-macro", "-U", "aj"},
   {"--use-version", "-V", "a"},
   {"--user-dependencies", "-MM", 0},
   {"--verbose", "-v", 0},
   {"--version", "-dumpversion", 0},
   {"--warn-", "-W", "*j"},
   {"--write-dependencies", "-MD", 0},
   {"--write-user-dependencies", "-MMD", 0},
   {"--", "-f", "*j"}
 };

/* Translate the options described by *ARGCP and *ARGVP.
   Make a new vector and store it back in *ARGVP,
   and store its length in *ARGVC.  */

static void
translate_options (argcp, argvp)
     int *argcp;
     const char ***argvp;
{
  int i;
  int argc = *argcp;
  const char **argv = *argvp;
  const char **newv =
    (const char **) xmalloc ((argc + 2) * 2 * sizeof (const char *));
  int newindex = 0;

  i = 0;
  newv[newindex++] = argv[i++];

  while (i < argc)
    {
      /* Translate -- options.  */
      if (argv[i][0] == '-' && argv[i][1] == '-')
	{
	  size_t j;
	  /* Find a mapping that applies to this option.  */
	  for (j = 0; j < sizeof (option_map) / sizeof (option_map[0]); j++)
	    {
	      size_t optlen = strlen (option_map[j].name);
	      size_t arglen = strlen (argv[i]);
	      size_t complen = arglen > optlen ? optlen : arglen;
	      const char *arginfo = option_map[j].arg_info;

	      if (arginfo == 0)
		arginfo = "";

	      if (!strncmp (argv[i], option_map[j].name, complen))
		{
		  const char *arg = 0;

		  if (arglen < optlen)
		    {
		      size_t k;
		      for (k = j + 1;
			   k < sizeof (option_map) / sizeof (option_map[0]);
			   k++)
			if (strlen (option_map[k].name) >= arglen
			    && !strncmp (argv[i], option_map[k].name, arglen))
			  {
			    error ("Ambiguous abbreviation %s", argv[i]);
			    break;
			  }

		      if (k != sizeof (option_map) / sizeof (option_map[0]))
			break;
		    }

		  if (arglen > optlen)
		    {
		      /* If the option has an argument, accept that.  */
		      if (argv[i][optlen] == '=')
			arg = argv[i] + optlen + 1;

		      /* If this mapping requires extra text at end of name,
			 accept that as "argument".  */
		      else if (index (arginfo, '*') != 0)
			arg = argv[i] + optlen;

		      /* Otherwise, extra text at end means mismatch.
			 Try other mappings.  */
		      else
			continue;
		    }

		  else if (index (arginfo, '*') != 0)
		    {
		      error ("Incomplete `%s' option", option_map[j].name);
		      break;
		    }

		  /* Handle arguments.  */
		  if (index (arginfo, 'a') != 0)
		    {
		      if (arg == 0)
			{
			  if (i + 1 == argc)
			    {
			      error ("Missing argument to `%s' option",
				     option_map[j].name);
			      break;
			    }

			  arg = argv[++i];
			}
		    }
		  else if (index (arginfo, '*') != 0)
		    ;
		  else if (index (arginfo, 'o') == 0)
		    {
		      if (arg != 0)
			error ("Extraneous argument to `%s' option",
			       option_map[j].name);
		      arg = 0;
		    }

		  /* Store the translation as one argv elt or as two.  */
		  if (arg != 0 && index (arginfo, 'j') != 0)
		    newv[newindex++] = concat (option_map[j].equivalent, arg,
					       NULL_PTR);
		  else if (arg != 0)
		    {
		      newv[newindex++] = option_map[j].equivalent;
		      newv[newindex++] = arg;
		    }
		  else
		    newv[newindex++] = option_map[j].equivalent;

		  break;
		}
	    }
	  i++;
	}

      /* Handle old-fashioned options--just copy them through,
	 with their arguments.  */
      else if (argv[i][0] == '-')
	{
	  const char *p = argv[i] + 1;
	  int c = *p;
	  int nskip = 1;

	  if (SWITCH_TAKES_ARG (c) > (p[1] != 0))
	    nskip += SWITCH_TAKES_ARG (c) - (p[1] != 0);
	  else if (WORD_SWITCH_TAKES_ARG (p))
	    nskip += WORD_SWITCH_TAKES_ARG (p);
	  else if ((c == 'B' || c == 'b' || c == 'V' || c == 'x')
		   && p[1] == 0)
	    nskip += 1;
	  else if (! strcmp (p, "Xlinker"))
	    nskip += 1;

	  /* Watch out for an option at the end of the command line that
	     is missing arguments, and avoid skipping past the end of the
	     command line.  */
	  if (nskip + i > argc)
	    nskip = argc - i;

	  while (nskip > 0)
	    {
	      newv[newindex++] = argv[i++];
	      nskip--;
	    }
	}
      else
	/* Ordinary operands, or +e options.  */
	newv[newindex++] = argv[i++];
    }

  newv[newindex] = 0;

  *argvp = newv;
  *argcp = newindex;
}

char *
xstrerror(e)
     int e;
{
#ifdef HAVE_STRERROR

  return strerror(e);

#else

  if (!e)
    return "errno = 0";

  if (e > 0 && e < sys_nerr)
    return sys_errlist[e];

  return "errno = ?";
#endif
}

static char *
skip_whitespace (p)
     char *p;
{
  while (1)
    {
      /* A fully-blank line is a delimiter in the SPEC file and shouldn't
	 be considered whitespace.  */
      if (p[0] == '\n' && p[1] == '\n' && p[2] == '\n')
	return p + 1;
      else if (*p == '\n' || *p == ' ' || *p == '\t')
	p++;
      else if (*p == '#')
	{
	  while (*p != '\n') p++;
	  p++;
	}
      else
	break;
    }

  return p;
}

/* Structure to keep track of the specs that have been defined so far.
   These are accessed using %(specname) or %[specname] in a compiler
   or link spec.  */

struct spec_list
{
				/* The following 2 fields must be first */
				/* to allow EXTRA_SPECS to be initialized */
  char *name;			/* name of the spec.  */
  char *ptr;			/* available ptr if no static pointer */

				/* The following fields are not initialized */
				/* by EXTRA_SPECS */
  char **ptr_spec;		/* pointer to the spec itself.  */
  struct spec_list *next;	/* Next spec in linked list.  */
  int name_len;			/* length of the name */
  int alloc_p;			/* whether string was allocated */
};

#define INIT_STATIC_SPEC(NAME,PTR) \
{ NAME, NULL_PTR, PTR, (struct spec_list *)0, sizeof (NAME)-1, 0 }

/* List of statically defined specs */
static struct spec_list static_specs[] = {
  INIT_STATIC_SPEC ("asm",			&asm_spec),
  INIT_STATIC_SPEC ("asm_final",		&asm_final_spec),
  INIT_STATIC_SPEC ("cpp",			&cpp_spec),
  INIT_STATIC_SPEC ("cc1",			&cc1_spec),
  INIT_STATIC_SPEC ("cc1plus",			&cc1plus_spec),
  INIT_STATIC_SPEC ("endfile",			&endfile_spec),
  INIT_STATIC_SPEC ("link",			&link_spec),
  INIT_STATIC_SPEC ("lib",			&lib_spec),
  INIT_STATIC_SPEC ("libgcc",			&libgcc_spec),
  INIT_STATIC_SPEC ("startfile",		&startfile_spec),
  INIT_STATIC_SPEC ("switches_need_spaces",	&switches_need_spaces),
  INIT_STATIC_SPEC ("signed_char",		&signed_char_spec),
  INIT_STATIC_SPEC ("predefines",		&cpp_predefines),
  INIT_STATIC_SPEC ("cross_compile",		&cross_compile),
  INIT_STATIC_SPEC ("version",			&compiler_version),
  INIT_STATIC_SPEC ("multilib",			&multilib_select),
  INIT_STATIC_SPEC ("multilib_defaults",	&multilib_defaults),
  INIT_STATIC_SPEC ("multilib_extra",		&multilib_extra),
  INIT_STATIC_SPEC ("multilib_matches",		&multilib_matches),
  INIT_STATIC_SPEC ("linker",			&linker_name_spec),
};

#ifdef EXTRA_SPECS		/* additional specs needed */
/* Structure to keep track of just the first two args of a spec_list.
   That is all that the EXTRA_SPECS macro gives us. */
struct spec_list_1
{
  char *name;
  char *ptr;
};

static struct spec_list_1 extra_specs_1[] = { EXTRA_SPECS };
static struct spec_list * extra_specs = (struct spec_list *)0;
#endif

/* List of dynamically allocates specs that have been defined so far.  */

static struct spec_list *specs = (struct spec_list *)0;


/* Initialize the specs lookup routines.  */

static void
init_spec ()
{
  struct spec_list *next = (struct spec_list *)0;
  struct spec_list *sl   = (struct spec_list *)0;
  int i;

  if (specs)
    return;			/* already initialized */

  if (verbose_flag)
    notice ("Using builtin specs.\n");

#ifdef EXTRA_SPECS
  extra_specs = (struct spec_list *)
    xmalloc (sizeof(struct spec_list) *
	     (sizeof(extra_specs_1)/sizeof(extra_specs_1[0])));
  bzero ((PTR) extra_specs, sizeof(struct spec_list) *
	 (sizeof(extra_specs_1)/sizeof(extra_specs_1[0])));
  
  for (i = (sizeof(extra_specs_1) / sizeof(extra_specs_1[0])) - 1; i >= 0; i--)
    {
      sl = &extra_specs[i];
      sl->name = extra_specs_1[i].name;
      sl->ptr = extra_specs_1[i].ptr;
      sl->next = next;
      sl->name_len = strlen (sl->name);
      sl->ptr_spec = &sl->ptr;
      next = sl;
    }
#endif

  for (i = (sizeof (static_specs) / sizeof (static_specs[0])) - 1; i >= 0; i--)
    {
      sl = &static_specs[i];
      sl->next = next;
      next = sl;
    }

  specs = sl;
}


/* Change the value of spec NAME to SPEC.  If SPEC is empty, then the spec is
   removed; If the spec starts with a + then SPEC is added to the end of the
   current spec.  */

static void
set_spec (name, spec)
     const char *name;
     const char *spec;
{
  struct spec_list *sl;
  char *old_spec;
  int name_len = strlen (name);
  int i;

  /* If this is the first call, initialize the statically allocated specs */
  if (!specs)
    {
      struct spec_list *next = (struct spec_list *)0;
      for (i = (sizeof (static_specs) / sizeof (static_specs[0])) - 1;
	   i >= 0; i--)
	{
	  sl = &static_specs[i];
	  sl->next = next;
	  next = sl;
	}
      specs = sl;
    }

  /* See if the spec already exists */
  for (sl = specs; sl; sl = sl->next)
    if (name_len == sl->name_len && !strcmp (sl->name, name))
      break;

  if (!sl)
    {
      /* Not found - make it */
      sl = (struct spec_list *) xmalloc (sizeof (struct spec_list));
      sl->name = save_string (name, strlen (name));
      sl->name_len = name_len;
      sl->ptr_spec = &sl->ptr;
      sl->alloc_p = 0;
      *(sl->ptr_spec) = "";
      sl->next = specs;
      specs = sl;
    }

  old_spec = *(sl->ptr_spec);
  *(sl->ptr_spec) = ((spec[0] == '+' && ISSPACE ((unsigned char)spec[1]))
		     ? concat (old_spec, spec + 1, NULL_PTR)
		     : save_string (spec, strlen (spec)));

#ifdef DEBUG_SPECS
  if (verbose_flag)
    notice ("Setting spec %s to '%s'\n\n", name, *(sl->ptr_spec));
#endif

  /* Free the old spec */
  if (old_spec && sl->alloc_p)
    free (old_spec);

  sl->alloc_p = 1;
}

/* Accumulate a command (program name and args), and run it.  */

/* Vector of pointers to arguments in the current line of specifications.  */

static char **argbuf;

/* Number of elements allocated in argbuf.  */

static int argbuf_length;

/* Number of elements in argbuf currently in use (containing args).  */

static int argbuf_index;

/* We want this on by default all the time now.  */
#define MKTEMP_EACH_FILE

#ifdef MKTEMP_EACH_FILE

extern char *make_temp_file PROTO((const char *));

/* This is the list of suffixes and codes (%g/%u/%U) and the associated
   temp file.  */

static struct temp_name {
  const char *suffix;	/* suffix associated with the code.  */
  int length;		/* strlen (suffix).  */
  int unique;		/* Indicates whether %g or %u/%U was used.  */
  const char *filename;	/* associated filename.  */
  int filename_length;	/* strlen (filename).  */
  struct temp_name *next;
} *temp_names;
#endif


/* Number of commands executed so far.  */

static int execution_count;

/* Number of commands that exited with a signal.  */

static int signal_count;

/* Name with which this program was invoked.  */

static const char *programname;

/* Structures to keep track of prefixes to try when looking for files.  */

struct prefix_list
{
  char *prefix;               /* String to prepend to the path.  */
  struct prefix_list *next;   /* Next in linked list.  */
  int require_machine_suffix; /* Don't use without machine_suffix.  */
  /* 2 means try both machine_suffix and just_machine_suffix.  */
  int *used_flag_ptr;	      /* 1 if a file was found with this prefix.  */
};

struct path_prefix
{
  struct prefix_list *plist;  /* List of prefixes to try */
  int max_len;                /* Max length of a prefix in PLIST */
  const char *name;           /* Name of this list (used in config stuff) */
};

/* List of prefixes to try when looking for executables.  */

static struct path_prefix exec_prefixes = { 0, 0, "exec" };

/* List of prefixes to try when looking for startup (crt0) files.  */

static struct path_prefix startfile_prefixes = { 0, 0, "startfile" };

/* List of prefixes to try when looking for include files.  */

static struct path_prefix include_prefixes = { 0, 0, "include" };

/* Suffix to attach to directories searched for commands.
   This looks like `MACHINE/VERSION/'.  */

static const char *machine_suffix = 0;

/* Suffix to attach to directories searched for commands.
   This is just `MACHINE/'.  */

static const char *just_machine_suffix = 0;

/* Adjusted value of GCC_EXEC_PREFIX envvar.  */

static const char *gcc_exec_prefix;

/* Default prefixes to attach to command names.  */

#ifdef CROSS_COMPILE  /* Don't use these prefixes for a cross compiler.  */
#undef MD_EXEC_PREFIX
#undef MD_STARTFILE_PREFIX
#undef MD_STARTFILE_PREFIX_1
#endif

#ifndef STANDARD_EXEC_PREFIX
#define STANDARD_EXEC_PREFIX "/usr/local/lib/gcc-lib/"
#endif /* !defined STANDARD_EXEC_PREFIX */

static const char *standard_exec_prefix = STANDARD_EXEC_PREFIX;
static const char *standard_exec_prefix_1 = "/usr/lib/gcc/";
#ifdef MD_EXEC_PREFIX
static const char *md_exec_prefix = MD_EXEC_PREFIX;
#endif

#ifndef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX "/usr/local/lib/"
#endif /* !defined STANDARD_STARTFILE_PREFIX */

#ifdef MD_STARTFILE_PREFIX
static const char *md_startfile_prefix = MD_STARTFILE_PREFIX;
#endif
#ifdef MD_STARTFILE_PREFIX_1
static const char *md_startfile_prefix_1 = MD_STARTFILE_PREFIX_1;
#endif
static const char *standard_startfile_prefix = STANDARD_STARTFILE_PREFIX;
static const char *standard_startfile_prefix_1 = "/lib/";
static const char *standard_startfile_prefix_2 = "/usr/lib/";

#ifndef BINUTILS
#error BINUTILS not defined
#endif

#ifndef TOOLDIR_BASE_PREFIX
#define TOOLDIR_BASE_PREFIX "/usr/local/"
#endif
static const char *tooldir_base_prefix = TOOLDIR_BASE_PREFIX;
static const char *tooldir_prefix;

/* Subdirectory to use for locating libraries.  Set by
   set_multilib_dir based on the compilation options.  */

static const char *multilib_dir;

/* Clear out the vector of arguments (after a command is executed).  */

static void
clear_args ()
{
  argbuf_index = 0;
}

/* Add one argument to the vector at the end.
   This is done when a space is seen or at the end of the line.
   If DELETE_ALWAYS is nonzero, the arg is a filename
    and the file should be deleted eventually.
   If DELETE_FAILURE is nonzero, the arg is a filename
    and the file should be deleted if this compilation fails.  */

static void
store_arg (arg, delete_always, delete_failure)
     char *arg;
     int delete_always, delete_failure;
{
  if (argbuf_index + 1 == argbuf_length)
    argbuf
      = (char **) xrealloc (argbuf, (argbuf_length *= 2) * sizeof (char *));

  argbuf[argbuf_index++] = arg;
  argbuf[argbuf_index] = 0;

  if (delete_always || delete_failure)
    record_temp_file (arg, delete_always, delete_failure);
}

/* Read compilation specs from a file named FILENAME,
   replacing the default ones.

   A suffix which starts with `*' is a definition for
   one of the machine-specific sub-specs.  The "suffix" should be
   *asm, *cc1, *cpp, *link, *startfile, *signed_char, etc.
   The corresponding spec is stored in asm_spec, etc.,
   rather than in the `compilers' vector.

   Anything invalid in the file is a fatal error.  */

static void
read_specs (filename, main_p)
     const char *filename;
     int main_p;
{
  int desc;
  int readlen;
  struct stat statbuf;
  char *buffer;
  register char *p;

  if (verbose_flag)
    notice ("Reading specs from %s\n", filename);

  /* Open and stat the file.  */
  desc = open (filename, O_RDONLY, 0);
  if (desc < 0)
    pfatal_with_name (filename);
  if (stat (filename, &statbuf) < 0)
    pfatal_with_name (filename);

  /* Read contents of file into BUFFER.  */
  buffer = xmalloc ((unsigned) statbuf.st_size + 1);
  readlen = read (desc, buffer, (unsigned) statbuf.st_size);
  if (readlen < 0)
    pfatal_with_name (filename);
  buffer[readlen] = 0;
  close (desc);

  /* Scan BUFFER for specs, putting them in the vector.  */
  p = buffer;
  while (1)
    {
      char *suffix;
      char *spec;
      char *in, *out, *p1, *p2, *p3;

      /* Advance P in BUFFER to the next nonblank nocomment line.  */
      p = skip_whitespace (p);
      if (*p == 0)
	break;

      /* Is this a special command that starts with '%'? */
      /* Don't allow this for the main specs file, since it would
	 encourage people to overwrite it.  */
      if (*p == '%' && !main_p)
	{
	  p1 = p;
	  while (*p && *p != '\n')
	    p++;

	  p++;			/* Skip '\n' */

	  if (!strncmp (p1, "%include", sizeof ("%include")-1)
	      && (p1[sizeof "%include" - 1] == ' '
		  || p1[sizeof "%include" - 1] == '\t'))
	    {
	      char *new_filename;

	      p1 += sizeof ("%include");
	      while (*p1 == ' ' || *p1 == '\t')
		p1++;

	      if (*p1++ != '<' || p[-2] != '>')
		fatal ("specs %%include syntax malformed after %ld characters",
		       (long) (p1 - buffer + 1));

	      p[-2] = '\0';
	      new_filename = find_a_file (&startfile_prefixes, p1, R_OK);
	      read_specs (new_filename ? new_filename : p1, FALSE);
	      continue;
	    }
	  else if (!strncmp (p1, "%include_noerr", sizeof "%include_noerr" - 1)
		   && (p1[sizeof "%include_noerr" - 1] == ' '
		       || p1[sizeof "%include_noerr" - 1] == '\t'))
	    {
	      char *new_filename;

	      p1 += sizeof "%include_noerr";
	      while (*p1 == ' ' || *p1 == '\t') p1++;

	      if (*p1++ != '<' || p[-2] != '>')
		fatal ("specs %%include syntax malformed after %ld characters",
		       (long) (p1 - buffer + 1));

	      p[-2] = '\0';
	      new_filename = find_a_file (&startfile_prefixes, p1, R_OK);
	      if (new_filename)
		read_specs (new_filename, FALSE);
	      else if (verbose_flag)
		notice ("Could not find specs file %s\n", p1);
	      continue;
	    }
	  else if (!strncmp (p1, "%rename", sizeof "%rename" - 1)
		   && (p1[sizeof "%rename" - 1] == ' '
		       || p1[sizeof "%rename" - 1] == '\t'))
	    {
	      int name_len;
	      struct spec_list *sl;

	      /* Get original name */
	      p1 += sizeof "%rename";
	      while (*p1 == ' ' || *p1 == '\t')
		p1++;

	      if (! ISALPHA ((unsigned char)*p1))
		fatal ("specs %%rename syntax malformed after %ld characters",
		       (long) (p1 - buffer));

	      p2 = p1;
	      while (*p2 && !ISSPACE ((unsigned char)*p2))
		p2++;

	      if (*p2 != ' ' && *p2 != '\t')
		fatal ("specs %%rename syntax malformed after %ld characters",
		       (long) (p2 - buffer));

	      name_len = p2 - p1;
	      *p2++ = '\0';
	      while (*p2 == ' ' || *p2 == '\t')
		p2++;

	      if (! ISALPHA ((unsigned char)*p2))
		fatal ("specs %%rename syntax malformed after %ld characters",
		       (long) (p2 - buffer));

	      /* Get new spec name */
	      p3 = p2;
	      while (*p3 && !ISSPACE ((unsigned char)*p3))
		p3++;

	      if (p3 != p-1)
		fatal ("specs %%rename syntax malformed after %ld characters",
		       (long) (p3 - buffer));
	      *p3 = '\0';

	      for (sl = specs; sl; sl = sl->next)
		if (name_len == sl->name_len && !strcmp (sl->name, p1))
		  break;

	      if (!sl)
		fatal ("specs %s spec was not found to be renamed", p1);

	      if (strcmp (p1, p2) == 0)
		continue;

	      if (verbose_flag)
		{
		  notice ("rename spec %s to %s\n", p1, p2);
#ifdef DEBUG_SPECS
		  notice ("spec is '%s'\n\n", *(sl->ptr_spec));
#endif
		}

	      set_spec (p2, *(sl->ptr_spec));
	      if (sl->alloc_p)
		free (*(sl->ptr_spec));

	      *(sl->ptr_spec) = "";
	      sl->alloc_p = 0;
	      continue;
	    }
	  else
	    fatal ("specs unknown %% command after %ld characters",
		   (long) (p1 - buffer));
	}

      /* Find the colon that should end the suffix.  */
      p1 = p;
      while (*p1 && *p1 != ':' && *p1 != '\n')
	p1++;

      /* The colon shouldn't be missing.  */
      if (*p1 != ':')
	fatal ("specs file malformed after %ld characters",
	       (long) (p1 - buffer));

      /* Skip back over trailing whitespace.  */
      p2 = p1;
      while (p2 > buffer && (p2[-1] == ' ' || p2[-1] == '\t'))
	p2--;

      /* Copy the suffix to a string.  */
      suffix = save_string (p, p2 - p);
      /* Find the next line.  */
      p = skip_whitespace (p1 + 1);
      if (p[1] == 0)
	fatal ("specs file malformed after %ld characters",
	       (long) (p - buffer));

      p1 = p;
      /* Find next blank line or end of string.  */
      while (*p1 && !(*p1 == '\n' && (p1[1] == '\n' || p1[1] == '\0')))
	p1++;

      /* Specs end at the blank line and do not include the newline.  */
      spec = save_string (p, p1 - p);
      p = p1;

      /* Delete backslash-newline sequences from the spec.  */
      in = spec;
      out = spec;
      while (*in != 0)
	{
	  if (in[0] == '\\' && in[1] == '\n')
	    in += 2;
	  else if (in[0] == '#')
	    while (*in && *in != '\n')
	      in++;

	  else
	    *out++ = *in++;
	}
      *out = 0;

      if (suffix[0] == '*')
	{
	  if (! strcmp (suffix, "*link_command"))
	    link_command_spec = spec;
	  else
	    set_spec (suffix + 1, spec);
	}
      else
	{
	  /* Add this pair to the vector.  */
	  compilers
	    = ((struct compiler *)
	       xrealloc (compilers,
			 (n_compilers + 2) * sizeof (struct compiler)));

	  compilers[n_compilers].suffix = suffix;
	  bzero ((char *) compilers[n_compilers].spec,
		 sizeof compilers[n_compilers].spec);
	  compilers[n_compilers].spec[0] = spec;
	  n_compilers++;
	  bzero ((char *) &compilers[n_compilers],
		 sizeof compilers[n_compilers]);
	}

      if (*suffix == 0)
	link_command_spec = spec;
    }

  if (link_command_spec == 0)
    fatal ("spec file has no spec for linking");
}

/* Record the names of temporary files we tell compilers to write,
   and delete them at the end of the run.  */

/* This is the common prefix we use to make temp file names.
   It is chosen once for each run of this program.
   It is substituted into a spec by %g.
   Thus, all temp file names contain this prefix.
   In practice, all temp file names start with this prefix.

   This prefix comes from the envvar TMPDIR if it is defined;
   otherwise, from the P_tmpdir macro if that is defined;
   otherwise, in /usr/tmp or /tmp;
   or finally the current directory if all else fails.  */

static const char *temp_filename;

/* Length of the prefix.  */

static int temp_filename_length;

/* Define the list of temporary files to delete.  */

struct temp_file
{
  const char *name;
  struct temp_file *next;
};

/* Queue of files to delete on success or failure of compilation.  */
static struct temp_file *always_delete_queue;
/* Queue of files to delete on failure of compilation.  */
static struct temp_file *failure_delete_queue;

/* Record FILENAME as a file to be deleted automatically.
   ALWAYS_DELETE nonzero means delete it if all compilation succeeds;
   otherwise delete it in any case.
   FAIL_DELETE nonzero means delete it if a compilation step fails;
   otherwise delete it in any case.  */

static void
record_temp_file (filename, always_delete, fail_delete)
     const char *filename;
     int always_delete;
     int fail_delete;
{
  register char *name;
  name = xmalloc (strlen (filename) + 1);
  strcpy (name, filename);

  if (always_delete)
    {
      register struct temp_file *temp;
      for (temp = always_delete_queue; temp; temp = temp->next)
	if (! strcmp (name, temp->name))
	  goto already1;

      temp = (struct temp_file *) xmalloc (sizeof (struct temp_file));
      temp->next = always_delete_queue;
      temp->name = name;
      always_delete_queue = temp;

    already1:;
    }

  if (fail_delete)
    {
      register struct temp_file *temp;
      for (temp = failure_delete_queue; temp; temp = temp->next)
	if (! strcmp (name, temp->name))
	  goto already2;

      temp = (struct temp_file *) xmalloc (sizeof (struct temp_file));
      temp->next = failure_delete_queue;
      temp->name = name;
      failure_delete_queue = temp;

    already2:;
    }
}

/* Delete all the temporary files whose names we previously recorded.  */

static void
delete_if_ordinary (name)
     const char *name;
{
  struct stat st;
#ifdef DEBUG
  int i, c;

  printf ("Delete %s? (y or n) ", name);
  fflush (stdout);
  i = getchar ();
  if (i != '\n')
    while ((c = getchar ()) != '\n' && c != EOF)
      ;

  if (i == 'y' || i == 'Y')
#endif /* DEBUG */
    if (stat (name, &st) >= 0 && S_ISREG (st.st_mode))
      if (unlink (name) < 0)
	if (verbose_flag)
	  perror_with_name (name);
}

static void
delete_temp_files ()
{
  register struct temp_file *temp;

  for (temp = always_delete_queue; temp; temp = temp->next)
    delete_if_ordinary (temp->name);
  always_delete_queue = 0;
}

/* Delete all the files to be deleted on error.  */

static void
delete_failure_queue ()
{
  register struct temp_file *temp;

  for (temp = failure_delete_queue; temp; temp = temp->next)
    delete_if_ordinary (temp->name);
}

static void
clear_failure_queue ()
{
  failure_delete_queue = 0;
}

/* Routine to add variables to the environment.  We do this to pass
   the pathname of the gcc driver, and the directories search to the
   collect2 program, which is being run as ld.  This way, we can be
   sure of executing the right compiler when collect2 wants to build
   constructors and destructors.  Since the environment variables we
   use come from an obstack, we don't have to worry about allocating
   space for them.  */

#ifndef HAVE_PUTENV

void
putenv (str)
     char *str;
{
#ifndef VMS			/* nor about VMS */

  extern char **environ;
  char **old_environ = environ;
  char **envp;
  int num_envs = 0;
  int name_len = 1;
  int str_len = strlen (str);
  char *p = str;
  int ch;

  while ((ch = *p++) != '\0' && ch != '=')
    name_len++;

  if (!ch)
    abort ();

  /* Search for replacing an existing environment variable, and
     count the number of total environment variables.  */
  for (envp = old_environ; *envp; envp++)
    {
      num_envs++;
      if (!strncmp (str, *envp, name_len))
	{
	  *envp = str;
	  return;
	}
    }

  /* Add a new environment variable */
  environ = (char **) xmalloc (sizeof (char *) * (num_envs+2));
  *environ = str;
  memcpy ((char *) (environ + 1), (char *) old_environ,
	 sizeof (char *) * (num_envs+1));

#endif	/* VMS */
}

#endif	/* HAVE_PUTENV */


/* Build a list of search directories from PATHS.
   PREFIX is a string to prepend to the list.
   If CHECK_DIR_P is non-zero we ensure the directory exists.
   This is used mostly by putenv_from_prefixes so we use `collect_obstack'.
   It is also used by the --print-search-dirs flag.  */

static char *
build_search_list (paths, prefix, check_dir_p)
     struct path_prefix *paths;
     const char *prefix;
     int check_dir_p;
{
  int suffix_len = (machine_suffix) ? strlen (machine_suffix) : 0;
  int just_suffix_len
    = (just_machine_suffix) ? strlen (just_machine_suffix) : 0;
  int first_time = TRUE;
  struct prefix_list *pprefix;

  obstack_grow (&collect_obstack, prefix, strlen (prefix));

  for (pprefix = paths->plist; pprefix != 0; pprefix = pprefix->next)
    {
      int len = strlen (pprefix->prefix);

      if (machine_suffix
	  && (! check_dir_p
	      || is_directory (pprefix->prefix, machine_suffix, 0)))
	{
	  if (!first_time)
	    obstack_1grow (&collect_obstack, PATH_SEPARATOR);
	    
	  first_time = FALSE;
	  obstack_grow (&collect_obstack, pprefix->prefix, len);
	  obstack_grow (&collect_obstack, machine_suffix, suffix_len);
	}

      if (just_machine_suffix
	  && pprefix->require_machine_suffix == 2
	  && (! check_dir_p
	      || is_directory (pprefix->prefix, just_machine_suffix, 0)))
	{
	  if (! first_time)
	    obstack_1grow (&collect_obstack, PATH_SEPARATOR);
	    
	  first_time = FALSE;
	  obstack_grow (&collect_obstack, pprefix->prefix, len);
	  obstack_grow (&collect_obstack, just_machine_suffix,
			just_suffix_len);
	}

      if (! pprefix->require_machine_suffix)
	{
	  if (! first_time)
	    obstack_1grow (&collect_obstack, PATH_SEPARATOR);

	  first_time = FALSE;
	  obstack_grow (&collect_obstack, pprefix->prefix, len);
	}
    }

  obstack_1grow (&collect_obstack, '\0');
  return obstack_finish (&collect_obstack);
}

/* Rebuild the COMPILER_PATH and LIBRARY_PATH environment variables
   for collect.  */

static void
putenv_from_prefixes (paths, env_var)
     struct path_prefix *paths;
     const char *env_var;
{
  putenv (build_search_list (paths, env_var, 1));
}

/* Search for NAME using the prefix list PREFIXES.  MODE is passed to
   access to check permissions.
   Return 0 if not found, otherwise return its name, allocated with malloc.  */

static char *
find_a_file (pprefix, name, mode)
     struct path_prefix *pprefix;
     const char *name;
     int mode;
{
  char *temp;
  const char *file_suffix = ((mode & X_OK) != 0 ? EXECUTABLE_SUFFIX : "");
  struct prefix_list *pl;
  int len = pprefix->max_len + strlen (name) + strlen (file_suffix) + 1;

#ifdef DEFAULT_ASSEMBLER
  if (! strcmp(name, "as") && access (DEFAULT_ASSEMBLER, mode) == 0) {
    name = DEFAULT_ASSEMBLER;
    len = strlen(name)+1;
    temp = xmalloc (len);
    strcpy (temp, name);
    return temp;
  }
#endif

#ifdef DEFAULT_LINKER
  if (! strcmp(name, "ld") && access (DEFAULT_LINKER, mode) == 0) {
    name = DEFAULT_LINKER;
    len = strlen(name)+1;
    temp = xmalloc (len);
    strcpy (temp, name);
    return temp;
  }
#endif

  if (machine_suffix)
    len += strlen (machine_suffix);

  temp = xmalloc (len);

  /* Determine the filename to execute (special case for absolute paths).  */

  if (IS_DIR_SEPARATOR (*name)
#ifdef HAVE_DOS_BASED_FILESYSTEM
      /* Check for disk name on MS-DOS-based systems.  */
      || (name[0] && name[1] == ':' && IS_DIR_SEPARATOR (name[2]))
#endif
      )
    {
      if (access (name, mode) == 0)
	{
	  strcpy (temp, name);
	  return temp;
	}
    }
  else
    for (pl = pprefix->plist; pl; pl = pl->next)
      {
	if (machine_suffix)
	  {
	    /* Some systems have a suffix for executable files.
	       So try appending that first.  */
	    if (file_suffix[0] != 0)
	      {
		strcpy (temp, pl->prefix);
		strcat (temp, machine_suffix);
		strcat (temp, name);
		strcat (temp, file_suffix);
		if (access (temp, mode) == 0)
		  {
		    if (pl->used_flag_ptr != 0)
		      *pl->used_flag_ptr = 1;
		    return temp;
		  }
	      }

	    /* Now try just the name.  */
	    strcpy (temp, pl->prefix);
	    strcat (temp, machine_suffix);
	    strcat (temp, name);
	    if (access (temp, mode) == 0)
	      {
		if (pl->used_flag_ptr != 0)
		  *pl->used_flag_ptr = 1;
		return temp;
	      }
	  }

	/* Certain prefixes are tried with just the machine type,
	   not the version.  This is used for finding as, ld, etc.  */
	if (just_machine_suffix && pl->require_machine_suffix == 2)
	  {
	    /* Some systems have a suffix for executable files.
	       So try appending that first.  */
	    if (file_suffix[0] != 0)
	      {
		strcpy (temp, pl->prefix);
		strcat (temp, just_machine_suffix);
		strcat (temp, name);
		strcat (temp, file_suffix);
		if (access (temp, mode) == 0)
		  {
		    if (pl->used_flag_ptr != 0)
		      *pl->used_flag_ptr = 1;
		    return temp;
		  }
	      }

	    strcpy (temp, pl->prefix);
	    strcat (temp, just_machine_suffix);
	    strcat (temp, name);
	    if (access (temp, mode) == 0)
	      {
		if (pl->used_flag_ptr != 0)
		  *pl->used_flag_ptr = 1;
		return temp;
	      }
	  }

	/* Certain prefixes can't be used without the machine suffix
	   when the machine or version is explicitly specified.  */
	if (! pl->require_machine_suffix)
	  {
	    /* Some systems have a suffix for executable files.
	       So try appending that first.  */
	    if (file_suffix[0] != 0)
	      {
		strcpy (temp, pl->prefix);
		strcat (temp, name);
		strcat (temp, file_suffix);
		if (access (temp, mode) == 0)
		  {
		    if (pl->used_flag_ptr != 0)
		      *pl->used_flag_ptr = 1;
		    return temp;
		  }
	      }

	    strcpy (temp, pl->prefix);
	    strcat (temp, name);
	    if (access (temp, mode) == 0)
	      {
		if (pl->used_flag_ptr != 0)
		  *pl->used_flag_ptr = 1;
		return temp;
	      }
	  }
      }

  free (temp);
  return 0;
}

/* Add an entry for PREFIX in PLIST.  If FIRST is set, it goes
   at the start of the list, otherwise it goes at the end.

   If WARN is nonzero, we will warn if no file is found
   through this prefix.  WARN should point to an int
   which will be set to 1 if this entry is used.

   COMPONENT is the value to be passed to update_path.

   REQUIRE_MACHINE_SUFFIX is 1 if this prefix can't be used without
   the complete value of machine_suffix.
   2 means try both machine_suffix and just_machine_suffix.  */

static void
add_prefix (pprefix, prefix, component, first, require_machine_suffix, warn)
     struct path_prefix *pprefix;
     const char *prefix;
     const char *component;
     int first;
     int require_machine_suffix;
     int *warn;
{
  struct prefix_list *pl, **prev;
  int len;

  if (! first && pprefix->plist)
    {
      for (pl = pprefix->plist; pl->next; pl = pl->next)
	;
      prev = &pl->next;
    }
  else
    prev = &pprefix->plist;

  /* Keep track of the longest prefix */

  prefix = update_path (prefix, component);
  len = strlen (prefix);
  if (len > pprefix->max_len)
    pprefix->max_len = len;

  pl = (struct prefix_list *) xmalloc (sizeof (struct prefix_list));
  pl->prefix = save_string (prefix, len);
  pl->require_machine_suffix = require_machine_suffix;
  pl->used_flag_ptr = warn;
  if (warn)
    *warn = 0;

  if (*prev)
    pl->next = *prev;
  else
    pl->next = (struct prefix_list *) 0;
  *prev = pl;
}

/* Print warnings for any prefixes in the list PPREFIX that were not used.  */

static void
unused_prefix_warnings (pprefix)
     struct path_prefix *pprefix;
{
  struct prefix_list *pl = pprefix->plist;

  while (pl)
    {
      if (pl->used_flag_ptr != 0 && !*pl->used_flag_ptr)
	{
	  if (pl->require_machine_suffix && machine_suffix)
	    error ("file path prefix `%s%s' never used", pl->prefix,
		   machine_suffix);
	  else
	    error ("file path prefix `%s' never used", pl->prefix);

	  /* Prevent duplicate warnings.  */
	  *pl->used_flag_ptr = 1;
	}

      pl = pl->next;
    }
}


/* Execute the command specified by the arguments on the current line of spec.
   When using pipes, this includes several piped-together commands
   with `|' between them.

   Return 0 if successful, -1 if failed.  */

static int
execute ()
{
  int i;
  int n_commands;		/* # of command.  */
  char *string;
  struct command
    {
      const char *prog;		/* program name.  */
      char **argv;	/* vector of args.  */
      int pid;			/* pid of process for this command.  */
    };

  struct command *commands;	/* each command buffer with above info.  */

  /* Count # of piped commands.  */
  for (n_commands = 1, i = 0; i < argbuf_index; i++)
    if (strcmp (argbuf[i], "|") == 0)
      n_commands++;

  /* Get storage for each command.  */
  commands
    = (struct command *) alloca (n_commands * sizeof (struct command));

  /* Split argbuf into its separate piped processes,
     and record info about each one.
     Also search for the programs that are to be run.  */

  commands[0].prog = argbuf[0]; /* first command.  */
  commands[0].argv = &argbuf[0];
  string = find_a_file (&exec_prefixes, commands[0].prog, X_OK);

  if (string)
    commands[0].argv[0] = string;

  for (n_commands = 1, i = 0; i < argbuf_index; i++)
    if (strcmp (argbuf[i], "|") == 0)
      {				/* each command.  */
#if defined (__MSDOS__) || defined (OS2) || defined (VMS)
        fatal ("-pipe not supported");
#endif
	argbuf[i] = 0;	/* termination of command args.  */
	commands[n_commands].prog = argbuf[i + 1];
	commands[n_commands].argv = &argbuf[i + 1];
	string = find_a_file (&exec_prefixes, commands[n_commands].prog, X_OK);
	if (string)
	  commands[n_commands].argv[0] = string;
	n_commands++;
      }

  argbuf[argbuf_index] = 0;

  /* If -v, print what we are about to do, and maybe query.  */

  if (verbose_flag)
    {
      /* For help listings, put a blank line between sub-processes.  */
      if (print_help_list)
	fputc ('\n', stderr);
      
      /* Print each piped command as a separate line.  */
      for (i = 0; i < n_commands ; i++)
	{
	  char **j;

	  for (j = commands[i].argv; *j; j++)
	    fprintf (stderr, " %s", *j);

	  /* Print a pipe symbol after all but the last command.  */
	  if (i + 1 != n_commands)
	    fprintf (stderr, " |");
	  fprintf (stderr, "\n");
	}
      fflush (stderr);
#ifdef DEBUG
      notice ("\nGo ahead? (y or n) ");
      fflush (stderr);
      i = getchar ();
      if (i != '\n')
	while (getchar () != '\n')
	  ;

      if (i != 'y' && i != 'Y')
	return 0;
#endif /* DEBUG */
    }

  /* Run each piped subprocess.  */

  for (i = 0; i < n_commands; i++)
    {
      char *errmsg_fmt, *errmsg_arg;
      char *string = commands[i].argv[0];

      commands[i].pid = pexecute (string, commands[i].argv,
				  programname, temp_filename,
				  &errmsg_fmt, &errmsg_arg,
				  ((i == 0 ? PEXECUTE_FIRST : 0)
				   | (i + 1 == n_commands ? PEXECUTE_LAST : 0)
				   | (string == commands[i].prog
				      ? PEXECUTE_SEARCH : 0)
				   | (verbose_flag ? PEXECUTE_VERBOSE : 0)));

      if (commands[i].pid == -1)
	pfatal_pexecute (errmsg_fmt, errmsg_arg);

      if (string != commands[i].prog)
	free (string);
    }

  execution_count++;

  /* Wait for all the subprocesses to finish.
     We don't care what order they finish in;
     we know that N_COMMANDS waits will get them all.
     Ignore subprocesses that we don't know about,
     since they can be spawned by the process that exec'ed us.  */

  {
    int ret_code = 0;

    for (i = 0; i < n_commands; )
      {
	int j;
	int status;
	int pid;

	pid = pwait (commands[i].pid, &status, 0);
	if (pid < 0)
	  abort ();

	for (j = 0; j < n_commands; j++)
	  if (commands[j].pid == pid)
	    {
	      i++;
	      if (status != 0)
		{
		  if (WIFSIGNALED (status))
		    {
		      fatal ("Internal compiler error: program %s got fatal signal %d",
			     commands[j].prog, WTERMSIG (status));
		      signal_count++;
		      ret_code = -1;
		    }
		  else if (WIFEXITED (status)
			   && WEXITSTATUS (status) >= MIN_FATAL_STATUS)
		    ret_code = -1;
		}
	      break;
	    }
      }
    return ret_code;
  }
}

/* Find all the switches given to us
   and make a vector describing them.
   The elements of the vector are strings, one per switch given.
   If a switch uses following arguments, then the `part1' field
   is the switch itself and the `args' field
   is a null-terminated vector containing the following arguments.
   The `live_cond' field is 1 if the switch is true in a conditional spec,
   -1 if false (overridden by a later switch), and is initialized to zero.
   The `validated' field is nonzero if any spec has looked at this switch;
   if it remains zero at the end of the run, it must be meaningless.  */

struct switchstr
{
  const char *part1;
  char **args;
  int live_cond;
  int validated;
};

static struct switchstr *switches;

static int n_switches;

struct infile
{
  const char *name;
  const char *language;
};

/* Also a vector of input files specified.  */

static struct infile *infiles;

static int n_infiles;

/* This counts the number of libraries added by lang_specific_driver, so that
   we can tell if there were any user supplied any files or libraries.  */

static int added_libraries;

/* And a vector of corresponding output files is made up later.  */

static const char **outfiles;

/* Used to track if none of the -B paths are used.  */
static int warn_B;

/* Used to track if standard path isn't used and -b or -V is specified.  */
static int warn_std;

/* Gives value to pass as "warn" to add_prefix for standard prefixes.  */
static int *warn_std_ptr = 0;

#if defined(FREEBSD_NATIVE)
#include <objformat.h>

typedef enum { OBJFMT_UNKNOWN, OBJFMT_AOUT, OBJFMT_ELF } objf_t;

static objf_t objformat = OBJFMT_UNKNOWN;
#endif	/* FREEBSD_NATIVE */

#if defined(HAVE_OBJECT_SUFFIX) || defined(HAVE_EXECUTABLE_SUFFIX)

/* Convert NAME to a new name if it is the standard suffix.  DO_EXE
   is true if we should look for an executable suffix as well.  */

static char *
convert_filename (name, do_exe)
     char *name;
     int do_exe;
{
  int i;
  int len;

  if (name == NULL)
    return NULL;
  
  len = strlen (name);

#ifdef HAVE_OBJECT_SUFFIX
  /* Convert x.o to x.obj if OBJECT_SUFFIX is ".obj".  */
  if (len > 2
      && name[len - 2] == '.'
      && name[len - 1] == 'o')
    {
      obstack_grow (&obstack, name, len - 2);
      obstack_grow0 (&obstack, OBJECT_SUFFIX, strlen (OBJECT_SUFFIX));
      name = obstack_finish (&obstack);
    }
#endif

#ifdef HAVE_EXECUTABLE_SUFFIX
  /* If there is no filetype, make it the executable suffix (which includes
     the ".").  But don't get confused if we have just "-o".  */
  if (! do_exe || EXECUTABLE_SUFFIX[0] == 0 || (len == 2 && name[0] == '-'))
    return name;

  for (i = len - 1; i >= 0; i--)
    if (IS_DIR_SEPARATOR (name[i]))
      break;

  for (i++; i < len; i++)
    if (name[i] == '.')
      return name;

  obstack_grow (&obstack, name, len);
  obstack_grow0 (&obstack, EXECUTABLE_SUFFIX, strlen (EXECUTABLE_SUFFIX));
  name = obstack_finish (&obstack);
#endif

  return name;
}
#endif

/* Display the command line switches accepted by gcc.  */
static void
display_help ()
{
  printf ("Usage: %s [options] file...\n", programname);
  printf ("Options:\n");

  printf ("  --help                   Display this information\n");
  if (! verbose_flag)
    printf ("  (Use '-v --help' to display command line options of sub-processes)\n");
  printf ("  -dumpspecs               Display all of the built in spec strings\n");
  printf ("  -dumpversion             Display the version of the compiler\n");
  printf ("  -dumpmachine             Display the compiler's target processor\n");
  printf ("  -print-search-dirs       Display the directories in the compiler's search path\n");
  printf ("  -print-libgcc-file-name  Display the name of the compiler's companion library\n");
  printf ("  -print-file-name=<lib>   Display the full path to library <lib>\n");
  printf ("  -print-prog-name=<prog>  Display the full path to compiler component <prog>\n");
  printf ("  -print-multi-directory   Display the root directory for versions of libgcc\n");
  printf ("  -print-multi-lib         Display the mapping between command line options and\n");
  printf ("                            multiple library search directories\n");
  printf ("  -Wa,<options>            Pass comma-separated <options> on to the assembler\n");
  printf ("  -Wp,<options>            Pass comma-separated <options> on to the preprocessor\n");
  printf ("  -Wl,<options>            Pass comma-separated <options> on to the linker\n");
  printf ("  -Xlinker <arg>           Pass <arg> on to the linker\n");
  printf ("  -save-temps              Do not delete intermediate files\n");
  printf ("  -pipe                    Use pipes rather than intermediate files\n");
  printf ("  -specs=<file>            Override builtin specs with the contents of <file>\n");
  printf ("  -std=<standard>          Assume that the input sources are for <standard>\n");
  printf ("  -B <directory>           Add <directory> to the compiler's search paths\n");
  printf ("  -b <machine>             Run gcc for target <machine>, if installed\n");
  printf ("  -V <version>             Run gcc version number <version>, if installed\n");
  printf ("  -v                       Display the programs invoked by the compiler\n");
  printf ("  -E                       Preprocess only; do not compile, assemble or link\n");
  printf ("  -S                       Compile only; do not assemble or link\n");
  printf ("  -c                       Compile and assemble, but do not link\n");
  printf ("  -o <file>                Place the output into <file>\n");
  printf ("  -x <language>            Specify the language of the following input files\n");
  printf ("                            Permissable languages include: c c++ assembler none\n");
  printf ("                            'none' means revert to the default behaviour of\n");
  printf ("                            guessing the language based on the file's extension\n");

  printf ("\nOptions starting with -g, -f, -m, -O or -W are automatically passed on to\n");
  printf ("the various sub-processes invoked by %s.  In order to pass other options\n",
	  programname);
  printf ("on to these processes the -W<letter> options must be used.\n");

  /* The rest of the options are displayed by invocations of the various
     sub-processes.  */
}

static void 								
add_preprocessor_option (option, len)					
     const char * option;
     int len;
{									
  n_preprocessor_options++;
									
  if (! preprocessor_options)
    preprocessor_options
      = (char **) xmalloc (n_preprocessor_options * sizeof (char *));
  else
    preprocessor_options
      = (char **) xrealloc (preprocessor_options,
			    n_preprocessor_options * sizeof (char *));
  									
  preprocessor_options [n_preprocessor_options - 1] =
    save_string (option, len);
}
     
static void 								
add_assembler_option (option, len)					
     const char * option;
     int len;
{
  n_assembler_options++;

  if (! assembler_options)
    assembler_options
      = (char **) xmalloc (n_assembler_options * sizeof (char *));
  else
    assembler_options
      = (char **) xrealloc (assembler_options,
			    n_assembler_options * sizeof (char *));

  assembler_options [n_assembler_options - 1] = save_string (option, len);
}
     
static void 								
add_linker_option (option, len)					
     const char * option;
     int    len;
{
  n_linker_options++;

  if (! linker_options)
    linker_options
      = (char **) xmalloc (n_linker_options * sizeof (char *));
  else
    linker_options
      = (char **) xrealloc (linker_options,
			    n_linker_options * sizeof (char *));

  linker_options [n_linker_options - 1] = save_string (option, len);
}

/* Create the vector `switches' and its contents.
   Store its length in `n_switches'.  */

static void
process_command (argc, argv)
     int argc;
     char **argv;
{
  register int i;
  const char *temp;
  char *temp1;
  char *spec_lang = 0;
  int last_language_n_infiles;
  int have_c = 0;
  int have_o = 0;
  int lang_n_infiles = 0;

  GET_ENV_PATH_LIST (gcc_exec_prefix, "GCC_EXEC_PREFIX");

  n_switches = 0;
  n_infiles = 0;
  added_libraries = 0;

  /* Figure compiler version from version string.  */

  compiler_version = temp1 =
    save_string (version_string, strlen (version_string));
  for (; *temp1; ++temp1)
    {
      if (*temp1 == ' ')
	{
	  *temp1 = '\0';
	  break;
	}
    }

  /* Set up the default search paths.  */

  if (gcc_exec_prefix)
    {
      int len = strlen (gcc_exec_prefix);
      if (len > (int) sizeof ("/lib/gcc-lib/")-1
	  && (IS_DIR_SEPARATOR (gcc_exec_prefix[len-1])))
	{
	  temp = gcc_exec_prefix + len - sizeof ("/lib/gcc-lib/") + 1;
	  if (IS_DIR_SEPARATOR (*temp)
	      && strncmp (temp+1, "lib", 3) == 0
	      && IS_DIR_SEPARATOR (temp[4])
	      && strncmp (temp+5, "gcc-lib", 7) == 0)
	    len -= sizeof ("/lib/gcc-lib/") - 1;
	}

      set_std_prefix (gcc_exec_prefix, len);
      add_prefix (&exec_prefixes, gcc_exec_prefix, "GCC", 0, 0, NULL_PTR);
      add_prefix (&startfile_prefixes, gcc_exec_prefix, "GCC", 0, 0, NULL_PTR);
    }

  /* COMPILER_PATH and LIBRARY_PATH have values
     that are lists of directory names with colons.  */

  GET_ENV_PATH_LIST (temp, "COMPILER_PATH");
  if (temp)
    {
      const char *startp, *endp;
      char *nstore = (char *) alloca (strlen (temp) + 3);

      startp = endp = temp;
      while (1)
	{
	  if (*endp == PATH_SEPARATOR || *endp == 0)
	    {
	      strncpy (nstore, startp, endp-startp);
	      if (endp == startp)
		strcpy (nstore, concat (".", dir_separator_str, NULL_PTR));
	      else if (!IS_DIR_SEPARATOR (endp[-1]))
		{
		  nstore[endp-startp] = DIR_SEPARATOR;
		  nstore[endp-startp+1] = 0;
		}
	      else
		nstore[endp-startp] = 0;
	      add_prefix (&exec_prefixes, nstore, 0, 0, 0, NULL_PTR);
	      add_prefix (&include_prefixes,
			  concat (nstore, "include", NULL_PTR),
			  0, 0, 0, NULL_PTR);
	      if (*endp == 0)
		break;
	      endp = startp = endp + 1;
	    }
	  else
	    endp++;
	}
    }

  GET_ENV_PATH_LIST (temp, "LIBRARY_PATH");
  if (temp && *cross_compile == '0')
    {
      const char *startp, *endp;
      char *nstore = (char *) alloca (strlen (temp) + 3);

      startp = endp = temp;
      while (1)
	{
	  if (*endp == PATH_SEPARATOR || *endp == 0)
	    {
	      strncpy (nstore, startp, endp-startp);
	      if (endp == startp)
		strcpy (nstore, concat (".", dir_separator_str, NULL_PTR));
	      else if (!IS_DIR_SEPARATOR (endp[-1]))
		{
		  nstore[endp-startp] = DIR_SEPARATOR;
		  nstore[endp-startp+1] = 0;
		}
	      else
		nstore[endp-startp] = 0;
	      add_prefix (&startfile_prefixes, nstore, NULL_PTR,
			  0, 0, NULL_PTR);
	      if (*endp == 0)
		break;
	      endp = startp = endp + 1;
	    }
	  else
	    endp++;
	}
    }

  /* Use LPATH like LIBRARY_PATH (for the CMU build program).  */
  GET_ENV_PATH_LIST (temp, "LPATH");
  if (temp && *cross_compile == '0')
    {
      const char *startp, *endp;
      char *nstore = (char *) alloca (strlen (temp) + 3);

      startp = endp = temp;
      while (1)
	{
	  if (*endp == PATH_SEPARATOR || *endp == 0)
	    {
	      strncpy (nstore, startp, endp-startp);
	      if (endp == startp)
		strcpy (nstore, concat (".", dir_separator_str, NULL_PTR));
	      else if (!IS_DIR_SEPARATOR (endp[-1]))
		{
		  nstore[endp-startp] = DIR_SEPARATOR;
		  nstore[endp-startp+1] = 0;
		}
	      else
		nstore[endp-startp] = 0;
	      add_prefix (&startfile_prefixes, nstore, NULL_PTR,
			  0, 0, NULL_PTR);
	      if (*endp == 0)
		break;
	      endp = startp = endp + 1;
	    }
	  else
	    endp++;
	}
    }

#if defined(FREEBSD_NATIVE)
  {
    char buf[64];
    if (getobjformat (buf, sizeof buf, &argc, argv))
      if (strcmp (buf, "aout") == 0)
        objformat = OBJFMT_AOUT;
      else if (strcmp (buf, "elf") == 0)
        objformat = OBJFMT_ELF;
      else
        fprintf(stderr, "Unrecognized object format: %s\n", buf);
  }
#endif	/* FREEBSD_NATIVE */

  /* Options specified as if they appeared on the command line.  */
  temp = getenv ("GCC_OPTIONS");
  if ((temp) && (strlen (temp) > 0))
    {
      int len;
      int optc = 1;
      int new_argc;
      char **new_argv;
      char *envopts;

      while (isspace (*temp))
	temp++;
      len = strlen (temp);
      envopts = (char *) xmalloc (len + 1);
      strcpy (envopts, temp);

      for (i = 0; i < (len - 1); i++)
	if ((isspace (envopts[i])) && ! (isspace (envopts[i+1])))
	  optc++;

      new_argv = (char **) alloca ((optc + argc) * sizeof(char *));

      for (i = 0, new_argc = 1; new_argc <= optc; new_argc++)
	{
	  while (isspace (envopts[i]))
	    i++;
	  new_argv[new_argc] = envopts + i;
	  while (!isspace (envopts[i]) && (envopts[i] != '\0'))
	    i++;
	  envopts[i++] = '\0';
	}
      for (i = 1; i < argc; i++)
	new_argv[new_argc++] = argv[i];

      argv = new_argv;
      argc = new_argc;
    }

  /* Convert new-style -- options to old-style.  */
  translate_options (&argc, &argv);

  /* Do language-specific adjustment/addition of flags.  */
  lang_specific_driver (fatal, &argc, &argv, &added_libraries);

  /* Scan argv twice.  Here, the first time, just count how many switches
     there will be in their vector, and how many input files in theirs.
     Here we also parse the switches that cc itself uses (e.g. -v).  */

  for (i = 1; i < argc; i++)
    {
      if (! strcmp (argv[i], "-dumpspecs"))
	{
	  struct spec_list *sl;
	  init_spec ();
	  for (sl = specs; sl; sl = sl->next)
	    printf ("*%s:\n%s\n\n", sl->name, *(sl->ptr_spec));
          if (link_command_spec)
            printf ("*link_command:\n%s\n\n", link_command_spec);
	  exit (0);
	}
      else if (! strcmp (argv[i], "-dumpversion"))
	{
	  printf ("%s\n", spec_version);
	  exit (0);
	}
      else if (! strcmp (argv[i], "-dumpmachine"))
	{
	  printf ("%s\n", spec_machine);
	  exit  (0);
	}
      else if (strcmp (argv[i], "-fhelp") == 0)
	{
	  /* translate_options () has turned --help into -fhelp.  */
	  print_help_list = 1;

	  /* We will be passing a dummy file on to the sub-processes.  */
	  n_infiles++;
	  n_switches++;
	  
	  add_preprocessor_option ("--help", 6);
	  add_assembler_option ("--help", 6);
	  add_linker_option ("--help", 6);
	}
      else if (! strcmp (argv[i], "-print-search-dirs"))
	print_search_dirs = 1;
      else if (! strcmp (argv[i], "-print-libgcc-file-name"))
	print_file_name = "libgcc.a";
      else if (! strncmp (argv[i], "-print-file-name=", 17))
	print_file_name = argv[i] + 17;
      else if (! strncmp (argv[i], "-print-prog-name=", 17))
	print_prog_name = argv[i] + 17;
      else if (! strcmp (argv[i], "-print-multi-lib"))
	print_multi_lib = 1;
      else if (! strcmp (argv[i], "-print-multi-directory"))
	print_multi_directory = 1;
      else if (! strncmp (argv[i], "-Wa,", 4))
	{
	  int prev, j;
	  /* Pass the rest of this option to the assembler.  */

	  /* Split the argument at commas.  */
	  prev = 4;
	  for (j = 4; argv[i][j]; j++)
	    if (argv[i][j] == ',')
	      {
		add_assembler_option (argv[i] + prev, j - prev);
		prev = j + 1;
	      }
	  
	  /* Record the part after the last comma.  */
	  add_assembler_option (argv[i] + prev, j - prev);
	}
      else if (! strncmp (argv[i], "-Wp,", 4))
	{
	  int prev, j;
	  /* Pass the rest of this option to the preprocessor.  */

	  /* Split the argument at commas.  */
	  prev = 4;
	  for (j = 4; argv[i][j]; j++)
	    if (argv[i][j] == ',')
	      {
		add_preprocessor_option (argv[i] + prev, j - prev);
		prev = j + 1;
	      }
	  
	  /* Record the part after the last comma.  */
	  add_preprocessor_option (argv[i] + prev, j - prev);
	}
      else if (argv[i][0] == '+' && argv[i][1] == 'e')
	/* The +e options to the C++ front-end.  */
	n_switches++;
      else if (strncmp (argv[i], "-Wl,", 4) == 0)
	{
	  int j;
	  /* Split the argument at commas.  */
	  for (j = 3; argv[i][j]; j++)
	    n_infiles += (argv[i][j] == ',');
	}
      else if (strcmp (argv[i], "-Xlinker") == 0)
	{
	  if (i + 1 == argc)
	    fatal ("argument to `-Xlinker' is missing");

	  n_infiles++;
	  i++;
	}
      else if (strcmp (argv[i], "-l") == 0)
	{
	  if (i + 1 == argc)
	    fatal ("argument to `-l' is missing");

	  n_infiles++;
	  i++;
	}
      else if (strncmp (argv[i], "-l", 2) == 0)
	n_infiles++;
      else if (strcmp (argv[i], "-save-temps") == 0)
	{
	  save_temps_flag = 1;
	  n_switches++;
	}
      else if (strcmp (argv[i], "-specs") == 0)
	{
	  struct user_specs *user = (struct user_specs *)
	    xmalloc (sizeof (struct user_specs));
	  if (++i >= argc)
	    fatal ("argument to `-specs' is missing");

	  user->next = (struct user_specs *)0;
	  user->filename = argv[i];
	  if (user_specs_tail)
	    user_specs_tail->next = user;
	  else
	    user_specs_head = user;
	  user_specs_tail = user;
	}
      else if (strncmp (argv[i], "-specs=", 7) == 0)
	{
	  struct user_specs *user = (struct user_specs *)
	    xmalloc (sizeof (struct user_specs));
	  if (strlen (argv[i]) == 7)
	    fatal ("argument to `-specs=' is missing");

	  user->next = (struct user_specs *)0;
	  user->filename = argv[i]+7;
	  if (user_specs_tail)
	    user_specs_tail->next = user;
	  else
	    user_specs_head = user;
	  user_specs_tail = user;
	}
      else if (argv[i][0] == '-' && argv[i][1] != 0)
	{
	  register char *p = &argv[i][1];
	  register int c = *p;

	  switch (c)
	    {
	    case 'b':
              n_switches++;
	      if (p[1] == 0 && i + 1 == argc)
		fatal ("argument to `-b' is missing");
	      if (p[1] == 0)
		spec_machine = argv[++i];
	      else
		spec_machine = p + 1;

	      warn_std_ptr = &warn_std;
	      break;

	    case 'B':
	      {
		char *value;
		if (p[1] == 0 && i + 1 == argc)
		  fatal ("argument to `-B' is missing");
		if (p[1] == 0)
		  value = argv[++i];
		else
		  value = p + 1;
		add_prefix (&exec_prefixes, value, NULL_PTR, 1, 0, &warn_B);
		add_prefix (&startfile_prefixes, value, NULL_PTR,
			    1, 0, &warn_B);
		add_prefix (&include_prefixes, concat (value, "include",
						       NULL_PTR),
			    NULL_PTR, 1, 0, NULL_PTR);

		/* As a kludge, if the arg is "[foo/]stageN/", just add
		   "[foo/]include" to the include prefix.  */
		{
		  int len = strlen (value);
		  if ((len == 7
		       || (len > 7
			   && (IS_DIR_SEPARATOR (value[len - 8]))))
		      && strncmp (value + len - 7, "stage", 5) == 0
		      && ISDIGIT (value[len - 2])
		      && (IS_DIR_SEPARATOR (value[len - 1])))
		    {
		      if (len == 7)
			add_prefix (&include_prefixes, "include", NULL_PTR,
				    1, 0, NULL_PTR);
		      else
			{
			  char *string = xmalloc (len + 1);
			  strncpy (string, value, len-7);
			  strcpy (string+len-7, "include");
			  add_prefix (&include_prefixes, string, NULL_PTR,
				      1, 0, NULL_PTR);
			}
		    }
		}
                n_switches++;
	      }
	      break;

	    case 'v':	/* Print our subcommands and print versions.  */
	      n_switches++;
	      /* If they do anything other than exactly `-v', don't set
		 verbose_flag; rather, continue on to give the error.  */
	      if (p[1] != 0)
		break;
	      verbose_flag++;
	      break;

	    case 'V':
	      n_switches++;
	      if (p[1] == 0 && i + 1 == argc)
		fatal ("argument to `-V' is missing");
	      if (p[1] == 0)
		spec_version = argv[++i];
	      else
		spec_version = p + 1;
	      compiler_version = spec_version;
	      warn_std_ptr = &warn_std;

	      /* Validate the version number.  Use the same checks
		 done when inserting it into a spec.

		 The format of the version string is
		 ([^0-9]*-)?[0-9]+[.][0-9]+([.][0-9]+)?([- ].*)?  */
	      {
		const char *v = compiler_version;

		/* Ignore leading non-digits.  i.e. "foo-" in "foo-2.7.2".  */
		while (! ISDIGIT (*v))
		  v++;

		if (v > compiler_version && v[-1] != '-')
		  fatal ("invalid version number format");

		/* Set V after the first period.  */
		while (ISDIGIT (*v))
		  v++;

		if (*v != '.')
		  fatal ("invalid version number format");

		v++;
		while (ISDIGIT (*v))
		  v++;

		if (*v != 0 && *v != ' ' && *v != '.' && *v != '-')
		  fatal ("invalid version number format");
	      }
	      break;

	    case 'S':
	    case 'c':
	      if (p[1] == 0)
		{
		  have_c = 1;
		  n_switches++;
		  break;
		}
	      goto normal_switch;

	    case 'o':
	      have_o = 1;
#if defined(HAVE_EXECUTABLE_SUFFIX)
	      if (! have_c)
		{
		  int skip;
		  
		  /* Forward scan, just in case -S or -c is specified
		     after -o.  */
		  int j = i + 1;
		  if (p[1] == 0)
		    ++j;
		  while (j < argc)
		    {
		      if (argv[j][0] == '-')
			{
			  if (SWITCH_CURTAILS_COMPILATION (argv[j][1])
			      && argv[j][2] == 0)
			    {
			      have_c = 1;
			      break;
			    }
			  else if (skip = SWITCH_TAKES_ARG (argv[j][1]))
			    j += skip - (argv[j][2] != 0);
			  else if (skip = WORD_SWITCH_TAKES_ARG (argv[j] + 1))
			    j += skip;
			}
		      j++;
		    }
		}
#endif
#if defined(HAVE_EXECUTABLE_SUFFIX) || defined(HAVE_OBJECT_SUFFIX)
	      if (p[1] == 0)
		argv[i+1] = convert_filename (argv[i+1], ! have_c);
	      else
		argv[i] = convert_filename (argv[i], ! have_c);
#endif
	      goto normal_switch;

	    default:
	    normal_switch:
	      n_switches++;

	      if (SWITCH_TAKES_ARG (c) > (p[1] != 0))
		i += SWITCH_TAKES_ARG (c) - (p[1] != 0);
	      else if (WORD_SWITCH_TAKES_ARG (p))
		i += WORD_SWITCH_TAKES_ARG (p);
	    }
	}
      else
	{
	  n_infiles++;
	  lang_n_infiles++;
	}
    }

  if (have_c && have_o && lang_n_infiles > 1)
    fatal ("cannot specify -o with -c or -S and multiple compilations");

  /* Set up the search paths before we go looking for config files.  */

  /* These come before the md prefixes so that we will find gcc's subcommands
     (such as cpp) rather than those of the host system.  */
  /* Use 2 as fourth arg meaning try just the machine as a suffix,
     as well as trying the machine and the version.  */
#ifdef FREEBSD_NATIVE
  add_prefix (&exec_prefixes, PREFIX"/libexec/" BINUTILS "/", "BINUTILS",
	      0, 0, NULL_PTR);
  switch (objformat)
    {
    case OBJFMT_AOUT:
      n_switches++;		/* add implied -maout */
      add_prefix (&exec_prefixes, PREFIX"/libexec/" BINUTILS "/aout/", "BINUTILS",
		  0, 0, NULL_PTR);
      break;
    case OBJFMT_ELF:
      add_prefix (&exec_prefixes, PREFIX"/libexec/" BINUTILS "/elf/", "BINUTILS",
		  0, 0, NULL_PTR);
      break;
    case OBJFMT_UNKNOWN:
      fatal ("object format unknown");
    }
#endif	/* FREEBSD_NATIVE */

#ifndef OS2
  add_prefix (&exec_prefixes, standard_exec_prefix, "BINUTILS",
	      0, 2, warn_std_ptr);
#ifndef FREEBSD_NATIVE
  add_prefix (&exec_prefixes, standard_exec_prefix_1, "BINUTILS",
	      0, 2, warn_std_ptr);
#endif	/* not FREEBSD_NATIVE */
#endif

#ifndef FREEBSD_NATIVE
  add_prefix (&startfile_prefixes, standard_exec_prefix, "BINUTILS",
	      0, 1, warn_std_ptr);
  add_prefix (&startfile_prefixes, standard_exec_prefix_1, "BINUTILS",
	      0, 1, warn_std_ptr);
#endif	/* not FREEBSD_NATIVE */

  tooldir_prefix = concat (tooldir_base_prefix, spec_machine, 
			   dir_separator_str, NULL_PTR);

  /* If tooldir is relative, base it on exec_prefixes.  A relative
     tooldir lets us move the installed tree as a unit.

     If GCC_EXEC_PREFIX is defined, then we want to add two relative
     directories, so that we can search both the user specified directory
     and the standard place.  */

  if (!IS_DIR_SEPARATOR (*tooldir_prefix))
    {
      if (gcc_exec_prefix)
	{
	  char *gcc_exec_tooldir_prefix
	    = concat (gcc_exec_prefix, spec_machine, dir_separator_str,
		      spec_version, dir_separator_str, tooldir_prefix, NULL_PTR);

	  add_prefix (&exec_prefixes,
		      concat (gcc_exec_tooldir_prefix, "bin", 
			      dir_separator_str, NULL_PTR),
		      NULL_PTR, 0, 0, NULL_PTR);
	  add_prefix (&startfile_prefixes,
		      concat (gcc_exec_tooldir_prefix, "lib", 
			      dir_separator_str, NULL_PTR),
		      NULL_PTR, 0, 0, NULL_PTR);
	}

      tooldir_prefix = concat (standard_exec_prefix, spec_machine,
			       dir_separator_str, spec_version, 
			       dir_separator_str, tooldir_prefix, NULL_PTR);
    }

#ifndef FREEBSD_NATIVE
  add_prefix (&exec_prefixes, 
              concat (tooldir_prefix, "bin", dir_separator_str, NULL_PTR),
	      "BINUTILS", 0, 0, NULL_PTR);
  add_prefix (&startfile_prefixes,
	      concat (tooldir_prefix, "lib", dir_separator_str, NULL_PTR),
	      "BINUTILS", 0, 0, NULL_PTR);
#endif	/* not FREEBSD_NATIVE */

  /* More prefixes are enabled in main, after we read the specs file
     and determine whether this is cross-compilation or not.  */


  /* Then create the space for the vectors and scan again.  */

  switches = ((struct switchstr *)
	      xmalloc ((n_switches + 1) * sizeof (struct switchstr)));
  infiles = (struct infile *) xmalloc ((n_infiles + 1) * sizeof (struct infile));
  n_switches = 0;
  n_infiles = 0;
  last_language_n_infiles = -1;

  /* This, time, copy the text of each switch and store a pointer
     to the copy in the vector of switches.
     Store all the infiles in their vector.  */

#if defined(FREEBSD_NATIVE)
  switch (objformat)
    {
    case OBJFMT_AOUT:
      switches[n_switches].part1 = "maout";
      switches[n_switches].args = 0;
      switches[n_switches].live_cond = 0;
      switches[n_switches].validated = 0;
      n_switches++;
      putenv("OBJFORMAT=aout");
      break;
    case OBJFMT_ELF:
      putenv("OBJFORMAT=elf");
      break;
    case OBJFMT_UNKNOWN:
      fatal ("object format unknown");
    }
#endif	/* FREEBSD_NATIVE */

  for (i = 1; i < argc; i++)
    {
      /* Just skip the switches that were handled by the preceding loop.  */
      if (! strncmp (argv[i], "-Wa,", 4))
	;
      else if (! strncmp (argv[i], "-Wp,", 4))
	;
      else if (! strcmp (argv[i], "-print-search-dirs"))
	;
      else if (! strcmp (argv[i], "-print-libgcc-file-name"))
	;
      else if (! strncmp (argv[i], "-print-file-name=", 17))
	;
      else if (! strncmp (argv[i], "-print-prog-name=", 17))
	;
      else if (! strcmp (argv[i], "-print-multi-lib"))
	;
      else if (! strcmp (argv[i], "-print-multi-directory"))
	;
      else if (strcmp (argv[i], "-fhelp") == 0)
	{
	  if (verbose_flag)
	    {
	      /* Create a dummy input file, so that we can pass --help on to
		 the various sub-processes.  */
	      infiles[n_infiles].language = "c";
	      infiles[n_infiles++].name   = "help-dummy";
	      
	      /* Preserve the --help switch so that it can be caught by the
		 cc1 spec string.  */
	      switches[n_switches].part1     = "--help";
	      switches[n_switches].args      = 0;
	      switches[n_switches].live_cond = 0;
	      switches[n_switches].validated     = 0;
	      
	      n_switches++;
	    }
	}
      else if (argv[i][0] == '+' && argv[i][1] == 'e')
	{
	  /* Compensate for the +e options to the C++ front-end;
	     they're there simply for cfront call-compatibility.  We do
	     some magic in default_compilers to pass them down properly.
	     Note we deliberately start at the `+' here, to avoid passing
	     -e0 or -e1 down into the linker.  */
	  switches[n_switches].part1 = &argv[i][0];
	  switches[n_switches].args = 0;
	  switches[n_switches].live_cond = 0;
	  switches[n_switches].validated = 0;
	  n_switches++;
	}
      else if (strncmp (argv[i], "-Wl,", 4) == 0)
	{
	  int prev, j;
	  /* Split the argument at commas.  */
	  prev = 4;
	  for (j = 4; argv[i][j]; j++)
	    if (argv[i][j] == ',')
	      {
		infiles[n_infiles].language = "*";
		infiles[n_infiles++].name
		  = save_string (argv[i] + prev, j - prev);
		prev = j + 1;
	      }
	  /* Record the part after the last comma.  */
	  infiles[n_infiles].language = "*";
	  infiles[n_infiles++].name = argv[i] + prev;
	}
      else if (strcmp (argv[i], "-Xlinker") == 0)
	{
	  infiles[n_infiles].language = "*";
	  infiles[n_infiles++].name = argv[++i];
	}
      else if (strcmp (argv[i], "-l") == 0)
	{ /* POSIX allows separation of -l and the lib arg;
	     canonicalize by concatenating -l with its arg */
	  infiles[n_infiles].language = "*";
	  infiles[n_infiles++].name = concat ("-l", argv[++i], NULL);
	}
      else if (strncmp (argv[i], "-l", 2) == 0)
	{
	  infiles[n_infiles].language = "*";
	  infiles[n_infiles++].name = argv[i];
	}
      else if (strcmp (argv[i], "-specs") == 0)
	i++;
      else if (strncmp (argv[i], "-specs=", 7) == 0)
	;
      /* -save-temps overrides -pipe, so that temp files are produced */
      else if (save_temps_flag && strcmp (argv[i], "-pipe") == 0)
	error ("Warning: -pipe ignored since -save-temps specified");
      else if (argv[i][0] == '-' && argv[i][1] != 0)
	{
	  register char *p = &argv[i][1];
	  register int c = *p;

	  if (c == 'x')
	    {
	      if (p[1] == 0 && i + 1 == argc)
		fatal ("argument to `-x' is missing");
	      if (p[1] == 0)
		spec_lang = argv[++i];
	      else
		spec_lang = p + 1;
	      if (! strcmp (spec_lang, "none"))
		/* Suppress the warning if -xnone comes after the last input
		   file, because alternate command interfaces like g++ might
		   find it useful to place -xnone after each input file.  */
		spec_lang = 0;
	      else
		last_language_n_infiles = n_infiles;
	      continue;
	    }
	  switches[n_switches].part1 = p;
	  /* Deal with option arguments in separate argv elements.  */
	  if ((SWITCH_TAKES_ARG (c) > (p[1] != 0))
	      || WORD_SWITCH_TAKES_ARG (p))
	    {
	      int j = 0;
	      int n_args = WORD_SWITCH_TAKES_ARG (p);

	      if (n_args == 0)
		{
		  /* Count only the option arguments in separate argv elements.  */
		  n_args = SWITCH_TAKES_ARG (c) - (p[1] != 0);
		}
	      if (i + n_args >= argc)
		fatal ("argument to `-%s' is missing", p);
	      switches[n_switches].args
		= (char **) xmalloc ((n_args + 1) * sizeof (char *));
	      while (j < n_args)
		switches[n_switches].args[j++] = argv[++i];
	      /* Null-terminate the vector.  */
	      switches[n_switches].args[j] = 0;
	    }
	  else if (index (switches_need_spaces, c))
	    {
	      /* On some systems, ld cannot handle some options without
		 a space.  So split the option from its argument.  */
	      char *part1 = (char *) xmalloc (2);
	      part1[0] = c;
	      part1[1] = '\0';
	      
	      switches[n_switches].part1 = part1;
	      switches[n_switches].args = (char **) xmalloc (2 * sizeof (char *));
	      switches[n_switches].args[0] = xmalloc (strlen (p));
	      strcpy (switches[n_switches].args[0], &p[1]);
	      switches[n_switches].args[1] = 0;
	    }
	  else
	    switches[n_switches].args = 0;

	  switches[n_switches].live_cond = 0;
	  switches[n_switches].validated = 0;
	  /* This is always valid, since gcc.c itself understands it.  */
	  if (!strcmp (p, "save-temps"))
	    switches[n_switches].validated = 1;
          else
            {
              char ch = switches[n_switches].part1[0];
              if (ch == 'V' || ch == 'b' || ch == 'B')
                switches[n_switches].validated = 1;
            }
	  n_switches++;
	}
      else
	{
#ifdef HAVE_OBJECT_SUFFIX
	  argv[i] = convert_filename (argv[i], 0);
#endif

	  if (strcmp (argv[i], "-") != 0 && access (argv[i], R_OK) < 0)
	    {
	      perror_with_name (argv[i]);
	      error_count++;
	    }
	  else
	    {
	      infiles[n_infiles].language = spec_lang;
	      infiles[n_infiles++].name = argv[i];
	    }
	}
    }

  if (n_infiles == last_language_n_infiles && spec_lang != 0)
    error ("Warning: `-x %s' after last input file has no effect", spec_lang);

  switches[n_switches].part1 = 0;
  infiles[n_infiles].name = 0;
}

/* Process a spec string, accumulating and running commands.  */

/* These variables describe the input file name.
   input_file_number is the index on outfiles of this file,
   so that the output file name can be stored for later use by %o.
   input_basename is the start of the part of the input file
   sans all directory names, and basename_length is the number
   of characters starting there excluding the suffix .c or whatever.  */

const char *input_filename;
static int input_file_number;
size_t input_filename_length;
static int basename_length;
static const char *input_basename;
static const char *input_suffix;

/* These are variables used within do_spec and do_spec_1.  */

/* Nonzero if an arg has been started and not yet terminated
   (with space, tab or newline).  */
static int arg_going;

/* Nonzero means %d or %g has been seen; the next arg to be terminated
   is a temporary file name.  */
static int delete_this_arg;

/* Nonzero means %w has been seen; the next arg to be terminated
   is the output file name of this compilation.  */
static int this_is_output_file;

/* Nonzero means %s has been seen; the next arg to be terminated
   is the name of a library file and we should try the standard
   search dirs for it.  */
static int this_is_library_file;

/* Nonzero means that the input of this command is coming from a pipe.  */
static int input_from_pipe;

/* Process the spec SPEC and run the commands specified therein.
   Returns 0 if the spec is successfully processed; -1 if failed.  */

int
do_spec (spec)
     const char *spec;
{
  int value;

  clear_args ();
  arg_going = 0;
  delete_this_arg = 0;
  this_is_output_file = 0;
  this_is_library_file = 0;
  input_from_pipe = 0;

  value = do_spec_1 (spec, 0, NULL_PTR);

  /* Force out any unfinished command.
     If -pipe, this forces out the last command if it ended in `|'.  */
  if (value == 0)
    {
      if (argbuf_index > 0 && !strcmp (argbuf[argbuf_index - 1], "|"))
	argbuf_index--;

      if (argbuf_index > 0)
	value = execute ();
    }

  return value;
}

/* Process the sub-spec SPEC as a portion of a larger spec.
   This is like processing a whole spec except that we do
   not initialize at the beginning and we do not supply a
   newline by default at the end.
   INSWITCH nonzero means don't process %-sequences in SPEC;
   in this case, % is treated as an ordinary character.
   This is used while substituting switches.
   INSWITCH nonzero also causes SPC not to terminate an argument.

   Value is zero unless a line was finished
   and the command on that line reported an error.  */

static int
do_spec_1 (spec, inswitch, soft_matched_part)
     const char *spec;
     int inswitch;
     const char *soft_matched_part;
{
  register const char *p = spec;
  register int c;
  int i;
  const char *string;
  int value;

  while ((c = *p++))
    /* If substituting a switch, treat all chars like letters.
       Otherwise, NL, SPC, TAB and % are special.  */
    switch (inswitch ? 'a' : c)
      {
      case '\n':
	/* End of line: finish any pending argument,
	   then run the pending command if one has been started.  */
	if (arg_going)
	  {
	    obstack_1grow (&obstack, 0);
	    string = obstack_finish (&obstack);
	    if (this_is_library_file)
	      string = find_file (string);
	    store_arg (string, delete_this_arg, this_is_output_file);
	    if (this_is_output_file)
	      outfiles[input_file_number] = string;
	  }
	arg_going = 0;

	if (argbuf_index > 0 && !strcmp (argbuf[argbuf_index - 1], "|"))
	  {
	    for (i = 0; i < n_switches; i++)
	      if (!strcmp (switches[i].part1, "pipe"))
		break;

	    /* A `|' before the newline means use a pipe here,
	       but only if -pipe was specified.
	       Otherwise, execute now and don't pass the `|' as an arg.  */
	    if (i < n_switches)
	      {
		input_from_pipe = 1;
		switches[i].validated = 1;
		break;
	      }
	    else
	      argbuf_index--;
	  }

	if (argbuf_index > 0)
	  {
	    value = execute ();
	    if (value)
	      return value;
	  }
	/* Reinitialize for a new command, and for a new argument.  */
	clear_args ();
	arg_going = 0;
	delete_this_arg = 0;
	this_is_output_file = 0;
	this_is_library_file = 0;
	input_from_pipe = 0;
	break;

      case '|':
	/* End any pending argument.  */
	if (arg_going)
	  {
	    obstack_1grow (&obstack, 0);
	    string = obstack_finish (&obstack);
	    if (this_is_library_file)
	      string = find_file (string);
	    store_arg (string, delete_this_arg, this_is_output_file);
	    if (this_is_output_file)
	      outfiles[input_file_number] = string;
	  }

	/* Use pipe */
	obstack_1grow (&obstack, c);
	arg_going = 1;
	break;

      case '\t':
      case ' ':
	/* Space or tab ends an argument if one is pending.  */
	if (arg_going)
	  {
	    obstack_1grow (&obstack, 0);
	    string = obstack_finish (&obstack);
	    if (this_is_library_file)
	      string = find_file (string);
	    store_arg (string, delete_this_arg, this_is_output_file);
	    if (this_is_output_file)
	      outfiles[input_file_number] = string;
	  }
	/* Reinitialize for a new argument.  */
	arg_going = 0;
	delete_this_arg = 0;
	this_is_output_file = 0;
	this_is_library_file = 0;
	break;

      case '%':
	switch (c = *p++)
	  {
	  case 0:
	    fatal ("Invalid specification!  Bug in cc.");

	  case 'b':
	    obstack_grow (&obstack, input_basename, basename_length);
	    arg_going = 1;
	    break;

	  case 'd':
	    delete_this_arg = 2;
	    break;

	  /* Dump out the directories specified with LIBRARY_PATH,
	     followed by the absolute directories
	     that we search for startfiles.  */
	  case 'D':
	    {
	      struct prefix_list *pl = startfile_prefixes.plist;
	      size_t bufsize = 100;
	      char *buffer = (char *) xmalloc (bufsize);
	      int idx;

	      for (; pl; pl = pl->next)
		{
#ifdef RELATIVE_PREFIX_NOT_LINKDIR
		  /* Used on systems which record the specified -L dirs
		     and use them to search for dynamic linking.  */
		  /* Relative directories always come from -B,
		     and it is better not to use them for searching
		     at run time.  In particular, stage1 loses  */
		  if (!IS_DIR_SEPARATOR (pl->prefix[0]))
		    continue;
#endif
		  /* Try subdirectory if there is one.  */
		  if (multilib_dir != NULL)
		    {
		      if (machine_suffix)
			{
			  if (strlen (pl->prefix) + strlen (machine_suffix)
			      >= bufsize)
			    bufsize = (strlen (pl->prefix)
				       + strlen (machine_suffix)) * 2 + 1;
			  buffer = (char *) xrealloc (buffer, bufsize);
			  strcpy (buffer, pl->prefix);
			  strcat (buffer, machine_suffix);
			  if (is_directory (buffer, multilib_dir, 1))
			    {
			      do_spec_1 ("-L", 0, NULL_PTR);
#ifdef SPACE_AFTER_L_OPTION
			      do_spec_1 (" ", 0, NULL_PTR);
#endif
			      do_spec_1 (buffer, 1, NULL_PTR);
			      do_spec_1 (multilib_dir, 1, NULL_PTR);
			      /* Make this a separate argument.  */
			      do_spec_1 (" ", 0, NULL_PTR);
			    }
			}
		      if (!pl->require_machine_suffix)
			{
			  if (is_directory (pl->prefix, multilib_dir, 1))
			    {
			      do_spec_1 ("-L", 0, NULL_PTR);
#ifdef SPACE_AFTER_L_OPTION
			      do_spec_1 (" ", 0, NULL_PTR);
#endif
			      do_spec_1 (pl->prefix, 1, NULL_PTR);
			      do_spec_1 (multilib_dir, 1, NULL_PTR);
			      /* Make this a separate argument.  */
			      do_spec_1 (" ", 0, NULL_PTR);
			    }
			}
		    }
		  if (machine_suffix)
		    {
		      if (is_directory (pl->prefix, machine_suffix, 1))
			{
			  do_spec_1 ("-L", 0, NULL_PTR);
#ifdef SPACE_AFTER_L_OPTION
			  do_spec_1 (" ", 0, NULL_PTR);
#endif
			  do_spec_1 (pl->prefix, 1, NULL_PTR);
			  /* Remove slash from machine_suffix.  */
			  if (strlen (machine_suffix) >= bufsize)
			    bufsize = strlen (machine_suffix) * 2 + 1;
			  buffer = (char *) xrealloc (buffer, bufsize);
			  strcpy (buffer, machine_suffix);
			  idx = strlen (buffer);
			  if (IS_DIR_SEPARATOR (buffer[idx - 1]))
			    buffer[idx - 1] = 0;
			  do_spec_1 (buffer, 1, NULL_PTR);
			  /* Make this a separate argument.  */
			  do_spec_1 (" ", 0, NULL_PTR);
			}
		    }
		  if (!pl->require_machine_suffix)
		    {
		      if (is_directory (pl->prefix, "", 1))
			{
			  do_spec_1 ("-L", 0, NULL_PTR);
#ifdef SPACE_AFTER_L_OPTION
			  do_spec_1 (" ", 0, NULL_PTR);
#endif
			  /* Remove slash from pl->prefix.  */
			  if (strlen (pl->prefix) >= bufsize)
			    bufsize = strlen (pl->prefix) * 2 + 1;
			  buffer = (char *) xrealloc (buffer, bufsize);
			  strcpy (buffer, pl->prefix);
			  idx = strlen (buffer);
			  if (IS_DIR_SEPARATOR (buffer[idx - 1]))
			    buffer[idx - 1] = 0;
			  do_spec_1 (buffer, 1, NULL_PTR);
			  /* Make this a separate argument.  */
			  do_spec_1 (" ", 0, NULL_PTR);
			}
		    }
		}
	      free (buffer);
	    }
	    break;

	  case 'e':
	    /* %efoo means report an error with `foo' as error message
	       and don't execute any more commands for this file.  */
	    {
	      const char *q = p;
	      char *buf;
	      while (*p != 0 && *p != '\n') p++;
	      buf = (char *) alloca (p - q + 1);
	      strncpy (buf, q, p - q);
	      buf[p - q] = 0;
	      error (buf);
	      return -1;
	    }
	    break;

	  case 'g':
	  case 'u':
	  case 'U':
	    if (save_temps_flag)
	      {
		obstack_grow (&obstack, input_basename, basename_length);
		delete_this_arg = 0;
	      }
	    else
	      {
#ifdef MKTEMP_EACH_FILE
		/* ??? This has a problem: the total number of
		   values mktemp can return is limited.
		   That matters for the names of object files.
		   In 2.4, do something about that.  */
		struct temp_name *t;
		int suffix_length;
		const char *suffix = p;

		if (p[0] == '%' && p[1] == 'O')
		  {
		    p += 2;
		    /* We don't support extra suffix characters after %O.  */
		    if (*p == '.' || ISALPHA ((unsigned char)*p))
		      abort ();
		    suffix = OBJECT_SUFFIX;
		    suffix_length = strlen (OBJECT_SUFFIX);
		  }
		else
		  {
		    while (*p == '.' || ISALPHA ((unsigned char)*p))
		      p++;
		    suffix_length = p - suffix;
		  }

		/* See if we already have an association of %g/%u/%U and
		   suffix.  */
		for (t = temp_names; t; t = t->next)
		  if (t->length == suffix_length
		      && strncmp (t->suffix, suffix, suffix_length) == 0
		      && t->unique == (c != 'g'))
		    break;

		/* Make a new association if needed.  %u requires one.  */
		if (t == 0 || c == 'u')
		  {
		    if (t == 0)
		      {
			t = (struct temp_name *) xmalloc (sizeof (struct temp_name));
			t->next = temp_names;
			temp_names = t;
		      }
		    t->length = suffix_length;
		    t->suffix = save_string (suffix, suffix_length);
		    t->unique = (c != 'g');
		    temp_filename = make_temp_file (t->suffix);
		    temp_filename_length = strlen (temp_filename);
		    t->filename = temp_filename;
		    t->filename_length = temp_filename_length;
		  }

		obstack_grow (&obstack, t->filename, t->filename_length);
		delete_this_arg = 1;
#else
		obstack_grow (&obstack, temp_filename, temp_filename_length);
		if (c == 'u' || c == 'U')
		  {
		    static int unique;
		    char buff[9];
		    if (c == 'u')
		      unique++;
		    sprintf (buff, "%d", unique);
		    obstack_grow (&obstack, buff, strlen (buff));
		  }
#endif
		delete_this_arg = 1;
	      }
	    arg_going = 1;
	    break;

	  case 'i':
	    obstack_grow (&obstack, input_filename, input_filename_length);
	    arg_going = 1;
	    break;

	  case 'I':
	    {
	      struct prefix_list *pl = include_prefixes.plist;

	      if (gcc_exec_prefix)
		{
		  do_spec_1 ("-iprefix", 1, NULL_PTR);
		  /* Make this a separate argument.  */
		  do_spec_1 (" ", 0, NULL_PTR);
		  do_spec_1 (gcc_exec_prefix, 1, NULL_PTR);
		  do_spec_1 (" ", 0, NULL_PTR);
		}

	      for (; pl; pl = pl->next)
		{
		  do_spec_1 ("-isystem", 1, NULL_PTR);
		  /* Make this a separate argument.  */
		  do_spec_1 (" ", 0, NULL_PTR);
		  do_spec_1 (pl->prefix, 1, NULL_PTR);
		  do_spec_1 (" ", 0, NULL_PTR);
		}
	    }
	    break;

	  case 'o':
	    {
	      int max = n_infiles;
	      max += lang_specific_extra_outfiles;

	      for (i = 0; i < max; i++)
		if (outfiles[i])
		  store_arg (outfiles[i], 0, 0);
	      break;
	    }

	  case 'O':
	    obstack_grow (&obstack, OBJECT_SUFFIX, strlen (OBJECT_SUFFIX));
	    arg_going = 1;
	    break;

	  case 's':
	    this_is_library_file = 1;
	    break;

	  case 'w':
	    this_is_output_file = 1;
	    break;

	  case 'W':
	    {
	      int cur_index = argbuf_index;
	      /* Handle the {...} following the %W.  */
	      if (*p != '{')
		abort ();
	      p = handle_braces (p + 1);
	      if (p == 0)
		return -1;
	      /* If any args were output, mark the last one for deletion
		 on failure.  */
	      if (argbuf_index != cur_index)
		record_temp_file (argbuf[argbuf_index - 1], 0, 1);
	      break;
	    }

	  /* %x{OPTION} records OPTION for %X to output.  */
	  case 'x':
	    {
	      const char *p1 = p;
	      char *string;

	      /* Skip past the option value and make a copy.  */
	      if (*p != '{')
		abort ();
	      while (*p++ != '}')
		;
	      string = save_string (p1 + 1, p - p1 - 2);

	      /* See if we already recorded this option.  */
	      for (i = 0; i < n_linker_options; i++)
		if (! strcmp (string, linker_options[i]))
		  {
		    free (string);
		    return 0;
		  }

	      /* This option is new; add it.  */
	      add_linker_option (string, strlen (string));
	    }
	    break;

	  /* Dump out the options accumulated previously using %x.  */
	  case 'X':
	    for (i = 0; i < n_linker_options; i++)
	      {
		do_spec_1 (linker_options[i], 1, NULL_PTR);
		/* Make each accumulated option a separate argument.  */
		do_spec_1 (" ", 0, NULL_PTR);
	      }
	    break;

	  /* Dump out the options accumulated previously using -Wa,.  */
	  case 'Y':
	    for (i = 0; i < n_assembler_options; i++)
	      {
		do_spec_1 (assembler_options[i], 1, NULL_PTR);
		/* Make each accumulated option a separate argument.  */
		do_spec_1 (" ", 0, NULL_PTR);
	      }
	    break;

	  /* Dump out the options accumulated previously using -Wp,.  */
	  case 'Z':
	    for (i = 0; i < n_preprocessor_options; i++)
	      {
		do_spec_1 (preprocessor_options[i], 1, NULL_PTR);
		/* Make each accumulated option a separate argument.  */
		do_spec_1 (" ", 0, NULL_PTR);
	      }
	    break;

	    /* Here are digits and numbers that just process
	       a certain constant string as a spec.  */

	  case '1':
	    value = do_spec_1 (cc1_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case '2':
	    value = do_spec_1 (cc1plus_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'a':
	    value = do_spec_1 (asm_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'A':
	    value = do_spec_1 (asm_final_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'c':
	    value = do_spec_1 (signed_char_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'C':
	    value = do_spec_1 (cpp_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'E':
	    value = do_spec_1 (endfile_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'l':
	    value = do_spec_1 (link_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'L':
	    value = do_spec_1 (lib_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'G':
	    value = do_spec_1 (libgcc_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	  case 'p':
	    {
	      char *x = (char *) alloca (strlen (cpp_predefines) + 1);
	      char *buf = x;
	      char *y;

	      /* Copy all of the -D options in CPP_PREDEFINES into BUF.  */
	      y = cpp_predefines;
	      while (*y != 0)
		{
		  if (! strncmp (y, "-D", 2))
		    /* Copy the whole option.  */
		    while (*y && *y != ' ' && *y != '\t')
		      *x++ = *y++;
		  else if (*y == ' ' || *y == '\t')
		    /* Copy whitespace to the result.  */
		    *x++ = *y++;
		  /* Don't copy other options.  */
		  else
		    y++;
		}

	      *x = 0;

	      value = do_spec_1 (buf, 0, NULL_PTR);
	      if (value != 0)
		return value;
	    }
	    break;

	  case 'P':
	    {
	      char *x = (char *) alloca (strlen (cpp_predefines) * 4 + 1);
	      char *buf = x;
	      char *y;

	      /* Copy all of CPP_PREDEFINES into BUF,
		 but put __ after every -D and at the end of each arg.  */
	      y = cpp_predefines;
	      while (*y != 0)
		{
		  if (! strncmp (y, "-D", 2))
		    {
		      int flag = 0;

		      *x++ = *y++;
		      *x++ = *y++;

		      if (*y != '_'
			  || (*(y+1) != '_'
			      && ! ISUPPER ((unsigned char)*(y+1))))
		        {
			  /* Stick __ at front of macro name.  */
			  *x++ = '_';
			  *x++ = '_';
			  /* Arrange to stick __ at the end as well.  */
			  flag = 1;
			}

		      /* Copy the macro name.  */
		      while (*y && *y != '=' && *y != ' ' && *y != '\t')
			*x++ = *y++;

		      if (flag)
		        {
			  *x++ = '_';
			  *x++ = '_';
			}

		      /* Copy the value given, if any.  */
		      while (*y && *y != ' ' && *y != '\t')
			*x++ = *y++;
		    }
		  else if (*y == ' ' || *y == '\t')
		    /* Copy whitespace to the result.  */
		    *x++ = *y++;
		  /* Don't copy -A options  */
		  else
		    y++;
		}
	      *x++ = ' ';

	      /* Copy all of CPP_PREDEFINES into BUF,
		 but put __ after every -D.  */
	      y = cpp_predefines;
	      while (*y != 0)
		{
		  if (! strncmp (y, "-D", 2))
		    {
		      y += 2;

		      if (*y != '_'
			  || (*(y+1) != '_'
			      && ! ISUPPER ((unsigned char)*(y+1))))
		        {
			  /* Stick -D__ at front of macro name.  */
			  *x++ = '-';
			  *x++ = 'D';
			  *x++ = '_';
			  *x++ = '_';

			  /* Copy the macro name.  */
			  while (*y && *y != '=' && *y != ' ' && *y != '\t')
			    *x++ = *y++;

			  /* Copy the value given, if any.  */
			  while (*y && *y != ' ' && *y != '\t')
			    *x++ = *y++;
			}
		      else
			{
			  /* Do not copy this macro - we have just done it before */
			  while (*y && *y != ' ' && *y != '\t')
			    y++;
			}
		    }
		  else if (*y == ' ' || *y == '\t')
		    /* Copy whitespace to the result.  */
		    *x++ = *y++;
		  /* Don't copy -A options  */
		  else
		    y++;
		}
	      *x++ = ' ';

	      /* Copy all of the -A options in CPP_PREDEFINES into BUF.  */
	      y = cpp_predefines;
	      while (*y != 0)
		{
		  if (! strncmp (y, "-A", 2))
		    /* Copy the whole option.  */
		    while (*y && *y != ' ' && *y != '\t')
		      *x++ = *y++;
		  else if (*y == ' ' || *y == '\t')
		    /* Copy whitespace to the result.  */
		    *x++ = *y++;
		  /* Don't copy other options.  */
		  else
		    y++;
		}

	      *x = 0;

	      value = do_spec_1 (buf, 0, NULL_PTR);
	      if (value != 0)
		return value;
	    }
	    break;

	  case 'S':
	    value = do_spec_1 (startfile_spec, 0, NULL_PTR);
	    if (value != 0)
	      return value;
	    break;

	    /* Here we define characters other than letters and digits.  */

	  case '{':
	    p = handle_braces (p);
	    if (p == 0)
	      return -1;
	    break;

	  case '%':
	    obstack_1grow (&obstack, '%');
	    break;

	  case '*':
	    do_spec_1 (soft_matched_part, 1, NULL_PTR);
	    do_spec_1 (" ", 0, NULL_PTR);
	    break;

	    /* Process a string found as the value of a spec given by name.
	       This feature allows individual machine descriptions
	       to add and use their own specs.
	       %[...] modifies -D options the way %P does;
	       %(...) uses the spec unmodified.  */
	  case '[':
	    error ("Warning: use of obsolete %%[ operator in specs");
	  case '(':
	    {
	      const char *name = p;
	      struct spec_list *sl;
	      int len;

	      /* The string after the S/P is the name of a spec that is to be
		 processed.  */
	      while (*p && *p != ')' && *p != ']')
		p++;

	      /* See if it's in the list */
	      for (len = p - name, sl = specs; sl; sl = sl->next)
		if (sl->name_len == len && !strncmp (sl->name, name, len))
		  {
		    name = *(sl->ptr_spec);
#ifdef DEBUG_SPECS
		    notice ("Processing spec %c%s%c, which is '%s'\n",
			    c, sl->name, (c == '(') ? ')' : ']', name);
#endif
		    break;
		  }

	      if (sl)
		{
		  if (c == '(')
		    {
		      value = do_spec_1 (name, 0, NULL_PTR);
		      if (value != 0)
			return value;
		    }
		  else
		    {
		      char *x = (char *) alloca (strlen (name) * 2 + 1);
		      char *buf = x;
		      const char *y = name;
		      int flag = 0;

		      /* Copy all of NAME into BUF, but put __ after
			 every -D and at the end of each arg,  */
		      while (1)
			{
			  if (! strncmp (y, "-D", 2))
			    {
			      *x++ = '-';
			      *x++ = 'D';
			      *x++ = '_';
			      *x++ = '_';
			      y += 2;
			      flag = 1;
			      continue;
			    }
                          else if (flag && (*y == ' ' || *y == '\t' || *y == '='
                                            || *y == '}' || *y == 0))
			    {
			      *x++ = '_';
			      *x++ = '_';
			      flag = 0;
			    }
                          if (*y == 0)
			    break;
			  else
			    *x++ = *y++;
			}
		      *x = 0;

		      value = do_spec_1 (buf, 0, NULL_PTR);
		      if (value != 0)
			return value;
		    }
		}

	      /* Discard the closing paren or bracket.  */
	      if (*p)
		p++;
	    }
	    break;

	  case 'v':
	    {
	      int c1 = *p++;  /* Select first or second version number.  */
	      char *v = compiler_version;
	      char *q;

	      /* The format of the version string is
		 ([^0-9]*-)?[0-9]+[.][0-9]+([.][0-9]+)?([- ].*)?  */

	      /* Ignore leading non-digits.  i.e. "foo-" in "foo-2.7.2".  */
	      while (! ISDIGIT (*v))
		v++;
	      if (v > compiler_version && v[-1] != '-')
		abort ();

	      /* If desired, advance to second version number.  */
	      if (c1 == '2')
		{
		  /* Set V after the first period.  */
		  while (ISDIGIT (*v))
		    v++;
		  if (*v != '.')
		    abort ();
		  v++;
		}

	      /* Set Q at the next period or at the end.  */
	      q = v;
	      while (ISDIGIT (*q))
		q++;
	      if (*q != 0 && *q != ' ' && *q != '.' && *q != '-')
		abort ();

	      /* Put that part into the command.  */
	      obstack_grow (&obstack, v, q - v);
	      arg_going = 1;
	    }
	    break;

	  case '|':
	    if (input_from_pipe)
	      do_spec_1 ("-", 0, NULL_PTR);
	    break;

	  default:
	    abort ();
	  }
	break;

      case '\\':
	/* Backslash: treat next character as ordinary.  */
	c = *p++;

	/* fall through */
      default:
	/* Ordinary character: put it into the current argument.  */
	obstack_1grow (&obstack, c);
	arg_going = 1;
      }

  return 0;		/* End of string */
}

/* Return 0 if we call do_spec_1 and that returns -1.  */

static const char *
handle_braces (p)
     register const char *p;
{
  const char *filter, *body = NULL, *endbody = NULL;
  int pipe_p = 0;
  int negate;
  int suffix;
  int include_blanks = 1;

  if (*p == '^')
    /* A '^' after the open-brace means to not give blanks before args.  */
    include_blanks = 0, ++p;

  if (*p == '|')
    /* A `|' after the open-brace means,
       if the test fails, output a single minus sign rather than nothing.
       This is used in %{|!pipe:...}.  */
    pipe_p = 1, ++p;

next_member:
  negate = suffix = 0;

  if (*p == '!')
    /* A `!' after the open-brace negates the condition:
       succeed if the specified switch is not present.  */
    negate = 1, ++p;

  if (*p == '.')
    /* A `.' after the open-brace means test against the current suffix.  */
    {
      if (pipe_p)
	abort ();

      suffix = 1;
      ++p;
    }

  filter = p;
  while (*p != ':' && *p != '}' && *p != '|') p++;

  if (*p == '|' && pipe_p)
    abort ();

  if (!body)
    {
      if (*p != '}')
        {
	  register int count = 1;
	  register const char *q = p;

	  while (*q++ != ':') continue;
	  body = q;
	  
	  while (count > 0)
	    {
	      if (*q == '{')
	        count++;
	      else if (*q == '}')
	        count--;
	      else if (*q == 0)
	        abort ();
	      q++;
	    }
	  endbody = q;
	}
      else
	body = p, endbody = p+1;
    }

  if (suffix)
    {
      int found = (input_suffix != 0
		   && (long) strlen (input_suffix) == (long)(p - filter)
		   && strncmp (input_suffix, filter, p - filter) == 0);

      if (body[0] == '}')
	abort ();

      if (negate != found
	  && do_spec_1 (save_string (body, endbody-body-1), 0, NULL_PTR) < 0)
	return 0;
    }
  else if (p[-1] == '*' && p[0] == '}')
    {
      /* Substitute all matching switches as separate args.  */
      register int i;
      --p;
      for (i = 0; i < n_switches; i++)
	if (!strncmp (switches[i].part1, filter, p - filter)
	    && check_live_switch (i, p - filter))
	  give_switch (i, 0, include_blanks);
    }
  else
    {
      /* Test for presence of the specified switch.  */
      register int i;
      int present = 0;

      /* If name specified ends in *, as in {x*:...},
	 check for %* and handle that case.  */
      if (p[-1] == '*' && !negate)
	{
	  int substitution;
	  const char *r = body;

	  /* First see whether we have %*.  */
	  substitution = 0;
	  while (r < endbody)
	    {
	      if (*r == '%' && r[1] == '*')
		substitution = 1;
	      r++;
	    }
	  /* If we do, handle that case.  */
	  if (substitution)
	    {
	      /* Substitute all matching switches as separate args.
		 But do this by substituting for %*
		 in the text that follows the colon.  */

	      unsigned hard_match_len = p - filter - 1;
	      char *string = save_string (body, endbody - body - 1);

	      for (i = 0; i < n_switches; i++)
		if (!strncmp (switches[i].part1, filter, hard_match_len)
		    && check_live_switch (i, -1))
		  {
		    do_spec_1 (string, 0, &switches[i].part1[hard_match_len]);
		    /* Pass any arguments this switch has.  */
		    give_switch (i, 1, 1);
		  }

	      /* We didn't match.  Try again.  */
	      if (*p++ == '|')
		goto next_member;
	      return endbody;
	    }
	}

      /* If name specified ends in *, as in {x*:...},
	 check for presence of any switch name starting with x.  */
      if (p[-1] == '*')
	{
	  for (i = 0; i < n_switches; i++)
	    {
	      unsigned hard_match_len = p - filter - 1;

	      if (!strncmp (switches[i].part1, filter, hard_match_len)
		  && check_live_switch (i, hard_match_len))
		{
		  present = 1;
		}
	    }
	}
      /* Otherwise, check for presence of exact name specified.  */
      else
	{
	  for (i = 0; i < n_switches; i++)
	    {
	      if (!strncmp (switches[i].part1, filter, p - filter)
		  && switches[i].part1[p - filter] == 0
		  && check_live_switch (i, -1))
		{
		  present = 1;
		  break;
		}
	    }
	}

      /* If it is as desired (present for %{s...}, absent for %{!s...})
	 then substitute either the switch or the specified
	 conditional text.  */
      if (present != negate)
	{
	  if (*p == '}')
	    {
	      give_switch (i, 0, include_blanks);
	    }
	  else
	    {
	      if (do_spec_1 (save_string (body, endbody - body - 1),
			     0, NULL_PTR) < 0)
		return 0;
	    }
	}
      else if (pipe_p)
	{
	  /* Here if a %{|...} conditional fails: output a minus sign,
	     which means "standard output" or "standard input".  */
	  do_spec_1 ("-", 0, NULL_PTR);
	  return endbody;
	}
    }

  /* We didn't match; try again.  */
  if (*p++ == '|')
    goto next_member;

  return endbody;
}

/* Return 0 iff switch number SWITCHNUM is obsoleted by a later switch
   on the command line.  PREFIX_LENGTH is the length of XXX in an {XXX*}
   spec, or -1 if either exact match or %* is used.

   A -O switch is obsoleted by a later -O switch.  A -f, -m, or -W switch
   whose value does not begin with "no-" is obsoleted by the same value
   with the "no-", similarly for a switch with the "no-" prefix.  */

static int
check_live_switch (switchnum, prefix_length)
     int switchnum;
     int prefix_length;
{
  const char *name = switches[switchnum].part1;
  int i;

  /* In the common case of {<at-most-one-letter>*}, a negating
     switch would always match, so ignore that case.  We will just
     send the conflicting switches to the compiler phase.  */
  if (prefix_length >= 0 && prefix_length <= 1)
    return 1;

  /* If we already processed this switch and determined if it was
     live or not, return our past determination.  */
  if (switches[switchnum].live_cond != 0)
    return switches[switchnum].live_cond > 0;

  /* Now search for duplicate in a manner that depends on the name.  */
  switch (*name)
    {
    case 'O':
	for (i = switchnum + 1; i < n_switches; i++)
	  if (switches[i].part1[0] == 'O')
	    {
	      switches[switchnum].validated = 1;
	      switches[switchnum].live_cond = -1;
	      return 0;
	    }
      break;

    case 'W':  case 'f':  case 'm':
      if (! strncmp (name + 1, "no-", 3))
	{
	  /* We have Xno-YYY, search for XYYY.  */
	  for (i = switchnum + 1; i < n_switches; i++)
	    if (switches[i].part1[0] == name[0]
		&& ! strcmp (&switches[i].part1[1], &name[4]))
	    {
	      switches[switchnum].validated = 1;
	      switches[switchnum].live_cond = -1;
	      return 0;
	    }
	}
      else
	{
	  /* We have XYYY, search for Xno-YYY.  */
	  for (i = switchnum + 1; i < n_switches; i++)
	    if (switches[i].part1[0] == name[0]
		&& switches[i].part1[1] == 'n'
		&& switches[i].part1[2] == 'o'
		&& switches[i].part1[3] == '-'
		&& !strcmp (&switches[i].part1[4], &name[1]))
	    {
	      switches[switchnum].validated = 1;
	      switches[switchnum].live_cond = -1;
	      return 0;
	    }
	}
      break;
    }

  /* Otherwise the switch is live.  */
  switches[switchnum].live_cond = 1;
  return 1;
}

/* Pass a switch to the current accumulating command
   in the same form that we received it.
   SWITCHNUM identifies the switch; it is an index into
   the vector of switches gcc received, which is `switches'.
   This cannot fail since it never finishes a command line.

   If OMIT_FIRST_WORD is nonzero, then we omit .part1 of the argument.

   If INCLUDE_BLANKS is nonzero, then we include blanks before each argument
   of the switch.  */

static void
give_switch (switchnum, omit_first_word, include_blanks)
     int switchnum;
     int omit_first_word;
     int include_blanks;
{
  if (!omit_first_word)
    {
      do_spec_1 ("-", 0, NULL_PTR);
      do_spec_1 (switches[switchnum].part1, 1, NULL_PTR);
    }

  if (switches[switchnum].args != 0)
    {
      char **p;
      for (p = switches[switchnum].args; *p; p++)
	{
	  if (include_blanks)
	    do_spec_1 (" ", 0, NULL_PTR);
	  do_spec_1 (*p, 1, NULL_PTR);
	}
    }

  do_spec_1 (" ", 0, NULL_PTR);
  switches[switchnum].validated = 1;
}

/* Search for a file named NAME trying various prefixes including the
   user's -B prefix and some standard ones.
   Return the absolute file name found.  If nothing is found, return NAME.  */

static const char *
find_file (name)
     const char *name;
{
  char *newname;

  /* Try multilib_dir if it is defined.  */
  if (multilib_dir != NULL)
    {
      char *try;

      try = (char *) alloca (strlen (multilib_dir) + strlen (name) + 2);
      strcpy (try, multilib_dir);
      strcat (try, dir_separator_str);
      strcat (try, name);

      newname = find_a_file (&startfile_prefixes, try, R_OK);

      /* If we don't find it in the multi library dir, then fall
	 through and look for it in the normal places.  */
      if (newname != NULL)
	return newname;
    }

  newname = find_a_file (&startfile_prefixes, name, R_OK);
  return newname ? newname : name;
}

/* Determine whether a directory exists.  If LINKER, return 0 for
   certain fixed names not needed by the linker.  If not LINKER, it is
   only important to return 0 if the host machine has a small ARG_MAX
   limit.  */

static int
is_directory (path1, path2, linker)
     const char *path1;
     const char *path2;
     int linker;
{
  int len1 = strlen (path1);
  int len2 = strlen (path2);
  char *path = (char *) alloca (3 + len1 + len2);
  char *cp;
  struct stat st;

#ifndef SMALL_ARG_MAX
  if (! linker)
    return 1;
#endif

  /* Construct the path from the two parts.  Ensure the string ends with "/.".
     The resulting path will be a directory even if the given path is a
     symbolic link.  */
  memcpy (path, path1, len1);
  memcpy (path + len1, path2, len2);
  cp = path + len1 + len2;
  if (!IS_DIR_SEPARATOR (cp[-1]))
    *cp++ = DIR_SEPARATOR;
  *cp++ = '.';
  *cp = '\0';

#ifndef FREEBSD_NATIVE
  /* Exclude directories that the linker is known to search.  */
  if (linker
      && ((cp - path == 6
	   && strcmp (path, concat (dir_separator_str, "lib", 
				    dir_separator_str, ".", NULL_PTR)) == 0)
	  || (cp - path == 10
	      && strcmp (path, concat (dir_separator_str, "usr", 
				       dir_separator_str, "lib", 
				       dir_separator_str, ".", NULL_PTR)) == 0)))
    return 0;
#endif	/* not FREEBSD_NATIVE */

  return (stat (path, &st) >= 0 && S_ISDIR (st.st_mode));
}

/* On fatal signals, delete all the temporary files.  */

static void
fatal_error (signum)
     int signum;
{
  signal (signum, SIG_DFL);
  delete_failure_queue ();
  delete_temp_files ();
  /* Get the same signal again, this time not handled,
     so its normal effect occurs.  */
  kill (getpid (), signum);
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  register size_t i;
  size_t j;
  int value;
  int linker_was_run = 0;
  char *explicit_link_files;
  char *specs_file;
  const char *p;
  struct user_specs *uptr;

  p = argv[0] + strlen (argv[0]);
  while (p != argv[0] && !IS_DIR_SEPARATOR (p[-1]))
    --p;
  programname = p;

#ifdef HAVE_LC_MESSAGES
  setlocale (LC_MESSAGES, "");
#endif
  (void) bindtextdomain (PACKAGE, localedir);
  (void) textdomain (PACKAGE);

  if (signal (SIGINT, SIG_IGN) != SIG_IGN)
    signal (SIGINT, fatal_error);
#ifdef SIGHUP
  if (signal (SIGHUP, SIG_IGN) != SIG_IGN)
    signal (SIGHUP, fatal_error);
#endif
  if (signal (SIGTERM, SIG_IGN) != SIG_IGN)
    signal (SIGTERM, fatal_error);
#ifdef SIGPIPE
  if (signal (SIGPIPE, SIG_IGN) != SIG_IGN)
    signal (SIGPIPE, fatal_error);
#endif

  argbuf_length = 10;
  argbuf = (char **) xmalloc (argbuf_length * sizeof (char *));

  obstack_init (&obstack);

  /* Build multilib_select, et. al from the separate lines that make up each
     multilib selection.  */
  {
    char **q = multilib_raw;
    int need_space;

    obstack_init (&multilib_obstack);
    while ((p = *q++) != (char *) 0)
      obstack_grow (&multilib_obstack, p, strlen (p));

    obstack_1grow (&multilib_obstack, 0);
    multilib_select = obstack_finish (&multilib_obstack);

    q = multilib_matches_raw;
    while ((p = *q++) != (char *) 0)
      obstack_grow (&multilib_obstack, p, strlen (p));

    obstack_1grow (&multilib_obstack, 0);
    multilib_matches = obstack_finish (&multilib_obstack);

    need_space = FALSE;
    for (i = 0;
	 i < sizeof (multilib_defaults_raw) / sizeof (multilib_defaults_raw[0]);
	 i++)
      {
	if (need_space)
	  obstack_1grow (&multilib_obstack, ' ');
	obstack_grow (&multilib_obstack,
		      multilib_defaults_raw[i],
		      strlen (multilib_defaults_raw[i]));
	need_space = TRUE;
      }

    obstack_1grow (&multilib_obstack, 0);
    multilib_defaults = obstack_finish (&multilib_obstack);
  }

  /* Set up to remember the pathname of gcc and any options
     needed for collect.  We use argv[0] instead of programname because
     we need the complete pathname.  */
  obstack_init (&collect_obstack);
  obstack_grow (&collect_obstack, "COLLECT_GCC=", sizeof ("COLLECT_GCC=")-1);
  obstack_grow (&collect_obstack, argv[0], strlen (argv[0])+1);
  putenv (obstack_finish (&collect_obstack));

#ifdef INIT_ENVIRONMENT
  /* Set up any other necessary machine specific environment variables.  */
  putenv (INIT_ENVIRONMENT);
#endif

  /* Choose directory for temp files.  */

#ifndef MKTEMP_EACH_FILE
  temp_filename = choose_temp_base ();
  temp_filename_length = strlen (temp_filename);
#endif

  /* Make a table of what switches there are (switches, n_switches).
     Make a table of specified input files (infiles, n_infiles).
     Decode switches that are handled locally.  */

  process_command (argc, argv);

  {
    int first_time;

    /* Build COLLECT_GCC_OPTIONS to have all of the options specified to
       the compiler.  */
    obstack_grow (&collect_obstack, "COLLECT_GCC_OPTIONS=",
		  sizeof ("COLLECT_GCC_OPTIONS=")-1);

    first_time = TRUE;
    for (i = 0; (int)i < n_switches; i++)
      {
	char **args;
	const char *p, *q;
	if (!first_time)
	  obstack_grow (&collect_obstack, " ", 1);

	first_time = FALSE;
	obstack_grow (&collect_obstack, "'-", 2);
        q = switches[i].part1;
	while ((p = index (q,'\'')))
          {
            obstack_grow (&collect_obstack, q, p-q);
            obstack_grow (&collect_obstack, "'\\''", 4);
            q = ++p;
          }
        obstack_grow (&collect_obstack, q, strlen (q));
	obstack_grow (&collect_obstack, "'", 1);

	for (args = switches[i].args; args && *args; args++)
	  {
	    obstack_grow (&collect_obstack, " '", 2);
	    q = *args;
	    while ((p = index (q,'\'')))
	      {
		obstack_grow (&collect_obstack, q, p-q);
		obstack_grow (&collect_obstack, "'\\''", 4);
		q = ++p;
	      }
	    obstack_grow (&collect_obstack, q, strlen (q));
	    obstack_grow (&collect_obstack, "'", 1);
	  }
      }
    obstack_grow (&collect_obstack, "\0", 1);
    putenv (obstack_finish (&collect_obstack));
  }

  /* Initialize the vector of specs to just the default.
     This means one element containing 0s, as a terminator.  */

  compilers = (struct compiler *) xmalloc (sizeof default_compilers);
  bcopy ((char *) default_compilers, (char *) compilers,
	 sizeof default_compilers);
  n_compilers = n_default_compilers;

  /* Read specs from a file if there is one.  */

#ifdef FREEBSD_NATIVE
  just_machine_suffix = "";	/* I don't support specs, gives determinism */
#else	/* FREEBSD_NATIVE */
  machine_suffix = concat (spec_machine, dir_separator_str,
			   spec_version, dir_separator_str, NULL_PTR);
  just_machine_suffix = concat (spec_machine, dir_separator_str, NULL_PTR);
#endif	/* FREEBSD_NATIVE */

  specs_file = find_a_file (&startfile_prefixes, "specs", R_OK);
  /* Read the specs file unless it is a default one.  */
  if (specs_file != 0 && strcmp (specs_file, "specs"))
    read_specs (specs_file, TRUE);
  else
    init_spec ();

  /* We need to check standard_exec_prefix/just_machine_suffix/specs
     for any override of as, ld and libraries. */
  specs_file = (char *) alloca (strlen (standard_exec_prefix)
				+ strlen (just_machine_suffix)
				+ sizeof ("specs"));

  strcpy (specs_file, standard_exec_prefix);
  strcat (specs_file, just_machine_suffix);
  strcat (specs_file, "specs");
  if (access (specs_file, R_OK) == 0)
    read_specs (specs_file, TRUE);
 
  /* If not cross-compiling, look for startfiles in the standard places.  */
  if (*cross_compile == '0')
    {
#ifdef MD_EXEC_PREFIX
      add_prefix (&exec_prefixes, md_exec_prefix, "GCC", 0, 0, NULL_PTR);
      add_prefix (&startfile_prefixes, md_exec_prefix, "GCC", 0, 0, NULL_PTR);
#endif

#ifdef MD_STARTFILE_PREFIX
      add_prefix (&startfile_prefixes, md_startfile_prefix, "GCC",
		  0, 0, NULL_PTR);
#endif

#ifdef MD_STARTFILE_PREFIX_1
      add_prefix (&startfile_prefixes, md_startfile_prefix_1, "GCC",
		  0, 0, NULL_PTR);
#endif

      /* If standard_startfile_prefix is relative, base it on
	 standard_exec_prefix.  This lets us move the installed tree
	 as a unit.  If GCC_EXEC_PREFIX is defined, base
	 standard_startfile_prefix on that as well.  */
      if (IS_DIR_SEPARATOR (*standard_startfile_prefix)
	    || *standard_startfile_prefix == '$'
#ifdef HAVE_DOS_BASED_FILESYSTEM
  	    /* Check for disk name on MS-DOS-based systems.  */
          || (standard_startfile_prefix[1] == ':'
	      && (IS_DIR_SEPARATOR (standard_startfile_prefix[2])))
#endif
	  )
	add_prefix (&startfile_prefixes, standard_startfile_prefix, "BINUTILS",
		    0, 0, NULL_PTR);
      else
	{
	  if (gcc_exec_prefix)
	    add_prefix (&startfile_prefixes,
			concat (gcc_exec_prefix, machine_suffix,
				standard_startfile_prefix, NULL_PTR),
			NULL_PTR, 0, 0, NULL_PTR);
	  add_prefix (&startfile_prefixes,
		      concat (standard_exec_prefix,
			      machine_suffix,
			      standard_startfile_prefix, NULL_PTR),
		      NULL_PTR, 0, 0, NULL_PTR);
	}		       

#ifndef FREEBSD_NATIVE
      add_prefix (&startfile_prefixes, standard_startfile_prefix_1,
		  "BINUTILS", 0, 0, NULL_PTR);
      add_prefix (&startfile_prefixes, standard_startfile_prefix_2,
		  "BINUTILS", 0, 0, NULL_PTR);
#endif /* FREEBSD_NATIVE */
#if 0 /* Can cause surprises, and one can use -B./ instead.  */
      add_prefix (&startfile_prefixes, "./", NULL_PTR, 0, 1, NULL_PTR);
#endif
    }
  else
    {
      if (!IS_DIR_SEPARATOR (*standard_startfile_prefix) && gcc_exec_prefix)
	add_prefix (&startfile_prefixes,
		    concat (gcc_exec_prefix, machine_suffix,
			    standard_startfile_prefix, NULL_PTR),
		    "BINUTILS", 0, 0, NULL_PTR);
#ifdef CROSS_STARTFILE_PREFIX
	add_prefix (&startfile_prefixes, CROSS_STARTFILE_PREFIX, "BINUTILS",
		    0, 0, NULL_PTR);
#endif
    }

  /* Process any user specified specs in the order given on the command
     line.  */
  for (uptr = user_specs_head; uptr; uptr = uptr->next)
    {
      char *filename = find_a_file (&startfile_prefixes, uptr->filename, R_OK);
      read_specs (filename ? filename : uptr->filename, FALSE);
    }

  /* If we have a GCC_EXEC_PREFIX envvar, modify it for cpp's sake.  */
  if (gcc_exec_prefix)
    {
      char * temp = (char *) xmalloc (strlen (gcc_exec_prefix)
				      + strlen (spec_version)
				      + strlen (spec_machine) + 3);
      strcpy (temp, gcc_exec_prefix);
      strcat (temp, spec_machine);
      strcat (temp, dir_separator_str);
      strcat (temp, spec_version);
      strcat (temp, dir_separator_str);
      gcc_exec_prefix = temp;
    }

  /* Now we have the specs.
     Set the `valid' bits for switches that match anything in any spec.  */

  validate_all_switches ();

  /* Now that we have the switches and the specs, set
     the subdirectory based on the options.  */
  set_multilib_dir ();

  /* Warn about any switches that no pass was interested in.  */

  for (i = 0; (int)i < n_switches; i++)
    if (! switches[i].validated)
      error ("unrecognized option `-%s'", switches[i].part1);

  /* Obey some of the options.  */

  if (print_search_dirs)
    {
      printf ("install: %s%s\n", standard_exec_prefix, machine_suffix);
      printf ("programs: %s\n", build_search_list (&exec_prefixes, "", 0));
      printf ("libraries: %s\n", build_search_list (&startfile_prefixes, "", 0));
      exit (0);
    }

  if (print_file_name)
    {
      printf ("%s\n", find_file (print_file_name));
      exit (0);
    }

  if (print_prog_name)
    {
      char *newname = find_a_file (&exec_prefixes, print_prog_name, X_OK);
      printf ("%s\n", (newname ? newname : print_prog_name));
      exit (0);
    }

  if (print_multi_lib)
    {
      print_multilib_info ();
      exit (0);
    }

  if (print_multi_directory)
    {
      if (multilib_dir == NULL)
	printf (".\n");
      else
	printf ("%s\n", multilib_dir);
      exit (0);
    }

  if (print_help_list)
    {
      display_help ();

      if (! verbose_flag)
	{
	  printf ("\nFor bug reporting instructions, please see:\n");
	  printf ("%s.\n", GCCBUGURL);
	  
	  exit (0);
	}

      /* We do not exit here.  Instead we have created a fake input file
	 called 'help-dummy' which needs to be compiled, and we pass this
	 on the the various sub-processes, along with the --help switch.  */
    }
  
  if (verbose_flag)
    {
      int n;

      /* compiler_version is truncated at the first space when initialized
	 from version string, so truncate version_string at the first space
	 before comparing.  */
      for (n = 0; version_string[n]; n++)
	if (version_string[n] == ' ')
	  break;

      if (! strncmp (version_string, compiler_version, n)
	  && compiler_version[n] == 0)
	notice ("gcc version %s\n", version_string);
      else
	notice ("gcc driver version %s executing gcc version %s\n",
		version_string, compiler_version);

      if (n_infiles == 0)
	exit (0);
    }

  if (n_infiles == added_libraries)
    fatal ("No input files specified");

  /* Make a place to record the compiler output file names
     that correspond to the input files.  */

  i = n_infiles;
  i += lang_specific_extra_outfiles;
  outfiles = (const char **) xmalloc (i * sizeof (char *));
  bzero ((char *) outfiles, i * sizeof (char *));

  /* Record which files were specified explicitly as link input.  */

  explicit_link_files = xmalloc (n_infiles);
  bzero (explicit_link_files, n_infiles);

  for (i = 0; (int)i < n_infiles; i++)
    {
      register struct compiler *cp = 0;
      int this_file_error = 0;

      /* Tell do_spec what to substitute for %i.  */

      input_filename = infiles[i].name;
      input_filename_length = strlen (input_filename);
      input_file_number = i;

      /* Use the same thing in %o, unless cp->spec says otherwise.  */

      outfiles[i] = input_filename;

      /* Figure out which compiler from the file's suffix.  */

      cp = lookup_compiler (infiles[i].name, input_filename_length,
			    infiles[i].language);

      if (cp)
	{
	  /* Ok, we found an applicable compiler.  Run its spec.  */
	  /* First say how much of input_filename to substitute for %b  */
	  register const char *p;
	  int len;

	  if (cp->spec[0][0] == '#')
	    {
	      error ("%s: %s compiler not installed on this system",
		     input_filename, &cp->spec[0][1]);
	      this_file_error = 1;
	    }
	  else
	    {
	      input_basename = input_filename;
	      for (p = input_filename; *p; p++)
		if (IS_DIR_SEPARATOR (*p))
		  input_basename = p + 1;

	      /* Find a suffix starting with the last period,
		 and set basename_length to exclude that suffix.  */
	      basename_length = strlen (input_basename);
	      p = input_basename + basename_length;
	      while (p != input_basename && *p != '.') --p;
	      if (*p == '.' && p != input_basename)
		{
		  basename_length = p - input_basename;
		  input_suffix = p + 1;
		}
	      else
		input_suffix = "";

	      len = 0;
	      for (j = 0; j < sizeof cp->spec / sizeof cp->spec[0]; j++)
		if (cp->spec[j])
		  len += strlen (cp->spec[j]);

	      {
		char *p1 = (char *) xmalloc (len + 1);
	    
		len = 0;
		for (j = 0; j < sizeof cp->spec / sizeof cp->spec[0]; j++)
		  if (cp->spec[j])
		    {
		      strcpy (p1 + len, cp->spec[j]);
		      len += strlen (cp->spec[j]);
		    }
	    
		value = do_spec (p1);
		free (p1);
	      }
	      if (value < 0)
		this_file_error = 1;
	    }
	}

      /* If this file's name does not contain a recognized suffix,
	 record it as explicit linker input.  */

      else
	explicit_link_files[i] = 1;

      /* Clear the delete-on-failure queue, deleting the files in it
	 if this compilation failed.  */

      if (this_file_error)
	{
	  delete_failure_queue ();
	  error_count++;
	}
      /* If this compilation succeeded, don't delete those files later.  */
      clear_failure_queue ();
    }

  if (error_count == 0)
    {
      /* Make sure INPUT_FILE_NUMBER points to first available open
	 slot.  */
      input_file_number = n_infiles;
      if (lang_specific_pre_link ())
	error_count++;
    }

  /* Run ld to link all the compiler output files.  */

  if (error_count == 0)
    {
      int tmp = execution_count;

      /* We'll use ld if we can't find collect2. */
      if (! strcmp (linker_name_spec, "collect2"))
	{
	  char *s = find_a_file (&exec_prefixes, "collect2", X_OK);
	  if (s == NULL)
	    linker_name_spec = "ld";
	}
      /* Rebuild the COMPILER_PATH and LIBRARY_PATH environment variables
	 for collect.  */
      putenv_from_prefixes (&exec_prefixes, "COMPILER_PATH=");
      putenv_from_prefixes (&startfile_prefixes, "LIBRARY_PATH=");

      value = do_spec (link_command_spec);
      if (value < 0)
	error_count = 1;
      linker_was_run = (tmp != execution_count);
    }

  /* Warn if a -B option was specified but the prefix was never used.  */
  unused_prefix_warnings (&exec_prefixes);
  unused_prefix_warnings (&startfile_prefixes);

  /* If options said don't run linker,
     complain about input files to be given to the linker.  */

  if (! linker_was_run && error_count == 0)
    for (i = 0; (int)i < n_infiles; i++)
      if (explicit_link_files[i])
	error ("%s: linker input file unused since linking not done",
	       outfiles[i]);

  /* Delete some or all of the temporary files we made.  */

  if (error_count)
    delete_failure_queue ();
  delete_temp_files ();

  if (print_help_list)
    {
      printf ("\nFor bug reporting instructions, please see:\n");
      printf ("%s\n", GCCBUGURL);
    }
  
  exit (error_count > 0 ? (signal_count ? 2 : 1) : 0);
  /* NOTREACHED */
  return 0;
}

/* Find the proper compilation spec for the file name NAME,
   whose length is LENGTH.  LANGUAGE is the specified language,
   or 0 if this file is to be passed to the linker.  */

static struct compiler *
lookup_compiler (name, length, language)
     const char *name;
     size_t length;
     const char *language;
{
  struct compiler *cp;

  /* If this was specified by the user to be a linker input, indicate that. */
  if (language != 0 && language[0] == '*')
    return 0;

  /* Otherwise, look for the language, if one is spec'd.  */
  if (language != 0)
    {
      for (cp = compilers + n_compilers - 1; cp >= compilers; cp--)
	if (cp->suffix[0] == '@' && !strcmp (cp->suffix + 1, language))
	  return cp;

      error ("language %s not recognized", language);
      return 0;
    }

  /* Look for a suffix.  */
  for (cp = compilers + n_compilers - 1; cp >= compilers; cp--)
    {
      if (/* The suffix `-' matches only the file name `-'.  */
	  (!strcmp (cp->suffix, "-") && !strcmp (name, "-"))
	  || (strlen (cp->suffix) < length
	      /* See if the suffix matches the end of NAME.  */
#ifdef OS2
	      && ((!strcmp (cp->suffix,
			   name + length - strlen (cp->suffix))
		   || !strpbrk (cp->suffix, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"))
		  && !strcasecmp (cp->suffix,
				  name + length - strlen (cp->suffix)))
#else
	      && !strcmp (cp->suffix,
			  name + length - strlen (cp->suffix))
#endif
	 ))
	{
	  if (cp->spec[0][0] == '@')
	    {
	      struct compiler *new;

	      /* An alias entry maps a suffix to a language.
		 Search for the language; pass 0 for NAME and LENGTH
		 to avoid infinite recursion if language not found.
		 Construct the new compiler spec.  */
	      language = cp->spec[0] + 1;
	      new = (struct compiler *) xmalloc (sizeof (struct compiler));
	      new->suffix = cp->suffix;
	      bcopy ((char *) lookup_compiler (NULL_PTR, 0, language)->spec,
		     (char *) new->spec, sizeof new->spec);
	      return new;
	    }

	  /* A non-alias entry: return it.  */
	  return cp;
	}
    }

  return 0;
}

PTR
xmalloc (size)
  size_t size;
{
  register PTR value = (PTR) malloc (size);
  if (value == 0)
    fatal ("virtual memory exhausted");
  return value;
}

PTR
xrealloc (old, size)
  PTR old;
  size_t size;
{
  register PTR ptr;
  if (old)
    ptr = (PTR) realloc (old, size);
  else
    ptr = (PTR) malloc (size);
  if (ptr == 0)
    fatal ("virtual memory exhausted");
  return ptr;
}

static char *
save_string (s, len)
  const char *s;
  int len;
{
  register char *result = xmalloc (len + 1);

  bcopy (s, result, len);
  result[len] = 0;
  return result;
}

static void
pfatal_with_name (name)
     const char *name;
{
  perror_with_name (name);
  delete_temp_files ();
  exit (1);
}

static void
perror_with_name (name)
     const char *name;
{
  error ("%s: %s", name, xstrerror (errno));
}

static void
pfatal_pexecute (errmsg_fmt, errmsg_arg)
     const char *errmsg_fmt;
     const char *errmsg_arg;
{
  if (errmsg_arg)
    {
      int save_errno = errno;

      /* Space for trailing '\0' is in %s.  */
      char *msg = xmalloc (strlen (errmsg_fmt) + strlen (errmsg_arg));
      sprintf (msg, errmsg_fmt, errmsg_arg);
      errmsg_fmt = msg;

      errno = save_errno;
    }

  pfatal_with_name (errmsg_fmt);
}

/* More 'friendly' abort that prints the line and file.
   config.h can #define abort fancy_abort if you like that sort of thing.  */

void
fancy_abort ()
{
  fatal ("Internal gcc abort.");
}

/* Output an error message and exit */

void
fatal VPROTO((const char *msgid, ...))
{
#ifndef ANSI_PROTOTYPES
  const char *msgid;
#endif
  va_list ap;

  VA_START (ap, msgid);

#ifndef ANSI_PROTOTYPES
  msgid = va_arg (ap, const char *);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, _(msgid), ap);
  va_end (ap);
  fprintf (stderr, "\n");
  delete_temp_files ();
  exit (1);
}

static void
error VPROTO((const char *msgid, ...))
{
#ifndef ANSI_PROTOTYPES
  const char *msgid;
#endif
  va_list ap;

  VA_START (ap, msgid);

#ifndef ANSI_PROTOTYPES
  msgid = va_arg (ap, const char *);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, _(msgid), ap);
  va_end (ap);

  fprintf (stderr, "\n");
}

static void
notice VPROTO((const char *msgid, ...))
{
#ifndef ANSI_PROTOTYPES
  const char *msgid;
#endif
  va_list ap;

  VA_START (ap, msgid);

#ifndef ANSI_PROTOTYPES
  msgid = va_arg (ap, const char *);
#endif

  vfprintf (stderr, _(msgid), ap);
  va_end (ap);
}


static void
validate_all_switches ()
{
  struct compiler *comp;
  register const char *p;
  register char c;
  struct spec_list *spec;

  for (comp = compilers; comp->spec[0]; comp++)
    {
      size_t i;
      for (i = 0; i < sizeof comp->spec / sizeof comp->spec[0] && comp->spec[i]; i++)
	{
	  p = comp->spec[i];
	  while ((c = *p++))
	    if (c == '%' && *p == '{')
	      /* We have a switch spec.  */
	      validate_switches (p + 1);
	}
    }

  /* look through the linked list of specs read from the specs file */
  for (spec = specs; spec ; spec = spec->next)
    {
      p = *(spec->ptr_spec);
      while ((c = *p++))
	if (c == '%' && *p == '{')
	  /* We have a switch spec.  */
	  validate_switches (p + 1);
    }

  p = link_command_spec;
  while ((c = *p++))
    if (c == '%' && *p == '{')
      /* We have a switch spec.  */
      validate_switches (p + 1);
}

/* Look at the switch-name that comes after START
   and mark as valid all supplied switches that match it.  */

static void
validate_switches (start)
     const char *start;
{
  register const char *p = start;
  const char *filter;
  register int i;
  int suffix = 0;

  if (*p == '|')
    ++p;

  if (*p == '!')
    ++p;

  if (*p == '.')
    suffix = 1, ++p;

  filter = p;
  while (*p != ':' && *p != '}') p++;

  if (suffix)
    ;
  else if (p[-1] == '*')
    {
      /* Mark all matching switches as valid.  */
      --p;
      for (i = 0; i < n_switches; i++)
	if (!strncmp (switches[i].part1, filter, p - filter))
	  switches[i].validated = 1;
    }
  else
    {
      /* Mark an exact matching switch as valid.  */
      for (i = 0; i < n_switches; i++)
	{
	  if (!strncmp (switches[i].part1, filter, p - filter)
	      && switches[i].part1[p - filter] == 0)
	    switches[i].validated = 1;
	}
    }
}

/* Check whether a particular argument was used.  The first time we
   canonicalize the switches to keep only the ones we care about.  */

static int
used_arg (p, len)
     const char *p;
     int len;
{
  struct mswitchstr {
    char *str;
    char *replace;
    int len;
    int rep_len;
  };

  static struct mswitchstr *mswitches;
  static int n_mswitches;
  int i, j;

  if (!mswitches)
    {
      struct mswitchstr *matches;
      char *q;
      int cnt = 0;

      /* Break multilib_matches into the component strings of string and replacement
         string */
      for (q = multilib_matches; *q != '\0'; q++)
	if (*q == ';')
	  cnt++;

      matches = (struct mswitchstr *) alloca ((sizeof (struct mswitchstr)) * cnt);
      i = 0;
      q = multilib_matches;
      while (*q != '\0')
	{
	  matches[i].str = q;
	  while (*q != ' ')
	    {
	      if (*q == '\0')
		abort ();
	      q++;
	    }
	  *q = '\0';
	  matches[i].len = q - matches[i].str;

	  matches[i].replace = ++q;
	  while (*q != ';' && *q != '\0')
	    {
	      if (*q == ' ')
		abort ();
	      q++;
	    }
	  matches[i].rep_len = q - matches[i].replace;
	  i++;
	  if (*q == ';')
	    *q++ = '\0';
	  else
	    break;
	}

      /* Now build a list of the replacement string for switches that we care
	 about.  Make sure we allocate at least one entry.  This prevents
	 xmalloc from calling fatal, and prevents us from re-executing this
	 block of code.  */
      mswitches
	= (struct mswitchstr *) xmalloc ((sizeof (struct mswitchstr))
					 * (n_switches ? n_switches : 1));
      for (i = 0; i < n_switches; i++)
	{
	  int xlen = strlen (switches[i].part1);
	  for (j = 0; j < cnt; j++)
	    if (xlen == matches[j].len && ! strcmp (switches[i].part1, matches[j].str))
	      {
		mswitches[n_mswitches].str = matches[j].replace;
		mswitches[n_mswitches].len = matches[j].rep_len;
		mswitches[n_mswitches].replace = (char *)0;
		mswitches[n_mswitches].rep_len = 0;
		n_mswitches++;
		break;
	      }
	}
    }

  for (i = 0; i < n_mswitches; i++)
    if (len == mswitches[i].len && ! strncmp (p, mswitches[i].str, len))
      return 1;

  return 0;
}

static int
default_arg (p, len)
     const char *p;
     int len;
{
  char *start, *end;

  for (start = multilib_defaults; *start != '\0'; start = end+1)
    {
      while (*start == ' ' || *start == '\t')
	start++;

      if (*start == '\0')
	break;

      for (end = start+1; *end != ' ' && *end != '\t' && *end != '\0'; end++)
	;

      if ((end - start) == len && strncmp (p, start, len) == 0)
	return 1;

      if (*end == '\0')
	break;
    }

  return 0;
}

/* Work out the subdirectory to use based on the
   options.  The format of multilib_select is a list of elements.
   Each element is a subdirectory name followed by a list of options
   followed by a semicolon.  gcc will consider each line in turn.  If
   none of the options beginning with an exclamation point are
   present, and all of the other options are present, that
   subdirectory will be used.  */

static void
set_multilib_dir ()
{
  char *p = multilib_select;
  int this_path_len;
  char *this_path, *this_arg;
  int not_arg;
  int ok;

  while (*p != '\0')
    {
      /* Ignore newlines.  */
      if (*p == '\n')
	{
	  ++p;
	  continue;
	}

      /* Get the initial path.  */
      this_path = p;
      while (*p != ' ')
	{
	  if (*p == '\0')
	    abort ();
	  ++p;
	}
      this_path_len = p - this_path;

      /* Check the arguments.  */
      ok = 1;
      ++p;
      while (*p != ';')
	{
	  if (*p == '\0')
	    abort ();

	  if (! ok)
	    {
	      ++p;
	      continue;
	    }

	  this_arg = p;
	  while (*p != ' ' && *p != ';')
	    {
	      if (*p == '\0')
		abort ();
	      ++p;
	    }

	  if (*this_arg != '!')
	    not_arg = 0;
	  else
	    {
	      not_arg = 1;
	      ++this_arg;
	    }

	  /* If this is a default argument, we can just ignore it.
	     This is true even if this_arg begins with '!'.  Beginning
	     with '!' does not mean that this argument is necessarily
	     inappropriate for this library: it merely means that
	     there is a more specific library which uses this
	     argument.  If this argument is a default, we need not
	     consider that more specific library.  */
	  if (! default_arg (this_arg, p - this_arg))
	    {
	      ok = used_arg (this_arg, p - this_arg);
	      if (not_arg)
		ok = ! ok;
	    }

	  if (*p == ' ')
	    ++p;
	}

      if (ok)
	{
	  if (this_path_len != 1
	      || this_path[0] != '.')
	    {
	      char * new_multilib_dir = xmalloc (this_path_len + 1);
	      strncpy (new_multilib_dir, this_path, this_path_len);
	      new_multilib_dir[this_path_len] = '\0';
	      multilib_dir = new_multilib_dir;
	    }
	  break;
	}

      ++p;
    }      
}

/* Print out the multiple library subdirectory selection
   information.  This prints out a series of lines.  Each line looks
   like SUBDIRECTORY;@OPTION@OPTION, with as many options as is
   required.  Only the desired options are printed out, the negative
   matches.  The options are print without a leading dash.  There are
   no spaces to make it easy to use the information in the shell.
   Each subdirectory is printed only once.  This assumes the ordering
   generated by the genmultilib script.  */

static void
print_multilib_info ()
{
  char *p = multilib_select;
  char *last_path = 0, *this_path;
  int skip;
  int last_path_len = 0;

  while (*p != '\0')
    {
      /* Ignore newlines.  */
      if (*p == '\n')
	{
	  ++p;
	  continue;
	}

      /* Get the initial path.  */
      this_path = p;
      while (*p != ' ')
	{
	  if (*p == '\0')
	    abort ();
	  ++p;
	}

      /* If this is a duplicate, skip it.  */
      skip = (last_path != 0 && p - this_path == last_path_len
	      && ! strncmp (last_path, this_path, last_path_len));

      last_path = this_path;
      last_path_len = p - this_path;

      /* If this directory requires any default arguments, we can skip
	 it.  We will already have printed a directory identical to
	 this one which does not require that default argument.  */
      if (! skip)
	{
	  char *q;

	  q = p + 1;
	  while (*q != ';')
	    {
	      char *arg;

	      if (*q == '\0')
		abort ();

	      if (*q == '!')
		arg = NULL;
	      else
		arg = q;

	      while (*q != ' ' && *q != ';')
		{
		  if (*q == '\0')
		    abort ();
		  ++q;
		}

	      if (arg != NULL
		  && default_arg (arg, q - arg))
		{
		  skip = 1;
		  break;
		}

	      if (*q == ' ')
		++q;
	    }
	}

      if (! skip)
	{
	  char *p1;

	  for (p1 = last_path; p1 < p; p1++)
	    putchar (*p1);
	  putchar (';');
	}

      ++p;
      while (*p != ';')
	{
	  int use_arg;

	  if (*p == '\0')
	    abort ();

	  if (skip)
	    {
	      ++p;
	      continue;
	    }

	  use_arg = *p != '!';

	  if (use_arg)
	    putchar ('@');

	  while (*p != ' ' && *p != ';')
	    {
	      if (*p == '\0')
		abort ();
	      if (use_arg)
		putchar (*p);
	      ++p;
	    }

	  if (*p == ' ')
	    ++p;
	}

      if (! skip)
	{
	  /* If there are extra options, print them now */
	  if (multilib_extra && *multilib_extra)
	    {
	      int print_at = TRUE;
	      char *q;

	      for (q = multilib_extra; *q != '\0'; q++)
		{
		  if (*q == ' ')
		    print_at = TRUE;
		  else
		    {
		      if (print_at)
			putchar ('@');
		      putchar (*q);
		      print_at = FALSE;
		    }
		}
	    }
	  putchar ('\n');
	}

      ++p;
    }
}
