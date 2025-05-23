/*
 * io.c - input/output routines for Phantasia
 *
 * $FreeBSD: src/games/phantasia/io.c,v 1.6 1999/11/16 02:57:33 billf Exp $
 * $DragonFly: src/games/phantasia/io.c,v 1.2 2003/06/17 04:25:24 dillon Exp $
 */

#include <string.h>
#include "include.h"

/************************************************************************
/
/ FUNCTION NAME: getstring()
/
/ FUNCTION: read a string from operator
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS:
/	char *cp - pointer to buffer area to fill
/	int mx - maximum number of characters to put in buffer
/
/ RETURN VALUE: none
/
/ MODULES CALLED: wmove(), _filbuf(), clearok(), waddstr(), wrefresh(),
/	wclrtoeol()
/
/ GLOBAL INPUTS: Echo, _iob[], Wizard, *stdscr
/
/ GLOBAL OUTPUTS: _iob[]
/
/ DESCRIPTION:
/	Read a string from the keyboard.
/	This routine is specially designed to:
/
/	    - strip non-printing characters (unless Wizard)
/	    - echo, if desired
/	    - redraw the screen if CH_REDRAW is entered
/	    - read in only 'mx - 1' characters or less characters
/	    - nul-terminate string, and throw away newline
/
/	'mx' is assumed to be at least 2.
/
*************************************************************************/

getstring(cp, mx)
char	*cp;
int	mx;
{
char	*inptr;		/* pointer into string for next string */
int	x, y;			/* original x, y coordinates on screen */
int	ch;			/* input */

    getyx(stdscr, y, x);	/* get coordinates on screen */
    inptr = cp;
    *inptr = '\0';		/* clear string to start */
    --mx;			/* reserve room in string for nul terminator */

    do
	/* get characters and process */
	{
	if (Echo)
	    mvaddstr(y, x, cp);	/* print string on screen */
	clrtoeol();		/* clear any data after string */
	refresh();		/* update screen */

	ch = getchar();		/* get character */

	switch (ch)
	    {
	    case CH_ERASE:	/* back up one character */
		if (inptr > cp)
		    --inptr;
		break;

	    case CH_KILL:	/* back up to original location */
		inptr = cp;
		break;

	    case CH_NEWLINE:	/* terminate string */
		break;

	    case CH_REDRAW:	/* redraw screen */
		clearok(stdscr, TRUE);
		continue;

	    default:		/* put data in string */
		if (ch >= ' ' || Wizard)
		    /* printing char; put in string */
		    *inptr++ = ch;
	    }

	*inptr = '\0';		/* terminate string */
	}
    while (ch != CH_NEWLINE && inptr < cp + mx);
}
/**/
/************************************************************************
/
/ FUNCTION NAME: more()
/
/ FUNCTION: pause and prompt player
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS:
/	int where - line on screen on which to pause
/
/ RETURN VALUE: none
/
/ MODULES CALLED: wmove(), waddstr(), getanswer()
/
/ GLOBAL INPUTS: *stdscr
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Print a message, and wait for a space character.
/
*************************************************************************/

more(where)
int	where;
{
    mvaddstr(where, 0, "-- more --");
    getanswer(" ", FALSE);
}
/**/
/************************************************************************
/
/ FUNCTION NAME: infloat()
/
/ FUNCTION: input a floating point number from operator
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS: none
/
/ RETURN VALUE: floating point number from operator
/
/ MODULES CALLED: sscanf(), getstring()
/
/ GLOBAL INPUTS: Databuf[]
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Read a string from player, and scan for a floating point
/	number.
/	If no valid number is found, return 0.0.
/
*************************************************************************/

double
infloat()
{
double	result;		/* return value */

    getstring(Databuf, SZ_DATABUF);
    if (sscanf(Databuf, "%lf", &result) < 1)
	/* no valid number entered */
	result = 0.0;

    return(result);
}
/**/
/************************************************************************
/
/ FUNCTION NAME: inputoption()
/
/ FUNCTION: input an option value from player
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: floor(), drandom(), getanswer()
/
/ GLOBAL INPUTS: Player
/
/ GLOBAL OUTPUTS: Player
/
/ DESCRIPTION:
/	Age increases with every move.
/	Refresh screen, and get a single character option from player.
/	Return a random value if player's ring has gone bad.
/
*************************************************************************/

inputoption()
{
    ++Player.p_age;		/* increase age */

    if (Player.p_ring.ring_type != R_SPOILED)
	/* ring ok */
	return(getanswer("T ", TRUE));
    else
	/* bad ring */
	{
	getanswer(" ", TRUE);
	return((int) ROLL(0.0, 5.0) + '0');
	}
}
/**/
/************************************************************************
/
/ FUNCTION NAME: interrupt()
/
/ FUNCTION: handle interrupt from operator
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: fork(), exit(), wait(), death(), alarm(), execl(), wmove(),
/	getgid(), signal(), getenv(), wclear(), setuid(), getuid(), setgid(),
/	crmode(), clearok(), waddstr(), cleanup(), wrefresh(), leavegame(),
/	getanswer()
/
/ GLOBAL INPUTS: Player, *stdscr
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Allow player to quit upon hitting the interrupt key.
/	If the player wants to quit while in battle, he/she automatically
/	dies.
/
*************************************************************************/

