# ################################################
# Duktape library finder
# 20170710 - Added to the edbrowser project
# Modified from Geoff's FindMOZJS.cmake.
# Defines
# DUKTAPE_FOUND
# DUKTAPE_LIBRARY
# DUKTAPE_INCLUDE_DIR
# ################################################
set(_LIB_NAME duktape)

FIND_PATH(DUKTAPE_INCLUDE_DIR duk_config.h
  )
if (MSVC)
    FIND_LIBRARY(DUKTAPE_LIB_DBG NAMES ${_LIB_NAME}d)
    FIND_LIBRARY(DUKTAPE_LIB_REL NAMES ${_LIB_NAME})
    if (DUKTAPE_LIB_DBG AND DUKTAPE_LIB_REL)
        set(DUKTAPE_LIBRARY
            debug ${DUKTAPE_LIB_DBG}
            optimized ${DUKTAPE_LIB_REL}
            )
    else ()
        if (DUKTAPE_LIB_REL)
            set(DUKTAPE_LIBRARY ${DUKTAPE_LIB_REL})
        endif ()
    endif ()
else ()
    FIND_LIBRARY(DUKTAPE_LIBRARY NAMES ${_LIB_NAME})
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DUKTAPE DEFAULT_MSG DUKTAPE_INCLUDE_DIR DUKTAPE_LIBRARY )

# eof
