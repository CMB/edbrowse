/* auth.c
 * user password authorization for web access
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL.
 *
 * Modified by Karl Dahlke for integration with edbrowse.
 */

#include "eb.h"

#define REALM_BASIC 1
#define REALM_MD5 2

static const char *const realmDesc[] = {
    "", "Basic ", "Digest "
};

struct httpAuth {
    struct httpAuth *next;
    struct httpAuth *prev;
    char user[MAXUSERPASS];
    char password[MAXUSERPASS];
/* These strings are allocated. */
    char *host;
    char *directory;
    char *user_password_encoded;
    int port;
    bool proxy;
    uchar realm;
};

static struct listHead authlist = { &authlist, &authlist };

/* This string is included in the outgoing http header.
 * It could include both a proxy and a host authorization.
 * Not that I understand any of the proxy stuff. */
char *
getAuthString(const char *url)
{
    const char *host = getHostURL(url);
    int port = getPortURL(url);
    const char *dir, *dirend;
    struct httpAuth *a;
    char *r = NULL;
    int l, d1len, d2len;

    if(isProxyURL(url)) {
	foreach(a, authlist) {
	    if(a->proxy && stringEqualCI(a->host, host) && a->port == port) {
		r = initString(&l);
		stringAndString(&r, &l, "Proxy-Authorization: ");
		stringAndString(&r, &l, realmDesc[a->realm]);
		stringAndString(&r, &l, a->user_password_encoded);
		stringAndString(&r, &l, eol);
	    }
	}

/* Skip past the proxy directive */
	url = getDataURL(url);
	host = getHostURL(url);
	port = getPortURL(url);
    }
    /* proxy */
    getDirURL(url, &dir, &dirend);
    d2len = dirend - dir;

    foreach(a, authlist) {
	if(!a->proxy && stringEqualCI(a->host, host) && a->port == port) {
	    d1len = strlen(a->directory);
	    if(d1len > d2len)
		continue;
	    if(memcmp(a->directory, dir, d1len))
		continue;
	    if(!r)
		r = initString(&l);
	    stringAndString(&r, &l, "Authorization: ");
	    stringAndString(&r, &l, realmDesc[a->realm]);
	    stringAndString(&r, &l, a->user_password_encoded);
	    stringAndString(&r, &l, eol);
	}
    }

    return r;
}				/* getAuthString */

bool
addWebAuthorization(const char *url,
   int realm, const char *user, const char *password, bool proxy)
{
    struct httpAuth *a;
    const char *host;
    const char *dir = 0, *dirend;
    int port, dl;
    bool urlProx = isProxyURL(url);
    bool updated = true;
    char *p;

    if(proxy) {
	if(!urlProx) {
	    setError(MSG_PROXYAUTH);
	    return false;
	}
    } else if(urlProx)
	url = getDataURL(url);

    host = getHostURL(url);
    port = getPortURL(url);
    if(!proxy) {
	getDirURL(url, &dir, &dirend);
	dl = dirend - dir;
    }

/* See if we've done this one before. */
    foreach(a, authlist) {
	if(a->proxy == proxy &&
	   a->port == port &&
	   a->realm == realm &&
	   stringEqualCI(a->host, host) &&
	   (proxy ||
	   dl == strlen(a->directory) && !memcmp(a->directory, dir, dl))) {
	    nzFree(a->user_password_encoded);
	    break;
	}
    }

    if(a == (struct httpAuth *)&authlist) {
	updated = false;
	a = allocZeroMem(sizeof (struct httpAuth));
	addToListFront(&authlist, a);
    }

    a->proxy = proxy;
    a->realm = realm;
    a->port = port;
    if(!a->host)
	a->host = cloneString(host);
    strcpy(a->user, user);
    strcpy(a->password, password);
    if(dir && !a->directory)
	a->directory = pullString1(dir, dirend);

/* Now compute the user password encoded */
    p = allocMem(strlen(user) + strlen(password) + 2);
    strcpy(p, user);
    strcat(p, ":");
    strcat(p, password);
    a->user_password_encoded = base64Encode(p, strlen(p), false);
    free(p);
    debugPrint(3, "%s authorization for %s%s",
       updated ? "updated" : "new", a->host, a->directory);
}				/* addWebAuthorization */
