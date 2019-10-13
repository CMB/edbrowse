#  This is the makefile for edbrowse under BSD Unix.
#  See GNUmakefile for the makefile used with GNU make on Linux.

# Default settings:
CC ?=		cc
CXX ?=		c++
PREFIX ?=	/usr/local
CFLAGS ?=	-Wall -Wextra -D_FILE_OFFSET_BITS=64
CXXFLAGS ?=	-Wall -Wextra -D_FILE_OFFSET_BITS=64
BUILD_EDBR_ODBC ?= off

PERL !=		which perl
TIDY5_LIBS ?=	-ltidy

LIBS =		-lpcre -lcurl -lreadline ${TIDY5_LIBS} -lduktape -lpthread -lm

# Add PREFIX to search paths.  These should go after everything else is in.
CPPFLAGS +=	-I${PREFIX}/include
LDFLAGS +=	-L${PREFIX}/lib

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS =	main.o buffers.o sendmail.o fetchmail.o cache.o \
		html.o format.o cookies.o ebjs.o plugin.o ebrc.o \
		messages.o url.o stringfile.o html-tidy.o decorate.o \
		msg-strings.o http.o auth.o css.o jseng-duk.o startwindow.o

.if ${BUILD_EDBR_ODBC:L:Mon}
EBOBJS +=	dbodbc.o dbinfx.o dbops.o
LIBS +=		-lodbc
.else
EBOBJS +=	dbstubs.o
.endif

$(EBOBJS): eb.h ebprot.h messages.h
dbodbc.o dbinfx.o dbops.o: dbapi.h

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

startwindow.c: startwindow.js endwindow.js
	cd .. ; ${PERL} -w ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/endwindow.js thirdJS src/startwindow.c

ebrc.c: ../lang/ebrc-* ../doc/usersguide*.html
	cd .. ; ${PERL} -w ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; ${PERL} -w ./tools/buildmsgstrings.pl

edbrowse: $(EBOBJS)
	${CC} $(LDFLAGS) -o edbrowse $> $(LIBS)

clean:
	rm -f *.o edbrowse \
	startwindow.c ebrc.c msg-strings.c

.PHONY: all clean
