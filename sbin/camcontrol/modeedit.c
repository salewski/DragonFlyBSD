/*-
 * Copyright (c) 2000 Kelly Yancey <kbyanc@posi.net>
 * Derived from work done by Julian Elischer <julian@tfs.com,
 * julian@dialix.oz.au>, 1993, and Peter Dufault <dufault@hda.com>, 1994.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution. 
 *    
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT  
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sbin/camcontrol/modeedit.c,v 1.5.2.2 2003/01/08 17:55:02 njl Exp $
 * $DragonFly: src/sbin/camcontrol/modeedit.c,v 1.3 2005/01/11 23:58:55 cpressey Exp $
 */

#include <sys/queue.h>
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <cam/scsi/scsi_all.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <camlib.h>
#include "camcontrol.h"

int verbose = 0;

#define	DEFAULT_SCSI_MODE_DB	"/usr/share/misc/scsi_modes"
#define	DEFAULT_EDITOR		"vi"
#define	MAX_FORMAT_SPEC		4096	/* Max CDB format specifier. */
#define	MAX_PAGENUM_LEN		10	/* Max characters in page num. */
#define	MAX_PAGENAME_LEN	64	/* Max characters in page name. */
#define	PAGEDEF_START		'{'	/* Page definition delimiter. */
#define	PAGEDEF_END		'}'	/* Page definition delimiter. */
#define	PAGENAME_START		'"'	/* Page name delimiter. */
#define	PAGENAME_END		'"'	/* Page name delimiter. */
#define	PAGEENTRY_END		';'	/* Page entry terminator (optional). */
#define	MAX_COMMAND_SIZE	255	/* Mode/Log sense data buffer size. */
#define PAGE_CTRL_SHIFT		6	/* Bit offset to page control field. */


/* Macros for working with mode pages. */
#define	MODE_PAGE_HEADER(mh)						\
	(struct scsi_mode_page_header *)find_mode_page_6(mh)

#define	MODE_PAGE_DATA(mph)						\
	(u_int8_t *)(mph) + sizeof(struct scsi_mode_page_header)


struct editentry {
	STAILQ_ENTRY(editentry) link;
	char	*name;
	char	type;
	int	editable;
	int	size;
	union {
		int	ivalue;
		char	*svalue;
	} value;
};
STAILQ_HEAD(, editentry) editlist;	/* List of page entries. */
int editlist_changed = 0;		/* Whether any entries were changed. */

struct pagename {
	SLIST_ENTRY(pagename) link;
	int pagenum;
	char *name;
};
SLIST_HEAD(, pagename) namelist;	/* Page number to name mappings. */

static char format[MAX_FORMAT_SPEC];	/* Buffer for scsi cdb format def. */

static FILE *edit_file = NULL;		/* File handle for edit file. */
static char edit_path[] = "/tmp/camXXXXXX";


/* Function prototypes. */
static void		 editentry_create(void *, int, void *, int, char *);
static void		 editentry_update(void *, int, void *, int, char *);
static int		 editentry_save(void *, char *);
static struct editentry	*editentry_lookup(char *);
static int		 editentry_set(char *, char *, int);
static void		 editlist_populate(struct cam_device *, int, int, int,
					   int, int);
static void		 editlist_save(struct cam_device *, int, int, int, int,
				       int);
static void		 nameentry_create(int, char *);
static struct pagename	*nameentry_lookup(int);
static int		 load_format(const char *, int);
static int		 modepage_write(FILE *, int);
static int		 modepage_read(FILE *);
static void		 modepage_edit(void);
static void		 modepage_dump(struct cam_device *, int, int, int, int,
				       int);
static void		 cleanup_editfile(void);


#define	returnerr(code) do {						\
	errno = code;							\
	return (-1);							\
} while (0)


#define	RTRIM(string) do {						\
	int _length;						\
	while (isspace(string[_length = strlen(string) - 1]))		\
		string[_length] = '\0';					\
} while (0)


