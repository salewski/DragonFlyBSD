/*-
 * Copyright (c) 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Arnold.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) Copyright (c) 1986, 1993 The Regents of the University of California.  All rights reserved.
 * @(#)fortune.c   8.1 (Berkeley) 5/31/93
 * $FreeBSD: src/games/fortune/fortune/fortune.c,v 1.18.2.1 2001/07/02 00:35:27 dd Exp $
 */
/* $FreeBSD: src/games/fortune/fortune/fortune.c,v 1.18.2.1 2001/07/02 00:35:27 dd Exp $ */
/* $DragonFly: src/games/fortune/fortune/fortune.c,v 1.3 2003/11/12 14:53:53 eirikn Exp $ */

# include	<sys/param.h>
# include	<sys/stat.h>

# include	<dirent.h>
# include	<fcntl.h>
# include	<assert.h>
# include	<unistd.h>
# include	<stdio.h>
# include	<ctype.h>
# include	<stdlib.h>
# include	<string.h>
# include       <locale.h>
# include       <time.h>
# include	"strfile.h"
# include	"pathnames.h"

# define	TRUE	1
# define	FALSE	0
# define	bool	short

# define	MINW	6		/* minimum wait if desired */
# define	CPERS	20		/* # of chars for each sec */
# define	SLEN	160		/* # of chars in short fortune */

# define        POS_UNKNOWN     ((long) -1)     /* pos for file unknown */
# define	NO_PROB		(-1)		/* no prob specified for file */

# ifdef DEBUG
# define        DPRINTF(l,x)    { if (Debug >= l) fprintf x; }
# undef		NDEBUG
# else
# define	DPRINTF(l,x)
# define	NDEBUG	1
# endif

typedef struct fd {
	int		percent;
	int		fd, datfd;
	long            pos;
	FILE		*inf;
	char		*name;
	char		*path;
	char		*datfile, *posfile;
	bool		read_tbl;
	bool		was_pos_file;
	STRFILE		tbl;
	int		num_children;
	struct fd	*child, *parent;
	struct fd	*next, *prev;
} FILEDESC;

bool	Found_one;			/* did we find a match? */
bool	Find_files	= FALSE;	/* just find a list of proper fortune files */
bool    Fortunes_only   = FALSE;        /* check only "fortunes" files */
bool	Wait		= FALSE;	/* wait desired after fortune */
bool	Short_only	= FALSE;	/* short fortune desired */
bool	Long_only	= FALSE;	/* long fortune desired */
bool	Offend		= FALSE;	/* offensive fortunes only */
bool	All_forts	= FALSE;	/* any fortune allowed */
bool	Equal_probs	= FALSE;	/* scatter un-allocted prob equally */
#ifndef NO_REGEX
bool	Match		= FALSE;	/* dump fortunes matching a pattern */
#endif
#ifdef DEBUG
bool	Debug = FALSE;			/* print debug messages */
#endif

char	*Fortbuf = NULL;			/* fortune buffer for -m */

int	Fort_len = 0;

long    Seekpts[2];                     /* seek pointers to fortunes */

FILEDESC	*File_list = NULL,	/* Head of file list */
		*File_tail = NULL;	/* Tail of file list */
FILEDESC	*Fortfile;		/* Fortune file to use */

STRFILE		Noprob_tbl;		/* sum of data for all no prob files */

int	 add_dir (FILEDESC *);
int	 add_file (int,
	    char *, char *, FILEDESC **, FILEDESC **, FILEDESC *);
void	 all_forts (FILEDESC *, char *);
char	*copy (char *, u_int);
void	 display (FILEDESC *);
void	 do_free (void *);
void	*do_malloc (u_int);
int	 form_file_list (char **, int);
int	 fortlen (void);
void	 get_fort (void);
void	 get_pos (FILEDESC *);
void	 get_tbl (FILEDESC *);
void	 getargs (int, char *[]);
void	 init_prob (void);
int	 is_dir (char *);
int	 is_fortfile (char *, char **, char **, int);
int	 is_off_name (char *);
int	 max (int, int);
FILEDESC *
	 new_fp (void);
char	*off_name (char *);
void	 open_dat (FILEDESC *);
void	 open_fp (FILEDESC *);
FILEDESC *
	 pick_child (FILEDESC *);
void	 print_file_list (void);
void	 print_list (FILEDESC *, int);
void	 sum_noprobs (FILEDESC *);
void	 sum_tbl (STRFILE *, STRFILE *);
void	 usage (void);
void	 zero_tbl (STRFILE *);

#ifndef	NO_REGEX
char	*conv_pat (char *);
int	 find_matches (void);
void	 matches_in_list (FILEDESC *);
int	 maxlen_in_list (FILEDESC *);
#endif

#ifndef NO_REGEX
#ifdef REGCMP
# define	RE_COMP(p)	(Re_pat = regcmp(p, NULL))
# define	BAD_COMP(f)	((f) == NULL)
# define	RE_EXEC(p)	regex(Re_pat, (p))

char	*Re_pat;

