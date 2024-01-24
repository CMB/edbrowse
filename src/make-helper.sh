#!/bin/sh

if [ $# -lt 1 ]; then
	printf %s\\n "Usage: $0 <command>[args]" >&2
	exit 1
fi

# make sure pkg-config is installed
if ! type pkg-config >/dev/null 2>&1 ; then echo pkg-config-not-installed; exit 0; fi

cmd="$1"
shift
case "${cmd}" in
	pkg-config-includes)
        pkg_config_libs=""
        for lib in "$@";do
            pkg_config_name="${lib%:*}"
            if pkg-config --exists "${pkg_config_name}"; then
                pkg_config_libs="${pkg_config_libs} ${pkg_config_name}"
            fi
        done
        pkg-config --cflags-only-I "${pkg_config_libs}"
        ;;
	pkg-config-libs)
        pkg_config_libs=""
        other_libs=""
        for lib in "$@";do
            pkg_config_name="${lib%:*}"
            lib_name="${lib#*:}"
            if pkg-config --exists "${pkg_config_name}"; then
                pkg_config_libs="${pkg_config_libs} ${pkg_config_name}"
            else
                other_libs="${other_libs} -l${lib_name}"
            fi
        done
        printf %s\\n "$(pkg-config --libs "${pkg_config_libs}") ${other_libs}"
        ;;

	*)
		printf %s\\n "Unknown request $1" 1>&2
		exit 1
		;;
esac
