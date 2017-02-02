/*********************************************************************
cache.c: maintain a cache of the http files.
The url is the key.
The result is a string that holds a 5 digit filename, the etag,
fetch time, and last access time.
nnnnn tab etag tab fetch tab access
The access time helps us clean house; delete the oldest files.
cacheDir is the directory holding the cached files,
and cacheControl is the file that houses the database,
either home grown or in some db format.
We might use a database library, if it allows separate edbrowse processes to
access the cache consistently.
Or I could home grow it; I'd just have to open a lock file with O_EXCL when
accessing the cache, and if busy then wait a few milliseconds and try again.
For now I'm rolling my own.
If the stored etag and header etag are both present, and don't match,
then the file is stale.
If one or the other etag is missing, and mod date > fetch date,
then the file is stale.
We don't even query the cache if we don't have at least one of etag or mod date.
*********************************************************************/

#include "eb.h"

static int control_fh = -1;	/* file handle for cacheControl */
static char *cache_data;
static time_t now_t;

/* a cache entry */
struct CENTRY {
	off_t offset;
	const char *url;
	int filenumber;
	const char *etag;
/* time is in tens of seconds past 1970, good for a while. */
	unsigned int fetchtime;
	unsigned int accesstime;
};

#define MAXCACHESIZE 2000

static struct CENTRY entries[MAXCACHESIZE];
static int numentries;

/*********************************************************************
Read the control file into memory and parse it into entry structures.
Sadly, I do this every time you access the cache.
It would be better to hold all this data in memory, with the time stamp of the
control file, and if the control file has not been updated
then just use what we have;
and if it has been updated then read it and parse it.
Well maybe we'll implement this later.
For now, the control file isn't too big, it's not prohibitive
to do this every time.
Note that control is a nice ascii readable file, helps with debugging.
*********************************************************************/

static bool readControl(void)
{
	char *s, *t, *endfile;
	char *data;
	int datalen;
	struct CENTRY *e;

	lseek(control_fh, 0L, 0);
	if (!fdIntoMemory(control_fh, &data, &datalen))
		return false;

	numentries = 0;
	e = entries;
	endfile = data + datalen;
	for (s = data; s != endfile; s = t) {
		t = strchr(s, '\n') + 1;
		e->offset = s - data;
		e->url = s;
		s = strchr(s, '\t');
		*s++ = 0;
		e->filenumber = strtol(s, &s, 10);
		++s;
		e->etag = s;
		s = strchr(s, '\t');
		*s++ = 0;
		sscanf(s, "%u %u", &e->fetchtime, &e->accesstime);
		++e, ++numentries;
	}

	cache_data = data;	/* remember to free this later */
	return true;
}				/* readControl */

/* create an ascii equivalent for a record, this is allocated */
static char *record2string(const struct CENTRY *e)
{
	char *t;
	asprintf(&t, "%s\t%05d\t%s\t%u\t%u\n",
		 e->url, e->filenumber, e->etag, e->fetchtime, e->accesstime);
	return t;
}				/* record2string */

/* ON a rare occasion we will have to rewrite the entire control file.
 * If this fails, and it shouldn't, then our only recourse is to clear the cache.
 * If successful, then the file is closed. */
static bool writeControl(void)
{
	struct CENTRY *e;
	int i;
	FILE *f;

	ftruncate(control_fh, 0);
	lseek(control_fh, 0L, 0);
/* buffered IO is more efficient */
	f = fdopen(control_fh, "w");

	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		char *newrec = record2string(e);
		int rc = fprintf(f, "%s", newrec);
		free(newrec);
		if (rc <= 0) {
			fclose(f);
			control_fh = -1;
			truncate(cacheControl, 0);
			return false;
		}
	}

	fclose(f);
	control_fh = -1;
	return true;
}				/* writeControl */

/* create a file number to fold into the file name.
 * This is chosen at random. At worst we should get
 * an unused number in 2 or 3 tries. */
static int generateFileNumber(void)
{
	struct CENTRY *e;
	int i, n;

	while (true) {
		n = rand() % 100000;
		e = entries;
		for (i = 0; i < numentries; ++i, ++e)
			if (e->filenumber == n)
				break;
		if (i == numentries)
			return n;
	}
}				/* generateFileNumber */

/* get exclusive access to the cache */
static bool setLock(void)
{
	int i;
	int lock_fh;
	time_t lock_t;

	if (!cacheDir)
		return false;

top:
	time(&now_t);

/* try every 5 ms, 200 times, for a total of 1 second */
	for (i = 0; i < 200; ++i) {
		lock_fh = open(cacheLock, O_WRONLY | O_EXCL | O_CREAT, 0666);
		if (lock_fh >= 0) {	/* got it */
			close(lock_fh);
			if (control_fh < 0) {
				control_fh =
				    open(cacheControl, O_RDWR | O_BINARY, 0);
				if (control_fh < 0) {
/* got the lock but couldn't open the database */
					unlink(cacheLock);
					return false;
				}
			}
			if (!readControl()) {
				unlink(cacheLock);
				return false;
			}
			return true;
		}
		if (errno != EEXIST)
			return false;
		usleep(5000);
	}

/* if lock file is more than an hour old then something bad has happened,
 * just remove it. */
	lock_t = fileTimeByName(cacheLock);
	if (now_t - lock_t > 3600) {
		if (unlink(cacheLock) == 0)
			goto top;
	}

	return false;
}				/* setLock */