char	*regcmp(), *regex();
#else
# define	RE_COMP(p)	(p = re_comp(p))
# define	BAD_COMP(f)	((f) != NULL)
# define	RE_EXEC(p)	re_exec(p)

#endif
#endif

int
main(ac, av)
int	ac;
char	*av[];
{
#ifdef	OK_TO_WRITE_DISK
	int	fd;
#endif	/* OK_TO_WRITE_DISK */

	(void) setlocale(LC_ALL, "");

	getargs(ac, av);

#ifndef NO_REGEX
	if (Match)
		exit(find_matches() != 0);
#endif

	init_prob();
	srandomdev();
	do {
		get_fort();
	} while ((Short_only && fortlen() > SLEN) ||
		 (Long_only && fortlen() <= SLEN));

	display(Fortfile);

#ifdef	OK_TO_WRITE_DISK
	if ((fd = creat(Fortfile->posfile, 0666)) < 0) {
		perror(Fortfile->posfile);
		exit(1);
	}
#ifdef	LOCK_EX
	/*
	 * if we can, we exclusive lock, but since it isn't very
	 * important, we just punt if we don't have easy locking
	 * available.
	 */
	(void) flock(fd, LOCK_EX);
#endif	/* LOCK_EX */
	write(fd, (char *) &Fortfile->pos, sizeof Fortfile->pos);
	if (!Fortfile->was_pos_file)
		(void) chmod(Fortfile->path, 0666);
#ifdef	LOCK_EX
	(void) flock(fd, LOCK_UN);
#endif	/* LOCK_EX */
#endif	/* OK_TO_WRITE_DISK */
	if (Wait) {
		if (Fort_len == 0)
			(void) fortlen();
		sleep((unsigned int) max(Fort_len / CPERS, MINW));
	}
	exit(0);
	/* NOTREACHED */
}

void
display(fp)
FILEDESC	*fp;
{
	char   *p;
	unsigned char ch;
	char	line[BUFSIZ];

	open_fp(fp);
	(void) fseek(fp->inf, Seekpts[0], 0);
	for (Fort_len = 0; fgets(line, sizeof line, fp->inf) != NULL &&
	    !STR_ENDSTRING(line, fp->tbl); Fort_len++) {
		if (fp->tbl.str_flags & STR_ROTATED)
			for (p = line; (ch = *p) != '\0'; ++p) {
				if (isascii(ch)) {
					if (isupper(ch))
						*p = 'A' + (ch - 'A' + 13) % 26;
					else if (islower(ch))
						*p = 'a' + (ch - 'a' + 13) % 26;
				}
			}
		if (fp->tbl.str_flags & STR_COMMENTS
		    && line[0] == fp->tbl.str_delim
		    && line[1] == fp->tbl.str_delim)
			continue;
		fputs(line, stdout);
	}
	(void) fflush(stdout);
}

/*
 * fortlen:
 *	Return the length of the fortune.
 */
int
fortlen()
{
	int	nchar;
	char		line[BUFSIZ];

	if (!(Fortfile->tbl.str_flags & (STR_RANDOM | STR_ORDERED)))
		nchar = (Seekpts[1] - Seekpts[0] <= SLEN);
	else {
		open_fp(Fortfile);
		(void) fseek(Fortfile->inf, Seekpts[0], 0);
		nchar = 0;
		while (fgets(line, sizeof line, Fortfile->inf) != NULL &&
		       !STR_ENDSTRING(line, Fortfile->tbl))
			nchar += strlen(line);
	}
	Fort_len = nchar;
	return nchar;
}

/*
 *	This routine evaluates the arguments on the command line
 */
void
getargs(argc, argv)
int	argc;
char	**argv;
{
	int	ignore_case;
# ifndef NO_REGEX
	char	*pat;
# endif	/* NO_REGEX */
	extern char *optarg;
	extern int optind;
	int ch;

	ignore_case = FALSE;
# ifndef NO_REGEX
	pat = NULL;
# endif /* NO_REGEX */

# ifdef DEBUG
	while ((ch = getopt(argc, argv, "aDefilm:osw")) != -1)
#else
	while ((ch = getopt(argc, argv, "aefilm:osw")) != -1)
#endif /* DEBUG */
		switch(ch) {
		case 'a':		/* any fortune */
			All_forts++;
			break;
# ifdef DEBUG
		case 'D':
			Debug++;
			break;
# endif /* DEBUG */
		case 'e':
			Equal_probs++;	/* scatter un-allocted prob equally */
			break;
		case 'f':		/* find fortune files */
			Find_files++;
			break;
		case 'l':		/* long ones only */
			Long_only++;
			Short_only = FALSE;
			break;
		case 'o':		/* offensive ones only */
			Offend++;
			break;
		case 's':		/* short ones only */
			Short_only++;
			Long_only = FALSE;
			break;
		case 'w':		/* give time to read */
			Wait++;
			break;
# ifdef	NO_REGEX
		case 'i':			/* case-insensitive match */
		case 'm':			/* dump out the fortunes */
			(void) fprintf(stderr,
			    "fortune: can't match fortunes on this system (Sorry)\n");
			exit(0);
# else	/* NO_REGEX */
		case 'm':			/* dump out the fortunes */
			Match++;
			pat = optarg;
			break;
		case 'i':			/* case-insensitive match */
			ignore_case++;
			break;
# endif	/* NO_REGEX */
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!form_file_list(argv, argc))
		exit(1);	/* errors printed through form_file_list() */
	if (Find_files) {
		print_file_list();
		exit(0);
	}
#ifdef DEBUG
	else if (Debug >= 1)
		print_file_list();
#endif /* DEBUG */

# ifndef NO_REGEX
	if (pat != NULL) {
		if (ignore_case)
			pat = conv_pat(pat);
		if (BAD_COMP(RE_COMP(pat))) {
#ifndef REGCMP
			fprintf(stderr, "%s\n", pat);
#else	/* REGCMP */
			fprintf(stderr, "bad pattern: %s\n", pat);
#endif	/* REGCMP */
		}
	}
# endif	/* NO_REGEX */
}

