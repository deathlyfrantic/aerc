cmake_minimum_required(VERSION 3.1.0)

project(aerc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

option(enable-openssl "Enables OpenSSL support" YES)

if(enable-openssl)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        add_definitions(-DUSE_OPENSSL)
    endif()
endif()

FILE(GLOB src ${PROJECT_SOURCE_DIR}/src/*.c)
FILE(GLOB util ${PROJECT_SOURCE_DIR}/src/util/*.c)
FILE(GLOB imap ${PROJECT_SOURCE_DIR}/src/imap/*.c)
FILE(GLOB imap_worker ${PROJECT_SOURCE_DIR}/src/imap/worker/*.c)

add_executable(aerc
    ${src}
    ${util}
    ${imap}
    ${imap_worker}
)

TARGET_LINK_LIBRARIES(aerc pthread ${OPENSSL_LIBRARIES})
