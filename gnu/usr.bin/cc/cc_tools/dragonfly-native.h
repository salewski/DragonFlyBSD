/* $FreeBSD: src/gnu/usr.bin/cc/cc_tools/freebsd-native.h,v 1.5.2.9 2002/05/01 20:04:37 obrien Exp $ */
/* $DragonFly: src/gnu/usr.bin/cc/cc_tools/Attic/dragonfly-native.h,v 1.5 2004/04/07 14:02:41 joerg Exp $ */

/* FREEBSD_NATIVE is defined when gcc is integrated into the FreeBSD
   source tree so it can be configured appropriately without using
   the GNU configure/build mechanism. */

#define FREEBSD_NATIVE 1

#undef SYSTEM_INCLUDE_DIR		/* We don't need one for now. */
#undef GCC_INCLUDE_DIR			/* We don't need one for now. */
#undef TOOL_INCLUDE_DIR			/* We don't need one for now. */
#undef LOCAL_INCLUDE_DIR		/* We don't wish to support one. */

/* Look for the include files in the system-defined places.  */
#define GPLUSPLUS_INCLUDE_DIR		PREFIX2"/include/c++/2.95"
#define GPLUSPLUS_INCLUDE_DIR2		PREFIX2"/include/c++"
#define GCC_INCLUDE_DIR			PREFIX2"/include"
#ifdef CROSS_COMPILE
#define CROSS_INCLUDE_DIR		PREFIX2"/include"
#else
#define STANDARD_INCLUDE_DIR		PREFIX2"/include"
#endif

/* Under FreeBSD, the normal location of the compiler back ends is the
   /usr/libexec directory.

   ``cc --print-search-dirs'' gives:
   install: STANDARD_EXEC_PREFIX/(null)
   programs: /usr/libexec/<OBJFORMAT>/:STANDARD_EXEC_PREFIX:MD_EXEC_PREFIX
   libraries: MD_EXEC_PREFIX:MD_STARTFILE_PREFIX:STANDARD_STARTFILE_PREFIX
*/

#undef  TOOLDIR_BASE_PREFIX		/* Old??  This is not documented. */
#define STANDARD_EXEC_PREFIX		PREFIX1"/libexec/gcc2/"
#undef  MD_EXEC_PREFIX			/* We don't want one. */

/* Under FreeBSD, the normal location of the various *crt*.o files is the
   /usr/lib directory.  */

#define STANDARD_STARTFILE_PREFIX	PREFIX2"/lib/"
#ifdef CROSS_COMPILE
#define CROSS_STARTFILE_PREFIX		PREFIX2"/lib/gcc2/"
#endif
#define  MD_STARTFILE_PREFIX		PREFIX2"/lib/gcc2/"

/* For the native system compiler, we actually build libgcc in a profiled
   version.  So we should use it with -pg.  */
#define LIBGCC_SPEC		"%{!pg: -lgcc} %{pg: -lgcc_p}"
#define LIBSTDCXX_PROFILE	"-lstdc++_p"
#define MATH_LIBRARY_PROFILE	"-lm_p"

/* FreeBSD is 4.4BSD derived */
#define bsd4_4