/*
 * form_file_list:
 *	Form the file list from the file specifications.
 */
int
form_file_list(files, file_cnt)
char	**files;
int	file_cnt;
{
	int	i, percent;
	char	*sp;

	if (file_cnt == 0) {
		if (Find_files) {
			Fortunes_only = TRUE;
			i = add_file(NO_PROB, FORTDIR, NULL, &File_list,
					&File_tail, NULL);
			Fortunes_only = FALSE;
			return i;
		} else
			return add_file(NO_PROB, "fortunes", FORTDIR,
					&File_list, &File_tail, NULL);
	}
	for (i = 0; i < file_cnt; i++) {
		percent = NO_PROB;
		if (!isdigit((unsigned char)files[i][0]))
			sp = files[i];
		else {
			percent = 0;
			for (sp = files[i]; isdigit((unsigned char)*sp); sp++)
				percent = percent * 10 + *sp - '0';
			if (percent > 100) {
				fprintf(stderr, "percentages must be <= 100\n");
				return FALSE;
			}
			if (*sp == '.') {
				fprintf(stderr, "percentages must be integers\n");
				return FALSE;
			}
			/*
			 * If the number isn't followed by a '%', then
			 * it was not a percentage, just the first part
			 * of a file name which starts with digits.
			 */
			if (*sp != '%') {
				percent = NO_PROB;
				sp = files[i];
			}
			else if (*++sp == '\0') {
				if (++i >= file_cnt) {
					fprintf(stderr, "percentages must precede files\n");
					return FALSE;
				}
				sp = files[i];
			}
		}
		if (strcmp(sp, "all") == 0)
			sp = FORTDIR;
		if (!add_file(percent, sp, NULL, &File_list, &File_tail, NULL))
			return FALSE;
	}
	return TRUE;
}

/*
 * add_file:
 *	Add a file to the file list.
 */
int
add_file(percent, file, dir, head, tail, parent)
int		percent;
char	*file;
char		*dir;
FILEDESC	**head, **tail;
FILEDESC	*parent;
{
	FILEDESC	*fp;
	int		fd;
	char		*path, *offensive;
	bool		was_malloc;
	bool		isdir;

	if (dir == NULL) {
		path = file;
		was_malloc = FALSE;
	}
	else {
		path = do_malloc((unsigned int) (strlen(dir) + strlen(file) + 2));
		(void) strcat(strcat(strcpy(path, dir), "/"), file);
		was_malloc = TRUE;
	}
	if ((isdir = is_dir(path)) && parent != NULL) {
		if (was_malloc)
			free(path);
		return FALSE;	/* don't recurse */
	}
	offensive = NULL;
	if (!isdir && parent == NULL && (All_forts || Offend) &&
	    !is_off_name(path)) {
		offensive = off_name(path);
		if (Offend) {
			if (was_malloc)
				free(path);
			path = offensive;
			offensive = NULL;
			was_malloc = TRUE;
			DPRINTF(1, (stderr, "\ttrying \"%s\"\n", path));
			file = off_name(file);
		}
	}

	DPRINTF(1, (stderr, "adding file \"%s\"\n", path));
over:
	if ((fd = open(path, 0)) < 0) {
		/*
		 * This is a sneak.  If the user said -a, and if the
		 * file we're given isn't a file, we check to see if
		 * there is a -o version.  If there is, we treat it as
		 * if *that* were the file given.  We only do this for
		 * individual files -- if we're scanning a directory,
		 * we'll pick up the -o file anyway.
		 */
		if (All_forts && offensive != NULL) {
			if (was_malloc)
				free(path);
			path = offensive;
			offensive = NULL;
			was_malloc = TRUE;
			DPRINTF(1, (stderr, "\ttrying \"%s\"\n", path));
			file = off_name(file);
			goto over;
		}
		if (dir == NULL && file[0] != '/')
			return add_file(percent, file, FORTDIR, head, tail,
					parent);
		if (parent == NULL)
			perror(path);
		if (was_malloc)
			free(path);
		return FALSE;
	}

	DPRINTF(2, (stderr, "path = \"%s\"\n", path));

	fp = new_fp();
	fp->fd = fd;
	fp->percent = percent;
	fp->name = file;
	fp->path = path;
	fp->parent = parent;

	if ((isdir && !add_dir(fp)) ||
	    (!isdir &&
	     !is_fortfile(path, &fp->datfile, &fp->posfile, (parent != NULL))))
	{
		if (parent == NULL)
			fprintf(stderr,
				"fortune:%s not a fortune file or directory\n",
				path);
		if (was_malloc)
			free(path);
		do_free(fp->datfile);
		do_free(fp->posfile);
		free((char *) fp);
		do_free(offensive);
		return FALSE;
	}
	/*
	 * If the user said -a, we need to make this node a pointer to
	 * both files, if there are two.  We don't need to do this if
	 * we are scanning a directory, since the scan will pick up the
	 * -o file anyway.
	 */
	if (All_forts && parent == NULL && !is_off_name(path))
		all_forts(fp, offensive);
	if (*head == NULL)
		*head = *tail = fp;
	else if (fp->percent == NO_PROB) {
		(*tail)->next = fp;
		fp->prev = *tail;
		*tail = fp;
	}
	else {
		(*head)->prev = fp;
		fp->next = *head;
		*head = fp;
	}
#ifdef	OK_TO_WRITE_DISK
	fp->was_pos_file = (access(fp->posfile, W_OK) >= 0);
#endif	/* OK_TO_WRITE_DISK */

	return TRUE;
}

