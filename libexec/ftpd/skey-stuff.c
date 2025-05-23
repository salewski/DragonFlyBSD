/* Author: Wietse Venema, Eindhoven University of Technology. 
 *
 * $FreeBSD: src/libexec/ftpd/skey-stuff.c,v 1.12 1999/08/28 00:09:32 peter Exp $
 * $DragonFly: src/libexec/ftpd/skey-stuff.c,v 1.2 2003/06/17 04:27:07 dillon Exp $
 */

#include <stdio.h>
#include <string.h>
#include <pwd.h>

#include <skey.h>

/* skey_challenge - additional password prompt stuff */

char   *skey_challenge(name, pwd, pwok)
char   *name;
struct passwd *pwd;
int    pwok;
{
    static char buf[128];
    struct skey skey;

    /* Display s/key challenge where appropriate. */

    *buf = '\0';
    if (pwd == NULL || skeychallenge(&skey, pwd->pw_name, buf))
	snprintf(buf, sizeof(buf), "Password required for %s.", name);
    else if (!pwok)
	strcat(buf, " (s/key required)");
    return (buf);
}
