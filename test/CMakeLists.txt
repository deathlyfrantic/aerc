FILE(GLOB src ${PROJECT_SOURCE_DIR}/src/*.c)
LIST(REMOVE_ITEM src ${PROJECT_SOURCE_DIR}/src/main.c)
FILE(GLOB util ${PROJECT_SOURCE_DIR}/src/util/*.c)
FILE(GLOB email ${PROJECT_SOURCE_DIR}/src/email/*.c)
FILE(GLOB imap ${PROJECT_SOURCE_DIR}/src/imap/*.c)
FILE(GLOB imap_worker ${PROJECT_SOURCE_DIR}/src/imap/worker/*.c)

FILE(GLOB tests ${PROJECT_SOURCE_DIR}/test/*.c)
FILE(GLOB imap_tests ${PROJECT_SOURCE_DIR}/test/imap/*.c)
FILE(GLOB email_tests ${PROJECT_SOURCE_DIR}/test/email/*.c)

include_directories(${CMOCKA_INCLUDE_DIR})
add_definitions(${CMOCKA_DEFINITIONS})

add_executable(tests
    ${src}
    ${tests}
    ${util}
    ${email} ${email_tests}
    ${imap} ${imap_tests}
    ${imap_worker}
)

set(WRAPPED
    "-Wl,--wrap=hashtable_get \
    -Wl,--wrap=poll \
    -Wl,--wrap=ab_recv \
    -Wl,--wrap=absocket_free"
)

set_target_properties(tests
    PROPERTIES
    LINK_FLAGS "${WRAPPED}"
)

target_link_libraries(tests pthread ${CMOCKA_LIBRARIES} ${OPENSSL_LIBRARIES} ${TERMBOX_LIBRARIES})