static void
editentry_create(void *hook __unused, int letter, void *arg, int count,
		 char *name)
{
	struct editentry *newentry;	/* Buffer to hold new entry. */

	/* Allocate memory for the new entry and a copy of the entry name. */
	if ((newentry = malloc(sizeof(struct editentry))) == NULL ||
	    (newentry->name = strdup(name)) == NULL)
		err(EX_OSERR, NULL);

	/* Trim any trailing whitespace for the entry name. */
	RTRIM(newentry->name);

	newentry->editable = (arg != NULL);
	newentry->type = letter;
	newentry->size = count;		/* Placeholder; not accurate. */
	newentry->value.svalue = NULL;

	STAILQ_INSERT_TAIL(&editlist, newentry, link);
}

static void
editentry_update(void *hook __unused, int letter, void *arg, int count,
		 char *name)
{
	struct editentry *dest;		/* Buffer to hold entry to update. */

	dest = editentry_lookup(name);
	assert(dest != NULL);

	dest->type = letter;
	dest->size = count;		/* We get the real size now. */

	switch (dest->type) {
	case 'i':			/* Byte-sized integral type. */
	case 'b':			/* Bit-sized integral types. */
	case 't':
		dest->value.ivalue = (intptr_t)arg;
		break;

	case 'c':			/* Character array. */
	case 'z':			/* Null-padded string. */
		editentry_set(name, (char *)arg, 0);
		break;
	default:
		; /* NOTREACHED */
	}
}

static int
editentry_save(void *hook __unused, char *name)
{
	struct editentry *src;		/* Entry value to save. */

	src = editentry_lookup(name);
	assert(src != NULL);

	switch (src->type) {
	case 'i':			/* Byte-sized integral type. */
	case 'b':			/* Bit-sized integral types. */
	case 't':
		return (src->value.ivalue);
		/* NOTREACHED */

	case 'c':			/* Character array. */
	case 'z':			/* Null-padded string. */
		return ((intptr_t)src->value.svalue);
		/* NOTREACHED */

	default:
		; /* NOTREACHED */
	}

	return (0);			/* This should never happen. */
}

static struct editentry *
editentry_lookup(char *name)
{
	struct editentry *scan;

	assert(name != NULL);

	STAILQ_FOREACH(scan, &editlist, link) {
		if (strcasecmp(scan->name, name) == 0)
			return (scan);
	}

	/* Not found during list traversal. */
	return (NULL);
}

static int
editentry_set(char *name, char *newvalue, int editonly)
{
	struct editentry *dest;	/* Modepage entry to update. */
	char *cval;		/* Pointer to new string value. */
	char *convertend;	/* End-of-conversion pointer. */
	int ival;		/* New integral value. */
	int resolution;		/* Resolution in bits for integer conversion. */
	int resolution_max;	/* Maximum resolution for modepage's size. */

	assert(newvalue != NULL);
	if (*newvalue == '\0')
		return (0);	/* Nothing to do. */

	if ((dest = editentry_lookup(name)) == NULL)
		returnerr(ENOENT);
	if (!dest->editable && editonly)
		returnerr(EPERM);

	switch (dest->type) {
	case 'i':		/* Byte-sized integral type. */
	case 'b':		/* Bit-sized integral types. */
	case 't':
		/* Convert the value string to an integer. */
		resolution = (dest->type == 'i')? 8: 1;
		ival = (int)strtol(newvalue, &convertend, 0);
		if (*convertend != '\0')
			returnerr(EINVAL);

		/*
		 * Determine the maximum value of the given size for the
		 * current resolution.
		 * XXX Lovely x86's optimize out the case of shifting by 32,
		 * and gcc doesn't currently workaround it (even for int64's),
		 * so we have to kludge it.
		 */
		if (resolution * dest->size == 32)
			resolution_max = 0xffffffff;
		else
			resolution_max = (1 << (resolution * dest->size)) - 1;

		if (ival > resolution_max || ival < 0) {
			int newival = (ival < 0) ? 0 : resolution_max;
			warnx("value %d is out of range for entry %s; clipping "
			    "to %d", ival, name, newival);
			ival = newival;
		}
		if (dest->value.ivalue != ival)
			editlist_changed = 1;
		dest->value.ivalue = ival;
		break;

	case 'c':		/* Character array. */
	case 'z':		/* Null-padded string. */
		if ((cval = malloc(dest->size + 1)) == NULL)
			err(EX_OSERR, NULL);
		bzero(cval, dest->size + 1);
		strncpy(cval, newvalue, dest->size);
		if (dest->type == 'z') {
			/* Convert trailing spaces to nulls. */
			char *conv_end;

			for (conv_end = cval + dest->size;
			    conv_end >= cval; conv_end--) {
				if (*conv_end == ' ')
					*conv_end = '\0';
				else if (*conv_end != '\0')
					break;
			}
		}
		if (strncmp(dest->value.svalue, cval, dest->size) == 0) {
			/* Nothing changed, free the newly allocated string. */
			free(cval);
			break;
		}
		if (dest->value.svalue != NULL) {
			/* Free the current string buffer. */
			free(dest->value.svalue);
			dest->value.svalue = NULL;
		}
		dest->value.svalue = cval;
		editlist_changed = 1;
		break;

	default:
		; /* NOTREACHED */
	}

	return (0);
}

