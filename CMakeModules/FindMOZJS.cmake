# ################################################
# MOZJS library finder
# 20151018 - Added to the edbrowser project
# Defines
# MOZJS_FOUND
# MOZJS_LIBRARY
# MOZJS_INCLUDE_DIR
# ################################################
set(_LIB_NAME mozjs-24)

FIND_PATH(MOZJS_INCLUDE_DIR jsapi.h
    PATH_SUFFIXES include/mozjs-24 include/mozjs 
  )
  
if (MSVC)
    FIND_LIBRARY(MOZJS_LIB_DBG NAMES ${_LIB_NAME}d)
    FIND_LIBRARY(MOZJS_LIB_REL NAMES ${_LIB_NAME})
    if (MOZJS_LIB_DBG AND MOZJS_LIB_REL)
        set(MOZJS_LIBRARY
            debug ${MOZJS_LIB_DBG}
            optimized ${MOZJS_LIB_REL}
            )
    else ()
        if (MOZJS_LIB_REL)
            set(MOZJS_LIBRARY ${MOZJS_LIB_REL})
        endif ()
    endif ()
else ()
    FIND_LIBRARY(MOZJS_LIBRARY NAMES ${_LIB_NAME})
endif ()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MOZJS DEFAULT_MSG MOZJS_INCLUDE_DIR MOZJS_LIBRARY )

# eof
