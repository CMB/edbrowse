# CMakeLists.txt generated 2015/09/20 00:38:06
# by make2cmake.pl from F:\Projects\edbrowse\makefile

cmake_minimum_required (VERSION 2.8.8)

project (edbrowse)

# local CMake Scripts
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMakeModules)

# ### NOTE: from sersguide.htm
set( eb_MAJOR 3 )
set( eb_MINOR 5 )
set( eb_POINT 1 )

# Allow developer to select is Dynamic or static library built
set( LIB_TYPE STATIC )  # set default static
option( BUILD_SHARED_LIB "Build Shared Library" OFF )


if(CMAKE_COMPILER_IS_GNUCXX)
    set( WARNING_FLAGS -Wall )
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") 
   set( WARNING_FLAGS "-Wall -Wno-overloaded-virtual" )
endif() 

if(WIN32)
    if(MSVC)
        # turn off various warnings
        set(WARNING_FLAGS "${WARNING_FLAGS} /wd4996")
        # foreach(warning 4244 4251 4267 4275 4290 4786 4305)
        # C4244: '=' : conversion from 'long' to 'unsigned char', possible loss of data
        # C4090: 'function' : different 'const' qualifiers
        # C4101: 'idx' : unreferenced local variable
        # C4018: '>' : signed/unsigned mismatch
        foreach(warning 4244 4101 4090 4018)
            set(WARNING_FLAGS "${WARNING_FLAGS} /wd${warning}")
        endforeach()
        set( MSVC_FLAGS "-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS" )
        # if (${MSVC_VERSION} EQUAL 1600)
        #    set( MSVC_LD_FLAGS "/FORCE:MULTIPLE" )
        # endif (${MSVC_VERSION} EQUAL 1600)
        # distinguish between debug and release libraries
        set( CMAKE_DEBUG_POSTFIX "d" )
    endif(MSVC)
    list(APPEND extra_LIBS ws2_32 Winmm)
    set( NOMINMAX 1 )
else()
    # unix/mac stuff
    message(STATUS "*** Some of these gcc warnings should be looked at, and FIXED!")
    set(WARNING_FLAGS "${WARNING_FLAGS} -Wno-char-subscripts -Wno-parentheses -Wno-unused-but-set-variable -Wno-unused-variable -Wno-pointer-sign -Wno-unused-function -Wno-implicit-function-declaration -Wno-format -Wno-return-type")
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} ${MSVC_FLAGS} -D_REENTRANT" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} ${MSVC_FLAGS} -D_REENTRANT" )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MSVC_LD_FLAGS}" )

#  TIDY_FOUND          - True if tidy found.
#  TIDY_INCLUDE_DIRS   - where to find tidy.h, etc.
#  TIDY_LIBRARIES      - List of libraries when using tidy.
find_package( Tidy )
if (TIDY_FOUND)
    message(STATUS "*** Tidy found inc ${TIDY_INCLUDE_DIRS} lib ${TIDY_LIBRARIES}")
    include_directories( ${TIDY_INCLUDE_DIRS} )
    list(APPEND add_LIBS ${TIDY_LIBRARIES})
else ()
    message(FATAL_ERROR "*** Tidy NOT found! Maybe set ENV TIDY_ROOT to install location")
endif ()

#  CURL_FOUND          - True if curl found.
#  CURL_INCLUDE_DIRS   - where to find curl/curl.h, etc.
#  CURL_LIBRARIES      - List of libraries when using curl.
#  CURL_VERSION_STRING - the version of curl found (since CMake 2.8.8)
find_package(CURL)
if (CURL_FOUND)
    message(STATUS "*** CURL found inc ${CURL_INCLUDE_DIRS} lib ${CURL_LIBRARIES}")
    include_directories( ${CURL_INCLUDE_DIRS} )
    list( APPEND add_LIBS ${CURL_LIBRARIES} )
    message(STATUS "cURL version ${CURL_VERSION_STRING}")
else ()
    message(FATAL_ERROR "*** CURL NOT FOUND")
endif ()