static void
nameentry_create(int pagenum, char *name) {
	struct pagename *newentry;

	if (pagenum < 0 || name == NULL || name[0] == '\0')
		return;

	/* Allocate memory for the new entry and a copy of the entry name. */
	if ((newentry = malloc(sizeof(struct pagename))) == NULL ||
	    (newentry->name = strdup(name)) == NULL)
		err(EX_OSERR, NULL);

	/* Trim any trailing whitespace for the page name. */
	RTRIM(newentry->name);

	newentry->pagenum = pagenum;
	SLIST_INSERT_HEAD(&namelist, newentry, link);
}

static struct pagename *
nameentry_lookup(int pagenum) {
	struct pagename *scan;

	SLIST_FOREACH(scan, &namelist, link) {
		if (pagenum == scan->pagenum)
			return (scan);
	}

	/* Not found during list traversal. */
	return (NULL);
}

static int
load_format(const char *pagedb_path, int page)
{
	FILE *pagedb;
	char str_pagenum[MAX_PAGENUM_LEN];
	char str_pagename[MAX_PAGENAME_LEN];
	int pagenum;
	int depth;			/* Quoting depth. */
	int found;
	int lineno;
	enum { LOCATE, PAGENAME, PAGEDEF } state;
	char c;

#define	SETSTATE_LOCATE do {						\
	str_pagenum[0] = '\0';						\
	str_pagename[0] = '\0';						\
	pagenum = -1;							\
	state = LOCATE;							\
} while (0)

#define	SETSTATE_PAGENAME do {						\
	str_pagename[0] = '\0';						\
	state = PAGENAME;						\
} while (0)

#define	SETSTATE_PAGEDEF do {						\
	format[0] = '\0';						\
	state = PAGEDEF;						\
} while (0)

#define	UPDATE_LINENO do {						\
	if (c == '\n')							\
		lineno++;						\
} while (0)

