#ifndef PAM_MACROS_H
#define PAM_MACROS_H

/*
 * All kind of macros used by PAM, but usable in some other
 * programs too.
 * Organized by Cristian Gafton <gafton@redhat.com>
 * $FreeBSD: src/contrib/libpam/libpam/include/security/_pam_macros.h,v 1.1.1.1.6.2 2001/06/11 15:28:14 markm Exp $
 * $DragonFly: src/contrib/libpam/libpam/include/security/Attic/_pam_macros.h,v 1.2 2003/06/17 04:24:03 dillon Exp $
 */

/* a 'safe' version of strdup */

extern char *strdup(const char *s);
#define  x_strdup(s)  ( (s) ? strdup(s):NULL )

/* Good policy to strike out passwords with some characters not just
   free the memory */

#define _pam_overwrite(x)        \
do {                             \
     register char *__xx__;      \
     if ((__xx__=(x)))           \
          while (*__xx__)        \
               *__xx__++ = '\0'; \
} while (0)

/*
 * Don't just free it, forget it too.
 */

#define _pam_drop(X) \
do {                 \
    if (X) {         \
        free(X);     \
        X=NULL;      \
    }                \
} while (0)

#define _pam_drop_reply(/* struct pam_response * */ reply, /* int */ replies) \
do {                                              \
    int reply_i;                                  \
                                                  \
    for (reply_i=0; reply_i<replies; ++reply_i) { \
	if (reply[reply_i].resp) {                \
	    _pam_overwrite(reply[reply_i].resp);  \
	    free(reply[reply_i].resp);            \
	}                                         \
    }                                             \
    if (reply)                                    \
	free(reply);                              \
} while (0)

/* some debugging code */

#ifdef DEBUG

/*
 * This provides the necessary function to do debugging in PAM.
 * Cristian Gafton <gafton@redhat.com>
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

/*
 * This is for debugging purposes ONLY. DO NOT use on live systems !!!
 * You have been warned :-) - CG
 *
 * to get automated debugging to the log file, it must be created manually.
 * _PAM_LOGFILE must exist, mode 666
 */

#ifndef _PAM_LOGFILE
#define _PAM_LOGFILE "/tmp/pam-debug.log"
#endif

static void _pam_output_debug_info(const char *file, const char *fn
				   , const int line)
{
    FILE *logfile;
    int must_close = 1;
    
    if (!(logfile = fopen(_PAM_LOGFILE,"a"))) {
        logfile = stderr;
        must_close = 0;
    }
    fprintf(logfile,"[%s:%s(%d)] ",file, fn, line);
    if (must_close) {
        fflush(logfile);
        fclose(logfile);
    }
}

static void _pam_output_debug(const char *format, ...)
{
    va_list args;
    FILE *logfile;
    int must_close = 1;
    
    va_start(args, format);

    if (!(logfile = fopen(_PAM_LOGFILE,"a"))) {
        logfile = stderr;
        must_close = 0;
    }
    vfprintf(logfile, format, args);
    fprintf(logfile, "\n");
    if (must_close) {
        fflush(logfile);
        fclose(logfile);
    }

    va_end(args);
}

#define D(x) do { \
    _pam_output_debug_info(__FILE__, __FUNCTION__, __LINE__); \
    _pam_output_debug x ; \
} while (0)

#define _pam_show_mem(X,XS) do {                                      \
      int i;                                                          \
      register unsigned char *x;                                      \
      x = (unsigned char *)X;                                         \
      fprintf(stderr, "  <start at %p>\n", X);                        \
      for (i = 0; i < XS ; ++x, ++i) {                                \
          fprintf(stderr, "    %02X. <%p:%02X>\n", i, x, *x);         \
      }                                                               \
      fprintf(stderr, "  <end for %p after %d bytes>\n", X, XS);      \
} while (0)

#define _pam_show_reply(/* struct pam_response * */reply, /* int */replies) \
do {                                                                        \
    int reply_i;                                                            \
    setbuf(stderr, NULL);                                                   \
    fprintf(stderr, "array at %p of size %d\n",reply,replies);              \
    fflush(stderr);                                                         \
    if (reply) {                                                            \
	for (reply_i = 0; reply_i < replies; reply_i++) {                   \
	    fprintf(stderr, "  elem# %d at %p: resp = %p, retcode = %d\n",  \
		    reply_i, reply+reply_i, reply[reply_i].resp,            \
		    reply[reply_i].resp, _retcode);                         \
	    fflush(stderr);                                                 \
	    if (reply[reply_i].resp) {                                      \
		fprintf(stderr, "    resp[%d] = '%s'\n",                    \
			strlen(reply[reply_i].resp), reply[reply_i].resp);  \
		fflush(stderr);                                             \
	    }                                                               \
	}                                                                   \
    }                                                                       \
    fprintf(stderr, "done here\n");                                         \
    fflush(stderr);                                                         \
} while (0)

#else

#define D(x)                             do { } while (0)
#define _pam_show_mem(X,XS)              do { } while (0)
#define _pam_show_reply(reply, replies)  do { } while (0)

#endif /* DEBUG */

#endif  /* PAM_MACROS_H */
