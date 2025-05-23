/* $DragonFly: src/lib/libbind/port_before.h,v 1.1 2004/05/27 18:15:40 dillon Exp $ */
#ifndef port_before_h
#define port_before_h
#include <config.h>

struct group;           /* silence warning */
struct passwd;          /* silence warning */
struct timeval;         /* silence warning */
struct timezone;        /* silence warning */

#ifdef HAVE_SYS_TIMERS_H
#include <sys/timers.h>
#endif
#include <limits.h>


#undef WANT_IRS_GR
#undef WANT_IRS_NIS
#undef WANT_IRS_PW

#undef BSD_COMP

#undef DO_PTHREADS
#define GETGROUPLIST_ARGS const char *name, int basegid, int *groups, int *ngroups
#define GETNETBYADDR_ADDR_T long
#define SETPWENT_VOID 1
#undef SETGRENT_VOID

#define NET_R_ARGS char *buf, int buflen
#define NET_R_BAD NULL
#define NET_R_COPY buf, buflen
#define NET_R_COPY_ARGS NET_R_ARGS
#define NET_R_END_RESULT(x) /*empty*/
#define NET_R_END_RETURN void
#undef NET_R_ENT_ARGS /*empty*/
#define NET_R_OK nptr
#define NET_R_RETURN struct netent *
#undef NET_R_SET_RESULT /*empty*/
#undef NET_R_SETANSWER
#define NET_R_SET_RETURN void
#undef NETENT_DATA

#define GROUP_R_RETURN struct group *
#define GROUP_R_SET_RETURN void
#undef GROUP_R_SET_RESULT /*empty*/
#define GROUP_R_END_RETURN void
#define GROUP_R_END_RESULT(x) /*empty*/
#define GROUP_R_ARGS char *buf, int buflen
#define GROUP_R_ENT_ARGS void
#define GROUP_R_OK gptr
#define GROUP_R_BAD NULL

#define HOST_R_ARGS char *buf, int buflen, int *h_errnop
#define HOST_R_BAD NULL
#define HOST_R_COPY buf, buflen
#define HOST_R_COPY_ARGS char *buf, int buflen
#define HOST_R_END_RESULT(x) /*empty*/
#define HOST_R_END_RETURN void
#undef HOST_R_ENT_ARGS /*empty*/
#define HOST_R_ERRNO *h_errnop = h_errno
#define HOST_R_OK hptr
#define HOST_R_RETURN struct hostent *
#undef HOST_R_SETANSWER
#undef HOST_R_SET_RESULT
#define HOST_R_SET_RETURN void
#undef HOSTENT_DATA

#define NGR_R_ARGS char *buf, int buflen
#define NGR_R_BAD (0)
#define NGR_R_COPY buf, buflen
#define NGR_R_COPY_ARGS NGR_R_ARGS
#define NGR_R_END_RESULT(x)  /*empty*/
#define NGR_R_END_RETURN void
#undef NGR_R_ENT_ARGS /*empty*/
#define NGR_R_OK 1
#define NGR_R_RETURN int
#undef NGR_R_SET_RESULT /*empty*/
#define NGR_R_SET_RETURN void


#define PROTO_R_ARGS char *buf, int buflen
#define PROTO_R_BAD NULL
#define PROTO_R_COPY buf, buflen
#define PROTO_R_COPY_ARGS PROTO_R_ARGS
#define PROTO_R_END_RESULT(x) /*empty*/
#define PROTO_R_END_RETURN void
#undef PROTO_R_ENT_ARGS /*empty*/
#define PROTO_R_OK pptr
#undef PROTO_R_SETANSWER
#define PROTO_R_RETURN struct protoent *
#undef PROTO_R_SET_RESULT
#define PROTO_R_SET_RETURN void

#define PASS_R_ARGS char *buf, int buflen
#define PASS_R_BAD NULL
#define PASS_R_COPY buf, buflen
#define PASS_R_COPY_ARGS PASS_R_ARGS
#define PASS_R_END_RESULT(x) /*empty*/
#define PASS_R_END_RETURN void
#undef PASS_R_ENT_ARGS
#define PASS_R_OK pwptr
#define PASS_R_RETURN struct passwd *
#undef PASS_R_SET_RESULT /*empty*/
#define PASS_R_SET_RETURN void

#define SERV_R_ARGS char *buf, int buflen
#define SERV_R_BAD NULL
#define SERV_R_COPY buf, buflen
#define SERV_R_COPY_ARGS SERV_R_ARGS
#define SERV_R_END_RESULT(x) /*empty*/
#define SERV_R_END_RETURN void 
#undef SERV_R_ENT_ARGS /*empty*/
#define SERV_R_OK sptr
#undef SERV_R_SETANSWER
#define SERV_R_RETURN struct servent *
#undef SERV_R_SET_RESULT
#define SERV_R_SET_RETURN void


#define DE_CONST(konst, var) \
        do { \
                union { const void *k; void *v; } _u; \
                _u.k = konst; \
                var = _u.v; \
        } while (0)

#define UNUSED(x) (x) = (x)

#undef NEED_SOLARIS_BITTYPES
#define ISC_SOCKLEN_T socklen_t

#ifdef __GNUC__
#define ISC_FORMAT_PRINTF(fmt, args) \
	__attribute__((__format__(__printf__, fmt, args)))
#else
#define ISC_FORMAT_PRINTF(fmt, args)
#endif

#endif