#    PCRE_FOUND - True if libpcre is found
#    PCRE_LIBRARY - A variable pointing to the PCRE library
#    PCRE_INCLUDE_DIR - Where to find the headers
find_package(PCRE)
if (PCRE_FOUND)
    message(STATUS "*** PCRE found inc ${PCRE_INCLUDE_DIR} lib ${PCRE_LIBRARY}")
    include_directories( ${PCRE_INCLUDE_DIR} )
    list( APPEND add_LIBS ${PCRE_LIBRARY} )
    #message(STATUS "PCRE version ${PCREL_VERSION_STRING}")
else ()
    message(FATAL_ERROR "*** PCRE NOT FOUND")
endif ()

# READLINE_FOUND
# READLINE_INCLUDE_DIR
# READLINE_LIBRARY
# HAVE_READLINE_H
find_package(Readline)
if (READLINE_FOUND)
    message( STATUS "*** READLINE found inc ${READLINE_INCLUDE_DIR} lib ${READLINE_LIBRARY}" )
    add_definitions( -DHAVE_READLINE_H )
    if (NOT BUILD_SHARED_LIB)
        add_definitions( -DREADLINE_STATIC )
    endif ()
    include_directories( ${READLINE_INCLUDE_DIR} )
    list( APPEND add_LIBS ${READLINE_LIBRARY} )
else ()
    message(FATAL_ERROR "*** READLINE NOT FOUND")
endif ()

# add_definitions( -DHAVE_CONFIG_H )

if(BUILD_SHARED_LIB)
   set(LIB_TYPE SHARED)
   message(STATUS "*** Building DLL library ${LIB_TYPE}")
else(BUILD_SHARED_LIB)
   message(STATUS "*** Building static library ${LIB_TYPE}")
endif(BUILD_SHARED_LIB)

# LDLIBS = -lpcre -lcurl -lreadline -lncurses -ltidy
#  edbrowse objects
#EBOBJS = main.o buffers.o url.o auth.o http.o sendmail.o fetchmail.o \
#	html.o format.o cookies.o stringfile.o messages.o ebjs.o plugin.o
#  Header file dependencies.
#$(EBOBJS) : eb.h ebprot.h messages.h ebjs.h

# see tools/buildsourcestring.pl
set(dir src)
set(GEN_SOURCES
    ${CMAKE_SOURCE_DIR}/${dir}/startwindow.c
    ${CMAKE_SOURCE_DIR}/${dir}/ebrc.c
    )
find_program(PERL perl)
if (PERL)
    add_custom_command(
        OUTPUT ${GEN_SOURCES}
        COMMAND perl -f tools/buildsourcestring.pl src/startwindow.js startWindowJS src/startwindow.c
        COMMAND perl -f tools/buildebrcstring.pl
        DEPENDS doc/ebrc-en doc/ebrc-fr doc/ebrc-pt_br src/startwindow.js 
            tools/buildsourcestring.pl
        COMMENT "Running perl to build ${GEN_SOURCES}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    message(STATUS "*** Setup build of ebrc.c and startwindow.c")
else ()
    message(FATAL_ERROR, "Error: Can NOT locate 'perl'???")
endif ()

set(name edbrowse)
set(dir src)
set(${name}_SRCS
    ${dir}/main.c
    ${dir}/buffers.c
    ${dir}/url.c
    ${dir}/auth.c
    ${dir}/http.c
    ${dir}/sendmail.c
    ${dir}/fetchmail.c
	${dir}/html.c
    ${dir}/format.c
    ${dir}/cookies.c
    ${dir}/stringfile.c
    ${dir}/messages.c
    ${dir}/ebjs.c
    ${dir}/plugin.c
    ${dir}/html-tidy.c
    ${dir}/decorate.c
    ${dir}/dbstubs.c
    ${GEN_SOURCES}
    )
set(${name}_HDRS
    ${dir}/eb.h
    ${dir}/ebprot.h
    ${dir}/messages.h
    ${dir}/ebjs.h
    )
if (MSVC)
    list(APPEND ${name}_SRCS win32/vsprtf.c win32/dirent.c ${dir}/ebrc.c)
    list(APPEND ${name}_HDRS win32/vsprtf.h win32/dirent.h)
    include_directories( win32 )
endif ()
add_executable( ${name} ${${name}_SRCS} ${${name}_HDRS} )
if (add_LIBS OR extra_LIBS)
    target_link_libraries( ${name} ${add_LIBS} ${extra_LIBS} )
endif ()
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif ()

# deal with INSTALL

# eof
