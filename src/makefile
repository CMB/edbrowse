#  compiler and flags
CC ?=		cc
CXX ?=		c++
QUICKJS_DIR ?= ../../quickjs
#  I would simply export QUICKJS_DIR, but that doesn't work
#  through $(shell in some versions of gnu make.
CFLAGS +=	-I$(QUICKJS_DIR) -Wall -Wno-unused -D_FILE_OFFSET_BITS=64
PLATFORM_CFLAGS = $(shell ./make-helper.sh --platform-cflags)
CFLAGS += $(PLATFORM_CFLAGS)
#  these includes may be redundent with the standard cc includes.
ifeq ($(BUILD_EDBR_ODBC), on)
INCLUDES = $(shell pkg-config --cflags-only-I libcurl libpcre2-8 tidy odbc)
else
INCLUDES = $(shell pkg-config --cflags-only-I libcurl libpcre2-8 tidy)
endif
CFLAGS += $(INCLUDES)

PERL = $(shell		which perl)
TIDY_LIBS = $(shell	pkg-config --libs tidy)
CURL_LIBS = $(shell	pkg-config --libs libcurl)
PCRE_LIBS = $(shell	pkg-config --libs libpcre2-8)
BUILD_EDBR_ODBC ?= off
EDBR_ODBC_OBJS = $(shell ./make-helper.sh --odbc-objs "${BUILD_EDBR_ODBC}")
EDBR_ODBC_LIBS = $(shell ./make-helper.sh --odbc-libs "${BUILD_EDBR_ODBC}")

#  Set EBDEMIN to a nonempty string to support dynamic js deminimization
EDBR_JS_ASSETS = $(shell ./make-helper.sh --js-assets "${EBDEMIN}")

# If EBDEBUG is set to a non-empty string, build with debug flags and
# don't strip executables.
DEBUGFLAGS = $(shell ./make-helper.sh --debugflags "${EBDEBUG}")
STRIP = $(shell ./make-helper.sh --strip "${EBDEBUG}")
CFLAGS += $(DEBUGFLAGS)

#  Libraries and linker flags for edbrowse.
LDFLAGS = $(STRIP) $(PCRE_LIBS) $(CURL_LIBS) -lreadline $(TIDY_LIBS) $(EDBR_ODBC_LIBS) -lpthread -lm
# LDFLAGS for quickjs loading.
QUICKJS_LDFLAGS = $(shell QUICKJS_DIR='$(QUICKJS_DIR)' ./make-helper.sh --js-libs)

#  ESQL C load flags
#ESQLDFLAGS = $(STRIP) -Xlinker -rpath -Xlinker $(INFORMIXDIR)/lib:$(INFORMIXDIR)/lib/esql
#  but it's better to put those two directories into /etc/ld.so.conf and then run ldconfig
ESQLDFLAGS = $(STRIP)

#  Make the dynamically linked executable program by default.
all: edbrowse

#  edbrowse objects
EBOBJS = main.o buffers.o sendmail.o fetchmail.o \
	html.o format.o plugin.o ebrc.o \
	messages.o stringfile.o html-tidy.o decorate.o \
	msg-strings.o http.o isup.o css.o startwindow.o $(EDBR_ODBC_OBJS)

#  Header file dependencies.
$(EBOBJS) : eb.h ebprot.h messages.h
jseng-duk.o jseng-quick.o : eb.h ebprot.h messages.h
dbodbc.o dbinfx.o dbops.o : dbapi.h

startwindow.c: $(EDBR_JS_ASSETS)
	$(PERL) ../tools/buildsourcestring.pl $(EDBR_JS_ASSETS) startwindow.c

ebrc.c: ../lang/ebrc-* ../doc/usersguide*.html
	cd .. ; $(PERL) ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; $(PERL) ./tools/buildmsgstrings.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) jseng-quick.o
	$(CC) $(EBOBJS) jseng-quick.o $(QUICKJS_LDFLAGS) $(LDFLAGS)  -o $@

PREFIX ?=	/usr/local
bindir = $(PREFIX)/bin
#  You probably need to be root to do this.
install:
	install -Dm755 edbrowse $(DESTDIR)$(bindir)/edbrowse

#  native Informix library for database access.
#  Others could be built, e.g. Oracle, but odbc is the most general.
dbinfx.o : dbinfx.ec
	esql -c dbinfx.ec

#  Informix executable
edbrowse-infx: $(EBOBJS) dbops.o dbinfx.o jseng-duk.o
	esql $(ESQLDFLAGS) -o edbrowse-infx $(EBOBJS) dbops.o dbinfx.o $(LDFLAGS) -lduktape

clean:
	rm -f *.o edbrowse edbrowseduk \
	startwindow.c ebrc.c msg-strings.c

#  some hello world targets, for testing and debugging

js_hello_duk: js_hello_duk.c
	$(CC) js_hello_duk.c -o js_hello_duk -lduktape -lm

#  need packages nodejs and libnode-dev
js_hello_v8 : js_hello_v8.cpp
	g++ -I/usr/include/v8 js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

js_hello_quick : js_hello_quick.c
	gcc $(CFLAGS) js_hello_quick.c stringfile.o messages.o msg-strings.o ebrc.o format.o $(QUICKJS_LDFLAGS) -o js_hello_quick -lm -lpthread

hello: js_hello_quick

