cmake_minimum_required(VERSION 3.1.0)

project(aerc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")

add_definitions(
	-DSYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
)

option(enable-openssl "Enables OpenSSL support" YES)

list(INSERT CMAKE_MODULE_PATH 0
	${CMAKE_CURRENT_SOURCE_DIR}/CMake
)

if(enable-openssl)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        add_definitions(-DUSE_OPENSSL)
    endif()
endif()

find_package(Termbox REQUIRED)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${TERMBOX_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

FILE(GLOB src ${PROJECT_SOURCE_DIR}/src/*.c)
FILE(GLOB util ${PROJECT_SOURCE_DIR}/src/util/*.c)
FILE(GLOB email ${PROJECT_SOURCE_DIR}/src/email/*.c)
FILE(GLOB imap ${PROJECT_SOURCE_DIR}/src/imap/*.c)
FILE(GLOB imap_worker ${PROJECT_SOURCE_DIR}/src/imap/worker/*.c)

add_executable(aerc
    ${src}
    ${util}
    ${email}
    ${imap}
    ${imap_worker}
)

MESSAGE(STATUS "Termbox: ${TERMBOX_LIBRARIES}")

TARGET_LINK_LIBRARIES(aerc pthread ${OPENSSL_LIBRARIES} ${TERMBOX_LIBRARIES})
