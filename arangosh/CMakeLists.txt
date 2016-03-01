# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")

################################################################################
## arangob
################################################################################

if (MSVC)
  generate_product_version(ProductVersionFiles_arangob
    NAME arangob
    FILE_DESCRIPTION ${ARANGOB_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOB}
  ${ProductVersionFiles_arangob}
  ${PROJECT_SOURCE_DIR}/lib/Basics/WorkMonitorDummy.cpp
  ArangoShell/ArangoClient.cpp
  Benchmark/arangob.cpp
)

target_link_libraries(${BIN_ARANGOB}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOB}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_BIN})

install_config(arangob)

add_dependencies(arangob zlibstatic v8_build) # v8_build includes ICU build

################################################################################
## arangodump
################################################################################

if (MSVC)
  generate_product_version(ProductVersionFiles_arangodump
    NAME arangodump
    FILE_DESCRIPTION ${ARANGO_DUMP_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGODUMP}
  ${ProductVersionFiles_arangodump}
  ${PROJECT_SOURCE_DIR}/lib/Basics/WorkMonitorDummy.cpp
  ArangoShell/ArangoClient.cpp
  V8Client/arangodump.cpp
)

target_link_libraries(${BIN_ARANGODUMP}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_BIN})

install_config(arangodump)

add_dependencies(arangodump zlibstatic v8_build) # v8_build includes ICU build

################################################################################
## arangoimp
################################################################################

if (MSVC)
  generate_product_version(ProductVersionFiles_arangoimp
    NAME arangoimp
    FILE_DESCRIPTION ${ARANGO_IMP_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOIMP}
  ${ProductVersionFiles_arangoimp}
  ${PROJECT_SOURCE_DIR}/lib/Basics/WorkMonitorDummy.cpp
  ArangoShell/ArangoClient.cpp
  V8Client/ImportHelper.cpp
  V8Client/arangoimp.cpp
)
target_link_libraries(${BIN_ARANGOIMP}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOIMP}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_BIN})

install_config(arangoimp)

add_dependencies(arangoimp zlibstatic v8_build) # v8_build includes ICU build

################################################################################
## arangorestore
################################################################################

if (MSVC)
  generate_product_version(ProductVersionFiles_arangorestore
    NAME arangorestore
    FILE_DESCRIPTION ${ARANGO_RESTORE_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGORESTORE}
  ${ProductVersionFiles_arangorestore}
  ${PROJECT_SOURCE_DIR}/lib/Basics/WorkMonitorDummy.cpp
  ArangoShell/ArangoClient.cpp
  V8Client/arangorestore.cpp
)

target_link_libraries(${BIN_ARANGORESTORE}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGORESTORE}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_BIN})

install_config(arangorestore)

add_dependencies(arangorestore zlibstatic v8_build) # v8_build includes ICU build

################################################################################
## arangosh
################################################################################

if (MSVC)
  generate_product_version(ProductVersionFiles_arangosh
    NAME arangosh
    FILE_DESCRIPTION ${ARANGOSH_FRIENDLY_STRING}
    ICON ${ARANGO_ICON}
    VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
    VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
    VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
    VERSION_REVISION ${BUILD_ID}
  )
endif ()

add_executable(${BIN_ARANGOSH}
  ${ProductVersionFiles_arangosh}
  ${PROJECT_SOURCE_DIR}/lib/Basics/WorkMonitorDummy.cpp
  ArangoShell/ArangoClient.cpp
  V8Client/ImportHelper.cpp
  V8Client/V8ClientConnection.cpp
  V8Client/arangosh.cpp
)

target_link_libraries(${BIN_ARANGOSH}
  ${LIB_ARANGO_V8}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${V8_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOSH}
  RUNTIME DESTINATION ${ARANGODB_INSTALL_BIN})

install_config(arangosh)

add_dependencies(arangosh zlibstatic v8_build)

################################################################################
## foxx-manager
################################################################################

install_command_alias(${BIN_ARANGOSH}
  ${ARANGODB_INSTALL_BIN}
  foxx-manager)

install_config(foxx-manager)
