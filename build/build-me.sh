#!/bin/sh
#< build-me.sh - 20150925 - for edbrowse project
BN="`basename $0`"
BLDLOG="bldlog-1.txt"

if [ -f "$BLDLOG" ]; then
    rm -f "$BLDLOG"
fi

##############################################
### ***** NOTE THIS INSTALL LOCATION ***** ###
### Change to suit your taste, environment ###
##############################################
: ${TMPOPTS:="-DCMAKE_INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX:-$HOME}\""}
#############################################
# Use -DCMAKE_BUILD_TYPE=Debug to add gdb symbols
# Use -DCMAKE_VERBOSE_MAKEFILE=ON

# correctly quoted $@ is the default for shell for loops
for arg; do
    case "$arg" in
       VERBOSE) TMPOPTS="$TMPOPTS -DCMAKE_VERBOSE_MAKEFILE=ON";;
        DEBUG) TMPOPTS="$TMPOPTS -DCMAKE_BUILD_TYPE=Debug";;
        *) TMPOPTS="$TMPOPTS $arg";;
    esac
done

echo "$BN: Doing: 'cmake .. $TMPOPTS' to $BLDLOG"
if ! eval cmake .. $TMPOPTS >> $BLDLOG 2>&1; then
    echo "$BN: cmake confiuration, generation error"
    exit 1
fi

echo "$BN: Doing: 'make' to $BLDLOG"
if ! make >> $BLDLOG 2>&1; then
    echo "$BN: make error - see $BLDLOG for details"
    exit 1
fi

echo ""
echo "$BN: appears a successful build... see $BLDLOG for details"
echo ""
echo "$BN: Time for 'make install' IF desired... to ${CMAKE_INSTALL_PREFIX:-$HOME}/bin"
