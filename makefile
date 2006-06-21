#  This is the makefile for edbrowse.

#  Flags for gcc compilation.
#  Note, some have reported seg-faults with this program when -O is used.
#  This is a problem with gcc version 2.95 or less.
#  Since performance is not critical here, better leave it out.
#
#  The -I flags assume smjs is installed in /usr/local
#  You may also need -I/usr/include/pcre or -I/usr/local/include/pcre

CFLAGS = -I/usr/local/js/src -I/usr/local/js/src/Linux_All_DBG.OBJ -DXP_UNIX -DX86_LINUX

#  If the smjs library is already installed by your linux distribution,
#  e.g. Debian, use the following flags.
#CFLAGS = -I/usr/include/smjs -DXP_UNIX -DX86_LINUX

#  Normal load flags
LFLAGS = -s

#  Libraries for edbrowse.
#  I assume you have linked libjs.so into /usr/lib/libsmjs.so
#  so that -lsmjs will suffice.
#  Some distros, e.g. Debian, already do this for you.
#  Some folks need to add -lcrypto to this list.

LIBS = -lpcre -lm -lssl -lsmjs

#  Make the dynamically linked executable program by default.
#  Edbrowse executable.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
	html.o format.o cookies.o stringfile.o jsdom.o jsloc.o

#  Header file dependencies.
$(EBOBJS) : eb.h eb.p
main.o tcp.o http.o sendmail.o: tcp.h

edbrowse: $(EBOBJS) tcp.o
	cc $(LFLAGS) -o edbrowse tcp.o $(EBOBJS) $(LIBS)

#  Build function prototypes.
proto:
	mkproto -g main.c buffers.c url.c auth.c http.c sendmail.c fetchmail.c \
	html.c format.c cookies.c stringfile.c jsdom.c jsloc.c >eb.p

#  I've had no luck getting this to work - can you help?
edbrowse.static: $(EBOBJS) tcp.o
	cc --static $(LFLAGS) -o edbrowse.static tcp.o $(EBOBJS) $(LIBS)