/*
 * new_fp:
 *	Return a pointer to an initialized new FILEDESC.
 */
FILEDESC *
new_fp()
{
	FILEDESC	*fp;

	fp = (FILEDESC *) do_malloc(sizeof *fp);
	fp->datfd = -1;
	fp->pos = POS_UNKNOWN;
	fp->inf = NULL;
	fp->fd = -1;
	fp->percent = NO_PROB;
	fp->read_tbl = FALSE;
	fp->next = NULL;
	fp->prev = NULL;
	fp->child = NULL;
	fp->parent = NULL;
	fp->datfile = NULL;
	fp->posfile = NULL;
	return fp;
}

/*
 * off_name:
 *	Return a pointer to the offensive version of a file of this name.
 */
char *
off_name(file)
char	*file;
{
	char	*new;

	new = copy(file, (unsigned int) (strlen(file) + 2));
	return strcat(new, "-o");
}

/*
 * is_off_name:
 *	Is the file an offensive-style name?
 */
int
is_off_name(file)
char	*file;
{
	int	len;

	len = strlen(file);
	return (len >= 3 && file[len - 2] == '-' && file[len - 1] == 'o');
}

/*
 * all_forts:
 *	Modify a FILEDESC element to be the parent of two children if
 *	there are two children to be a parent of.
 */
void
all_forts(fp, offensive)
FILEDESC	*fp;
char			*offensive;
{
	char		*sp;
	FILEDESC	*scene, *obscene;
	int		fd;
	auto char		*datfile, *posfile;

	if (fp->child != NULL)	/* this is a directory, not a file */
		return;
	if (!is_fortfile(offensive, &datfile, &posfile, FALSE))
		return;
	if ((fd = open(offensive, 0)) < 0)
		return;
	DPRINTF(1, (stderr, "adding \"%s\" because of -a\n", offensive));
	scene = new_fp();
	obscene = new_fp();
	*scene = *fp;

	fp->num_children = 2;
	fp->child = scene;
	scene->next = obscene;
	obscene->next = NULL;
	scene->child = obscene->child = NULL;
	scene->parent = obscene->parent = fp;

	fp->fd = -1;
	scene->percent = obscene->percent = NO_PROB;

	obscene->fd = fd;
	obscene->inf = NULL;
	obscene->path = offensive;
	if ((sp = rindex(offensive, '/')) == NULL)
		obscene->name = offensive;
	else
		obscene->name = ++sp;
	obscene->datfile = datfile;
	obscene->posfile = posfile;
	obscene->read_tbl = FALSE;
#ifdef	OK_TO_WRITE_DISK
	obscene->was_pos_file = (access(obscene->posfile, W_OK) >= 0);
#endif	/* OK_TO_WRITE_DISK */
}

/*
 * add_dir:
 *	Add the contents of an entire directory.
 */
int
add_dir(fp)
FILEDESC	*fp;
{
	DIR		*dir;
	struct dirent	*dirent;
	auto FILEDESC		*tailp;
	auto char		*name;

	(void) close(fp->fd);
	fp->fd = -1;
	if ((dir = opendir(fp->path)) == NULL) {
		perror(fp->path);
		return FALSE;
	}
	tailp = NULL;
	DPRINTF(1, (stderr, "adding dir \"%s\"\n", fp->path));
	fp->num_children = 0;
	while ((dirent = readdir(dir)) != NULL) {
		if (dirent->d_namlen == 0)
			continue;
		name = copy(dirent->d_name, dirent->d_namlen);
		if (add_file(NO_PROB, name, fp->path, &fp->child, &tailp, fp))
			fp->num_children++;
		else
			free(name);
	}
	if (fp->num_children == 0) {
		(void) fprintf(stderr,
		    "fortune: %s: No fortune files in directory.\n", fp->path);
		return FALSE;
	}
	return TRUE;
}

