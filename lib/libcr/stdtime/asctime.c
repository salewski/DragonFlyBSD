/*
** This file is in the public domain, so clarified as of
** June 5, 1996 by Arthur David Olson (arthur_david_olson@nih.gov).
**
** $FreeBSD: src/lib/libc/stdtime/asctime.c,v 1.7.6.1 2001/03/05 11:37:20 obrien Exp $
** $DragonFly: src/lib/libcr/stdtime/Attic/asctime.c,v 1.3 2004/10/25 19:38:44 drhodus Exp $
*/

/*
 * @(#)asctime.c	7.7
 */
/*LINTLIBRARY*/

#include "private.h"
#include "tzfile.h"

/*
** A la X3J11, with core dump avoidance.
*/


char *
asctime(timeptr)
const struct tm *	timeptr;
{
	static char		result[3 * 2 + 5 * INT_STRLEN_MAXIMUM(int) +
					3 + 2 + 1 + 1];
	return(asctime_r(timeptr, result));
}

char *
asctime_r(timeptr, result)
const struct tm *	timeptr;
char *result;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	/*
	** Big enough for something such as
	** ??? ???-2147483648 -2147483648:-2147483648:-2147483648 -2147483648\n
	** (two three-character abbreviations, five strings denoting integers,
	** three explicit spaces, two explicit colons, a newline,
	** and a trailing ASCII nul).
	*/
	const char *	wn;
	const char *	mn;

	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];
	/*
	** The X3J11-suggested format is
	**	"%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n"
	** Since the .2 in 02.2d is ignored, we drop it.
	*/
	(void) sprintf(result, "%.3s %.3s%3d %02d:%02d:%02d %d\n",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		TM_YEAR_BASE + timeptr->tm_year);
	return result;
}
