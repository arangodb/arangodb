# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/arangosh)

## -----------------------------------------------------------------------------
## --SECTION--                                                       EXECUTABLES
## -----------------------------------------------------------------------------

################################################################################
### @brief output directory
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
### @brief arangoirb
################################################################################

add_executable(
    ${BIN_ARANGOIRB}
    ../arangosh/ArangoShell/ArangoClient.cpp
    MRClient/MRubyClientConnection.cpp
    MRClient/arangoirb.cpp
)

target_link_libraries(
    ${BIN_ARANGOIRB}
    ${LIB_ARANGO_MRUBY}
    ${LIB_ARANGO_CLIENT}
    ${LIB_ARANGO}
    ${V8_LIBS} # need this for rest::Version
    ${MRUBY_LIBS}
    ${ICU_LIBS}
    ${ZLIB_LIBS}
    ${READLINE_LIBS}
    ${OPENSSL_LIBS}
)

install(
  TARGETS ${BIN_ARANGOIRB}
  RUNTIME DESTINATION bin)

if (USE_RAW_CONFIG)
  install(
    FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangoirb.conf.in
    DESTINATION ${ARANGODB_CONF_DIR}
    RENAME arangoirb-relative.conf)
endif ()

install(
  FILES ${PROJECT_SOURCE_DIR}/etc/arangodb/arangoirb.conf
  DESTINATION ${ARANGODB_CONF_DIR})

install(
  DIRECTORY ${PROJECT_SOURCE_DIR}/mr/common ${PROJECT_SOURCE_DIR}/mr/client
  DESTINATION share/arangodb/mr
  FILES_MATCHING PATTERN "*.rb")

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
