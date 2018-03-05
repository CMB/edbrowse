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

const struct MIMETYPE *findMimeBySuffix(const char *suffix)
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

const struct MIMETYPE *findMimeByProtocol(const char *prot)
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
const struct MIMETYPE *findMimeByURL(const char *url)
{
	const char *prot, *suffix;
	const struct MIMETYPE *mt, *m;
	int i, j, l, url_length;
	char *s, *t;

// protocol first, it seems higher precedence.
	if ((prot = getProtURL(url)) && (mt = findMimeByProtocol(prot)))
		return mt;

	if ((suffix = url2suffix(url)) && (mt = findMimeBySuffix(suffix)))
		return mt;

/* not by protocol or suffix, let's look for a url match */
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
					if (memEqualCI(s, url + j, l))
						return m;
				}
			}
			if (*t)
				++t;
			s = t;
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
		if (!makeTempFilename(suffix, tempIndex, false))
			return false;
		if (!memoryOutToFile(tempin, indata, inlength,
				     MSG_TempNoCreate2, MSG_NoWrite2))
			return false;
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
		if (indata)
			unlink(tempin);
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

// if there is no output, or the program has %o, then just run it,
// otherwise we have to send its output over to outdata,
// which should be present.
// There's no popen on windows, so here is a unix only
// fragment to use popen, which can be more efficient.

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
		fclose(p);
		if (!rc)
			goto fail;
		cnzFree(indata);
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
	cnzFree(indata);
// fall through

success:
	nzFree(cmd);
	if (indata)
		unlink(tempin);
	unlink(tempout);
	return true;

fail:
	nzFree(cmd);
	if (indata)
		unlink(tempin);
	unlink(tempout);
	return false;
}

/* play the contents of the current buffer, or otherwise
 * act upon it based on the program corresponding to its mine type.
 * This is called from twoLetter() in buffers.c, and should return:
* 0 error, 1 success, 2 not a play buffer command */
int playBuffer(const char *line, const char *playfile)
{
	const struct MIMETYPE *mt = cf->mt;
	const char *suffix;
	char c = line[2];
	if (c && c != '.')
		return 2;

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
	if (cw->dirMode && !playfile) {
		setError(MSG_AudioDir);
		return 0;
	}

	if (playfile) {
/* play the file passed in */
		mt = findMimeByFile(playfile);
		if (!mt || mt->outtype) {
			suffix = file2suffix(playfile);
			if (!suffix)
				suffix = "?";
			setError(MSG_SuffixBad, suffix);
			return 0;
		}
		return runPluginCommand(mt, 0, playfile, 0, 0, 0, 0);
	}
// current buffer
	if (c) {
		mt = findMimeBySuffix(line + 3);
		if (!mt || mt->outtype) {
			setError(MSG_SuffixBad, line + 3);
			return 0;
		}
	}

	suffix = NULL;
	if (!mt && cf->fileName) {
		if (isURL(cf->fileName)) {
			suffix = url2suffix(cf->fileName);
			cf->mt = mt = findMimeByURL(cf->fileName);
		} else {
			suffix = file2suffix(cf->fileName);
			cf->mt = mt = findMimeByFile(cf->fileName);
		}
	}
	if (!mt || mt->outtype) {
		if (suffix)
			setError(MSG_SuffixBad, suffix);
		else
			setError(MSG_NoSuffix);
		return 0;
	}

	if (cf->fileName) {
		if (isURL(cf->fileName))
			return runPluginCommand(mt, cf->fileName, 0, 0, 0, 0,
						0);
		else
			return runPluginCommand(mt, 0, cf->fileName, 0, 0, 0,
						0);
	}
// buffer has no name.
// I could unfold it into a string and pass it to runPluginCommand that way,
// but this just never happens, so I don't care.
	setError(MSG_NoSuffix);
	return 0;
}
