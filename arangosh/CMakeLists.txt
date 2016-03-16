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
  Benchmark/BenchFeature.cpp
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
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangob)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangob zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangob zlibstatic)
endif ()

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
  Dump/DumpFeature.cpp
  Dump/arangodump.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGODUMP}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGODUMP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangodump)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangodump zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangodump zlibstatic)
endif ()

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
  Import/ImportFeature.cpp
  Import/ImportHelper.cpp
  Import/arangoimp.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGOIMP}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGOIMP}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangoimp)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangoimp zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangoimp zlibstatic) # v8_build includes ICU build
endif ()

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
  Restore/RestoreFeature.cpp
  Restore/arangorestore.cpp
  V8Client/ArangoClientHelper.cpp
)

target_link_libraries(${BIN_ARANGORESTORE}
  ${LIB_ARANGO}
  ${LINENOISE_LIBS}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

install(
  TARGETS ${BIN_ARANGORESTORE}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangorestore)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangorestore zlibstatic v8_build) # v8_build includes ICU build
else ()
  add_dependencies(arangorestore zlibstatic) # v8_build includes ICU build
endif ()

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
  Import/ImportHelper.cpp
  Shell/ShellFeature.cpp
  Shell/V8ClientConnection.cpp
  Shell/V8ShellFeature.cpp
  Shell/arangosh.cpp
  V8Client/ArangoClientHelper.cpp
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
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(arangosh)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(arangosh zlibstatic v8_build)
else ()
  add_dependencies(arangosh zlibstatic)
endif ()

################################################################################
## foxx-manager
################################################################################

install_command_alias(${BIN_ARANGOSH}
  ${CMAKE_INSTALL_BINDIR}
  foxx-manager)

install_config(foxx-manager)
