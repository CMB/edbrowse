# Default settings:
CC ?=		cc
CXX ?=		c++
PREFIX ?=	/usr/local
bindir = $(prefix)/bin
CFLAGS +=	-I/usr/local/include -Wall -Wextra -D_FILE_OFFSET_BITS=64
CXXFLAGS +=	-I/usr/local/include -Wall -Wextra -D_FILE_OFFSET_BITS=64
BUILD_EDBR_ODBC ?= off
PLATFORM_CFLAGS != ./make-helper.sh --platform-cflags
CFLAGS += $(PLATFORM_CFLAGS)
PERL !=		which perl
TIDY_LIBS !=	pkg-config --libs tidy
CURL_LIBS !=	pkg-config --libs libcurl
PCRE_LIBS !=	pkg-config --libs libpcre
EDBR_ODBC_OBJS != ./make-helper.sh --odbc-objs "${BUILD_EDBR_ODBC}"
EDBR_ODBC_LIBS != ./make-helper.sh --odbc-libs "${BUILD_EDBR_ODBC}"
#  Set EBDEMIN to a nonempty string to support dynamic js deminimization
EDBR_JS_ASSETS != ./make-helper.sh --js-assets "${EBDEMIN}"

# If EBDEBUG is set to a non-empty string, build with debug flags and
# don't strip executables.
DEBUGFLAGS != ./make-helper.sh --debugflags "${EBDEBUG}"
STRIP != ./make-helper.sh --strip "${EBDEBUG}"
CFLAGS += $(DEBUGFLAGS)
CXXFLAGS += $(DEBUGFLAGS)

#  Libraries and linker flags for edbrowse.
LDFLAGS = $(STRIP) $(PCRE_LIBS) $(CURL_LIBS) -lreadline $(TIDY_LIBS) $(EDBR_ODBC_LIBS) -lpthread -lm

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
jseng-duk.o jseng-moz.o : eb.h ebprot.h messages.h
dbodbc.o dbinfx.o dbops.o : dbapi.h

startwindow.c: $(EDBR_JS_ASSETS)
	$(PERL) ../tools/buildsourcestring.pl $(EDBR_JS_ASSETS) startwindow.c

ebrc.c: ../lang/ebrc-* ../doc/usersguide*.html
	cd .. ; $(PERL) ./tools/buildebrcstring.pl

msg-strings.c: ../lang/msg-*
	cd .. ; $(PERL) ./tools/buildmsgstrings.pl

# The implicit linking rule isn't good enough, because we don't have an
# edbrowse.o object, and it expects one.
edbrowse: $(EBOBJS) jseng-duk.o
	$(CC) $(EBOBJS) jseng-duk.o -lduktape $(LDFLAGS) -o $@

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
	rm -f *.o edbrowse edbrowseqk edbrowsesm \
	startwindow.c ebrc.c msg-strings.c

#  The mozilla version, highly experimental
#  need packages libmozjs-52-0 and libmozjs-52-dev, or higher.
#  Set SMV to your spider monkey version.
SMV ?= 52
#  I added -Wno-psabi to supress warning   changed in GCC 7.1
#  https://stackoverflow.com/questions/48149323/what-does-the-gcc-warning-project-parameter-passing-for-x-changed-in-gcc-7-1-m
CXXFLAGS += -Wno-psabi -I/usr/include/mozjs-$(SMV)
#  For now a separate target, not built by all.
edbrowsesm: $(EBOBJS) jseng-moz.o
	$(CC) $(EBOBJS) jseng-moz.o $(LDFLAGS) -lmozjs-$(SMV) -lstdc++ -o $@

edbrowseqk: $(EBOBJS) jseng-quick.o
	$(CC) $(EBOBJS) jseng-quick.o $(QUICKJS_LDFLAGS) $(LDFLAGS)  -o $@

#  some hello world targets, for testing and debugging

js_hello_duk: js_hello_duk.c
	$(CC) js_hello_duk.c -o js_hello_duk -lduktape -lm

#  need packages nodejs and libnode-dev
js_hello_v8 : js_hello_v8.cpp
	g++ -I/usr/include/v8 js_hello_v8.cpp -lv8 -lstdc++ -o js_hello_v8

HELLOEXTRA = stringfile.o messages.o msg-strings.o startwindow.o ebrc.o format.o http.o isup.o fetchmail.o sendmail.o plugin.o buffers.o dbstubs.o html.o decorate.o html-tidy.o css.o
js_hello_moz : js_hello_moz.o $(HELLOEXTRA) jseng-moz.o
	$(CC) js_hello_moz.o $(HELLOEXTRA) jseng-moz.o $(LDFLAGS) -lmozjs-$(SMV) -lstdc++ -o $@

js_hello_quick : js_hello_quick.c
	gcc $(CFLAGS) js_hello_quick.c stringfile.o messages.o msg-strings.o ebrc.o format.o -o js_hello_quick -L/usr/local/lib/quickjs -lquickjs -lm -ldl -lpthread -latomic

hello: js_hello_duk js_hello_v8 js_hello_moz js_hello_quick

