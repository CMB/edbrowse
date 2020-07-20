# Find Tidy
# Find the native TIDY includes and library.
# Once done this will define
#
#  TIDY_FOUND          - True if tidy found.
#  TIDY_INCLUDE_DIRS   - where to find tidy.h, etc.
#  TIDY_LIBRARIES      - List of libraries when using tidy.
#
# An includer may set TIDY_ROOT to a Tidy installation root to tell
# this module where to look.
#
# 20150921 - Remove completely the devel '5'
# 

set(_TIDY_SEARCHES ${CMAKE_INSTALL_PREFIX})

# Search TIDY_ROOT first if it is set.
if (TIDY_ROOT)
  set(_TIDY_SEARCH_ROOT PATHS ${TIDY_ROOT})
  list(INSERT _TIDY_SEARCHES 0 ${_TIDY_SEARCH_ROOT})
endif()
set( _TIDY_ROOT $ENV{TIDY_ROOT} )
if (_TIDY_ROOT)
  list(INSERT _TIDY_SEARCHES 0 ${_TIDY_ROOT})
endif ()

set(TIDY_NAMES tidy)
if (${CMAKE_SYSTEM_NAME} STREQUAL  "FreeBSD")
    set(TIDY_NAMES tidy5)
endif ()

if (_TIDY_SEARCHES)
    # Try each search configuration.
    message(STATUS "+++ Search using paths ${_TIDY_SEARCHES}")
    if (MSVC)
        foreach(search ${_TIDY_SEARCHES})
          find_path(TIDY_INCLUDE_DIR
            NAMES tidy.h
            PATHS ${search}
            PATH_SUFFIXES include include/tidy
            )
          # search for the STATIC version first
          #find_library(TIDY_LIBRARY_DBG
          #  NAMES tidysd
          #  PATHS ${search}
          #  PATH_SUFFIXES lib
          #  )
          find_library(TIDY_LIBRARY_REL
            NAMES tidys
            PATHS ${search}
            PATH_SUFFIXES lib
            )
        endforeach()
        if (TIDY_LIBRARY_DBG AND TIDY_LIBRARY_REL)
            set(TIDY_LIBRARY
                debug ${TIDY_LIBRARY_DBG}
                optimized ${TIDY_LIBRARY_REL}
                )
        elseif (TIDY_LIBRARY_REL)
            set(TIDY_LIBRARY ${TIDY_LIBRARY_REL} )
        endif ()
        if (NOT TIDY_LIBRARY)
            foreach(search ${_TIDY_SEARCHES})
              find_path(TIDY_INCLUDE_DIR
                NAMES tidy.h
                PATHS ${search}
                PATH_SUFFIXES include include/tidy
                )
              find_library(TIDY_LIBRARY
                NAMES ${TIDY_NAMES}
                PATHS ${search}
                PATH_SUFFIXES lib
                )
            endforeach()
        endif ()
    else ()
        find_path(TIDY_INCLUDE_DIR
          NAMES tidy.h
          PATHS ${_TIDY_SEARCHES}
          PATH_SUFFIXES include include/tidy
          NO_DEFAULT_PATH)
        find_library(TIDY_LIBRARY
          NAMES ${TIDY_NAMES}
          PATHS ${_TIDY_SEARCHES}
          PATH_SUFFIXES lib
          NO_DEFAULT_PATH)
    endif ()
endif ()

if (NOT TIDY_LIBRARY OR NOT TIDY_INCLUDE_DIR)
    message(STATUS "+++ Default search with no search paths")
    find_path(TIDY_INCLUDE_DIR
        NAMES tidy.h
        PATH_SUFFIXES include include/tidy
    )
    find_library(TIDY_LIBRARY
        NAMES ${TIDY_NAMES}
        PATH_SUFFIXES lib
    )
endif ()
###mark_as_advanced(TIDY_LIBRARY TIDY_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set TIDY_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(TIDY REQUIRED_VARS TIDY_LIBRARY TIDY_INCLUDE_DIR
                                       VERSION_VAR TIDY_VERSION_STRING)

if(TIDY_FOUND)
    set(TIDY_INCLUDE_DIRS ${TIDY_INCLUDE_DIR})
    set(TIDY_LIBRARIES ${TIDY_LIBRARY})
endif()

# eof
