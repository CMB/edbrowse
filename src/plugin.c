/*********************************************************************
plugin.c: mime types and plugins.
Run audio players, pdf converters, etc, based on suffix or content-type.
*********************************************************************/

#include "eb.h"

#ifdef DOSLIKE
#include <process.h>		// for _getpid(),...
#define getpid _getpid
#endif

/* create an input or an output file for edbrowse under /tmp.
 * Since an external program may act upon this file, a certain suffix
 * may be required.
 * Fails if /tmp/.edbrowse does not exist or cannot be created. */
static char *tempin, *tempout;

static bool makeTempFilename(const char *suffix, int idx, bool output)
{
	char *filename;

// if no temp directory then we can't proceed
	if (!ebUserDir) {
		setError(MSG_TempNone);
		return false;
	}

	if (!suffix)
		suffix = "eb";
	if (asprintf(&filename, "%s/pf%d-%d.%s",
		     ebUserDir, getpid(), idx, suffix) < 0)
		i_printfExit(MSG_MemAllocError, strlen(ebUserDir) + 24);

	if (output) {
// free the last one, don't need it any more.
		nzFree(tempout);
		tempout = filename;
	} else {
		nzFree(tempin);
		tempin = filename;
	}

	return true;
}				/* makeTempFilename */

static int tempIndex;

static const struct MIMETYPE *findMimeBySuffix(const char *suffix)
{
	int i;
	int len = strlen(suffix);
	const struct MIMETYPE *m = mimetypes;

	if (!len)
		return NULL;

	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->suffix, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, suffix, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeBySuffix */

static char *file2suffix(const char *filename)
{
	static char suffix[12];
	const char *post, *s;
	post = filename + strlen(filename);
	for (s = post - 1; s >= filename && *s != '.' && *s != '/'; --s) ;
	if (s < filename || *s != '.')
		return NULL;
	++s;
	if (post >= s + sizeof(suffix))
		return NULL;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	return suffix;
}

static char *url2suffix(const char *url)
{
	static char suffix[12];
	const char *post, *s;

/*********************************************************************
We need to skip past protocol and host, suffix should not be the suffix on the host.
Example http://foo.bar.mobi when you have a suffix = mobi plugin.
But the urls we create for our own purposes don't look like protocol://host/file
They are what I call free_syntax in url.c.
See the plugins and protocols for zip files in the edbrowse wiki.
So I only run this check for the 3 recognized transport protocols
that might bring data into the edbrowse buffer.
The various ftp protocols all download data to files
and don't run plugins at all. We don't have to check for those.
*********************************************************************/

	if (memEqualCI(url, "http:/", 6) ||
	    memEqualCI(url, "https:/", 7) || memEqualCI(url, "gopher:/", 8)) {
		s = strstr(url, "://");
		if (!s)		// should never happen
			s = url;
		else
			s += 3;
		s = strchr(s, '/');
		if (!s)
			return 0;
		url = s + 1;	// start here
	}
// lop off post data, get data, hash
	post = url + strcspn(url, "?\1");
	for (s = post - 1; s >= url && *s != '.' && *s != '/'; --s) ;
	if (s < url || *s != '.')
		return NULL;
	++s;
	if (post >= s + sizeof(suffix))
		return NULL;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	return suffix;
}

static const struct MIMETYPE *findMimeByProtocol(const char *prot)
{
	int i;
	int len = strlen(prot);
	const struct MIMETYPE *m = mimetypes;
	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->prot, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, prot, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeByProtocol */

// look for match on protocol, suffix, or url string
const struct MIMETYPE *findMimeByURL(const char *url, uchar * sxfirst)
{
	const char *prot, *suffix;
	const struct MIMETYPE *mt, *m;
	int i, j, l, url_length;
	char *s, *t;

// protocol first, unless sxfirst is 1, then suffix first.
// If sxfirst = 2 then protocol only.
	if (*sxfirst == 1) {
		if ((suffix = url2suffix(url))
		    && (mt = findMimeBySuffix(suffix)))
			return mt;
	}

	if ((prot = getProtURL(url)) && (mt = findMimeByProtocol(prot))) {
		*sxfirst = 0;
		return mt;
	}
	if (*sxfirst == 2)
		return 0;

	url_length = strlen(url);
	m = mimetypes;
	for (i = 0; i < maxMime; ++i, ++m) {
		s = m->urlmatch;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, '|');
			if (!t)
				t = s + strlen(s);
			l = t - s;
			if (l && l <= url_length) {
				for (j = 0; j + l <= url_length; ++j) {
					if (memEqualCI(s, url + j, l)) {
						*sxfirst = 0;
						return m;
					}
				}
			}
			if (*t)
				++t;
			s = t;
		}
	}

	if (!*sxfirst) {
		if ((suffix = url2suffix(url))
		    && (mt = findMimeBySuffix(suffix))) {
			*sxfirst = 1;
			return mt;
		}
	}

	return NULL;
}				/* findMimeByURL */

