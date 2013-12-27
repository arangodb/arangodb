# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/lib)

## -----------------------------------------------------------------------------
## --SECTION--                                                       EXECUTABLES
## -----------------------------------------------------------------------------

################################################################################
### @brief output directory
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

################################################################################
### @brief arangob
################################################################################

add_executable(
    ${BIN_ARANGOB}
    ArangoShell/ArangoClient.cpp
    Benchmark/arangob.cpp
)

target_link_libraries(
    ${BIN_ARANGOB}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangob binary
################################################################################

install(
  TARGETS ${BIN_ARANGOB}
  RUNTIME DESTINATION ${TRI_BINDIR_INSTALL})

################################################################################
### @brief install arangob config
################################################################################

install_config(arangob)

################################################################################
### @brief arangodump
################################################################################

add_executable(
    ${BIN_ARANGODUMP}
    ArangoShell/ArangoClient.cpp
    V8Client/arangodump.cpp
)

target_link_libraries(
    ${BIN_ARANGODUMP}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangodump binary
################################################################################

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${TRI_BINDIR_INSTALL})

################################################################################
### @brief install arangodump config
################################################################################

install_config(arangodump)

################################################################################
### @brief arangoimp
################################################################################

add_executable(
    ${BIN_ARANGOIMP}
    ArangoShell/ArangoClient.cpp
    V8Client/ImportHelper.cpp
    V8Client/V8ClientConnection.cpp
    V8Client/arangoimp.cpp
)

target_link_libraries(
    ${BIN_ARANGOIMP}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangoimp binary
################################################################################

install(
  TARGETS ${BIN_ARANGOIMP}
  RUNTIME DESTINATION ${TRI_BINDIR_INSTALL})

################################################################################
### @brief install arangoimp config
################################################################################

install_config(arangoimp)

################################################################################
### @brief arangorestore
################################################################################

add_executable(
    ${BIN_ARANGORESTORE}
    ArangoShell/ArangoClient.cpp
    V8Client/arangorestore.cpp
)

target_link_libraries(
    ${BIN_ARANGORESTORE}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangorestore binary
################################################################################

install(
  TARGETS ${BIN_ARANGORESTORE}
  RUNTIME DESTINATION ${TRI_BINDIR_INSTALL})

################################################################################
### @brief install arangorestore config
################################################################################

install_config(arangorestore)

################################################################################
### @brief arangosh
################################################################################

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
    ${MSVC_LIBS}
)

################################################################################
### @brief install arangosh binary
################################################################################

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION ${TRI_BINDIR_INSTALL})

################################################################################
### @brief install arangosh config
################################################################################

install_config(arangosh)

################################################################################
### @brief install foxx-manager binary
################################################################################

install_command_alias(${BIN_ARANGOSH} ${TRI_BINDIR_INSTALL} foxx-manager)

################################################################################
### @brief install foxx-manager config
################################################################################

install_config(foxx-manager)

################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

################################################################################
### @brief check-server
################################################################################

add_executable(
    ${BIN_CHECK_SERVER}
    V8Client/V8ClientConnection.cpp
    V8Client/check-server.cpp
)

target_link_libraries(
    ${BIN_CHECK_SERVER}
    ${LIB_ARANGO_V8}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${OPENSSL_LIBS}
)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
