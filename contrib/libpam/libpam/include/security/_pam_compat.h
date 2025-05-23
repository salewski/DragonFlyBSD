#ifndef _PAM_COMPAT_H
#define _PAM_COMPAT_H

/*
 * This file was contributed by Derrick J Brashear <shadow@dementia.org>
 *
 * A number of operating systems have started to implement PAM.
 * unfortunately, they have a different set of numeric values for
 * certain constants.  This file is included for compatibility's sake.
 * $FreeBSD: src/contrib/libpam/libpam/include/security/_pam_compat.h,v 1.1.1.1.6.2 2001/06/11 15:28:14 markm Exp $
 * $DragonFly: src/contrib/libpam/libpam/include/security/Attic/_pam_compat.h,v 1.2 2003/06/17 04:24:03 dillon Exp $
 */

/* Solaris uses different constants. We redefine to those here */
#if defined(solaris) || (defined(__SVR4) && defined(sun))

/* generic for pam_* functions */
# undef PAM_SILENT
# define PAM_SILENT              0x80000000

/* flags for pam_chauthtok() */
# undef PAM_PRELIM_CHECK
# define PAM_PRELIM_CHECK        0x1

# undef PAM_UPDATE_AUTHTOK
# define PAM_UPDATE_AUTHTOK      0x2

/* flags for pam_setcred() */
# undef PAM_ESTABLISH_CRED
# define PAM_ESTABLISH_CRED      0x1

# undef PAM_DELETE_CRED
# define PAM_DELETE_CRED         0x2

# undef PAM_REINITIALIZE_CRED
# define PAM_REINITIALIZE_CRED   0x4

# define PAM_REFRESH_CRED        0x8
# undef PAM_REFRESH_CRED

/* another binary incompatibility comes from the return codes! */

# undef PAM_CONV_ERR
# define PAM_CONV_ERR            6

# undef PAM_PERM_DENIED
# define PAM_PERM_DENIED         7

# undef PAM_MAXTRIES
# define PAM_MAXTRIES            8

# undef PAM_AUTH_ERR
# define PAM_AUTH_ERR            9

# undef PAM_NEW_AUTHTOK_REQD
# define PAM_NEW_AUTHTOK_REQD    10

# undef PAM_CRED_INSUFFICIENT
# define PAM_CRED_INSUFFICIENT   11

# undef PAM_AUTHINFO_UNAVAIL
# define PAM_AUTHINFO_UNAVAIL    12

# undef PAM_USER_UNKNOWN
# define PAM_USER_UNKNOWN        13

# undef PAM_CRED_UNAVAIL
# define PAM_CRED_UNAVAIL        14

# undef PAM_CRED_EXPIRED
# define PAM_CRED_EXPIRED        15

# undef PAM_CRED_ERR
# define PAM_CRED_ERR            16

# undef PAM_ACCT_EXPIRED
# define PAM_ACCT_EXPIRED        17

# undef PAM_AUTHTOK_EXPIRED
# define PAM_AUTHTOK_EXPIRED     18

# undef PAM_SESSION_ERR
# define PAM_SESSION_ERR         19

# undef PAM_AUTHTOK_ERR
# define PAM_AUTHTOK_ERR           20

# undef PAM_AUTHTOK_RECOVERY_ERR
# define PAM_AUTHTOK_RECOVERY_ERR  21

# undef PAM_AUTHTOK_LOCK_BUSY
# define PAM_AUTHTOK_LOCK_BUSY     22

# undef PAM_AUTHTOK_DISABLE_AGING
# define PAM_AUTHTOK_DISABLE_AGING 23

# undef PAM_NO_MODULE_DATA
# define PAM_NO_MODULE_DATA      24

# undef PAM_IGNORE
# define PAM_IGNORE              25

# undef PAM_ABORT
# define PAM_ABORT               26

# undef PAM_TRY_AGAIN
# define PAM_TRY_AGAIN           27

#endif /* defined(solaris) || (defined(__SVR4) && defined(sun)) */

#endif /* _PAM_COMPAT_H */
