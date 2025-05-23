#ifndef lint
#ifndef NOID
static char	elsieid[] = "@(#)scheck.c	8.15";
#endif /* !defined lint */
#endif /* !defined NOID */

/*
 * @(#)scheck.c	8.15
 * $FreeBSD: src/usr.sbin/zic/scheck.c,v 1.4 1999/08/28 01:21:19 peter Exp $
 * $DragonFly: src/usr.sbin/zic/scheck.c,v 1.3 2004/02/29 16:55:28 joerg Exp $
 */
/*LINTLIBRARY*/

#include "private.h"

char *
scheck(const char * const string, const char * const format)
{
	char *fbuf;
	const char *fp;
	char *tp;
	int c;
	char *result;
	char dummy;
	static char nada;

	result = &nada;
	if (string == NULL || format == NULL)
		return result;
	fbuf = imalloc((int) (2 * strlen(format) + 4));
	if (fbuf == NULL)
		return result;
	fp = format;
	tp = fbuf;
	while ((*tp++ = c = *fp++) != '\0') {
		if (c != '%')
			continue;
		if (*fp == '%') {
			*tp++ = *fp++;
			continue;
		}
		*tp++ = '*';
		if (*fp == '*')
			++fp;
		while (is_digit(*fp))
			*tp++ = *fp++;
		if (*fp == 'l' || *fp == 'h')
			*tp++ = *fp++;
		else if (*fp == '[')
			do *tp++ = *fp++;
				while (*fp != '\0' && *fp != ']');
		if ((*tp++ = *fp++) == '\0')
			break;
	}
	*(tp - 1) = '%';
	*tp++ = 'c';
	*tp = '\0';
	if (sscanf(string, fbuf, &dummy) != 1)
		result = (char *) format;
	ifree(fbuf);
	return result;
}
