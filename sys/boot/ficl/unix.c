/*
 * $FreeBSD: src/sys/boot/ficl/unix.c,v 1.1 2001/04/29 02:36:34 dcs Exp $
 * $DragonFly: src/sys/boot/ficl/unix.c,v 1.1 2003/11/10 06:08:33 dillon Exp $
 */

#include <string.h>
#include <netinet/in.h>

#include "ficl.h"



unsigned long ficlNtohl(unsigned long number)
	{
	return ntohl(number);
	}




void ficlCompilePlatform(FICL_DICT *dp)
{
    return;
}


