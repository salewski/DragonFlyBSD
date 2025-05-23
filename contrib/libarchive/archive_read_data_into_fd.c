/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_data_into_fd.c,v 1.7 2004/06/27 01:15:31 kientzle Exp $");

#include <sys/types.h>
#include <unistd.h>

#include "archive.h"
#include "archive_private.h"

/* Maximum amount of data to write at one time. */
#define	MAX_WRITE	(1024 * 1024)

/*
 * This implementation minimizes copying of data and is sparse-file aware.
 */
int
archive_read_data_into_fd(struct archive *a, int fd)
{
	int r;
	const void *buff;
	ssize_t size, bytes_to_write;
	ssize_t bytes_written, total_written;
	off_t offset;
	off_t output_offset;

	archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA);

	total_written = 0;
	output_offset = 0;

	while ((r = archive_read_data_block(a, &buff, &size, &offset)) ==
	    ARCHIVE_OK) {
		if (offset > output_offset) {
			lseek(fd, offset - output_offset, SEEK_CUR);
			output_offset = offset;
		}
		while (size > 0) {
			bytes_to_write = size;
			if (bytes_to_write > MAX_WRITE)
				bytes_to_write = MAX_WRITE;
			bytes_written = write(fd, buff, bytes_to_write);
			if (bytes_written < 0)
				return (-1);
			output_offset += bytes_written;
			total_written += bytes_written;
			size -= bytes_written;
			if (a->extract_progress != NULL)
				(*a->extract_progress)(a->extract_progress_user_data);
		}
	}

	if (r != ARCHIVE_EOF)
		return (r);
	return (ARCHIVE_OK);
}
