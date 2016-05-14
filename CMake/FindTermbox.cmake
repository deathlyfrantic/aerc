# - Try to find the termbox library
# Once done this will define
#
#  TERMBOX_FOUND - system has termbox
#  TERMBOX_LIBRARIES - Link these to use termbox

if(TERMBOX_LIBRARIES)
    set (TERMBOX_FIND_QUIETLY TRUE)
endif()

FIND_PATH(TERMBOX_INCLUDE_DIRS termbox.h)
FIND_LIBRARY(TERMBOX_LIBRARIES NAMES termbox)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TERMBOX DEFAULT_MSG TERMBOX_LIBRARIES TERMBOX_INCLUDE_DIRS)

if (TERMBOX_FOUND)
    message (STATUS "Found components for Termbox")
    message (STATUS "TERMBOX_LIBRARIES = ${TERMBOX_LIBRARIES}")
else (TERMBOX_FOUND)
    if (TERMBOX_FIND_REQUIRED)
        message (FATAL_ERROR "Could not find Termbox!")
    endif (TERMBOX_FIND_REQUIRED)
endif (TERMBOX_FOUND)

MARK_AS_ADVANCED(TERMBOX_LIBRARIES TERMBOX_INCLUDE_DIRS)