#define	BUFFERFULL(buffer)	(strlen(buffer) + 1 >= sizeof(buffer))

	if ((pagedb = fopen(pagedb_path, "r")) == NULL)
		returnerr(ENOENT);

	SLIST_INIT(&namelist);

	depth = 0;
	lineno = 0;
	found = 0;
	SETSTATE_LOCATE;
	while ((c = fgetc(pagedb)) != EOF) {

		/* Keep a line count to make error messages more useful. */
		UPDATE_LINENO;

		/* Skip over comments anywhere in the mode database. */
		if (c == '#') {
			do {
				c = fgetc(pagedb);
			} while (c != '\n' && c != EOF);
			UPDATE_LINENO;
			continue;
		}

		/* Strip out newline characters. */
		if (c == '\n')
			continue;

		/* Keep track of the nesting depth for braces. */
		if (c == PAGEDEF_START)
			depth++;
		else if (c == PAGEDEF_END) {
			depth--;
			if (depth < 0) {
				errx(EX_OSFILE, "%s:%d: %s", pagedb_path,
				    lineno, "mismatched bracket");
			}
		}

		switch (state) {
		case LOCATE:
			/*
			 * Locate the page the user is interested in, skipping
			 * all others.
			 */
			if (isspace(c)) {
				/* Ignore all whitespace between pages. */
				break;
			} else if (depth == 0 && c == PAGEENTRY_END) {
				/*
				 * A page entry terminator will reset page
				 * scanning (useful for assigning names to
				 * modes without providing a mode definition).
				 */
				/* Record the name of this page. */
				pagenum = strtol(str_pagenum, NULL, 0);
				nameentry_create(pagenum, str_pagename);
				SETSTATE_LOCATE;
			} else if (depth == 0 && c == PAGENAME_START) {
				SETSTATE_PAGENAME;
			} else if (c == PAGEDEF_START) {
				pagenum = strtol(str_pagenum, NULL, 0);
				if (depth == 1) {
					/* Record the name of this page. */
					nameentry_create(pagenum, str_pagename);
					/*
					 * Only record the format if this is
					 * the page we are interested in.
					 */
					if (page == pagenum && !found)
						SETSTATE_PAGEDEF;
				}
			} else if (c == PAGEDEF_END) {
				/* Reset the processor state. */
				SETSTATE_LOCATE;
			} else if (depth == 0 && ! BUFFERFULL(str_pagenum)) {
				strncat(str_pagenum, &c, 1);
			} else if (depth == 0) {
				errx(EX_OSFILE, "%s:%d: %s %d %s", pagedb_path,
				    lineno, "page identifier exceeds",
				    sizeof(str_pagenum) - 1, "characters");
			}
			break;

		case PAGENAME:
			if (c == PAGENAME_END) {
				/*
				 * Return to LOCATE state without resetting the
				 * page number buffer.
				 */
				state = LOCATE;
			} else if (! BUFFERFULL(str_pagename)) {
				strncat(str_pagename, &c, 1);
			} else {
				errx(EX_OSFILE, "%s:%d: %s %d %s", pagedb_path,
				    lineno, "page name exceeds",
				    sizeof(str_pagenum) - 1, "characters");
			}
			break;

		case PAGEDEF:
			/*
			 * Transfer the page definition into a format buffer
			 * suitable for use with CDB encoding/decoding routines.
			 */
			if (depth == 0) {
				found = 1;
				SETSTATE_LOCATE;
			} else if (! BUFFERFULL(format)) {
				strncat(format, &c, 1);
			} else {
				errx(EX_OSFILE, "%s:%d: %s %d %s", pagedb_path,
				    lineno, "page definition exceeds",
				    sizeof(format) - 1, "characters");
			}
			break;

		default:
			; /* NOTREACHED */
		}

		/* Repeat processing loop with next character. */
	}

	if (ferror(pagedb))
		err(EX_OSFILE, "%s", pagedb_path);

	/* Close the SCSI page database. */
	fclose(pagedb);

	if (!found)			/* Never found a matching page. */
		returnerr(ESRCH);

	return (0);
}

static void
editlist_populate(struct cam_device *device, int modepage, int page_control,
		  int dbd, int retries, int timeout)
{
	u_int8_t data[MAX_COMMAND_SIZE];/* Buffer to hold sense data. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_header_6 *mh;	/* Location of mode header. */
	struct scsi_mode_page_header *mph;

	STAILQ_INIT(&editlist);

	/* Fetch changeable values; use to build initial editlist. */
	mode_sense(device, modepage, 1, dbd, retries, timeout, data,
		   sizeof(data));

	mh = (struct scsi_mode_header_6 *)data;
	mph = MODE_PAGE_HEADER(mh);
	mode_pars = MODE_PAGE_DATA(mph);

	/* Decode the value data, creating edit_entries for each value. */
	buff_decode_visit(mode_pars, mh->data_length, format,
	    editentry_create, 0);

	/* Fetch the current/saved values; use to set editentry values. */
	mode_sense(device, modepage, page_control, dbd, retries, timeout, data,
		   sizeof(data));
	buff_decode_visit(mode_pars, mh->data_length, format,
	    editentry_update, 0);
}

