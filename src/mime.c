/*********************************************************************
mime.c: mime types and plugins.
Run audio players, pdf converters, etc, based on suffix or content-type.
*********************************************************************/

#include "eb.h"

const struct MIMETYPE *findMimeBySuffix(const char *suffix)
{
	int i;
	int len = strlen(suffix);
	const struct MIMETYPE *m = mimetypes;

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

const struct MIMETYPE *findMimeByURL(const char *url)
{
	char suffix[12];
	const char *post, *s;
	post = url + strcspn(url, "?\1");
	for (s = post - 1; s >= url && *s != '.' && *s != '/'; --s) ;
	if (*s != '.')
		return NULL;
	++s;
	if (post >= s + sizeof(suffix))
		return NULL;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	return findMimeBySuffix(suffix);
}				/* findMimeByURL */

const struct MIMETYPE *findMimeByFile(const char *filename)
{
	char suffix[12];
	const char *post, *s;
	post = filename + strlen(filename);
	for (s = post - 1; s >= filename && *s != '.' && *s != '/'; --s) ;
	if (*s != '.')
		return NULL;
	++s;
	if (post >= s + sizeof(suffix))
		return NULL;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	return findMimeBySuffix(suffix);
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

/* The result is allocated */
char *pluginCommand(const char *file, const char *suffix)
{
	const struct MIMETYPE *m = cw->mt;
	int len, suflen;
	const char *s;
	char *cmd, *t;
	bool trailPercent = false;

/* leave room for space quote quote null */
	len = strlen(m->program) + 4;
	if (file) {
		len += strlen(file);
	} else if (m->program[strlen(m->program) - 1] == '%') {
		trailPercent = true;
		len += strlen(edbrowseTempFile) + 6;
	}

	suflen = 0;
	if (suffix) {
		suflen = strlen(suffix);
		for (s = m->program; *s; ++s)
			if (*s == '*')
				len += suflen - 1;
	}

	cmd = allocMem(len);
	t = cmd;
	for (s = m->program; *s; ++s) {
		if (suffix && *s == '*') {
			strcpy(t, suffix);
			t += suflen;
		} else {
			*t++ = *s;
		}
	}
	*t = 0;

	if (file) {
		sprintf(t, " \"%s\"", file);
	} else if (trailPercent) {
		sprintf(t - 1, " \"%s.%s\"", edbrowseTempFile, suffix);
	}

	debugPrint(3, "%s", cmd);
	return cmd;
}				/* pluginCommand */

/* Send the contents of the current buffer to a running program */
static bool bufferToProgram(const char *cmd, const char *suffix,
			    bool trailPercent)
{
	char *buf = 0;
	int buflen, n;
	int size1, size2;
	char *u = edbrowseTempFile + strlen(edbrowseTempFile);

	if (!trailPercent) {
/* pipe the buffer into the program */
		FILE *f = popen(cmd, "w");
		if (!f) {
			setError(MSG_NoSpawn, cmd, errno);
			return false;
		}
		if (!unfoldBuffer(context, false, &buf, &buflen)) {
			pclose(f);
			return false;	/* should never happen */
		}
		n = fwrite(buf, buflen, 1, f);
		pclose(f);
	} else {
		sprintf(u, ".%s", suffix);
		size1 = currentBufferSize();
		size2 = fileSizeByName(edbrowseTempFile);
		if (size1 == size2) {
/* assume it's the same data */
			*u = 0;
		} else {
			if (!unfoldBuffer(context, false, &buf, &buflen)) {
				*u = 0;
				return false;	/* should never happen */
			}
			if (!memoryOutToFile(edbrowseTempFile, buf, buflen,
					     MSG_TempNoCreate2, MSG_NoWrite2)) {
				*u = 0;
				return false;
			}
			*u = 0;
		}
		system(cmd);
	}

	nzFree(buf);
	return true;
}				/* bufferToProgram */

/* play the contents of the current buffer, or otherwise
 * act upon it based on the program corresponding to its mine type.
 * This is called from twoLetter() in buffers.c, and should return:
* 0 error, 1 success, 2 not a play buffer command */
int playBuffer(const char *line)
{
	const struct MIMETYPE *mt = cw->mt;
	char *cmd;
	const char *suffix = NULL;
	int rc;
	bool trailPercent = false;
	char c = line[2];

	if (c && c != '.')
		return 2;

	if (!cw->dol) {
		setError(MSG_AudioEmpty);
		return 0;
	}
	if (cw->browseMode) {
		setError(MSG_AudioBrowse);
		return 0;
	}
	if (cw->dirMode) {
		setError(MSG_AudioDir);
		return 0;
	}
	if (cw->sqlMode) {
		setError(MSG_AudioDB);
		return 0;
	}

	if (!mt) {
/* need to determine the mime type */
		if (c == '.') {
			suffix = line + 3;
		} else {
			if (cw->fileName)
				suffix = strrchr(cw->fileName, '.');
			if (!suffix) {
				setError(MSG_NoSuffix);
				return 0;
			}
			++suffix;
		}
		if (strlen(suffix) > 5) {
			setError(MSG_SuffixLong);
			return 0;
		}
		mt = findMimeBySuffix(suffix);
		if (!mt) {
			setError(MSG_SuffixBad, suffix);
			return 0;
		}
		cw->mt = mt;
	}

	if (mt->program[strlen(mt->program) - 1] == '%')
		trailPercent = true;
	cmd = pluginCommand(0, suffix);
	rc = bufferToProgram(cmd, suffix, trailPercent);
	nzFree(cmd);
	return rc;
}				/* playBuffer */