/*
 * is_dir:
 *	Return TRUE if the file is a directory, FALSE otherwise.
 */
int
is_dir(file)
char	*file;
{
	auto struct stat	sbuf;

	if (stat(file, &sbuf) < 0)
		return FALSE;
	return (sbuf.st_mode & S_IFDIR);
}

/*
 * is_fortfile:
 *	Return TRUE if the file is a fortune database file.  We try and
 *	exclude files without reading them if possible to avoid
 *	overhead.  Files which start with ".", or which have "illegal"
 *	suffixes, as contained in suflist[], are ruled out.
 */
/* ARGSUSED */
int
is_fortfile(file, datp, posp, check_for_offend)
char	*file, **datp, **posp;
int	check_for_offend;
{
	int	i;
	char	*sp;
	char	*datfile;
	static char	*suflist[] = {	/* list of "illegal" suffixes" */
				"dat", "pos", "c", "h", "p", "i", "f",
				"pas", "ftn", "ins.c", "ins,pas",
				"ins.ftn", "sml",
				NULL
			};

	DPRINTF(2, (stderr, "is_fortfile(%s) returns ", file));

	/*
	 * Preclude any -o files for offendable people, and any non -o
	 * files for completely offensive people.
	 */
	if (check_for_offend && !All_forts) {
		i = strlen(file);
		if (Offend ^ (file[i - 2] == '-' && file[i - 1] == 'o')) {
			DPRINTF(2, (stderr, "FALSE (offending file)\n"));
			return FALSE;
		}
	}

	if ((sp = rindex(file, '/')) == NULL)
		sp = file;
	else
		sp++;
	if (*sp == '.') {
		DPRINTF(2, (stderr, "FALSE (file starts with '.')\n"));
		return FALSE;
	}
	if (Fortunes_only && strncmp(sp, "fortunes", 8) != 0) {
		DPRINTF(2, (stderr, "FALSE (check fortunes only)\n"));
		return FALSE;
	}
	if ((sp = rindex(sp, '.')) != NULL) {
		sp++;
		for (i = 0; suflist[i] != NULL; i++)
			if (strcmp(sp, suflist[i]) == 0) {
				DPRINTF(2, (stderr, "FALSE (file has suffix \".%s\")\n", sp));
				return FALSE;
			}
	}

	datfile = copy(file, (unsigned int) (strlen(file) + 4)); /* +4 for ".dat" */
	strcat(datfile, ".dat");
	if (access(datfile, R_OK) < 0) {
		DPRINTF(2, (stderr, "FALSE (no readable \".dat\" file)\n"));
#ifdef DEBUG
		if (Debug < 2)
			DPRINTF(0, (stderr, "Warning: file \"%s\" unreadable\n", datfile));
#endif
		free(datfile);
		return FALSE;
	}
	if (datp != NULL)
		*datp = datfile;
	else
		free(datfile);
	if (posp != NULL) {
#ifdef	OK_TO_WRITE_DISK
		*posp = copy(file, (unsigned int) (strlen(file) + 4)); /* +4 for ".dat" */
		(void) strcat(*posp, ".pos");
#else
		*posp = NULL;
#endif	/* OK_TO_WRITE_DISK */
	}
	DPRINTF(2, (stderr, "TRUE\n"));
	return TRUE;
}

/*
 * copy:
 *	Return a malloc()'ed copy of the string
 */
char *
copy(str, len)
char		*str;
unsigned int	len;
{
	char	*new, *sp;

	new = do_malloc(len + 1);
	sp = new;
	do {
		*sp++ = *str;
	} while (*str++);
	return new;
}

/*
 * do_malloc:
 *	Do a malloc, checking for NULL return.
 */
void *
do_malloc(size)
unsigned int	size;
{
	void	*new;

	if ((new = malloc(size)) == NULL) {
		(void) fprintf(stderr, "fortune: out of memory.\n");
		exit(1);
	}
	return new;
}

/*
 * do_free:
 *	Free malloc'ed space, if any.
 */
void
do_free(ptr)
void	*ptr;
{
	if (ptr != NULL)
		free(ptr);
}

/*
 * init_prob:
 *	Initialize the fortune probabilities.
 */