static void
editlist_save(struct cam_device *device, int modepage, int page_control,
	      int dbd, int retries, int timeout)
{
	u_int8_t data[MAX_COMMAND_SIZE];/* Buffer to hold sense data. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_header_6 *mh;	/* Location of mode header. */
	struct scsi_mode_page_header *mph;

	/* Make sure that something changed before continuing. */
	if (! editlist_changed)
		return;

	/*
	 * Preload the CDB buffer with the current mode page data.
	 * XXX If buff_encode_visit would return the number of bytes encoded
	 *     we *should* use that to build a header from scratch. As it is
	 *     now, we need mode_sense to find out the page length.
	 */
	mode_sense(device, modepage, page_control, dbd, retries, timeout, data,
		   sizeof(data));

	/* Initial headers & offsets. */
	mh = (struct scsi_mode_header_6 *)data;
	mph = MODE_PAGE_HEADER(mh);
	mode_pars = MODE_PAGE_DATA(mph);

	/* Encode the value data to be passed back to the device. */
	buff_encode_visit(mode_pars, mh->data_length, format,
	    editentry_save, 0);

	/* Eliminate block descriptors. */
	bcopy(mph, ((u_int8_t *)mh) + sizeof(*mh),
	    sizeof(*mph) + mph->page_length);

	/* Recalculate headers & offsets. */
	mh->blk_desc_len = 0;		/* No block descriptors. */
	mh->dev_spec = 0;		/* Clear device-specific parameters. */
	mph = MODE_PAGE_HEADER(mh);
	mode_pars = MODE_PAGE_DATA(mph);

	mph->page_code &= SMS_PAGE_CODE;/* Isolate just the page code. */
	mh->data_length = 0;		/* Reserved for MODE SELECT command. */

	/*
	 * Write the changes back to the device. If the user editted control
	 * page 3 (saved values) then request the changes be permanently
	 * recorded.
	 */
	mode_select(device,
	    (page_control << PAGE_CTRL_SHIFT == SMS_PAGE_CTRL_SAVED),
	    retries, timeout, (u_int8_t *)mh,
	    sizeof(*mh) + mh->blk_desc_len + sizeof(*mph) + mph->page_length);
}

static int
modepage_write(FILE *file, int editonly)
{
	struct editentry *scan;
	int written = 0;

	STAILQ_FOREACH(scan, &editlist, link) {
		if (scan->editable || !editonly) {
			written++;
			if (scan->type == 'c' || scan->type == 'z') {
				fprintf(file, "%s:  %s\n", scan->name,
				    scan->value.svalue);
			} else {
				fprintf(file, "%s:  %d\n", scan->name,
				    scan->value.ivalue);
			}
		}
	}
	return (written);
}

