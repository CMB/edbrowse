#  This is the makefile for edbrowse.

prefix = /usr/local
bindir = $(prefix)/bin

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
#CFLAGS = -I/usr/include/js -DXP_UNIX -DX86_LINUX $(DEBUGFLAGS)

#  Normal load flags
LFLAGS = $(STRIP)

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Libraries for edbrowse.
#  I assume you have linked libjs.so into /usr/lib/libjs.so
#  so that -ljs will suffice.
#  Some distros, e.g. Debian, already do this for you.
#  Or you can use the target jslink below.
#  Some folks need to add -lcrypto to this list.

LIBS = -lpcre -lm -lssl -ljs -lcurl -lreadline

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
	install -Dm755 edbrowse $(bindir)/edbrowse

#  If you had to build the javascript library yourself,
#  link it into /usr/lib.
#  If it's already there, do nothing.
#  This has to be done as root.
jslink:
	[ -f /usr/lib/libjs.so ] || ln -s /usr/local/js/src/Linux_All_DBG.OBJ/libjs.so /usr/lib/libjs.so
	[ -f /usr/lib/libjs.a ] || ln -s /usr/local/js/src/Linux_All_DBG.OBJ/libjs.a /usr/lib/libjs.a

#  Database files.
dbodbc.o dbinfx.o dbops.o : dbapi.h eb.h eb.p

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowseinf: $(EBOBJS) tcp.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowseinf $(EBOBJS) tcp.o dbops.o dbinfx.o $(LIBS)

#  odbc access
edbrowseodbc: $(EBOBJS) tcp.o dbops.o dbodbc.o
	cc $(LFLAGS) -o edbrowseodbc $(EBOBJS) tcp.o dbops.o dbodbc.o $(LIBS) -lodbc

#  Build function prototypes.
#  mkproto is my program, not a general unix utility.
proto:
	mkproto -g main.c buffers.c url.c auth.c http.c messages.c \
	sendmail.c fetchmail.c html.c \
	format.c cookies.c stringfile.c jsdom.c jsloc.c dbstubs.c >eb.p

STATICLIBS = -lodbc -lpcre -lm -ljs -lcurl -lssl -lc -lcrypto -lpthread -lm -ldl -lrt
edbrowse.static: $(EBOBJS) tcp.o dbops.o dbodbc.o
	cc --static $(LFLAGS) -o edbrowse.static $(EBOBJS) tcp.o dbops.o dbodbc.o $(STATICLIBS)

clean:
	rm -f *.o edbrowse edbrowseinf edbrowseodbc edbrowse.static

