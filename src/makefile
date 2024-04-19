#  compiler and flags
CC ?=		cc
PERL ?= perl
QUICKJS_INCLUDE ?= ../../quickjs
QUICKJS_LIB ?= ../../quickjs
CFLAGS +=	-Wall -Wno-unused -D_FILE_OFFSET_BITS=64
ifeq ($(shell uname),Linux)
	PLATFORM_CFLAGS = -DEDBROWSE_ON_LINUX
endif
CFLAGS += $(PLATFORM_CFLAGS)

# determine includes and linker flags
DEPENDENCIES = libcurl:curl odbc libpcre2-8:pcre2-8 readline openssl
INCLUDES = $(shell ./make-helper.sh pkg-config-includes $(DEPENDENCIES))
LINKER_LIBS = $(shell ./make-helper.sh pkg-config-libs $(DEPENDENCIES))
CPPFLAGS += $(INCLUDES) -I$(QUICKJS_INCLUDE)

#  Set EBDEMIN=on to support dynamic js deminimization
ifneq ($(EBDEMIN),)
	EDBR_JS_ASSETS = shared.js startwindow.js demin.js
else
	EDBR_JS_ASSETS = shared.js startwindow.js endwindow.js
endif

# If EBDEBUG is set to on, or a non-empty string, build with debug flags and
# don't strip executables.
	STRIP = -s
ifneq ($(EBDEBUG),)
	CFLAGS += -g -ggdb -Wextra
	STRIP =
endif

#  EBPROF=on for profiling, probably shouldn't combine with EBDEBUG
ifneq ($(EBPROF),)
	CFLAGS += -g -pg
	STRIP = -pg
endif

# LDFLAGS for quickjs loading.
QUICKJS_LDFLAGS = -L$(QUICKJS_LIB) -lquickjs -ldl
ifeq ($(shell uname),Linux)
	QUICKJS_LDFLAGS += -latomic
endif
LDFLAGS += $(QUICKJS_LDFLAGS)
#  and the other loader flags
LDFLAGS += $(STRIP) $(LINKER_LIBS) -lpthread -lm -lssl -lcrypto

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o sendmail.o fetchmail.o \
	html.o html-tags.o format.o stringfile.o ebrc.o \
	msg-strings.o http.o isup.o css.o startwindow.o dbops.o dbodbc.o \
	jseng-quick.o

#  Header file dependencies.
$(EBOBJS) : eb.h ebprot.h messages.h
dbodbc.o dbinfx.o dbops.o : dbapi.h

startwindow.c: $(EDBR_JS_ASSETS)
	$(PERL) ../tools/buildsourcestring.pl $(EDBR_JS_ASSETS) startwindow.c

ebrc.c: ../lang/ebrc-* ../doc/usersguide*.html
	cd .. ; $(PERL) ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; $(PERL) ./tools/buildmsgstrings.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS)
	$(CC) $(EBOBJS) $(LDFLAGS)  -o $@

PREFIX ?=	/usr/local
#  You probably need to be root to do this.
install:
	mkdir -p -m 755 $(DESTDIR)$(PREFIX)/bin
	install -m755 edbrowse $(DESTDIR)$(PREFIX)/bin
	mkdir -p -m 755 $(DESTDIR)$(PREFIX)/share/doc/edbrowse
	install -m644 ../doc/usersguide.html $(DESTDIR)$(PREFIX)/share/doc/edbrowse

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
#edbrowse-infx: $(EBOBJS) dbops.o dbinfx.o jseng-duk.o
#	esql $(ESQLDFLAGS) -o edbrowse-infx $(EBOBJS) dbops.o dbinfx.o $(LDFLAGS) -lduktape

clean:
	rm -f *.o edbrowse \
	startwindow.c ebrc.c msg-strings.c

#  some hello world targets, for testing and debugging

#  need packages nodejs and libnode-dev
js_hello_v8 : js_hello_v8.cpp
	g++ -I/usr/include/v8 js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

js_hello_quick : js_hello_quick.c stringfile.o msg-strings.o ebrc.o format.o
	$(CC) -I$(QUICKJS_INCLUDE) $(CFLAGS) js_hello_quick.c stringfile.o msg-strings.o ebrc.o format.o $(QUICKJS_LDFLAGS) -o js_hello_quick -lm -lpthread

js0: js0.c
	$(CC) -I$(QUICKJS_INCLUDE) js0.c $(QUICKJS_LIB)/libquickjs.a -lm -latomic -o js0

hello: js_hello_quick js0

