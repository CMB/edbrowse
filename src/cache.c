/*********************************************************************
cache.c: maintain a cache of the http files.
The url is the key.
The result is a string that holds a 5 digit filename, the etag,
last modified time, last access time, and file size.
nnnnn tab etag tab last-mod tab access tab size
The access time helps us clean house; delete the oldest files.
If you change the format of this file in any way, increment the version number.
Previous cache files will be left hanging around, but oh well.
Not expecting to change this file format very often.
cacheDir is the directory holding the cached files,
and cacheControl is the file that houses the database.
I open a lock file with O_EXCL when accessing the cache,
and if busy then I wait a few milliseconds and try again.
If the stored etag and header etag are both present, and don't match,
then the file is stale.
If one or the other etag is missing, and mod time website > mod time cached,
then the file is stale.
We don't even query the cache if we don't have at least one of etag or mod time.
*********************************************************************/

#include "eb.h"

#define CACHECONTROLVERSION 1

#ifdef DOSLIKE
#define USLEEP(a) Sleep(a / 1000)	// sleep millisecs
#else
#define USLEEP(a) usleep(a)	// sleep microsecs
#endif

static int control_fh = -1;	/* file handle for cacheControl */
static char *cache_data;
static time_t now_t;
static char *cacheFile, *cacheLock, *cacheControl;

/* a cache entry */
struct CENTRY {
	off_t offset;
	size_t textlength;
	const char *url;
	int filenumber;
	const char *etag;
	int modtime;
	int accesstime;
	int pages;		/* in 4K pages */
};

static struct CENTRY *entries;
static int numentries;

void setupEdbrowseCache(void)
{
	int fh;

	if (control_fh >= 0) {
		close(control_fh);
		control_fh = -1;
	}
#ifdef DOSLIKE
	if (!cacheDir) {
		if (!ebUserDir)
			return;
		cacheDir = allocMem(strlen(ebUserDir) + 7);
		sprintf(cacheDir, "%s/cache", ebUserDir);
	}
	if (fileTypeByName(cacheDir, false) != 'd') {
		if (mkdir(cacheDir, 0700)) {
/* Don't want to abort here; we might be on a readonly filesystem.
 * Don't have a cache directory and can't creat one; yet we should move on. */
			free(cacheDir);
			cacheDir = 0;
			return;
		}
	}
#else
	if (!cacheDir) {
		cacheDir = allocMem(strlen(home) + 10);
		sprintf(cacheDir, "%s/.ebcache", home);
	}
	if (fileTypeByName(cacheDir, false) != 'd') {
		if (mkdir(cacheDir, 0700)) {
/* Don't want to abort here; we might be on a readonly filesystem.
 * Don't have a cache directory and can't creat one; yet we should move on. */
			free(cacheDir);
			cacheDir = 0;
			return;
		}
	}
#endif

/* the cache control file, which urls go to which files, and when fetched? */
	nzFree(cacheControl);
	cacheControl = allocMem(strlen(cacheDir) + 11);
	sprintf(cacheControl, "%s/control%02d", cacheDir, CACHECONTROLVERSION);
/* make sure the control file exists, just for grins */
	fh = open(cacheControl, O_WRONLY | O_APPEND | O_CREAT, MODE_private);
	if (fh >= 0)
		close(fh);

	nzFree(cacheLock);
	cacheLock = allocMem(strlen(cacheDir) + 6);
	sprintf(cacheLock, "%s/lock", cacheDir);

	nzFree(cacheFile);
	cacheFile = allocMem(strlen(cacheDir) + 7);

	nzFree(entries);
	entries = allocMem(cacheCount * sizeof(struct CENTRY));
}				/* setupEdbrowseCache */

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
	int ln = 1;

	lseek(control_fh, 0L, 0);
	if (!fdIntoMemory(control_fh, &data, &datalen))
		return false;

	numentries = 0;
	e = entries;
	endfile = data + datalen;
	for (s = data; s != endfile; s = t, ++ln) {
		t = strchr(s, '\n');
		if (!t) {
/* file does not end in newline; this should never happen! */
/* Not sure what to do, but at least it's not a seg fault. */
			break;
		}
		++t;
		e->offset = s - data;
		e->textlength = t - s;
		e->url = s;
		s = strchr(s, '\t');
		if (!s || s >= t) {
			debugPrint(3, "cache control file line %d is bogus",
				   ln);
			continue;
		}
		*s++ = 0;
		e->filenumber = strtol(s, &s, 10);
		++s;
		e->etag = s;
		s = strchr(s, '\t');
		if (!s || s >= t) {
			debugPrint(3, "cache control file line %d is bogus",
				   ln);
			continue;
		}
		*s++ = 0;
		sscanf(s, "%d %d %d", &e->modtime, &e->accesstime, &e->pages);
		++e, ++numentries;
	}

	cache_data = data;	/* remember to free this later */
	return true;
}				/* readControl */

