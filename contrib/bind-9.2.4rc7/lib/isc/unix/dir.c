/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dir.c,v 1.18.2.3 2004/03/09 06:12:09 marka Exp $ */

/* Principal Authors: DCL */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <isc/dir.h>
#include <isc/magic.h>
#include <isc/string.h>
#include <isc/util.h>

#include "errno2result.h"

#define ISC_DIR_MAGIC		ISC_MAGIC('D', 'I', 'R', '*')
#define VALID_DIR(dir)		ISC_MAGIC_VALID(dir, ISC_DIR_MAGIC)

void
isc_dir_init(isc_dir_t *dir) {
	REQUIRE(dir != NULL);

	dir->entry.name[0] = '\0';
	dir->entry.length = 0;

	dir->handle = NULL;

	dir->magic = ISC_DIR_MAGIC;
}

/*
 * Allocate workspace and open directory stream. If either one fails,
 * NULL will be returned.
 */
isc_result_t
isc_dir_open(isc_dir_t *dir, const char *dirname) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(VALID_DIR(dir));
	REQUIRE(dirname != NULL);

	/*
	 * Open stream.
	 */
	dir->handle = opendir(dirname);

	if (dir->handle == NULL)
		return isc__errno2result(errno);

	return (result);
}

/*
 * Return previously retrieved file or get next one.  Unix's dirent has
 * separate open and read functions, but the Win32 and DOS interfaces open
 * the dir stream and reads the first file in one operation.
 */
isc_result_t
isc_dir_read(isc_dir_t *dir) {
	struct dirent *entry;

	REQUIRE(VALID_DIR(dir) && dir->handle != NULL);

	/*
	 * Fetch next file in directory.
	 */
	entry = readdir(dir->handle);

	if (entry == NULL)
		return (ISC_R_NOMORE);

	/*
	 * Make sure that the space for the name is long enough.
	 */
	if (sizeof(dir->entry.name) <= strlen(entry->d_name))
	    return (ISC_R_UNEXPECTED);

	strcpy(dir->entry.name, entry->d_name);

	/*
	 * Some dirents have d_namlen, but it is not portable.
	 */
	dir->entry.length = strlen(entry->d_name);

	return (ISC_R_SUCCESS);
}

/*
 * Close directory stream.
 */
void
isc_dir_close(isc_dir_t *dir) {
       REQUIRE(VALID_DIR(dir) && dir->handle != NULL);

       (void)closedir(dir->handle);
       dir->handle = NULL;
}

/*
 * Reposition directory stream at start.
 */
isc_result_t
isc_dir_reset(isc_dir_t *dir) {
	REQUIRE(VALID_DIR(dir) && dir->handle != NULL);

	rewinddir(dir->handle);

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_dir_chdir(const char *dirname) {
	/*
	 * Change the current directory to 'dirname'.
	 */

	REQUIRE(dirname != NULL);

	if (chdir(dirname) < 0)
		return (isc__errno2result(errno));

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_dir_chroot(const char *dirname) {

	REQUIRE(dirname != NULL);

	if (chroot(dirname) < 0)
		return (isc__errno2result(errno));

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_dir_current(char *dirname, size_t length, isc_boolean_t end_sep) {
	char *cwd;
	isc_result_t result = ISC_R_SUCCESS;

	/*
	 * XXXDCL Could automatically allocate memory if dirname == NULL.
	 */
	REQUIRE(dirname != NULL);
	REQUIRE(length > 0U);

	cwd = getcwd(dirname, length);

	if (cwd == NULL) {
		if (errno == ERANGE)
			result = ISC_R_NOSPACE;
		else
			result = isc__errno2result(errno);
	} else if (end_sep) {
		if (strlen(dirname) + 1 == length)
			result = ISC_R_NOSPACE;
		else if (dirname[1] != '\0')
			strcat(dirname, "/");
	}

	return (result);
}

isc_result_t
isc_dir_createunique(char *templet) {
	isc_result_t result;
	char *x;
	char *p;
	int i;
	int pid;

	REQUIRE(templet != NULL);

	/*
	 * mkdtemp is not portable, so this emulates it.
	 */

	pid = getpid();

	/*
	 * Replace trailing Xs with the process-id, zero-filled.
	 */
	for (x = templet + strlen(templet) - 1; *x == 'X' && x >= templet;
	     x--, pid /= 10)
		*x = pid % 10 + '0';

	x++;			/* Set x to start of ex-Xs. */

	do {
		i = mkdir(templet, 0700);
		if (i == 0 || errno != EEXIST)
			break;

		/*
		 * The BSD algorithm.
		 */
		p = x;
		while (*p != '\0') {
			if (isdigit(*p & 0xff))
				*p = 'a';
			else if (*p != 'z')
				++*p;
			else {
				/*
				 * Reset character and move to next.
				 */
				*p++ = 'a';
				continue;
			}

			break;
		}

		if (*p == '\0') {
			/*
			 * Tried all combinations.  errno should already
			 * be EEXIST, but ensure it is anyway for
			 * isc__errno2result().
			 */
			errno = EEXIST;
			break;
		}
	} while (1);

	if (i == -1)
		result = isc__errno2result(errno);
	else
		result = ISC_R_SUCCESS;

	return (result);
}