static int
modepage_read(FILE *file)
{
	char *buffer;			/* Pointer to dynamic line buffer.  */
	char *line;			/* Pointer to static fgetln buffer. */
	char *name;			/* Name portion of the line buffer. */
	char *value;			/* Value portion of line buffer.    */
	int length;			/* Length of static fgetln buffer.  */

#define	ABORT_READ(message, param) do {					\
	warnx(message, param);						\
	free(buffer);							\
	returnerr(EAGAIN);						\
} while (0)

	while ((line = fgetln(file, &length)) != NULL) {
		/* Trim trailing whitespace (including optional newline). */
		while (length > 0 && isspace(line[length - 1]))
			length--;

	    	/* Allocate a buffer to hold the line + terminating null. */
	    	if ((buffer = malloc(length + 1)) == NULL)
			err(EX_OSERR, NULL);
		memcpy(buffer, line, length);
		buffer[length] = '\0';

		/* Strip out comments. */
		if ((value = strchr(buffer, '#')) != NULL)
			*value = '\0';

		/* The name is first in the buffer. Trim whitespace.*/
		name = buffer;
		RTRIM(name);
		while (isspace(*name))
			name++;

		/* Skip empty lines. */
		if (strlen(name) == 0)
			continue;

		/* The name ends at the colon; the value starts there. */
		if ((value = strrchr(buffer, ':')) == NULL)
			ABORT_READ("no value associated with %s", name);
		*value = '\0';			/* Null-terminate name. */
		value++;			/* Value starts afterwards. */

		/* Trim leading and trailing whitespace. */
		RTRIM(value);
		while (isspace(*value))
			value++;

		/* Make sure there is a value left. */
		if (strlen(value) == 0)
			ABORT_READ("no value associated with %s", name);

		/* Update our in-memory copy of the modepage entry value. */
		if (editentry_set(name, value, 1) != 0) {
			if (errno == ENOENT) {
				/* No entry by the name. */
				ABORT_READ("no such modepage entry \"%s\"",
				    name);
			} else if (errno == EINVAL) {
				/* Invalid value. */
				ABORT_READ("Invalid value for entry \"%s\"",
				    name);
			} else if (errno == ERANGE) {
				/* Value out of range for entry type. */
				ABORT_READ("value out of range for %s", name);
			} else if (errno == EPERM) {
				/* Entry is not editable; not fatal. */
				warnx("modepage entry \"%s\" is read-only; "
				    "skipping.", name);
			}
		}

		free(buffer);
	}
	return (ferror(file)? -1: 0);

#undef ABORT_READ
}

static void
modepage_edit(void)
{
	const char *editor;
	char *commandline;
	int fd;
	int written;

	if (!isatty(fileno(stdin))) {
		/* Not a tty, read changes from stdin. */
		modepage_read(stdin);
		return;
	}

	/* Lookup editor to invoke. */
	if ((editor = getenv("EDITOR")) == NULL)
		editor = DEFAULT_EDITOR;

	/* Create temp file for editor to modify. */
	if ((fd = mkstemp(edit_path)) == -1)
		errx(EX_CANTCREAT, "mkstemp failed");

	atexit(cleanup_editfile);

	if ((edit_file = fdopen(fd, "w")) == NULL)
		err(EX_NOINPUT, "%s", edit_path);

	written = modepage_write(edit_file, 1);

	fclose(edit_file);
	edit_file = NULL;

	if (written == 0) {
		warnx("no editable entries");
		cleanup_editfile();
		return;
	}

	/*
	 * Allocate memory to hold the command line (the 2 extra characters
	 * are to hold the argument separator (a space), and the terminating
	 * null character.
	 */
	commandline = malloc(strlen(editor) + strlen(edit_path) + 2);
	if (commandline == NULL)
		err(EX_OSERR, NULL);
	sprintf(commandline, "%s %s", editor, edit_path);

	/* Invoke the editor on the temp file. */
	if (system(commandline) == -1)
		err(EX_UNAVAILABLE, "could not invoke %s", editor);
	free(commandline);

	if ((edit_file = fopen(edit_path, "r")) == NULL)
		err(EX_NOINPUT, "%s", edit_path);

	/* Read any changes made to the temp file. */
	modepage_read(edit_file);

	cleanup_editfile();
}

static void
modepage_dump(struct cam_device *device, int page, int page_control, int dbd,
	      int retries, int timeout)
{
	u_int8_t data[MAX_COMMAND_SIZE];/* Buffer to hold sense data. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_header_6 *mh;	/* Location of mode header. */
	struct scsi_mode_page_header *mph;
	int mode_idx;			/* Index for scanning mode params. */

	mode_sense(device, page, page_control, dbd, retries, timeout, data,
		   sizeof(data));

	mh = (struct scsi_mode_header_6 *)data;
	mph = MODE_PAGE_HEADER(mh);
	mode_pars = MODE_PAGE_DATA(mph);

	/* Print the raw mode page data with newlines each 8 bytes. */
	for (mode_idx = 0; mode_idx < mph->page_length; mode_idx++) {
		printf("%02x%c", mode_pars[mode_idx],
		    (((mode_idx + 1) % 8) == 0) ? '\n' : ' ');
	}
	putchar('\n');
}