/* create an ascii equivalent for a record, this is allocated */
static char *record2string(const struct CENTRY *e)
{
	char *t;
	asprintf(&t, "%s\t%05d\t%s\t%d\t%d\t%d\n",
		 e->url, e->filenumber, e->etag, e->modtime, e->accesstime,
		 e->pages);
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

	lseek(control_fh, 0L, 0);
	truncate0(cacheControl, control_fh);
/* buffered IO is more efficient */
	f = fdopen(control_fh, "w");

	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		int rc;
		char *newrec = record2string(e);
		e->textlength = strlen(newrec);
		rc = fprintf(f, "%s", newrec);
		free(newrec);
		if (rc <= 0) {
			fclose(f);
			control_fh = -1;
			truncate0(cacheControl, -1);
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
	if (!cacheSize)
		return false;

top:
	time(&now_t);

/* try every 10 ms, 100 times, for a total of 1 second */
	for (i = 0; i < 100; ++i) {
		lock_fh =
		    open(cacheLock, O_WRONLY | O_EXCL | O_CREAT, MODE_private);
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
		USLEEP(10000);
	}

/* if lock file is more than 5 minutes old then something bad has happened,
 * just remove it. */
	lock_t = fileTimeByName(cacheLock);
	if (now_t - lock_t > 5 * 60) {
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

	debugPrint(3, "clear cache");

/* loop through and remove the files */
	e = entries;
	for (i = 0; i < numentries; ++i, ++e) {
		sprintf(cacheFile, "%s/%05d", cacheDir, e->filenumber);
		unlink(cacheFile);
	}

	truncate0(cacheControl, -1);
}				/* clearCacheInternal */

// This function is not used and has not been tested.
// Maybe some day it will be invoked from an edbrowse command.
void clearCache(void)
{
	if (!setLock())
		return;
	close(control_fh);
	control_fh = -1;
	clearCacheInternal();
	free(cache_data);
	clearLock();
}				/* clearCache */

/* Fetch a file from cache. return true if fetched successfully,
false if the file has not been cached or is stale.
If true then the last access time is set to now.
The data is returned by the pointer provided; if there is no pointer
for the length of the data, then the name of the cache file is returned instead,
wherein the calling routine can access the file directly.
You might think there is a race condition here; some other edbrowse
process fills the cache and removes 100 files, but this file was just accessed,
so is at the top of the list, and won't be removed.
In other words, a destructive race condition is almost impossible. Some goofy
characters are prepended to the filename to help us identify it as such. */

bool fetchCache(const char *url, const char *etag, time_t modtime,
		char **data, int *data_len)
{
	struct CENTRY *e;
	int i;
	char *newrec;
	size_t newlen = 0;

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
		if (modtime / 8 > e->modtime)
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
	if (data_len) {
		if (!fileIntoMemory(cacheFile, data, data_len))
			goto nomatch;
	} else {
		char *a = allocMem(strlen(cacheFile) + 5 + 1);
		sprintf(a, "`cfn~%s", cacheFile);
		*data = a;
	}

/* file has been pulled from cache */
/* have to update the access time */
	e->accesstime = now_t / 8;
	newrec = record2string(e);
	newlen = strlen(newrec);
	if (newlen == e->textlength) {
		lseek(control_fh, e->offset, 0);
		write(control_fh, newrec, newlen);
	} else {
		if (!writeControl())
			clearCacheInternal();
	}

	debugPrint(3, "from cache");
	free(newrec);
	free(cache_data);
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

	free(cache_data);
	clearLock();
	return ret;
}				/* presentInCache */

/* Put a file into the cache.
 * Sets the modified time and last access time to now.
 * Time is in 8 second chunks, so even a 32 bit int will hold us for centuries. */

void storeCache(const char *url, const char *etag, time_t modtime,
		const char *data, int datalen)
{
	struct CENTRY *e;
	int i;
	int filenum;
	bool append = false;

	if (!setLock())
		return;

/* leading http:// is the default, and not needed in the control file.
 * sameURL() takes care of all that. */
	if (memEqualCI(url, "http://", 7))
		url += 7;

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
		debugPrint(3, "cannot write web page into cache");
		free(cache_data);
		clearLock();
		return;
	}

	if (i < numentries) {
		char *newrec;
		size_t newlen;
/* we're just updating a preexisting record */
		e->accesstime = now_t / 8;
		e->modtime = modtime / 8;
		e->etag = (etag ? etag : emptyString);
		e->pages = (datalen + 4095) / 4096;
		newrec = record2string(e);
		newlen = strlen(newrec);
		if (newlen == e->textlength) {
/* record is the same length, just update it */
			lseek(control_fh, e->offset, 0);
			write(control_fh, newrec, newlen);
			debugPrint(3, "into cache");
			free(cache_data);
			free(newrec);
			clearLock();
			return;
		}

/* Record has changed length, have to rewrite the whole control file */
		e->textlength = newlen;
		if (!writeControl())
			clearCacheInternal();
		else
			debugPrint(3, "into cache");
		free(cache_data);
		clearLock();
		return;
	}

/* this file is new. See if the database is full. */
	append = true;
	if (numentries >= 140) {
		int npages = 0;
		e = entries;
		for (i = 0; i < numentries; ++i, ++e)
			npages += e->pages;

		if (numentries == cacheCount || npages / 256 >= cacheSize) {
/* sort to find the 100 oldest files */
			qsort(entries, numentries, sizeof(struct CENTRY),
			      entry_cmp);
			debugPrint(3,
				   "cache is full; removing the 100 oldest files");
			e = entries + numentries - 100;
			for (i = 0; i < 100; ++i, ++e) {
				sprintf(cacheFile, "%s/%05d", cacheDir,
					e->filenumber);
				unlink(cacheFile);
			}
			numentries -= 100;
			append = false;
		}
	}

	e = entries + numentries;
	++numentries;
	e->url = url;
	e->filenumber = filenum;
	e->etag = (etag ? etag : emptyString);
	e->accesstime = now_t / 8;
	e->modtime = modtime / 8;
	e->pages = (datalen + 4095) / 4096;

	if (append) {
/* didn't have to prune; just append this record */
		char *newrec = record2string(e);
		e->textlength = strlen(newrec);
		lseek(control_fh, 0L, 2);
		write(control_fh, newrec, e->textlength);
		debugPrint(3, "into cache");
		free(cache_data);
		free(newrec);
		clearLock();
		return;
	}

/* have to rewrite the whole control file */
	if (!writeControl())
		clearCacheInternal();
	else
		debugPrint(3, "into cache");
	free(cache_data);
	clearLock();
}				/* storeCache */
