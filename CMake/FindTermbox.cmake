# - Try to find the termbox library
# Once done this will define
#
#  TERMBOX_FOUND - system has termbox
#  TERMBOX_INCLUDE_DIRS - the termbox
#  TERMBOX_LIBRARIES - Link these to use termbox

INCLUDE(FindPkgConfig)

IF(Termbox_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "REQUIRED")
ELSE(Termbox_FIND_REQUIRED)
        SET(_pkgconfig_REQUIRED "")
ENDIF(Termbox_FIND_REQUIRED)

IF(NOT TERMBOX_FOUND AND NOT PKG_CONFIG_FOUND)
        FIND_PATH(TERMBOX_INCLUDE_DIRS termbox.h)
        FIND_LIBRARY(TERMBOX_LIBRARIES termbox)

        # Report results
        IF(TERMBOX_LIBRARIES AND TERMBOX_INCLUDE_DIRS)
                SET(TERMBOX_FOUND 1)
                IF(NOT Termbox_FIND_QUIETLY)
                        MESSAGE(STATUS "Found Termbox: ${TERMBOX_LIBRARIES}")
                ENDIF(NOT Termbox_FIND_QUIETLY)
        ELSE(TERMBOX_LIBRARIES AND TERMBOX_INCLUDE_DIRS)
                IF(Termbox_FIND_REQUIRED)
                        MESSAGE(SEND_ERROR "Could not find Termbox")
                ELSE(Termbox_FIND_REQUIRED)
                        IF(NOT Termbox_FIND_QUIETLY)
                                MESSAGE(STATUS "Could not find Termbox")
                        ENDIF(NOT Termbox_FIND_QUIETLY)
                ENDIF(Termbox_FIND_REQUIRED)
        ENDIF(TERMBOX_LIBRARIES AND CAIRO_INCLUDE_DIRS)
ENDIF(NOT TERMBOX_FOUND AND NOT PKG_CONFIG_FOUND)

# Hide advanced variables from CMake GUIs
MARK_AS_ADVANCED(TERMBOX_LIBRARIES TERMBOX_INCLUDE_DIRS)