void
init_prob()
{
	FILEDESC       *fp, *last = NULL;
	int		percent, num_noprob, frac;

	/*
	 * Distribute the residual probability (if any) across all
	 * files with unspecified probability (i.e., probability of 0)
	 * (if any).
	 */

	percent = 0;
	num_noprob = 0;
	for (fp = File_tail; fp != NULL; fp = fp->prev)
		if (fp->percent == NO_PROB) {
			num_noprob++;
			if (Equal_probs)
				last = fp;
		}
		else
			percent += fp->percent;
	DPRINTF(1, (stderr, "summing probabilities:%d%% with %d NO_PROB's",
		    percent, num_noprob));
	if (percent > 100) {
		(void) fprintf(stderr,
		    "fortune: probabilities sum to %d%% > 100%%!\n", percent);
		exit(1);
	}
	else if (percent < 100 && num_noprob == 0) {
		(void) fprintf(stderr,
		    "fortune: no place to put residual probability (%d%% < 100%%)\n",
		    percent);
		exit(1);
	}
	else if (percent == 100 && num_noprob != 0) {
		(void) fprintf(stderr,
		    "fortune: no probability left to put in residual files (100%%)\n");
		exit(1);
	}
	percent = 100 - percent;
	if (Equal_probs) {
		if (num_noprob != 0) {
			if (num_noprob > 1) {
				frac = percent / num_noprob;
				DPRINTF(1, (stderr, ", frac = %d%%", frac));
				for (fp = File_list; fp != last; fp = fp->next)
					if (fp->percent == NO_PROB) {
						fp->percent = frac;
						percent -= frac;
					}
			}
			last->percent = percent;
			DPRINTF(1, (stderr, ", residual = %d%%", percent));
		}
	else
		DPRINTF(1, (stderr,
			    ", %d%% distributed over remaining fortunes\n",
			    percent));
	}
	DPRINTF(1, (stderr, "\n"));

#ifdef DEBUG
	if (Debug >= 1)
		print_file_list();
#endif
}

/*
 * get_fort:
 *	Get the fortune data file's seek pointer for the next fortune.
 */
void
get_fort()
{
	FILEDESC	*fp;
	int		choice;

	if (File_list->next == NULL || File_list->percent == NO_PROB)
		fp = File_list;
	else {
		choice = random() % 100;
		DPRINTF(1, (stderr, "choice = %d\n", choice));
		for (fp = File_list; fp->percent != NO_PROB; fp = fp->next)
			if (choice < fp->percent)
				break;
			else {
				choice -= fp->percent;
				DPRINTF(1, (stderr,
					    "    skip \"%s\", %d%% (choice = %d)\n",
					    fp->name, fp->percent, choice));
			}
			DPRINTF(1, (stderr,
				    "using \"%s\", %d%% (choice = %d)\n",
				    fp->name, fp->percent, choice));
	}
	if (fp->percent != NO_PROB)
		get_tbl(fp);
	else {
		if (fp->next != NULL) {
			sum_noprobs(fp);
			choice = random() % Noprob_tbl.str_numstr;
			DPRINTF(1, (stderr, "choice = %d (of %ld) \n", choice,
				    Noprob_tbl.str_numstr));
			while (choice >= fp->tbl.str_numstr) {
				choice -= fp->tbl.str_numstr;
				fp = fp->next;
				DPRINTF(1, (stderr,
					    "    skip \"%s\", %ld (choice = %d)\n",
					    fp->name, fp->tbl.str_numstr,
					    choice));
			}
			DPRINTF(1, (stderr, "using \"%s\", %ld\n", fp->name,
				    fp->tbl.str_numstr));
		}
		get_tbl(fp);
	}
	if (fp->child != NULL) {
		DPRINTF(1, (stderr, "picking child\n"));
		fp = pick_child(fp);
	}
	Fortfile = fp;
	get_pos(fp);
	open_dat(fp);
	(void) lseek(fp->datfd,
		     (off_t) (sizeof fp->tbl + fp->pos * sizeof Seekpts[0]), 0);
	read(fp->datfd, Seekpts, sizeof Seekpts);
	Seekpts[0] = ntohl(Seekpts[0]);
	Seekpts[1] = ntohl(Seekpts[1]);
}

/*
 * pick_child
 *	Pick a child from a chosen parent.
 */
FILEDESC *
pick_child(parent)
FILEDESC	*parent;
{
	FILEDESC	*fp;
	int		choice;

	if (Equal_probs) {
		choice = random() % parent->num_children;
		DPRINTF(1, (stderr, "    choice = %d (of %d)\n",
			    choice, parent->num_children));
		for (fp = parent->child; choice--; fp = fp->next)
			continue;
		DPRINTF(1, (stderr, "    using %s\n", fp->name));
		return fp;
	}
	else {
		get_tbl(parent);
		choice = random() % parent->tbl.str_numstr;
		DPRINTF(1, (stderr, "    choice = %d (of %ld)\n",
			    choice, parent->tbl.str_numstr));
		for (fp = parent->child; choice >= fp->tbl.str_numstr;
		     fp = fp->next) {
			choice -= fp->tbl.str_numstr;
			DPRINTF(1, (stderr, "\tskip %s, %ld (choice = %d)\n",
				    fp->name, fp->tbl.str_numstr, choice));
		}
		DPRINTF(1, (stderr, "    using %s, %ld\n", fp->name,
			    fp->tbl.str_numstr));
		return fp;
	}
}

