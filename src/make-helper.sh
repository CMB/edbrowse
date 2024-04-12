#!/bin/sh

if [ $# -lt 1 ]; then
	printf %s\\n "Usage: $0 <command>[args]" >&2
	exit 1
fi

[ -n "$PKG_CONFIG" ] || PKG_CONFIG=pkg-config

# make sure pkg-config is installed
if ! type "$PKG_CONFIG" >/dev/null 2>&1 ; then echo pkg-config-not-installed; exit 0; fi

cmd="$1"
shift
case "${cmd}" in
	pkg-config-includes)
        pkg_config_libs=""
        for lib in "$@";do
            pkg_config_name="${lib%:*}"
            if "$PKG_CONFIG" --exists "${pkg_config_name}"; then
                pkg_config_libs="${pkg_config_libs} ${pkg_config_name}"
            fi
        done
        "$PKG_CONFIG" --cflags-only-I "${pkg_config_libs}"
        ;;
	pkg-config-libs)
        pkg_config_libs=""
        other_libs=""
        for lib in "$@";do
            pkg_config_name="${lib%:*}"
            lib_name="${lib#*:}"
            if "$PKG_CONFIG" --exists "${pkg_config_name}"; then
                pkg_config_libs="${pkg_config_libs} ${pkg_config_name}"
            else
                other_libs="${other_libs} -l${lib_name}"
            fi
        done
        printf %s\\n "$("$PKG_CONFIG" --libs "${pkg_config_libs}") ${other_libs}"
        ;;

	*)
		printf %s\\n "Unknown request $1" 1>&2
		exit 1
		;;
esac
