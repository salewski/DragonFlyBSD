/*
 * Copyright (c) 2003 The Free Software Foundation.
 *
 * Portions Copyright (c) 2003 Derek Price
 *                         and Ximbiot <http://ximbiot.com>,
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS kit.
 *
 *
 *
 * This file contains the interface between the server and the rest of CVS.
 */

/* Miscellaneous stuff which isn't actually particularly server-specific.  */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif


/*
 * Expand to `S', ` ', or the empty string.  Used in `%s-> ...' trace printfs.
 */
#ifdef SERVER_SUPPORT
# define CLIENT_SERVER_STR ((server_active) ? "S" : " ")
#else
# define CLIENT_SERVER_STR ""
#endif

#ifdef SERVER_SUPPORT

/*
 * Nonzero if we are using the server.  Used by various places to call
 * server-specific functions.
 */
extern int server_active;

/* Server functions exported to the rest of CVS.  */

/* Run the server.  */
int server (int argc, char **argv);

/* kserver user authentication.  */
# ifdef HAVE_KERBEROS
void kserver_authenticate_connection (void);
# endif

/* pserver user authentication.  */
# if defined (AUTH_SERVER_SUPPORT) || defined (HAVE_GSSAPI)
void pserver_authenticate_connection (void);
# endif

/* See server.c for description.  */
void server_pathname_check (char *);

/* We have a new Entries line for a file.  TAG or DATE can be NULL.  */
void server_register (const char *name, const char *version,
                      const char *timestamp, const char *options,
                      const char *tag, const char *date, const char *conflict);

/* Set the modification time of the next file sent.  This must be
   followed by a call to server_updated on the same file.  */
void server_modtime (struct file_info *finfo, Vers_TS *vers_ts);

/*
 * We want to nuke the Entries line for a file, and (unless
 * server_scratch_entry_only is subsequently called) the file itself.
 */
void server_scratch (const char *name);

/*
 * The file which just had server_scratch called on it needs to have only
 * the Entries line removed, not the file itself.
 */
void server_scratch_entry_only (void);

/*
 * We just successfully checked in FILE (which is just the bare
 * filename, with no directory).  REPOSITORY is the directory for the
 * repository.
 */
void server_checked_in (const char *file, const char *update_dir,
                        const char *repository);

void server_copy_file (const char *file, const char *update_dir,
                       const char *repository, const char *newfile);

/* Send the appropriate responses for a file described by FINFO and
   VERS.  This is called after server_register or server_scratch.  In
   the latter case the file is to be removed (and VERS can be NULL).
   In the former case, VERS must be non-NULL, and UPDATED indicates
   whether the file is now up to date (SERVER_UPDATED, yes,
   SERVER_MERGED, no, SERVER_PATCHED, yes, but file is a diff from
   user version to repository version, SERVER_RCS_DIFF, yes, like
   SERVER_PATCHED but with an RCS style diff).  MODE is the mode the
   file should get, or (mode_t) -1 if this should be obtained from the
   file itself.  CHECKSUM is the MD5 checksum of the file, or NULL if
   this need not be sent.  If FILEBUF is not NULL, it holds the
   contents of the file, in which case the file itself may not exist.
   If FILEBUF is not NULL, server_updated will free it.  */
enum server_updated_arg4
{
    SERVER_UPDATED,
    SERVER_MERGED,
    SERVER_PATCHED,
    SERVER_RCS_DIFF
};

struct buffer;

void server_updated (struct file_info *finfo, Vers_TS *vers,
                     enum server_updated_arg4 updated, mode_t mode,
                     unsigned char *checksum, struct buffer *filebuf);

/* Whether we should send RCS format patches.  */
int server_use_rcs_diff (void);

/* Set the Entries.Static flag.  */
void server_set_entstat (const char *update_dir, const char *repository);
/* Clear it.  */
void server_clear_entstat (const char *update_dir, const char *repository);

/* Set or clear a per-directory sticky tag or date.  */
void server_set_sticky (const char *update_dir, const char *repository,
                        const char *tag, const char *date, int nonbranch);

/* Send Clear-template response.  */
void server_clear_template (const char *update_dir, const char *repository);

/* Send Template response.  */
void server_template (const char *update_dir, const char *repository);

void server_update_entries (const char *file, const char *update_dir,
                            const char *repository,
                            enum server_updated_arg4 updated);

/* Pointer to a malloc'd string which is the directory which
   the server should prepend to the pathnames which it sends
   to the client.  */
extern char *server_dir;

void server_cleanup (void);

#ifdef SERVER_FLOWCONTROL
/* Pause if it's convenient to avoid memory blowout */
void server_pause_check (void);
#endif /* SERVER_FLOWCONTROL */

#ifdef AUTH_SERVER_SUPPORT
extern char *CVS_Username;
#endif /* AUTH_SERVER_SUPPORT */

#endif /* SERVER_SUPPORT */

/* Stuff shared with the client.  */
struct request
{
  /* Name of the request.  */
  char *name;

#ifdef SERVER_SUPPORT
  /*
   * Function to carry out the request.  ARGS is the text of the command
   * after name and, if present, a single space, have been stripped off.
   */
  void (*func) (char *args);
#endif

  /* One or more of the RQ_* flags described below.  */
  int flags;

  /* If set, failure to implement this request can imply a fatal
     error.  This should be set only for commands which were in the
     original version of the protocol; it should not be set for new
     commands.  */
#define RQ_ESSENTIAL 1

  /* Set by the client if the server we are talking to supports it.  */
#define RQ_SUPPORTED 2

  /* If set, and client and server both support the request, the
     client should tell the server by making the request.  */
#define RQ_ENABLEME 4

  /* The server may accept this request before "Root".  */
#define RQ_ROOTLESS 8
};

/* Table of requests ending with an entry with a NULL name.  */
extern struct request requests[];

/* Gzip library, see zlib.c.  */
int gunzip_and_write (int, const char *, unsigned char *, size_t);
int read_and_gzip (int, const char *, unsigned char **, size_t *, size_t *,
                   int);
void server_edit_file (struct file_info *finfo);

extern cvsroot_t *referrer;