static void clearLock(void)
{
	unlink(cacheLock);
}				/* clearLock */

/* Remove any cached files and initialize the database */
static void clearCacheInternal(void)
{
	struct CENTRY *e;
	int i;

/* loop through and remove the files */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
		unlink(cacheFile);
	}

	ftruncate(control_fh, 0);
}				/* clearCacheInternal */

void clearCache(void)
{
	if (!setLock())
		return;
	clearCacheInternal();
	free(cache_data);
	clearLock();
}				/* clearCache */

/* Fetch a file from cache. return true if fetched successfully,
false if the file has not been cached or is stale.
If true then the last access time is set to now. */
bool fetchCache(const char *url, const char *etag, time_t modtime,
		char **data, int *data_len)
{
	struct CENTRY *e;
	int i;
	char *newrec;

/* you have to give me enough information */
	if (!modtime && (!etag || !*etag))
		return false;

	if (!setLock())
		return false;

/* find the url */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		if (!sameURL(url, e->url))
			continue;
/* look for match on etag */
		if (e->etag[0] && etag && etag[0]) {
/* both etags are present */
			if (stringEqual(etag, e->etag))
				goto match;
			goto nomatch;
		}
		if (!modtime)
			goto nomatch;
		if (modtime > e->fetchtime)
			goto nomatch;
		goto match;
	}
/* url not found */

nomatch:
	free(cache_data);
	clearLock();
	return false;

match:
	sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
	if (!fileIntoMemory(cacheFile, data, data_len))
		goto nomatch;
/* file has been pulled from cache */
/* have to update the access time */
	e->accesstime = now_t / 10;
	newrec = record2string(e);
	lseek(control_fh, e->offset, 0);
	write(control_fh, newrec, strlen(newrec));
	free(cache_data);
	free(newrec);
	clearLock();
	return true;
}				/* fetchCache */

/* for quicksort */
/* records sorted by access time in reverse order */
static int entry_cmp(const void *s, const void *t)
{
	return ((struct CENTRY *)t)->accesstime -
	    ((struct CENTRY *)s)->accesstime;
}				/* entry_cmp */

/*
 * Is a URL present in the cache?  This can save on HEAD requests,
 * since we can just do a straight GET if the item is not there.
 */
bool presentInCache(const char *url)
{
	bool ret = false;
	struct CENTRY *e;
	int i;

	if (!setLock())
		return false;

	e = entries;

	for (i = 0; (!ret && i < numentries); ++i, ++e) {
		if (!sameURL(url, e->url))
			continue;
		ret = true;
	}

	clearLock();
	return ret;
}				/* presentInCache */

/* Put a file into the cache.
 * Sets the fetch time and last access time to now.
 * If you ask me to store a file that I just fetched, I'll do it,
 * but it's a waste, so don't go there. */
void storeCache(const char *url, const char *etag, const char *data,
		int datalen)
{
	struct CENTRY *e;
	int i;
	int filenum;
	bool rewrite;

	if (!setLock())
		return;

/* find the url */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		if (sameURL(url, e->url))
			break;
	}

	if (i < numentries)
		filenum = e->filenumber;
	else
		filenum = generateFileNumber();
	sprintf(cacheFile, "%s/%05d", cacheDir, filenum);
	if (!memoryOutToFile(cacheFile, data, datalen,
			     MSG_TempNoCreate2, MSG_NoWrite2)) {
/* oops, can't write the file */
		unlink(cacheFile);
		free(cache_data);
		clearLock();
		return;
	}

	if (i < numentries) {
/* we're just updating a preexisting record */
		rewrite = false;
		e->fetchtime = e->accesstime = now_t / 10;
		if (etag && *etag) {
			if (strlen(etag) != strlen(e->etag))
				rewrite = true;
			e->etag = etag;
		}
		if (!rewrite) {
/* record is the same length, just update it */
			char *newrec = record2string(e);
			lseek(control_fh, e->offset, 0);
			write(control_fh, newrec, strlen(newrec));
			free(cache_data);
			free(newrec);
			clearLock();
			return;
		}

/* etag has changed length, have to rewrite the whole control file */
		if (!writeControl())
			clearCacheInternal();
		free(cache_data);
		clearLock();
		return;
	}

/* this file is new. See if the database is full. */
	rewrite = false;
	if (numentries == MAXCACHESIZE) {
		rewrite = true;
/* sort to find the 100 oldest files */
		qsort(entries, numentries, sizeof(struct CENTRY), entry_cmp);
		e = entries + numentries - 100;
		for (i = 0; i < 100; ++i, ++e) {
			sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
			unlink(cacheFile);
		}
		numentries -= 100;
	}

	e = entries + numentries;
	++numentries;
	e->url = url;
	e->filenumber = filenum;
	e->etag = (etag ? etag : emptyString);
	e->fetchtime = e->accesstime = now_t / 10;

	if (!rewrite) {
/* didn't have to prune; just append this record */
		char *newrec = record2string(e);
		lseek(control_fh, 0L, 2);
		write(control_fh, newrec, strlen(newrec));
		free(cache_data);
		free(newrec);
		clearLock();
		return;
	}

/* have to rewrite the whole control file */
	if (!writeControl())
		clearCacheInternal();
	free(cache_data);
	clearLock();
}				/* storeCache */
