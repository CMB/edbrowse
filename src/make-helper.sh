#!/bin/sh
case "$1" in
    --platform-cflags)
	if [ "$(uname)" = Linux ] ; then
	    printf -- '-DEDBROWSE_ON_LINUX\n'
	fi
	;;
    --js-assets)
	if [ -n "$2" ] ; then
	    printf -- 'shared.js startwindow.js demin.js\n'
	else
	    printf -- 'shared.js startwindow.js endwindow.js\n'
	fi
	;;
    --js-libs)
#  certain operating systems require -latomic
	if [ "$(uname)" = Linux ] ; then
	    printf -- '../../quickjs/libquickjs.a -ldl -latomic\n'
	else
	    printf -- '../../quickjs/libquickjs.a -ldl\n'
	fi
	;;
    --odbc-objs)
	if [ "$2" = on ] ; then
	    printf -- 'dbodbc.o dbops.o\n'
	else
	    printf -- 'dbstubs.o\n'
	fi
	;;
    --odbc-libs)
	if [ "$2" = on ] ; then
	    printf -- '-lodbc\n'
	fi
	;;
    --debugflags)
	if [ -n "$2" ] ; then
	    printf -- '-g -ggdb -Wextra\n'
	fi
	;;
    --strip)
	if [ -z "$2" ] ; then
	    printf -- '-s\n'
	fi
	;;
    *)
	printf -- "Unknown request $1\n" 1>&2
	exit 1
	;;
esac
