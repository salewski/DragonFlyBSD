/* $DragonFly: src/gnu/usr.bin/cc34/cc_prep/dragonfly-native.h,v 1.1 2004/06/14 22:27:53 joerg Exp $ */

/* auto-host.h.  Generated by configure.  */
/* config.in.  Generated from configure.ac by autoheader.  */

/* 1234 = LIL_ENDIAN, 4321 = BIGENDIAN */
#define BYTEORDER 1234

/* Define as the number of bits in a byte, if \`limits.h' doesn't. */
/* #undef CHAR_BIT */

/* Define 0/1 to force the choice for exception handling model. */
/* #undef CONFIG_SJLJ_EXCEPTIONS */

/* Define to enable the use of a default assembler. */
/* #undef DEFAULT_ASSEMBLER */

/* Define to enable the use of a default linker. */
/* #undef DEFAULT_LINKER */

/* Define if you want to use __cxa_atexit, rather than atexit, to register C++
   destructors for local statics and global objects. This is essential for
   fully standards-compliant handling of destructors, but requires
   __cxa_atexit in libc. */
#define DEFAULT_USE_CXA_ATEXIT 1

/* Define if you want more run-time sanity checks. This one gets a grab bag of
   miscellaneous but relatively cheap checks. */
/* #undef ENABLE_CHECKING */

/* Define if you want fold checked that it never destructs its argument. This
   is quite expensive. */
/* #undef ENABLE_FOLD_CHECKING */

/* Define if you want the garbage collector to operate in maximally paranoid
   mode, validating the entire heap and collecting garbage at every
   opportunity. This is extremely expensive. */
/* #undef ENABLE_GC_ALWAYS_COLLECT */

/* Define if you want the garbage collector to do object poisoning and other
   memory allocation checks. This is quite expensive. */
/* #undef ENABLE_GC_CHECKING */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define if you want all operations on RTL (the basic data structure of the
   optimizer and back end) to be checked for dynamic type safety at runtime.
   This is quite expensive. */
/* #undef ENABLE_RTL_CHECKING */

/* Define if you want RTL flag accesses to be checked against the RTL codes
   that are supported for each access macro. This is relatively cheap. */
/* #undef ENABLE_RTL_FLAG_CHECKING */

/* Define if you want all operations on trees (the basic data structure of the
   front ends) to be checked for dynamic type safety at runtime. This is
   moderately expensive. */
/* #undef ENABLE_TREE_CHECKING */

/* Define if you want to run subprograms and generated programs through
   valgrind (a memory checker). This is extremely expensive. */
/* #undef ENABLE_VALGRIND_CHECKING */

/* Define to 1 if installation paths should be looked up in Windows32
   Registry. Ignored on non windows32 hosts. */
/* #undef ENABLE_WIN32_REGISTRY */

/* Define to the name of a file containing a list of extra machine modes for
   this architecture. */
#define EXTRA_MODES_FILE "config/i386/i386-modes.def"

/* Define to enable detailed memory allocation stats gathering. */
/* #undef GATHER_STATISTICS */

/* Define to the type of elements in the array set by `getgroups'. Usually
   this is either `int' or `gid_t'. */
#define GETGROUPS_T gid_t

/* Define to 1 if you have the `alphasort' function. */
#define HAVE_ALPHASORT 1

/* Define if your assembler supports dwarf2 .file/.loc directives, and
   preserves file table indices exactly as given. */
#define HAVE_AS_DWARF2_DEBUG_LINE 1

/* Define if your assembler supports explicit relocations. */
/* #undef HAVE_AS_EXPLICIT_RELOCS */

/* Define if your assembler supports the --gdwarf2 option. */
#define HAVE_AS_GDWARF2_DEBUG_FLAG 1

/* Define true if the assembler supports '.long foo@GOTOFF'. */
#define HAVE_AS_GOTOFF_IN_DATA 1

/* Define if your assembler supports the --gstabs option. */
#define HAVE_AS_GSTABS_DEBUG_FLAG 1

/* Define if your assembler supports the Sun syntax for cmov. */
/* #undef HAVE_AS_IX86_CMOV_SUN_SYNTAX */

/* Define if your assembler supports .sleb128 and .uleb128. */
#define HAVE_AS_LEB128 1

