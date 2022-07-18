/*********************************************************************
html-tags.c: parse the html tags and attributes.
This was originally handled by the tidy library, but tidy is no longer maintained.
Bugs are accumulating, and we continue to work around those bugs,
in html-tidy.c and in decorate.c, and it's getting old.
So, at some point, this file will do the job,
and will be maintained by us.
I believe it is easier, in the long run, to roll our own, rather than using
yet another html library, but I could be wrong,
tidy may have left a bad taste in my mouth.
In any case, I don't think it's terribly hard,
certainly easier than a javascript parser, so let's give it a whirl.
Note that we can paste the tags directly into the edbrowse tree;
any other library, including tidy, builds its own tree of nodes,
which we then import into the edbrowse tree of tags.
*********************************************************************/

#include "eb.h"

static void findAttributes(const char *start, const char *end);
static void setAttribute(const char *a1, const char *a2, const char *v1, const char *v2);
static char *pullAnd(const char *start, const char *end);

void html2tags(const char *htmltext, bool startpage)
{
	int ln = 1; // line number
	int i;
	const char *lt; // les than sign
	const char *gt; // greater than sign
	const char *seek, *s, *t, *u;
	char *w;
	bool slash; // </foo>
	char tagname[MAXTAGNAME];

	seek = s = htmltext;
// loop looking for tags
	while(*s) {
// the next literal < should begin the next tag
// text like x<y is invalid; you should be using &lt; in this case
		if(!(lt = strchr(s, '<')))
			break;
		slash = false, t = lt + 1;
		if(*t == '/') ++t, slash = true;

// bare < just passes through
		if((!slash && *t != '!' && !isalpha(*t)) ||
		(slash && !isalpha(*t))) {
			s = t + 1;
			continue;
		}

// text fragment between tags
		if(lt > seek) {
			w = pullAnd(seek, lt);
			  printf("text{%s}\n", w);
			nzFree(w);
// adjust line number
			for(u = seek; u < lt; ++u)
				if(*u == '\n') ++ln;
		}

// special code here for html comment
		if(*t == '!') {
			int hyphens = 0;
			++t;
			while(t[0] == '-' && t[1] == '-')
				t += 2, hyphens += 2;
			u = t;
//   <!------> is not self contained
			if(*u == '>') ++u;
closecomment:
			u = strchr(u, '>');
			if(!u) goto opencomment;
			for(i = 1; i <= hyphens; ++i)
				if(u[-i] != '-') break;
			if(i < hyphens) { ++u; goto closecomment; }
// this is a valid comment
			puts("comment");
// adjust line number
			for(t = lt; t < u; ++t)
				if(*t == '\n') ++ln;
			seek = s = u + 1;
			continue;
opencomment:
			puts("open comment, html parsing stops here");
			return;
		}

// at this point it is <tag or </tag
		i = 0;
		while(isalnum(*t)) {
			if(i < MAXTAGNAME - 1) tagname[i++] = *t;
			++t;
		}
		tagname[i] = 0;
// the next > should close the tag
// <tag foo="bar>bar"> is not valid, you should be using &gt; in this case
		gt = strchr(t, '>');
// if no > then I'll go up to the next <
// this isn't correct html but that's how tidy handles it
		if(!gt) gt = strchr(t, '<');
		if(!gt) {
			printf("open tag %s, html parsing stops here\n", tagname);
			return;
		}
// adjust line number for this tag
		for(u = lt; u < gt; ++u)
			if(*u == '\n') ++ln;
		if(*(seek = gt) == '>') ++seek;
		s = seek; // ready to march on

		if(slash) {
// close the corresponging open tag. If none found then discard this one.
// create this tag in the edbrowse world.
			printf("</%s>\n", tagname);
// No need to gather attributes
			continue;
		}

// create this tag in the edbrowse world.
		printf("<%s>\n", tagname);

		findAttributes(t, gt);
	}

// seek points to the last piece of the buffer, after the last tag
	if(*seek) {
		w = pullAnd(seek, seek + strlen(seek));
		  printf("text{%s}\n", w);
		nzFree(w);
	}

}

static void findAttributes(const char *start, const char *end)
{
	const char *s = start;
	char qc; // quote character
	const char *a1, *a2; // attribute name
	const char *v1, *v2; // attribute value

	while(s < end) {
// look for a C identifier, then whitespace, then =
		if(!isalpha(*s)) { ++s; continue; }
		a1 = s; // start of attribute
		a2 = a1 + 1;
		while(*a2 == '_' || isalnum(*a2)) ++a2;
		for(s = a2; isspace(*s); ++s)  ;
		if(*s != '=' || s == end) {
// it could be an attribute with no value, but then we need whitespace
			if(s > a2 || s == end)
				setAttribute(a1, a2, a2, a2);
			continue;
		}
		for(v1 = s + 1; isspace(*v1); ++v1)  ;
		qc = 0;
		if(*v1 == '"' || *v1 == '\'') qc = *v1++;
		for(v2 = v1; v2 < end; ++v2)
			if((!qc && isspace(*v2)) || (qc && *v2 != qc)) break;
		setAttribute(a1, a2, v1, v2);
		if(*v2 == qc) ++v2;
		s = v2;
	}
}

// this is just a print for now
static void setAttribute(const char *a1, const char *a2, const char *v1, const char *v2)
{
	char *w;
	w = pullAnd(a1, a2);
	printf("%s=", w);
	nzFree(w);
	w = pullAnd(v1, v2);
	printf("%s\n", w);
	nzFree(w);
}

// make an allocated copy of the designated string,
// then decode the & fragments.

static char *pullAnd(const char *start, const char *end)
{
	int l = end - start;
	char *w = pullString(start, l);

// the assumption here is that &stuff always encodes to something smaller
// when represented as utf8.

	return w;
}

