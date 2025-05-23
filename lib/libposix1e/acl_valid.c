/*-
 * Copyright (c) 1999 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/lib/libposix1e/acl_valid.c,v 1.4 2000/01/28 20:07:00 rwatson Exp $
 *	$DragonFly: src/lib/libposix1e/acl_valid.c,v 1.2 2003/06/17 04:26:51 dillon Exp $
 */
/*
 * acl_valid -- POSIX.1e ACL check routine
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/errno.h>

#include "acl_support.h"

/*
 * acl_valid: accepts an ACL, returns 0 on valid ACL, -1 for invalid,
 * and errno set to EINVAL.
 *
 * Implemented by calling the acl_check routine in acl_support, which
 * requires ordering.  We call acl_support's acl_sort to make this
 * true.  POSIX.1e allows acl_valid() to reorder the ACL as it sees fit.
 *
 * This call is deprecated, as it doesn't ask whether the ACL is valid
 * for a particular target.  However, this call is standardized, unlike
 * the other two forms.
 */ 
int
acl_valid(acl_t acl)
{
	int	error;

	acl_sort(acl);
	error = acl_check(acl);
	if (error) {
		errno = error;
		return (-1);
	} else {
		return (0);
	}
}


int
acl_valid_file_np(const char *pathp, acl_type_t type, acl_t acl)
{
	int	error;

	if (acl_posix1e(acl, type)) {
		error = acl_sort(acl);
		if (error) {
			errno = error;
			return (-1);
		}
	}

	return (__acl_aclcheck_file(pathp, type, acl));
}


int
acl_valid_fd_np(int fd, acl_type_t type, acl_t acl)
{
	int	error;

	if (acl_posix1e(acl, type)) {
		error = acl_sort(acl);
		if (error) {
			errno = error;
			return (-1);
		}
	}

	return (__acl_aclcheck_fd(fd, type, acl));
}
