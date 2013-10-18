# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/lib)

## -----------------------------------------------------------------------------
## --SECTION--                                                       EXECUTABLES
## -----------------------------------------------------------------------------

add_executable(
    ${BIN_ARANGOSH}
    ArangoShell/ArangoClient.cpp
    V8Client/ImportHelper.cpp
    V8Client/V8ClientConnection.cpp
    V8Client/arangosh.cpp
)

target_link_libraries(
    ${BIN_ARANGOSH}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
