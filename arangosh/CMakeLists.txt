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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

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
)

install(
  TARGETS ${BIN_ARANGOB}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangob.conf.in
    DESTINATION ${ETCDIR_INSTALL}
    RENAME arangob-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangob.conf
  DESTINATION ${ETCDIR_INSTALL})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

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
)

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangodump.conf.in
    DESTINATION ${ETCDIR_INSTALL}
    RENAME arangodump-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangodump.conf
  DESTINATION ${ETCDIR_INSTALL})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

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
)

install(
  TARGETS ${BIN_ARANGOIMP}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangoimp.conf.in
    DESTINATION ${ETCDIR_INSTALL}
    RENAME arangoimp-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangoimp.conf
  DESTINATION ${ETCDIR_INSTALL})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

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
)

install(
  TARGETS ${BIN_ARANGORESTORE}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangorestore.conf.in
    DESTINATION ${ETCDIR_INSTALL}
    RENAME arangorestore-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangorestore.conf
  DESTINATION ${ETCDIR_INSTALL})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

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
)

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangosh.conf.in
    DESTINATION ${ETCDIR_INSTALL}
    RENAME arangosh-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangosh.conf
  DESTINATION ${ETCDIR_INSTALL})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb/js
  FILES_MATCHING PATTERN "*.js"
  REGEX "^.*/common/test-data$" EXCLUDE
  REGEX "^.*/common/tests$" EXCLUDE
  REGEX "^.*/client/tests$" EXCLUDE)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
