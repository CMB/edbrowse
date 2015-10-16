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
# Override JSLIB on the command-line, if your distro uses a different name.
# E.G., make JSLIB=-lmozjs
JSLIB = -lmozjs-24
LDLIBS = -lpcre -lcurl -lreadline -lncurses -ltidy

#  Make the dynamically linked executable program by default.
all: edbrowse edbrowse-js

#  edbrowse objects
EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
	html.o format.o cookies.o stringfile.o messages.o \
	ebjs.o plugin.o html-tidy.o decorate.o ebrc.o

#  Header file dependencies.
$(EBOBJS) jseng-moz.o : eb.h ebprot.h messages.h ebjs.h

startwindow.c: startwindow.js
	cd .. ; ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/startwindow.c

ebrc.c: ../lang/ebrc-*
	cd .. ; ./tools/buildebrcstring.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) startwindow.o dbstubs.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse
	install -Dm755 edbrowse-js $(DESTDIR)$(bindir)/edbrowse-js

#  Database files.
dbodbc.o dbinfx.o dbops.o : dbapi.h eb.h ebprot.h

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowse-infx: $(EBOBJS) startwindow.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowse-infx $(EBOBJS) startwindow.o dbops.o dbinfx.o $(LDLIBS)

#  odbc access
edbrowse-odbc: LDLIBS += -lodbc
edbrowse-odbc: $(EBOBJS) startwindow.o dbops.o dbodbc.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	rm -f *.o edbrowse edbrowse-infx edbrowse-odbc edbrowse-js startwindow.c

jseng-moz.o: jseng-moz.cpp
	$(CXX) -c $(CXXFLAGS) $(JS_CXXFLAGS) jseng-moz.cpp -o $@


edbrowse-js : jseng-moz.o messages.o url.o stringfile.o html-tidy.o decorate.o
	$(LINK.o) $^ $(LOADLIBES) $(JSLIB) -ltidy -lstdc++ -o $@

#  some hello world targets, for testing and debugging

js_hello_moz : js_hello_moz.cpp
	gcc $(JS_CXXFLAGS) js_hello_moz.cpp $(JSLIB) -lstdc++ -o js_hello_moz

js_hello_v8 : js_hello_v8.cpp
	gcc js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

