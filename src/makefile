#  This is the makefile for edbrowse.

prefix = /usr/local
bindir = $(prefix)/bin

#  Flags for gcc compilation.
#  This assumes js and js-devel have been installed.
#  These are available packages on fedora, and most other distros.
#  You also need pcre-devel and openssl-devel

#  Early adopters have occasionally needed -DMOZILLA_1_8_BRANCH
#  This issue has cropped up on certain flavors of debian.

JS_CXXFLAGS =-I/usr/include/mozjs

# we need to only use the js flags when building with c++, so use CXXFLAGS
CXXFLAGS += -fpermissive  $(JS_CXXFLAGS)

# By default, we strip the executables.
# Override this behavior on the command line, by setting STRIP to the
# empty string.  E.G., in order to build executables with debugging symbols,
# CFLAGS='-g -ggdb' make STRIP=''

STRIP=-s

#  Normal load flags
LDFLAGS += $(STRIP)

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Libraries for edbrowse.
#  This assumes the javascript library is in /lib/libjs.so
#  This is the case for fedora and debian.

# Override JSLIB on the command-line, if your distro uses a different name.
# E.G., make JSLIB=-lmozjs
JSLIB = -lmozjs
LDLIBS = -lpcre -lm -lssl $(JSLIB) -lcurl -lreadline -lcrypto -lstdc++

#  Make the dynamically linked executable program by default.
#  Edbrowse executable.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
	html.o format.o cookies.o stringfile.o jsdom.o jsloc.o messages.o

#  Header file dependencies.
$(EBOBJS) : eb.h eb.p messages.h tcp.h
html.o jsdom.o jsloc.o : js.h ebjs.p

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) tcp.o dbstubs.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse

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
edbrowseinf: CFLAGS += -DHAVE_SQL
edbrowseinf: $(EBOBJS) tcp.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowseinf $(EBOBJS) tcp.o dbops.o dbinfx.o $(LDLIBS)

#  odbc access
edbrowseodbc: LDLIBS += -lodbc
edbrowseodbc: CFLAGS += -DHAVE_SQL
edbrowseodbc: $(EBOBJS) tcp.o dbops.o dbodbc.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

#  Build function prototypes.
#  mkproto is Karl's program,
# with source in ../tools/mkproto.c.
proto: ../tools/mkproto
	../tools/mkproto -g main.c buffers.c url.c auth.c http.c messages.c \
	sendmail.c fetchmail.c html.c \
	format.c cookies.c stringfile.c jsdom.c jsloc.c dbstubs.c >eb.p

# Can't think of a better way to insure that mkproto is built:
../tools/mkproto: ../tools/mkproto.c
	$(MAKE) --directory=../tools

STATICLIBS = -lodbc -lpcre -lm -ljs -lcurl -lssl -lc -lcrypto -lpthread -lm -ldl -lrt
edbrowse.static: $(EBOBJS) tcp.o dbops.o dbodbc.o
	cc --static $(LFLAGS) -o edbrowse.static $(EBOBJS) tcp.o dbops.o dbodbc.o $(STATICLIBS)

clean:
	rm -f *.o edbrowse edbrowseinf edbrowseodbc edbrowse.static