/*
 * sum_noprobs:
 *	Sum up all the noprob probabilities, starting with fp.
 */
void
sum_noprobs(fp)
FILEDESC	*fp;
{
	static bool	did_noprobs = FALSE;

	if (did_noprobs)
		return;
	zero_tbl(&Noprob_tbl);
	while (fp != NULL) {
		get_tbl(fp);
		sum_tbl(&Noprob_tbl, &fp->tbl);
		fp = fp->next;
	}
	did_noprobs = TRUE;
}

int
max(i, j)
int	i, j;
{
	return (i >= j ? i : j);
}

/*
 * open_fp:
 *	Assocatiate a FILE * with the given FILEDESC.
 */
void
open_fp(fp)
FILEDESC	*fp;
{
	if (fp->inf == NULL && (fp->inf = fdopen(fp->fd, "r")) == NULL) {
		perror(fp->path);
		exit(1);
	}
}

/*
 * open_dat:
 *	Open up the dat file if we need to.
 */
void
open_dat(fp)
FILEDESC	*fp;
{
	if (fp->datfd < 0 && (fp->datfd = open(fp->datfile, 0)) < 0) {
		perror(fp->datfile);
		exit(1);
	}
}

/*
 * get_pos:
 *	Get the position from the pos file, if there is one.  If not,
 *	return a random number.
 */
void
get_pos(fp)
FILEDESC	*fp;
{
#ifdef	OK_TO_WRITE_DISK
	int	fd;
#endif /* OK_TO_WRITE_DISK */

	assert(fp->read_tbl);
	if (fp->pos == POS_UNKNOWN) {
#ifdef	OK_TO_WRITE_DISK
		if ((fd = open(fp->posfile, 0)) < 0 ||
		    read(fd, &fp->pos, sizeof fp->pos) != sizeof fp->pos)
			fp->pos = random() % fp->tbl.str_numstr;
		else if (fp->pos >= fp->tbl.str_numstr)
			fp->pos %= fp->tbl.str_numstr;
		if (fd >= 0)
			(void) close(fd);
#else
		fp->pos = random() % fp->tbl.str_numstr;
#endif /* OK_TO_WRITE_DISK */
	}
	if (++(fp->pos) >= fp->tbl.str_numstr)
		fp->pos -= fp->tbl.str_numstr;
	DPRINTF(1, (stderr, "pos for %s is %ld\n", fp->name, fp->pos));
}

/*
 * get_tbl:
 *	Get the tbl data file the datfile.
 */
void
get_tbl(fp)
FILEDESC	*fp;
{
	auto int		fd;
	FILEDESC	*child;

	if (fp->read_tbl)
		return;
	if (fp->child == NULL) {
		if ((fd = open(fp->datfile, 0)) < 0) {
			perror(fp->datfile);
			exit(1);
		}
		if (read(fd, (char *) &fp->tbl, sizeof fp->tbl) != sizeof fp->tbl) {
			(void)fprintf(stderr,
			    "fortune: %s corrupted\n", fp->path);
			exit(1);
		}
		/* fp->tbl.str_version = ntohl(fp->tbl.str_version); */
		fp->tbl.str_numstr = ntohl(fp->tbl.str_numstr);
		fp->tbl.str_longlen = ntohl(fp->tbl.str_longlen);
		fp->tbl.str_shortlen = ntohl(fp->tbl.str_shortlen);
		fp->tbl.str_flags = ntohl(fp->tbl.str_flags);
		(void) close(fd);
	}
	else {
		zero_tbl(&fp->tbl);
		for (child = fp->child; child != NULL; child = child->next) {
			get_tbl(child);
			sum_tbl(&fp->tbl, &child->tbl);
		}
	}
	fp->read_tbl = TRUE;
}

/*
 * zero_tbl:
 *	Zero out the fields we care about in a tbl structure.
 */
void
zero_tbl(tp)
STRFILE	*tp;
{
	tp->str_numstr = 0;
	tp->str_longlen = 0;
	tp->str_shortlen = ~((unsigned long)0);
}

/*
 * sum_tbl:
 *	Merge the tbl data of t2 into t1.
 */
void
sum_tbl(t1, t2)
STRFILE	*t1, *t2;
{
	t1->str_numstr += t2->str_numstr;
	if (t1->str_longlen < t2->str_longlen)
		t1->str_longlen = t2->str_longlen;
	if (t1->str_shortlen > t2->str_shortlen)
		t1->str_shortlen = t2->str_shortlen;
}

#define	STR(str)	((str) == NULL ? "NULL" : (str))

/*
 * print_file_list:
 *	Print out the file list
 */
void
print_file_list()
{
	print_list(File_list, 0);
}

/*
 * print_list:
 *	Print out the actual list, recursively.
 */