const struct MIMETYPE *findMimeByFile(const char *filename)
{
	char *suffix = file2suffix(filename);
	if (suffix)
		return findMimeBySuffix(suffix);
	return NULL;
}				/* findMimeByFile */

const struct MIMETYPE *findMimeByContent(const char *content)
{
	int i;
	int len = strlen(content);
	const struct MIMETYPE *m = mimetypes;

	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->content, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, content, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeByContent */

/*********************************************************************
Notes on where and why runPluginCommand is run.

The pb (play buffer) command,
if the file has a suffix plugin for playing, not rendering.
.xxx will overrule the suffix.
As you would imagine, this runs through playBuffer() below.
If you override with .xxx I spin the buffer data out to a temp file.
I assume this is necessary, as the player would not accept the file
with its current suffix or filename, or you downloaded it
from the internet or some other source.

g in directory mode if the file is playable, also goes through playBuffer().

If a new buffer, (not an r command or fetching javascript
in the background or some such),
or if the match is on protocol and the plugin is a converter,
(whence the plugin is necessary just to get the data),
then httpConnect sets the plugin cf->mt,
if such can be determine from protocol or content type or suffix.
If this plugin is url allowed, and does not require url download,
then run the plugin. Set cf->render1 if it's a rendering plugin.
If rendered by suffix, then so indicate, but I've never seen this happen.
Music players can take a url, converters not.
pdftohtml for instance, doesn't take a url,
you have to download the data from the internet,
whence this plugin does not run, at least not here, not from httpConnect.

When browsing a new buffer, b whatever, and it's a local file,
in readFile() in buffers.c, set cf->mt by suffix, and if it's rendering,
call the plugin so we don't have to pull the entire file into memory,
just the output of the plugin's converter.
Change b to e if the output is text, cause there's no html to browse.
This rendering will always be by suffix, there is no protocol or content type.
Mark it accordingly.

If we're browsing a new buffer, and httpConnect hasn't already rendered,
and http code is 200 or 201,
and cf->mt indicates render, then do so in readFile().
Mark as rendered by url or by suffix.
Again, if outtype is t then change b to e.

If we're browsing a new buffer, and http code is 200 or 201,
and suffix indicates play, then do so in readFile().
Return nothing, so we don't push a new buffer.

If we're browsing a new buffer, and a suffix plugin would render,
but plugins are inactive, change b to e so we don't go through browseCurrentBuffer().

In browseCurrentBuffer():
If this has not yet been rendered via suffix,
and you can find a plugin by suffix,
and it renders,
and it's different from the attached plugin,
then render it now. Set render2.
Why does it have to be different? Look at pdf.
httpConnect might find it by content = application/pdf.
That's not by suffix, so render2 is not set.
Here we are and we find it by suffix, but it's the same plugin,
so don't render it again.
*********************************************************************/

bool runPluginCommand(const struct MIMETYPE * m,
		      const char *inurl, const char *infile, const char *indata,
		      int inlength, char **outdata, int *outlength)
{
	const char *s;
	char *cmd = NULL, *t;
	char *outfile;
	char *suffix;
	int len, inlen, outlen;
	bool has_o = false;

	if (indata) {
// calling function has gathered the data for us,
// maybe we could pipe it to the program but for now
// I'm just putting it in a temp file having the same suffix.
		suffix = NULL;
		if (infile)
			suffix = file2suffix(infile);
		else
			suffix = url2suffix(inurl);
		++tempIndex;
		if (!makeTempFilename(suffix, tempIndex, false)) {
			cnzFree(indata);
			return false;
		}
		if (!memoryOutToFile(tempin, indata, inlength,
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
			cnzFree(indata);
			return false;
		}
		infile = tempin;
	} else if (inurl)
		infile = inurl;

// reserve an output file, whether we need it or not
	++tempIndex;
	suffix = "out";
	if (m->outtype == 't')
		suffix = "txt";
	if (m->outtype == 'h')
		suffix = "html";
	if (!makeTempFilename(suffix, tempIndex, true)) {
		if (indata) {
			cnzFree(indata);
			unlink(tempin);
		}
		return false;
	}
	outfile = tempout;

	len = 0;
	inlen = shellProtectLength(infile);
	outlen = shellProtectLength(outfile);
	for (s = m->program; *s; ++s) {
		if (*s == '%' && s[1] == 'i') {
			len += inlen;
			++s;
			continue;
		}
		if (*s == '%' && s[1] == 'o') {
			has_o = true;
			len += outlen;
			++s;
			continue;
		}
		++len;
	}
	++len;

// reserve space for > outfile
	cmd = allocMem(len + outlen + 3);
	t = cmd;
// pass 2
	for (s = m->program; *s; ++s) {
		if (*s == '%' && s[1] == 'i') {
			shellProtect(t, infile);
			t += inlen;
			++s;
			continue;
		}
		if (*s == '%' && s[1] == 'o') {
			shellProtect(t, outfile);
			t += outlen;
			++s;
			continue;
		}
		*t++ = *s;
	}
	*t = 0;

/*********************************************************************
if there is no output, or the program has %o, then just run it,
otherwise we have to send its output over to outdata,
which should be present.
There's no popen on windows, so here is a unix only
fragment to use popen, which can be more efficient.
*********************************************************************/

#ifndef DOSLIKE
	if (m->outtype && !has_o) {
		FILE *p;
		bool rc;
		debugPrint(3, "plugin %s", cmd);
		p = popen(cmd, "r");
		if (!p) {
			setError(MSG_NoSpawn, cmd, errno);
			goto fail;
		}
		rc = fdIntoMemory(fileno(p), outdata, outlength);
		pclose(p);
		if (!rc)
			goto fail;
		goto success;
	}
#endif

	if (m->outtype && !has_o) {
		strcat(cmd, " > ");
		strcat(cmd, outfile);
	}

	debugPrint(3, "plugin %s", cmd);

// time to run the command.
	if (eb_system(cmd, !m->outtype) < 0)
		goto success;

	if (!outdata)		// not capturing output
		goto success;
	if (!fileIntoMemory(outfile, outdata, outlength))
		goto fail;
// fall through

success:
	nzFree(cmd);
	if (indata) {
		unlink(tempin);
		cnzFree(indata);
	}
	unlink(tempout);
	return true;

fail:
	nzFree(cmd);
	if (indata) {
		unlink(tempin);
		cnzFree(indata);
	}
	unlink(tempout);
	return false;
}

/* play the contents of the current buffer, or otherwise
 * act upon it based on the program corresponding to its mine type.
 * This is called from twoLetter() in buffers.c, and should return:
* 0 error, 1 success, 2 not a play buffer command */
int playBuffer(const char *line, const char *playfile)
{
	const struct MIMETYPE *mt = 0;
	const char *suffix = NULL;
	bool rc;
	char c = line[2];
	if (c && c != '.')
		return 2;

	if (playfile) {
/* play the file passed in */
		mt = findMimeByFile(playfile);
// We wouldn't be here unless the file was playable,
// so this check and error return isn't really necessary.
#if 0
		if (!mt || mt->outtype) {
			suffix = file2suffix(playfile);
			if (!suffix)
				setError(MSG_NoSuffix);
			else
				setError(MSG_SuffixBad, suffix);
			return 0;
		}
#endif
		return runPluginCommand(mt, 0, playfile, 0, 0, 0, 0);
	}

	if (!cw->dol) {
		setError(cw->dirMode ? MSG_EmptyBuffer : MSG_AudioEmpty);
		return 0;
	}
	if (cw->browseMode) {
		setError(MSG_AudioBrowse);
		return 0;
	}
	if (cw->sqlMode) {
		setError(MSG_AudioDB);
		return 0;
	}
	if (cw->dirMode) {
		setError(MSG_AudioDir);
		return 0;
	}

	if (c) {
		char *buf;
		int buflen;
		suffix = line + 3;
		mt = findMimeBySuffix(suffix);
		if (!mt) {
			setError(MSG_SuffixBad, suffix);
			return 0;
		}
		if (mt->outtype) {
			setError(MSG_NotPlayer);
			return 0;
		}
// If you had to specify suffix then we have to run from the buffer.
		if (!unfoldBuffer(context, false, &buf, &buflen))
			return 0;
// runPluginCommand always frees the input data.
		return runPluginCommand(mt, 0, line, buf, buflen, 0, 0);
	}

	if (!mt && cf->fileName) {
		if (isURL(cf->fileName)) {
			uchar sxfirst = 1;
			suffix = url2suffix(cf->fileName);
			mt = findMimeByURL(cf->fileName, &sxfirst);
		} else {
			suffix = file2suffix(cf->fileName);
			mt = findMimeByFile(cf->fileName);
		}
	}
	if (!mt) {
		if (suffix)
			setError(MSG_SuffixBad, suffix);
		else
			setError(MSG_NoSuffix);
		return 0;
	}

	if (mt->outtype) {
		setError(MSG_NotPlayer);
		return 0;
	}

	if (isURL(cf->fileName))
		rc = runPluginCommand(mt, cf->fileName, 0, 0, 0, 0, 0);
	else
		rc = runPluginCommand(mt, 0, cf->fileName, 0, 0, 0, 0);
	return rc;
}