/* Define if your assembler supports ltoffx and ldxmov relocations. */
/* #undef HAVE_AS_LTOFFX_LDXMOV_RELOCS */

/* Define if your assembler supports mfcr field. */
/* #undef HAVE_AS_MFCRF */

/* Define if your assembler supports the -no-mul-bug-abort option. */
/* #undef HAVE_AS_NO_MUL_BUG_ABORT_OPTION */

/* Define if your assembler supports offsetable %lo(). */
/* #undef HAVE_AS_OFFSETABLE_LO10 */

/* Define if your assembler supports .register. */
/* #undef HAVE_AS_REGISTER_PSEUDO_OP */

/* Define if your assembler supports -relax option. */
/* #undef HAVE_AS_RELAX_OPTION */

/* Define if your assembler and linker support unaligned PC relative relocs.
   */
/* #undef HAVE_AS_SPARC_UA_PCREL */

/* Define if your assembler and linker support unaligned PC relative relocs
   against hidden symbols. */
/* #undef HAVE_AS_SPARC_UA_PCREL_HIDDEN */

/* Define if your assembler supports thread-local storage. */
#define HAVE_AS_TLS 1

/* Define to 1 if you have the `atoll' function. */
/* #undef HAVE_ATOLL */

/* Define to 1 if you have the `atoq' function. */
/* #undef HAVE_ATOQ */

/* Define to 1 if you have the `clock' function. */
#define HAVE_CLOCK 1

/* Define if <time.h> defines clock_t. */
#define HAVE_CLOCK_T 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ABORT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ATOF 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ATOL 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_BASENAME 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_CALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_CLOCK 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_ERRNO 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FPRINTF_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FPUTS_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FREE 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_FWRITE_UNLOCKED 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETCWD 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETENV 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETOPT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETRLIMIT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETRUSAGE 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_GETWD 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_LDGETNAME 0

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_MALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_PUTC_UNLOCKED 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_REALLOC 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_SBRK 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_SETRLIMIT 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_SNPRINTF 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_STRSIGNAL 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_STRSTR 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_TIMES 1

/* Define to 1 if we found this declaration otherwise define to 0. */
#define HAVE_DECL_VASPRINTF 1

/* Define to 1 if you have the <direct.h> header file. */
/* #undef HAVE_DIRECT_H */

/* Define to 1 if you have the `dup2' function. */
#define HAVE_DUP2 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `fprintf_unlocked' function. */
/* #undef HAVE_FPRINTF_UNLOCKED */

/* Define to 1 if you have the `fputc_unlocked' function. */
/* #undef HAVE_FPUTC_UNLOCKED */

/* Define to 1 if you have the `fputs_unlocked' function. */
/* #undef HAVE_FPUTS_UNLOCKED */

/* Define to 1 if you have the `fwrite_unlocked' function. */
/* #undef HAVE_FWRITE_UNLOCKED */

/* Define if your assembler supports .balign and .p2align. */
#define HAVE_GAS_BALIGN_AND_P2ALIGN 1

/* Define if your assembler uses the new HImode fild and fist notation. */
#define HAVE_GAS_FILDS_FISTS 1

/* Define if your assembler and linker support .hidden. */
#define HAVE_GAS_HIDDEN 1

/* Define if your assembler supports specifying the maximum number of bytes to
   skip when using the GAS .p2align command. */
#define HAVE_GAS_MAX_SKIP_P2ALIGN 1

/* Define 0/1 if your assembler supports marking sections with SHF_MERGE flag.
   */
#define HAVE_GAS_SHF_MERGE 1

/* Define if your assembler supports .subsection and .subsection -1 starts
   emitting at the beginning of your section. */
#define HAVE_GAS_SUBSECTION_ORDERING 1

/* Define if your assembler supports .weak. */
#define HAVE_GAS_WEAK 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the iconv() function. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <iconv.h> header file. */
/* #undef HAVE_ICONV_H */

/* Define .init_array/.fini_array sections are available and working. */
/* #undef HAVE_INITFINI_ARRAY */

/* Define if you have a working <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `kill' function. */
#define HAVE_KILL 1

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if you have the <ldfcn.h> header file. */
/* #undef HAVE_LDFCN_H */

/* Define if your linker supports --as-needed and --no-as-needed options. */
/* #undef HAVE_LD_AS_NEEDED */

