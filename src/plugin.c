/*********************************************************************
plugin.c: mime types and plugins.
Run audio players, pdf converters, etc, based on suffix or content-type.
*********************************************************************/

#include "eb.h"

/* create an input or an output file for edbrowse under /tmp.
 * Since an external program may act upon this file, a certain suffix
 * may be required.
 * Fails if /tmp/.edbrowse does not exist or cannot be created. */
static const char tempbase[] = "/tmp/.edbrowse";
static char tempin[60], tempout[60];
static char *suffixin;		/* suffix of input file */
static char *suffixout;		/* suffix of output file */
static bool makeTempFilename(const char *suffix, int idx, bool output)
{
	if (fileTypeByName(tempbase, false) != 'd') {
/* no such directory, try to make it */
		if (mkdir(tempbase)) {
			setError(MSG_TempDir);
			return false;
		}
/* this temp edbrowse directory is used by everyone system wide */
		chmod(tempbase, 0777);
	}

	sprintf((output ? tempout : tempin), "%s/pf%d-%d.%s",
		tempbase, getpid(), idx, suffix);
	if (!output)
		suffixin = tempin + strlen(tempin) - strlen(suffix);
	return true;
}				/* makeTempFilename */

static int tempIndex;

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
char *pluginCommand(const struct MIMETYPE *m,
		    const char *infile, const char *outfile, const char *suffix)
{
	int len, inlen, outlen;
	const char *s;
	char *cmd, *t;

	if (!suffix)
		suffix = EMPTYSTRING;
	if (!infile)
		infile = EMPTYSTRING;
	if (!outfile)
		outfile = EMPTYSTRING;

	len = 0;
	for (s = m->program; *s; ++s) {
#if 0
		if (*s == '*') {
			len += strlen(suffix);
			continue;
		}
#endif
		if (*s == '%' && s[1] == 'i') {
			inlen = shellProtectLength(infile);
			len += inlen;
			++s;
			continue;
		}
		if (*s == '%' && s[1] == 'o') {
			outlen = shellProtectLength(outfile);
			len += outlen;
			++s;
			continue;
		}
		++len;
	}
	++len;

	cmd = allocMem(len);
	t = cmd;
	for (s = m->program; *s; ++s) {
#if 0
		if (*s == '*') {
			strcpy(t, suffix);
			t += strlen(suffix);
			continue;
		}
#endif
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

	debugPrint(3, "plugin %s", cmd);
	return cmd;
}				/* pluginCommand */

#if 0
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
#endif

/* play the contents of the current buffer, or otherwise
 * act upon it based on the program corresponding to its mine type.
 * This is called from twoLetter() in buffers.c, and should return:
* 0 error, 1 success, 2 not a play buffer command */
int playBuffer(const char *line, const char *playfile)
{
	const struct MIMETYPE *mt = cw->mt;
	static char sufbuf[12];
	char *cmd;
	const char *suffix = NULL;
	char *buf;
	int buflen;
	char *infile;
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
		suffix = strrchr(playfile, '.') + 1;
		strcpy(sufbuf, suffix);
		suffix = sufbuf;
		mt = findMimeBySuffix(suffix);
		if (!mt || mt->outtype | mt->stream) {
			setError(MSG_SuffixBad, suffix);
			return false;
		}
		cmd = pluginCommand(mt, playfile, 0, suffix);
		if (!cmd)
			return 0;
		goto play_command;
	}

	if (!mt) {
/* need to determine the mime type */
		if (c == '.') {
			suffix = line + 3;
		} else {
			if (cw->fileName) {
				const char *endslash;
				suffix = strrchr(cw->fileName, '.');
				endslash = strrchr(cw->fileName, '/');
				if (suffix && endslash && endslash > suffix)
					suffix = NULL;
			}
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

	if (!suffix) {
		suffix = mt->suffix;
		if (!suffix)
			suffix = "x";
		else {
			int i;
			for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
				if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
					break;
				sufbuf[i] = mt->suffix[i];
			}
			sufbuf[i] = 0;
			suffix = sufbuf;
		}
	}

	if (mt->outtype | mt->stream) {
		setError(MSG_SuffixBad, suffix);
		return 0;
	}

	++tempIndex;
	makeTempFilename(suffix, tempIndex, false);
	infile = tempin;
	if (!isURL(cw->fileName) && !access(cw->fileName, 4))
		infile = cw->fileName;
	cmd = pluginCommand(mt, infile, 0, suffix);
	if (!cmd)
		return 0;
	if (infile == tempin) {
		if (!unfoldBuffer(context, false, &buf, &buflen)) {
			nzFree(cmd);
			return 0;
		}
		if (!memoryOutToFile(tempin, buf, buflen,
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
			unlink(tempin);
			nzFree(cmd);
			nzFree(buf);
			return 0;
		}
		nzFree(buf);
	}

play_command:
	signal(SIGPIPE, SIG_DFL);
	system(cmd);
	signal(SIGPIPE, SIG_IGN);

	if (!cw->dirMode && infile == tempin)
		unlink(tempin);
	nzFree(cmd);
	i_puts(MSG_OK);

	return 1;
}				/* playBuffer */

bool playServerData(void)
{
	const struct MIMETYPE *mt = cw->mt;
	char *cmd;
	const char *suffix = mt->suffix;

	if (!suffix)
		suffix = "x";
	else {
		static char sufbuf[12];
		int i;
		for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
			if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
				break;
			sufbuf[i] = mt->suffix[i];
		}
		sufbuf[i] = 0;
		suffix = sufbuf;
	}

	++tempIndex;
	makeTempFilename(suffix, tempIndex, false);
	cmd = pluginCommand(cw->mt, tempin, 0, suffix);
	if (!cmd)
		return false;
	if (!memoryOutToFile(tempin, serverData, serverDataLen,
			     MSG_TempNoCreate2, MSG_NoWrite2)) {
		unlink(tempin);
		nzFree(cmd);
		return false;
	}

	signal(SIGPIPE, SIG_DFL);
	system(cmd);
	signal(SIGPIPE, SIG_IGN);

	unlink(tempin);
	nzFree(cmd);
	i_puts(MSG_OK);

	return true;
}				/* playServerData */

/* return the name of the output file, or 0 upon failure */
char *runPluginConverter(const char *buf, int buflen)
{
	const struct MIMETYPE *mt = cw->mt;
	char *cmd;
	const char *suffix = mt->suffix;
	bool ispipe = !strstr(mt->program, "%o");
	bool rc;
	char *infile;

	if (!suffix)
		suffix = "x";
	else {
		static char sufbuf[12];
		int i;
		for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
			if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
				break;
			sufbuf[i] = mt->suffix[i];
		}
		sufbuf[i] = 0;
		suffix = sufbuf;
	}

	++tempIndex;
	makeTempFilename(suffix, tempIndex, false);
	suffixout = (cw->mt->outtype == 'h' ? "html" : "txt");
	++tempIndex;
	makeTempFilename(suffixout, tempIndex, true);
	infile = tempin;
	if (!isURL(cw->fileName) && !access(cw->fileName, 4))
		infile = cw->fileName;
	cmd = pluginCommand(cw->mt, infile, tempout, suffix);
	if (!cmd)
		return NULL;
	if (infile == tempin) {
		if (!memoryOutToFile(tempin, buf, buflen,
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
			unlink(tempin);
			nzFree(cmd);
			return NULL;
		}
	}

	if (ispipe) {
		FILE *p = popen(cmd, "r");
		if (!p) {
			setError(MSG_NoSpawn, cmd, errno);
			if (infile == tempin)
				unlink(tempin);
			nzFree(cmd);
			return NULL;
		}
/* borrow a global data array */
		rc = fdIntoMemory(fileno(p), &serverData, &serverDataLen);
		fclose(p);
		if (infile == tempin)
			unlink(tempin);
		nzFree(cmd);
		if (rc)
			return "|";
		nzFree(serverData);
		serverData = NULL;
		return NULL;
	}

	signal(SIGPIPE, SIG_DFL);
	system(cmd);
	signal(SIGPIPE, SIG_IGN);

	if (infile == tempin)
		unlink(tempin);
	nzFree(cmd);
	return tempout;
}				/* runPluginConverter */
