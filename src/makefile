#  This is the makefile for edbrowse.

#  Flags for gcc compilation.
#  Note, some have reported seg-faults with this program when -O is used.
#  This is a problem with gcc version 2.95 or less.
#  Since performance is not critical here, I leave it out.

#  The -I flags assume smjs is installed in /usr/local
#  You may also need -I/usr/include/pcre or -I/usr/local/include/pcre

#  If smjs comes compiled on your distribution,
#  you may need -DMOZILLA_1_8_BRANCH
#  This issue has cropped up on certain flavors of debian.

#  Allow for symbolic debugging.
DEBUGFLAGS=-g -ggdb
STRIP=
ifeq ($(EBDEBUG),) # debugging turned off
DEBUGFLAGS=
STRIP=-s
endif

CFLAGS = -I/usr/local/js/src -I/usr/local/js/src/Linux_All_DBG.OBJ -DXP_UNIX -DX86_LINUX $(DEBUGFLAGS)

#  If the smjs library is already installed by your linux distribution,
#  e.g. Debian, use the following flags.
#
#CFLAGS = -I/usr/include/smjs -DXP_UNIX -DX86_LINUX $(DEBUGFLAGS)

#  Normal load flags
LFLAGS = $(STRIP)

#  ESQL C load flags
ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql

#  Libraries for edbrowse.
#  I assume you have linked libjs.so into /usr/lib/libsmjs.so
#  so that -lsmjs will suffice.
#  Some distros, e.g. Debian, already do this for you.
#  You can use the target jslink below.
#  Some folks need to add -lcrypto to this list.

LIBS = -lpcre -lm -lssl -lsmjs

#  Make the dynamically linked executable program by default.
#  Edbrowse executable.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
	html.o format.o cookies.o stringfile.o jsdom.o jsloc.o messages.o

#  Header file dependencies.
$(EBOBJS) : eb.h eb.p messages.h tcp.h

edbrowse: $(EBOBJS) tcp.o dbstubs.o
	cc $(LFLAGS) -o edbrowse $(EBOBJS) tcp.o dbstubs.o $(LIBS)

#  You probably need to be root to do this.
install:
	install edbrowse /usr/local/bin

#  If you had to build the javascript library yourself,
#  link it into /usr/lib.
#  If it's already there, do nothing.
#  This has to be done as root.
jslink:
	[ -f /usr/lib/libsmjs.so ] || ln -s /usr/local/js/src/Linux_All_DBG.OBJ/libjs.so /usr/lib/libsmjs.so

dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

dbinfx.o dbops.o : dbapi.h eb.h

edbrowseinf: $(EBOBJS) tcp.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowseinf $(EBOBJS) tcp.o dbops.o dbinfx.o $(LIBS)

edbrowseodbc: $(EBOBJS) tcp.o dbops.o dbodbc.o
	cc $(LFLAGS) -o edbrowseodbc $(EBOBJS) tcp.o dbops.o dbodbc.o $(LIBS)

#  Build function prototypes.
#  mkproto is my program, not a general unix utility.
proto:
	mkproto -g main.c buffers.c url.c auth.c http.c messages.c \
	sendmail.c fetchmail.c html.c \
	format.c cookies.c stringfile.c jsdom.c jsloc.c dbstubs.c >eb.p

#  I've had no luck getting this to work - can you help?
edbrowse.static: $(EBOBJS) tcp.o
	cc --static $(LFLAGS) -o edbrowse.static tcp.o $(EBOBJS) $(LIBS)

clean:
	rm -f *.o edbrowse edbrowseinf edbrowseodbc

