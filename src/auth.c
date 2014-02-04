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
/* These strings are allocated. */
	char *host;
	char *directory;
	char *user_password;
	int port;
	eb_bool proxy;
	uchar realm;
};

static struct listHead authlist = { &authlist, &authlist };

eb_bool getUserPass(const char *url, char *creds, eb_bool find_proxy)
{
	const char *host = getHostURL(url);
	int port = getPortURL(url);
	const char *dir, *dirend;
	struct httpAuth *a;
	struct httpAuth *found = NULL;
	int l, d1len, d2len;

	getDirURL(url, &dir, &dirend);
	d2len = dirend - dir;

	foreach(a, authlist) {
		if (found == NULL && a->proxy == find_proxy &&
		    stringEqualCI(a->host, host) && a->port == port) {
			if (!a->proxy) {
/* Directory match not done for proxy records. */
				d1len = strlen(a->directory);
				if (d1len > d2len)
					continue;
				if (memcmp(a->directory, dir, d1len))
					continue;
				found = a;
			} else	/* not proxy */
				found = a;
		}
	}

	if (found)
		strcpy(creds, found->user_password);

	return (found != NULL);
}				/* getUserPass */

eb_bool
addWebAuthorization(const char *url,
		    int realm, const char *credentials, eb_bool proxy)
{
	struct httpAuth *a;
	const char *host;
	const char *dir = 0, *dirend;
	int port, dl;
	eb_bool urlProx = isProxyURL(url);
	eb_bool updated = eb_true;
	char *p;

	if (proxy) {
		if (!urlProx) {
			setError(MSG_ProxyAuth);
			return eb_false;
		}
	} else if (urlProx)
		url = getDataURL(url);

	host = getHostURL(url);
	port = getPortURL(url);
	if (!proxy) {
		getDirURL(url, &dir, &dirend);
		dl = dirend - dir;
	}

/* See if we've done this one before. */
	foreach(a, authlist) {
		if (a->proxy == proxy &&
		    a->port == port &&
		    a->realm == realm &&
		    stringEqualCI(a->host, host) &&
		    (proxy ||
		     dl == strlen(a->directory)
		     && !memcmp(a->directory, dir, dl))) {
			nzFree(a->user_password);
			break;
		}
	}

	if (a == (struct httpAuth *)&authlist) {
		updated = eb_false;
		a = allocZeroMem(sizeof(struct httpAuth));
		addToListFront(&authlist, a);
	}

	a->proxy = proxy;
	a->realm = realm;
	a->port = port;
	if (!a->host)
		a->host = cloneString(host);
	if (dir && !a->directory)
		a->directory = pullString1(dir, dirend);

	a->user_password = cloneString(credentials);
	debugPrint(3, "%s authorization for %s%s",
		   updated ? "updated" : "new", a->host, a->directory);
	return eb_true;
}				/* addWebAuthorization */
