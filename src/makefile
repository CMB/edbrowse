#  This is the makefile for edbrowse under BSD Unix.
#  See GNUmakefile for the makefile used with GNU make on Linux.

# Default settings:
CC ?=		cc
CXX ?=		c++
PREFIX ?=	/usr/local
MOZJS ?=	mozjs-24	# Name of mozjs pkg-config(1) module.
CFLAGS ?=	-Wall -Wextra
CXXFLAGS ?=	-Wall -Wextra
ODBC ?=		off

CPPFLAGS +!=	pkg-config --cflags-only-I ${MOZJS}
CXXFLAGS +!=	pkg-config --cflags-only-other ${MOZJS}

#  Libraries for edbrowse.
LIBS +=		-lpcre -lcurl -lreadline -lncurses -ltidy
LIBS +!= 	pkg-config --libs mozjs-24
LIBS +=		-lstdc++

# Add PREFIX to search paths.  These should go after everything else is in.
CPPFLAGS +=	-I${PREFIX}/include
LDFLAGS +=	-L${PREFIX}/lib

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS =	main.o buffers.o sendmail.o fetchmail.o \
		html.o format.o cookies.o ebjs.o plugin.o ebrc.o \
		messages.o url.o stringfile.o html-tidy.o decorate.o \
		msg-strings.o http.o auth.o jseng-moz.o startwindow.o

.if ${ODBC:L:Mon}
EBOBJS +=	dbodbc.o dbinfx.o dbops.o
LIBS +=	-lodbc
.else
EBOBJS +=	dbstubs.o
.endif

$(EBOBJS): eb.h ebprot.h messages.h ebjs.h
dbodbc.o dbinfx.o dbops.o: dbapi.h

startwindow.c: startwindow.js
	cd .. ; ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/startwindow.c

ebrc.c: ../lang/ebrc-*
	cd .. ; ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; ./tools/buildmsgstrings.pl

edbrowse: $(EBOBJS)
	${CC} $(LDFLAGS) -o edbrowse $> $(LIBS)

clean:
	rm -f *.o edbrowse \
	startwindow.c ebrc.c msg-strings.c

.PHONY: all clean
