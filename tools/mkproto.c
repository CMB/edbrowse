/* mkproto.c: make function prototype headers out of a sourcefile */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* the symbol DOSLIKE is used to conditionally compile those constructs
 * that are common to DOS and NT, but not typical of Unix. */
#ifdef MSDOS
#define DOSLIKE
#endif
#ifdef _WIN32
#define DOSLIKE
#endif

static char globalflag, staticflag;	/* extract global or static prototypes. */
static void mkproto(char *fname);

int
main(int argc, char **argv)
{
    if(argc > 1 && !strcmp(argv[1], "-g")) {
	++argv, --argc;
	globalflag = 1;
    }

    if(argc > 1 && !strcmp(argv[1], "-s")) {
	++argv, --argc;
	staticflag = 1;
    }

    if(argc == 1) {
	fprintf(stderr, "Usage:  mkproto [-g|s] file1.c file2.c ...\n");
	exit(1);
    }

    if(!staticflag)
	puts("/* This file is machine-generated, do not hand edit. */\n");

    while(argc > 1) {
	++argv, --argc;
	mkproto(*argv);
    }
    return 0;
}				/* main */

/* this function runs as a state machine, with the following states.
 0 clear C text.
 1 / received, starting a comment?
 2 / * received, in comment.
 3 * received in a comment, ending the comment?
 4 in a #xxx preprocesssor line.
5 " received, starting a string.
6 \ received inside a string.
 7 ' received, starting a char constant.
 8 \ received inside a char constant.
 */

static void
mkproto(char *fname)
{
    int c;
    short comstate, nestlev, semstate;
    char lastchar, last_ns, spc;
    long offset, charcnt = 0;
    FILE *f = fopen(fname, "r");
    char fword[10];
    int fword_cnt;

    if(!f) {
	fprintf(stderr, "cannot open sourcefile %s\n", fname);
	return;
    }

    if(!staticflag)
	printf("/* sourcefile=%s */\n", fname);
    comstate = nestlev = 0;
    semstate = 1;
    last_ns = lastchar = 0;
    while((c = getc(f)) != EOF) {
	++charcnt;
#ifdef DOSLIKE
	if(c == '\n')
	    ++charcnt;
#endif
	if(comstate < 2 && c == '#' && lastchar == '\n') {
	    comstate = 4;
	    continue;
	}
	if(comstate == 1 && c == '/') {
	    comstate = 4;
	    continue;
	}
	if(c == '*' && comstate == 1) {
	    comstate = 2;
	    continue;
	}
	if(c == '*' && comstate == 2) {
	    comstate = 3;
	    continue;
	}
	if(c == '/' && comstate == 3) {
	    comstate = 0;
	    continue;
	}
	if(c == '/' && comstate == 0) {
	    comstate = 1;
	    continue;
	}
	if(comstate == 3 && c != '*')
	    comstate = 2;
	if(comstate == 1)
	    comstate = 0;	/* not resolved into a comment */
	if(c == '\n' && comstate == 4 && lastchar != '\\') {
	    lastchar = c;
	    comstate = 0;
	    continue;
	}
	if(comstate == 5 || comstate == 7) {
	    if(c == '\\')
		++comstate;
	    if(c == '"' && comstate == 5)
		comstate = 0;
	    if(c == '\'' && comstate == 7)
		comstate = 0;
	    continue;
	}
	if(comstate == 6 || comstate == 8)
	    --comstate;
	if(!comstate && c == '"')
	    comstate = 5;
	if(!comstate && c == '\'')
	    comstate = 7;
	if(comstate) {
	    lastchar = c;
	    continue;
	}
	lastchar = c;

	if(c == '{') {
	    ++nestlev;
	    if(nestlev > 1)
		continue;
	    if(last_ns != ')')
		continue;
	    if(!strncmp(fword, "static", 6) || !strncmp(fword, "gstatic", 7)) {
		if(globalflag)
		    continue;
	    } else {
		if(staticflag)
		    continue;
	    }

	    fseek(f, offset, 0);
	    while(++offset < charcnt) {
		c = getc(f);
#ifdef DOSLIKE
		if(c == '\n')
		    ++offset;
#endif
		if(c == '\n' && comstate == 4) {
		    comstate = 0;
		    continue;
		}
		if(c == '/') {
		    char newstate[] = { 1, 4, 0, 0, 4 };
		    comstate = newstate[comstate];
		    continue;
		}
		if(comstate == 1)
		    comstate = 2;
		if(comstate)
		    continue;
		if(isspace(c)) {
		    c = ' ';
		    if(spc)
			continue;
		    spc = 1;
		} else
		    spc = 0;
		putchar(c);
	    }
	    printf(";\n");
	    getc(f);
	    continue;
	}			/* { read */
	if(c == '}') {
	    --nestlev;
	    if(!nestlev)
		semstate = 1;
	    continue;
	}
	if(nestlev)
	    continue;

	if(c == ';') {
	    semstate = 1;
	    continue;
	}

	if(isspace(c))
	    continue;
	last_ns = c;
	if(!semstate) {
	    if(fword_cnt < 10)
		fword[fword_cnt++] = c;
	    continue;
	}

	offset = charcnt - 1;
	fword[0] = c;
	fword_cnt = 1;
	semstate = 0;
    }

    if(!staticflag)
	printf("\n");
    fclose(f);
}				/* mkproto */
