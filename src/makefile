#  This is the makefile for edbrowse.

prefix = /usr/local
bindir = $(prefix)/bin

#  Flags for gcc compilation.
#  This assumes js and js-devel have been installed.
#  These are available packages on most distros.
#  You also need pcre[-devel] and curl[-devel]

JS_CXXFLAGS =-I/usr/include/mozjs-24


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
JSLIB = -lmozjs-24
LDLIBS = -lpcre -lcurl -lreadline -lncurses

#  Make the dynamically linked executable program by default.
all: edbrowse edbrowse-js

#  edbrowse objects
EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
	html.o format.o cookies.o stringfile.o messages.o ebjs.o

#  Header file dependencies.
$(EBOBJS) : eb.h eb.p messages.h ebjs.h

startwindow.c: startwindow.js
	../tools/buildstartwindow

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) startwindow.o dbstubs.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse
	install -Dm755 edbrowse-js $(DESTDIR)$(bindir)/edbrowse-js

#  Database files.
dbodbc.o dbinfx.o dbops.o : dbapi.h eb.h eb.p

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowseinf: $(EBOBJS) startwindow.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowseinf $(EBOBJS) startwindow.o dbops.o dbinfx.o $(LDLIBS)

#  odbc access
edbrowse-odbc: LDLIBS += -lodbc
edbrowse-odbc: $(EBOBJS) startwindow.o dbops.o dbodbc.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f *.o edbrowse edbrowseinf edbrowse-odbc edbrowse-js startwindow.c

jseng-moz.o: jseng-moz.cpp
	$(CXX) -c $(CXXFLAGS) $(JS_CXXFLAGS) $^ -o $@

edbrowse-js : jseng-moz.o
	$(LINK.o) $^ $(LOADLIBES) $(JSLIB) -lstdc++ -o $@

#  some hello world targets, for testing and debugging

js_hello_moz : js_hello_moz.cpp
	gcc $(JS_CXXFLAGS) js_hello_moz.cpp $(JSLIB) -lstdc++ -o js_hello_moz

js_hello_v8 : js_hello_v8.cpp
	gcc js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