void
print_list(list, lev)
FILEDESC	*list;
int			lev;
{
	while (list != NULL) {
		fprintf(stderr, "%*s", lev * 4, "");
		if (list->percent == NO_PROB)
			fprintf(stderr, "___%%");
		else
			fprintf(stderr, "%3d%%", list->percent);
		fprintf(stderr, " %s", STR(list->name));
		DPRINTF(1, (stderr, " (%s, %s, %s)", STR(list->path),
			    STR(list->datfile), STR(list->posfile)));
		fprintf(stderr, "\n");
		if (list->child != NULL)
			print_list(list->child, lev + 1);
		list = list->next;
	}
}

#ifndef	NO_REGEX
/*
 * conv_pat:
 *	Convert the pattern to an ignore-case equivalent.
 */
char *
conv_pat(orig)
char	*orig;
{
	char		*sp;
	unsigned int	cnt;
	char		*new;

	cnt = 1;	/* allow for '\0' */
	for (sp = orig; *sp != '\0'; sp++)
		if (isalpha((unsigned char)*sp))
			cnt += 4;
		else
			cnt++;
	if ((new = malloc(cnt)) == NULL) {
		fprintf(stderr, "pattern too long for ignoring case\n");
		exit(1);
	}

	for (sp = new; *orig != '\0'; orig++) {
		if (islower((unsigned char)*orig)) {
			*sp++ = '[';
			*sp++ = *orig;
			*sp++ = toupper((unsigned char)*orig);
			*sp++ = ']';
		}
		else if (isupper((unsigned char)*orig)) {
			*sp++ = '[';
			*sp++ = *orig;
			*sp++ = tolower((unsigned char)*orig);
			*sp++ = ']';
		}
		else
			*sp++ = *orig;
	}
	*sp = '\0';
	return new;
}

/*
 * find_matches:
 *	Find all the fortunes which match the pattern we've been given.
 */
int
find_matches()
{
	Fort_len = maxlen_in_list(File_list);
	DPRINTF(2, (stderr, "Maximum length is %d\n", Fort_len));
	/* extra length, "%\n" is appended */
	Fortbuf = do_malloc((unsigned int) Fort_len + 10);

	Found_one = FALSE;
	matches_in_list(File_list);
	return Found_one;
	/* NOTREACHED */
}

/*
 * maxlen_in_list
 *	Return the maximum fortune len in the file list.
 */
int
maxlen_in_list(list)
FILEDESC	*list;
{
	FILEDESC	*fp;
	int		len, maxlen;

	maxlen = 0;
	for (fp = list; fp != NULL; fp = fp->next) {
		if (fp->child != NULL) {
			if ((len = maxlen_in_list(fp->child)) > maxlen)
				maxlen = len;
		}
		else {
			get_tbl(fp);
			if (fp->tbl.str_longlen > maxlen)
				maxlen = fp->tbl.str_longlen;
		}
	}
	return maxlen;
}

/*
 * matches_in_list
 *	Print out the matches from the files in the list.
 */
void
matches_in_list(list)
FILEDESC	*list;
{
	char           *sp, *p;
	FILEDESC	*fp;
	int			in_file;
	unsigned char           ch;

	for (fp = list; fp != NULL; fp = fp->next) {
		if (fp->child != NULL) {
			matches_in_list(fp->child);
			continue;
		}
		DPRINTF(1, (stderr, "searching in %s\n", fp->path));
		open_fp(fp);
		sp = Fortbuf;
		in_file = FALSE;
		while (fgets(sp, Fort_len, fp->inf) != NULL)
			if (fp->tbl.str_flags & STR_COMMENTS
			    && sp[0] == fp->tbl.str_delim
			    && sp[1] == fp->tbl.str_delim)
				continue;
			else if (!STR_ENDSTRING(sp, fp->tbl))
				sp += strlen(sp);
			else {
				*sp = '\0';
				if (fp->tbl.str_flags & STR_ROTATED)
					for (p = Fortbuf; (ch = *p) != '\0'; ++p) {
						if (isascii(ch)) {
							if (isupper(ch))
								*p = 'A' + (ch - 'A' + 13) % 26;
							else if (islower(ch))
								*p = 'a' + (ch - 'a' + 13) % 26;
						}
					}
				if (RE_EXEC(Fortbuf)) {
					printf("%c%c", fp->tbl.str_delim,
					    fp->tbl.str_delim);
					if (!in_file) {
						printf(" (%s)", fp->name);
						Found_one = TRUE;
						in_file = TRUE;
					}
					putchar('\n');
					(void) fwrite(Fortbuf, 1, (sp - Fortbuf), stdout);
				}
				sp = Fortbuf;
			}
	}
}
# endif	/* NO_REGEX */

void
usage()
{
	(void) fprintf(stderr, "fortune [-a");
#ifdef	DEBUG
	(void) fprintf(stderr, "D");
#endif	/* DEBUG */
	(void) fprintf(stderr, "f");
#ifndef	NO_REGEX
	(void) fprintf(stderr, "i");
#endif	/* NO_REGEX */
	(void) fprintf(stderr, "losw]");
#ifndef	NO_REGEX
	(void) fprintf(stderr, " [-m pattern]");
#endif	/* NO_REGEX */
	(void) fprintf(stderr, "[[#%%] file/directory/all]\n");
	exit(1);
}