interrupt()
{
char	line[81];		/* a place to store data already on screen */
int	loop;		/* counter */
int	x, y;			/* coordinates on screen */
int	ch;			/* input */
unsigned	savealarm;	/* to save alarm value */

#ifdef SYS3
    signal(SIGINT, SIG_IGN);
#endif
#ifdef SYS5
    signal(SIGINT, SIG_IGN);
#endif

    savealarm = alarm(0);		/* turn off any alarms */

    getyx(stdscr, y, x);		/* save cursor location */

    for (loop = 0; loop < 80; ++loop)	/* save line on screen */
	{
	move(4, loop);
	line[loop] = inch();
	}
    line[80] = '\0';			/* nul terminate */

    if (Player.p_status == S_INBATTLE || Player.p_status == S_MONSTER)
	/* in midst of fighting */
	{
	mvaddstr(4, 0, "Quitting now will automatically kill your character.  Still want to ? ");
	ch = getanswer("NY", FALSE);
	if (ch == 'Y')
	    death("Bailing out");
	    /*NOTREACHED*/
	}
    else
	{
	mvaddstr(4, 0, "Do you really want to quit ? ");
	ch = getanswer("NY", FALSE);
	if (ch == 'Y')
	    leavegame();
	    /*NOTREACHED*/
	}

    mvaddstr(4, 0, line); 		/* restore data on screen */
    move(y, x);				/* restore cursor */
    refresh();

#ifdef SYS3
    signal(SIGINT, interrupt);
#endif
#ifdef SYS5
    signal(SIGINT, interrupt);
#endif

    alarm(savealarm);			/* restore alarm */
}
/**/
/************************************************************************
/
/ FUNCTION NAME: getanswer()
/
/ FUNCTION: get an answer from operator
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS:
/	char *choices - string of (upper case) valid choices
/	bool def - set if default answer
/
/ RETURN VALUE: none
/
/ MODULES CALLED: alarm(), wmove(), waddch(), signal(), setjmp(), strchr(),
/	_filbuf(), clearok(), toupper(), wrefresh(), mvprintw(), wclrtoeol()
/
/ GLOBAL INPUTS: catchalarm(), Echo, _iob[], _ctype[], *stdscr, Timeout,
/	Timeoenv[]
/
/ GLOBAL OUTPUTS: _iob[]
/
/ DESCRIPTION:
/	Get a single character answer from operator.
/	Timeout waiting for response.  If we timeout, or the
/	answer in not in the list of valid choices, print choices,
/	and wait again, otherwise return the first character in ths
/	list of choices.
/	Give up after 3 tries.
/
*************************************************************************/

getanswer(choices, def)
char	*choices;
bool	def;
{
int	ch;			/* input */
int	loop;			/* counter */
int	oldx, oldy;		/* original coordinates on screen */

    getyx(stdscr, oldy, oldx);
    alarm(0);				/* make sure alarm is off */

    for (loop = 3; loop; --loop)
	/* try for 3 times */
	{
	if (setjmp(Timeoenv) != 0)
	    /* timed out waiting for response */
	    {
	    if (def || loop <= 1)
		/* return default answer */
		break;
	    else
		/* prompt, and try again */
		goto YELL;
	    }
	else
	    /* wait for response */
	    {
	    clrtoeol();
	    refresh();
#ifdef BSD41
	    sigset(SIGALRM, catchalarm);
#else
	    signal(SIGALRM, catchalarm);
#endif
	    /* set timeout */
	    if (Timeout)
		alarm(7);		/* short */
	    else
		alarm(600);		/* long */

	    ch = getchar();

	    alarm(0);			/* turn off timeout */

	    if (ch < 0)
		/* caught some signal */
		{
		++loop;
		continue;
		}
	    else if (ch == CH_REDRAW)
		/* redraw screen */
		{
		clearok(stdscr, TRUE);	/* force clear screen */
		++loop;			/* don't count this input */
		continue;
		}
	    else if (Echo)
		{
		addch(ch);		/* echo character */
		refresh();
		}

	    if (islower(ch))
		/* convert to upper case */
		ch = toupper(ch);

	    if (def || strchr(choices, ch) != NULL)
		/* valid choice */
		return(ch);
	    else if (!def && loop > 1)
		/* bad choice; prompt, and try again */
		{
YELL:		mvprintw(oldy + 1, 0, "Please choose one of : [%s]\n", choices);
		move(oldy, oldx);
		clrtoeol();
		continue;
		}
	    else
		/* return default answer */
		break;
	    }
	}

    return(*choices);
}
/**/
/************************************************************************
/
/ FUNCTION NAME: catchalarm()
/
/ FUNCTION: catch timer when waiting for input
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: longjmp()
/
/ GLOBAL INPUTS: Timeoenv[]
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Come here when the alarm expires while waiting for input.
/	Simply longjmp() into getanswer().
/
*************************************************************************/

void
catchalarm()
{
    longjmp(Timeoenv, 1);
}