/* Define if your linker supports --eh-frame-hdr option. */
#define HAVE_LD_EH_FRAME_HDR 1

/* Define if your linker supports -pie option. */
/* #undef HAVE_LD_PIE */

/* Define if your linker links a mix of read-only and read-write sections into
   a read-write section. */
#define HAVE_LD_RO_RW_SECTION_MIXING 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define if your compiler supports the \`long long' type. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the `mbstowcs' function. */
#define HAVE_MBSTOWCS 1

/* Define if valgrind's memcheck.h header is installed. */
/* #undef HAVE_MEMCHECK_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mincore' function. */
#define HAVE_MINCORE 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define if mmap with MAP_ANON(YMOUS) works. */
#define HAVE_MMAP_ANON 1

/* Define if mmap of /dev/zero works. */
#define HAVE_MMAP_DEV_ZERO 1

/* Define if read-only mmap of a plain file works. */
#define HAVE_MMAP_FILE 1

/* Define to 1 if you have the `nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define if printf supports "%p". */
#define HAVE_PRINTF_PTR 1

/* Define to 1 if you have the `putc_unlocked' function. */
/* #undef HAVE_PUTC_UNLOCKED */

/* Define to 1 if you have the `scandir' function. */
#define HAVE_SCANDIR 1

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define if you have a working <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Define if <sys/times.h> defines struct tms. */
#define HAVE_STRUCT_TMS 1

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <sys/file.h> header file. */
#define HAVE_SYS_FILE_H 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/times.h> header file. */
#define HAVE_SYS_TIMES_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `times' function. */
#define HAVE_TIMES 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define if <sys/types.h> defines \`uchar'. */
/* #undef HAVE_UCHAR */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if valgrind's valgrind/memcheck.h header is installed. */
/* #undef HAVE_VALGRIND_MEMCHECK_H */

/* Define to 1 if you have the `vfork' function. */
#define HAVE_VFORK 1

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the `wcswidth' function. */
/* #undef HAVE_WCSWIDTH */

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define this macro if mbstowcs does not crash when its first argument is
   NULL. */
#define HAVE_WORKING_MBSTOWCS 1

/* Define to 1 if `vfork' works. */
#define HAVE_WORKING_VFORK 1

/* Define if the \`_Bool' type is built-in. */
/* #undef HAVE__BOOL */

/* Define if your compiler supports the \`__int64' type. */
/* #undef HAVE___INT64 */

/* Define if the host machine stores words of multi-word integers in
   big-endian order. */
/* #undef HOST_WORDS_BIG_ENDIAN */

/* Define as const if the declaration of iconv() needs const. */
/* #undef ICONV_CONST */

/* Define if host mkdir takes a single argument. */
/* #undef MKDIR_TAKES_ONE_ARG */

/* Define to 1 if HOST_WIDE_INT must be 64 bits wide (see hwint.h). */
/* #undef NEED_64BIT_HOST_WIDE_INT */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to PREFIX/include if cpp should also search that directory. */
/* #undef PREFIX_INCLUDE_DIR */

/* The number of bytes in type int */
#define SIZEOF_INT 4

/* The number of bytes in type long */
#define SIZEOF_LONG 4

/* The number of bytes in type long long */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in type short */
#define SIZEOF_SHORT 2

/* The number of bytes in type void * */
#define SIZEOF_VOID_P 4

/* The number of bytes in type __int64 */
/* #undef SIZEOF___INT64 */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if you can safely include both <string.h> and <strings.h>. */
#define STRING_WITH_STRINGS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define if your assembler mis-optimizes .eh_frame data. */
/* #undef USE_AS_TRADITIONAL_FORMAT */

/* Define if gcc should use -lunwind. */
/* #undef USE_LIBUNWIND_EXCEPTIONS */

/* Define to be the last portion of registry key on windows hosts. */
/* #undef WIN32_REGISTRY_KEY */

/* whether byteorder is bigendian */
/* #undef WORDS_BIGENDIAN */

/* Always define this when using the GNU C Library */
/* #undef _GNU_SOURCE */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
/* #undef inline */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to \`long' if <sys/resource.h> doesn't define. */
/* #undef rlim_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #undef uid_t */

/* Define as `fork' if `vfork' does not work. */
/* #undef vfork */
