#  This is the makefile for edbrowse.

prefix = /usr/local
bindir = $(prefix)/bin

PLATFORM := $(shell uname)
ifeq ($(PLATFORM),Linux)
CFLAGS += -DEDBROWSE_ON_LINUX
endif

#  Flags for gcc compilation.
#  This assumes js and js-devel have been installed.
#  This is legacy, when edbrowse used mozilla js.
JS_CXXFLAGS =-I/usr/include/mozjs-24

# By default, we strip the executables.
# Override this behavior by setting EBDEBUG to a nonempty string
STRIP=-s
ifneq ($(EBDEBUG),)
STRIP=
CFLAGS += -g -ggdb
CXXFLAGS += -g -ggdb
endif

#  Normal load flags
LDFLAGS += $(STRIP)

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Libraries for edbrowse.
LDLIBS = -lpcre -lcurl -lreadline -lncurses -ltidy -lduktape -lm
#  legacy mozilla js
JSLIB = -lmozjs-24

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o sendmail.o fetchmail.o cache.o \
	html.o format.o cookies.o ebjs.o plugin.o ebrc.o \
	messages.o url.o stringfile.o html-tidy.o decorate.o \
	msg-strings.o http.o auth.o jseng-duk.o
ifeq ($(BUILD_EDBR_ODBC),on)
EBOBJS += dbodbc.o dbops.o
LDLIBS += -lodbc
else
EBOBJS += dbstubs.o
endif

#  Header file dependencies.
$(EBOBJS) : eb.h ebprot.h messages.h ebjs.h
dbodbc.o dbinfx.o dbops.o : dbapi.h

startwindow.c: startwindow.js
	cd .. ; ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/startwindow.c

ebrc.c: ../lang/ebrc-*
	cd .. ; ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; ./tools/buildmsgstrings.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) startwindow.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowse-infx: $(EBOBJS) startwindow.o dbops.o dbinfx.o
	esql $(ESQLDFLAGS) -o edbrowse-infx $(EBOBJS) startwindow.o dbops.o dbinfx.o $(LDLIBS)

clean:
	rm -f *.o edbrowse \
	startwindow.c ebrc.c msg-strings.c

#  legacy mozilla js
jseng-moz.o: jseng-moz.cpp
	$(CXX) -c $(CXXFLAGS) $(JS_CXXFLAGS) jseng-moz.cpp -o $@

#  some hello world targets, for testing and debugging

js_hello_moz : js_hello_moz.cpp
	gcc $(JS_CXXFLAGS) js_hello_moz.cpp $(JSLIB) -lstdc++ -o js_hello_moz

js_hello_v8 : js_hello_v8.cpp
	gcc js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

js_hello_duk: js_hello_duk.c
	$(CC) js_hello_duk.c -o js_hello_duk -lduktape -lm

testsock : socket.c socket.h
	gcc -DTEST_CONNECT socket.c -o testsock

