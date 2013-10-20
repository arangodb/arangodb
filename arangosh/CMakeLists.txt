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

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangosh.conf
  DESTINATION ${ARANGODB_CONF_DIR})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/js/common ${PROJECT_SOURCE_DIR}/js/client
  DESTINATION share/arangodb
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
