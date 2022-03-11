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
	    printf -- "$2/libquickjs.a -ldl -latomic\n"
	else
	    printf -- "$2/libquickjs.a -ldl\n"
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