static void
cleanup_editfile(void)
{
	if (edit_file == NULL)
		return;
	if (fclose(edit_file) != 0 || unlink(edit_path) != 0)
		warn("%s", edit_path);
	edit_file = NULL;
}

void
mode_edit(struct cam_device *device, int page, int page_control, int dbd,
	  int edit, int binary, int retry_count, int timeout)
{
	const char *pagedb_path;	/* Path to modepage database. */

	if (edit && binary)
		errx(EX_USAGE, "cannot edit in binary mode.");

	if (! binary) {
		if ((pagedb_path = getenv("SCSI_MODES")) == NULL)
			pagedb_path = DEFAULT_SCSI_MODE_DB;

		if (load_format(pagedb_path, page) != 0 && (edit || verbose)) {
			if (errno == ENOENT) {
				/* Modepage database file not found. */
				warn("cannot open modepage database \"%s\"",
				    pagedb_path);
			} else if (errno == ESRCH) {
				/* Modepage entry not found in database. */
				warnx("modepage %d not found in database"
				    "\"%s\"", page, pagedb_path);
			}
			/* We can recover in display mode, otherwise we exit. */
			if (!edit) {
				warnx("reverting to binary display only");
				binary = 1;
			} else
				exit(EX_OSFILE);
		}

		editlist_populate(device, page, page_control, dbd, retry_count,
			timeout);
	}

	if (edit) {
		if (page_control << PAGE_CTRL_SHIFT != SMS_PAGE_CTRL_CURRENT &&
		    page_control << PAGE_CTRL_SHIFT != SMS_PAGE_CTRL_SAVED)
			errx(EX_USAGE, "it only makes sense to edit page 0 "
			    "(current) or page 3 (saved values)");
		modepage_edit();
		editlist_save(device, page, page_control, dbd, retry_count,
			timeout);
	} else if (binary || STAILQ_EMPTY(&editlist)) {
		/* Display without formatting information. */
		modepage_dump(device, page, page_control, dbd, retry_count,
		    timeout);
	} else {
		/* Display with format. */
		modepage_write(stdout, 0);
	}
}

void
mode_list(struct cam_device *device, int page_control, int dbd,
	  int retry_count, int timeout)
{
	u_int8_t data[MAX_COMMAND_SIZE];/* Buffer to hold sense data. */
	u_int8_t *mode_pars;		/* Pointer to modepage params. */
	struct scsi_mode_header_6 *mh;	/* Location of mode header. */
	struct scsi_mode_page_header *mph;
	struct pagename *nameentry;
	const char *pagedb_path;
	int len;

	if ((pagedb_path = getenv("SCSI_MODES")) == NULL)
		pagedb_path = DEFAULT_SCSI_MODE_DB;

	if (load_format(pagedb_path, 0) != 0 && verbose && errno == ENOENT) {
		/* Modepage database file not found. */
		warn("cannot open modepage database \"%s\"", pagedb_path);
	}

	/* Build the list of all mode pages by querying the "all pages" page. */
	mode_sense(device, SMS_ALL_PAGES_PAGE, page_control, dbd, retry_count,
	    timeout, data, sizeof(data));

	mh = (struct scsi_mode_header_6 *)data;
	len = mh->blk_desc_len;		/* Skip block descriptors. */
	/* Iterate through the pages in the reply. */
	while (len < mh->data_length) {
		/* Locate the next mode page header. */
		mph = (struct scsi_mode_page_header *)
		    ((intptr_t)mh + sizeof(*mh) + len);
		mode_pars = MODE_PAGE_DATA(mph);

		mph->page_code &= SMS_PAGE_CODE;
		nameentry = nameentry_lookup(mph->page_code);

		if (nameentry == NULL || nameentry->name == NULL)
			printf("0x%02x\n", mph->page_code);
		else
			printf("0x%02x\t%s\n", mph->page_code,
			    nameentry->name); 
		len += mph->page_length + sizeof(*mph);
	}
}
