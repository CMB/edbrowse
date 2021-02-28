#  This is the makefile for edbrowse.

prefix = /usr/local
bindir = $(prefix)/bin

PLATFORM := $(shell uname)
ifeq ($(PLATFORM),Linux)
CFLAGS += -DEDBROWSE_ON_LINUX
endif

#  Flags for gcc compilation.

# By default, we strip the executables.
# Override this behavior by setting EBDEBUG to a nonempty string
STRIP=-s
ifneq ($(EBDEBUG),)
STRIP=
CFLAGS += -g -ggdb
CXXFLAGS += -g -ggdb
endif

#  check for errors and mid level warnings.
CFLAGS += -Wall

# So edbrowse can show you the size of large files
CFLAGS += -D_FILE_OFFSET_BITS=64

#  Normal load flags
LDFLAGS += $(STRIP)

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Libraries for edbrowse.
LDLIBS = -lpcre -lcurl -lreadline -ltidy -lpthread -lm

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o sendmail.o fetchmail.o \
	html.o format.o plugin.o ebrc.o \
	messages.o stringfile.o html-tidy.o decorate.o \
	msg-strings.o http.o isup.o css.o
ifeq ($(BUILD_EDBR_ODBC),on)
EBOBJS += dbodbc.o dbops.o
LDLIBS += -lodbc
else
EBOBJS += dbstubs.o
endif

#  Header file dependencies.
$(EBOBJS) : eb.h ebprot.h messages.h
jseng-duk.o jseng-moz.o : eb.h ebprot.h messages.h
dbodbc.o dbinfx.o dbops.o : dbapi.h

#  Set EBDEMIN to a nonempty string to support dynamic js deminimization
ifneq ($(EBDEMIN),)
startwindow.c: startwindow.js third.js
	cd .. ; ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/third.js thirdJS src/startwindow.c
else
startwindow.c: startwindow.js endwindow.js
	cd .. ; ./tools/buildsourcestring.pl src/startwindow.js startWindowJS src/endwindow.js thirdJS src/startwindow.c
endif

ebrc.c: ../lang/ebrc-* ../doc/usersguide*.html
	cd .. ; ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; ./tools/buildmsgstrings.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) startwindow.o jseng-duk.o
	$(LINK.o) $^ $(LOADLIBES) -lduktape $(LDLIBS) -o $@

#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowse-infx: $(EBOBJS) startwindow.o dbops.o dbinfx.o jseng-duk.o
	esql $(ESQLDFLAGS) -o edbrowse-infx $(EBOBJS) startwindow.o dbops.o dbinfx.o $(LDLIBS) -lduktape

clean:
	rm -f *.o edbrowse \
	startwindow.c ebrc.c msg-strings.c

#  The mozilla version, highly experimental
#  need packages libmozjs-52-0 and libmozjs-52-dev, or higher.
#  Set SMV to your spider monkey version.
ifeq ($(SMV),)
SMV=52
endif
#  I added -Wno-psabi to supress warning   changed in GCC 7.1
#  https://stackoverflow.com/questions/48149323/what-does-the-gcc-warning-project-parameter-passing-for-x-changed-in-gcc-7-1-m
CXXFLAGS += -Wno-psabi -I/usr/include/mozjs-$(SMV)
#  For now a separate target, not built by all.
edbrowsesm: $(EBOBJS) startwindow.o jseng-moz.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -lmozjs-$(SMV) -lstdc++ -o $@

#  some hello world targets, for testing and debugging

js_hello_duk: js_hello_duk.c
	$(CC) js_hello_duk.c -o js_hello_duk -lduktape -lm

#  need packages nodejs and libnode-dev
js_hello_v8 : js_hello_v8.cpp
	g++ -I/usr/include/v8 js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

HELLOEXTRA = stringfile.o messages.o msg-strings.o startwindow.o ebrc.o format.o http.o isup.o fetchmail.o sendmail.o plugin.o buffers.o dbstubs.o html.o decorate.o html-tidy.o css.o
js_hello_moz : js_hello_moz.o $(HELLOEXTRA) jseng-moz.o
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -lmozjs-$(SMV) -lstdc++ -o $@

js_hello_quick : js_hello_quick.c
	gcc $(CFLAGS) js_hello_quick.c stringfile.o messages.o msg-strings.o ebrc.o format.o -o js_hello_quick -L/usr/local/lib/quickjs -lquickjs -lm -ldl -lpthread -latomic

hello: js_hello_duk js_hello_v8 js_hello_moz js_hello_quick

